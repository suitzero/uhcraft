#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
#include "Command.hpp"
#include "Replay.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"

void test_header_validation() {
    ReplayHeader h1;
    h1.seed = 123456789012345ULL;
    h1.start_state_hash = 0xABCDEF01;
    h1.tick_count = 1000;

    std::vector<uint8_t> buf = h1.serialize();
    ReplayHeader h2 = ReplayHeader::deserialize(buf);

    assert(h1 == h2);
    assert(h2.magic == ReplayHeader::MAGIC);
    assert(h2.version == ReplayHeader::VERSION);
    assert(h2.seed == 123456789012345ULL);
    assert(h2.start_state_hash == 0xABCDEF01);
    assert(h2.tick_count == 1000);
}

// Helper to generate some inputs
std::vector<Command> generate_test_inputs(int num_ticks) {
    std::vector<Command> inputs;

    for (int i = 0; i < 50; ++i) {
        Command spawn_cmd;
        spawn_cmd.tick = 0;
        spawn_cmd.player_id = 1;
        spawn_cmd.set_spawn_payload((i * 10) % 1000 - 500, (i * 15) % 1000 - 500);
        inputs.push_back(spawn_cmd);
    }

    for (int tick = 100; tick < num_ticks; tick += 250) {
        Command dir_cmd;
        dir_cmd.tick = tick;
        dir_cmd.player_id = 1;
        dir_cmd.set_dir_payload(5, 10, -10);
        inputs.push_back(dir_cmd);
    }

    return inputs;
}

void test_record_and_playback() {
    const std::string replay_file = "test_run.replay";
    const uint64_t SEED = 42;
    const int NUM_TICKS = 1000;

    auto commands = generate_test_inputs(NUM_TICKS);
    std::vector<uint32_t> recorded_hashes(NUM_TICKS);

    // 1. Record Simulation
    {
        SimulationEngine engine(SEED);
        uint32_t start_hash = engine.get_state().compute_hash();
        
        ReplayWriter writer(replay_file, SEED, start_hash);
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
        assert(header.seed == SEED);
        assert(header.tick_count > 0);

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
                std::cerr << "Mismatch at tick " << i << ": Recorded " << recorded_hashes[i] << ", Playback " << playback_hash << std::endl;
                mismatches++;
            }
        }
        
        assert(mismatches == 0 && "Playback hashes did not match recorded hashes.");
    }

    // Cleanup
    std::filesystem::remove(replay_file);
}

int main() {
    std::cout << "Running Replay System Tests..." << std::endl;
    
    test_header_validation();
    std::cout << " - Header Validation OK" << std::endl;
    
    test_record_and_playback();
    std::cout << " - Record and Playback (Hash Verification) OK" << std::endl;
    
    std::cout << "All Replay System Tests Passed!" << std::endl;
    return 0;
}
