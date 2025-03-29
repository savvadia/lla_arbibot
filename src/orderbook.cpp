#include "orderbook.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)

bool OrderBook::hasBestPricesChanged(bool isBid, double price, double quantity) const {
    // Note: This method should only be called while holding the mutex
    if (isBid) {
        if (bids.empty()) return quantity > 0;
        if (quantity <= 0 && price == bids[0].price) return true;
        return price > bids[0].price || (price == bids[0].price && quantity != bids[0].quantity);
    } else {
        if (asks.empty()) return quantity > 0;
        if (quantity <= 0 && price == asks[0].price) return true;
        return price < asks[0].price || (price == asks[0].price && quantity != asks[0].quantity);
    }
}

void OrderBook::update(const std::vector<PriceLevel>& newBids, const std::vector<PriceLevel>& newAsks) {
    bool bestPricesChanged = false;
    double oldBestBid = 0.0, oldBestAsk = 0.0;
    double oldBestBidQty = 0.0, oldBestAskQty = 0.0;
    
    {
        MUTEX_LOCK(mutex);
        
        // Store old best prices for comparison
        if (!bids.empty()) {
            oldBestBid = bids[0].price;
            oldBestBidQty = bids[0].quantity;
        }
        if (!asks.empty()) {
            oldBestAsk = asks[0].price;
            oldBestAskQty = asks[0].quantity;
        }
    
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
            
        // Check if best prices changed
        if (!bids.empty() && (oldBestBid != bids[0].price || oldBestBidQty != bids[0].quantity)) {
            bestPricesChanged = true;
        }
        if (!asks.empty() && (oldBestAsk != asks[0].price || oldBestAskQty != asks[0].quantity)) {
            bestPricesChanged = true;
        }
        
        // Update lastUpdate only if best prices changed
        if (bestPricesChanged) {
            lastUpdate = std::chrono::system_clock::now();
        }
    }

    std::ostringstream oss;
    oss << "bid: " << std::fixed << std::setprecision(2) << getBestBid() 
        << " (" << std::setprecision(4) << getBestBidQuantity() << ") "
        << "ask: " << std::setprecision(2) << getBestAsk() 
        << " (" << std::setprecision(4) << getBestAskQuantity() << ")";
    
    DEBUG("Updated with ", newBids.size(), " bid updates and ", newAsks.size(), " ask updates: ", oss.str());
}

void OrderBook::updatePriceLevel(bool isBid, double price, double quantity) {
    if (price <= 0.0) return;  // Ignore invalid prices
    if (quantity < 0.0) return;  // Ignore negative quantities
    
    bool bestPricesChanged = false;
    double oldBestPrice = 0.0, oldBestQty = 0.0;
    
    {
        MUTEX_LOCK(mutex);
        auto& levels = isBid ? bids : asks;
        
        // Store old best price for comparison
        if (!levels.empty()) {
            oldBestPrice = levels[0].price;
            oldBestQty = levels[0].quantity;
        }
        
        // Find the position to insert/update using binary search
        auto it = std::lower_bound(levels.begin(), levels.end(), price,
            [isBid](const PriceLevel& level, double p) { 
                return isBid ? level.price > p : level.price < p;
            });
        
        // Update or insert the price level
        if (it != levels.end() && std::abs(it->price - price) < 1e-10) {
            if (quantity > 0) {
                it->quantity = quantity;  // Update quantity
            } else {
                levels.erase(it);  // Remove price level
            }
        } else if (quantity > 0) {
            levels.insert(it, PriceLevel(price, quantity));  // Add new price level at sorted position
        }
        
        // Clean up empty levels
        levels.erase(std::remove_if(levels.begin(), levels.end(), 
            [](const PriceLevel& level) { return level.quantity <= 0; }), levels.end());
            
        // Check if best price changed
        if (!levels.empty()) {
            if (std::abs(oldBestPrice - levels[0].price) >= 1e-10 || 
                std::abs(oldBestQty - levels[0].quantity) >= 1e-10) {
                bestPricesChanged = true;
            }
        } else if (oldBestPrice != 0.0 || oldBestQty != 0.0) {
            bestPricesChanged = true;  // All levels were removed
        }
        
        // Update lastUpdate only if best prices changed
        if (bestPricesChanged) {
            lastUpdate = std::chrono::system_clock::now();
        }
    }

    DEBUG("Updated price level ", isBid ? "bid" : "ask", " to ", price, " with quantity ", quantity);
}

OrderBookManager::OrderBookManager() {
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    OrderBook* book = nullptr;
    {
        MUTEX_LOCK(mutex);
        if (orderBooks.find(pair) == orderBooks.end()) {
            orderBooks.try_emplace(pair, exchangeId, pair);
        }
        book = &orderBooks[pair];
    }
    
    if (book) {
        book->update(bids, asks);
        
        // Call callback outside the lock
        if (updateCallback) {
            updateCallback(exchangeId, pair, *book);
        }
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, double price, double quantity, bool isBid) {
    OrderBook* book = nullptr;
    {
        MUTEX_LOCK(mutex);
        if (orderBooks.find(pair) == orderBooks.end()) {
            orderBooks.try_emplace(pair, exchangeId, pair);
        }
        book = &orderBooks[pair];
    }
    
    if (book) {
        book->updatePriceLevel(isBid, price, quantity);
        
        // Call callback outside the lock
        if (updateCallback) {
            updateCallback(exchangeId, pair, *book);
        }
    }
}

OrderBook& OrderBookManager::getOrderBook(TradingPair pair) {
    MUTEX_LOCK(mutex);
    return orderBooks[pair];
}

void OrderBookManager::setUpdateCallback(std::function<void(ExchangeId, TradingPair, const OrderBook&)> callback) {
    updateCallback = callback;
    
    std::vector<std::pair<ExchangeId, std::pair<TradingPair, const OrderBook*>>> books;
    {
        MUTEX_LOCK(mutex);
        for (const auto& [pair, book] : orderBooks) {
            books.push_back({book.getExchangeId(), {pair, &book}});
        }
    }
    
    // Call callbacks outside the lock
    for (const auto& [exchangeId, pairAndBook] : books) {
        updateCallback(exchangeId, pairAndBook.first, *pairAndBook.second);
    }
} 