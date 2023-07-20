{%- macro include_operators(operators) -%}
{% for op in operators %}
#include "operators/{{ op.name }}.hpp"
{% endfor %}
{%- endmacro %}

{%- macro a2a_emitter(left, right) %}
fx::A2A::Emitter_{{left.get_dispatch_name()}}<{{left.get_par_macro()}}, {{right.get_par_macro()}}>(
    in, {{left.name}}_{{right.name}}{{", " + left.get_keyby_name() if left.is_dispatch_KEYBY() else ""}}
);
{%- endmacro %}

{%- macro a2a_operator(left, mid, right) %}
fx::A2A::{{mid.get_type_name()}}<{{mid.name}}, fx::A2A::{{mid.get_gather_name()}}, fx::A2A::{{mid.get_dispatch_name()}}, {{left.get_par_macro()}}, {{mid.get_par_macro()}}, {{right.get_par_macro()}}>(
    {{left.name}}_{{mid.name}}, {{mid.name}}_{{right.name}}{{", " + mid.get_keyby_name() if mid.is_dispatch_KEYBY() else ""}}
);
{%- endmacro %}

{%- macro a2a_collector(left, right) %}
fx::A2A::Collector_{{right.get_gather_name()}}<{{left.get_par_macro()}}, {{right.get_par_macro()}}>(
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

    // stream_t mr_ma[MR_PAR][MA_PAR];
    // stream_t ma_sd[MA_PAR][SD_PAR];
    // stream_t sd_mw[SD_PAR][MW_PAR];
    {% for op in nodes %}
    {% if not loop.last %}
    {% set next_op = nodes[loop.index] %}
    stream_{{op.o_datatype}} {{op.name}}_{{next_op.name}}[{{op.get_par_macro()}}][{{next_op.get_par_macro()}}];
    {% endif %}
    {% endfor %}


    // fx::A2A::Emitter_KB<MR_PAR, MA_PAR>(
    //     in, mr_ma,
    //     [](const record_t & r) {
    //         return (int)(r.key % MA_PAR);
    //     }
    // );
    {{a2a_emitter(mr, operators[0]) | indent(4)}}


    // A2A_Map<MovingAverage<float>, MR_PAR, MA_PAR, SD_PAR>(mr_ma, ma_sd);
    // A2A_Filter<SpikeDetector<float>, MA_PAR, SD_PAR, MW_PAR>(ma_sd, sd_mw);
    {% for op in operators %}
    {% if loop.first %}
    {{a2a_operator(mr, op, nodes[loop.index + 1]) | indent(4)}}}
    {% elif loop.last %}
    {{a2a_operator(nodes[loop.index - 1], op, mw) | indent(4)}}
    {% else %}
    {{a2a_operator(nodes[loop.index - 1], op, nodes[loop.index + 1]) | indent(4)}}
    {% endif %}
    {% endfor %}


    // fx::A2A::Collector_LB<SD_PAR, MW_PAR>(sd_mw, out);
    {{a2a_collector(operators[-1], mw) | indent(4)}}
}
}
