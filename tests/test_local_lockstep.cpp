#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <string>
#include <cstdlib>
#include <fstream>
#include "Peer.hpp"
#include "Command.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"
#include "RNG.hpp"
#include "LockstepBarrier.hpp"
#include "DesyncMonitor.hpp"
#include "DesyncDump.hpp"

struct PeerNode {
    uint32_t id;
    LoopbackPeer* peer;
    SimulationEngine* engine;
    InputBuffer* buffer;
    LockstepBarrier* barrier;
    DesyncMonitor* monitor;
};

int main(int argc, char** argv) {
    uint32_t num_ticks = 100000;
    if (argc > 1) {
        num_ticks = std::stoul(argv[1]);
    }
    std::cout << "Starting Local Lockstep Verification for " << num_ticks << " ticks...\n";

    LoopbackHub hub;
    const uint32_t num_peers = 3;
    std::vector<uint32_t> expected_peers = {1, 2, 3};
    std::vector<PeerNode> nodes(num_peers);

    uint64_t seed = 0x87654321;
    DeterministicRNG rng(seed);
    
    bool desync_occurred = false;

    // Initialize peers
    for (uint32_t i = 0; i < num_peers; ++i) {
        uint32_t id = i + 1;
        nodes[i].id = id;
        nodes[i].peer = new LoopbackPeer(hub, id);
        nodes[i].peer->connect();
        
        nodes[i].engine = new SimulationEngine(seed);
        nodes[i].buffer = new InputBuffer();
        nodes[i].buffer->set_delay(2); // delay model of 2 ticks
        
        nodes[i].barrier = new LockstepBarrier(nodes[i].peer, expected_peers);
        
        nodes[i].monitor = new DesyncMonitor(nodes[i].peer, expected_peers, 
            [&desync_occurred, &nodes, i](uint32_t tick, uint32_t expected, uint32_t actual, uint32_t remote_id) {
                std::cerr << "\n[ERROR] Desync detected on Peer " << nodes[i].id << " at tick " << tick << "!\n";
                std::cerr << "Expected Hash (Local): " << expected << "\n";
                std::cerr << "Actual Hash (from Peer " << remote_id << "): " << actual << "\n";
                
                desync_occurred = true;
                
                std::string dump_local = DesyncDump::dump_state(nodes[i].engine->get_state());
                
                // Fetch the remote engine to dump its state directly (since we're all in the same memory space)
                uint32_t remote_index = remote_id - 1;
                std::string dump_remote = DesyncDump::dump_state(nodes[remote_index].engine->get_state());
                
                std::cerr << "\nDesync Diff:\n" << DesyncDump::diff_dumps(dump_local, dump_remote) << "\n";
            }
        );
    }

    const int COMMANDS_PER_TICK_PROB = 10; // 10% chance to send command

    for (uint32_t tick = 0; tick < num_ticks; ++tick) {
        // Generate random commands for this tick
        for (uint32_t i = 0; i < num_peers; ++i) {
            std::vector<Command> cmds_to_send;
            
            if (rng.next_range(100) < COMMANDS_PER_TICK_PROB) {
                Command cmd;
                cmd.tick = tick; // Current tick, but buffer will schedule for tick+2
                cmd.player_id = nodes[i].id;
                
                if (rng.next_range(2) == 0) {
                    // Spawn
                    int32_t x = static_cast<int32_t>(rng.next_range(1000)) - 500;
                    int32_t y = static_cast<int32_t>(rng.next_range(1000)) - 500;
                    cmd.set_spawn_payload(x, y);
                } else {
                    // Change direction
                    // Assuming up to 50 active units, we just pick a random id between 1 and 50
                    uint32_t unit_id = rng.next_range(50) + 1;
                    int32_t dx = static_cast<int32_t>(rng.next_range(21)) - 10;
                    int32_t dy = static_cast<int32_t>(rng.next_range(21)) - 10;
                    cmd.set_dir_payload(unit_id, dx, dy);
                }
                cmds_to_send.push_back(cmd);
            }
            
            auto payload = LockstepBarrier::bundle_commands(cmds_to_send);
            
            // Broadcast commands to all peers including self
            for (uint32_t j = 0; j < num_peers; ++j) {
                nodes[i].peer->sendInput(nodes[j].id, tick, payload);
            }
            
            // Note: because we have an InputBuffer with delay=2, we don't apply immediately.
            // The commands received from network will be placed into the InputBuffer by the receiver.
        }

        // Process network inputs, barrier advance, engine tick, and desync monitor
        for (uint32_t i = 0; i < num_peers; ++i) {
            std::vector<Command> out_cmds;
            bool advanced = nodes[i].barrier->poll_and_advance(tick, out_cmds, 10, std::chrono::milliseconds(0));
            if (!advanced) {
                std::cerr << "Barrier stalled unexpectedly in loopback environment!\n";
                std::abort();
            }
            
            // Enqueue received commands into the input buffer (delay model applied here)
            for (const auto& cmd : out_cmds) {
                nodes[i].buffer->enqueue(cmd);
            }
            
            // Get commands scheduled for execution at THIS tick
            auto cmds_to_execute = nodes[i].buffer->get_commands_for_tick(tick);
            
            // Advance simulation
            nodes[i].engine->tick(cmds_to_execute);
            
            uint32_t current_hash = nodes[i].engine->get_state().compute_hash();
            
            // Desync check prep
            nodes[i].monitor->add_local_hash(tick, current_hash);
            
            // Broadcast state hash
            for (uint32_t j = 0; j < num_peers; ++j) {
                nodes[i].peer->sendStateHash(nodes[j].id, tick, current_hash);
            }
        }
        
        // Poll for desync checks (after hashes are broadcasted)
        for (uint32_t i = 0; i < num_peers; ++i) {
            nodes[i].monitor->poll_and_check();
        }
        
        if (desync_occurred) {
            std::cerr << "Verification FAILED at tick " << tick << std::endl;
            return 1;
        }
    }

    std::cout << "SUCCESS: 100% Deterministic match over " << num_ticks << " ticks with 3 peers!" << std::endl;

    for (uint32_t i = 0; i < num_peers; ++i) {
        delete nodes[i].monitor;
        delete nodes[i].barrier;
        delete nodes[i].buffer;
        delete nodes[i].engine;
        delete nodes[i].peer;
    }

    return 0;
}
