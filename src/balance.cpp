#include "balance.h"
#include "tracer.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <sstream>

using namespace std;

#define TRACE(...) TRACE_INST(TRACE_BALANCE, __VA_ARGS__)

Balance::Balance() {
    balances = {};
}

std::ostream& operator<<(std::ostream& os, const Balance& balance) {
    os << fixed << setprecision(5);
    os << "Balance { ";
    for (const auto& exchange : balance.balances) {
        os << exchange.first << " { ";
        for (const auto& coin : exchange.second) {
            os << coin.first << ": " << coin.second << ", ";
        }
        os << "}, ";
    }
    os << " }";
    return os;
}

void Balance::increaseBalance(std::string exchange, std::string coin, double amount) {
    TRACE("Inc for %s %s by %f", exchange.c_str(), coin.c_str(), amount);
    balances[exchange][coin] += amount;
}

void Balance::decreaseBalance(std::string exchange, std::string coin, double amount) {
    TRACE("Dec for %s %s by %f", exchange.c_str(), coin.c_str(), amount);
    balances[exchange][coin] -= amount;
}

double Balance::getBalance(std::string exchange, std::string coin) {
    TRACE("Get for %s %s: %f", exchange.c_str(), coin.c_str(), balances[exchange][coin]);
    return balances[exchange][coin];
}

const unordered_map<std::string,  unordered_map<std::string, double>> Balance::getBalances() const {
    return balances;
}

void Balance::retrieveBalances() {
    TRACE("Retrieving balances...");
    balances["kraken"]["BTC"] = 0.01;
    balances["kraken"]["USDT"] = 100.0;
    balances["binance"]["BTC"] = 0.02;
    balances["binance"]["USDT"] = 200.0;
    for (const auto& exchange : balances) {
        ostringstream oss;
        oss << exchange.first << " { ";
        for (const auto& coin : exchange.second) {
            oss << coin.first << ": " << coin.second << ", ";
        }
        oss << "}, ";
        TRACE("Balance @ %s", oss.str().c_str());
    }
}


