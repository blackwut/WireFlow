
#include "common/constants.hpp"
#include "common/input_t.hpp"

struct generator {

    uint64_t size;
    unsigned int max_key;

    generator(uint64_t size, unsigned int max_key)
    : size(size)
    , max_key(max_key)
    {}

    void operator()(const uint64_t i, input_t & out, bool & last) {
    #pragma HLS INLINE
        out.key = static_cast<unsigned int>(i % max_key);
        out.property_value = static_cast<float>(i / (float)max_key);

        last = (i == size - 1);
    }
};