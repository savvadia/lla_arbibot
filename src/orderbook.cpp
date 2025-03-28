#include "orderbook.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)
void OrderBook::update(const std::vector<PriceLevel>& newBids, const std::vector<PriceLevel>& newAsks) {

    {
        MUTEX_LOCK(mutex);
    
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
    }

    std::ostringstream oss;
    oss << "bid: " << std::fixed << std::setprecision(2) << getBestBid() 
        << " (" << std::setprecision(4) << getBestBidQuantity() << ") "
        << "ask: " << std::setprecision(2) << getBestAsk() 
        << " (" << std::setprecision(4) << getBestAskQuantity() << ")";
    
    DEBUG("Updated with ", newBids.size(), " bid updates and ", newAsks.size(), " ask updates: ", oss.str());
}

void OrderBook::updatePriceLevel(bool isBid, double price, double quantity) {
    MUTEX_LOCK(mutex);
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

    DEBUG("Updated price level ", isBid ? "bid" : "ask", " to ", price, " with quantity ", quantity);
}

OrderBookManager::OrderBookManager() {
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    // Initialize the book if it doesn't exist
    if (orderBooks.find(pair) == orderBooks.end()) {
        MUTEX_LOCK(mutex);
        orderBooks.try_emplace(pair, exchangeId, pair);
    }
    
    auto& book = orderBooks[pair];
    book.update(bids, asks);
    
    // Call callback outside the lock
    if (updateCallback) {
        updateCallback(exchangeId, pair, orderBooks[pair]);
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, double price, double quantity, bool isBid) {
    // Initialize the book if it doesn't exist
    if (orderBooks.find(pair) == orderBooks.end()) {
        MUTEX_LOCK(mutex);
        orderBooks.try_emplace(pair, exchangeId, pair);
    }
    
    auto& book = orderBooks[pair];
    book.updatePriceLevel(isBid, price, quantity);
    
    // Call callback outside the lock
    if (updateCallback) {
        updateCallback(exchangeId, pair, orderBooks[pair]);
    }
}

OrderBook& OrderBookManager::getOrderBook(TradingPair pair) {
    return orderBooks[pair];
}

void OrderBookManager::setUpdateCallback(std::function<void(ExchangeId, TradingPair, const OrderBook&)> callback) {
    updateCallback = callback;
    
    for (const auto& [pair, book] : orderBooks) {
        updateCallback(book.getExchangeId(), pair, book);
    }
} 