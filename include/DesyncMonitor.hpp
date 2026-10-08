#pragma once

#include "Peer.hpp"
#include <map>
#include <vector>
#include <functional>
#include <algorithm>
#include <set>

class DesyncMonitor {
public:
    using DesyncCallback = std::function<void(uint32_t tick, uint32_t expected_hash, uint32_t actual_hash, uint32_t peer_id)>;

    DesyncMonitor(Peer* peer, const std::vector<uint32_t>& expected_peers, DesyncCallback callback)
        : peer(peer), expected_peers(expected_peers.begin(), expected_peers.end()), on_desync(callback) {}

    // Store the locally computed state hash for a tick
    void add_local_hash(uint32_t tick, uint32_t hash) {
        local_hashes[tick] = hash;
    }

    // Polls the network for peer hashes and checks them against local hashes.
    // Call this repeatedly (e.g., each tick/frame).
    void poll_and_check() {
        auto it = local_hashes.begin();
        while (it != local_hashes.end()) {
            uint32_t tick = it->first;
            uint32_t expected_hash = it->second;
            auto received_hashes = peer->pollStateHashes(tick);
            
            for (const auto& pair : received_hashes) {
                uint32_t peer_id = pair.first;
                uint32_t actual_hash = pair.second;

                if (actual_hash != expected_hash) {
                    on_desync(tick, expected_hash, actual_hash, peer_id);
                }
                verified_peers[tick].insert(peer_id);
            }

            // Check if we verified all expected peers for this tick
            bool all_verified = true;
            for (uint32_t ep : expected_peers) {
                if (verified_peers[tick].find(ep) == verified_peers[tick].end()) {
                    all_verified = false;
                    break;
                }
            }

            // If fully verified, remove it to prevent memory leak
            if (all_verified) {
                verified_peers.erase(tick);
                it = local_hashes.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    Peer* peer;
    std::vector<uint32_t> expected_peers;
    DesyncCallback on_desync;
    std::map<uint32_t, uint32_t> local_hashes; // tick -> local_hash
    std::map<uint32_t, std::set<uint32_t>> verified_peers; // tick -> set of verified peer_ids
};
