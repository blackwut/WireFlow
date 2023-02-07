#ifndef __TUPLES_H
#define __TUPLES_H

#include "constants.h"

#if defined(INTELFPGA_CL)
    #include "ihc_apint.h"
    #define UINT_T uint
#else
    #define UINT_T uint32_t
#endif


#if defined(INTELFPGA_CL)
#define win_state_t uint5_t
#define win_size_t  uint7_t

typedef struct {
    win_state_t win[WIN_DIM];
    win_size_t win_elems;
} predictor_t;
#endif

typedef struct {
    UINT_T key;
    UINT_T state_id;
#ifdef MEASURE_LATENCY
    UINT_T timestamp;
#endif
} input_t;

typedef struct {
    UINT_T key;
    UINT_T state_id;
    FLOAT_T score;
#ifdef MEASURE_LATENCY
    UINT_T timestamp;
#endif
} tuple_t;

inline UINT_T input_t_getKey(input_t data) {
    return data.key;
}

inline UINT_T tuple_t_getKey(tuple_t data) {
    return data.key;
}

#endif //__TUPLES_H