#include <iostream>
#include <string>
#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"

Strategy::Strategy(std::string name, std::string coin, std::string stableCoin, TimersMgr &timersMgr) : name(name), coin(coin), stableCoin(stableCoin), timersMgr(timersMgr)
{
    balances = {};
}

void Strategy::setBalances(BalanceData balances)
{
    this->balances = balances;
    // validate that balances contain only the coins we need
    for (const auto &exchange : balances)
    {
        for (const auto &coin : exchange.second)
        {
            if (coin.first != this->coin && coin.first != this->stableCoin)
            {
                throw std::invalid_argument("Invalid coin in balances: " + coin.first + ", expected: " + this->coin + " or " + this->stableCoin);
            }
        }
    }
}

std::string Strategy::getName() const
{
    return name;
}

StrategyPoplavki::StrategyPoplavki(std::string coin, std::string stableCoin, TimersMgr &timersMgr) : Strategy("Poplavki", coin, stableCoin, timersMgr)
{
    timersMgr.addTimer(1000, timerCallback, this);
}
void StrategyPoplavki::timerCallback(int id, void *data)
{
    // std::cout << "Timer callback" << std::endl;
    StrategyPoplavki *strategy = static_cast<StrategyPoplavki *>(data);
    strategy->timersMgr.addTimer(1000, timerCallback, strategy);
    strategy->execute();
}
void StrategyPoplavki::execute()
{
    std::cout << "Executing Poplavki strategy" << std::endl;
}
