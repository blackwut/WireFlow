#define CL_AUTORUN                      \
__attribute__((max_global_work_dim(0))) \
__attribute__((autorun))                \
__kernel void

#define CL_SINGLE_TASK                      \
__attribute__((uses_global_work_offset(0))) \
__attribute__((max_global_work_dim(0)))     \
__kernel void

typedef struct __attribute__((aligned(4))) {
    unsigned int received;
    bool EOS[{{ sink.i_degree }}];
} _sink_context_t;


typedef union {
    unsigned int i;
    float f;
} rng_state_t;

inline unsigned int next_int(rng_state_t * s)
{
    s->i = (s->i >> 1) | (((s->i >> 0) ^ (s->i >> 12) ^ (s->i >> 6) ^ (s->i >> 7)) << 31);
    return s->i;
}

// Generates a float random number [1.0, 2.0[
inline float next_float(rng_state_t * s)
{
    rng_state_t _s;
    s->i = (s->i >> 1) | (((s->i >> 0) ^ (s->i >> 12) ^ (s->i >> 6) ^ (s->i >> 7)) << 31);
    _s.i = ((s->i & 0x007fffff) | 0x3f800000);
    return _s.f;
}

#if defined(INTELFPGA_CL)
typedef uint header_t;
typedef uint header_elems_t;
#else
typedef cl_uint header_t;
typedef cl_uint header_elems_t;
#endif

inline header_t header_new(const bool close, const bool ready, const header_elems_t size)
{
    header_t v = 0;
    v = (close << 31) | (ready << 30) | (size & 0x3FFFFFFF);
    return v;
}

inline bool header_close(const header_t h)
{
    return (h >> 31);
}

inline bool header_ready(const header_t h)
{
    return (h >> 30) & 1;
}

inline header_elems_t header_size(const header_t h)
{
    return (h & 0x3FFFFFFF);
}