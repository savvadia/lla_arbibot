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

class OpportunityHistoryEntry : public Traceable {
    public:
        OpportunityHistoryEntry(std::chrono::system_clock::time_point timestamp, OpportunityState state, OrderState buyState, OrderState sellState);
    protected:
        void trace(std::ostream& os) const override {
            os << "OppHistory: " << tsRequested << " " << delayMicros << " " << state << " [" << buyState << " " << sellState << "]";
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
        AcceptedOpportunity();
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
        
    protected:
        void trace(std::ostream& os) const override {
            os << "AccOpportunity: " << opportunity << " [" << id << " " << state << "]";
        }
};

class OrderManager : public Traceable {
    public:
        OrderManager() {};
        void handleOpportunity(Opportunity& opportunity);
        void handleOrderStateChange(int orderId, OrderState newState);
        void handleOpportunityTimeout(int opportunityId, void* data);
        Order& getOrder(int orderId) { return m_idToOrder[orderId]; }
    protected:
        void trace(std::ostream& os) const override {
            os << "OrderManager";
        }
        void handleAction(OpportunityAction action, int opportunityId);
    private:
        int m_nextAcceptedOpportunityId = 1;
        int m_nextOrderId = 1;
        bool isOpportunityFeasible(Opportunity& opportunity);

        std::unordered_map<int, int> m_orderToOpportunity; // orderId -> opportunityId
        std::unordered_map<int, Order> m_idToOrder; // orderId -> Order
        std::unordered_map<int, AcceptedOpportunity> m_idToOpportunity; // opportunityId -> AcceptedOpportunity
};

extern OrderManager& orderMgr;