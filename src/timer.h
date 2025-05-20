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

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>

class Timer {
public:
    Timer(): m_done(false) {
    }

    template<class Fn, class ...Args>
    void start(std::chrono::milliseconds ms, Fn&& Fx, Args&&... Ax) {
        auto lambd_expr = [=, this](Args ...ax) -> void {
            while (true) {
                std::unique_lock<std::mutex> lock(m_mutex);
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                if (m_cond.wait_until(lock, now + ms, [this]() {return this->m_done.load();})) {
                    break;
                }
                std::invoke(Fx, ax...);
            }
        };
        m_thread = std::thread(lambd_expr, Ax...);
    }

    void stop() {
        m_done = true;
        m_cond.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    std::atomic_bool m_done;
    std::condition_variable m_cond;
    std::mutex m_mutex;
    std::thread m_thread;
};
