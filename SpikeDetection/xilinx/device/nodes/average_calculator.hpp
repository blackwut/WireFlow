#include "common/constants.hpp"
#include "common/tuple_t.hpp"

template <typename T, int SIZE>
struct window_t
{
    T win[SIZE];

    window_t()
    : win{}
    {
    #pragma HLS array_partition variable=win complete dim=1
    }

    T update(T val)
    {
    #pragma HLS INLINE
    T sum = 0;
    WIN_SHIFT:
        for (int i = 0; i < SIZE - 1; ++i) {
        #pragma HLS unroll
            win[i] = win[i + 1];
            sum += win[i];
        }
        win[SIZE - 1] = val;
        sum += val;
        return sum;
    }
};

struct average_calculator
{
    unsigned int sizes[MAX_KEYS / AVERAGE_CALCULATOR_PAR];
    window_t<float, WIN_DIM> windows[MAX_KEYS / AVERAGE_CALCULATOR_PAR];

    average_calculator()
    : sizes{}
    {
        #pragma HLS INLINE
        #pragma HLS array_partition variable=sizes type=complete dim=1
    }

    void operator() (tuple_t in, tuple_t & out) {
    #pragma HLS INLINE

        const unsigned int idx = in.key / AVERAGE_CALCULATOR_PAR;

        float N = float(1) / WIN_DIM;
        if (sizes[idx] < WIN_DIM) {
            N = 1.0f / ++(sizes[idx]);
        }

        const float sum = windows[idx].update(in.property_value);
        out.key = in.key;
        out.property_value = in.property_value;
        out.incremental_average = sum * N;
        out.timestamp = in.timestamp;
    }
};
