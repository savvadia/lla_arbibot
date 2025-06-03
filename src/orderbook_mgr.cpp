#include "orderbook_mgr.h"

#define TRACE(_exchangeId, ...) TRACE_THIS(TraceInstance::ORDERBOOK_MGR, _exchangeId, __VA_ARGS__)
#define DEBUG(_exchangeId, ...) DEBUG_THIS(TraceInstance::ORDERBOOK_MGR, _exchangeId, __VA_ARGS__)
#define NOTICE(_exchangeId, ...) DEBUG_BASE(TraceInstance::ORDERBOOK_MGR, _exchangeId, __VA_ARGS__)
#define ERROR(_exchangeId, ...) ERROR_BASE(TraceInstance::ORDERBOOK_MGR, _exchangeId, __VA_ARGS__)
#define ERROR_CNT(_id, _exchangeId, ...) ERROR_COUNT(TraceInstance::ORDERBOOK_MGR, _exchangeId, _id, __VA_ARGS__)

OrderBookManager::OrderBookManager() {
    for (int pair = 0; pair < static_cast<int>(TradingPair::COUNT); pair++) {
        if (pair == static_cast<int>(TradingPair::UNKNOWN)) continue;
        for (int exchangeIx = static_cast<int>(ExchangeId::UNKNOWN) + 1; 
            exchangeIx < static_cast<int>(ExchangeId::COUNT); exchangeIx++) {
            if (exchangeIx == static_cast<int>(ExchangeId::UNKNOWN)) continue;
            ExchangeId exchangeId = static_cast<ExchangeId>(exchangeIx);
            orderBooks[exchangeId][static_cast<TradingPair>(pair)] = OrderBook(exchangeId, static_cast<TradingPair>(pair));
        }
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks, bool isCompleteUpdate, int maxDepth) {
    bool changed = false;
    {
        MUTEX_LOCK(mutex);
    
        auto& book = orderBooks[exchangeId][pair];
        
        // Update the order book
        auto result = book.update(pair, bids, asks, isCompleteUpdate, maxDepth);
        if(result == OrderBook::UpdateOutcome::UPDATE_ERROR) {
            return;
        }
        
        // Only trigger callback if the lastUpdate timestamp changed
        if (result == OrderBook::UpdateOutcome::BEST_PRICES_CHANGED) {
            changed = true;
        }

        NOTICE(exchangeId, "Update order book - Exchange: ", exchangeId, " Pair: ", pair, 
               " calling callback: ", changed, 
               " updated: ", book.getLastUpdate());
    }

    if (changed) {        
        if (updateCallback) {
            TRACE(exchangeId, "Calling update callback for exchange: ", exchangeId, " pair: ", pair);
            updateCallback(exchangeId, pair);
        }
    }
}

void OrderBookManager::updateOrderBookBestBidAsk(ExchangeId exchangeId, TradingPair pair, 
                                                double bidPrice, double bidQuantity, 
                                                double askPrice, double askQuantity) {
    bool changed = false;
    TRACE(exchangeId, "Updating order book best bid/ask - Exchange: ", exchangeId, " Pair: ", pair, 
          " Bid: ", bidPrice, "@", bidQuantity,
          " Ask: ", askPrice, "@", askQuantity);
    
    try {
        MUTEX_LOCK(mutex);
    
        auto& book = orderBooks[exchangeId][pair];
        
        // Update the order book with best bid/ask
        auto result = book.setBestBidAsk(bidPrice, bidQuantity, askPrice, askQuantity);
        if(result == OrderBook::UpdateOutcome::UPDATE_ERROR) {
            return;
        }
        
        // Only trigger callback if the lastUpdate timestamp changed
        if (result == OrderBook::UpdateOutcome::BEST_PRICES_CHANGED) {
            changed = true;
        }

        NOTICE("Update order book best bid/ask - Exchange: ", exchangeId, " Pair: ", pair, 
               " Bid: ", bidPrice, "@", bidQuantity,
               " Ask: ", askPrice, "@", askQuantity,
               " calling callback: ", changed, 
               " updated: ", book.getLastUpdate());
    } catch (const std::exception& e) {
        ERROR_CNT(exchangeId, CountableTrace::A_UNKNOWN_ERROR, "Error updating order book best bid/ask: ", e.what(), " Exchange: ", exchangeId, " Pair: ", pair);
    }

    if (changed) {        
        if (updateCallback) {
            TRACE(exchangeId, "Calling update callback for exchange: ", exchangeId, " pair: ", pair);
            updateCallback(exchangeId, pair);
        } else {
            TRACE(exchangeId, "No update callback for exchange: ", exchangeId, " pair: ", pair);
        }
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