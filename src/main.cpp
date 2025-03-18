#include <iostream>
using namespace std;

#include <string>
#include <curl/curl.h>
#include <json/json.h>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void getBinanceOrderBook(const string& symbol) {
    CURL* curl;
    CURLcode res;
    string readBuffer;

    // initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(!curl) {
     curl_global_cleanup();
        cerr << "[getBinanceOrderBook] cannot initialize curl"<<endl;
    }

    string url = "https://api.binance.com/api/v3/depth?symbol=" + symbol;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    // run the request
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "[getBinanceOrderBook] curl_easy_perform() failed ("<< url<<"):" << curl_easy_strerror(res) << std::endl;
    } else {
        
        
        cout<< readBuffer <<endl<<endl;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

int main() {

    cout <<"hello, world" <<endl;
    getBinanceOrderBook("BTCUSDT");
    return 0;
}
