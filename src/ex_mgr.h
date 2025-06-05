#pragma once

#include "types.h"
#include "api_exchange.h"
#include "orderbook_mgr.h"
#include <vector>
#include <memory>
#include <map>
#include "tracer.h"

class ExchangeManager : public Traceable {
public:
    ExchangeManager(const std::vector<TradingPair> pairs);
    ~ExchangeManager();

    // Initialize and connect to exchanges
    bool initializeExchanges(const std::vector<ExchangeId>& exchangeIds);
    
    // Get exchange API instance
    ApiExchange* getExchange(ExchangeId id) const;
    
    // Get order book manager
    OrderBookManager& getOrderBookManager() { return orderBookManager; }
    
    // Connect to all exchanges
    bool connectAll();
    
    // Disconnect from all exchanges
    void disconnectAll();
    
    // Subscribe to order book for all exchanges
    bool subscribeAllOrderBooks();
    
    // Get order book snapshots for all exchanges
    bool getOrderBookSnapshots(TradingPair pair);

    // For TRACE identification
    const std::vector<ExchangeId>& getExchangeIds() const { return exchangeIds; }

protected:
    void trace(std::ostream& os) const override {
        for (const auto& id : exchangeIds) {
            os << id << " ";
        }
    }

private:
    std::map<ExchangeId, std::unique_ptr<ApiExchange>> exchanges;
    std::vector<ExchangeId> exchangeIds;
    std::vector<TradingPair> m_pairs;
}; 

extern ExchangeManager& exchangeManager;
