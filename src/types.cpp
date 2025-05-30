#include "types.h"

#include <algorithm>

// ---- TradingPairInfo Table ----
const std::array<TradingPairInfo, static_cast<size_t>(TradingPair::COUNT)>
TradingPairData::pairData = {{
    {"UNKNOWN", "UNKNOWN", "UNKNOWN", 8, {}},
    {"ADA/USDT", "ADA", "USDT", 6, {{ExchangeId::BINANCE, "ADAUSDT"}, {ExchangeId::KRAKEN, "ADA/USD"}}},
    {"ALGO/USDT", "ALGO", "USDT", 5, {{ExchangeId::BINANCE, "ALGOUSDT"}, {ExchangeId::KRAKEN, "ALGO/USD"}}},
    {"ATOM/USDT", "ATOM", "USDT", 4, {{ExchangeId::BINANCE, "ATOMUSDT"}, {ExchangeId::KRAKEN, "ATOM/USD"}}},
    {"AVAX/USDT", "AVAX", "USDT", 2, {{ExchangeId::BINANCE, "AVAXUSDT"}, {ExchangeId::KRAKEN, "AVAX/USD"}}},
    {"BCH/USDT", "BCH", "USDT", 2, {{ExchangeId::BINANCE, "BCHUSDT"}, {ExchangeId::KRAKEN, "BCH/USD"}}},
    {"BTC/USDT", "BTC", "USDT", 1, {{ExchangeId::BINANCE, "BTCUSDT"}, {ExchangeId::KRAKEN, "BTC/USD"}}},
    {"DOGE/USDT", "DOGE", "USDT", 7, {{ExchangeId::BINANCE, "DOGEUSDT"}, {ExchangeId::KRAKEN, "DOGE/USD"}}},
    {"DOT/USDT", "DOT", "USDT", 4, {{ExchangeId::BINANCE, "DOTUSDT"}, {ExchangeId::KRAKEN, "DOT/USD"}}},
    {"EOS/USDT", "EOS", "USDT", 4, {{ExchangeId::BINANCE, "EOSUSDT"}, {ExchangeId::KRAKEN, "EOS/USD"}}},
    {"ETH/USDT", "ETH", "USDT", 2, {{ExchangeId::BINANCE, "ETHUSDT"}, {ExchangeId::KRAKEN, "ETH/USD"}}},
    {"LINK/USDT", "LINK", "USDT", 5, {{ExchangeId::BINANCE, "LINKUSDT"}, {ExchangeId::KRAKEN, "LINK/USD"}}},
    {"SOL/USDT", "SOL", "USDT", 2, {{ExchangeId::BINANCE, "SOLUSDT"}, {ExchangeId::KRAKEN, "SOL/USD"}}},
    {"XRP/USDT", "XRP", "USDT", 5, {{ExchangeId::BINANCE, "XRPUSDT"}, {ExchangeId::KRAKEN, "XRP/USD"}}},
    {"XTZ/USDT", "XTZ", "USDT", 4, {{ExchangeId::BINANCE, "XTZUSDT"}, {ExchangeId::KRAKEN, "XTZ/USD"}}},
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
    if (pair == TradingPair::UNKNOWN || pair >= TradingPair::COUNT) {
        throw std::runtime_error("Invalid trading pair");
    }
    return pairData[static_cast<size_t>(pair)];
}

const std::string& TradingPairData::getSymbol(ExchangeId ex, TradingPair pair) {
    const auto& info = get(pair);
    auto it = info.exchangeSymbols.find(ex);
    if (it == info.exchangeSymbols.end()) {
        throw std::runtime_error("Symbol not found for exchange");
    }
    return it->second;
}

int TradingPairData::getPrecision(TradingPair pair) {
    const auto& info = get(pair);
    return info.precision;
}

TradingPair TradingPairData::fromSymbol(ExchangeId ex, const std::string& symbol) {
    if (ex == ExchangeId::UNKNOWN || ex >= ExchangeId::COUNT) {
        return TradingPair::UNKNOWN;
    }
    std::string lower = symbol;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const auto& map = symbolMaps[static_cast<size_t>(ex)];
    auto it = map.find(lower);
    return it != map.end() ? it->second : TradingPair::UNKNOWN;
}
