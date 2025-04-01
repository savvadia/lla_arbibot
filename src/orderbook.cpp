#include "orderbook.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDERBOOK, __VA_ARGS__)

bool OrderBook::hasPricesChanged(const BestPrices& oldPrices, const BestPrices& newPrices) const {
    return oldPrices.bestBid != newPrices.bestBid ||
           oldPrices.bestAsk != newPrices.bestAsk ||
           oldPrices.worstBid != newPrices.worstBid ||
           oldPrices.worstAsk != newPrices.worstAsk;
}

// Helper function to merge sorted lists while maintaining size limit
void mergeSortedLists(std::vector<PriceLevel>& oldList, const std::vector<PriceLevel>& newList, 
                     bool isBid, double& totalAmount, double limit) {
    std::vector<PriceLevel> result;
    result.reserve(oldList.size() + newList.size());
    
    // Handle empty lists
    if (oldList.empty()) {
        for (auto it = newList.rbegin(); it != newList.rend() && totalAmount < limit; ++it) {
            if (it->quantity > 0) {
                result.push_back(*it);
                totalAmount += it->price * it->quantity;
            }
        }
        std::reverse(result.begin(), result.end());
        oldList = std::move(result);
        return;
    }
    
    if (newList.empty()) {
        for (auto it = oldList.rbegin(); it != oldList.rend() && totalAmount < limit; ++it) {
            result.push_back(*it);
            totalAmount += it->price * it->quantity;
        }
        std::reverse(result.begin(), result.end());
        oldList = std::move(result);
        return;
    }
    
    auto oldIt = oldList.rbegin();  // Start from best prices
    auto newIt = newList.rbegin();  // Start from best prices
    
    while (oldIt != oldList.rend() && newIt != newList.rend()) {
        if (isBid ? (newIt->price > oldIt->price) : (newIt->price < oldIt->price)) {
            // New price is better, add it
            result.push_back(*newIt);
            totalAmount += newIt->price * newIt->quantity;
            ++newIt;
        } else if (newIt->price == oldIt->price) {
            // Same price, update quantity
            if (newIt->quantity > 0) {
                result.push_back(*newIt);
                totalAmount += newIt->price * newIt->quantity;
            }
            ++newIt;
            ++oldIt;
        } else {
            // Old price is better, keep it
            result.push_back(*oldIt);
            totalAmount += oldIt->price * oldIt->quantity;
            ++oldIt;
        }
        
        // Check if we've reached the limit
        if (totalAmount >= limit) {
            break;
        }
    }
    
    // Add remaining elements from old list if we haven't reached the limit
    while (oldIt != oldList.rend() && totalAmount < limit) {
        result.push_back(*oldIt);
        totalAmount += oldIt->price * oldIt->quantity;
        ++oldIt;
    }
    
    // Add remaining elements from new list if we haven't reached the limit
    while (newIt != newList.rend() && totalAmount < limit) {
        if (newIt->quantity > 0) {
            result.push_back(*newIt);
            totalAmount += newIt->price * newIt->quantity;
        }
        ++newIt;
    }
    
    // Reverse to maintain order (best prices at end)
    std::reverse(result.begin(), result.end());
    oldList = std::move(result);
}

