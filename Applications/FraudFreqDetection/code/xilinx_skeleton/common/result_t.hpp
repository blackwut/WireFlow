#ifndef __RESULT_T_HPP__
#define __RESULT_T_HPP__

#include "constants.hpp"

struct result_t {
    unsigned int key;
    unsigned int state_id;
    float score;
    unsigned int frequency;
    unsigned int timestamp;
    char padding[12];

    bool operator==(const result_t & other) const {
        return key == other.key && state_id == other.state_id;
    }
};

#endif // __RESULT_T_HPP__