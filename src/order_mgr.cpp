#include "order_mgr.h"
#include "tracer.h"

#define TRACE(...) TRACE_THIS(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::ORDER_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)

AcceptedOpportunity::AcceptedOpportunity()
    : Opportunity(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, TradingPair::UNKNOWN, 0.0, 0.0, 0.0, std::chrono::system_clock::now()), opportunity(*this), id(0) {}

void AcceptedOpportunity::setState(OpportunityState newState) {
    state = newState;
    history.push_back(OpportunityHistoryEntry(std::chrono::system_clock::now(), 
        newState, 
        orderBuyId ? orderMgr.getOrder(orderBuyId).state : OrderState::NONE, 
        orderSellId ? orderMgr.getOrder(orderSellId).state : OrderState::NONE));
}

void OrderManager::handleOpportunity(Opportunity& opportunity) {
    if (!isOpportunityFeasible(opportunity)) {
        ERROR("Opportunity is not feasible");
        return;
    }

    auto acceptedOpportunity = AcceptedOpportunity(opportunity, m_nextAcceptedOpportunityId++);
    acceptedOpportunity.setState(OPP_ACCEPTED);
    m_idToOpportunity[acceptedOpportunity.id] = acceptedOpportunity;
    
    handleAction(OPP_ACTION_PLACE, acceptedOpportunity.id);
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
                TRACE("Placed buy order: ", order, " for opportunity: ", opportunity.opportunity);
                order.execute();
            }
            if (opportunity.orderSellId == 0) {
                int orderId = m_nextOrderId++;
                auto& opp = opportunity.opportunity;
                m_idToOrder[orderId] = Order(opp.sellExchange, opp.pair, OrderType::SELL, orderId, opp.sellPrice, opp.amount);
                m_orderToOpportunity[orderId] = opportunityId;
                opportunity.orderSellId = orderId;
                auto& order = m_idToOrder[orderId];
                TRACE("Placed sell order: ", order, " for opportunity: ", opportunity.opportunity);
                order.execute();
            }
            break;
        case OPP_ACTION_CANCEL:
            if (opportunity.orderBuyId != 0) {
                auto& order = m_idToOrder[opportunity.orderBuyId];
                if (order.state < OrderState::EXECUTED) {
                    order.cancel();
                    TRACE("Cancelled buy order: ", order, " for opportunity: ", opportunity.opportunity);
                }
            }
            if (opportunity.orderSellId != 0) {
                auto& order = m_idToOrder[opportunity.orderSellId];
                if (order.state < OrderState::EXECUTED) {
                    order.cancel();
                    TRACE("Cancelled sell order: ", order, " for opportunity: ", opportunity.opportunity);
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
    if (m_idToOpportunity.find(orderId) == m_idToOpportunity.end()) {
        ERROR("Order not found: ", orderId, " for opportunity: ", m_idToOpportunity[m_orderToOpportunity[orderId]]);
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
    } else if ((buyState == OrderState::PARTIALLY_EXECUTED || sellState == OrderState::PARTIALLY_EXECUTED) &&
        (buyState <= OrderState::EXECUTED || sellState <= OrderState::EXECUTED)) {
        opportunity.setState(OPP_PARTIALLY_EXECUTED);
    } else if (buyState == OrderState::CANCELLED || sellState == OrderState::CANCELLED) {
        if(buyState < OrderState::EXECUTED || sellState < OrderState::EXECUTED) {
            opportunity.setState(OPP_CANCELLING);
            action = OPP_ACTION_CANCEL;
        } else {
            opportunity.setState(OPP_CANCELLED);
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

    if (action != OPP_ACTION_NONE) {
        handleAction(action, opportunity.id);
    }
}
