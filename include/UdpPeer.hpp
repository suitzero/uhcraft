#pragma once

#include "Peer.hpp"
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <utility>
#include <netinet/in.h>
#include <mutex>

// UdpPeer implements the Peer interface over POSIX UDP sockets.
// Target environment: Linux/POSIX only.
class UdpPeer : public Peer {
public:
    // Takes local bind port, and a map of peerId -> {ip, port}
    UdpPeer(uint32_t myPeerId, 
            const std::string& localBindHost, uint16_t localBindPort,
            const std::map<uint32_t, std::pair<std::string, uint16_t>>& peerAddresses,
            size_t redundancy = 4);
    
    ~UdpPeer() override;

    void connect() override;
    void disconnect() override;
    void sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) override;
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> pollInputs(uint32_t tick) override;
    void sendStateHash(uint32_t peerId, uint32_t tick, uint32_t hash, uint32_t delay_vote = 0) override;
    std::vector<StateHashResult> pollStateHashes(uint32_t tick) override;
    uint32_t getRTT(uint32_t peerId) const override;

    // Test-only: force the next N sent datagrams to be dropped (not sent)
    void setDropNextPackets(int count);

private:
    void drainSocket();

    uint32_t myPeerId;
    std::string localBindHost;
    uint16_t localBindPort;
    std::map<uint32_t, sockaddr_in> peerAddresses;
    size_t redundancy;
    
    int sock_fd;
    bool connected;
    int dropNextPackets;

    // History for redundant send: peerId -> tick -> payload
    std::map<uint32_t, std::map<uint32_t, std::vector<uint8_t>>> sendHistory;

    // Received queues
    // For inputs: tick -> peerId -> payload (ensures dedup & ordering)
    std::map<uint32_t, std::map<uint32_t, std::vector<uint8_t>>> receivedInputs;
    
    // For hashes: tick -> peerId -> {hash, delay_vote}
    std::map<uint32_t, std::map<uint32_t, std::pair<uint32_t, uint32_t>>> receivedHashes;

    // A small buffer for receiving
    std::vector<uint8_t> recvBuffer;
    
    // Track the highest tick we've successfully polled to avoid buffering old redundant data
    uint32_t lastPolledInputTick;
    uint32_t lastPolledHashTick;

    // RTT tracking
    std::map<uint32_t, uint32_t> smoothedRttMs; // peerId -> smoothed RTT
    std::map<uint32_t, uint64_t> lastPingSentTime; // peerId -> last ping sent time (unix millis)

    void sendPing(uint32_t peerId);
    void sendPong(uint32_t peerId, uint64_t timestamp);
};
