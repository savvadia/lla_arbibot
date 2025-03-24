#include "exchange_api.h"
#include "api_binance.h"
#include "api_kraken.h"
#include <stdexcept>

std::unique_ptr<ExchangeApi> createExchangeApi(const std::string& exchangeName, OrderBookManager& orderBookManager) {
    if (exchangeName == "Binance") {
        return std::make_unique<BinanceApi>(orderBookManager);
    } else if (exchangeName == "Kraken") {
        return std::make_unique<KrakenApi>(orderBookManager);
    }
    // Add more exchanges here as we implement them
    throw std::runtime_error("Unsupported exchange: " + exchangeName);
} 