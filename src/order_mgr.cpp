#include <format>

#include "order_mgr.h"
#include "config.h"
#include "tracer.h"
#include "timers.h"
#define TRACE(...) TRACE_THIS(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)

#define TRACE_EX(_ex, ...) TRACE_THIS(TraceInstance::ORDER_MGR, _ex, __VA_ARGS__)
#define DEBUG_EX(_ex, ...) DEBUG_THIS(TraceInstance::ORDER_MGR, _ex, __VA_ARGS__)
#define ERROR_EX(_ex, ...) ERROR_BASE(TraceInstance::ORDER_MGR, _ex, __VA_ARGS__)

std::ostream& operator<<(std::ostream& os, const OpportunityState& state) {
    switch (state) {
        case OPP_ACCEPTED: os << "ACCEPTED"; break;
        case OPP_EXECUTING: os << "EXECUTING"; break;
        case OPP_PARTIALLY_EXECUTED: os << "PART_EXECUTED"; break;
        case OPP_CANCELLING: os << "CANCELLING"; break;
        case OPP_CANCELLED: os << "CANCELLED"; break;
        case OPP_EXECUTED_AS_PLANNED: os << "EXEC_AS_PLANNED"; break;
        case OPP_EXECUTION_TIMEOUT: os << "EXEC_TIMEOUT"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const OpportunityAction& action) {
    switch (action) {
        case OPP_ACTION_NONE: os << "NONE"; break;
        case OPP_ACTION_PLACE: os << "PLACE"; break;
        case OPP_ACTION_CANCEL: os << "CANCEL"; break;
    }
    return os;
}

void AcceptedOpportunity::trace(std::ostream& os) const {
    Order* orderBuy = orderManager.getOrder(orderBuyId);
    Order* orderSell = orderManager.getOrder(orderSellId);
    OrderState buyState = orderBuy ? orderBuy->state : OrderState::NONE;
    OrderState sellState = orderSell ? orderSell->state : OrderState::NONE;
    os << "AccOpp " << id << ": "  << state
        << " buy: " << " " << orderBuyId << " " << opportunity.buyExchange << " " << buyState
        << " sell: " << " " << orderSellId << " " << opportunity.sellExchange << " " << sellState;
}

OpportunityHistoryEntry::OpportunityHistoryEntry(std::chrono::system_clock::time_point timestamp,
    OpportunityState state, OrderState buyState, OrderState sellState)
    : tsRequested(timestamp), state(state), buyState(buyState), sellState(sellState) {
    delayMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - tsRequested).count();
}

void AcceptedOpportunity::setState(OpportunityState newState) {
    MUTEX_LOCK(m_mutex);
    state = newState;
    history.push_back(OpportunityHistoryEntry(std::chrono::system_clock::now(), 
        newState, 
        orderBuyId ? orderManager.getOrder(orderBuyId)->state : OrderState::NONE, 
        orderSellId ? orderManager.getOrder(orderSellId)->state : OrderState::NONE));
}

Order* OrderManager::getOrder(int orderId) {
    Order* order_p = nullptr;
    {
        MUTEX_LOCK(m_mutex);
        auto it = m_idToOrder.find(orderId);
        if(it != m_idToOrder.end()) {
            order_p = &it->second;
        }
    }
    if(order_p == nullptr) {
        ERROR("Order not found: ", orderId);
    }
    return order_p;
}

AcceptedOpportunity* OrderManager::getAcceptedOpportunity(int id) {
    AcceptedOpportunity* accOpp_p = nullptr;
    {
        MUTEX_LOCK(m_mutex);
        auto it = m_idToOpportunity.find(id);
        if(it != m_idToOpportunity.end()) {
            accOpp_p = &it->second;
        }
    }   
    if(accOpp_p == nullptr) {
        ERROR("Accepted opp not found: ", id);
    }
    return accOpp_p;
}

AcceptedOpportunity* OrderManager::getAcceptedOpportunityByOrderId(int orderId) {
    AcceptedOpportunity* accOpp_p = nullptr;
    int oppId = 0;
    {  
        MUTEX_LOCK(m_mutex);
        auto it = m_orderToOpportunity.find(orderId);
        if(it != m_orderToOpportunity.end()) {
            oppId = it->second;
            auto oppIt = m_idToOpportunity.find(oppId);
            if(oppIt != m_idToOpportunity.end()) {
                accOpp_p = &oppIt->second;
            }
        }
    }
    if(accOpp_p == nullptr) {
        ERROR("Accepted opp not found: ", orderId, " oppId: ", oppId);
    }
    return accOpp_p;
}

void callbackForOpportunityTimeout(int id, void* data) {
    orderManager.handleOpportunityTimeout(id, data);
}

