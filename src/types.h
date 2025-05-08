#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <chrono>

enum class ExchangeId {
    UNKNOWN = 0,
    BINANCE = 1,
    KRAKEN = 2,
    COUNT = 3 // To track the number of exchange IDs
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
    ADA_USDT,
    ALGO_USDT,
    ATOM_USDT,
    AVAX_USDT,
    BCH_USDT,
    BTC_USDT,
    DOGE_USDT,
    DOT_USDT,
    EOS_USDT,
    ETH_USDT,
    LINK_USDT,
    SOL_USDT,
    XRP_USDT,
    XTZ_USDT,
    COUNT
};

// Convert TradingPair to string
inline const char* toString(TradingPair pair) {
    switch (pair) {
        case TradingPair::ADA_USDT: return "ADA/USDT";
        case TradingPair::ALGO_USDT: return "ALGO/USDT";
        case TradingPair::ATOM_USDT: return "ATOM/USDT";
        case TradingPair::AVAX_USDT: return "AVAX/USDT";
        case TradingPair::BCH_USDT: return "BCH/USDT";
        case TradingPair::BTC_USDT: return "BTC/USDT";
        case TradingPair::DOGE_USDT: return "DOGE/USDT";
        case TradingPair::DOT_USDT: return "DOT/USDT";
        case TradingPair::EOS_USDT: return "EOS/USDT";
        case TradingPair::ETH_USDT: return "ETH/USDT";
        case TradingPair::LINK_USDT: return "LINK/USDT";
        case TradingPair::SOL_USDT: return "SOL/USDT";
        case TradingPair::XRP_USDT: return "XRP/USDT";
        case TradingPair::XTZ_USDT: return "XTZ/USDT";
        default: return "UNKNOWN";
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

class PricePrecision {
public:
    static int getPrecision(TradingPair pair) {
        switch (pair) {
            case TradingPair::ADA_USDT: return 6;   // ADA
            case TradingPair::ALGO_USDT: return 5;  // ALGO
            case TradingPair::ATOM_USDT: return 4;  // ATOM
            case TradingPair::AVAX_USDT: return 2;  // AVAX
            case TradingPair::BCH_USDT: return 2;   // BCH
            case TradingPair::BTC_USDT: return 1;   // BTC
            case TradingPair::DOGE_USDT: return 7;  // DOGE
            case TradingPair::DOT_USDT: return 4;   // DOT
            case TradingPair::EOS_USDT: return 4;   // EOS
            case TradingPair::ETH_USDT: return 2;   // ETH
            case TradingPair::LINK_USDT: return 5;  // LINK
            case TradingPair::SOL_USDT: return 2;   // SOL
            case TradingPair::XRP_USDT: return 5;   // XRP
            case TradingPair::XTZ_USDT: return 4;   // XTZ
            default: return 8;
        }
    }
};

#endif // TYPES_H
