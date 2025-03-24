#pragma once

#include "types.h"
#include <string>

namespace trading {

// Convert TradingPair to Binance format string (for tracing)
inline std::string toString(TradingPair pair) {
    switch (pair) {
        case TradingPair::BTC_USDT: return "BTCUSDT";
        case TradingPair::ETH_USDT: return "ETHUSDT";
        case TradingPair::XTZ_USDT: return "XTZUSDT";
        default: return "UNKNOWN";
    }
}

} // namespace trading 