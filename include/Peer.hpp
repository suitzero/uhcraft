#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>

// Abstract peer transport interface
class Peer {
public:
    virtual ~Peer() = default;

    virtual void connect() = 0;
    virtual void disconnect() = 0;
    
    // sendInput sends a serialized input to the specified peerId for the given tick
    virtual void sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) = 0;
    
    // pollInputs returns all received inputs for the given tick, sorted by sender's peerId
    virtual std::vector<std::pair<uint32_t, std::vector<uint8_t>>> pollInputs(uint32_t tick) = 0;
};

// LoopbackHub serves as the shared message broker for in-memory loopback peers
class LoopbackHub {
public:
    struct Message {
        uint32_t fromPeerId;
        uint32_t toPeerId;
        uint32_t tick;
        std::vector<uint8_t> payload;

        bool operator<(const Message& other) const {
            if (tick != other.tick) return tick < other.tick;
            return fromPeerId < other.fromPeerId;
        }
    };

    void enqueue(uint32_t fromPeerId, uint32_t toPeerId, uint32_t tick, const std::vector<uint8_t>& payload) {
        messages.push_back({fromPeerId, toPeerId, tick, payload});
    }

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> dequeue(uint32_t toPeerId, uint32_t tick) {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> results;
        for (auto it = messages.begin(); it != messages.end(); ) {
            if (it->toPeerId == toPeerId && it->tick == tick) {
                results.push_back({it->fromPeerId, std::move(it->payload)});
                it = messages.erase(it);
            } else {
                ++it;
            }
        }
        // Ensure deterministic ordering based on fromPeerId
        std::stable_sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        return results;
    }

private:
    std::vector<Message> messages;
};

// LoopbackPeer represents an endpoint in the loopback network
class LoopbackPeer : public Peer {
public:
    LoopbackPeer(LoopbackHub& hub, uint32_t myPeerId);
    ~LoopbackPeer() override = default;

    void connect() override;
    void disconnect() override;
    void sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) override;
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> pollInputs(uint32_t tick) override;

private:
    LoopbackHub& hub;
    uint32_t myPeerId;
    bool connected;
};
