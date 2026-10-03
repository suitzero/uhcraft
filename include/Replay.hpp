#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "Command.hpp"

struct ReplayHeader {
    static constexpr uint32_t MAGIC = 0x5245504C; // "REPL"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint64_t seed = 0;
    uint32_t start_state_hash = 0;
    uint32_t tick_count = 0;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        Command::write_uint32_le(buf, magic);
        Command::write_uint32_le(buf, version);
        Command::write_uint32_le(buf, static_cast<uint32_t>(seed & 0xFFFFFFFF));
        Command::write_uint32_le(buf, static_cast<uint32_t>((seed >> 32) & 0xFFFFFFFF));
        Command::write_uint32_le(buf, start_state_hash);
        Command::write_uint32_le(buf, tick_count);
        return buf;
    }

    static ReplayHeader deserialize(const std::vector<uint8_t>& buf, size_t offset = 0) {
        ReplayHeader header;
        if (buf.size() >= offset + 24) {
            header.magic = Command::read_uint32_le(buf.data() + offset);
            header.version = Command::read_uint32_le(buf.data() + offset + 4);
            uint32_t seed_low = Command::read_uint32_le(buf.data() + offset + 8);
            uint32_t seed_high = Command::read_uint32_le(buf.data() + offset + 12);
            header.seed = (static_cast<uint64_t>(seed_high) << 32) | seed_low;
            header.start_state_hash = Command::read_uint32_le(buf.data() + offset + 16);
            header.tick_count = Command::read_uint32_le(buf.data() + offset + 20);
        }
        return header;
    }

    bool operator==(const ReplayHeader& other) const {
        return magic == other.magic &&
               version == other.version &&
               seed == other.seed &&
               start_state_hash == other.start_state_hash &&
               tick_count == other.tick_count;
    }
};

class ReplayWriter {
public:
    ReplayWriter(const std::string& filename, uint64_t seed, uint32_t start_hash)
        : filename(filename) {
        header.seed = seed;
        header.start_state_hash = start_hash;
        header.tick_count = 0;

        file.open(filename, std::ios::binary | std::ios::out);
        if (file.is_open()) {
            // Write placeholder header
            std::vector<uint8_t> buf = header.serialize();
            file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        }
    }

    ~ReplayWriter() {
        close();
    }

    void write_command(const Command& cmd) {
        if (!file.is_open()) return;

        std::vector<uint8_t> buf = cmd.serialize();
        uint32_t size = buf.size();
        
        // Write size prefix then command bytes
        std::vector<uint8_t> size_buf;
        Command::write_uint32_le(size_buf, size);
        file.write(reinterpret_cast<const char*>(size_buf.data()), size_buf.size());
        file.write(reinterpret_cast<const char*>(buf.data()), buf.size());

        // Update max tick
        if (cmd.tick > header.tick_count) {
            header.tick_count = cmd.tick;
        }
    }
    
    void close() {
        if (file.is_open()) {
            // Rewrite header with updated tick_count
            file.seekp(0);
            std::vector<uint8_t> buf = header.serialize();
            file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
            file.close();
        }
    }

private:
    std::string filename;
    ReplayHeader header;
    std::ofstream file;
};

class ReplayReader {
public:
    ReplayReader(const std::string& filename) : filename(filename) {
        file.open(filename, std::ios::binary | std::ios::in);
        if (file.is_open()) {
            std::vector<uint8_t> buf(24);
            file.read(reinterpret_cast<char*>(buf.data()), 24);
            if (file.gcount() == 24) {
                header = ReplayHeader::deserialize(buf);
                valid = (header.magic == ReplayHeader::MAGIC && header.version == ReplayHeader::VERSION);
            }
        }
    }

    const ReplayHeader& get_header() const {
        return header;
    }

    bool is_valid() const {
        return valid;
    }

    bool read_command(Command& cmd) {
        if (!file.is_open() || !valid) return false;

        std::vector<uint8_t> size_buf(4);
        file.read(reinterpret_cast<char*>(size_buf.data()), 4);
        if (file.gcount() != 4) return false;

        uint32_t size = Command::read_uint32_le(size_buf.data());
        
        std::vector<uint8_t> cmd_buf(size);
        file.read(reinterpret_cast<char*>(cmd_buf.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return false;

        cmd = Command::deserialize(cmd_buf);
        return true;
    }

private:
    std::string filename;
    ReplayHeader header;
    std::ifstream file;
    bool valid = false;
};
