#include "orderbook.h"
#include <algorithm>
#include "tracer.h"


#define TRACE(...) TRACE_THIS(TraceInstance::ORDERBOOK, exchangeId, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::ORDERBOOK, exchangeId, __VA_ARGS__)
#define NOTICE(...) DEBUG_BASE(TraceInstance::ORDERBOOK, exchangeId, __VA_ARGS__)

bool OrderBook::hasPricesChanged(const BestPrices& oldPrices, const BestPrices& newPrices) const {
    if(oldPrices.bestBid != newPrices.bestBid) {
        NOTICE("changed bestBid: ", oldPrices.bestBid, "->", newPrices.bestBid);
        return true;
    }
    if(oldPrices.bestAsk != newPrices.bestAsk) {
        NOTICE("changed bestAsk: ", oldPrices.bestAsk, "->", newPrices.bestAsk);
        return true;
    }
    if(oldPrices.worstBid != newPrices.worstBid) {
        NOTICE("changed worstBid: ", oldPrices.worstBid, "->", newPrices.worstBid);
        return true;
    }
    if(oldPrices.worstAsk != newPrices.worstAsk) {
        NOTICE("changed worstAsk: ", oldPrices.worstAsk, "->", newPrices.worstAsk);
        return true;
    }
    return false;
}

bool OrderBook::isSorted(const std::vector<PriceLevel>& list, bool isBid) {
    if (list.empty()) return true;
    if (isBid) {
        // sort in descending order
        return std::is_sorted(list.begin(), list.end(), [](const PriceLevel& a, const PriceLevel& b) { return a.price > b.price; });
    } else {
        // sort in ascending order
        return std::is_sorted(list.begin(), list.end(), [](const PriceLevel& a, const PriceLevel& b) { return a.price < b.price; });
    }
}

void OrderBook::sortList(std::vector<PriceLevel>& list, bool isBid) {
    if (!OrderBook::isSorted(list, isBid)) {
        if (isBid) {
            // sort bids in descending order
            std::sort(list.begin(), list.end(), [](const PriceLevel& a, const PriceLevel& b) { return a.price > b.price; });
        } else {
            // sort asks in ascending order
            std::sort(list.begin(), list.end(), [](const PriceLevel& a, const PriceLevel& b) { return a.price < b.price; });
        }
    }
}

void OrderBook::pushElement(std::vector<PriceLevel>& result, std::vector<PriceLevel>::iterator& it, double& totalAmount, int scenario) {
    NOTICE("Pushing element - Price: ", it->price, " Quantity: ", it->quantity, " Scenario: ", scenario);
    if(it->price > 0 && it->quantity > 0) {
        result.push_back(*it);
        totalAmount += it->price * it->quantity;
    }
    it++;
}

