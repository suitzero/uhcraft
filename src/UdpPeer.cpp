#include "UdpPeer.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <chrono>

static uint64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

static void write_uint64_le(std::vector<uint8_t>& buf, uint64_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 32) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 40) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 48) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 56) & 0xFF));
}

static uint64_t read_uint64_le(const uint8_t* buf) {
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32) |
           (static_cast<uint64_t>(buf[5]) << 40) |
           (static_cast<uint64_t>(buf[6]) << 48) |
           (static_cast<uint64_t>(buf[7]) << 56);
}

static void write_uint32_le(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

static uint32_t read_uint32_le(const uint8_t* buf) {
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

UdpPeer::UdpPeer(uint32_t myPeerId, 
                 const std::string& localBindHost, uint16_t localBindPort,
                 const std::map<uint32_t, std::pair<std::string, uint16_t>>& peerAddresses,
                 size_t redundancy)
    : myPeerId(myPeerId), localBindHost(localBindHost), localBindPort(localBindPort), 
      redundancy(redundancy), sock_fd(-1), connected(false), dropNextPackets(0),
      lastPolledInputTick(0), lastPolledHashTick(0)
{
    recvBuffer.resize(65536);
    
    for (const auto& kv : peerAddresses) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kv.second.second);
        inet_pton(AF_INET, kv.second.first.c_str(), &addr.sin_addr);
        this->peerAddresses[kv.first] = addr;
        smoothedRttMs[kv.first] = 0;
        lastPingSentTime[kv.first] = 0;
    }
}

UdpPeer::~UdpPeer() {
    disconnect();
}

