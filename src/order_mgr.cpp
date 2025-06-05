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
    os << "AccOpp " << id << ": "  << state
        << " buy: " << " " << orderBuyId << " " << opportunity.buyExchange << " " << orderManager.getOrder(orderBuyId).state
        << " sell: " << " " << orderSellId << " " << opportunity.sellExchange << " " << orderManager.getOrder(orderSellId).state;
}

OpportunityHistoryEntry::OpportunityHistoryEntry(std::chrono::system_clock::time_point timestamp,
    OpportunityState state, OrderState buyState, OrderState sellState)
    : tsRequested(timestamp), state(state), buyState(buyState), sellState(sellState) {
    delayMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - tsRequested).count();
}

AcceptedOpportunity::AcceptedOpportunity()
    : Opportunity(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, TradingPair::UNKNOWN, 0.0, 0.0, 0.0, std::chrono::system_clock::now()), opportunity(*this), id(0) {}

void AcceptedOpportunity::setState(OpportunityState newState) {
    state = newState;
    history.push_back(OpportunityHistoryEntry(std::chrono::system_clock::now(), 
        newState, 
        orderBuyId ? orderManager.getOrder(orderBuyId).state : OrderState::NONE, 
        orderSellId ? orderManager.getOrder(orderSellId).state : OrderState::NONE));
}

void callbackForOpportunityTimeout(int id, void* data) {
    orderManager.handleOpportunityTimeout(id, data);
}

void OrderManager::handleOpportunityTimeout(int id, void* data) {
    int accOppId = *static_cast<int*>(data);

    TRACE("Handling opportunity timeout for AccOpp: ", accOppId);

    auto& accOpp = m_idToOpportunity[accOppId];
    auto action = OPP_ACTION_NONE;

    auto& orderBuy = m_idToOrder[accOpp.orderBuyId];
    auto& orderSell = m_idToOrder[accOpp.orderSellId];
    auto buyState = orderBuy.state;
    auto sellState = orderSell.state;

    if (buyState == OrderState::NEW && sellState == OrderState::NEW) {
        ERROR("Opportunity timeout on NEW orders: ", accOpp.orderBuyId, "(" , buyState, ")  and ", accOpp.orderSellId, "(" , sellState, ")");
        action = OPP_ACTION_CANCEL;
    } else if (buyState >= OrderState::EXECUTED && sellState >= OrderState::EXECUTED) {
        ERROR("Opportunity timeout on closed orders: ", accOpp.orderBuyId, "(" , buyState, ")  and ", accOpp.orderSellId, "(" , sellState, ")");
    } else {
        TRACE("Opportunity timeout: ", accOpp.opportunity, " ",  accOpp.orderBuyId, "(" , buyState, ")  and ", accOpp.orderSellId, "(" , sellState, ")");
        accOpp.setState(OPP_EXECUTION_TIMEOUT);
        action = OPP_ACTION_CANCEL;
    }

    if (action != OPP_ACTION_NONE) {
        handleAction(action, accOppId);
    }
}

void OrderManager::handleOpportunity(Opportunity& opportunity) {
    if (!isOpportunityFeasible(opportunity)) {
        ERROR("Opportunity is not feasible: ", opportunity);
        return;
    }

    auto acceptedOpportunity = AcceptedOpportunity(opportunity, m_nextAcceptedOpportunityId++);
    acceptedOpportunity.setState(OPP_ACCEPTED);
    m_idToOpportunity[acceptedOpportunity.id] = acceptedOpportunity;
    
    TRACE("Placing: AccOpp:", acceptedOpportunity.id, " ", opportunity);
    handleAction(OPP_ACTION_PLACE, acceptedOpportunity.id);
    acceptedOpportunity.timeoutTimerId = 
        timersManager.addTimer(Config::OPPORTUNITY_TIMEOUT_MS, 
        &callbackForOpportunityTimeout, &acceptedOpportunity.id, TimerType::OPPORTUNITY_TIMEOUT, false);
}

