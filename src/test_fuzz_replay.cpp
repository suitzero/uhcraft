#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
#include <string>
#include "Command.hpp"
#include "Replay.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"
#include "RNG.hpp"

void fuzz_replay(uint64_t seed) {
    const std::string replay_file = "fuzz_run_" + std::to_string(seed) + ".replay";
    const int NUM_TICKS = 500;
    const int NUM_COMMANDS = 2000;

    DeterministicRNG rng(seed);
    std::vector<Command> commands;

    for (int i = 0; i < NUM_COMMANDS; ++i) {
        Command cmd;
        cmd.tick = rng.next_range(NUM_TICKS); // schedule at random tick within the run
        cmd.player_id = rng.next_range(10); // random player 0-9
        
        // Random command type (0 to 2 are valid, let's include some random ones up to 5)
        cmd.type = static_cast<Command::Type>(rng.next_range(6));
        
        // Random payload
        cmd.payload_size = rng.next_range(MAX_PAYLOAD_SIZE + 1);
        for (uint32_t j = 0; j < cmd.payload_size; ++j) {
            cmd.payload[j] = static_cast<uint8_t>(rng.next_range(256));
        }

        // Specifically try to exercise valid payloads to cause physics events
        uint64_t rnd = rng.next_range(10);
        if (rnd < 3) {
            cmd.set_spawn_payload(
                static_cast<int32_t>(rng.next_range(2000)) - 1000, 
                static_cast<int32_t>(rng.next_range(2000)) - 1000
            );
        } else if (rnd < 6) {
            cmd.set_dir_payload(
                static_cast<uint32_t>(rng.next_range(100)), // random unit id
                static_cast<int32_t>(rng.next_range(200)) - 100,
                static_cast<int32_t>(rng.next_range(200)) - 100
            );
        }
        
        commands.push_back(cmd);
    }

    std::vector<uint32_t> recorded_hashes(NUM_TICKS);

    // 1. Record Simulation
    {
        SimulationEngine engine(seed);
        uint32_t start_hash = engine.get_state().compute_hash();
        
        ReplayWriter writer(replay_file, seed, start_hash);
        for (const auto& cmd : commands) {
            writer.write_command(cmd);
        }

        InputBuffer buffer;
        for (const auto& cmd : commands) {
            buffer.enqueue(cmd);
        }

        for (int i = 0; i < NUM_TICKS; ++i) {
            engine.tick(buffer.get_commands_for_tick(i));
            recorded_hashes[i] = engine.get_state().compute_hash();
        }
    } // writer closed

    // 2. Playback Simulation
    {
        ReplayReader reader(replay_file);
        assert(reader.is_valid());

        const ReplayHeader& header = reader.get_header();
        assert(header.seed == seed);

        SimulationEngine engine(header.seed);
        assert(engine.get_state().compute_hash() == header.start_state_hash);

        InputBuffer buffer;
        Command cmd;
        while (reader.read_command(cmd)) {
            buffer.enqueue(cmd);
        }

        int mismatches = 0;
        for (int i = 0; i < NUM_TICKS; ++i) {
            engine.tick(buffer.get_commands_for_tick(i));
            uint32_t playback_hash = engine.get_state().compute_hash();
            if (playback_hash != recorded_hashes[i]) {
                std::cerr << "Mismatch at tick " << i << " for seed " << seed 
                          << ": Recorded " << recorded_hashes[i] 
                          << ", Playback " << playback_hash << std::endl;
                mismatches++;
            }
        }
        
        assert(mismatches == 0 && "Playback hashes did not match recorded hashes in fuzz test.");
    }

    // Cleanup
    std::filesystem::remove(replay_file);
}

int main() {
    std::cout << "Running Replay Fuzz Tests..." << std::endl;
    
    uint64_t seeds[] = {
        0x1234567890ABCDEFULL,
        0xDEADBEEFCAFEBABEULL,
        42,
        1337,
        0x01234567,
        999999999,
        0xFFFFFFFFFFFFFFFFULL,
        0
    };

    for (uint64_t seed : seeds) {
        std::cout << " - Fuzzing with seed: " << seed << std::endl;
        fuzz_replay(seed);
    }
    
    std::cout << "All Replay Fuzz Tests Passed!" << std::endl;
    return 0;
}
