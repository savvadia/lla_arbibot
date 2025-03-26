#include "balance.h"
#include "tracer.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <sstream>

using namespace std;

// Define TRACE macro for Balance class
#define TRACE(...) TRACE_THIS(TraceInstance::BALANCE, __VA_ARGS__)

Balance::Balance() {
    balances = {};
}

void Balance::inc(const std::string& exchange, const std::string& coin, double amount) {
    balances[exchange][coin] += amount;
    TRACE("Inc for ", exchange, " ", coin, " by ", amount);
}

void Balance::dec(const std::string& exchange, const std::string& coin, double amount) {
    balances[exchange][coin] -= amount;
    TRACE("Dec for ", exchange, " ", coin, " by ", amount);
}

double Balance::get(const std::string& exchange, const std::string& coin) const {
    TRACE("Get for ", exchange, " ", coin, ": ", balances.at(exchange).at(coin));
    return balances.at(exchange).at(coin);
}

const BalanceData Balance::getBalances() const {
    return balances;
}

void Balance::retrieveBalances() {
    TRACE("Retrieving balances...");
    balances["kraken"]["BTC"] = 0.01;
    balances["kraken"]["USDT"] = 100.0;
    balances["binance"]["BTC"] = 0.02;
    balances["binance"]["USDT"] = 200.0;
    for (const auto& exchange : balances) {
        std::ostringstream oss;
        oss << exchange.first << ": ";
        for (const auto& coin : exchange.second) {
            oss << coin.first << "=" << coin.second << " ";
        }
        TRACE("Balance @ ", oss.str());
    }
}


