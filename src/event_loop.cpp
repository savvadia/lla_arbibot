#include "event_loop.h"
#include <thread>
#include <string>
#include <unordered_map>
#include <iostream>

using namespace std;

#define TRACE(...) TRACE_INST(TRACE_EVENT_LOOP, __VA_ARGS__)

EventLoop::EventLoop() {
    TRACE("created");
}

EventLoop::~EventLoop() {
    stop();
    TRACE("destroyed");
}

void EventLoop::start() {
    if (running) return;
    
    running = true;
    startTime = std::chrono::system_clock::now();
    
    eventThread = std::thread(&EventLoop::eventLoop, this);
    TRACE("started");
}

void EventLoop::stop() {
    if (!running) return;
    
    running = false;
    queueCV.notify_all();
    
    if (eventThread.joinable()) {
        eventThread.join();
    }
    
    TRACE("stopped");
}

void EventLoop::postEvent(EventType type, std::function<void()> handler) {
    Event event{
        .type = type,
        .timestamp = std::chrono::system_clock::now(),
        .handler = std::move(handler)
    };
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        eventQueue.push(std::move(event));
    }
    queueCV.notify_one();
}

void EventLoop::eventLoop() {
    while (running) {
        Event event;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this] { 
                return !running || !eventQueue.empty(); 
            });
            
            if (!running) break;
            
            event = std::move(eventQueue.front());
            eventQueue.pop();
        }
        
        try {
            event.handler();
        } catch (const std::exception& e) {
            TRACE("error processing event: %s", e.what());
        }
    }
} 