void OrderManager::handleAction(OpportunityAction action, int opportunityId) {
    auto& opportunity = m_idToOpportunity[opportunityId];
    switch (action) {
        case OPP_ACTION_PLACE:
            if (opportunity.orderBuyId == 0) {
                int orderId = m_nextOrderId++;
                auto& opp = opportunity.opportunity;
                m_idToOrder[orderId] = Order(opp.buyExchange, opp.pair, OrderType::BUY, orderId, opp.buyPrice, opp.amount);
                m_orderToOpportunity[orderId] = opportunityId;
                opportunity.orderBuyId = orderId;
                auto& order = m_idToOrder[orderId];
                TRACE_EX(opp.buyExchange, "Placed buy order: ", order, " for opportunity: ", opportunity.opportunity);
                order.execute();
            }
            if (opportunity.orderSellId == 0) {
                int orderId = m_nextOrderId++;
                auto& opp = opportunity.opportunity;
                m_idToOrder[orderId] = Order(opp.sellExchange, opp.pair, OrderType::SELL, orderId, opp.sellPrice, opp.amount);
                m_orderToOpportunity[orderId] = opportunityId;
                opportunity.orderSellId = orderId;
                auto& order = m_idToOrder[orderId];
                TRACE_EX(opp.sellExchange, "Placed sell order: ", order, " for opportunity: ", opportunity.opportunity);
                order.execute();
            }
            break;
        case OPP_ACTION_CANCEL:
            if (opportunity.orderBuyId != 0) {
                auto& order = m_idToOrder[opportunity.orderBuyId];
                auto& opp = opportunity.opportunity;
                if (order.state < OrderState::EXECUTED) {
                    order.cancel();
                    TRACE_EX(opp.buyExchange, "Cancelled buy order: ", order, " for opportunity: ", opportunity.opportunity);
                }
            }
            if (opportunity.orderSellId != 0) {
                auto& order = m_idToOrder[opportunity.orderSellId];
                auto& opp = opportunity.opportunity;
                if (order.state < OrderState::EXECUTED) {
                    order.cancel();
                    TRACE_EX(opp.sellExchange, "Cancelled sell order: ", order, " for opportunity: ", opportunity.opportunity);
                }
            }
            break;
        default:
            ERROR("Unhandled action: ", action, " for opportunity: ", opportunity.opportunity);
            break;
    }
}

bool OrderManager::isOpportunityFeasible(Opportunity& opportunity) {
    return true;
}

void OrderManager::handleOrderStateChange(int orderId, OrderState newState) {
    if (m_idToOrder.find(orderId) == m_idToOrder.end()) {
        ERROR("Order not found: ", orderId);
        return;
    }

    m_idToOrder[orderId].stateChange(newState);

    auto& opportunity = m_idToOpportunity[m_orderToOpportunity[orderId]];
    auto action = OPP_ACTION_NONE;

    auto& orderBuy = m_idToOrder[opportunity.orderBuyId];
    auto& orderSell = m_idToOrder[opportunity.orderSellId];
    auto buyState = orderBuy.state;
    auto sellState = orderSell.state;

    if (buyState == OrderState::NEW && sellState == OrderState::NEW) {
        ERROR("State change on NEW orders: ", opportunity.orderBuyId, "(" , buyState, ")  and ", opportunity.orderSellId, "(" , sellState, ")");
        opportunity.setState(OPP_CANCELLED);
    } else if (buyState == OrderState::EXECUTED && sellState == OrderState::EXECUTED) {
        opportunity.setState(OPP_EXECUTED_AS_PLANNED);
        timersManager.stopTimer(opportunity.timeoutTimerId);
    } else if ((buyState == OrderState::PARTIALLY_EXECUTED || sellState == OrderState::PARTIALLY_EXECUTED) &&
        (buyState <= OrderState::EXECUTED || sellState <= OrderState::EXECUTED)) {
        opportunity.setState(OPP_PARTIALLY_EXECUTED);
    } else if (buyState == OrderState::CANCELLED || sellState == OrderState::CANCELLED) {
        if(buyState < OrderState::EXECUTED || sellState < OrderState::EXECUTED) {
            opportunity.setState(OPP_CANCELLING);
            action = OPP_ACTION_CANCEL;
            timersManager.stopTimer(opportunity.timeoutTimerId);
        } else {
            opportunity.setState(OPP_CANCELLED);
            timersManager.stopTimer(opportunity.timeoutTimerId);
        }
    } else if (buyState == OrderState::TIMEOUT || sellState == OrderState::TIMEOUT) {
        if (buyState < OrderState::EXECUTED || sellState < OrderState::EXECUTED) {
            action = OPP_ACTION_CANCEL;
        } else if (buyState == OrderState::EXECUTED || sellState == OrderState::EXECUTED) {
            opportunity.setState(OPP_PARTIALLY_EXECUTED);
        }
    } else {
        ERROR("Unhandled state: ", opportunity.orderBuyId, "(" , buyState, ")  and ", opportunity.orderSellId, "(" , sellState, ")");
    }

    TRACE("Selected ACTION: ", action, " for opportunity: ", opportunity);

    if (action != OPP_ACTION_NONE) {
        handleAction(action, opportunity.id);
    }
}
