
#include "common/constants.hpp"
#include "includes/defines.hpp"
#include "includes/keyby_lambdas.hpp"

#include "nodes/generator.hpp"
#include "nodes/predictor.hpp"
#include "nodes/drainer.hpp"

extern "C" {
void compute(
    uint64_t size, unsigned int max_key
)
{

    #pragma HLS DATAFLOW

    stream_input_t in[GENERATOR_PAR];
    stream_input_t generator_predictor[GENERATOR_PAR][PREDICTOR_PAR];
    stream_tuple_t predictor_drainer[PREDICTOR_PAR][DRAINER_PAR];
    stream_tuple_t out[DRAINER_PAR];

    fx::A2A::ReplicateGenerator<uint64_t, generator, GENERATOR_PAR>(in, size, max_key);
    fx::A2A::Emitter<fx::A2A::Policy_t::KB, GENERATOR_PAR, PREDICTOR_PAR>(
        in, generator_predictor, generator_keyby
    );

    fx::A2A::Operator<fx::A2A::Operator_t::FLATMAP, predictor, fx::A2A::Policy_t::LB, fx::A2A::Policy_t::RR, GENERATOR_PAR, PREDICTOR_PAR, DRAINER_PAR>(
        generator_predictor, predictor_drainer
    );

    fx::A2A::Collector<fx::A2A::Policy_t::LB, PREDICTOR_PAR, DRAINER_PAR>(
        predictor_drainer, out
    );
    fx::A2A::ReplicateDrainer<uint64_t, drainer, DRAINER_PAR>(out);
}
}