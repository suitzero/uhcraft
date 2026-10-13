#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include "Peer.hpp"
#include "Command.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"
#include "LockstepBarrier.hpp"
#include "DesyncMonitor.hpp"
#include "NetworkSimulator.hpp"
#include "AdaptiveInputDelay.hpp"

struct PeerNode {
    uint32_t id;
    LoopbackPeer* loopback_peer;
    NetworkSimulator* sim_peer;
    SimulationEngine* engine;
    InputBuffer* buffer;
    LockstepBarrier* barrier;
    DesyncMonitor* monitor;
    AdaptiveInputDelay* adaptive_delay;
};

int main() {
    std::cout << "Testing Adaptive Input Delay..." << std::endl;

    LoopbackHub hub;
    const uint32_t num_peers = 2;
    std::vector<uint32_t> expected_peers = {1, 2};
    std::vector<PeerNode> nodes(num_peers);

    uint64_t seed = 0x12345678;

    bool desync_occurred = false;

    // Simulation configuration
    AdaptiveInputDelay::Config delayConfig;
    delayConfig.minDelayTicks = 2;
    delayConfig.maxDelayTicks = 15;
    delayConfig.tickDurationMs = 16;
    delayConfig.safetyMarginTicks = 1;
    delayConfig.windowSize = 10; // short for testing

    NetworkSimulatorConfig netConfig1;
    netConfig1.baseLatencyMs = 0; // Starts at 0
    NetworkSimulatorConfig netConfig2;
    netConfig2.baseLatencyMs = 0; // Starts at 0

    // Initialize peers
    for (uint32_t i = 0; i < num_peers; ++i) {
        uint32_t id = i + 1;
        nodes[i].id = id;
        nodes[i].loopback_peer = new LoopbackPeer(hub, id);
        
        if (i == 0) {
            nodes[i].sim_peer = new NetworkSimulator(*(nodes[i].loopback_peer), netConfig1);
        } else {
            nodes[i].sim_peer = new NetworkSimulator(*(nodes[i].loopback_peer), netConfig2);
        }
        
        nodes[i].sim_peer->connect();
        
        nodes[i].engine = new SimulationEngine(seed);
        nodes[i].buffer = new InputBuffer();
        
        nodes[i].adaptive_delay = new AdaptiveInputDelay(delayConfig);
        nodes[i].buffer->set_delay(nodes[i].adaptive_delay->get_current_delay());
        
        nodes[i].barrier = new LockstepBarrier(nodes[i].sim_peer, expected_peers);
        
        nodes[i].monitor = new DesyncMonitor(nodes[i].sim_peer, expected_peers, 
            [&desync_occurred](uint32_t tick, uint32_t /*expected*/, uint32_t /*actual*/, uint32_t /*remote_id*/) {
                std::cerr << "Desync detected on tick " << tick << std::endl;
                desync_occurred = true;
            }
        );

        nodes[i].monitor->set_verified_callback(
            [&nodes, i](uint32_t tick, const std::vector<uint32_t>& votes) {
                uint32_t new_delay = nodes[i].adaptive_delay->update_and_get_delay(tick, votes);
                nodes[i].buffer->set_delay(new_delay);
            }
        );
    }

    const uint32_t NUM_TICKS = 100;
    std::vector<uint32_t> delay_history_1;
    std::vector<uint32_t> delay_history_2;

    for (uint32_t tick = 0; tick < NUM_TICKS; ++tick) {
        
        // Dynamically change latency
        if (tick == 30) {
            // Spike latency
            netConfig1.baseLatencyMs = 80;
            netConfig2.baseLatencyMs = 80;
            // Hack to update config internally since it's copied.
            // We just use a fresh simulator config or reconstruct. We'll simulate via getRTT override implicitly since we're using NetworkSimulator.
            // Wait, NetworkSimulator takes config by value. Let's just create a test specific change.
            // Actually, for simplicity we can just change how we feed the vote.
        }
        
        // Compute local RTT vote for this tick
        for (uint32_t i = 0; i < num_peers; ++i) {
            std::vector<uint32_t> rtts;
            for (size_t k = 0; k < expected_peers.size(); ++k) {
                // Simulate RTT change dynamically based on tick
                uint32_t simulated_rtt = 0;
                if (tick >= 20 && tick < 60) simulated_rtt = 100; // ~6 ticks needed
                
                rtts.push_back(simulated_rtt);
            }
            uint32_t local_vote = nodes[i].adaptive_delay->compute_local_vote(rtts);
            
            // Send empty commands
            std::vector<Command> cmds;
            auto payload = LockstepBarrier::bundle_commands(cmds);
            
            for (uint32_t j = 0; j < num_peers; ++j) {
                nodes[i].sim_peer->sendInput(nodes[j].id, tick, payload);
            }
            
            // Advance simulation
            std::vector<Command> out_cmds;
            nodes[i].barrier->poll_and_advance(tick, out_cmds);
            nodes[i].engine->tick(out_cmds); // simplified, buffer bypass for empty cmds test is fine
            
            uint32_t current_hash = nodes[i].engine->get_state().compute_hash();
            nodes[i].monitor->add_local_hash(tick, current_hash);
            
            for (uint32_t j = 0; j < num_peers; ++j) {
                nodes[i].sim_peer->sendStateHash(nodes[j].id, tick, current_hash, local_vote);
            }
        }
        
        for (uint32_t i = 0; i < num_peers; ++i) {
            nodes[i].monitor->poll_and_check();
            if (i == 0) delay_history_1.push_back(nodes[i].adaptive_delay->get_current_delay());
            if (i == 1) delay_history_2.push_back(nodes[i].adaptive_delay->get_current_delay());
        }
        
        assert(!desync_occurred);
    }
    
    // Assert determinism
    assert(delay_history_1 == delay_history_2);
    
    // Assert delay logic
    assert(delay_history_1[0] == 2); // Initial
    assert(delay_history_1[29] == 2); // Before spike applies fully
    assert(delay_history_1[35] > 2); // Hysteresis window applied new delay
    assert(delay_history_1[95] == 2); // Dropped back down

    std::cout << "Adaptive Input Delay passed." << std::endl;

    for (uint32_t i = 0; i < num_peers; ++i) {
        delete nodes[i].monitor;
        delete nodes[i].barrier;
        delete nodes[i].buffer;
        delete nodes[i].engine;
        delete nodes[i].sim_peer;
        delete nodes[i].loopback_peer;
        delete nodes[i].adaptive_delay;
    }

    return 0;
}
