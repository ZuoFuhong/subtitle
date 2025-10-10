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

#include "convert_timer.h"
#include "lru_queue.h"
#include "packet.h"
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include "../third_party/json.hpp"


ConvertTimer::ConvertTimer(LRUQueue* audio_queue, LRUQueue* subtitle_queue) {
    m_audio_queue = audio_queue;
    m_subtitle_queue = subtitle_queue;
}

void ConvertTimer::set_target(std::string ip, unsigned short port) {
    m_ip = std::move(ip);
    m_port = port;
}

boost::asio::awaitable<void> handle_stream_reader(boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& ws_stream, LRUQueue* m_subtitle_queue) {
    boost::beast::flat_buffer read_buffer;
    boost::system::error_code ec;
    while (true) {
        std::size_t n = co_await ws_stream.async_read(read_buffer, boost::asio::redirect_error(
            boost::asio::use_awaitable, ec));
        if (ec) {
            if (ec == boost::beast::websocket::error::closed) {
                spdlog::info("WebSocket server closed the connection.");
                break;
            }
            spdlog::error("WebSocket read error: {}", ec.message());
            break;
        }

        std::string sentence = boost::beast::buffers_to_string(read_buffer.data());
        read_buffer.consume(n);
        spdlog::debug("Received subtitle data: {}", sentence);

        nlohmann::json sentence_obj = nlohmann::json::parse(sentence);
        std::string source_text = sentence_obj["text"];

        auto pkt = new Packet();
        pkt->type = SUBTITLE;
        pkt->body_size = source_text.size();
        pkt->body = new uint8_t[pkt->body_size];
        memcpy(pkt->body, source_text.data(), pkt->body_size);
        m_subtitle_queue->push(pkt);
    }
}

boost::asio::awaitable<void> ConvertTimer::run_websocket() {
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::ip::tcp::resolver resolver(executor);
    auto results = co_await resolver.async_resolve(m_ip, std::to_string(m_port), 
        boost::asio::use_awaitable);

    boost::asio::ip::tcp::socket socket(executor);
    boost::system::error_code ec;
    co_await boost::asio::async_connect(socket, results, boost::asio::redirect_error(
        boost::asio::use_awaitable, ec));
    if (ec) {
        spdlog::error("WebSocket connect failed: {}", ec.message());
        co_return;
    }

    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_stream(std::move(socket));
    co_await ws_stream.async_handshake(m_ip, "/ws", boost::asio::use_awaitable);

    // Start a separate coroutine to read messages from the WebSocket server
    boost::asio::co_spawn(executor, handle_stream_reader(ws_stream, m_subtitle_queue), boost::asio::detached);

    ws_stream.binary(true);
    while(true) {
        if (m_audio_queue->empty()) {
            co_await boost::asio::steady_timer(executor, std::chrono::milliseconds(20)).async_wait(
                boost::asio::use_awaitable);
            continue;
        }
        Packet *av_packet = m_audio_queue->pop();
        co_await ws_stream.async_write(boost::asio::buffer(av_packet->body, av_packet->body_size),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            spdlog::error("Send PCM data failed: {}", ec.message());
            std::exit(EXIT_FAILURE);
        }
        delete av_packet;
    }
}

void ConvertTimer::start() {
    boost::asio::co_spawn(m_io_context, run_websocket(), boost::asio::detached);
    m_io_context.run();
}