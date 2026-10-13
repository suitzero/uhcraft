#pragma once

#include "Peer.hpp"
#include <vector>
#include <cstdint>
#include <utility>
#include <random>
#include <chrono>
#include <queue>
#include <map>
#include <functional>

struct NetworkSimulatorConfig {
    double lossProbability = 0.0; // 0.0-1.0
    uint32_t baseLatencyMs = 0;
    uint32_t jitterMs = 0;
    uint64_t seed = 0;
};

// NetworkSimulator wraps an underlying Peer and injects artificial network impairment
// (loss, latency, jitter) for testing purposes.
class NetworkSimulator : public Peer {
public:
    NetworkSimulator(Peer& delegate, const NetworkSimulatorConfig& config);
    ~NetworkSimulator() override = default;

    void connect() override;
    void disconnect() override;
    void sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) override;
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> pollInputs(uint32_t tick) override;
    void sendStateHash(uint32_t peerId, uint32_t tick, uint32_t hash, uint32_t delay_vote = 0) override;
    std::vector<StateHashResult> pollStateHashes(uint32_t tick) override;
    uint32_t getRTT(uint32_t peerId) const override;

    // Test tool to force flush of delayed packets. Not strictly necessary if 
    // real time passes, but helpful for tests.
    void flush();

private:
    Peer& delegate;
    NetworkSimulatorConfig config;

    // PRNG for deterministic drop/jitter calculations
    std::mt19937_64 prng;
    std::uniform_real_distribution<double> lossDist;

    struct DelayedInput {
        std::chrono::steady_clock::time_point deliverAt;
        uint32_t peerId;
        uint32_t tick;
        std::vector<uint8_t> payload;

        bool operator>(const DelayedInput& other) const {
            return deliverAt > other.deliverAt;
        }
    };

    struct DelayedHash {
        std::chrono::steady_clock::time_point deliverAt;
        uint32_t peerId;
        uint32_t tick;
        uint32_t hash;
        uint32_t delay_vote;

        bool operator>(const DelayedHash& other) const {
            return deliverAt > other.deliverAt;
        }
    };

    std::priority_queue<DelayedInput, std::vector<DelayedInput>, std::greater<>> delayedInputs;
    std::priority_queue<DelayedHash, std::vector<DelayedHash>, std::greater<>> delayedHashes;

    bool shouldDrop();
    std::chrono::steady_clock::time_point calculateDeliveryTime();
};
