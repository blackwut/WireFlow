
#include "common/constants.hpp"
#include "includes/defines.hpp"
#include "includes/keyby_lambdas.hpp"

#include "nodes/average_calculator.hpp"
#include "nodes/spike_detector.hpp"


extern "C" {
void compute(
    mr_axis_stream_t in[MR_PAR],
    mw_axis_stream_t out[MW_PAR]
)
{
    #pragma HLS interface ap_ctrl_none port=return

    #pragma HLS DATAFLOW

    stream_input_t mr_average_calculator[MR_PAR][AVERAGE_CALCULATOR_PAR];
    stream_tuple_t average_calculator_spike_detector[AVERAGE_CALCULATOR_PAR][SPIKE_DETECTOR_PAR];
    stream_tuple_t spike_detector_mw[SPIKE_DETECTOR_PAR][MW_PAR];

    fx::A2A::Emitter<fx::A2A::Policy_t::KB, MR_PAR, AVERAGE_CALCULATOR_PAR>(
        in, mr_average_calculator, mr_keyby
    );

    fx::A2A::Operator<fx::A2A::Operator_t::MAP, average_calculator, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::RR, MR_PAR, AVERAGE_CALCULATOR_PAR, SPIKE_DETECTOR_PAR>(
        mr_average_calculator, average_calculator_spike_detector
    );
    fx::A2A::Operator<fx::A2A::Operator_t::FILTER, spike_detector, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::LB, AVERAGE_CALCULATOR_PAR, SPIKE_DETECTOR_PAR, MW_PAR>(
        average_calculator_spike_detector, spike_detector_mw
    );

    fx::A2A::Collector<fx::A2A::Policy_t::LB, SPIKE_DETECTOR_PAR, MW_PAR>(
        spike_detector_mw, out
    );
}
}