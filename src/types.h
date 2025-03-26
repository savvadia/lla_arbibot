#pragma once

#include <string>
#include <iostream>

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