#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>

namespace manus::transport {

// Live connections, for the concurrency caps and for closing everything on
// shutdown. Used from the I/O thread only.
class ConnectionTracker {
public:
    class Ticket {
    public:
        Ticket() = default;
        Ticket(ConnectionTracker* tracker, std::uint64_t id) : tracker_(tracker), id_(id) {}
        Ticket(const Ticket&) = delete;
        Ticket& operator=(const Ticket&) = delete;
        Ticket(Ticket&& other) noexcept : tracker_(std::exchange(other.tracker_, nullptr)), id_(other.id_) {}
        Ticket& operator=(Ticket&& other) noexcept {
            if (this != &other) {
                release();
                tracker_ = std::exchange(other.tracker_, nullptr);
                id_ = other.id_;
            }
            return *this;
        }
        ~Ticket() { release(); }

    private:
        void release() {
            if (tracker_ != nullptr) tracker_->remove(id_);
            tracker_ = nullptr;
        }
        ConnectionTracker* tracker_ = nullptr;
        std::uint64_t id_ = 0;
    };

    Ticket add(std::function<void()> closer, bool websocket) {
        const std::uint64_t id = next_id_++;
        entries_.emplace(id, Entry{std::move(closer), websocket});
        if (websocket) ++websockets_;
        return Ticket(this, id);
    }

    std::size_t total() const { return entries_.size(); }
    std::size_t websockets() const { return websockets_; }

    // Closes every connection (Node: client.terminate() + closeAllConnections()).
    void close_all() {
        auto entries = entries_;  // closers may remove themselves
        for (auto& [id, entry] : entries) {
            if (entry.closer) entry.closer();
        }
    }

private:
    struct Entry {
        std::function<void()> closer;
        bool websocket = false;
    };

    void remove(std::uint64_t id) {
        const auto it = entries_.find(id);
        if (it == entries_.end()) return;
        if (it->second.websocket) --websockets_;
        entries_.erase(it);
    }

    std::map<std::uint64_t, Entry> entries_;
    std::size_t websockets_ = 0;
    std::uint64_t next_id_ = 1;
};

}  // namespace manus::transport
