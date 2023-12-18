
#include "common/constants.hpp"
#include "includes/defines.hpp"
#include "includes/keyby_lambdas.hpp"

#include "nodes/generator.hpp"
#include "nodes/average_calculator.hpp"
#include "nodes/spike_detector.hpp"
#include "nodes/drainer.hpp"

extern "C" {
void compute(
    uint64_t size, unsigned int max_key
)
{

    #pragma HLS DATAFLOW

    stream_input_t in[GENERATOR_PAR];
    stream_input_t generator_average_calculator[GENERATOR_PAR][AVERAGE_CALCULATOR_PAR];
    stream_tuple_t average_calculator_spike_detector[AVERAGE_CALCULATOR_PAR][SPIKE_DETECTOR_PAR];
    stream_tuple_t spike_detector_drainer[SPIKE_DETECTOR_PAR][DRAINER_PAR];
    stream_tuple_t out[DRAINER_PAR];

    fx::A2A::ReplicateGenerator<uint64_t, generator, GENERATOR_PAR>(in, size, max_key);
    fx::A2A::Emitter<fx::A2A::Policy_t::KB, GENERATOR_PAR, AVERAGE_CALCULATOR_PAR>(
        in, generator_average_calculator, generator_keyby
    );

    fx::A2A::Operator<fx::A2A::Operator_t::MAP, average_calculator, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::RR, GENERATOR_PAR, AVERAGE_CALCULATOR_PAR, SPIKE_DETECTOR_PAR>(
        generator_average_calculator, average_calculator_spike_detector
    );
    fx::A2A::Operator<fx::A2A::Operator_t::FILTER, spike_detector, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::LB, AVERAGE_CALCULATOR_PAR, SPIKE_DETECTOR_PAR, DRAINER_PAR>(
        average_calculator_spike_detector, spike_detector_drainer
    );

    fx::A2A::Collector<fx::A2A::Policy_t::LB, SPIKE_DETECTOR_PAR, DRAINER_PAR>(
        spike_detector_drainer, out
    );
    fx::A2A::ReplicateDrainer<uint64_t, drainer, DRAINER_PAR>(out);
}
}