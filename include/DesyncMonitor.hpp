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

    using VerifiedCallback = std::function<void(uint32_t tick, const std::vector<uint32_t>& votes)>;

    // Optionally set a callback to receive delay votes when a tick is fully verified
    void set_verified_callback(VerifiedCallback cb) {
        on_verified = cb;
    }

    // Polls the network for peer hashes and checks them against local hashes.
    // Call this repeatedly (e.g., each tick/frame).
    void poll_and_check() {
        auto it = local_hashes.begin();
        while (it != local_hashes.end()) {
            uint32_t tick = it->first;
            uint32_t expected_hash = it->second;
            auto received_hashes = peer->pollStateHashes(tick);
            
            for (const auto& res : received_hashes) {
                uint32_t peer_id = res.peerId;
                uint32_t actual_hash = res.hash;
                uint32_t delay_vote = res.delay_vote;

                if (actual_hash != expected_hash) {
                    on_desync(tick, expected_hash, actual_hash, peer_id);
                }
                
                // Store vote
                tick_votes[tick][peer_id] = delay_vote;
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

            // If fully verified, remove it to prevent memory leak and optionally trigger callback
            if (all_verified) {
                if (on_verified) {
                    std::vector<uint32_t> all_votes;
                    for (uint32_t ep : expected_peers) {
                        all_votes.push_back(tick_votes[tick][ep]);
                    }
                    on_verified(tick, all_votes);
                }

                verified_peers.erase(tick);
                tick_votes.erase(tick);
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
    VerifiedCallback on_verified;
    std::map<uint32_t, uint32_t> local_hashes; // tick -> local_hash
    std::map<uint32_t, std::set<uint32_t>> verified_peers; // tick -> set of verified peer_ids
    std::map<uint32_t, std::map<uint32_t, uint32_t>> tick_votes; // tick -> peer_id -> delay_vote
};
