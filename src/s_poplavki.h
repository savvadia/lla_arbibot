#ifndef S_POPLAVKI_H
#define S_POPLAVKI_H

#include <string>
#include "balance.h"
#include "timers.h"

class Strategy
{
private:
    std::string name;
    std::string coin;       // coin to trade
    std::string stableCoin; // coin to trade against
    BalanceData balances;

public:
    TimersMgr &timersMgr;
    Strategy(std::string name, std::string coin, std::string stableCoin, TimersMgr &timersMgr);

    void setBalances(BalanceData balances);

    virtual void execute() = 0;
    static void timerCallback(int id, void *data) {};

    std::string getName() const;
};

class StrategyPoplavki : public Strategy
{
public:
    StrategyPoplavki(std::string coin, std::string stableCoin, TimersMgr &timersMgr);
    static void timerCallback(int id, void *data);
    void execute() override;
};

#endif // S_POPLAVKI_H