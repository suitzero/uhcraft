#pragma once

#include "SimulationState.hpp"
#include "RNG.hpp"
#include <vector>

// Commands represent deterministic inputs applied during a tick
struct Command {
    uint32_t tick;
    uint32_t player_id;
    // Payload (e.g., spawn unit, move to, etc.)
    enum class Type { NONE, SPAWN_UNIT, CHANGE_DIRECTION };
    Type type;

    // Simplistic payload for the demo
    union {
        struct {
            int32_t x;
            int32_t y;
        } spawn;
        struct {
            uint32_t unit_id;
            int32_t dx;
            int32_t dy;
        } dir;
    } payload;
};

class SimulationEngine {
public:
    SimulationEngine(uint64_t seed) : rng(seed) {
        // Zero initialize the state to prevent uninitialized memory affecting the hash
        state = {};
        state.tick = 0;
        state.active_unit_count = 0;
    }

    void apply_commands(const std::vector<Command>& commands) {
        for (const auto& cmd : commands) {
            if (cmd.tick != state.tick) continue;

            if (cmd.type == Command::Type::SPAWN_UNIT) {
                Vector2 pos = {FixedPoint::from_int(cmd.payload.spawn.x), FixedPoint::from_int(cmd.payload.spawn.y)};

                // Random velocity for the demo using our deterministic RNG
                int32_t vx = static_cast<int32_t>(rng.next_range(11)) - 5; // -5 to 5
                int32_t vy = static_cast<int32_t>(rng.next_range(11)) - 5;

                Vector2 vel = {FixedPoint::from_int(vx), FixedPoint::from_int(vy)};
                state.add_unit(next_unit_id++, pos, vel);
            }
            else if (cmd.type == Command::Type::CHANGE_DIRECTION) {
                for (size_t i = 0; i < state.active_unit_count; ++i) {
                    if (state.units[i].id == cmd.payload.dir.unit_id) {
                        state.units[i].velocity.x = FixedPoint::from_int(cmd.payload.dir.dx);
                        state.units[i].velocity.y = FixedPoint::from_int(cmd.payload.dir.dy);
                        break;
                    }
                }
            }
        }
    }

    void tick(const std::vector<Command>& commands_for_this_tick) {
        // 1. Apply inputs
        apply_commands(commands_for_this_tick);

        // 2. Step physics/logic
        for (size_t i = 0; i < state.active_unit_count; ++i) {
            Unit& u = state.units[i];
            if (!u.active) continue;

            // Simple integration: pos += vel * dt (assuming dt is 1 logic tick)
            u.position.x += u.velocity.x;
            u.position.y += u.velocity.y;

            // Simple bounds bouncing (-1000 to 1000)
            FixedPoint limit = FixedPoint::from_int(1000);
            FixedPoint neg_limit = FixedPoint::from_int(-1000);

            if (u.position.x > limit) {
                u.position.x = limit;
                u.velocity.x = FixedPoint::from_int(0) - u.velocity.x;
            } else if (u.position.x < neg_limit) {
                u.position.x = neg_limit;
                u.velocity.x = FixedPoint::from_int(0) - u.velocity.x;
            }

            if (u.position.y > limit) {
                u.position.y = limit;
                u.velocity.y = FixedPoint::from_int(0) - u.velocity.y;
            } else if (u.position.y < neg_limit) {
                u.position.y = neg_limit;
                u.velocity.y = FixedPoint::from_int(0) - u.velocity.y;
            }
        }

        // 3. Advance tick
        state.tick++;
    }

    const SimulationState& get_state() const {
        return state;
    }

private:
    SimulationState state;
    DeterministicRNG rng;
    uint32_t next_unit_id = 1;
};
