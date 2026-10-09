#include "DesyncDump.hpp"
#include "SimulationEngine.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "Testing DesyncDump...\n";

    SimulationEngine e1(123);
    SimulationEngine e2(123);

    // Apply some commands
    std::vector<Command> cmds;
    Command c;
    c.tick = 0;
    c.player_id = 1;
    c.set_spawn_payload(50, 50);
    cmds.push_back(c);

    e1.tick(cmds);
    e2.tick(cmds);

    std::string dump1 = DesyncDump::dump_state(e1.get_state());
    std::string dump2 = DesyncDump::dump_state(e2.get_state());

    // Test 1: Dump determinism
    assert(dump1 == dump2);
    std::cout << "Determinism check passed.\n";

    // Test 2: Diff reports no difference for identical states
    std::string diff_same = DesyncDump::diff_dumps(dump1, dump2);
    assert(diff_same == "No differences found.");
    std::cout << "Identical diff check passed.\n";

    // Simulate divergence
    SimulationEngine e3(123);
    std::vector<Command> cmds_diverge;
    Command c2;
    c2.tick = 0;
    c2.player_id = 1;
    c2.set_spawn_payload(100, 100);
    cmds_diverge.push_back(c2);
    
    e3.tick(cmds_diverge);
    
    std::string dump3 = DesyncDump::dump_state(e3.get_state());
    
    // Test 3: Diff identifies exactly differing fields
    std::string diff_divergent = DesyncDump::diff_dumps(dump1, dump3);
    assert(diff_divergent != "No differences found.");
    
    std::cout << "Divergent diff check passed.\n";
    std::cout << "Sample diff:\n" << diff_divergent << "\n";

    return 0;
}
