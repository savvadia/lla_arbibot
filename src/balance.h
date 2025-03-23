#ifndef BALANCE_H
#define BALANCE_H

#include <string>
#include <unordered_map>

using namespace std;

class Balance {
    unordered_map<std::string,  unordered_map<std::string, double>> balances; // exchange -> coin -> amount
public:
    Balance();
    void increaseBalance(std::string exchange, std::string coin, double amount);
    void decreaseBalance(std::string exchange, std::string coin, double amount);
    void retrieveBalances();
    double getBalance(std::string exchange, std::string coin);
    const unordered_map<std::string,  unordered_map<std::string, double>> getBalances() const;
    friend std::ostream& operator<<(std::ostream& os, const Balance& balance);
};

#endif // BALANCE_H