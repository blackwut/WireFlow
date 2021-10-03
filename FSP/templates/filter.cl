{% import 'channel.cl' as ch with context %}

{% macro node(node, idx) -%}

CL_SINGLE_TASK {{ node.kernel_name(idx) }}({{ node.parameter_global_buffers() }})
{
{% if node.is_dispatch_RR() and node.o_degree > 1 %}
    uint w = {{ idx % node.o_degree }};
{% endif %}
    bool done = false;
{% if node.i_degree > 1 %}
    uint r = {{ idx % node.i_degree }};
{% endif %}
    bool EOS[{{ node.i_degree }}];
    #pragma unroll
    for (uint i = 0; i < {{ node.i_degree }}; ++i) {
        EOS[i] = false;
    }

{% if node.get_private_buffers() | count > 0 %}
{{ node.declare_private_buffers() | indent(4, true) }}
{% endif %}
{% if node.get_local_buffers() | count > 0 %}
{{ node.declare_local_buffers() | indent(4, true) }}
{% endif %}

    {% if node.has_begin_function() %}
    {{ node.call_begin_function() }};
    {% endif %}

    while (!done) {
        {{ node.declare_i_tuple('t_in') }};
        {{ ch.gather_tuple(node, idx, 'r', 't_in', 't_out', process_tuple) | indent(8) }}
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function() }};
    {% endif %}

    {{ch.write_broadcast_EOS(node, idx)|indent(4)}}
}

{%- endmacro %}


{% macro process_tuple(node, idx, t_in, t_out) -%}
if ({{ node.call_function(t_in + '.data') }}) {
    {{ node.create_o_tuple(t_out, t_in + '.data') }};
    {{ ch.dispatch_tuple(node, idx, 'w', t_out, true) }}
}
{%- endmacro %}

{% macro declare_begin_function(node) -%}
inline void {{ node.begin_function_name() }}({{ node.parameter_buffers_list() | join(', ') }})
{
    // begin function computation
}
{%- endmacro %}

{% macro declare_function(node) -%}
{% set args = [node.i_datatype + ' in']%}
{% set args = args + node.parameter_buffers_list() %}
inline bool {{ node.function_name() }}({{ args | join(', ') }})
{
    return (in.value >= 0.0f);
}
{%- endmacro %}

{% macro declare_end_function(node) -%}
inline void {{ node.end_function_name() }}({{ node.parameter_buffers_list() | join(', ') }})
{
    // end function computation
}
{%- endmacro %}

{% macro declare_functions(node) -%}
{{ declare_begin_function(node) if node.has_begin_function()}}

{{ declare_function(node) }}

{{ declare_end_function(node) if node.has_end_function()}}
{%- endmacro %}
