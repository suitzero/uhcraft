#include <iostream>
#include <vector>
#include <cassert>
#include "SimulationEngine.hpp"

// Generate some dummy inputs for our simulation
std::vector<std::vector<Command>> generate_inputs(int num_ticks) {
    std::vector<std::vector<Command>> inputs_per_tick(num_ticks);

    // Spawn a few units at the start
    for (int i = 0; i < 50; ++i) {
        Command spawn_cmd;
        spawn_cmd.tick = 0;
        spawn_cmd.player_id = 1;
        spawn_cmd.type = Command::Type::SPAWN_UNIT;
        spawn_cmd.payload.spawn.x = (i * 10) % 1000 - 500;
        spawn_cmd.payload.spawn.y = (i * 15) % 1000 - 500;
        inputs_per_tick[0].push_back(spawn_cmd);
    }

    // Add some random direction changes
    for (int tick = 100; tick < num_ticks; tick += 250) {
        Command dir_cmd;
        dir_cmd.tick = tick;
        dir_cmd.player_id = 1;
        dir_cmd.type = Command::Type::CHANGE_DIRECTION;
        dir_cmd.payload.dir.unit_id = 5; // Target unit 5
        dir_cmd.payload.dir.dx = 10;
        dir_cmd.payload.dir.dy = -10;
        inputs_per_tick[tick].push_back(dir_cmd);
    }

    return inputs_per_tick;
}

int main() {
    constexpr int NUM_TICKS = 10000;

    std::cout << "Generating inputs for " << NUM_TICKS << " ticks..." << std::endl;
    auto inputs = generate_inputs(NUM_TICKS);

    std::vector<uint32_t> hashes_run1(NUM_TICKS);
    std::vector<uint32_t> hashes_run2(NUM_TICKS);

    std::cout << "Running Simulation 1..." << std::endl;
    {
        SimulationEngine engine(9999);
        for (int i = 0; i < NUM_TICKS; ++i) {
            engine.tick(inputs[i]);
            hashes_run1[i] = engine.get_state().compute_hash();
        }
    }

    std::cout << "Running Simulation 2..." << std::endl;
    {
        SimulationEngine engine(9999);
        for (int i = 0; i < NUM_TICKS; ++i) {
            engine.tick(inputs[i]);
            hashes_run2[i] = engine.get_state().compute_hash();
        }
    }

    std::cout << "Verifying Determinism..." << std::endl;
    int mismatches = 0;
    for (int i = 0; i < NUM_TICKS; ++i) {
        if (hashes_run1[i] != hashes_run2[i]) {
            std::cerr << "Mismatch at tick " << i << ": "
                      << hashes_run1[i] << " != " << hashes_run2[i] << std::endl;
            mismatches++;
        }
    }

    if (mismatches == 0) {
        std::cout << "SUCCESS: 100% Deterministic match over " << NUM_TICKS << " ticks!" << std::endl;
        std::cout << "Final State Hash: " << hashes_run1.back() << std::endl;
        return 0;
    } else {
        std::cerr << "FAILED: " << mismatches << " mismatches found." << std::endl;
        return 1;
    }
}
