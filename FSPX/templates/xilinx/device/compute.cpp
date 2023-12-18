{%- macro include_operator(op) -%}
#include "nodes/{{ op.name }}.hpp"
{%- endmacro %}

{%- macro include_operators(operators) -%}
{{ include_operator(generator) if generator else "" }}
{% for op in operators %}
{{ include_operator(op) }}
{% endfor %}
{{ include_operator(drainer) if drainer else ""}}
{%- endmacro %}


{%- macro compute_params(mr, mw, gn, dr) %}
{%- if gn %}
uint64_t size, unsigned int max_key{{", " if mw else ""}}
{%- endif %}
{%- if mr %}
mr_axis_stream_t in[{{mr.get_par_macro()}}]{{", " if mw else ""}}
{%- endif %}
{%- if mw %}
mw_axis_stream_t out[{{mw.get_par_macro()}}]
{%- endif %}
{%- endmacro %}


{%- macro a2a_emitter(left, right) %}
fx::A2A::Emitter<fx::A2A::Policy_t::{{left.get_dispatch_name()}}, {{left.get_par_macro()}}, {{right.get_par_macro()}}>(
    in, {{left.name}}_{{right.name}}{{", " + left.get_keyby_lambda_name() if left.is_dispatch_KB() else ""}}
);
{%- endmacro %}

{%- macro a2a_operator(left, mid, right) %}
fx::A2A::Operator<fx::A2A::Operator_t::{{mid.get_type_name() | upper}}, {{mid.name}}, fx::A2A::Policy_t::{{mid.get_gather_name()}}, fx::A2A::Policy_t::{{mid.get_dispatch_name()}}, {{left.get_par_macro()}}, {{mid.get_par_macro()}}, {{right.get_par_macro()}}>(
    {{left.name}}_{{mid.name}}, {{mid.name}}_{{right.name}}{{", " + mid.get_keyby_lambda_name() if mid.is_dispatch_KB() else ""}}
);
{%- endmacro %}

{%- macro a2a_collector(left, right) %}
fx::A2A::Collector<fx::A2A::Policy_t::{{right.get_gather_name()}}, {{left.get_par_macro()}}, {{right.get_par_macro()}}>(
    {{left.name}}_{{right.name}}, out
);
{%- endmacro %}

#include "common/constants.hpp"
#include "includes/defines.hpp"
#include "includes/keyby_lambdas.hpp"

{{include_operators(operators)}}

extern "C" {
void compute(
    {{compute_params(mr, mw, generator, drainer)}}
)
{
    {% if not generator or not drainer %}
    #pragma HLS interface ap_ctrl_none port=return
    {% endif %}

    #pragma HLS DATAFLOW

    {% if generator %}
    stream_{{generator.o_datatype}} in[{{generator.get_par_macro()}}];
    {% endif %}
    {% for op in nodes %}
    {% if not loop.last %}
    {% set next_op = nodes[loop.index] %}
    stream_{{op.o_datatype}} {{op.name}}_{{next_op.name}}[{{op.get_par_macro()}}][{{next_op.get_par_macro()}}];
    {% endif %}
    {% endfor %}
    {% if drainer %}
    stream_{{drainer.i_datatype}} out[{{drainer.get_par_macro()}}];
    {% endif %}

    {% if generator %}
    fx::A2A::ReplicateGenerator<uint64_t, {{generator.name}}, {{generator.get_par_macro()}}>(in, size, max_key);
    {{a2a_emitter(generator, operators[0]) | indent(4)}}
    {% else %}
    {{a2a_emitter(mr, operators[0]) | indent(4)}}
    {% endif %}

    {% for op in operators %}
    {% set in_node = mr if mr else generator %}
    {% set out_node = mw if mw else drainer %}
    {% if loop.first %}
    {{a2a_operator(in_node, op, nodes[loop.index + 1]) | indent(4)}}
    {% elif loop.last %}
    {{a2a_operator(nodes[loop.index - 1], op, out_node) | indent(4)}}
    {% else %}
    {{a2a_operator(nodes[loop.index - 1], op, nodes[loop.index + 1]) | indent(4)}}
    {% endif %}
    {% endfor %}

    {% if drainer %}
    {{a2a_collector(operators[-1], drainer) | indent(4)}}
    fx::A2A::ReplicateDrainer<uint64_t, {{drainer.name}}, {{drainer.get_par_macro()}}>(out);
    {% else %}
    {{a2a_collector(operators[-1], mw) | indent(4)}}
    {% endif %}
}
}
