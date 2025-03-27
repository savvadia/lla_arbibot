#include "orderbook.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)

void OrderBook::update(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    std::lock_guard<std::mutex> lock(mutex);
    this->bids = bids;
    this->asks = asks;
    TRACE("Updated with ", bids.size(), " bids and ", asks.size(), " asks");

    if (!bids.empty() && !asks.empty()) {
        TRACE("Best bid: ", std::fixed, std::setprecision(2), bids[0].price, " (", std::setprecision(4), bids[0].quantity, 
              "), best ask: ", std::setprecision(2), asks[0].price, " (", std::setprecision(4), asks[0].quantity, ")");
    }
}

void OrderBook::updatePriceLevel(bool isBid, double price, double quantity) {
    std::lock_guard<std::mutex> lock(mutex);
    auto& levels = isBid ? bids : asks;
    
    // Find the position to insert/update
    auto it = std::lower_bound(levels.begin(), levels.end(), price,
        [isBid](const PriceLevel& level, double p) { 
            return isBid ? level.price > p : level.price < p;
        });
    
    if (it != levels.end() && it->price == price) {
        // Update existing level
        it->quantity = quantity;
    } else {
        // Insert new level
        levels.insert(it, PriceLevel(price, quantity));
    }
}

void OrderBook::updateBids(const std::vector<PriceLevel>& newBids) {
    std::lock_guard<std::mutex> lock(mutex);
    bids = newBids;
    TRACE("Updated bids with ", bids.size(), " levels");
}

void OrderBook::updateAsks(const std::vector<PriceLevel>& newAsks) {
    std::lock_guard<std::mutex> lock(mutex);
    asks = newAsks;
    TRACE("Updated asks with ", asks.size(), " levels");
}

OrderBookManager::OrderBookManager() {
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto& book = orderBooks[pair];
    book.update(bids, asks);
    
    // Notify callback if set
    if (updateCallback) {
        updateCallback(exchangeId, pair, book);
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, double price, double quantity, bool isBid) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto& book = orderBooks[pair];
    book.updatePriceLevel(isBid, price, quantity);
    
    // Notify callback if set
    if (updateCallback) {
        updateCallback(exchangeId, pair, book);
    }
}

OrderBook& OrderBookManager::getOrderBook(TradingPair pair) {
    std::lock_guard<std::mutex> lock(mutex);
    return orderBooks[pair];
}

void OrderBookManager::setUpdateCallback(std::function<void(ExchangeId, TradingPair, const OrderBook&)> callback) {
    std::lock_guard<std::mutex> lock(mutex);
    updateCallback = callback;
} 