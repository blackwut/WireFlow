{%- macro include_tuples(op) -%}
#include "common/{{op.i_datatype}}.hpp"
{% if op.i_datatype != op.o_datatype %}
#include "common/{{op.o_datatype}}.hpp"
{%- endif %}
{%- endmacro %}

{%- macro map_functor(t_in, t_out) %}
void operator()({{t_in}} in, {{t_out}} & out)
{
#pragma HLS INLINE
    out.key = in.key;
    out.value = in.value;
}
{%- endmacro %}

{%- macro filter_functor(t_in, t_out) %}
void operator()({{t_in}} in, {{t_out}} & out, bool & keep)
{
#pragma HLS INLINE
    out.key = in.key;
    out.value = in.value;
    keep = true;
}
{%- endmacro %}

{%- macro flatmap_functor(t_in, t_out) %}
void operator()({{t_in}} in, FlatMapShipper<{{t_out}}> & shipper)
{
#pragma HLS INLINE
    {{t_out}} out;
    out.key = in.key;
    out.value = in.value;
    shipper.send(out);
    // shipper.send_eos();
}
{%- endmacro %}

{%- macro op_functor(op) %}
{% if op.is_map() %}
{{ map_functor(op.i_datatype, op.o_datatype) }}
{% elif op.is_filter() %}
{{ filter_functor(op.i_datatype, op.o_datatype) }}
{% elif op.is_flat_map() %}
{{ flatmap_functor(op.i_datatype, op.o_datatype) }}
{% endif %}
{%- endmacro %}

#include "common/constants.hpp"
{{ include_tuples(op) }}

struct {{ op.name }}
{
    // Private members
    {% for b in op.get_private_buffers() %}
    {{ b.datatype }} {{ b.name }};
    {% endfor %}
    {% for b in op.get_local_buffers() %}
    {{ b.datatype }} {{ b.name }};
    {% endfor %}

    // Constructor
    {{ op.name }}() = default;

    {{op_functor(op) | indent(4)}}
};