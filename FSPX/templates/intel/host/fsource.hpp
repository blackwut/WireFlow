{%- set source_name = source.name -%}
#pragma once

#include <vector>
#include <queue>
#include <string>
#include <chrono>

// TODO: rename 'source' with "MemoryReader"

#include "../../ocl/ocl.hpp"
#include "../../ocl/fbuffers.hpp"
#include "../../ocl/utils.hpp"
#include "../../device/includes/fsp.cl"
#include "../../common/constants.h"
#include "../../common/tuples.h"

// https://developer.arm.com/documentation/dui0802/a/A32-and-T32-Instructions/DMB--DSB--and-ISB
#if defined(__arm__) || defined(__aarch64__)
#define dmb() __asm__ __volatile__ ("dmb" : : : "memory")
#define LMB() dmb()//__asm__ __volatile__ ("dmb ld": : : "memory")
#define WMB() __asm__ __volatile__ ("dmb st": : : "memory")
#else
#define LMB() std::atomic_thread_fence(std::memory_order_acquire)
#define WMB() std::atomic_thread_fence(std::memory_order_release)
#endif


template <typename T>
struct FSource
{
    virtual T * get_batch(const size_t rid) = 0;
    virtual void push(T * batch,
                      const size_t batch_size,
                      const size_t rid,
                      const bool last = false) = 0;
    virtual void launch_kernels() = 0;
    virtual void finish() = 0;
    virtual void clean() = 0;
};

template <typename T>
struct FSourceCopy : FSource<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t number_of_buffers;

    std::vector<size_t> iterations;

    std::vector< std::vector<cl_kernel> > kernels;
    std::vector<cl_command_queue> kernels_queues;
    std::vector< std::vector<cl_event> > kernels_events;

    std::vector< std::vector<cl_mem> > buffers;
    std::vector<cl_command_queue> buffers_queues;

    std::vector< std::queue<T *> > batches_waiting_queue;

    FSourceCopy(OCL & ocl,
               const size_t par,
               const size_t batch_size,
               const size_t N)
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(N)
    , iterations(par, 0)
    , kernels(par, std::vector<cl_kernel>(number_of_buffers))
    , kernels_queues(par)
    , kernels_events(par, std::vector<cl_event>(number_of_buffers))
    , buffers(par, std::vector<cl_mem>(number_of_buffers))
    , buffers_queues(par)
    , batches_waiting_queue(par, std::queue<T *>())
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSourceCopy: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        for (size_t rid = 0; rid < par; ++rid) {
            kernels_queues[rid] = ocl.createCommandQueue();
            buffers_queues[rid] = ocl.createCommandQueue();

            for (size_t n = 0; n < number_of_buffers; ++n) {
                kernels[rid][n] = ocl.createKernel("{{source_name}}_" + std::to_string(rid));

                cl_int status;
                buffers[rid][n] = clCreateBuffer(ocl.context,
                                                 CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                 max_batch_size * sizeof(T),
                                                 NULL, &status);
                clCheckErrorMsg(status, "Failed to create clBuffer");

                batches_waiting_queue[rid].push(f_alloc<T>(max_batch_size));
            }
        }
    }

    T * get_batch(const size_t rid)
    {
        const size_t idx = iterations[rid] % number_of_buffers;
        if (iterations[rid] >= number_of_buffers) {
            clCheckError(clWaitForEvents(1, &kernels_events[rid][idx]));
            clCheckError(clReleaseEvent(kernels_events[rid][idx]));
            kernels_events[rid][idx] = NULL;
        }

        T * b = batches_waiting_queue[rid].front();
        batches_waiting_queue[rid].pop();
        return b;
    }

    void push(T * batch,
              const size_t batch_size,
              const size_t rid,
              const bool last = false)
    {
        const cl_uint _batch_size = static_cast<cl_uint>(batch_size);
        const cl_uint _last = static_cast<cl_uint>(last);
        const size_t it = iterations[rid];
        const size_t idx = it % number_of_buffers;

        cl_event buffer_event;
        clCheckError(clEnqueueWriteBuffer(buffers_queues[rid],
                                          buffers[rid][idx],
                                          CL_FALSE, 0,
                                          batch_size * sizeof(T), batch,
                                          0, NULL, &buffer_event));
        clFlush(buffers_queues[rid]);

        // recycle buffer
        batches_waiting_queue[rid].push(batch);

        size_t argi = 0;
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(buffers[rid][idx]), &buffers[rid][idx]));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(_batch_size),       &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(_last),             &_last));
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid][idx], 1, &buffer_event, &kernels_events[rid][idx]));
        clFlush(kernels_queues[rid]);
        clCheckError(clReleaseEvent(buffer_event));

        iterations[rid]++;
    }

    void launch_kernels() {}

    void finish()
    {
        for (size_t rid = 0; rid < par; ++rid) {
            clFinish(buffers_queues[rid]);
            clFinish(kernels_queues[rid]);
        }
    }

    void clean()
    {
        finish();

        for (size_t rid = 0; rid < par; ++rid) {

            while (!batches_waiting_queue[rid].empty()) {
                T * b = batches_waiting_queue[rid].front();
                batches_waiting_queue[rid].pop();
                free(b);
            }

            if (buffers_queues[rid]) clReleaseCommandQueue(buffers_queues[rid]);
            for (auto & b : buffers[rid]) {
                if (b) clCheckError(clReleaseMemObject(b));
            }

            for (auto & e : kernels_events[rid]) {
                if (e) clCheckError(clReleaseEvent(e));
            }
            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);
            for (auto & k : kernels[rid]) {
                if (k) clReleaseKernel(k);
            }
        }
    }
};

