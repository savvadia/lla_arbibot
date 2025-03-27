#include "api_exchange.h"
#include "api_binance.h"
#include "api_kraken.h"
#include <stdexcept>

std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager) {
    if (exchangeName == "Binance") {
        return std::make_unique<ApiBinance>(orderBookManager);
    } else if (exchangeName == "Kraken") {
        return std::make_unique<ApiKraken>(orderBookManager);
    }
    // Add more exchanges here as we implement them
    throw std::runtime_error("Unsupported exchange: " + exchangeName);
} 