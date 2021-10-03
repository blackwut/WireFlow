inline void average_calculator_begin(__private int sizes[AVG_KEYS],
                                     __local float windows[AVG_KEYS][WIN_DIM])
{
    #pragma unroll
    for (int i = 0; i < AVG_KEYS; ++i) {
        sizes[i] = 0;
    }
}

inline tuple_t average_calculator_function(tuple_t in,
                                          __private int sizes[AVG_KEYS],
                                          __local float windows[AVG_KEYS][WIN_DIM])
{
    const uint idx = in.device_id / __AVERAGE_CALCULATOR_PAR;
    const float val = in.temperature;

    float N = 0.0f;
    #pragma unroll
    for (uint i = 0; i < WIN_DIM; ++i) {
        if (sizes[idx] == ((1 << i) >> 1)) N = 1.0f / (i + 1);
    }

    if (sizes[idx] & (1 << (WIN_DIM - 2))) {
        sizes[idx] = (1 << (WIN_DIM - 2));
    } else {
        sizes[idx] = (sizes[idx] == 0 ? 1 : sizes[idx] << 1);
    }
    float sum = 0.0f;
    #pragma unroll
    for (uint i = 0; i < WIN_DIM - 1; ++i) {
        windows[idx][i] = windows[idx][i + 1];
        sum += windows[idx][i];
    }
    windows[idx][WIN_DIM - 1] = val;
    sum += val;

    tuple_t out;
    out.device_id = in.device_id;
    out.temperature = in.temperature;
    out.average = sum * N;
    return out;
}