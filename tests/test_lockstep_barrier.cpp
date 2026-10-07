#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <chrono>
#include "Peer.hpp"
#include "Command.hpp"
#include "LockstepBarrier.hpp"

// Reusing LoopbackHub and LoopbackPeer implementation



void test_lockstep_barrier() {
    std::cout << "Testing LockstepBarrier..." << std::endl;

    LoopbackHub hub;
    LoopbackPeer peer1(hub, 1);
    LoopbackPeer peer2(hub, 2);
    LoopbackPeer peer3(hub, 3);

    peer1.connect();
    peer2.connect();
    peer3.connect();

    // All peers expect inputs from 1, 2, and 3
    std::vector<uint32_t> expected = {1, 2, 3};
    LockstepBarrier barrier1(&peer1, expected);
    LockstepBarrier barrier2(&peer2, expected);

    std::vector<Command> out_commands;

    // Test a) Tick proceeds when all inputs arrive
    Command cmd1;
    cmd1.tick = 0; cmd1.player_id = 1; cmd1.set_spawn_payload(10, 10);
    Command cmd2;
    cmd2.tick = 0; cmd2.player_id = 2; cmd2.set_spawn_payload(20, 20);
    Command cmd3;
    cmd3.tick = 0; cmd3.player_id = 3; cmd3.set_spawn_payload(30, 30);

    peer1.sendInput(1, 0, LockstepBarrier::bundle_commands({cmd1}));
    peer2.sendInput(1, 0, LockstepBarrier::bundle_commands({cmd2}));
    peer3.sendInput(1, 0, LockstepBarrier::bundle_commands({cmd3}));

    bool advanced = barrier1.poll_and_advance(0, out_commands, 1, std::chrono::milliseconds(0));
    assert(advanced == true);
    assert(out_commands.size() == 3);
    // Deterministic sorting (Command::operator< sorts by tick, player_id, type)
    assert(out_commands[0].player_id == 1);
    assert(out_commands[1].player_id == 2);
    assert(out_commands[2].player_id == 3);
    std::cout << "Test (a) passed." << std::endl;

    // Test b) Tick blocks (returns false) while an input is missing
    Command cmd1_t1;
    cmd1_t1.tick = 1; cmd1_t1.player_id = 1; cmd1_t1.set_spawn_payload(11, 11);
    Command cmd2_t1;
    cmd2_t1.tick = 1; cmd2_t1.player_id = 2; cmd2_t1.set_spawn_payload(21, 21);
    // cmd3_t1 is missing for peer 2

    peer1.sendInput(2, 1, LockstepBarrier::bundle_commands({cmd1_t1}));
    peer2.sendInput(2, 1, LockstepBarrier::bundle_commands({cmd2_t1}));
    
    out_commands.clear();
    // Use low retries for fast blocking
    advanced = barrier2.poll_and_advance(1, out_commands, 3, std::chrono::milliseconds(1));
    assert(advanced == false);
    assert(out_commands.empty());
    std::cout << "Test (b) passed." << std::endl;

    // Test c & d) Tick resumes deterministically once missing input is provided, no deadlock
    Command cmd3_t1;
    cmd3_t1.tick = 1; cmd3_t1.player_id = 3; cmd3_t1.set_spawn_payload(31, 31);
    
    // Simulate delayed arrival
    peer3.sendInput(2, 1, LockstepBarrier::bundle_commands({cmd3_t1}));

    advanced = barrier2.poll_and_advance(1, out_commands, 3, std::chrono::milliseconds(1));
    assert(advanced == true);
    assert(out_commands.size() == 3);
    assert(out_commands[0].player_id == 1);
    assert(out_commands[1].player_id == 2);
    assert(out_commands[2].player_id == 3);
    std::cout << "Test (c) and (d) passed." << std::endl;
}

int main() {
    test_lockstep_barrier();
    return 0;
}
