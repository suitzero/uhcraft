#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include "Peer.hpp"
#include "Command.hpp"
#include "SimulationEngine.hpp"
#include "InputBuffer.hpp"
#include "RNG.hpp"
#include "LockstepBarrier.hpp"
#include "DesyncMonitor.hpp"
#include "DesyncDump.hpp"
#include <fstream>

void run_test(bool simulate_desync) {
    std::cout << "Testing DesyncMonitor (" << (simulate_desync ? "Desync Path" : "Happy Path") << ")..." << std::endl;

    LoopbackHub hub;
    
    LoopbackPeer p1(hub, 1);
    LoopbackPeer p2(hub, 2);
    
    p1.connect();
    p2.connect();

    uint64_t seed = 12345;
    SimulationEngine e1(seed);
    SimulationEngine e2(seed);

    InputBuffer b1;
    InputBuffer b2;

    std::vector<uint32_t> expected_peers = {1, 2};
    LockstepBarrier bar1(&p1, expected_peers);
    LockstepBarrier bar2(&p2, expected_peers);

    bool d1_flag = false, d2_flag = false;
    uint32_t d1_tick = 0, d2_tick = 0;

    auto cb1 = [&](uint32_t tick, uint32_t expected, uint32_t actual, uint32_t peer_id) {
        (void)expected;
        (void)actual;
        (void)peer_id;
        d1_flag = true;
        d1_tick = tick;
        
        if (simulate_desync) {
            std::string dump1 = DesyncDump::dump_state(e1.get_state());
            std::string dump2 = DesyncDump::dump_state(e2.get_state());
            
            std::ofstream out1("dump_local_p1.txt");
            out1 << dump1;
            
            std::ofstream out2("dump_remote_p2.txt");
            out2 << dump2;
            
            std::cout << "\n[DesyncMonitor Hook] Desync Dump Generated at tick " << tick << "\n";
            std::cout << DesyncDump::diff_dumps(dump1, dump2) << "\n";
        }
    };
    auto cb2 = [&](uint32_t tick, uint32_t expected, uint32_t actual, uint32_t peer_id) {
        (void)expected;
        (void)actual;
        (void)peer_id;
        d2_flag = true;
        d2_tick = tick;
    };

    DesyncMonitor m1(&p1, expected_peers, cb1);
    DesyncMonitor m2(&p2, expected_peers, cb2);

    const uint32_t NUM_TICKS = 10000;
    const uint32_t DESYNC_TICK = 50;

    for (uint32_t tick = 0; tick < NUM_TICKS; ++tick) {
        // Enqueue commands
        Command cmd;
        cmd.tick = tick;
        cmd.player_id = 1;
        cmd.set_spawn_payload(10, 10);
        
        // If testing desync, p2 issues a different command at tick 50 to p1
        // (Simulating a bug where peer2 applied one state locally but sent another to peer1, 
        // OR simply simulate their SimulationEngines diverging because of different commands)
        
        Command cmd2_for_p1;
        cmd2_for_p1.tick = tick;
        cmd2_for_p1.player_id = 2;
        cmd2_for_p1.set_spawn_payload(20, 20);
        
        Command cmd2_for_p2;
        cmd2_for_p2.tick = tick;
        cmd2_for_p2.player_id = 2;
        cmd2_for_p2.set_spawn_payload(20, 20);

        if (simulate_desync && tick == DESYNC_TICK) {
            cmd2_for_p2.set_spawn_payload(30, 30); // Divergent command for p2's local simulation
        }
        
        // P1 sends to P2 and applies locally
        p1.sendInput(2, tick, LockstepBarrier::bundle_commands({cmd}));
        p1.sendInput(1, tick, LockstepBarrier::bundle_commands({cmd})); // self

        p2.sendInput(1, tick, LockstepBarrier::bundle_commands({cmd2_for_p1}));
        p2.sendInput(2, tick, LockstepBarrier::bundle_commands({cmd2_for_p2})); // self

        // Now barrier processes tick
        std::vector<Command> out_cmds1, out_cmds2;
        [[maybe_unused]] bool adv1 = bar1.poll_and_advance(tick, out_cmds1, 10, std::chrono::milliseconds(0));
        [[maybe_unused]] bool adv2 = bar2.poll_and_advance(tick, out_cmds2, 10, std::chrono::milliseconds(0));
        
        assert(adv1 && adv2);

        e1.tick(out_cmds1);
        e2.tick(out_cmds2);

        uint32_t h1 = e1.get_state().compute_hash();
        uint32_t h2 = e2.get_state().compute_hash();

        m1.add_local_hash(tick, h1);
        m2.add_local_hash(tick, h2);

        // Exchange hashes
        p1.sendStateHash(2, tick, h1);
        p1.sendStateHash(1, tick, h1); // self
        p2.sendStateHash(1, tick, h2);
        p2.sendStateHash(2, tick, h2); // self

        // Ensure that poll_and_check fetches the newly sent hashes
        // For poll_and_check to work for a tick, it has to fetch all queued hashes 
        // up to that tick. Because pollStateHashes dequeues only for a specific tick
        // in our implementation, and poll_and_check iterates over local_hashes keys.
        m1.poll_and_check();
        m2.poll_and_check();

        if (simulate_desync && tick == DESYNC_TICK) {
            assert(d1_flag || d2_flag); // One or both will detect mismatch depending on order/self-reporting
            std::cout << "Desync detected accurately at tick " << DESYNC_TICK << std::endl;
            break;
        } else if (!simulate_desync || tick < DESYNC_TICK) {
            assert(!d1_flag);
            assert(!d2_flag);
        }
    }

    if (!simulate_desync) {
        std::cout << "Happy path completed successfully." << std::endl;
    }
}

int main() {
    run_test(false);
    run_test(true);
    return 0;
}