// Helper function to merge sorted lists while maintaining size limit
void OrderBook::mergeSortedLists(std::vector<PriceLevel>& oldList, std::vector<PriceLevel>& newList, 
                     bool isBid, double& totalAmount, double limit) {

    if(newList.empty()) {
        NOTICE("New list is empty for ", isBid ? "bid" : "ask", ", returning");
        return;
    }

    MUTEX_LOCK(mutex);

    std::vector<PriceLevel> result;
    result.reserve(oldList.size() + newList.size());
    totalAmount = 0.0;  // Reset total amount

    // Sort both lists before merging
    OrderBook::sortList(oldList, isBid);
    OrderBook::sortList(newList, isBid);

    auto ito=oldList.begin();
    auto itn=newList.begin();
    while((ito != oldList.end() || itn != newList.end())) {
        int scenario = 0;
        bool pushOld = false;
        bool pushNew = false;

        NOTICE("Merging - Old it: ", 
            ito == oldList.end() ? "x" : "",
            ito != oldList.end() ? ito->price : 0.0, "/",
            ito != oldList.end() ? ito->quantity : 0.0,
            " New it: ", 
            itn == newList.end() ? "x" : "",
            itn != newList.end() ? itn->price : 0.0, "/",
            itn != newList.end() ? itn->quantity : 0.0);

        if(totalAmount > limit) {
            NOTICE("Total amount limit reached. skipping the test");
            break;
        }
        if(itn != newList.end() && (itn->price < 0.0 || itn->quantity < 0.0)) {
            NOTICE("New list has negative price or quantity, skipping: ", itn->price, " ", itn->quantity);
            itn++;
            continue;
        }
        if(ito != oldList.end() && ito->price == 0.0) {
            NOTICE("Old list has zero price, skipping: ", ito->price, " ", ito->quantity);
            ito++;
            continue;
        }

        if(ito == oldList.end()) {
            pushNew = true;
            scenario = 1;
        } else if(itn == newList.end()) {
            pushOld = true;
            scenario = 2;
        } else if(ito->price == itn->price) {
            if(itn->quantity > 0) {
                pushNew = true;
                scenario = 3;
            } else {
                NOTICE("Dropping old list level as new quantity is 0: ", itn->price);
                scenario = 4;
            }
            ito++; // in both cases we need to move to the old list element
        } else if (isBid) {
            if(ito->price > itn->price) { // highest goes first. smaller will be checked on next iteration
                pushOld = true;
                scenario = 5;
            } else {
                pushNew = true;
                scenario = 6;
            }
        } else {
            if(ito->price < itn->price) { // lowest goes first. bigger will be checked on next iteration
                pushOld = true;
                scenario = 7;
            } else {
                pushNew = true;
                scenario = 8;
            }
        }

        if(pushOld) {
            pushElement(result, ito, totalAmount, scenario);
        } else if(pushNew) {
            pushElement(result, itn, totalAmount, scenario);
        } else {
            NOTICE("Pushing - Scenario: ", scenario, " Push old: ", pushOld, " Push new: ", pushNew);
        }
    }

    // Sort the result list
    OrderBook::sortList(result, isBid);

    oldList = std::move(result);
    // trace belongs to exchange more than to the order book, so use 
    TRACE_BASE(TraceInstance::ORDERBOOK, exchangeId, " merged size: ", oldList.size(), " amount: ", totalAmount, " ",
        isBid ? "bids" : "asks", " ", traceBidsAsks(oldList));
}

