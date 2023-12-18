
#include "common/constants.hpp"
#include "common/tuple_t.hpp"


struct drainer
{
    void operator()(const int i, const tuple_t & in, const bool last) {
    #pragma HLS INLINE
    }
};
