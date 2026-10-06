#pragma once

#include "Command.hpp"
#include <map>
#include <vector>
#include <algorithm>

// InputBuffer holds input commands per tick and supports scheduling future inputs
class InputBuffer {
public:
    // Sets the delay applied to enqueued inputs.
    void set_delay(uint32_t d) {
        delay = d;
    }

    // Adds a command to the buffer. The command is scheduled for cmd.tick + delay.
    void enqueue(const Command& cmd) {
        Command delayed_cmd = cmd;
        delayed_cmd.tick += delay;
        buffer[delayed_cmd.tick].push_back(delayed_cmd);
    }

    // Schedules a command to be executed at a future tick relative to its current tick, ignoring the default delay.
    void schedule(const Command& cmd, uint32_t additional_delay) {
        Command delayed_cmd = cmd;
        delayed_cmd.tick += additional_delay;
        buffer[delayed_cmd.tick].push_back(delayed_cmd);
    }

    // Retrieves all commands for a specific tick, sorted deterministically
    std::vector<Command> get_commands_for_tick(uint32_t tick) {
        auto it = buffer.find(tick);
        if (it != buffer.end()) {
            std::vector<Command> cmds = it->second;
            // Sort to ensure deterministic interleaving for multiple players
            std::sort(cmds.begin(), cmds.end());
            // Optionally remove them to save memory since we only tick forward
            buffer.erase(it);
            return cmds;
        }
        return {};
    }

private:
    // We use std::map to ensure that ticks are stored sorted, although we typically query per-tick.
    // The key is the tick number, and the value is a list of commands for that tick.
    std::map<uint32_t, std::vector<Command>> buffer;

    uint32_t delay = 2; // Default delay parameter
};
