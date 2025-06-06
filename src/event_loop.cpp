#include <iostream>
#include <thread>
#include "event_loop.h"
#include "tracer.h"
EventLoop::EventLoop() : m_running(false) {}

EventLoop::~EventLoop() {
    stop();
}

void EventLoop::start() {
    if (m_running) {
        return;
    }
    m_running = true;
    eventThread = std::thread(&EventLoop::processEvents, this);
}

void EventLoop::stop() {
    m_running = false;
    m_queueCondition.notify_one();
    if (eventThread.joinable()) {
        eventThread.join();
    }
}

void EventLoop::postEvent(EventType type, std::function<void()> callback) {
    {
        MUTEX_LOCK(m_queueMutex);
        m_eventQueue.emplace(type, callback, std::chrono::steady_clock::now());
    }
    m_queueCondition.notify_one();
}

void EventLoop::postExchangeUpdate(ExchangeId exchange) {
    postEvent(EventType::EXCHANGE_UPDATE, []() {
        // Handle exchange update
    });
}

void EventLoop::processEvents() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondition.wait(lock, [this] {
            return !m_running || !m_eventQueue.empty();
        });

        if (!m_running) {
            break;
        }

        Event event = std::move(m_eventQueue.front());
        m_eventQueue.pop();
        lock.unlock();

        try {
            event.callback();
        } catch (const std::exception& e) {
            std::cerr << "Error processing event: " << e.what() << std::endl;
        }
    }
} 