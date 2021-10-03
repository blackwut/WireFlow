#pragma once

#include <limits>
#include <cstdint>
#include <cmath>
#include <random>
#include <sys/time.h>


inline uint64_t current_time_ns() __attribute__((always_inline));
inline uint64_t current_time_ns()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (t.tv_sec) * uint64_t(1000000000) + t.tv_nsec;
}

inline float next_float() __attribute__((always_inline));
inline float next_float()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(36.5, 37.5);

    return dist(gen);
}


inline size_t divide_up() __attribute__((always_inline));
inline size_t divide_up(size_t size, size_t div)
{
    return (size + div - 1) / div;
}

inline size_t round_up() __attribute__((always_inline));
inline size_t round_up(size_t size, size_t div)
{
    return divide_up(size, div) * div;
}

inline bool approximatelyEqual() __attribute__((always_inline));
inline bool approximatelyEqual(float a, float b, float epsilon = std::numeric_limits<float>::epsilon())
{
    return fabs(a - b) <= ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline bool essentiallyEqual() __attribute__((always_inline));
inline bool essentiallyEqual(float a, float b, float epsilon = std::numeric_limits<float>::epsilon())
{
    return fabs(a - b) <= ( (fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}
