
#include "common/constants.hpp"
#include "common/tuple_t.hpp"
#include "common/result_t.hpp"

struct frequency
{
    unsigned int frequencies[NUM_STATES];

    frequency()
    {
    #pragma HLS ARRAY_PARTITION variable=frequencies complete dim=1
        // initialize frequencies
        for (int i = 0; i < NUM_STATES; ++i) {
        #pragma HLS unroll
            frequencies[i] = 0;
        }
    }

    void operator()(tuple_t in, result_t & out)
    {
    #pragma HLS INLINE
        const unsigned int key = in.key;
        const float score = in.score;
        const unsigned int state_id = in.state_id;
        const unsigned int timestamp = in.timestamp;

        // update frequency
        const unsigned int freq = ++frequencies[state_id];

        // update output
        out.key = key;
        out.state_id = state_id;
        out.score = score;
        out.frequency = freq;
        out.timestamp = timestamp;
    }
};