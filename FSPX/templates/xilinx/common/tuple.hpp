#ifndef __{{tuple_name | upper}}_HPP__
#define __{{tuple_name | upper}}_HPP__

#include "constants.hpp"

struct {{tuple_name}} {
    unsigned int key;
    float value;

    bool operator==(const {{tuple_name}} & other) const {
        return key == other.key && value == other.value;
    }
};

#endif // __{{tuple_name | upper}}_HPP__