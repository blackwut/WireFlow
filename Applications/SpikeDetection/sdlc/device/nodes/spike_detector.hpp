#include "common/constants.hpp"
#include "common/tuple_t.hpp"

#ifdef __SYNTHESIS__
#include <hls_math.h>
#endif

struct spike_detector
{
    void operator() (tuple_t in, tuple_t & out, bool & keep) {
    #pragma HLS INLINE
        out.key                 = in.key;
        out.property_value      = in.property_value;
        out.incremental_average = in.incremental_average;
        out.timestamp           = in.timestamp;
        keep = (fabs(in.property_value - in.incremental_average) > (float(THRESHOLD) * in.incremental_average));
    }
};
