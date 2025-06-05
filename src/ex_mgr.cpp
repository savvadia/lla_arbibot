#include "ex_mgr.h"
#include "tracer.h"

// Define TRACE macro for ExchangeManager
#define TRACE(...) TRACE_THIS(TraceInstance::EX_MGR, ExchangeId::UNKNOWN, __VA_ARGS__)

bool ExchangeManager::initializeExchanges(
  const std::vector<TradingPair>& pairs,
  const std::vector<ExchangeId>& exchangeIds) {
  TRACE("initializing exchanges");
  m_pairs = pairs;
  // Clear any existing exchanges
  exchanges.clear();
  this->exchangeIds = exchangeIds;

  TRACE("initializing ", exchangeIds.size(), " exchanges");

  for (const auto &exchangeId : exchangeIds) {
    TRACE("creating exchange API for: ", exchangeId);

    std::unique_ptr<ApiExchange> api =
        createApiExchange(exchangeId, m_pairs, true);
    if (!api) {
      TRACE("failed to create exchange API for: ", exchangeId);
      return false;
    }

    exchanges[exchangeId] = std::move(api);
  }
  return true;
}

ApiExchange *ExchangeManager::getExchange(ExchangeId id) const {
  auto it = exchanges.find(id);
  return it != exchanges.end() ? it->second.get() : nullptr;
}

bool ExchangeManager::connectAll() {
  for (const auto &[exchangeId, api] : exchanges) {
    if (!api->connect()) {
      return false;
    }
  }
  return true;
}

void ExchangeManager::disconnectAll() {
  TRACE("disconnecting from all exchanges");

  for (const auto &[exchangeId, api] : exchanges) {
    if (api) {
      std::string exchangeName =
          (exchangeId == ExchangeId::BINANCE) ? "BINANCE" : "KRAKEN";
      TRACE("disconnecting from ", exchangeName);
      api->disconnect();
    }
  }
}

bool ExchangeManager::subscribeAllOrderBooks() {
  for (const auto &[exchangeId, api] : exchanges) {
    TRACE("subscribing to order book for ", exchangeId);
    if (!api->subscribeOrderBook()) {
      TRACE("failed to subscribe to order book for ", exchangeId);
      return false;
    }
  }

  return true;
}

bool ExchangeManager::getOrderBookSnapshots(TradingPair pair) {
  TRACE("Ignored getting order book snapshot for ", pair);
  return true;
}
