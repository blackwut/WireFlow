#ifndef __INPUT_T_HPP__
#define __INPUT_T_HPP__

#include "constants.hpp"

struct input_t {
    unsigned int key;
    float property_value;
#if MEASURE_LATENCY
    float incremental_average;
    unsigned int timestamp;
#endif
};

#endif // __INPUT_T_HPP__