void OrderManager::handleOpportunityTimeout(int id, void* data) {
    int accOppId = *static_cast<int*>(data);

    TRACE("Opp timeout for AccOpp: ", accOppId);

    AcceptedOpportunity* accOpp_p = getAcceptedOpportunity(accOppId);
    if(accOpp_p == nullptr) {
        ERROR("Accepted opportunity not found: ", accOppId);
        return;
    }
    int orderBuyId = accOpp_p->orderBuyId;
    int orderSellId = accOpp_p->orderSellId;
    Order* orderBuy_p = getOrder(orderBuyId);
    Order* orderSell_p = getOrder(orderSellId);

    if(orderBuy_p == nullptr || orderSell_p == nullptr) {
        ERROR("Opp timeout without orders: ", *accOpp_p);
        return;
    }

    auto action = OPP_ACTION_NONE;
    int scenario = 0;
    {
        MUTEX_LOCK(m_mutex);
        OrderState buyState = orderBuy_p->state;
        OrderState sellState = orderSell_p->state;

        if (buyState == OrderState::NEW && sellState == OrderState::NEW) {
            scenario = 1;
            action = OPP_ACTION_CANCEL;
        } else if (buyState >= OrderState::EXECUTED && sellState >= OrderState::EXECUTED) {
            scenario = 2;
        } else {
            scenario = 3;       
            action = OPP_ACTION_CANCEL;
        }
    }
    if(scenario == 3) {
        accOpp_p->setState(OPP_EXECUTION_TIMEOUT);
    }
    TRACE("Opp timeout scenario: ", scenario, " ", *accOpp_p, " action: ", action);

    if (action != OPP_ACTION_NONE) {
        handleAction(action, accOpp_p->id);
    }
}

void OrderManager::handleOpportunity(Opportunity& opportunity) {
    if (!isOpportunityFeasible(opportunity)) {
        ERROR("Opp is not feasible: ", opportunity);
        return;
    }

    int id = getNextAcceptedOpportunityId();
    AcceptedOpportunity* accOpp_p = nullptr;
    {
        MUTEX_LOCK(m_mutex);
        auto [it, inserted] = m_idToOpportunity.try_emplace(id, opportunity, id);
        if (inserted) {
            accOpp_p = &it->second;
        }
    }
    if (accOpp_p == nullptr) {
        ERROR("Failed to create opportunity: ", opportunity);
        return;
    }
    accOpp_p->setState(OPP_ACCEPTED);
    
    TRACE("Placing: AccOpp:", id, " ", opportunity);
    accOpp_p->timeoutTimerId = 
        timersManager.addTimer(Config::OPPORTUNITY_TIMEOUT_MS, 
        &callbackForOpportunityTimeout, &id, TimerType::OPPORTUNITY_TIMEOUT, false);
    handleAction(OPP_ACTION_PLACE, id);
}

void OrderManager::handleAction(OpportunityAction action, int opportunityId) {
    AcceptedOpportunity* accOpp_p = getAcceptedOpportunity(opportunityId);
    if(accOpp_p == nullptr) {
        ERROR("[handleAction] Opp not found: ", opportunityId);
        return;
    }
    auto& opp = accOpp_p->opportunity;
    int orderBuyId = 0;
    int orderSellId = 0;
    {
        switch (action) {
            case OPP_ACTION_PLACE: { // the opp is just created. there could be no events for it yet
                if(accOpp_p->orderBuyId != 0 || accOpp_p->orderSellId != 0) {
                    ERROR("[handleAction] Opp has orders: ", *accOpp_p);
                    return;
                }
                {
                    MUTEX_LOCK(m_mutex);

                    orderBuyId = m_nextOrderId++;
                    m_idToOrder.try_emplace(orderBuyId, opp.buyExchange, opp.pair, OrderType::BUY, orderBuyId, opp.buyPrice, opp.amount);
                    m_orderToOpportunity[orderBuyId] = opportunityId;
                    accOpp_p->orderBuyId = orderBuyId;
                
                    orderSellId = m_nextOrderId++;
                    m_idToOrder.try_emplace(orderSellId, opp.sellExchange, opp.pair, OrderType::SELL, orderSellId, opp.sellPrice, opp.amount);
                    m_orderToOpportunity[orderSellId] = opportunityId;
                    accOpp_p->orderSellId = orderSellId;
                }
                TRACE_EX(opp.buyExchange, "Placed orders buy: ", orderBuyId, " sell: ", accOpp_p->orderSellId, " for opportunity: ", opp);
                Order* orderBuy = getOrder(orderBuyId);
                Order* orderSell = getOrder(orderSellId);
                orderBuy->execute();
                orderSell->execute();
                break;
            }
            case OPP_ACTION_CANCEL: {
                if (accOpp_p->orderBuyId == 0 || accOpp_p->orderSellId == 0) {
                    ERROR("[handleAction] Opp has no orders: ", *accOpp_p);
                    return;
                }
                Order* orderBuy_p = getOrder(accOpp_p->orderBuyId);
                Order* orderSell_p = getOrder(accOpp_p->orderSellId);
                if(orderBuy_p == nullptr) {
                    ERROR_EX(opp.buyExchange, "[handleAction] Opp has no buy order: ", *accOpp_p);
                } else {
                    if (orderBuy_p->state < OrderState::EXECUTED) {
                        orderBuy_p->cancel();
                        TRACE_EX(opp.buyExchange, "Cancelled buy order: ", *orderBuy_p, " for opportunity: ", opp);
                    } else {
                        ERROR_EX(opp.buyExchange, "Buy order already executed: ", *orderBuy_p, " for opportunity: ", opp);
                    }
                }

                if (orderSell_p == nullptr) {
                    ERROR_EX(opp.sellExchange, "[handleAction] Opp has no sell order: ", *accOpp_p);
                } else {
                    if (orderSell_p->state < OrderState::EXECUTED) {
                        orderSell_p->cancel();
                        TRACE_EX(opp.sellExchange, "Cancelled sell order: ", *orderSell_p, " for opportunity: ", opp);
                    } else {
                        ERROR_EX(opp.sellExchange, "Sell order already executed: ", *orderSell_p, " for opportunity: ", opp);
                    }
                }
                break;
            }
            default:
                ERROR("Unhandled action: ", action, " for opportunity: ", opp);
                break;
        }
    }
}

