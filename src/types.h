#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <iostream>
#include <chrono>

enum class ExchangeId {
    UNKNOWN = 0,
    BINANCE = 1,
    KRAKEN = 2
};

// Convert ExchangeId to string
inline const char* toString(ExchangeId id) {
    switch (id) {
        case ExchangeId::BINANCE:
            return "BINANCE";
        case ExchangeId::KRAKEN:
            return "KRAKEN";
        default:
            return "UNKNOWN";
    }
}

// Stream operator for ExchangeId
inline std::ostream& operator<<(std::ostream& os, ExchangeId id) {
    os << toString(id);
    return os;
}

enum class TradingPair {
    UNKNOWN = 0,
    BTC_USDT = 1,
    ETH_USDT = 2,
    XTZ_USDT = 3
};

// Convert TradingPair to string
inline const char* toString(TradingPair pair) {
    switch (pair) {
        case TradingPair::BTC_USDT:
            return "BTC/USDT";
        case TradingPair::ETH_USDT:
            return "ETH/USDT";
        case TradingPair::XTZ_USDT:
            return "XTZ/USDT";
        default:
            return "UNKNOWN";
    }
}

// Stream operator for TradingPair
inline std::ostream& operator<<(std::ostream& os, TradingPair pair) {
    os << toString(pair);
    return os;
}

enum class OrderType {
    BUY = 0,
    SELL = 1
};

// Convert OrderType to string
inline const char* toString(OrderType type) {
    switch (type) {
        case OrderType::BUY:
            return "BUY";
        case OrderType::SELL:
            return "SELL";
        default:
            return "UNKNOWN";
    }
}

// Stream operator for OrderType
inline std::ostream& operator<<(std::ostream& os, OrderType type) {
    os << toString(type);
    return os;
}

struct OrderBookData {
    double bestBid;
    double bestAsk;
    double bestBidQuantity;
    double bestAskQuantity;
    std::chrono::system_clock::time_point lastUpdate;
}; 

#endif // TYPES_H
