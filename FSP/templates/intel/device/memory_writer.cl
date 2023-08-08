{% import 'channel.cl' as ch with context %}
{%- set shared_memory = true if transfer_mode == transferMode.SHARED else false %}

{% macro node(node, idx) -%}
{% if shared_memory %}
CL_SINGLE_TASK {{node.kernel_name(idx)}}(__global volatile header_t * restrict headers,
                      __global volatile {{ node.o_datatype }} * restrict data,
                      const uint header_stride_exp,
                      const uint data_stride_exp)
{
    uint idx = 0;
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

        while (header_ready(headers[idx])) {}

        int n = 0;
        bool read_done = false;
        while (!read_done) {
            {{ node.declare_i_tuple('t_in') }};
            {{ ch.gather_tuple(node, idx, 'r', 't_in', 't_out', process_tuple_shared) | indent(8) }}

            read_done = (n == (1 << data_stride_exp)) || done;
        }

        mem_fence(CLK_GLOBAL_MEM_FENCE);
        headers[idx] = header_new(done, true, n);
        idx = (idx + 1) % (1 << header_stride_exp);
    }
}
{% else %}
{# TODO: aggiungere nei parametri i buffers! #}
CL_SINGLE_TASK {{node.kernel_name(idx)}}(__global {{ node.o_datatype }} * restrict data,
{% filter indent(node.kernel_name(idx)|length + 16, true) %}
const uint size,
__global mw_context_t * restrict context)
{% endfilter %}
{
    uint n = 0;
    bool done = false;
    bool filled = false;
{% if node.i_degree > 1 %}
    uint r = {{ idx % node.i_degree }};
{% endif %}
    bool EOS[{{ node.i_degree }}];
    #pragma unroll
    for (uint i = 0; i < {{ node.i_degree }}; ++i) {
        EOS[i] = context->EOS[i];
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

    while (!filled && !done) {
        {{ node.declare_i_tuple('t_in') }};
        {{ ch.gather_tuple(node, idx, 'r', 't_in', 't_out', process_tuple) | indent(8) }}
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function('data, size') }};
    {% endif %}

    context->received = n;
    #pragma unroll
    for (uint i = 0; i < {{ node.i_degree }}; ++i) {
        context->EOS[i] = EOS[i];
    }
}
{% endif %}
{%- endmacro %}

{% macro process_tuple_shared(node, idx, t_in, t_out) -%}
data[(idx << data_stride_exp) + n] = {{ t_in }}.data;
n++;
{%- endmacro %}


{% macro process_tuple(node, idx, t_in, t_out) -%}
data[n] = {{ t_in }}.data;
n++;

if (n == size) {
    filled = true;
}
{%- endmacro %}


{% macro declare_begin_function(node) -%}
inline void {{ node.begin_function_name() }}(__global {{ node.o_datatype }} * restrict data, const uint size)
{
    // begin function computation
}
{%- endmacro %}

{% macro declare_function(node) -%}
{# TODO: aggiungere nei parametri i buffers! #}
{# TODO: sistemare
% set args = [node.i_datatype + ' in']%}
{% set args = args + node.parameter_buffers_list() %}
inline {{ node.o_datatype }} {{ node.function_name() }}({{ args | join(', ') }})
{
    // apply your map function to 'in' and store result to 'out'
    {{ node.o_datatype }} out;
    return out;
}
#}
{%- endmacro %}

{% macro declare_end_function(node) -%}
inline void {{ node.end_function_name() }}(__global {{ node.o_datatype }} * restrict data, const uint size)
{
    // end function computation
}
{%- endmacro %}

{% macro declare_functions(node) -%}
{{ declare_begin_function(node) if node.has_begin_function() }}

{{ declare_function(node) if node.has_compute_function() }}

{{ declare_end_function(node) if node.has_end_function() }}
{%- endmacro %}
