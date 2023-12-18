inline void predictor_begin(const unsigned int num_states,
                            const __global FLOAT_T * restrict trans_prob,
                            __local predictor_t state[STATE_SIZE])
{
    for (uint i = 0; i < STATE_SIZE; ++i) {
        state[i].win_elems = (win_size_t)0;
    }
}

#if FLOAT_T == float
inline FLOAT_T get_local_metric(const win_state_t win[5],
                               const __global FLOAT_T * restrict trans_prob,
                               const uint num_states)
{
    FLOAT_T param0[WIN_DIM - 1];
    const FLOAT_T param1 = WIN_DIM - 1;

    #pragma unroll
    for (uint i = 0; i < (WIN_DIM - 1); ++i) {
        param0[i] = 0;
        const uint prev = win[i];
        const uint curr = win[i + 1];
        for (uint j = 0; j < num_states; ++j) {
            param0[i] += (j != curr ? trans_prob[prev * num_states + j] : 0);
        }
    }

    FLOAT_T param0_sum = 0;
    #pragma unroll
    for (uint i = 0; i < (WIN_DIM - 1); ++i) {
        param0_sum += param0[i];
    }

    return param0_sum / param1;
}
#else
inline FLOAT_T get_local_metric(const win_state_t win[5],
                               const __global FLOAT_T * restrict trans_prob,
                               const uint num_states)
{
    #define II_CYCLES   12
    FLOAT_T shift_reg[II_CYCLES + 1];
    #pragma unroll
    for (uint i = 0; i < II_CYCLES + 1; ++i) {
        shift_reg[i] = 0;
    }

    uint i = 0;
    uint j = 0;
    while (i < (WIN_DIM - 1)) {
        uint prev = win[i];
        uint curr = win[i + 1];
        const FLOAT_T prob = trans_prob[prev * num_states + j];

        shift_reg[II_CYCLES] = shift_reg[0] + (j != curr ? prob : 0);
        #pragma unroll
        for (uint s = 0; s < II_CYCLES; ++s) {
            shift_reg[s] = shift_reg[s + 1];
        }

        j++;
        if (j == num_states) {
            j = 0;
            i++;
        }
    }

    FLOAT_T param0 = 0;
    const FLOAT_T param1 = WIN_DIM - 1;

    #pragma unroll
    for (uint i = 0; i < II_CYCLES; ++i) {
        param0 += shift_reg[i];
    }

    return param0 / param1;
}
#endif

inline tuple_t predictor_function(input_t in, 
                                  const uint num_states,
                                  const __global FLOAT_T * restrict trans_prob,
                                  __local predictor_t * restrict state)
{
    const uint idx = in.key / __PREDICTOR_PAR;

    // load state
    predictor_t p = state[idx];
    
    // push_back(state_id)
    #pragma unroll
    for (int i = 0; i < (WIN_DIM - 1); ++i) {
        p.win[i] = p.win[i + 1];
    }
    p.win[WIN_DIM - 1] = (win_state_t)in.state_id;

    // increment win_elems up to WIN_DIM
    if (p.win_elems < WIN_DIM) {
        p.win_elems++;
    }

    // update state
    state[idx] = p;

    // calculate score if there are WIN_DIM state_id (transactions)
    FLOAT_T score = 0;
    if (p.win_elems == WIN_DIM) {
        score = get_local_metric(p.win, trans_prob, num_states);
    }

    // prepare output tuple
    tuple_t out;
    out.key = in.key;
    out.state_id = in.state_id;
    out.score = score;

#ifdef MEASURE_LATENCY
    out.timestamp = in.timestamp;
#endif

    return out;
}
