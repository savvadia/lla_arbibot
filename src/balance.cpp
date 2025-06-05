#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>

#include "balance.h"
#include "tracer.h"
#include "types.h"

using namespace std;

// Define TRACE macro for Balance class
#define TRACE(...) TRACE_THIS(TraceInstance::BALANCE, ExchangeId::UNKNOWN, __VA_ARGS__)

BalanceManager::BalanceManager() {
    balances = {};
}

void BalanceManager::inc(const std::string& exchange, const std::string& coin, double amount) {
    auto exchangeIt = balances.find(exchange);
    if (exchangeIt == balances.end()) {
        TRACE("Inc for ", exchange, " ", coin, " by ", amount, " (exchange not found)");
        return;
    }
    
    auto coinIt = exchangeIt->second.find(coin);
    if (coinIt == exchangeIt->second.end()) {
        TRACE("Inc for ", exchange, " ", coin, " by ", amount, " (coin not found)");
        return;
    }
    
    coinIt->second += amount;
    TRACE("Inc for ", exchange, " ", coin, " by ", amount);
}

void BalanceManager::dec(const std::string& exchange, const std::string& coin, double amount) {
    auto exchangeIt = balances.find(exchange);
    if (exchangeIt == balances.end()) {
        TRACE("Dec for ", exchange, " ", coin, " by ", amount, " (exchange not found)");
        return;
    }
    
    auto coinIt = exchangeIt->second.find(coin);
    if (coinIt == exchangeIt->second.end()) {
        TRACE("Dec for ", exchange, " ", coin, " by ", amount, " (coin not found)");
        return;
    }
    
    coinIt->second -= amount;
    TRACE("Dec for ", exchange, " ", coin, " by ", amount);
}

double BalanceManager::get(const std::string& exchange, const std::string& coin) const {
    auto exchangeIt = balances.find(exchange);
    if (exchangeIt == balances.end()) {
        TRACE("Get for ", exchange, " ", coin, ": 0.0 (exchange not found)");
        return 0.0;
    }
    
    auto coinIt = exchangeIt->second.find(coin);
    if (coinIt == exchangeIt->second.end()) {
        TRACE("Get for ", exchange, " ", coin, ": 0.0 (coin not found)");
        return 0.0;
    }
    
    TRACE("Get for ", exchange, " ", coin, ": ", coinIt->second);
    return coinIt->second;
}

const BalanceData BalanceManager::getBalances() const {
    return balances;
}

void BalanceManager::retrieveBalances() {
    TRACE("Retrieving balances...");
    balances["kraken"]["BTC"] = 0.01;
    balances["kraken"]["USDT"] = 100.0;
    balances["binance"]["BTC"] = 0.02;
    balances["binance"]["USDT"] = 200.0;
    balances["binance"]["XTZ"] = 0.003;
    balances["binance"]["ETH"] = 50.0;
    for (const auto& exchange : balances) {
        std::ostringstream oss;
        oss << exchange.first << ": ";
        for (const auto& coin : exchange.second) {
            oss << coin.first << "=" << coin.second << " ";
        }
        TRACE("Balance @ ", oss.str());
    }
}


