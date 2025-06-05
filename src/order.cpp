#include "order.h"
#include "tracer.h"

#define TRACE(...) TRACE_THIS(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::ORDER, this->exchangeId, __VA_ARGS__)

OrderHistoryEntry::OrderHistoryEntry(std::chrono::system_clock::time_point timestamp, OrderState state)
    : tsRequested(timestamp), state(state) {
    delayMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - tsRequested).count();
}

Order::Order(ExchangeManager& exMgr, ExchangeId exchangeId, TradingPair pair, OrderType type, int orderId, double quantity, double price)
    : exMgr(exMgr), exchangeId(exchangeId), pair(pair), type(type), orderId(orderId), price(price), quantity(quantity) {
    setState(OrderState::NEW, std::chrono::system_clock::now());
}

void Order::execute() {
    auto tsRequested = std::chrono::system_clock::now();

    auto exchange = exMgr.getExchange(exchangeId);
    if (!exchange) {
        ERROR("Exchange not found");
        return;
    }

    orderIdAtExchange = exchange->placeOrder(pair, type, quantity, price);
    setState(OrderState::EXECUTED, tsRequested);
}

void Order::cancel() {
    auto tsRequested = std::chrono::system_clock::now();
    auto exchange = exMgr.getExchange(exchangeId);
    if (!exchange) {
        ERROR("Exchange not found");
        return;
    }

    exchange->cancelOrder(orderIdAtExchange);
    setState(OrderState::CANCELLED, tsRequested);
}

void Order::stateChange(OrderState newState) {
    setState(newState, std::chrono::system_clock::now());
}

void Order::setState(OrderState newState, std::chrono::system_clock::time_point timestamp) {
    state = newState;
    history.push_back(OrderHistoryEntry(timestamp, newState));
}

