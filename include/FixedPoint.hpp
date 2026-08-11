#pragma once

#include <cstdint>

// FixedPoint Q16.16 arithmetic for deterministic numerical computations
class FixedPoint {
public:
    int32_t value;

    static constexpr int FRACTIONAL_BITS = 16;
    static constexpr int32_t ONE = 1 << FRACTIONAL_BITS;

    constexpr FixedPoint() : value(0) {}
    constexpr explicit FixedPoint(int32_t raw) : value(raw) {}

    static constexpr FixedPoint from_int(int i) {
        return FixedPoint(i << FRACTIONAL_BITS);
    }

    // Careful with from_float, use only for initialization, not simulation logic
    static constexpr FixedPoint from_float(float f) {
        return FixedPoint(static_cast<int32_t>(f * ONE));
    }

    constexpr int to_int() const {
        return value >> FRACTIONAL_BITS;
    }

    FixedPoint operator+(const FixedPoint& other) const {
        return FixedPoint(value + other.value);
    }

    FixedPoint operator-(const FixedPoint& other) const {
        return FixedPoint(value - other.value);
    }

    FixedPoint operator*(const FixedPoint& other) const {
        int64_t temp = static_cast<int64_t>(value) * static_cast<int64_t>(other.value);
        return FixedPoint(static_cast<int32_t>(temp >> FRACTIONAL_BITS));
    }

    FixedPoint operator/(const FixedPoint& other) const {
        int64_t temp = (static_cast<int64_t>(value) << FRACTIONAL_BITS) / other.value;
        return FixedPoint(static_cast<int32_t>(temp));
    }

    FixedPoint& operator+=(const FixedPoint& other) {
        value += other.value;
        return *this;
    }

    FixedPoint& operator-=(const FixedPoint& other) {
        value -= other.value;
        return *this;
    }

    FixedPoint& operator*=(const FixedPoint& other) {
        *this = *this * other;
        return *this;
    }

    FixedPoint& operator/=(const FixedPoint& other) {
        *this = *this / other;
        return *this;
    }

    bool operator==(const FixedPoint& other) const {
        return value == other.value;
    }

    bool operator!=(const FixedPoint& other) const {
        return value != other.value;
    }

    bool operator<(const FixedPoint& other) const {
        return value < other.value;
    }

    bool operator<=(const FixedPoint& other) const {
        return value <= other.value;
    }

    bool operator>(const FixedPoint& other) const {
        return value > other.value;
    }

    bool operator>=(const FixedPoint& other) const {
        return value >= other.value;
    }
};
