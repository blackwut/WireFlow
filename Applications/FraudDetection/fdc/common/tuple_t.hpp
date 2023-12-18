#ifndef __TUPLE_T_HPP__
#define __TUPLE_T_HPP__

#include "constants.hpp"

struct tuple_t {
    unsigned int key;
    unsigned int state_id;
    float score;
    unsigned int timestamp;

    bool operator==(const tuple_t & other) const {
        return key == other.key && state_id == other.state_id;
    }
};

#endif // __TUPLE_T_HPP__