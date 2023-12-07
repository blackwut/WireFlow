{% import 'channel.cl' as ch with context %}

{% macro node(node, idx) -%}

CL_SINGLE_TASK {{ node.kernel_name(idx) }}(const uint size{{ ',' if node.parameter_global_buffers_list()|length > 0 else ')'}}
{% if node.parameter_global_buffers_list()|length > 0 %}
{% filter indent(node.kernel_name(idx)|length + 16, true) %}
{% for b in node.parameter_global_buffers_list() %}
{{ b }}{{ ',\n' if not loop.last else ')'}}
{% endfor %}
{% endfilter %}
{% endif %}
{
    {% if node.is_dispatch_RR() and node.o_degree > 1 %}
    uint w = {{ idx % node.o_degree }};
    {% endif %}

    {% if node.get_private_buffers() | count > 0 %}
    {{ node.declare_private_buffers() | indent(4, true) }}
    {% endif %}
    {% if node.get_local_buffers() | count > 0 %}
    {{ node.declare_local_buffers() | indent(4, true) }}
    {% endif %}

    {% if node.has_begin_function() %}
    {{ node.call_begin_function('size') }};
    {% endif %}

    for (uint n = 0; n < size; ++n) {
        {{ process_tuple(node, idx, '', 't_out')}}
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function('size') }};
    {% endif %}

    {{ ch.write_br_EOS(node, idx) | indent(8) }}
}

{%- endmacro %}

{% macro process_tuple(node, idx, t_in, t_out) -%}
{{ node.create_o_tuple(t_out, node.call_function('size')) | indent(8) }};
{{ ch.dispatch_tuple(node, idx, 'w', t_out, true) | indent(8) }}
{%- endmacro %}


{% macro declare_begin_function(node) -%}
{% set args = ['const uint size'] %}
{% set args = args + node.parameter_buffers_list() %}
inline void {{ node.begin_function_name() }}({{ args | join(', ') }})
{
    // begin function computation
}
{%- endmacro %}

{% macro declare_function(node) -%}
inline void {{ node.function_name() }}(const uint size)
{
    // end function computation
}
{%- endmacro %}


{% macro declare_function(node) -%}
{% set args = ['const uint size'] %}
{% set args = args + node.parameter_buffers_list() %}
inline {{ node.o_datatype }} {{ node.function_name() }}({{ args | join(', ') }})
{
    // store the generated data to 'out'
    // use next_int(int_state) to generate a random unsigned int
    // use next_float(float_state) to generatoe a random float
    {{ node.o_datatype }} out;
    return out;
}
{%- endmacro %}


{% macro declare_end_function(node) -%}
{% set args = ['const uint size'] %}
{% set args = args + node.parameter_buffers_list() %}
inline void {{ node.end_function_name() }}({{ args | join(', ') }})
{
    // begin function computation
}
{%- endmacro %}


{% macro declare_functions(node) -%}
{{ declare_begin_function(node) if node.has_begin_function() }}

{{ declare_function(node) if node.has_compute_function()}}

{{ declare_end_function(node) if node.has_end_function() }}
{%- endmacro %}
