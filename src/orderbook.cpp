#include "orderbook.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)

void OrderBook::update(const std::vector<PriceLevel>& newBids, const std::vector<PriceLevel>& newAsks) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Process bids
    for (const auto& bid : newBids) {
        auto it = std::lower_bound(bids.begin(), bids.end(), bid.price,
            [](const PriceLevel& level, double p) { return level.price > p; });
            
        if (it != bids.end() && it->price == bid.price) {
            if (bid.quantity > 0) {
                it->quantity = bid.quantity;  // Update quantity
            } else {
                bids.erase(it);  // Remove price level
            }
        } else if (bid.quantity > 0) {
            bids.insert(it, bid);  // Add new price level
        }
    }
    
    // Process asks
    for (const auto& ask : newAsks) {
        auto it = std::lower_bound(asks.begin(), asks.end(), ask.price,
            [](const PriceLevel& level, double p) { return level.price < p; });
            
        if (it != asks.end() && it->price == ask.price) {
            if (ask.quantity > 0) {
                it->quantity = ask.quantity;  // Update quantity
            } else {
                asks.erase(it);  // Remove price level
            }
        } else if (ask.quantity > 0) {
            asks.insert(it, ask);  // Add new price level
        }
    }
    
    // Sort and clean up empty levels
    bids.erase(std::remove_if(bids.begin(), bids.end(), 
        [](const PriceLevel& level) { return level.quantity <= 0; }), bids.end());
    asks.erase(std::remove_if(asks.begin(), asks.end(), 
        [](const PriceLevel& level) { return level.quantity <= 0; }), asks.end());
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << getBestBid() 
        << " (" << std::setprecision(4) << getBestBidQuantity() << ") "
        << "ask: " << std::setprecision(2) << getBestAsk() 
        << " (" << std::setprecision(4) << getBestAskQuantity() << ")";
    
    TRACE("Updated with ", newBids.size(), " bid updates and ", newAsks.size(), " ask updates: ", oss.str());
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

    TRACE("Updated ", (isBid ? "bid" : "ask"), " level: ", price, "(", quantity, ")");
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
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Initialize the book if it doesn't exist
        if (orderBooks.find(pair) == orderBooks.end()) {
            orderBooks.try_emplace(pair, exchangeId, pair);
        }
        
        auto& book = orderBooks[pair];
        book.update(bids, asks);
    }
    
    // Call callback outside the lock
    if (updateCallback) {
        updateCallback(exchangeId, pair, orderBooks[pair]);
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, double price, double quantity, bool isBid) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Initialize the book if it doesn't exist
        if (orderBooks.find(pair) == orderBooks.end()) {
            orderBooks.try_emplace(pair, exchangeId, pair);
        }
        
        auto& book = orderBooks[pair];
        book.updatePriceLevel(isBid, price, quantity);
    }
    
    // Call callback outside the lock
    if (updateCallback) {
        updateCallback(exchangeId, pair, orderBooks[pair]);
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