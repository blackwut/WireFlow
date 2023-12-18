
#ifndef __DEFINES_HPP__
#define __DEFINES_HPP__

#include "fspx.hpp"
#include "tuple_t.hpp"
#include "input_t.hpp"
#include "result_t.hpp"


static constexpr int BUS_WIDTH = 512;
using line_t = ap_uint<BUS_WIDTH>;

using mr_axis_stream_t = fx::axis_stream<input_t, 16>;
using mw_axis_stream_t = fx::axis_stream<result_t, 16>;

using stream_tuple_t = fx::stream<tuple_t, 16>;
using stream_input_t = fx::stream<input_t, 16>;
using stream_result_t = fx::stream<result_t, 16>;


#endif // __DEFINES_HPP__