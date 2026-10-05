#include "Peer.hpp"
#include <stdexcept>

LoopbackPeer::LoopbackPeer(LoopbackHub& hub, uint32_t myPeerId)
    : hub(hub), myPeerId(myPeerId), connected(false) {
}

void LoopbackPeer::connect() {
    connected = true;
}

void LoopbackPeer::disconnect() {
    connected = false;
}

void LoopbackPeer::sendInput(uint32_t peerId, uint32_t tick, const std::vector<uint8_t>& serializedInput) {
    if (!connected) {
        throw std::runtime_error("Cannot send input while disconnected");
    }
    hub.enqueue(myPeerId, peerId, tick, serializedInput);
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> LoopbackPeer::pollInputs(uint32_t tick) {
    if (!connected) {
        throw std::runtime_error("Cannot poll inputs while disconnected");
    }
    return hub.dequeue(myPeerId, tick);
}