template <typename T>
struct FSourceHybrid : FSource<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t number_of_buffers;

    std::vector<size_t> iterations;

    std::vector< std::vector<cl_kernel> > kernels;
    std::vector<cl_command_queue> kernels_queues;
    std::vector< std::vector<cl_event> > kernels_events;

    std::vector< std::vector< clSharedBuffer<T> > > buffers;


    FSourceHybrid(OCL & ocl,
                  const size_t par,           // number of replicas
                  const size_t batch_size,    // max batch size
                  const size_t N)             // number of buffers used for N-Buffer technique
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(N)
    , iterations(par, 0)
    , kernels(par, std::vector<cl_kernel>(number_of_buffers))
    , kernels_queues(par)
    , kernels_events(par, std::vector<cl_event>(number_of_buffers))
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSourceHybrid: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        for (size_t rid = 0; rid < par; ++rid) {
            kernels_queues[rid] = ocl.createCommandQueue();

            std::vector< clSharedBuffer<T> > _buffs;
            for (size_t n = 0; n < number_of_buffers; ++n) {
                kernels[rid][n] = ocl.createKernel("{{source_name}}_" + std::to_string(rid));

                _buffs.push_back(clSharedBuffer<T>(ocl,
                                                   max_batch_size,
                                                   CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                   false));
                _buffs[n].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);
            }
            buffers.push_back(_buffs);
        }

        WMB(); // ensures that writes on buffers are completed
    }

    T * get_batch(const size_t rid)
    {
        const size_t idx = iterations[rid] % number_of_buffers;

        if (iterations[rid] >= number_of_buffers) {
            clCheckError(clWaitForEvents(1, &kernels_events[rid][idx]));
            clCheckError(clReleaseEvent(kernels_events[rid][idx]));
            kernels_events[rid][idx] = NULL;
        }

        return buffers[rid][idx].ptr();
    }

    void push(T * batch,                // unused
              const size_t batch_size,
              const size_t rid,
              const bool last = false)
    {
        (void)batch;

        const cl_uint _batch_size = static_cast<cl_uint>(batch_size);
        const cl_uint _last = static_cast<cl_uint>(last);
        const size_t idx = iterations[rid] % number_of_buffers;
        WMB(); // ensures that writes on buffers are completed

        cl_uint argi = 0;
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(*buffers[rid][idx].mem()), buffers[rid][idx].mem()));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(_batch_size),              &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(_last),                    &_last));
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid][idx], 0, NULL, &kernels_events[rid][idx]));
        clFlush(kernels_queues[rid]);

        iterations[rid]++;
    }

    void launch_kernels() {}

    void finish()
    {
        for (size_t rid = 0; rid < par; ++rid) {
            clFinish(kernels_queues[rid]);
        }
    }

    void clean()
    {
        finish();

        for (size_t rid = 0; rid < par; ++rid) {
            for (auto & b : buffers[rid]) {
                b.release();
            }

            for (auto & e : kernels_events[rid]) {
                if (e) clCheckError(clReleaseEvent(e));
            }

            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);

            for (auto & k : kernels[rid])
            if (k) clReleaseKernel(k);
        }
    }
};


