#ifndef __TUPLE_T_HPP__
#define __TUPLE_T_HPP__

#include "constants.hpp"

struct tuple_t {
    unsigned int key;
    float property_value;
    float incremental_average;
    unsigned int timestamp;
};

#endif // __TUPLE_T_HPP__