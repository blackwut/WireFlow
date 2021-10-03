#ifndef __TUPLES_H
#define __TUPLES_H

typedef struct {
    uint device_id;
    float temperature;
    float average;
} tuple_t;

inline uint tuple_t_getKey(tuple_t data) {
    return data.device_id;
}

#endif //__TUPLES_H