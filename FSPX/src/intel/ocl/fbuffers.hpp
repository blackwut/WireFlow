#pragma once
#include "opencl.hpp"
#include "utils.hpp"

template <typename T>
struct clSharedBuffer
{
    OCL & ocl;
    size_t size;
    cl_mem_flags buffer_flags;
    bool is_volatile;

    cl_command_queue queue;
    cl_mem buffer;
    T * _ptr;
    volatile T * _ptr_volatile;

    clSharedBuffer(OCL & ocl,
                   size_t size,
                   cl_mem_flags buffer_flags,
                   bool is_volatile = false)
    : ocl(ocl)
    , size(size)
    , buffer_flags(buffer_flags)
    , is_volatile(is_volatile)
    {
        queue = ocl.createCommandQueue();
        _ptr = nullptr;
        _ptr_volatile = nullptr;
    }

    void map(cl_map_flags flags,
             cl_event * event = NULL,
             bool blocking = true)
    {
        cl_int status;
        buffer = clCreateBuffer(ocl.context,
                                CL_MEM_ALLOC_HOST_PTR | buffer_flags,
                                size * sizeof(T),
                                NULL, &status);
        clCheckErrorMsg(status, "Failed to create clBufferShared");
        if (is_volatile) {
            _ptr_volatile = (volatile T *)clEnqueueMapBuffer(queue, buffer,
                                                            blocking, flags,
                                                            0, size * sizeof(T),
                                                            0, NULL,
                                                            event, &status);
        } else {
            _ptr = (T *)clEnqueueMapBuffer(queue, buffer,
                                           blocking, flags,
                                           0, size * sizeof(T),
                                           0, NULL,
                                           event, &status);
        }
        clCheckErrorMsg(status, "Failed to map clBufferShared");
    }

    cl_mem * mem() { return &buffer; }
    T * ptr() { return _ptr; }
    volatile T * ptr_volatile() { return _ptr_volatile; }

    void release()
    {
        if (_ptr and buffer) clCheckError(clEnqueueUnmapMemObject(queue, buffer, _ptr, 0, NULL, NULL));
        if (_ptr_volatile and buffer) clCheckError(clEnqueueUnmapMemObject(queue, buffer, (void*)_ptr_volatile, 0, NULL, NULL));
        if (buffer) clCheckError(clReleaseMemObject(buffer));
    }
};