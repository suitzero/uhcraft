#pragma once

#include <cstdint>

// Xoshiro256** RNG
// Deterministic RNG with explicit state management.
class DeterministicRNG {
public:
    explicit DeterministicRNG(uint64_t seed) {
        // Simple SplitMix64 to initialize state
        uint64_t z = (seed + 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        s[0] = z ^ (z >> 31);

        z = (seed + 2 * 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        s[1] = z ^ (z >> 31);

        z = (seed + 3 * 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        s[2] = z ^ (z >> 31);

        z = (seed + 4 * 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        s[3] = z ^ (z >> 31);
    }

    uint64_t next() {
        const uint64_t result = rotl(s[1] * 5, 7) * 9;

        const uint64_t t = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;

        s[3] = rotl(s[3], 45);

        return result;
    }

    // Generate random number in range [0, max)
    uint64_t next_range(uint64_t max) {
        if (max == 0) return 0;
        // Simple modulo, can be biased but fine for our simple physics
        return next() % max;
    }

private:
    uint64_t s[4];

    static inline uint64_t rotl(const uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};
