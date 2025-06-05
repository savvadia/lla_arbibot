#include "order.h"
#include "tracer.h"
#include "ex_mgr.h"
#define TRACE(...) TRACE_THIS(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)

OrderHistoryEntry::OrderHistoryEntry(std::chrono::system_clock::time_point timestamp, OrderState state)
    : tsRequested(timestamp), state(state) {
    delayMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - tsRequested).count();
}

Order::Order()
    : exchangeId(ExchangeId::UNKNOWN), pair(TradingPair::UNKNOWN), type(OrderType::BUY), orderId(0), price(0), quantity(0), executedQuantity(0), executedPrice(0), state(OrderState::NONE) {}

Order::Order(ExchangeId exchangeId, TradingPair pair, OrderType type, int orderId, double price, double quantity)
    : exchangeId(exchangeId), pair(pair), type(type), orderId(orderId), price(price), quantity(quantity) {
    setState(OrderState::NEW, std::chrono::system_clock::now());
}

void Order::execute() {
    auto tsRequested = std::chrono::system_clock::now();

    auto exchange = exchangeManager.getExchange(exchangeId);
    if (!exchange) {
        ERROR("Exchange not found");
        return;
    }

    // orderIdAtExchange = exchange->placeOrder(pair, type, quantity, price);
    TRACE("FAKE: Executed order: ", *this);
    setState(OrderState::EXECUTED, tsRequested);
}

void Order::cancel() {
    auto tsRequested = std::chrono::system_clock::now();
    auto exchange = exchangeManager.getExchange(exchangeId);
    if (!exchange) {
        ERROR("Exchange not found");
        return;
    }

    // exchange->cancelOrder(orderIdAtExchange);
    TRACE("FAKE: Cancelled order: ", *this);
    setState(OrderState::CANCELLED, tsRequested);
}

void Order::stateChange(OrderState newState) {
    setState(newState, std::chrono::system_clock::now());
}

void Order::setState(OrderState newState, std::chrono::system_clock::time_point timestamp) {
    state = newState;
    history.push_back(OrderHistoryEntry(timestamp, newState));
}

