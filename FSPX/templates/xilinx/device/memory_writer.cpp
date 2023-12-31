#include "includes/defines.hpp"

extern "C" {
void memory_writer(
    mw_axis_stream_t & in,
    line_t * out,
    int out_count,
    int * items_written,
    int * eos
)
{
    #pragma HLS INTERFACE mode=m_axi port=out bundle=out num_write_outstanding=64 max_write_burst_length=64

    if (eos[0] == 0) {
        fx::StoWM(in, out, out_count, items_written, eos);
    }
}
}
