#pragma once
#include "opencl.hpp"

#include <iostream>

// #define AOCL_ALIGNMENT  64

// template <typename T>
// struct clMemory
// {
//     cl_context context;
//     cl_command_queue queue;
//     size_t size;
//     cl_mem_flags buffer_flags;

//     cl_mem buffer;
//     T * ptr;

//     clMemory(cl_context context,
//              cl_command_queue queue,
//              size_t size,
//              cl_mem_flags buffer_flags)
//     : context(context)
//     , queue(queue)
//     , size(size)
//     , buffer_flags(buffer_flags)
//     {}

//     virtual void map(cl_map_flags flags, cl_event * event = NULL, bool blocking = true) = 0;
//     virtual void read(cl_event * event = NULL, bool blocking = true) = 0;
//     virtual void write(cl_event * event = NULL, bool blocking = true) = 0;
//     virtual void release() = 0;

//     virtual ~clMemory() {};
// };

// template <typename T>
// struct clMemShared: clMemory<T>
// {
//     using super = clMemory<T>;
//     using super::context;
//     using super::queue;
//     using super::size;
//     using super::buffer_flags;
//     using super::buffer;
//     using super::ptr;

//     clMemShared(cl_context context,
//                 cl_command_queue queue,
//                 size_t size,
//                 cl_mem_flags buffer_flags)
//     : super(context, queue, size, buffer_flags)
//     {}

//     void map(cl_map_flags flags,
//              cl_event * event = NULL,
//              bool blocking = true) override
//     {
//         cl_int status;
//         buffer = clCreateBuffer(context,
//                                 CL_MEM_ALLOC_HOST_PTR | buffer_flags,
//                                 size * sizeof(T),
//                                 NULL, &status);
//         clCheckErrorMsg(status, "Failed to create clBufferShared");
//         ptr = (T *)clEnqueueMapBuffer(queue, buffer,
//                                       blocking, flags,
//                                       0, size * sizeof(T),
//                                       0, NULL,
//                                       event, &status);
//         clCheckErrorMsg(status, "Failed to map clBufferShared");
//     }

//     void read(cl_event * event = NULL, bool blocking = true) override
//     {}

//     void write(cl_event * event = NULL, bool blocking = true) override
//     {}

//     void release() override
//     {
//         if (ptr and buffer) clCheckError(clEnqueueUnmapMemObject(queue, buffer, ptr, 0, NULL, NULL));
//         if (buffer) clCheckError(clReleaseMemObject(buffer));
//     }
// };

// // TODO: Do not use this type of clMemory. Try to identify problems
// template <typename T>
// struct clMemBuffer : clMemory<T>
// {
//     using super = clMemory<T>;
//     using super::context;
//     using super::queue;
//     using super::size;
//     using super::buffer_flags;
//     using super::buffer;
//     using super::ptr;

//     clMemBuffer(cl_context context,
//                 cl_command_queue queue,
//                 size_t size,
//                 cl_mem_flags buffer_flags)
//     : clMemory<T>(context, queue, size, buffer_flags)
//     {
//         cl_int status;
//         buffer = clCreateBuffer(context, buffer_flags, size * sizeof(T), NULL, &status);
//         clCheckErrorMsg(status, "Failed to create clBuffer");

//         if (buffer_flags | CL_MEM_HOST_NO_ACCESS) {
//             std::cout << "Buffer does NOT allocate host PTR" << std::endl;
//             ptr = nullptr;
//         } else {
//             status = posix_memalign((void**)&ptr, AOCL_ALIGNMENT, size * sizeof(T));
//             if (status != 0) clCheckErrorMsg(-255, "Failed to create host buffer");
//         }
//     }

//     void map(cl_map_flags flags, cl_event * event = NULL, bool blocking = true) override
//     {}

//     void read(cl_event * event = NULL, bool blocking = true) override
//     {
//         if (ptr) {
//             clCheckError(clEnqueueReadBuffer(queue, buffer, blocking,
//                                              0, size * sizeof(T), ptr,
//                                              0, NULL, event));
//         } else {
//             abort();
//         }
//     }

//     void write(cl_event * event = NULL, bool blocking = true) override
//     {
//         if (ptr) {
//             clCheckError(clEnqueueWriteBuffer(queue, buffer, blocking,
//                                               0, size * sizeof(T), ptr,
//                                               0, NULL, event));
//         } else {
//             abort();
//         }
//     }

//     void release() override
//     {
//         if (buffer) clCheckError(clReleaseMemObject(buffer));
//         if (ptr) free(ptr);
//     }
// };


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