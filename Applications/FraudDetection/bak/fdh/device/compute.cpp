
#include "common/constants.hpp"
#include "includes/defines.hpp"
#include "includes/keyby_lambdas.hpp"

#include "nodes/predictor.hpp"


extern "C" {
void compute(
    mr_axis_stream_t in[MR_PAR],
    mw_axis_stream_t out[MW_PAR]
)
{
    #pragma HLS interface ap_ctrl_none port=return

    #pragma HLS DATAFLOW

    stream_input_t mr_predictor[MR_PAR][PREDICTOR_PAR];
    stream_tuple_t predictor_mw[PREDICTOR_PAR][MW_PAR];

    fx::A2A::Emitter<fx::A2A::Policy_t::KB, MR_PAR, PREDICTOR_PAR>(
        in, mr_predictor, mr_keyby
    );

    fx::A2A::Operator<fx::A2A::Operator_t::FLATMAP, predictor, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::RR, MR_PAR, PREDICTOR_PAR, MW_PAR>(
        mr_predictor, predictor_mw
    );

    fx::A2A::Collector<fx::A2A::Policy_t::LB, PREDICTOR_PAR, MW_PAR>(
        predictor_mw, out
    );
}
}