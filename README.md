# LLA Arbitrage Bot

A simple arbitrage bot for cryptocurrency trading.

## Setup

# Brew install
```
brew install gcc
brew install cmake
brew install jsoncpp
brew install curl
brew install websocketpp
brew install nlohmann_json
```

# Clean build from scratch:

```
cd ~/repo/lla_arbibot/ && rm -rf build && mkdir build && cd build && cmake -B . -S .. && make -j
```

# run

```sh
make clean
make
make run
```

## Architecture

Exchange
    subscribeForOrderBookUpdates
    placeOrder
    cancelOrder

ArbitrageManager
    scanOrderBooks

Arbitrage
    handleOrderStatusChange

Balance
    updateCoinAvgPrice

Order
    place
    cancel
