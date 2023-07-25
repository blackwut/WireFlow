{%- macro include_tuples(tuples) -%}
{% for t in tuples %}
#include "{{ t }}.hpp"
{% endfor %}
{%- endmacro %}

{%- macro define_axis_stream(mr, mw) %}
using mr_axis_stream_t = fx::axis_stream<{{mr.o_datatype}}, 16>;
using mw_axis_stream_t = fx::axis_stream<{{mw.o_datatype}}, 16>;
{%- endmacro %}

{%- macro define_stream(datatype) %}
using stream_{{datatype}} = fx::stream<{{datatype}}, 16>;
{%- endmacro %}

#ifndef __DEFINES_HPP__
#define __DEFINES_HPP__

#include "fspx.hpp"
{{include_tuples(tuples)}}

static constexpr int BUS_WIDTH = 512;
using line_t = ap_uint<BUS_WIDTH>;

{{define_axis_stream(mr, mw)}}
{% for dt in datatypes %}
{{define_stream(dt)}}
{% endfor %}

#endif // __DEFINES_HPP__