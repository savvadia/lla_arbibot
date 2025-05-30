#pragma once

#include "orderbook.h"
#include "tracer.h"

// Order book manager for all trading pairs
class OrderBookManager : public Traceable {
public:
    OrderBookManager();
    ~OrderBookManager() = default;

    // Update order book for a trading pair
    void updateOrderBook(ExchangeId exchangeId, TradingPair pair, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks,
                        bool isCompleteUpdate = false, int maxDepth = 10);

    // Update order book with best bid/ask prices (for bookTicker style updates)
    void updateOrderBookBestBidAsk(ExchangeId exchangeId, TradingPair pair, 
                                 double bidPrice, double bidQuantity, 
                                 double askPrice, double askQuantity);

    // Get order book for a specific exchange and trading pair
    OrderBook& getOrderBook(ExchangeId exchangeId, TradingPair pair);

    // Get all order books for a trading pair
    std::vector<std::reference_wrapper<OrderBook>> getOrderBooks(TradingPair pair);

    // Get all order books for an exchange
    std::vector<std::reference_wrapper<OrderBook>> getOrderBooks(ExchangeId exchangeId);

    // Set callback for order book updates
    void setUpdateCallback(std::function<void(ExchangeId, TradingPair)> callback);

protected:
    void trace(std::ostream& os) const override {
        os << "OrderBookMgr";
    }

private:
    // Map of exchange ID to map of trading pair to order book
    std::unordered_map<ExchangeId, std::unordered_map<TradingPair, OrderBook>> orderBooks;
    std::function<void(ExchangeId, TradingPair)> updateCallback;
    mutable std::mutex mutex;
}; 