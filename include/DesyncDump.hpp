#pragma once

#include "SimulationState.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

class DesyncDump {
public:
    static std::string dump_state(const SimulationState& state) {
        std::stringstream ss;
        ss << "tick: " << state.tick << "\n";
        ss << "active_unit_count: " << state.active_unit_count << "\n";
        
        for (uint32_t i = 0; i < state.active_unit_count; ++i) {
            const Unit& u = state.units[i];
            ss << "unit[" << i << "].id: " << u.id << "\n";
            ss << "unit[" << i << "].position.x: " << u.position.x.value << "\n";
            ss << "unit[" << i << "].position.y: " << u.position.y.value << "\n";
            ss << "unit[" << i << "].velocity.x: " << u.velocity.x.value << "\n";
            ss << "unit[" << i << "].velocity.y: " << u.velocity.y.value << "\n";
            ss << "unit[" << i << "].active: " << (u.active ? 1 : 0) << "\n";
        }
        
        return ss.str();
    }

    static std::string diff_dumps(const std::string& dump1, const std::string& dump2) {
        std::vector<std::string> lines1 = split_lines(dump1);
        std::vector<std::string> lines2 = split_lines(dump2);
        
        std::stringstream diff;
        size_t max_lines = std::max(lines1.size(), lines2.size());
        bool has_diff = false;

        for (size_t i = 0; i < max_lines; ++i) {
            std::string l1 = (i < lines1.size()) ? lines1[i] : "<missing>";
            std::string l2 = (i < lines2.size()) ? lines2[i] : "<missing>";
            
            if (l1 != l2) {
                has_diff = true;
                diff << "Difference at line " << (i + 1) << ":\n";
                diff << "  Expected (Dump 1): " << l1 << "\n";
                diff << "  Actual   (Dump 2): " << l2 << "\n";
            }
        }

        if (!has_diff) {
            return "No differences found.";
        }
        return diff.str();
    }

private:
    static std::vector<std::string> split_lines(const std::string& str) {
        std::vector<std::string> lines;
        std::stringstream ss(str);
        std::string line;
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }
        return lines;
    }
};
