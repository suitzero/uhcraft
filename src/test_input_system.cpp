#include <iostream>
#include <vector>
#include <cassert>
#include "Command.hpp"
#include "InputBuffer.hpp"

void test_command_serialization() {
    Command cmd1;
    cmd1.tick = 42;
    cmd1.player_id = 1;
    cmd1.set_spawn_payload(100, -200);

    std::vector<uint8_t> buffer = cmd1.serialize();
    Command cmd2 = Command::deserialize(buffer);

    assert(cmd1 == cmd2);
    assert(cmd1.tick == cmd2.tick);
    assert(cmd1.player_id == cmd2.player_id);
    assert(cmd1.type == cmd2.type);
    
    int32_t x = 0, y = 0;
    cmd2.get_spawn_payload(x, y);
    assert(x == 100);
    assert(y == -200);
}

void test_command_ordering() {
    Command cmd1, cmd2, cmd3;
    cmd1.tick = 1; cmd1.player_id = 2; cmd1.type = Command::Type::SPAWN_UNIT;
    cmd2.tick = 1; cmd2.player_id = 1; cmd2.type = Command::Type::CHANGE_DIRECTION;
    cmd3.tick = 2; cmd3.player_id = 1; cmd3.type = Command::Type::SPAWN_UNIT;

    // cmd2 < cmd1 (same tick, player 1 < player 2)
    assert(cmd2 < cmd1);
    
    // cmd1 < cmd3 (tick 1 < tick 2)
    assert(cmd1 < cmd3);
}

void test_input_buffer() {
    InputBuffer buffer;

    Command cmd_p1;
    cmd_p1.tick = 10;
    cmd_p1.player_id = 1;
    cmd_p1.type = Command::Type::SPAWN_UNIT;

    Command cmd_p2;
    cmd_p2.tick = 10;
    cmd_p2.player_id = 2;
    cmd_p2.type = Command::Type::CHANGE_DIRECTION;

    // Add out of order to ensure get_commands_for_tick sorts them
    buffer.enqueue(cmd_p2);
    buffer.enqueue(cmd_p1);

    auto cmds_tick10 = buffer.get_commands_for_tick(10);
    assert(cmds_tick10.size() == 2);
    // Player 1 should be before Player 2 due to deterministic sorting
    assert(cmds_tick10[0].player_id == 1);
    assert(cmds_tick10[1].player_id == 2);

    // Test scheduling
    Command cmd_sched;
    cmd_sched.tick = 10; // Base tick
    cmd_sched.player_id = 3;
    buffer.schedule(cmd_sched, 5); // Delay by 5 ticks

    auto cmds_tick15 = buffer.get_commands_for_tick(15);
    assert(cmds_tick15.size() == 1);
    assert(cmds_tick15[0].player_id == 3);
    assert(cmds_tick15[0].tick == 15);
    
    auto cmds_tick10_again = buffer.get_commands_for_tick(10);
    assert(cmds_tick10_again.empty()); // The previous call erased tick 10 cmds
}

int main() {
    std::cout << "Running Input System Tests..." << std::endl;
    
    test_command_serialization();
    std::cout << " - Serialization OK" << std::endl;
    
    test_command_ordering();
    std::cout << " - Command Ordering OK" << std::endl;
    
    test_input_buffer();
    std::cout << " - Input Buffer OK" << std::endl;
    
    std::cout << "All Input System Tests Passed!" << std::endl;
    return 0;
}
