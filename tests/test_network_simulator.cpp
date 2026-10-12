#include "NetworkSimulator.hpp"
#include "Peer.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void test_zero_config() {
    std::cout << "Testing NetworkSimulator Zero Config (Passthrough)..." << std::endl;

    LoopbackHub hub;
    LoopbackPeer lp1(hub, 1);
    LoopbackPeer lp2(hub, 2);

    NetworkSimulatorConfig config;
    NetworkSimulator ns1(lp1, config);
    NetworkSimulator ns2(lp2, config);

    ns1.connect();
    ns2.connect();

    std::vector<uint8_t> payload = {1, 2, 3};
    ns1.sendInput(2, 100, payload);
    ns1.sendStateHash(2, 100, 42);

    auto inputs = ns2.pollInputs(100);
    assert(inputs.size() == 1);
    assert(inputs[0].first == 1);
    assert(inputs[0].second == payload);

    auto hashes = ns2.pollStateHashes(100);
    assert(hashes.size() == 1);
    assert(hashes[0].first == 1);
    assert(hashes[0].second == 42);

    ns1.disconnect();
    ns2.disconnect();

    std::cout << "Zero Config passed." << std::endl;
}

void test_100_percent_loss() {
    std::cout << "Testing NetworkSimulator 100% Loss..." << std::endl;

    LoopbackHub hub;
    LoopbackPeer lp1(hub, 1);
    LoopbackPeer lp2(hub, 2);

    NetworkSimulatorConfig config;
    config.lossProbability = 1.0;
    NetworkSimulator ns1(lp1, config);
    NetworkSimulator ns2(lp2, config);

    ns1.connect();
    ns2.connect();

    std::vector<uint8_t> payload = {1, 2, 3};
    ns1.sendInput(2, 100, payload);
    ns1.sendStateHash(2, 100, 42);

    auto inputs = ns2.pollInputs(100);
    assert(inputs.empty());

    auto hashes = ns2.pollStateHashes(100);
    assert(hashes.empty());

    ns1.disconnect();
    ns2.disconnect();

    std::cout << "100% Loss passed." << std::endl;
}

void test_seeded_loss_determinism() {
    std::cout << "Testing NetworkSimulator Seeded Loss Determinism..." << std::endl;

    auto runSimulation = [](uint64_t seed) {
        LoopbackHub hub;
        LoopbackPeer lp1(hub, 1);
        LoopbackPeer lp2(hub, 2);

        NetworkSimulatorConfig config;
        config.lossProbability = 0.5;
        config.seed = seed;
        NetworkSimulator ns1(lp1, config);
        NetworkSimulator ns2(lp2, config);

        ns1.connect();
        ns2.connect();

        std::vector<bool> received;
        for (uint32_t i = 0; i < 100; ++i) {
            std::vector<uint8_t> payload = {static_cast<uint8_t>(i)};
            ns1.sendInput(2, i, payload);
            auto inputs = ns2.pollInputs(i);
            received.push_back(!inputs.empty());
        }

        ns1.disconnect();
        ns2.disconnect();

        return received;
    };

    auto run1 = runSimulation(12345);
    auto run2 = runSimulation(12345);
    auto run3 = runSimulation(54321);

    assert(run1 == run2);
    assert(run1 != run3);

    std::cout << "Seeded Loss Determinism passed." << std::endl;
}

void test_latency() {
    std::cout << "Testing NetworkSimulator Latency..." << std::endl;

    LoopbackHub hub;
    LoopbackPeer lp1(hub, 1);
    LoopbackPeer lp2(hub, 2);

    NetworkSimulatorConfig config;
    config.baseLatencyMs = 50;
    NetworkSimulator ns1(lp1, config);
    NetworkSimulator ns2(lp2, config);

    ns1.connect();
    ns2.connect();

    std::vector<uint8_t> payload = {1, 2, 3};
    ns1.sendInput(2, 100, payload);

    // Should not be available immediately
    auto inputs_immediate = ns2.pollInputs(100);
    assert(inputs_immediate.empty());

    // Sleep past latency
    std::this_thread::sleep_for(100ms);
    
    // Flush the sender so it delivers the delayed packet to the underlying peer
    ns1.flush();

    // Should be available now
    auto inputs_delayed = ns2.pollInputs(100);
    assert(inputs_delayed.size() == 1);
    assert(inputs_delayed[0].first == 1);
    assert(inputs_delayed[0].second == payload);

    ns1.disconnect();
    ns2.disconnect();

    std::cout << "Latency passed." << std::endl;
}

int main() {
    test_zero_config();
    test_100_percent_loss();
    test_seeded_loss_determinism();
    test_latency();
    return 0;
}
