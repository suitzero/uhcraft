#include "NetworkSimulator.hpp"

NetworkSimulator::NetworkSimulator(Peer& delegate, const NetworkSimulatorConfig& config)
    : delegate(delegate), config(config), prng(config.seed), lossDist(0.0, 1.0) {
}

void NetworkSimulator::connect() {
    delegate.connect();
}

void NetworkSimulator::disconnect() {
    delegate.disconnect();
}

bool NetworkSimulator::shouldDrop() {
    if (config.lossProbability <= 0.0) return false;
    if (config.lossProbability >= 1.0) return true;
    return lossDist(prng) < config.lossProbability;
}

std::chrono::steady_clock::time_point NetworkSimulator::calculateDeliveryTime() {
    auto now = std::chrono::steady_clock::now();
    if (config.baseLatencyMs == 0 && config.jitterMs == 0) {
        return now;
    }
    
    uint32_t delayMs = config.baseLatencyMs;
    if (config.jitterMs > 0) {
        std::uniform_int_distribution<uint32_t> jitterDist(0, config.jitterMs);
        delayMs += jitterDist(prng);
    }
    
    return now + std::chrono::milliseconds(delayMs);
}

void NetworkSimulator::flush() {
    auto now = std::chrono::steady_clock::now();

    while (!delayedInputs.empty() && delayedInputs.top().deliverAt <= now) {
        auto item = delayedInputs.top();
        delayedInputs.pop();
        delegate.sendInput(item.peerId, item.tick, item.payload);
    }

    while (!delayedHashes.empty() && delayedHashes.top().deliverAt <= now) {
        auto item = delayedHashes.top();
        delayedHashes.pop();
        delegate.sendStateHash(item.peerId, item.tick, item.hash, item.delay_vote);
    }
}

void NetworkSimulator::sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) {
    flush();

    if (shouldDrop()) {
        return;
    }

    if (config.baseLatencyMs == 0 && config.jitterMs == 0) {
        delegate.sendInput(peerId, tick, serializedInput);
    } else {
        DelayedInput delayed;
        delayed.deliverAt = calculateDeliveryTime();
        delayed.peerId = peerId;
        delayed.tick = tick;
        delayed.payload = serializedInput;
        delayedInputs.push(delayed);
    }
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> NetworkSimulator::pollInputs(uint32_t tick) {
    flush();
    return delegate.pollInputs(tick);
}

void NetworkSimulator::sendStateHash(uint32_t peerId, uint32_t tick, uint32_t hash, uint32_t delay_vote) {
    flush();

    if (shouldDrop()) {
        return;
    }

    if (config.baseLatencyMs == 0 && config.jitterMs == 0) {
        delegate.sendStateHash(peerId, tick, hash, delay_vote);
    } else {
        DelayedHash delayed;
        delayed.deliverAt = calculateDeliveryTime();
        delayed.peerId = peerId;
        delayed.tick = tick;
        delayed.hash = hash;
        delayed.delay_vote = delay_vote;
        delayedHashes.push(delayed);
    }
}

std::vector<StateHashResult> NetworkSimulator::pollStateHashes(uint32_t tick) {
    flush();
    return delegate.pollStateHashes(tick);
}

uint32_t NetworkSimulator::getRTT(uint32_t peerId) const {
    // Add 2x the base latency to simulate round trip time over the artificial link.
    // In a real scenario, latency jitter causes this to fluctuate, but for simplistic tests,
    // we use baseLatencyMs * 2 as a solid baseline addition to whatever delegate returns.
    return delegate.getRTT(peerId) + (config.baseLatencyMs * 2);
}
