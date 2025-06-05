#include "../src/balance.h"
#include <gtest/gtest.h>
#include <sstream>

// Test fixture for Balance
class BalanceTest : public ::testing::Test {
protected:
    BalanceManager balance;

    void SetUp() override {
        balance.retrieveBalances();
    }
};

// Test retrieving initial balances
TEST_F(BalanceTest, RetrieveBalances) {
    auto balances = balance.getBalances();
    
    EXPECT_DOUBLE_EQ(balances["kraken"]["BTC"], 0.01);
    EXPECT_DOUBLE_EQ(balances["kraken"]["USDT"], 100.0);
    EXPECT_DOUBLE_EQ(balances["binance"]["BTC"], 0.02);
    EXPECT_DOUBLE_EQ(balances["binance"]["USDT"], 200.0);
}

// Test increasing balances
TEST_F(BalanceTest, IncreaseBalance) {
    balance.inc("kraken", "BTC", 0.005);
    
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.015);
}

// Test decreasing balances
TEST_F(BalanceTest, DecreaseBalance) {
    balance.dec("binance", "USDT", 50.0);
    
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), 150.0);
}

// Test retrieving balance for a specific exchange/coin
TEST_F(BalanceTest, GetBalance) {
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.01);
}

// Test basic balance operations
TEST_F(BalanceTest, BasicOperations) {
    // Test initial state
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.01);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "USDT"), 100.0);
    EXPECT_DOUBLE_EQ(balance.get("binance", "BTC"), 0.02);
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), 200.0);
    
    // Test increasing balances
    balance.inc("kraken", "BTC", 0.005);
    balance.inc("kraken", "USDT", 50.0);
    balance.inc("binance", "BTC", 0.01);
    balance.inc("binance", "USDT", 100.0);
    
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.015);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "USDT"), 150.0);
    EXPECT_DOUBLE_EQ(balance.get("binance", "BTC"), 0.03);
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), 300.0);
    
    // Test decreasing balances
    balance.dec("kraken", "BTC", 0.005);
    balance.dec("kraken", "USDT", 25.0);
    balance.dec("binance", "BTC", 0.01);
    balance.dec("binance", "USDT", 50.0);
    
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.01);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "USDT"), 125.0);
    EXPECT_DOUBLE_EQ(balance.get("binance", "BTC"), 0.02);
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), 250.0);
    
    // Test non-existent currency
    EXPECT_DOUBLE_EQ(balance.get("kraken", "XRP"), 0.0);
    balance.dec("kraken", "XRP", 1.0);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "XRP"), 0.0);
}
