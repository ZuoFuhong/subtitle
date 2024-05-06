#pragma once

#include "packet.h"
#include <iostream>
#include <mutex>
#include <queue>

class LRUQueue {
public:
    explicit LRUQueue(std::string name = "", int max_size = 10);

    ~LRUQueue();

    void push(Packet* packet);

    Packet* pop();

    bool empty();

    bool full();

    uint32_t size();

    // 负载水平
    float load_ratio();
private:
    std::string m_name;

    std::queue<Packet*> m_queue;

    uint32_t m_max_size;

    std::mutex mutex;
};