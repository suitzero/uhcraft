#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "FixedPoint.hpp"

// Simple FNV-1a Hash
class HashFNV1a {
public:
    static constexpr uint32_t FNV_PRIME = 16777619u;
    static constexpr uint32_t OFFSET_BASIS = 2166136261u;

    static uint32_t hash_bytes(const void* data, size_t length) {
        uint32_t hash = OFFSET_BASIS;
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < length; ++i) {
            hash ^= ptr[i];
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

struct Vector2 {
    FixedPoint x;
    FixedPoint y;
};

// Simplified simulation state
// Avoid pointers, unordered collections, or dynamic allocations inside the state that might vary across runs
struct Unit {
    Vector2 position;
    Vector2 velocity;
    uint32_t id;
    bool active;
};

static constexpr size_t MAX_UNITS = 1024;

struct SimulationState {
    uint32_t tick;
    std::array<Unit, MAX_UNITS> units;
    uint32_t active_unit_count;

    // Helper to add a unit
    void add_unit(uint32_t id, Vector2 pos, Vector2 vel) {
        if (active_unit_count < MAX_UNITS) {
            units[active_unit_count] = {pos, vel, id, true};
            active_unit_count++;
        }
    }

    // Hashes the entire state
    uint32_t compute_hash() const {
        return HashFNV1a::hash_bytes(this, sizeof(SimulationState));
    }
};
