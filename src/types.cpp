#include "types.h"

#include <algorithm>

// ---- TradingPairInfo Table ----
const std::array<TradingPairInfo, static_cast<size_t>(TradingPair::COUNT)>
TradingPairData::pairData = {{
    {"UNKNOWN", "UNKNOWN", "UNKNOWN", 8, {}},
    {"ADA/USDT", "ADA", "USDT", 6, {{ExchangeId::BINANCE, "ADAUSDT"}, {ExchangeId::KRAKEN, "ADA/USD"}, {ExchangeId::KUCOIN, "ADA-USDT"}}},
    {"ALGO/USDT", "ALGO", "USDT", 5, {{ExchangeId::BINANCE, "ALGOUSDT"}, {ExchangeId::KRAKEN, "ALGO/USD"}, {ExchangeId::KUCOIN, "ALGO-USDT"}}},
    {"ATOM/USDT", "ATOM", "USDT", 4, {{ExchangeId::BINANCE, "ATOMUSDT"}, {ExchangeId::KRAKEN, "ATOM/USD"}, {ExchangeId::KUCOIN, "ATOM-USDT"}}},
    {"AVAX/USDT", "AVAX", "USDT", 2, {{ExchangeId::BINANCE, "AVAXUSDT"}, {ExchangeId::KRAKEN, "AVAX/USD"}, {ExchangeId::KUCOIN, "AVAX-USDT"}}},
    {"BCH/USDT", "BCH", "USDT", 2, {{ExchangeId::BINANCE, "BCHUSDT"}, {ExchangeId::KRAKEN, "BCH/USD"}, {ExchangeId::KUCOIN, "BCH-USDT"}}},
    {"BTC/USDT", "BTC", "USDT", 1, {{ExchangeId::BINANCE, "BTCUSDT"}, {ExchangeId::KRAKEN, "BTC/USD"}, {ExchangeId::KUCOIN, "BTC-USDT"}}},
    {"DOGE/USDT", "DOGE", "USDT", 7, {{ExchangeId::BINANCE, "DOGEUSDT"}, {ExchangeId::KRAKEN, "DOGE/USD"}, {ExchangeId::KUCOIN, "DOGE-USDT"}}},
    {"DOT/USDT", "DOT", "USDT", 4, {{ExchangeId::BINANCE, "DOTUSDT"}, {ExchangeId::KRAKEN, "DOT/USD"}, {ExchangeId::KUCOIN, "DOT-USDT"}}},
    {"EOS/USDT", "EOS", "USDT", 4, {{ExchangeId::BINANCE, "EOSUSDT"}, {ExchangeId::KRAKEN, "EOS/USD"}, {ExchangeId::KUCOIN, "EOS-USDT"}}},
    {"ETH/USDT", "ETH", "USDT", 2, {{ExchangeId::BINANCE, "ETHUSDT"}, {ExchangeId::KRAKEN, "ETH/USD"}, {ExchangeId::KUCOIN, "ETH-USDT"}}},
    {"LINK/USDT", "LINK", "USDT", 5, {{ExchangeId::BINANCE, "LINKUSDT"}, {ExchangeId::KRAKEN, "LINK/USD"}, {ExchangeId::KUCOIN, "LINK-USDT"}}},
    {"SOL/USDT", "SOL", "USDT", 2, {{ExchangeId::BINANCE, "SOLUSDT"}, {ExchangeId::KRAKEN, "SOL/USD"}, {ExchangeId::KUCOIN, "SOL-USDT"}}},
    {"XRP/USDT", "XRP", "USDT", 5, {{ExchangeId::BINANCE, "XRPUSDT"}, {ExchangeId::KRAKEN, "XRP/USD"}, {ExchangeId::KUCOIN, "XRP-USDT"}}},
    {"XTZ/USDT", "XTZ", "USDT", 4, {{ExchangeId::BINANCE, "XTZUSDT"}, {ExchangeId::KRAKEN, "XTZ/USD"}, {ExchangeId::KUCOIN, "XTZ-USDT"}}},
}};

// ---- Symbol Maps ----
const std::array<std::unordered_map<std::string, TradingPair>,
                 static_cast<size_t>(ExchangeId::COUNT)>
TradingPairData::symbolMaps = []{
    std::array<std::unordered_map<std::string, TradingPair>, static_cast<size_t>(ExchangeId::COUNT)> maps;

    for (size_t pairIdx = 1; pairIdx < static_cast<size_t>(TradingPair::COUNT); ++pairIdx) {
        const auto& info = TradingPairData::pairData[pairIdx];
        for (const auto& [ex, symbol] : info.exchangeSymbols) {
            std::string lower = symbol;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            maps[static_cast<size_t>(ex)].emplace(lower, static_cast<TradingPair>(pairIdx));
        }
    }
    return maps;
}();

// ---- API ----
const TradingPairInfo& TradingPairData::get(TradingPair pair) {
    if (pair >= TradingPair::COUNT) {
        throw std::runtime_error("Invalid trading pair " + std::to_string(static_cast<int>(pair)));
    }
    return pairData[static_cast<size_t>(pair)];
}

const std::string& TradingPairData::getSymbol(ExchangeId ex, TradingPair pair) {
    const auto& info = get(pair);
    auto it = info.exchangeSymbols.find(ex);
    if (it == info.exchangeSymbols.end()) {
        throw std::runtime_error("Symbol not found for exchange " + std::to_string(static_cast<int>(ex)) + " and trading pair " + std::to_string(static_cast<int>(pair)));
    }
    return it->second;
}

int TradingPairData::getPrecision(TradingPair pair) {
    const auto& info = get(pair);
    return info.precision;
}

TradingPair TradingPairData::fromSymbol(ExchangeId ex, const std::string& symbol) {
    if (ex == ExchangeId::UNKNOWN || ex >= ExchangeId::COUNT) {
        throw std::runtime_error("Invalid exchange " + std::to_string(static_cast<int>(ex)));
    }
    std::string lower = symbol;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const auto& map = symbolMaps[static_cast<size_t>(ex)];
    auto it = map.find(lower);
    if (it == map.end()) {
        throw std::runtime_error("Symbol not found for exchange " + std::to_string(static_cast<int>(ex)) + " and symbol " + symbol);
    }
    return it->second;
}
