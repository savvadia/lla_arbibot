#include "orderbook.h"
#include <algorithm>

void OrderBook::updatePriceLevel(bool isBid, double price, double quantity) {
    std::lock_guard<std::mutex> lock(mutex);
    auto& levels = isBid ? bids : asks;
    
    // Find existing price level
    auto it = std::find_if(levels.begin(), levels.end(),
        [price](const PriceLevel& level) { return level.price == price; });
    
    if (it != levels.end()) {
        // Update existing level
        it->quantity = quantity;
    } else {
        // Add new level
        levels.push_back(PriceLevel(price, quantity));
    }
    
    // Sort levels
    std::sort(levels.begin(), levels.end(),
        [isBid](const PriceLevel& a, const PriceLevel& b) {
            return isBid ? a.price > b.price : a.price < b.price;
        });
}

void OrderBook::updateBids(const std::vector<PriceLevel>& newBids) {
    std::lock_guard<std::mutex> lock(mutex);
    bids = newBids;
    std::sort(bids.begin(), bids.end(),
        [](const PriceLevel& a, const PriceLevel& b) { return a.price > b.price; });
}

void OrderBook::updateAsks(const std::vector<PriceLevel>& newAsks) {
    std::lock_guard<std::mutex> lock(mutex);
    asks = newAsks;
    std::sort(asks.begin(), asks.end(),
        [](const PriceLevel& a, const PriceLevel& b) { return a.price < b.price; });
} 