void OrderBook::update(const std::vector<PriceLevel>& newBids, const std::vector<PriceLevel>& newAsks) {
    BestPrices oldPrices = getBestPrices();
    bool pricesChanged = false;
    
    {
        
        // If we receive a large number of levels, treat it as a snapshot
        if (newBids.size() > 100 || newAsks.size() > 100) {
            MUTEX_LOCK(mutex);
            // For snapshots, accumulate levels until we reach MAX_ORDER_BOOK_AMOUNT
            bids.clear();
            asks.clear();
            
            // Process bids (descending order)
            double totalBidAmount = 0.0;
            for (auto it = newBids.rbegin(); it != newBids.rend() && totalBidAmount < Config::MAX_ORDER_BOOK_AMOUNT; ++it) {
                if (it->quantity > 0) {
                    bids.push_back(*it);
                    totalBidAmount += it->price * it->quantity;
                }
            }
            
            // Process asks (ascending order)
            double totalAskAmount = 0.0;
            for (auto it = newAsks.begin(); it != newAsks.end() && totalAskAmount < Config::MAX_ORDER_BOOK_AMOUNT; ++it) {
                if (it->quantity > 0) {
                    asks.push_back(*it);
                    totalAskAmount += it->price * it->quantity;
                }
            }
            
            pricesChanged = true;
        } else {
            {
                MUTEX_LOCK(mutex);

                // For updates, merge with existing levels
                double totalBidAmount = 0.0;
                mergeSortedLists(bids, newBids, true, totalBidAmount, Config::MAX_ORDER_BOOK_AMOUNT);
                
                double totalAskAmount = 0.0;
                mergeSortedLists(asks, newAsks, false, totalAskAmount, Config::MAX_ORDER_BOOK_AMOUNT);
            }
            // Check if prices changed
            BestPrices newPrices = getBestPrices();
            pricesChanged = hasPricesChanged(oldPrices, newPrices);
        }
    }
    
    // Update lastUpdate only if prices changed
    if (pricesChanged) {
        {
            MUTEX_LOCK(mutex);
            lastUpdate = std::chrono::system_clock::now();
        }
        TRACE("Order book updated");
    }
}
void OrderBook::updatePriceLevel(bool isBid, double price, double quantity) {
    std::vector<PriceLevel> newBids, newAsks;
    if (isBid) {
        newBids = {{price, quantity}};
    } else {
        newAsks = {{price, quantity}};
    }
    update(newBids, newAsks);
}

OrderBookManager::OrderBookManager() {
    // Initialize order books for each exchange and trading pair
    for (const auto& exchangeId : {ExchangeId::BINANCE, ExchangeId::KRAKEN}) {
        for (const auto& pair : {TradingPair::BTC_USDT, TradingPair::ETH_USDT}) {
            orderBooks[exchangeId][pair] = OrderBook(exchangeId, pair);
        }
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    bool changed = false;
    {
        MUTEX_LOCK(mutex);
    
        auto& book = orderBooks[exchangeId][pair];
        std::chrono::system_clock::time_point lastUpdate = book.getLastUpdate();
        
        // Log current state before update
        auto [oldBids, oldAsks] = book.getState();
        DEBUG("Before update - Exchange: ", exchangeId, " Pair: ", pair, " book: ", book);
        
        // Update the order book
        book.update(bids, asks);

        // Only trigger callback if the lastUpdate timestamp changed
        if (book.getLastUpdate() > lastUpdate) {
            changed = true;
            // Log new state after update
            DEBUG("After update - Exchange: ", exchangeId, " Pair: ", pair, " book: ", book);
        }
    }

    if (changed) {        
        if (updateCallback) {
            DEBUG("Calling update callback for exchange: ", exchangeId, " pair: ", pair);
            updateCallback(exchangeId, pair);
        } else {
            TRACE("No update callback set for exchange: ", exchangeId, " pair: ", pair);
        }
    } else {
        DEBUG("Update skipped - no changes detected for exchange: ", exchangeId, " pair: ", pair);
    }
}

OrderBook& OrderBookManager::getOrderBook(ExchangeId exchangeId, TradingPair pair) {
    MUTEX_LOCK(mutex);
    return orderBooks[exchangeId][pair];
}

std::vector<std::reference_wrapper<OrderBook>> OrderBookManager::getOrderBooks(TradingPair pair) {
    MUTEX_LOCK(mutex);
    std::vector<std::reference_wrapper<OrderBook>> books;
    
    // Get all order books for the given trading pair across exchanges
    for (auto& [exchangeId, exchangeBooks] : orderBooks) {
        auto it = exchangeBooks.find(pair);
        if (it != exchangeBooks.end()) {
            books.push_back(std::ref(it->second));
        }
    }
    
    return books;
}

std::vector<std::reference_wrapper<OrderBook>> OrderBookManager::getOrderBooks(ExchangeId exchangeId) {
    MUTEX_LOCK(mutex);
    std::vector<std::reference_wrapper<OrderBook>> books;
    
    // Get all order books for the given exchange
    auto it = orderBooks.find(exchangeId);
    if (it != orderBooks.end()) {
        for (auto& [pair, book] : it->second) {
            books.push_back(std::ref(book));
        }
    }
    
    return books;
}

void OrderBookManager::setUpdateCallback(std::function<void(ExchangeId, TradingPair)> callback) {
    MUTEX_LOCK(mutex);
    updateCallback = callback;
} 