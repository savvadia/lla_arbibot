#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <chrono>
#include <array>
#include <unordered_map>
#include <string>

enum class ExchangeId {
    UNKNOWN = 0,
    BINANCE = 1,
    KRAKEN = 2,
    KUCOIN = 3,
    OKX = 4,
    CRYPTO = 5,
    COUNT = 6 // To track the number of exchange IDs
};

// Convert ExchangeId to string
inline const char* toString(ExchangeId id) {
    switch (id) {
        case ExchangeId::BINANCE:
            return "BINANCE";
        case ExchangeId::KRAKEN:
            return "KRAKEN";
        case ExchangeId::KUCOIN:
            return "KUCOIN";
        case ExchangeId::OKX:
            return "OKX";
        case ExchangeId::CRYPTO:
            return "CRYPTO";
        default:
            return "UNKNOWN";
    }
}

// Stream operator for ExchangeId
inline std::ostream& operator<<(std::ostream& os, ExchangeId id) {
    os << toString(id);
    return os;
}

struct TradingPairCoins {
    std::string base;
    std::string quote;
};

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

// Central data structure
struct TradingPairInfo {
    std::string displayName;
    std::string baseSymbol;
    std::string quoteSymbol;
    int precision;
    std::unordered_map<ExchangeId, std::string> exchangeSymbols;
};

// Global lookup tables
class TradingPairData {
public:
    static const TradingPairInfo& get(TradingPair pair);
    static const std::string& getSymbol(ExchangeId ex, TradingPair pair);
    static TradingPair fromSymbol(ExchangeId ex, const std::string& symbol);
    static int getPrecision(TradingPair pair);

private:
    static const std::array<TradingPairInfo, static_cast<size_t>(TradingPair::COUNT)> pairData;
    static const std::array<std::unordered_map<std::string, TradingPair>, static_cast<size_t>(ExchangeId::COUNT)> symbolMaps;
};

inline std::ostream& operator<<(std::ostream& os, const TradingPairInfo& info) {
    os << 
        info.displayName << " " << 
        info.baseSymbol << " " << 
        info.quoteSymbol << " [";
    for(const auto& [ex, symbol] : info.exchangeSymbols) {
        os << ex << " " << symbol << " ";
    }
    os << "]";
    return os;
}

inline const TradingPairCoins getTradingPairCoins(TradingPair pair) {
    auto& data = TradingPairData::get(pair);
    return TradingPairCoins{data.baseSymbol, data.quoteSymbol};
}

// Stream operator for TradingPair
inline std::ostream& operator<<(std::ostream& os, TradingPair pair) {
    os << TradingPairData::get(pair).displayName;
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


enum class OrderState {
    NEW,
    PENDING,
    EXECUTED,
    CANCELLED,
    TIMEOUT
};

// Convert OrderState to string
inline const char* toString(OrderState state) {
    switch (state) {
        case OrderState::NEW:
            return "NEW";
        case OrderState::PENDING:
            return "PENDING";
        case OrderState::EXECUTED:
            return "EXECUTED";
        case OrderState::CANCELLED:
            return "CANCELLED";
        case OrderState::TIMEOUT:
            return "TIMEOUT";
        default:
            return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, OrderState state) {
    os << toString(state);
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
