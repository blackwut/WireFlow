#ifndef __TUPLES_H
#define __TUPLES_H


#if defined(INTELFPGA_CL)
    #define UINT_T uint
#else
    #define UINT_T uint32_t
#endif

typedef struct {
    UINT_T key;
    float property_value;

#ifdef MEASURE_LATENCY
    UINT_T timestamp;
#endif
} input_t;

typedef struct {
    UINT_T key;
    float property_value;
    float incremental_average;
#ifdef MEASURE_LATENCY
    UINT_T timestamp;
#endif
} tuple_t;

inline UINT_T input_t_getKey(tuple_t data) {
    return data.key;
}

inline UINT_T tuple_t_getKey(tuple_t data) {
    return data.key;
}

#endif //__TUPLES_H