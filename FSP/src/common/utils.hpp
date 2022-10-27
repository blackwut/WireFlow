#pragma once

#include <type_traits>
#include <limits>
#include <cstdint>
#include <cmath>
#include <random>
#include <sys/time.h>

#define ALWAYS_INLINE           inline __attribute__((always_inline))
#define TEMPLATE_FLOATING       template<typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
#define TEMPLATE_INTEGRAL       template<typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
#define TEMPLATE_INTEGRAL_(x)   template<typename T, typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == x), bool>::type = true>
#define TEMPLATE_INTEGRAL_32    TEMPLATE_INTEGRAL_(4)
#define TEMPLATE_INTEGRAL_64    TEMPLATE_INTEGRAL_(8)

ALWAYS_INLINE uint64_t current_time_ns()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (t.tv_sec) * uint64_t(1000000000) + t.tv_nsec;
}

ALWAYS_INLINE uint64_t current_time_us()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (t.tv_sec) * uint64_t(1000000) + t.tv_nsec / uint64_t(1000);
}

ALWAYS_INLINE uint64_t current_time_ms()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (t.tv_sec) * uint64_t(1000) + t.tv_nsec / uint64_t(1000000);
}



TEMPLATE_INTEGRAL
ALWAYS_INLINE T divide_up(T size, T div)
{
    return (size + div - 1) / div;
}

TEMPLATE_INTEGRAL
ALWAYS_INLINE T round_up(T size, T div)
{
    return divide_up(size, div) * div;
}


TEMPLATE_INTEGRAL_32
ALWAYS_INLINE T next_pow2(T v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    v += (v == 0);
    return v;
}

TEMPLATE_INTEGRAL_64
ALWAYS_INLINE T next_pow2(T v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    v += (v == 0);
    return v;
}


TEMPLATE_INTEGRAL
ALWAYS_INLINE T log_2(T v)
{
    T r = 0;
    while (v >>= 1) { r++; }
    return r;
}


TEMPLATE_FLOATING
ALWAYS_INLINE bool approximatelyEqual(const T a, const T b, const T epsilon = std::numeric_limits<T>::epsilon())
{
    return std::abs(a - b) <= ( (std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a)) * epsilon);
}


TEMPLATE_FLOATING
ALWAYS_INLINE bool essentiallyEqual(const T a, const T b, const T epsilon = std::numeric_limits<T>::epsilon())
{
    return std::abs(a - b) <= ( (std::abs(a) > std::abs(b) ? std::abs(b) : std::abs(a)) * epsilon);
}


TEMPLATE_FLOATING
ALWAYS_INLINE T next_float(const T from, const T to)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<T> dist(from, to);

    return dist(gen);
}