template <typename T>
struct FSourceShared : FSource<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t number_of_buffers;

    std::vector<cl_kernel> kernels;
    std::vector<cl_command_queue> kernels_queues;

    std::vector<size_t> header_indexes;
    std::vector< clSharedBuffer<header_t> > headers;
    std::vector< clSharedBuffer<T> > buffers;

    FSourceShared(OCL & ocl,
                  const size_t par,         // number of replicas
                  const size_t batch_size,  // max batch size
                  const size_t N)           // number of buffers used for N-Buffer technique
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(next_pow2(N))
    , kernels(par)
    , kernels_queues(par)
    , header_indexes(par, 0)
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSourceShared: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        if (N != number_of_buffers) {
            std::cout << "FSourceShared: `N` is rounded to the next power of 2 ("
                      << N << " -> " << number_of_buffers << ")" << std::endl;
        }

        for (size_t rid = 0; rid < par; ++rid) {
            kernels[rid] = ocl.createKernel("{{source_name}}_" + std::to_string(rid));
            kernels_queues[rid] = ocl.createCommandQueue();

            headers.push_back(clSharedBuffer<header_t>(ocl,
                                                       number_of_buffers,
                                                       CL_MEM_READ_WRITE,
                                                       true));
            headers[rid].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            for (size_t n = 0; n < number_of_buffers; ++n) {
                headers[rid].ptr_volatile()[n] = header_new(false, false, 0);
            }

            buffers.push_back(clSharedBuffer<T>(ocl,
                                                number_of_buffers * max_batch_size,
                                                CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                false));
            buffers[rid].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);
        }

        WMB(); // ensures that writes on buffers are completed
    }

    T * get_batch(const size_t rid)
    {
        const size_t idx = header_indexes[rid];

        while (header_ready(headers[rid].ptr_volatile()[idx])) {
            // SHARED_SLEEP_FUN();
        };
        return &(buffers[rid].ptr()[idx * max_batch_size]);
    }

    void push(T * batch,                // unused
              const size_t batch_size,
              const size_t rid,
              const bool last = false)
    {
        (void)batch;
        const size_t idx = header_indexes[rid];
        WMB(); // ensures that writes on buffers are completed
        headers[rid].ptr_volatile()[idx] = header_new(last, true, batch_size);
        header_indexes[rid] = (header_indexes[rid] + 1) % number_of_buffers;
    }

    void launch_kernels()
    {
        const cl_uint header_stride_exp = static_cast<cl_uint>(log_2(number_of_buffers));
        const cl_uint data_stride_exp = static_cast<cl_uint>(log_2(max_batch_size));
        for (size_t rid = 0; rid < par; ++rid) {
            size_t argi = 0;
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(*headers[rid].mem()), headers[rid].mem()));
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(*buffers[rid].mem()), buffers[rid].mem()));
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(header_stride_exp),   &header_stride_exp));
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(data_stride_exp),     &data_stride_exp));
            clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid], 0, NULL, NULL));
        }
    }

    void finish()
    {
        for (size_t rid = 0; rid < par; ++rid) {
            clFinish(kernels_queues[rid]);
        }
    }

    void clean()
    {
        finish();

        for (auto & b : buffers) {
            b.release();
        }

        for (auto & h : headers) {
            h.release();
        }

        for (size_t rid = 0; rid < par; ++rid) {
            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);
            if (kernels[rid]) clReleaseKernel(kernels[rid]);
        }
    }
};