void OrderBook::update(std::vector<PriceLevel>& newBids, std::vector<PriceLevel>& newAsks, bool isCompleteUpdate) {
    BestPrices oldPrices = getBestPrices();
    bool pricesChanged = false;
    TRACE("OrderBook update - Bids: ", newBids.size(), " Asks: ", newAsks.size(), " Complete update: ", isCompleteUpdate);
    
    {
        if (isCompleteUpdate) {
            // allocate mutex here to avoid deadlock. it will be used by orderbook manager
            MUTEX_LOCK(mutex);

            // For complete updates (like Binance bookTicker), replace all data
            bids.clear();
            asks.clear();
            
            // Validate bids are sorted in descending order
            if (!newBids.empty() && newBids.front().price < newBids.back().price) {
                TRACE("ERROR: Bids are not sorted in descending order");
                return;
            }
            
            // Process bids (descending order)
            for (const auto& bid : newBids) {
                if (bid.quantity > 0 && bid.price > 0) {  // Reject zero or negative prices
                    bids.push_back(bid);
                }
            }
            
            // Validate asks are sorted in ascending order
            if (!newAsks.empty() && newAsks.front().price > newAsks.back().price) {
                TRACE("ERROR: Asks are not sorted in ascending order");
                return;
            }
            
            // Process asks (ascending order)
            for (const auto& ask : newAsks) {
                if (ask.quantity > 0 && ask.price > 0) {  // Reject zero or negative prices
                    asks.push_back(ask);
                }
            }
            
            pricesChanged = true;
        } else {
            // we don't need MUTEX here and merge requests below will need it
            
            // For incremental updates (like Kraken), merge with existing data
            double totalBidAmount = 0.0;
            mergeSortedLists(bids, newBids, true, totalBidAmount, Config::MAX_ORDER_BOOK_AMOUNT);
            
            double totalAskAmount = 0.0;
            mergeSortedLists(asks, newAsks, false, totalAskAmount, Config::MAX_ORDER_BOOK_AMOUNT);
            DEBUG("After merge - Bids size: ", bids.size(), " ask size: ", asks.size());
        }
        
        // Check if prices changed
        BestPrices newPrices = getBestPrices();
        pricesChanged = hasPricesChanged(oldPrices, newPrices);
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

void OrderBook::setBestBidAsk(double bidPrice, double bidQuantity, double askPrice, double askQuantity) {
    BestPrices oldPrices = getBestPrices();
    bool pricesChanged = false;
    DEBUG("setBestBidAsk - Bid: ", bidPrice, "@", bidQuantity, " Ask: ", askPrice, "@", askQuantity);
    
    {
        MUTEX_LOCK(mutex);
        
        // Set new best bid if valid
        if (bids.size() == 1) {
            if (bids[0].price != bidPrice) {
                bids[0].price = bidPrice;
                pricesChanged = true;
            }
            bids[0].quantity = bidQuantity;
        } else if (bidPrice > 0 && bidQuantity > 0) {
            bids.clear();
            bids.push_back(PriceLevel(bidPrice, bidQuantity));
        }
        
        // Set new best ask if valid
        if (askPrice > 0 && askQuantity > 0) {
            if (asks.size() == 1) {
                if (asks[0].price != askPrice) {
                    asks[0].price = askPrice;
                    pricesChanged = true;
                }
                asks[0].quantity = askQuantity;
            } else {
                asks.clear();
                asks.push_back(PriceLevel(askPrice, askQuantity));
            }
        }
    }
    
    // these checks will need mutex so do not allocate mutex here
    // Check if prices changed
    BestPrices newPrices = getBestPrices();
    pricesChanged = hasPricesChanged(oldPrices, newPrices);
    
    // Update lastUpdate only if prices changed
    if (pricesChanged) {
        {
            MUTEX_LOCK(mutex);
            lastUpdate = std::chrono::system_clock::now();
        }
        // no traces under nutex
        TRACE("setBestBidAsk updated - b/a: ", bidPrice, "@", bidQuantity, " ", askPrice, "@", askQuantity, " u: ", lastUpdate);
    }
}

OrderBookManager::OrderBookManager() {
    // Initialize order books for each exchange and trading pair
    for (const auto& exchangeId : {ExchangeId::BINANCE, ExchangeId::KRAKEN}) {
        for (const auto& pair : {TradingPair::BTC_USDT, TradingPair::ETH_USDT, TradingPair::XTZ_USDT}) {
            orderBooks[exchangeId][pair] = OrderBook(exchangeId, pair);
        }
    }
}

void OrderBookManager::updateOrderBook(ExchangeId exchangeId, TradingPair pair, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks, bool isCompleteUpdate) {
    bool changed = false;
    {
        MUTEX_LOCK(mutex);
    
        auto& book = orderBooks[exchangeId][pair];
        std::chrono::system_clock::time_point prevLastUpdate = book.getLastUpdate();
        
        // Update the order book
        book.update(bids, asks, isCompleteUpdate);
        
        // Only trigger callback if the lastUpdate timestamp changed
        if (book.getLastUpdate() > prevLastUpdate) {
            changed = true;
        }

        NOTICE("Update order book - Exchange: ", exchangeId, " Pair: ", pair, 
               " calling callback: ", changed, 
               " last update: ", prevLastUpdate, 
               " new update: ", book.getLastUpdate());
    }

    if (changed) {        
        if (updateCallback) {
            TRACE("Calling update callback for exchange: ", exchangeId, " pair: ", pair);
            updateCallback(exchangeId, pair);
        }
    }
}

void OrderBookManager::updateOrderBookBestBidAsk(ExchangeId exchangeId, TradingPair pair, 
                                                double bidPrice, double bidQuantity, 
                                                double askPrice, double askQuantity) {
    bool changed = false;
    {
        MUTEX_LOCK(mutex);
    
        auto& book = orderBooks[exchangeId][pair];
        std::chrono::system_clock::time_point lastUpdate = book.getLastUpdate();
        
        // Update the order book with best bid/ask
        book.setBestBidAsk(bidPrice, bidQuantity, askPrice, askQuantity);
        
        // Only trigger callback if the lastUpdate timestamp changed
        if (book.getLastUpdate() > lastUpdate) {
            changed = true;
        }

        NOTICE("Update order book best bid/ask - Exchange: ", exchangeId, " Pair: ", pair, 
               " Bid: ", bidPrice, "@", bidQuantity,
               " Ask: ", askPrice, "@", askQuantity,
               " calling callback: ", changed, 
               " last update: ", lastUpdate, 
               " new update: ", book.getLastUpdate());
    }

    if (changed) {        
    if (updateCallback) {
            TRACE("Calling update callback for exchange: ", exchangeId, " pair: ", pair);
            updateCallback(exchangeId, pair);
    } else {
            TRACE("No update callback for exchange: ", exchangeId, " pair: ", pair);
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