#pragma once

#include "Peer.hpp"
#include "Command.hpp"
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <thread>
#include <chrono>

class LockstepBarrier {
public:
    LockstepBarrier(Peer* peer, const std::vector<uint32_t>& expected_peers)
        : peer(peer), expected_peers(expected_peers.begin(), expected_peers.end()) {}

    // Utility to bundle multiple commands into a single payload
    static std::vector<uint8_t> bundle_commands(const std::vector<Command>& cmds) {
        std::vector<uint8_t> payload;
        for (const auto& cmd : cmds) {
            auto serialized = cmd.serialize();
            payload.insert(payload.end(), serialized.begin(), serialized.end());
        }
        return payload;
    }

    // Attempts to gather all inputs for the given tick.
    // Returns true and populates out_commands if all inputs have arrived.
    // Returns false if we are stalling (waiting for a peer).
    bool poll_and_advance(uint32_t tick, std::vector<Command>& out_commands, uint32_t max_retries = 10, std::chrono::milliseconds retry_delay = std::chrono::milliseconds(5)) {
        out_commands.clear();
        
        uint32_t retries = 0;
        
        while (retries < max_retries) {
            auto new_inputs = peer->pollInputs(tick);
            for (const auto& pair : new_inputs) {
                uint32_t sender_id = pair.first;
                
                size_t offset = 0;
                while (offset < pair.second.size()) {
                    Command cmd = Command::deserialize(pair.second, offset);
                    received_inputs_per_tick[tick][sender_id].push_back(cmd);
                    // Standard header is 16 bytes
                    offset += 16 + cmd.payload_size;
                }
                
                // Mark peer as having sent data for this tick (even if empty payload)
                received_peers_per_tick[tick].insert(sender_id);
            }

            if (all_peers_arrived(tick)) {
                // Collect all commands
                for (const auto& peer_id : expected_peers) {
                    if (received_inputs_per_tick[tick].count(peer_id)) {
                        for(const auto& cmd : received_inputs_per_tick[tick][peer_id]) {
                            out_commands.push_back(cmd);
                        }
                    }
                }

                // Deterministically sort the aggregated commands
                std::sort(out_commands.begin(), out_commands.end());

                // We can cleanup tracking for this tick
                received_peers_per_tick.erase(tick);
                received_inputs_per_tick.erase(tick);
                return true;
            }
            
            // Wait with backoff before next poll attempt
            if (retries < max_retries - 1) {
                std::this_thread::sleep_for(retry_delay);
            }
            retries++;
        }

        // Hit max retries without receiving all inputs - stall
        return false;
    }

private:
    bool all_peers_arrived(uint32_t tick) {
        for (uint32_t expected_id : expected_peers) {
            if (received_peers_per_tick[tick].find(expected_id) == received_peers_per_tick[tick].end()) {
                return false;
            }
        }
        return true;
    }

    Peer* peer;
    std::vector<uint32_t> expected_peers;
    std::map<uint32_t, std::set<uint32_t>> received_peers_per_tick; // tick -> set of peers we got data from
    std::map<uint32_t, std::map<uint32_t, std::vector<Command>>> received_inputs_per_tick; // tick -> peer_id -> commands
};
