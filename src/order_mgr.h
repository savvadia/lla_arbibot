#pragma once

#include <unordered_map>

#include "order.h"
#include "strategy.h"

enum OpportunityState {
    OPP_ACCEPTED,
    OPP_EXECUTING,
    OPP_PARTIALLY_EXECUTED,
    OPP_CANCELLING,
    OPP_CANCELLED,
    OPP_EXECUTED_AS_PLANNED,
    OPP_EXECUTION_TIMEOUT,
};

enum OpportunityAction {
    OPP_ACTION_NONE,
    OPP_ACTION_PLACE,
    OPP_ACTION_CANCEL,
};

std::ostream& operator<<(std::ostream& os, const OpportunityState& state);
std::ostream& operator<<(std::ostream& os, const OpportunityAction& action);

class OpportunityHistoryEntry : public Traceable {
    public:
        OpportunityHistoryEntry(std::chrono::system_clock::time_point timestamp, OpportunityState state, OrderState buyState, OrderState sellState);
    protected:
        void trace(std::ostream& os) const override {
            os << "OppHist: " << tsRequested << " " << delayMicros << " " << state << " [" << buyState << " " << sellState << "]";
        }
    private:
        std::chrono::system_clock::time_point tsRequested;
        long delayMicros;
        OpportunityState state;
        OrderState buyState;
        OrderState sellState;
};

class AcceptedOpportunity : public Opportunity {
    public:
        AcceptedOpportunity(const Opportunity& opportunity, int id)
            : Opportunity(opportunity), opportunity(opportunity), id(id) {}
        
        void setState(OpportunityState newState);
        
        Opportunity opportunity;
        int id;
        OpportunityState state = OPP_ACCEPTED;
        int orderBuyId = 0;
        int orderSellId = 0;
        std::vector<OpportunityHistoryEntry> history;
        int timeoutTimerId = 0;
        std::mutex m_mutex; // for state and history
        
    protected:
        void trace(std::ostream& os) const override;
};

class OrderManager : public Traceable {
    public:
        OrderManager() {};
        void handleOpportunity(Opportunity& opportunity);
        void handleOrderStateChange(int orderId, OrderState newState);
        void handleOpportunityTimeout(int id, void* data);
        Order* getOrder(int orderId);
        AcceptedOpportunity* getAcceptedOpportunity(int id);
        AcceptedOpportunity* getAcceptedOpportunityByOrderId(int orderId);

    protected:
        void trace(std::ostream& os) const override {};
        void handleAction(OpportunityAction action, int opportunityId);
    private:
        int m_nextAcceptedOpportunityId = 1;
        int m_nextOrderId = 1;
        bool isOpportunityFeasible(Opportunity& opportunity);

        int getNextOrderId() { MUTEX_LOCK(m_mutex); return m_nextOrderId++; }
        int getNextAcceptedOpportunityId() { MUTEX_LOCK(m_mutex); return m_nextAcceptedOpportunityId++; }

        std::unordered_map<int, int> m_orderToOpportunity; // orderId -> opportunityId
        std::unordered_map<int, Order> m_idToOrder; // orderId -> Order
        std::unordered_map<int, AcceptedOpportunity> m_idToOpportunity; // opportunityId -> AcceptedOpportunity

        std::mutex m_mutex; // for m_idToOrder, m_idToOpportunity, m_orderToOpportunity, m_nextOrderId, m_nextAcceptedOpportunityId
};

extern OrderManager orderManager;