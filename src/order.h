#pragma once

#include <chrono>
#include "types.h"
#include "ex_mgr.h"


class OrderHistoryEntry {
    public:
        OrderHistoryEntry(std::chrono::system_clock::time_point timestamp, OrderState state);
    private:
        std::chrono::system_clock::time_point tsRequested;
        long delayMicros;
        OrderState state;
};

class Order : public Traceable {
    public:
        Order(ExchangeManager& exMgr, ExchangeId exchangeId, TradingPair pair, OrderType type, int orderId, double price, double quantity);
        void execute();
        void cancel();
        void stateChange(OrderState newState);

    protected:
        void trace(std::ostream& os) const override {
            os << "Order: " << pair << " " << type << " " << state << " " << price << " " << quantity;
        }
    private:
        void setState(OrderState newState, std::chrono::system_clock::time_point timestamp);
        ExchangeManager& exMgr;
        ExchangeId exchangeId;
        TradingPair pair;
        OrderType type;
        int orderId;
        std::string orderIdAtExchange;
        double price;
        double quantity;
        std::vector<OrderHistoryEntry> history;
        OrderState state;
};
