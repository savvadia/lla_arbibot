#include <iostream>
#include <iomanip>
using namespace std;

#include <balance.h>

#include <string>
#include <curl/curl.h>
#include <json/json.h>
#include "timers.h"
#include "s_poplavki.h"
#include "event_loop.h"

// #include <websocketpp/client.hpp>
// #include <nlohmann/json.hpp>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetchCurlData(const string& url) {
    CURL* curl = curl_easy_init();
    string readBuffer;
    CURLcode res;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "[fetchCurlData] curl_easy_perform() failed ("<< url<<"):" << curl_easy_strerror(res) << std::endl;
    } else {
        cout << readBuffer << endl << endl;
    }
    curl_easy_cleanup(curl);
    return readBuffer;
}

// Token price data
struct TokenPrice {
    string symbol;
    double bidPrice;
    double bidQuantity;
    double askPrice;
    double askQuantity;

    TokenPrice() : symbol(""), bidPrice(0.0), bidQuantity(0.0), askPrice(0.0), askQuantity(0.0) {}

    TokenPrice(const std::string& sym, const Json::Value& bidPriceVal, const Json::Value& bidQtyVal, 
               const Json::Value& askPriceVal, const Json::Value& askQtyVal)
        : symbol(sym),
          bidPrice(0.0), bidQuantity(0.0), askPrice(0.0), askQuantity(0.0) 
    {
        bool success = true;
        double bPrice = parseDouble(bidPriceVal, success);
        double bQty = parseDouble(bidQtyVal, success);
        double aPrice = parseDouble(askPriceVal, success);
        double aQty = parseDouble(askQtyVal, success);

        if (success) {
            bidPrice = bPrice;
            bidQuantity = bQty;
            askPrice = aPrice;
            askQuantity = aQty;
        }

        // cout << "TokenPrice constructor: " << *this << endl;
    }

    static double parseDouble(const Json::Value& value, bool& success) {
        if (!value.isString()) {
            success = false;
            return 0.0;
        }
        try {
            return std::stod(value.asString());
        } catch (const std::exception&) {
            success = false;
            return 0.0;
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const TokenPrice& tp) {
        os << fixed << setprecision(5);
        os << "TokenPrice { "
           << "symbol: " << tp.symbol
           << ", bidPrice: " << tp.bidPrice
           << ", bidQuantity: " << tp.bidQuantity
           << ", askPrice: " << tp.askPrice
           << ", askQuantity: " << tp.askQuantity
           << " }";
        return os;
    }
};

TokenPrice getKrakenPrice(const string& symbol) {
    string url = "https://api.kraken.com/0/public/Ticker?pair=" + symbol;
    string data = fetchCurlData(url);
    Json::Value jsonData;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    
    // Parse JSON string into the jsonData object
    bool parsingSuccessful = reader->parse(data.c_str(), data.c_str() + data.length(), &jsonData, &errs);
    if (!parsingSuccessful) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return {};
    }

    // cout << "[getKrakenPrice] jsonData: " << jsonData << endl;

    Json::Value result = jsonData["result"][symbol];
    return TokenPrice {symbol, result["b"][0], result["b"][2], result["a"][0], result["a"][2]};
}

TokenPrice getBinancePrice(const string& symbol) {
    string url = "https://api.binance.com/api/v3/ticker/bookTicker?symbol=" + symbol;
    string data = fetchCurlData(url);
    Json::Value jsonData;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    
    // Parse JSON string into the jsonData object
    bool parsingSuccessful = reader->parse(data.c_str(), data.c_str() + data.length(), &jsonData, &errs);
    if (!parsingSuccessful) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return TokenPrice();
    }

    // cout << "[getBinancePrice] jsonData: " << jsonData << endl;

    return TokenPrice {symbol, jsonData["bidPrice"], jsonData["bidQty"], jsonData["askPrice"], jsonData["askQty"]};
}

void checkArbitrage() {
    cout << "Checking for arbitrage opportunities" << endl;
    TokenPrice kp = getKrakenPrice("XBTUSDT");
    TokenPrice bp = getBinancePrice("BTCUSDT");

    cout << "Kraken price: " << kp << endl;
    cout << "Binance price: " << bp << endl;

    auto& kpa = kp.askPrice, &kpb = kp.bidPrice, &bpa = bp.askPrice, &bpb = bp.bidPrice;

    if (bpa < kpb) {
        cout << "Buy on Binance and sell on Kraken with spread " <<bpa <<" -> "<< kpb << " "<< (kpb-bpa)/bpa*100 << "% * "<< min(bp.bidQuantity,kp.askQuantity) << endl;
    } else if (kpa < kpb) {
        cout << "Buy on Kraken and sell on Binance with spread "<< kpa<< " -> "<<bpb<< " "<<(bpb-kpa)/kpa*100 << "% * "<< min(bp.bidQuantity, kp.askQuantity) << endl;
    } else {
        cout << "No arbitrage opportunity found\nBinance: "<< bp<< "\nKraken: "<< kp << endl;
    }
}

void getBinanceOrderBook(const string& symbol) {
    string url = "https://api.binance.com/api/v3/depth?symbol=" + symbol;

    string data = fetchCurlData(url);
    Json::Value jsonData;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    
    // Parse JSON string into the jsonData object
    bool parsingSuccessful = reader->parse(data.c_str(), data.c_str() + data.length(), &jsonData, &errs);
    if (!parsingSuccessful) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return;
    }
}

int main() {
    Balance b;
    TimersMgr tm;
    StrategyPoplavki s("BTC", "USDT", tm);
    EventLoop eventLoop;
    
    b.retrieveBalances();
    checkArbitrage();

    // Start the event loop
    eventLoop.start();

    // Post initial events
    eventLoop.postEvent(EventType::TIMER, [&tm]() {
        tm.checkTimers();
    });

    // Keep the main thread alive
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop the event loop
    eventLoop.stop();
    return 0;
}
