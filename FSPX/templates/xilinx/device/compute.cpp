{%- macro include_operators(operators) -%}
{% for op in operators %}
#include "nodes/{{ op.name }}.hpp"
{% endfor %}
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
    mr_axis_stream_t in[{{mr.get_par_macro()}}],
    mw_axis_stream_t out[{{mw.get_par_macro()}}]
)
{
    #pragma HLS interface ap_ctrl_none port=return

    #pragma HLS DATAFLOW

    {% for op in nodes %}
    {% if not loop.last %}
    {% set next_op = nodes[loop.index] %}
    stream_{{op.o_datatype}} {{op.name}}_{{next_op.name}}[{{op.get_par_macro()}}][{{next_op.get_par_macro()}}];
    {% endif %}
    {% endfor %}

    {{a2a_emitter(mr, operators[0]) | indent(4)}}

    {% for op in operators %}
    {% if loop.first %}
    {{a2a_operator(mr, op, nodes[loop.index + 1]) | indent(4)}}
    {% elif loop.last %}
    {{a2a_operator(nodes[loop.index - 1], op, mw) | indent(4)}}
    {% else %}
    {{a2a_operator(nodes[loop.index - 1], op, nodes[loop.index + 1]) | indent(4)}}
    {% endif %}
    {% endfor %}

    {{a2a_collector(operators[-1], mw) | indent(4)}}
}
}
