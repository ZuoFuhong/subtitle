// Copyright (c) 2025 Mars Zuo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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