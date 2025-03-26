void KrakenApi::processOrderBookUpdate(const json& data) {
    try {
        if (!data.contains("event") || data["event"] != "book") {
            return;
        }

        std::string symbol = data["pair"].get<std::string>();
        TradingPair pair = krakenSymbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            return;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["bids"]) {
            bids.push_back({
                std::stod(bid[0].get<std::string>()),  // price
                std::stod(bid[1].get<std::string>())   // quantity
            });
        }

        // Process asks
        for (const auto& ask : data["asks"]) {
            asks.push_back({
                std::stod(ask[0].get<std::string>()),  // price
                std::stod(ask[1].get<std::string>())   // quantity
            });
        }

        orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE("KRAKEN", "Error processing order book update: %s", e.what());
    }
}

void KrakenApi::processOrderBookSnapshot(const json& data) {
    try {
        if (!data.contains("pair")) {
            return;
        }

        std::string symbol = data["pair"].get<std::string>();
        TradingPair pair = krakenSymbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            return;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["bids"]) {
            bids.push_back({
                std::stod(bid[0].get<std::string>()),  // price
                std::stod(bid[1].get<std::string>())   // quantity
            });
        }

        // Process asks
        for (const auto& ask : data["asks"]) {
            asks.push_back({
                std::stod(ask[0].get<std::string>()),  // price
                std::stod(ask[1].get<std::string>())   // quantity
            });
        }

        orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE("KRAKEN", "Error processing order book snapshot: %s", e.what());
    }
} 