void UdpPeer::connect() {
    if (connected) return;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(localBindPort);
    inet_pton(AF_INET, localBindHost.c_str(), &localAddr.sin_addr);

    if (bind(sock_fd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        close(sock_fd);
        sock_fd = -1;
        throw std::runtime_error("Failed to bind UDP socket");
    }

    // Set non-blocking
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    connected = true;
}

void UdpPeer::disconnect() {
    if (!connected) return;
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    connected = false;
}

void UdpPeer::setDropNextPackets(int count) {
    dropNextPackets = count;
}

void UdpPeer::sendPing(uint32_t peerId) {
    uint64_t now = current_time_ms();
    
    // Send ping roughly every 100ms
    if (now - lastPingSentTime[peerId] < 100) return;
    lastPingSentTime[peerId] = now;

    std::vector<uint8_t> packet;
    packet.push_back(1); // Version
    packet.push_back(2); // Type = Ping
    write_uint32_le(packet, myPeerId);
    write_uint64_le(packet, now);
    
    if (dropNextPackets > 0) {
        dropNextPackets--;
        return;
    }

    sendto(sock_fd, packet.data(), packet.size(), 0, 
           (struct sockaddr*)&peerAddresses[peerId], sizeof(peerAddresses[peerId]));
}

void UdpPeer::sendPong(uint32_t peerId, uint64_t timestamp) {
    std::vector<uint8_t> packet;
    packet.push_back(1); // Version
    packet.push_back(3); // Type = Pong
    write_uint32_le(packet, myPeerId);
    write_uint64_le(packet, timestamp);
    
    // Note: Pongs usually shouldn't be artificially dropped by dropNextPackets for tests, 
    // but applying it for consistency.
    if (dropNextPackets > 0) {
        dropNextPackets--;
        return;
    }

    sendto(sock_fd, packet.data(), packet.size(), 0, 
           (struct sockaddr*)&peerAddresses[peerId], sizeof(peerAddresses[peerId]));
}

uint32_t UdpPeer::getRTT(uint32_t peerId) const {
    auto it = smoothedRttMs.find(peerId);
    if (it != smoothedRttMs.end()) {
        return it->second;
    }
    return 0;
}

void UdpPeer::sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) {
    if (!connected) throw std::runtime_error("UdpPeer not connected");
    if (peerAddresses.find(peerId) == peerAddresses.end()) return; // Unknown peer

    sendPing(peerId);

    // Save history
    sendHistory[peerId][tick] = serializedInput;

    // Prune old history
    if (sendHistory[peerId].size() > redundancy + 1) {
        auto it = sendHistory[peerId].begin();
        sendHistory[peerId].erase(it); // Erase oldest
    }

    // Construct packet
    // Version(1), Type=0(1), SenderPeerId(4), Tick(4), RedundancyCount(1)
    std::vector<uint8_t> packet;
    packet.push_back(1); // Version
    packet.push_back(0); // Type = Input
    write_uint32_le(packet, myPeerId);
    write_uint32_le(packet, tick);
    
    uint8_t redundancyCount = static_cast<uint8_t>(sendHistory[peerId].size());
    packet.push_back(redundancyCount);

    for (auto it = sendHistory[peerId].rbegin(); it != sendHistory[peerId].rend(); ++it) {
        write_uint32_le(packet, it->first); // EntryTick
        write_uint32_le(packet, static_cast<uint32_t>(it->second.size())); // Length
        packet.insert(packet.end(), it->second.begin(), it->second.end()); // Payload
    }

    // dropNextPackets is handled globally or per-send. We just use it here too.
    if (dropNextPackets > 0) {
        dropNextPackets--;
        return; // Simulate loss
    }

    sendto(sock_fd, packet.data(), packet.size(), 0, 
           (struct sockaddr*)&peerAddresses[peerId], sizeof(peerAddresses[peerId]));
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> UdpPeer::pollInputs(uint32_t tick) {
    if (!connected) throw std::runtime_error("UdpPeer not connected");
    
    // Opportunistically ping peers we haven't pinged recently
    for (const auto& kv : peerAddresses) {
        sendPing(kv.first);
    }
    
    drainSocket();
    
    if (tick > lastPolledInputTick) {
        lastPolledInputTick = tick;
    }

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> results;
    if (receivedInputs.count(tick)) {
        for (const auto& kv : receivedInputs[tick]) {
            results.push_back({kv.first, kv.second});
        }
        receivedInputs.erase(tick);
    }
    
    // Clean up any stale inputs below the tick we just polled, just in case
    for (auto it = receivedInputs.begin(); it != receivedInputs.end(); ) {
        if (it->first < tick) {
            it = receivedInputs.erase(it);
        } else {
            ++it;
        }
    }
    
    return results;
}

void UdpPeer::sendStateHash(uint32_t peerId, uint32_t tick, uint32_t hash, uint32_t delay_vote) {
    if (!connected) throw std::runtime_error("UdpPeer not connected");
    if (peerAddresses.find(peerId) == peerAddresses.end()) return;

    // Version(1), Type=1(1), SenderPeerId(4), Tick(4), Hash(4), DelayVote(4)
    std::vector<uint8_t> packet;
    packet.push_back(1); // Version
    packet.push_back(1); // Type = Hash
    write_uint32_le(packet, myPeerId);
    write_uint32_le(packet, tick);
    write_uint32_le(packet, hash);
    write_uint32_le(packet, delay_vote);

    if (dropNextPackets > 0) {
        dropNextPackets--;
        return; // Simulate loss
    }

    sendto(sock_fd, packet.data(), packet.size(), 0, 
           (struct sockaddr*)&peerAddresses[peerId], sizeof(peerAddresses[peerId]));
}

std::vector<StateHashResult> UdpPeer::pollStateHashes(uint32_t tick) {
    if (!connected) throw std::runtime_error("UdpPeer not connected");

    drainSocket();
    
    if (tick > lastPolledHashTick) {
        lastPolledHashTick = tick;
    }

    std::vector<StateHashResult> results;
    if (receivedHashes.count(tick)) {
        for (const auto& kv : receivedHashes[tick]) {
            results.push_back({kv.first, kv.second.first, kv.second.second});
        }
        receivedHashes.erase(tick);
    }
    
    // Clean up any stale hashes
    for (auto it = receivedHashes.begin(); it != receivedHashes.end(); ) {
        if (it->first < tick) {
            it = receivedHashes.erase(it);
        } else {
            ++it;
        }
    }
    
    return results;
}

void UdpPeer::drainSocket() {
    while (true) {
        sockaddr_in senderAddr{};
        socklen_t senderLen = sizeof(senderAddr);
        ssize_t bytesRead = recvfrom(sock_fd, recvBuffer.data(), recvBuffer.size(), 0, 
                                     (struct sockaddr*)&senderAddr, &senderLen);
        if (bytesRead < 0) {
            break; // No more data
        }

        if (bytesRead < 10) continue; // Minimum valid packet size

        const uint8_t* ptr = recvBuffer.data();
        uint8_t version = ptr[0];
        uint8_t type = ptr[1];

        if (version != 1) continue;

        uint32_t senderPeerId = read_uint32_le(ptr + 2);
        // uint32_t packetTick = read_uint32_le(ptr + 6); // Could be used if needed

        if (type == 0) {
            // Input packet
            if (bytesRead < 11) continue;
            uint8_t redundancyCount = ptr[10];
            
            size_t offset = 11;
            for (int i = 0; i < redundancyCount; ++i) {
                if (offset + 8 > (size_t)bytesRead) break;
                
                uint32_t entryTick = read_uint32_le(ptr + offset);
                uint32_t length = read_uint32_le(ptr + offset + 4);
                offset += 8;
                
                if (offset + length > (size_t)bytesRead) break;

                // Only insert if it's a tick we might still care about and haven't already buffered
                if (entryTick >= lastPolledInputTick) {
                    if (receivedInputs[entryTick].find(senderPeerId) == receivedInputs[entryTick].end()) {
                        receivedInputs[entryTick][senderPeerId] = std::vector<uint8_t>(ptr + offset, ptr + offset + length);
                    }
                }

                offset += length;
            }
        } else if (type == 1) {
            // Hash packet
            if (bytesRead < 14) continue;
            uint32_t hashTick = read_uint32_le(ptr + 6);
            uint32_t hash = read_uint32_le(ptr + 10);
            uint32_t delay_vote = 0;
            if (bytesRead >= 18) {
                delay_vote = read_uint32_le(ptr + 14);
            }
            
            if (hashTick >= lastPolledHashTick) {
                receivedHashes[hashTick][senderPeerId] = {hash, delay_vote};
            }
        } else if (type == 2) {
            // Ping packet
            if (bytesRead < 14) continue;
            uint64_t timestamp = read_uint64_le(ptr + 6);
            sendPong(senderPeerId, timestamp);
        } else if (type == 3) {
            // Pong packet
            if (bytesRead < 14) continue;
            uint64_t timestamp = read_uint64_le(ptr + 6);
            uint64_t now = current_time_ms();
            if (now >= timestamp) {
                uint32_t rtt = static_cast<uint32_t>(now - timestamp);
                if (smoothedRttMs[senderPeerId] == 0) {
                    smoothedRttMs[senderPeerId] = rtt;
                } else {
                    // EWMA with alpha = 0.125
                    smoothedRttMs[senderPeerId] = (smoothedRttMs[senderPeerId] * 7 + rtt) / 8;
                }
            }
        }
    }
}
