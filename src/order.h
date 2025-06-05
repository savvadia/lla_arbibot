#pragma once

#include <chrono>
#include "types.h"
#include "tracer.h"

class OrderHistoryEntry : public Traceable {
    public:
        OrderHistoryEntry(std::chrono::system_clock::time_point timestamp, OrderState state);
    protected:
        void trace(std::ostream& os) const override {
            os << "OrderHistoryEntry: " << tsRequested << " " << delayMicros << " " << state;
        }
    private:
        std::chrono::system_clock::time_point tsRequested;
        long delayMicros;
        OrderState state;
};

class Order : public Traceable {
    public:
        Order();
        Order(ExchangeId exchangeId, TradingPair pair, OrderType type, int orderId, double price, double quantity);
        void execute();
        void cancel();
        void stateChange(OrderState newState);

        ExchangeId exchangeId;
        TradingPair pair;
        OrderType type;
        int orderId;
        std::string orderIdAtExchange;
        double price;
        double quantity;
        double executedQuantity;
        double executedPrice;
        std::vector<OrderHistoryEntry> history;
        OrderState state;

    protected:
        void trace(std::ostream& os) const override {
            os << "Order " << orderId << ": " << pair << " " << type << " " << state;
        }
    private:
        void setState(OrderState newState, std::chrono::system_clock::time_point timestamp);
};
