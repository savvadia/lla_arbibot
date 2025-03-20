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

### 1. Install Nix  
Ensure you have Nix installed on your system. If not, follow the instructions at [https://nixos.org/download.html](https://nixos.org/download.html).

### 2. Enter Nix Shell  
Run the following command to enter the development environment:  
```sh
nix-shell
```

### 3. Build and run

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
