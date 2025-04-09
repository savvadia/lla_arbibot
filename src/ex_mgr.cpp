#include "ex_mgr.h"
#include "tracer.h"

// Define TRACE macro for ExchangeManager
#define TRACE(...) TRACE_THIS(TraceInstance::EX_MGR, __VA_ARGS__)

ExchangeManager::ExchangeManager(TimersMgr &timersMgr,
                                 OrderBookManager &orderBookManager)
    : m_timersMgr(timersMgr), m_orderBookManager(orderBookManager) {
  TRACE("initializing");
}

ExchangeManager::~ExchangeManager() = default;

bool ExchangeManager::initializeExchanges(
    const std::vector<ExchangeId> &exchangeIds) {
  // Clear any existing exchanges
  exchanges.clear();
  this->exchangeIds = exchangeIds;

  TRACE("initializing ", exchangeIds.size(), " exchanges");

  for (const auto &exchangeId : exchangeIds) {
    std::string exchangeName =
        (exchangeId == ExchangeId::BINANCE) ? "Binance" : "Kraken";
    TRACE("creating exchange API for: ", exchangeName);

    std::unique_ptr<ApiExchange> api =
        createApiExchange(exchangeName, m_orderBookManager, m_timersMgr, true);
    if (!api) {
      TRACE("failed to create exchange API for: ", exchangeName);
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
    std::string exchangeName =
        (exchangeId == ExchangeId::BINANCE) ? "BINANCE" : "KRAKEN";
    TRACE("connecting to ", exchangeName);
    if (!api->connect()) {
      TRACE("failed to connect to ", exchangeName);
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

bool ExchangeManager::subscribeAllOrderBooks(TradingPair pair) {
  for (const auto &[exchangeId, api] : exchanges) {
    std::string exchangeName =
        (exchangeId == ExchangeId::BINANCE) ? "BINANCE" : "KRAKEN";
    TRACE("subscribing to order book for ", exchangeName);
    if (!api->subscribeOrderBook(pair)) {
      TRACE("failed to subscribe to order book for ", exchangeName);
      return false;
    }
  }

  return true;
}

bool ExchangeManager::getOrderBookSnapshots(TradingPair pair) {
  for (const auto &[exchangeId, api] : exchanges) {
    std::string exchangeName =
        (exchangeId == ExchangeId::BINANCE) ? "BINANCE" : "KRAKEN";
    TRACE("getting order book snapshot for ", exchangeName);
    if (!api->getOrderBookSnapshot(pair)) {
      TRACE("failed to get order book snapshot for ", exchangeName);
      return false;
    }
  }

  return true;
}

void ExchangeManager::processMessages() {
  for (const auto &exchangeId : exchangeIds) {
    if (auto *exchange = exchanges[exchangeId].get()) {
      exchange->processMessages();
    }
  }
}