
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
        out.state_id = static_cast<unsigned int>(i % NUM_STATES);
        out.score = 0;
        out.timestamp = 0;

        last = (i == size - 1);
    }
};