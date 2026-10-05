#include <iostream>
#include <vector>
#include <cassert>
#include "Peer.hpp"
#include "Command.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"
#include "RNG.hpp"

void test_loopback_peer() {
    std::cout << "Testing LoopbackPeer..." << std::endl;

    LoopbackHub hub;
    LoopbackPeer peer1(hub, 1);
    LoopbackPeer peer2(hub, 2);

    peer1.connect();
    peer2.connect();

    uint64_t seed = 0x12345678;
    SimulationEngine engine1(seed);
    SimulationEngine engine2(seed);

    InputBuffer buffer1;
    InputBuffer buffer2;

    DeterministicRNG rng(seed);

    const int NUM_TICKS = 500;
    const int COMMANDS_PER_TICK = 5;

    // Generate random commands to be sent from peer1 to peer2, and peer2 to peer1
    for (int tick = 0; tick < NUM_TICKS; ++tick) {
        // Peer 1 sends to Peer 2
        for (int i = 0; i < COMMANDS_PER_TICK; ++i) {
            Command cmd;
            cmd.tick = tick;
            cmd.player_id = 1; // From peer1
            cmd.type = static_cast<Command::Type>(rng.next_range(3));
            cmd.payload_size = rng.next_range(16);
            for (uint32_t j = 0; j < cmd.payload_size; ++j) {
                cmd.payload[j] = static_cast<uint8_t>(rng.next_range(256));
            }
            
            // Send serialized command
            peer1.sendInput(2, tick, cmd.serialize());
            // Also peer 1 needs to apply its own command locally
            buffer1.enqueue(cmd);
        }

        // Peer 2 sends to Peer 1
        for (int i = 0; i < COMMANDS_PER_TICK; ++i) {
            Command cmd;
            cmd.tick = tick;
            cmd.player_id = 2; // From peer2
            cmd.type = static_cast<Command::Type>(rng.next_range(3));
            cmd.payload_size = rng.next_range(16);
            for (uint32_t j = 0; j < cmd.payload_size; ++j) {
                cmd.payload[j] = static_cast<uint8_t>(rng.next_range(256));
            }
            
            // Send serialized command
            peer2.sendInput(1, tick, cmd.serialize());
            // Also peer 2 needs to apply its own command locally
            buffer2.enqueue(cmd);
        }
    }

    // Now, run the simulation loop for both peers
    for (int tick = 0; tick < NUM_TICKS; ++tick) {
        // Peer 1 receives inputs for this tick
        auto inputs1 = peer1.pollInputs(tick);
        int received_from_2 = 0;
        for (const auto& pair : inputs1) {
            assert(pair.first == 2); // should come from peer 2
            Command cmd = Command::deserialize(pair.second);
            assert(cmd.tick == static_cast<uint32_t>(tick));
            assert(cmd.player_id == 2);
            buffer1.enqueue(cmd);
            received_from_2++;
        }
        assert(received_from_2 == COMMANDS_PER_TICK);

        // Peer 2 receives inputs for this tick
        auto inputs2 = peer2.pollInputs(tick);
        int received_from_1 = 0;
        for (const auto& pair : inputs2) {
            assert(pair.first == 1); // should come from peer 1
            Command cmd = Command::deserialize(pair.second);
            assert(cmd.tick == static_cast<uint32_t>(tick));
            assert(cmd.player_id == 1);
            buffer2.enqueue(cmd);
            received_from_1++;
        }
        assert(received_from_1 == COMMANDS_PER_TICK);

        // Advance both engines
        engine1.tick(buffer1.get_commands_for_tick(tick));
        engine2.tick(buffer2.get_commands_for_tick(tick));

        // Verify state hashes match every tick
        assert(engine1.get_state().compute_hash() == engine2.get_state().compute_hash());
    }

    std::cout << "LoopbackPeer test passed. Hashes matched for " << NUM_TICKS << " ticks." << std::endl;
}

int main() {
    test_loopback_peer();
    return 0;
}
