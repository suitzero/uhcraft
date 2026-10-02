#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>

// Define max payload size (e.g., 64 bytes)
constexpr size_t MAX_PAYLOAD_SIZE = 64;

// Commands represent deterministic inputs applied during a tick
struct Command {
    uint32_t tick;
    uint32_t player_id;

    enum class Type : uint32_t { NONE, SPAWN_UNIT, CHANGE_DIRECTION };
    Type type;

    uint32_t payload_size;
    std::array<uint8_t, MAX_PAYLOAD_SIZE> payload;

    Command() : tick(0), player_id(0), type(Type::NONE), payload_size(0), payload{} {}

    // Serialization helper function for little endian
    static void write_uint32_le(std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back(static_cast<uint8_t>(val & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    static void write_int32_le(std::vector<uint8_t>& buf, int32_t val) {
        write_uint32_le(buf, static_cast<uint32_t>(val));
    }

    static uint32_t read_uint32_le(const uint8_t* buf) {
        return static_cast<uint32_t>(buf[0]) |
               (static_cast<uint32_t>(buf[1]) << 8) |
               (static_cast<uint32_t>(buf[2]) << 16) |
               (static_cast<uint32_t>(buf[3]) << 24);
    }

    static int32_t read_int32_le(const uint8_t* buf) {
        return static_cast<int32_t>(read_uint32_le(buf));
    }

    // Helper functions to interact with payload
    void set_spawn_payload(int32_t x, int32_t y) {
        type = Type::SPAWN_UNIT;
        payload_size = 8;
        payload.fill(0);
        std::vector<uint8_t> buf;
        write_int32_le(buf, x);
        write_int32_le(buf, y);
        for(size_t i = 0; i < 8; ++i) payload[i] = buf[i];
    }

    void get_spawn_payload(int32_t& x, int32_t& y) const {
        if (type == Type::SPAWN_UNIT && payload_size >= 8) {
            x = read_int32_le(&payload[0]);
            y = read_int32_le(&payload[4]);
        }
    }

    void set_dir_payload(uint32_t unit_id, int32_t dx, int32_t dy) {
        type = Type::CHANGE_DIRECTION;
        payload_size = 12;
        payload.fill(0);
        std::vector<uint8_t> buf;
        write_uint32_le(buf, unit_id);
        write_int32_le(buf, dx);
        write_int32_le(buf, dy);
        for(size_t i = 0; i < 12; ++i) payload[i] = buf[i];
    }

    void get_dir_payload(uint32_t& unit_id, int32_t& dx, int32_t& dy) const {
        if (type == Type::CHANGE_DIRECTION && payload_size >= 12) {
            unit_id = read_uint32_le(&payload[0]);
            dx = read_int32_le(&payload[4]);
            dy = read_int32_le(&payload[8]);
        }
    }

    // Full serialize of this command
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        write_uint32_le(buf, tick);
        write_uint32_le(buf, player_id);
        write_uint32_le(buf, static_cast<uint32_t>(type));
        write_uint32_le(buf, payload_size);
        for (uint32_t i = 0; i < payload_size; ++i) {
            buf.push_back(payload[i]);
        }
        return buf;
    }

    // Full deserialize
    static Command deserialize(const std::vector<uint8_t>& buf, size_t offset = 0) {
        Command cmd;
        if (buf.size() >= offset + 16) {
            cmd.tick = read_uint32_le(buf.data() + offset);
            cmd.player_id = read_uint32_le(buf.data() + offset + 4);
            cmd.type = static_cast<Type>(read_uint32_le(buf.data() + offset + 8));
            cmd.payload_size = read_uint32_le(buf.data() + offset + 12);
            cmd.payload.fill(0);

            // Cap payload size to prevent buffer overflow
            if (cmd.payload_size > MAX_PAYLOAD_SIZE) {
                cmd.payload_size = MAX_PAYLOAD_SIZE;
            }

            if (buf.size() >= offset + 16 + cmd.payload_size) {
                for (uint32_t i = 0; i < cmd.payload_size; ++i) {
                    cmd.payload[i] = buf[offset + 16 + i];
                }
            }
        }
        return cmd;
    }

    // Equality operator
    bool operator==(const Command& other) const {
        if (tick != other.tick) return false;
        if (player_id != other.player_id) return false;
        if (type != other.type) return false;
        if (payload_size != other.payload_size) return false;
        for (uint32_t i = 0; i < payload_size; ++i) {
            if (payload[i] != other.payload[i]) return false;
        }
        return true;
    }

    // Ordering operator (sort by tick, then player_id, then type)
    bool operator<(const Command& other) const {
        if (tick != other.tick) return tick < other.tick;
        if (player_id != other.player_id) return player_id < other.player_id;
        return type < other.type;
    }
};
