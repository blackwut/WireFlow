{% import 'channel.cl' as ch with context %}
{%- set shared_memory = true if transfer_mode == transferMode.SHARED else false %}

{% macro node(node, idx) -%}
{% if shared_memory %}
CL_SINGLE_TASK {{ node.kernel_name(idx) }}(__global volatile header_t * restrict headers,
                        __global const volatile {{ node.i_datatype }}  * restrict data,
                        const uint header_stride_exp,
                        const uint data_stride_exp)
{
    uint r_idx = 0;
    uint w_idx = 0;
    bool done = false;

    {% if node.is_dispatch_RR()and node.o_degree > 1 %}
    uint w = {{ idx % node.o_degree }};
    {% endif %}

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

        header_t h;
        do { h = headers[r_idx]; } while (!header_ready(h));
        w_idx = r_idx;
        r_idx = (r_idx + 1) % (1 << header_stride_exp);


        done = header_close(h);

        const uint n = header_size(h);
        for (uint i = 0; i < n; ++i) {
            const {{ node.i_datatype }} d = data[(w_idx << data_stride_exp) + i];
            {{ process_tuple(node, idx, 'd', 't_out') }}
        }

        // needed to enforce dependency between (read data) --> (set ready to false)
        mem_fence(CLK_GLOBAL_MEM_FENCE | CLK_CHANNEL_MEM_FENCE);
        headers[w_idx] = header_new(false, false, 0);
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function('data, size') }};
    {% endif %}

    {{ ch.write_broadcast_EOS(node, idx) | indent(8) }}
}

{% else %}
{# TODO: aggiungere nei parametri i buffers! #}
CL_SINGLE_TASK {{ node.kernel_name(idx) }}(__global const {{ node.i_datatype }} * restrict data,
{% filter indent(node.kernel_name(idx)|length + 16, true) %}
const uint size,
const uint shutdown)
{% endfilter %}
{
    {% if node.is_dispatch_RR()and node.o_degree > 1 %}
    uint w = {{ idx % node.o_degree }};
    {% endif %}

    {% if node.get_private_buffers() | count > 0 %}
    {{ node.declare_private_buffers() | indent(4, true) }}
    {% endif %}
    {% if node.get_local_buffers() | count > 0 %}
    {{ node.declare_local_buffers() | indent(4, true) }}
    {% endif %}

    {% if node.has_begin_function() %}
    {{ node.call_begin_function('data, size') }};
    {% endif %}

    for (uint n = 0; n < size; ++n) {
        {% if node.has_compute_function() %}
        {{ process_tuple(node, idx, 'data[n]', 't_out') }}
        {% else %}
        {{ node.create_o_tuple('t_out', 'data[n]') | indent(8) }};
        {{ ch.dispatch_tuple(node, idx, 'w', 't_out', true) | indent(8) }}
        {% endif %}
    }

    {% if node.has_end_function() %}
    {{ node.call_end_function('data, size') }};
    {% endif %}

    if (shutdown == 1) {
        {{ ch.write_broadcast_EOS(node, idx) | indent(8) }}
    }
}
{% endif %}
{%- endmacro %}


{% macro process_tuple(node, idx, t_in, t_out) -%}
{{ node.create_o_tuple(t_out, node.call_function(t_in)) | indent(8) }};
{{ ch.dispatch_tuple(node, idx, 'w', t_out, true) | indent(8) }}
{%- endmacro %}


{% macro declare_begin_function(node) -%}
inline void {{ node.begin_function_name() }}(__global const {{ node.i_datatype }} * restrict data, const uint size)
{
    // begin function computation
}
{%- endmacro %}

{# TODO: aggiungere nei parametri i buffers! #}
{% macro declare_function(node) -%}
inline void {{ node.function_name() }}(__global const {{ node.i_datatype }} * restrict data, const uint size)
{
    // end function computation
}
{%- endmacro %}


{% macro declare_function(node) -%}
{% set args = [node.i_datatype + ' in']%}
{% set args = args + node.parameter_buffers_list() %}
inline {{ node.o_datatype }} {{ node.function_name() }}({{ args | join(', ') }})
{
    // apply your source function to 'in' and store result to 'out'
    {{ node.o_datatype }} out;
    return out;
}
{%- endmacro %}


{% macro declare_end_function(node) -%}
inline void {{ node.end_function_name() }}(__global const {{ node.i_datatype }} * restrict data, const uint size)
{
    // end function computation
}
{%- endmacro %}

{% macro declare_functions(node) -%}
{{ declare_begin_function(node) if node.has_begin_function() }}

{{ declare_function(node) if node.has_compute_function()}}

{{ declare_end_function(node) if node.has_end_function() }}
{%- endmacro %}
