#include "mock_websocket_server.h"
#include <iostream>

MockWebSocketServer::MockWebSocketServer(net::io_context& ioc)
    : m_ioc(ioc)
    , m_acceptor(ioc)
    , m_running(false)
    , m_port(0) {
    
    // Create an acceptor with a dynamic port
    tcp::endpoint endpoint(net::ip::make_address("127.0.0.1"), 0);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(net::socket_base::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();

    // Get the actual port
    m_port = m_acceptor.local_endpoint().port();
}

MockWebSocketServer::~MockWebSocketServer() {
    stop();
}

void MockWebSocketServer::start() {
    if (m_running) return;
    m_running = true;
    accept();
}

void MockWebSocketServer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_ws) {
        m_ws->async_close(websocket::close_code::normal,
            [](beast::error_code ec) {
                if (ec) {
                    std::cerr << "Error closing WebSocket: " << ec.message() << std::endl;
                }
            });
    }
    m_acceptor.close();
}

void MockWebSocketServer::sendMessage(const std::string& message) {
    if (!m_ws || !m_running) return;

    m_ws->async_write(net::buffer(message),
        [](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Error sending message: " << ec.message() << std::endl;
            }
        });
}

void MockWebSocketServer::setOnConnect(std::function<void()> callback) {
    m_onConnect = std::move(callback);
}

void MockWebSocketServer::setOnMessage(std::function<void(const std::string&)> callback) {
    m_onMessage = std::move(callback);
}

void MockWebSocketServer::accept() {
    m_acceptor.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (ec) {
                std::cerr << "Error accepting connection: " << ec.message() << std::endl;
                if (m_running) {
                    accept();
                }
                return;
            }

            m_ws = std::make_unique<websocket::stream<tcp::socket>>(std::move(socket));

            m_ws->async_accept(
                [this](beast::error_code ec) {
                    if (ec) {
                        std::cerr << "Error in WebSocket handshake: " << ec.message() << std::endl;
                        m_ws.reset();
                        if (m_running) {
                            accept();
                        }
                        return;
                    }

                    if (m_onConnect) {
                        m_onConnect();
                    }

                    doRead();
                });
        });
}

void MockWebSocketServer::doRead() {
    m_buffer.clear();
    m_ws->async_read(m_buffer,
        [this](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Error reading message: " << ec.message() << std::endl;
                m_ws.reset();
                if (m_running) {
                    accept();
                }
                return;
            }

            std::string message{static_cast<char*>(m_buffer.data().data()), m_buffer.data().size()};
            m_buffer.consume(m_buffer.size());

            if (m_onMessage) {
                m_onMessage(message);
            }

            doRead();
        });
} 