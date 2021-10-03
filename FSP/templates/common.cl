#pragma OPENCL EXTENSION cl_intel_channels : enable

#define PRIMITIVE_CAT(a, b)     a ## b
#define CAT(a, b)               PRIMITIVE_CAT(a, b)

#define CL_AUTORUN                      \
__attribute__((max_global_work_dim(0))) \
__attribute__((autorun))                \
__kernel void

#define CL_SINGLE_TASK                      \
__attribute__((uses_global_work_offset(0))) \
__attribute__((max_global_work_dim(0)))     \
__kernel void