bool OrderManager::isOpportunityFeasible(Opportunity& opportunity) {
    return true;
}

void OrderManager::handleOrderStateChange(int orderId, OrderState newState) {
    Order* order = getOrder(orderId);
    if(order == nullptr) {
        ERROR("Order not found: ", orderId, " on state change: ", newState);
        return;
    }
    order->stateChange(newState);

    int oppId = 0;
    {
        MUTEX_LOCK(m_mutex);
        auto it = m_orderToOpportunity.find(orderId);
        if (it == m_orderToOpportunity.end()) {
            ERROR("Order not mapped to opportunity: ", orderId);
            return;
        }
        oppId = it->second;
    }
    AcceptedOpportunity* accOpp_p = getAcceptedOpportunity(oppId);
    if(accOpp_p == nullptr) {
        ERROR("AccOpp not found: ", orderId);
        return;
    }
    auto action = OPP_ACTION_NONE;

    int orderBuyId = accOpp_p->orderBuyId;
    int orderSellId = accOpp_p->orderSellId;

    Order* orderBuy_p = getOrder(orderBuyId);
    Order* orderSell_p = getOrder(orderSellId);
    if(orderBuy_p == nullptr || orderSell_p == nullptr) {
        ERROR("Order not found: ", orderBuyId, " or ", orderSellId, " in ", *accOpp_p);
        return;
    }
    auto buyState = orderBuy_p->state;
    auto sellState = orderSell_p->state;

    if (buyState == OrderState::NEW && sellState == OrderState::NEW) {
        ERROR("State change on NEW orders: ", orderBuyId, "(" , buyState, ")  and ", orderSellId, "(" , sellState, ")");
        accOpp_p->setState(OPP_CANCELLED);
    } else if (buyState == OrderState::EXECUTED && sellState == OrderState::EXECUTED) {
        accOpp_p->setState(OPP_EXECUTED_AS_PLANNED);
        timersManager.stopTimer(accOpp_p->timeoutTimerId);
    } else if ((buyState == OrderState::PARTIALLY_EXECUTED || sellState == OrderState::PARTIALLY_EXECUTED) &&
        (buyState <= OrderState::EXECUTED || sellState <= OrderState::EXECUTED)) {
        accOpp_p->setState(OPP_PARTIALLY_EXECUTED);
    } else if (buyState == OrderState::CANCELLED || sellState == OrderState::CANCELLED) {
        if(buyState < OrderState::EXECUTED || sellState < OrderState::EXECUTED) {
            accOpp_p->setState(OPP_CANCELLING);
            action = OPP_ACTION_CANCEL;
            timersManager.stopTimer(accOpp_p->timeoutTimerId);
        } else {
            accOpp_p->setState(OPP_CANCELLED);
            timersManager.stopTimer(accOpp_p->timeoutTimerId);
        }
    } else if (buyState == OrderState::TIMEOUT || sellState == OrderState::TIMEOUT) {
        if (buyState < OrderState::EXECUTED || sellState < OrderState::EXECUTED) {
            action = OPP_ACTION_CANCEL;
        } else if (buyState == OrderState::EXECUTED || sellState == OrderState::EXECUTED) {
            accOpp_p->setState(OPP_PARTIALLY_EXECUTED);
        }
    } else {
        ERROR("Unhandled state: ", orderBuyId, "(" , buyState, ")  and ", orderSellId, "(" , sellState, ")");
    }

    if (accOpp_p-> state >= OPP_EXECUTED_AS_PLANNED) {
        timersManager.stopTimer(accOpp_p->timeoutTimerId);
        accOpp_p->timeoutTimerId = 0;
    }
    if (accOpp_p-> state >= OPP_EXECUTED_AS_PLANNED) {
        double profit = (accOpp_p->opportunity.sellPrice - accOpp_p->opportunity.buyPrice) * accOpp_p->opportunity.amount;
        // convert profit to string "  0.0000". 4 digits after comma, 8 symbols in total
        std::ostringstream oss;
        oss << std::fixed << std::setw(8) << std::setprecision(4) << profit;
        TRACE("PROFIT: ", oss.str(), " for opp: ", *accOpp_p);
    } else {
        TRACE("Selected ACTION: ", action, " for opp: ", *accOpp_p);
        if (action != OPP_ACTION_NONE) {
            handleAction(action, accOpp_p->id);
        }
    }

}
