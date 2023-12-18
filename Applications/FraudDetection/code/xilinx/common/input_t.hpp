#ifndef __INPUT_T_HPP__
#define __INPUT_T_HPP__

#include "constants.hpp"

struct input_t {
    unsigned int key;
    unsigned int state_id;
    float score;
    unsigned int timestamp;

    bool operator==(const input_t & other) const {
        return key == other.key && state_id == other.state_id;
    }
};

#endif // __INPUT_T_HPP__