#ifndef BALANCE_H
#define BALANCE_H

#include <string>
#include <unordered_map>

using namespace std;

using BalanceData = unordered_map<std::string,  unordered_map<std::string, double>>;

class Balance {
    BalanceData balances; // exchange -> coin -> amount
public:
    Balance();
    void increaseBalance(std::string exchange, std::string coin, double amount);
    void decreaseBalance(std::string exchange, std::string coin, double amount);
    void retrieveBalances();
    double getBalance(std::string exchange, std::string coin);
    const BalanceData getBalances() const;
    friend std::ostream& operator<<(std::ostream& os, const Balance& balance);
};

#endif // BALANCE_H