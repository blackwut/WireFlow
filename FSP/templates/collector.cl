{% import 'channel.cl' as ch with context %}

{% macro node(node, idx) -%}

{# TODO: aggiungere nei parametri i buffers! #}
CL_SINGLE_TASK {{node.kernel_name(idx)}}()
{

    uint n = 0;
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
    {{ node.call_begin_function('data, size') }};
    {% endif %}

    while (!done) {
        {{ node.declare_i_tuple('t_in') }};
        {{ ch.gather_tuple(node, idx, 'r', 't_in', 't_out', process_tuple) | indent(8) }}
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function('data, size') }};
    {% endif %}
}

{%- endmacro %}


{% macro process_tuple(node, idx, t_in, t_out) -%}
{% if node.has_begin_function() %}
{{ node.call_function(t_in + '.data') }};
{% endif %}
n++;
{%- endmacro %}


{% macro declare_begin_function(node) -%}
inline void {{ node.begin_function_name() }}()
{
    // begin function computation
}
{%- endmacro %}

{% macro declare_function(node) -%}
{% set args = [node.i_datatype + ' in']%}
{% set args = args + node.parameter_buffers_list() %}
inline void {{ node.function_name() }}({{ args | join(', ') }})
{
    // apply your collector function to 'in'
}
#}
{%- endmacro %}

{% macro declare_end_function(node) -%}
inline void {{ node.end_function_name() }}()
{
    // end function computation
}
{%- endmacro %}

{% macro declare_functions(node) -%}
{{ declare_begin_function(node) if node.has_begin_function() }}

{{ declare_function(node) if node.has_compute_function() }}

{{ declare_end_function(node) if node.has_end_function() }}
{%- endmacro %}
