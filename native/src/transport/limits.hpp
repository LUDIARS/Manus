#pragma once

#include <chrono>
#include <cstddef>

namespace manus::transport {

// Every external I/O path has a size limit, a deadline and a concurrency cap.
// Node's http/ws had no connection caps and a 100 MiB WebSocket payload limit;
// the values below are the C++ service's explicit limits.
struct ServerLimits {
    std::size_t max_connections = 64;          // over the cap: accepted and closed at once (refuse)
    std::size_t max_websockets = 16;           // over the cap: 503 before the handshake (refuse)
    std::size_t header_limit = 16 * 1024;      // Node default max header size; 431 + close
    std::size_t body_limit = 4096;             // api.ts readBody limit; 400 (/api/pair) or 413 + close
    std::size_t ws_message_limit = 64 * 1024;  // close 1009 when exceeded
    std::chrono::seconds first_request_timeout{60};  // Node headersTimeout
    std::chrono::seconds keep_alive_timeout{5};      // Node keepAliveTimeout
    std::chrono::seconds write_timeout{30};
    std::chrono::seconds ws_handshake_timeout{30};
    std::chrono::seconds ws_idle_timeout{300};  // a ping is sent half-way; browsers answer with pong
};

}  // namespace manus::transport
