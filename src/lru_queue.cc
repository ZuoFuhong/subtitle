#include "lru_queue.h"
#include <mutex>
#include <utility>
#include <spdlog/spdlog.h>

LRUQueue::LRUQueue(std::string name, int max_size) {
    m_name = std::move(name);
    m_max_size = max_size;
}

void LRUQueue::push(Packet* packet) {
    std::lock_guard<std::mutex> lock(mutex);
    if (m_queue.size() >= m_max_size) {
        auto value = m_queue.front();
        delete value;
        m_queue.pop();
        spdlog::warn("LRUQueue {} overflow drop packet", m_name);
    }
    m_queue.push(packet);
}

Packet* LRUQueue::pop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (m_queue.empty()) {
        return nullptr;
    }
    auto value = m_queue.front();
    m_queue.pop();
    return value;
}

bool LRUQueue::empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return m_queue.empty();
}

bool LRUQueue::full() {
    std::lock_guard<std::mutex> lock(mutex);
    return m_queue.size() == m_max_size;
}

uint32_t LRUQueue::size() {
    std::lock_guard<std::mutex> lock(mutex);
    return m_queue.size();
}

float LRUQueue::load_ratio() {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<float>(m_queue.size()) / static_cast<float>(m_max_size);
}

LRUQueue::~LRUQueue() {
    while(!m_queue.empty()) {
        auto value = m_queue.front();
        delete value;
        m_queue.pop();
    }
}