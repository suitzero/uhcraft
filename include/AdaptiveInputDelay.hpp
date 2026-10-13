#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <map>

// AdaptiveInputDelay dynamically adjusts the input delay `d` based on RTT measurements.
// It uses a consensus-based approach to ensure all peers agree on the same delay
// at deterministic tick boundaries, maintaining the lockstep determinism.
class AdaptiveInputDelay {
public:
    struct Config {
        uint32_t minDelayTicks = 2;
        uint32_t maxDelayTicks = 15;
        uint32_t tickDurationMs = 16; // Assuming ~60Hz ticks
        uint32_t safetyMarginTicks = 1;
        uint32_t windowSize = 60; // Ticks before applying a change (hysteresis window)
    };

    AdaptiveInputDelay(const Config& config) 
        : config(config), currentDelay(config.minDelayTicks), agreedTargetDelay(config.minDelayTicks), windowStableCount(0) {}

    // Computes the local recommendation for the input delay based on max RTT among all peers
    uint32_t compute_local_vote(const std::vector<uint32_t>& peer_rtts) {
        uint32_t max_rtt = 0;
        for (uint32_t rtt : peer_rtts) {
            max_rtt = std::max(max_rtt, rtt);
        }

        // rtt is a full round trip, one way latency is roughly rtt / 2.
        // But for safety and network variance, taking ceil(rtt / tickDurationMs) is often used,
        // which gives us a full RTT worth of buffer.
        // Let's use standard formula: d = ceil(RTT / tick_duration) + margin
        uint32_t raw_delay = (max_rtt + config.tickDurationMs - 1) / config.tickDurationMs + config.safetyMarginTicks;
        
        return std::clamp(raw_delay, config.minDelayTicks, config.maxDelayTicks);
    }

    // Feeds the consensus votes from all peers for a given tick, evaluates hysteresis,
    // and returns the new actual delay to apply. 
    // CRITICAL: This method MUST be called on exactly the same tick with identical vote data 
    // across all peers to preserve determinism.
    uint32_t update_and_get_delay(uint32_t tick, const std::vector<uint32_t>& all_votes) {
        if (all_votes.empty()) return currentDelay;

        uint32_t max_vote = 0;
        for (uint32_t vote : all_votes) {
            max_vote = std::max(max_vote, vote);
        }

        // Ensure max_vote is within bounds just in case peers send bad data
        max_vote = std::clamp(max_vote, config.minDelayTicks, config.maxDelayTicks);

        if (max_vote != agreedTargetDelay) {
            agreedTargetDelay = max_vote;
            windowStableCount = 0;
        } else {
            windowStableCount++;
        }

        // Only change current delay at agreed tick boundaries and if stable
        if (tick % config.windowSize == 0 && windowStableCount >= config.windowSize) {
            currentDelay = agreedTargetDelay;
        }

        return currentDelay;
    }

    uint32_t get_current_delay() const {
        return currentDelay;
    }

private:
    Config config;
    uint32_t currentDelay;
    uint32_t agreedTargetDelay;
    uint32_t windowStableCount;
};
