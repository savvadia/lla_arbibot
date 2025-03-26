#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class MockWebSocketServer {
public:
    explicit MockWebSocketServer(net::io_context& ioc);
    ~MockWebSocketServer();

    void start();
    void stop();
    void sendMessage(const std::string& message);
    void setOnConnect(std::function<void()> callback);
    void setOnMessage(std::function<void(const std::string&)> callback);
    uint16_t getPort() const { return m_port; }

private:
    void accept();
    void doRead();

    net::io_context& m_ioc;
    tcp::acceptor m_acceptor;
    std::unique_ptr<websocket::stream<tcp::socket>> m_ws;
    beast::flat_buffer m_buffer;
    bool m_running;
    uint16_t m_port;
    std::function<void()> m_onConnect;
    std::function<void(const std::string&)> m_onMessage;
}; 