inline void average_calculator_begin(__private int sizes[AVG_KEYS],
                                     __local float windows[AVG_KEYS][WIN_DIM])
{
    #pragma unroll
    for (int i = 0; i < AVG_KEYS; ++i) {
        sizes[i] = 0;
    }

    for (int i = 0; i < AVG_KEYS; ++i) {
        #pragma unroll
        for (int j = 0; j < WIN_DIM; ++j) {
            windows[i][j] = 0;
        }
    }
}

inline tuple_t average_calculator_function(input_t in,
                                          __private int sizes[AVG_KEYS],
                                          __local float windows[AVG_KEYS][WIN_DIM])
{
    const uint idx = in.key / __AVERAGE_CALCULATOR_PAR;
    const float val = in.property_value;

#undef STATEMENT
#define STATEMENT(i) N = 1.0 / (sizes[i] == WIN_DIM ? sizes[i] : ++sizes[i]);

    float N = 0.0;
    SWITCH_CASE(idx, 63)

    float sum = 0.0;
    #pragma unroll
    for (uint i = 0; i < WIN_DIM - 1; ++i) {
        windows[idx][i] = windows[idx][i + 1];
        sum += windows[idx][i];
    }
    windows[idx][WIN_DIM - 1] = val;
    sum += val;

    tuple_t out;
    out.key = in.key;
    out.property_value = in.property_value;
    out.incremental_average = sum * N;

#ifdef MEASURE_LATENCY
    out.timestamp = in.timestamp;
#endif
    return out;
}
