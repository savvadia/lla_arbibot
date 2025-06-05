#pragma once

#include <string>
#include <unordered_map>
#include <iostream>
#include "tracer.h"

using BalanceData = std::unordered_map<std::string, std::unordered_map<std::string, double>>;

class BalanceManager : public Traceable {
private:
    BalanceData balances;

public:
    BalanceManager();
    void inc(const std::string& exchange, const std::string& coin, double amount);
    void dec(const std::string& exchange, const std::string& coin, double amount);
    double get(const std::string& exchange, const std::string& coin) const;
    const BalanceData getBalances() const;
    void retrieveBalances();

protected:
    void trace(std::ostream& os) const override {
        // for (const auto& exchange : balances) {
        //     os << exchange.first << "{";
        //     for (const auto& coin : exchange.second) {
        //         os << std::fixed << std::setprecision(8) 
        //            << coin.first << "=" << coin.second << " ";
        //     }
        //     os << "} ";
        // }
    }
};

extern BalanceManager& balanceManager;
