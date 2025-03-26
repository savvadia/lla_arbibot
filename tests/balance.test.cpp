#include "../src/balance.h"
#include <gtest/gtest.h>
#include <sstream>

// Test fixture for Balance
class BalanceTest : public ::testing::Test {
protected:
    Balance balance;

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

TEST(BalanceTest, BasicOperations) {
    Balance balance;
    
    // Test increasing balance
    balance.inc("kraken", "BTC", 0.005);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.005);
    
    // Test increasing balance again
    balance.inc("kraken", "BTC", 0.01);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.015);
    
    // Test decreasing balance
    balance.dec("binance", "USDT", 50.0);
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), -50.0);
    
    // Test decreasing balance again
    balance.dec("binance", "USDT", 100.0);
    EXPECT_DOUBLE_EQ(balance.get("binance", "USDT"), -150.0);
    
    // Test decreasing balance from positive
    balance.dec("kraken", "BTC", 0.005);
    EXPECT_DOUBLE_EQ(balance.get("kraken", "BTC"), 0.01);
}
