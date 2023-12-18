{%- macro include_tuples(tuples) -%}
{% for t in tuples %}
#include "{{ t }}.hpp"
{% endfor %}
{%- endmacro %}

{%- macro define_axis_streams(mr, mw) %}
{% if mr %}
using mr_axis_stream_t = fx::axis_stream<{{mr.o_datatype}}, 16>;
{% endif %}
{% if mw %}
using mw_axis_stream_t = fx::axis_stream<{{mw.o_datatype}}, 16>;
{% endif %}
{%- endmacro %}

{%- macro define_streams(tuples) %}
{% for t in tuples %}
using stream_{{t}} = fx::stream<{{t}}, 16>;
{% endfor %}
{%- endmacro %}

#ifndef __DEFINES_HPP__
#define __DEFINES_HPP__

#include "fspx.hpp"
{{include_tuples(tuples)}}

static constexpr int BUS_WIDTH = 512;
using line_t = ap_uint<BUS_WIDTH>;

{{define_axis_streams(mr, mw)}}
{{define_streams(tuples)}}

#endif // __DEFINES_HPP__