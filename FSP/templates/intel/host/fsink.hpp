{%- set sink_name = sink.name -%}
#pragma once

#include <vector>
#include <queue>
#include <string>

// TODO: rename 'sink' with "MemoryWriter"

#include "../../ocl/ocl.hpp"
#include "../../ocl/fbuffers.hpp"
#include "../../ocl/utils.hpp"
#include "../../device/includes/fsp.cl"
#include "../../common/constants.h"
#include "../../common/tuples.h"

template <typename T>
struct FSink
{
    virtual void pop_empty(const size_t rid,
                           const size_t batch_size,
                           size_t * received,
                           bool * last) {
        (void)rid;
        (void)batch_size;
        (void)received;
        (void)last;
    };
    virtual T * pop(const size_t rid,
                    const size_t batch_size,
                    size_t * received,
                    bool * last) = 0;
    virtual void put_batch(const size_t rid,
                           T * batch) = 0;
    virtual void launch_kernels() = 0;
    virtual void finish() = 0;
    virtual void clean() = 0;
};

#if 1

// template <typename T>
// struct FSinkTask
// {
//     size_t rid;
//     size_t data_size;
//     T * data;
//     unsigned int * received;

//     cl_kernel kernel;
//     cl_mem b_data;
//     cl_mem b_received;
//     cl_mem b_context;

//     FSinkTask(size_t rid, size_t data_size, cl_mem b_context)
//     , rid(rid)
//     , data_size(data_size)
//     , data(f_alloc<T>(data_size))
//     , received(f_alloc<unsigned int>(1))
//     {


//     }
// }


template <typename T>
struct FSinkCopy : FSink<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t number_of_buffers;
    size_t previous_node_par;

    std::vector<size_t> iterations;

    std::vector< std::vector<cl_kernel> > kernels;
    std::vector<cl_command_queue> kernels_queues;

    std::vector< std::vector<cl_mem> > buffers;
    std::vector<cl_command_queue> buffers_queues;
    std::vector< std::vector<cl_event> > buffers_events;

    std::vector< std::vector<cl_mem> > received;
    std::vector<cl_command_queue> received_queues;
    std::vector< std::vector<cl_event> > received_events;

    std::vector<cl_mem> contexts;
    std::vector<cl_command_queue> contexts_queues;

    std::vector< std::queue<T *> > batches_waiting_queue;
    std::vector< std::queue<unsigned int *> > received_waiting_queue;

    std::vector< std::queue<T *> > batches_read_queue;
    std::vector< std::queue<unsigned int *> > received_read_queue;


    std::vector<size_t> number_of_launches;
    std::vector<size_t> number_of_pop;

    FSinkCopy(OCL & ocl,
             const size_t par,
             const size_t batch_size,
             const size_t N,
             const size_t previous_node_par)
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(N)
    , previous_node_par(previous_node_par)
    , iterations(par, 0)
    , kernels(par, std::vector<cl_kernel>(number_of_buffers))
    , kernels_queues(par)
    , buffers(par, std::vector<cl_mem>(number_of_buffers))
    , buffers_queues(par)
    , buffers_events(par, std::vector<cl_event>(number_of_buffers))
    , received(par, std::vector<cl_mem>(number_of_buffers))
    , received_queues(par)
    , received_events(par, std::vector<cl_event>(number_of_buffers))
    , contexts(par)
    , contexts_queues(par)
    , batches_waiting_queue(par, std::queue<T *>())
    , received_waiting_queue(par, std::queue<unsigned int *>())
    , batches_read_queue(par, std::queue<T *>())
    , received_read_queue(par, std::queue<unsigned int *>())
    , number_of_launches(par, 0)
    , number_of_pop(par, 0)
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSinkCopy: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        mw_context_t empty_context;
        empty_context.received = 0;
        for (size_t i = 0; i < previous_node_par; ++i) {
            empty_context.EOS[i] = false;
        }

        cl_int status;
        for (size_t rid = 0; rid < par; ++rid) {
            kernels_queues[rid] = ocl.createCommandQueue();
            buffers_queues[rid] = ocl.createCommandQueue();
            received_queues[rid] = ocl.createCommandQueue();
            contexts_queues[rid] = ocl.createCommandQueue();

            for (size_t n = 0; n < number_of_buffers; ++n) {
                kernels[rid][n] = ocl.createKernel("sink_" + std::to_string(rid));

                buffers[rid][n] = clCreateBuffer(ocl.context,
                                                 CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
                                                 max_batch_size * sizeof(T),
                                                 NULL, &status);
                clCheckErrorMsg(status, "Failed to create clBuffer (buffer_mem)");

                received[rid][n] = clCreateBuffer(ocl.context,
                                                  CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
                                                  sizeof(unsigned int),
                                                  NULL, &status);
                clCheckErrorMsg(status, "Failed to create clBuffer (received_mem)");

                contexts[rid] = clCreateBuffer(ocl.context,
                                               CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE,
                                               sizeof(mw_context_t),
                                               &empty_context, &status);
                clCheckErrorMsg(status, "Failed to create clBuffer (context_mem)");

                batches_waiting_queue[rid].push(f_alloc<T>(max_batch_size));
                received_waiting_queue[rid].push(f_alloc<unsigned int>(1));
            }
        }

        for (size_t rid = 0; rid < par; ++rid) {
            for (size_t i = 0; i < number_of_buffers; ++i) {
                _launch_kernel(rid, false);
            }
        }
    }

    void _launch_kernel(const size_t rid, bool is_flush = true)
    {
        number_of_launches[rid]++;
        const cl_uint _batch_size = static_cast<cl_uint>(max_batch_size);

        const size_t it = iterations[rid];
        const size_t idx = it % number_of_buffers;

        cl_event kernel_event;

        cl_uint argi = 0;
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(buffers[rid][idx]),  &buffers[rid][idx]));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(_batch_size),   &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(contexts[rid]), &contexts[rid]));
        clCheckError(clSetKernelArg(kernels[rid][idx], argi++, sizeof(received[rid][idx]), &received[rid][idx]));
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid][idx], 0, NULL, &kernel_event));
        if (is_flush) clFlush(kernels_queues[rid]);

        unsigned int * received_ = received_waiting_queue[rid].front();
        received_waiting_queue[rid].pop();
        clCheckError(clEnqueueReadBuffer(received_queues[rid],
                                         received[rid][idx],
                                         CL_FALSE, 0,
                                         sizeof(unsigned int), received_,
                                         1, &kernel_event, &received_events[rid][idx]));
        if (is_flush) clFlush(received_queues[rid]);
        received_read_queue[rid].push(received_);

        T * batch = batches_waiting_queue[rid].front();
        batches_waiting_queue[rid].pop();
        clCheckError(clEnqueueReadBuffer(buffers_queues[rid],
                                         buffers[rid][idx],
                                         CL_FALSE, 0,
                                         sizeof(T) * max_batch_size, batch,
                                         1, &kernel_event, &buffers_events[rid][idx]));
        if (is_flush) clFlush(buffers_queues[rid]);
        clCheckError(clReleaseEvent(kernel_event));
        batches_read_queue[rid].push(batch);

        iterations[rid]++;
    }

    T * pop(const size_t rid,
            const size_t batch_size,
            size_t * received,
            bool * last)
    {
        const size_t idx = number_of_pop[rid] % number_of_buffers;
        clCheckError(clWaitForEvents(1, &received_events[rid][idx]));
        clCheckError(clReleaseEvent(received_events[rid][idx]));
        received_events[rid][idx] = NULL;

        unsigned int * received_ = received_read_queue[rid].front();
        received_read_queue[rid].pop();

        *received = *received_;
        *last = ((*received_) == 0);

        received_waiting_queue[rid].push(received_);

        clCheckError(clWaitForEvents(1, &buffers_events[rid][idx]));
        clCheckError(clReleaseEvent(buffers_events[rid][idx]));
        buffers_events[rid][idx] = NULL;

        T * batch = batches_read_queue[rid].front();
        batches_read_queue[rid].pop();

        number_of_pop[rid]++;
        return batch;
    }

    void put_batch(const size_t rid,
                   T * batch)
    {
        batches_waiting_queue[rid].push(batch);
        _launch_kernel(rid);
    }

    void launch_kernels() {}

    void finish()
    {
        for (size_t rid = 0; rid < par; ++rid) {
            clFinish(buffers_queues[rid]);
            clFinish(contexts_queues[rid]);
            clFinish(kernels_queues[rid]);
        }
    }

    void clean()
    {
        finish();

        for (size_t rid = 0; rid < par; ++rid) {

            while (!received_read_queue[rid].empty()) {
                unsigned int * b = received_read_queue[rid].front();
                received_read_queue[rid].pop();
                free(b);
            }

            while (!batches_read_queue[rid].empty()) {
                T * b = batches_read_queue[rid].front();
                batches_read_queue[rid].pop();
                free(b);
            }

            while (!received_waiting_queue[rid].empty()) {
                unsigned int * b = received_waiting_queue[rid].front();
                received_waiting_queue[rid].pop();
                free(b);
            }

            while (!batches_waiting_queue[rid].empty()) {
                T * b = batches_waiting_queue[rid].front();
                batches_waiting_queue[rid].pop();
                free(b);
            }

            if (contexts[rid]) clCheckError(clReleaseMemObject(contexts[rid]));

            for (size_t n = 0; n < number_of_buffers; ++n) {
                if (buffers[rid][n]) clCheckError(clReleaseMemObject(buffers[rid][n]));
                if (kernels[rid][n]) clReleaseKernel(kernels[rid][n]);
            }
            if (contexts_queues[rid]) clReleaseCommandQueue(contexts_queues[rid]);
            if (buffers_queues[rid]) clReleaseCommandQueue(buffers_queues[rid]);
            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);
        }
    }
};
#else
template <typename T>
struct FSinkCopy : FSink<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t previous_node_par;

    std::vector<size_t> iterations;

    std::vector<cl_kernel> kernels;
    std::vector<cl_command_queue> kernels_queues;

    std::vector<cl_mem> buffers;
    std::vector<cl_command_queue> buffers_queues;
    std::vector<cl_event> buffers_events;

    std::vector<cl_mem> contexts;
    std::vector<cl_command_queue> contexts_queues;

    std::vector< std::queue<T *> > batches_waiting_queue;


    FSinkCopy(OCL & ocl,
             const size_t par,
             const size_t batch_size,
             const size_t N,
             const size_t previous_node_par)
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , previous_node_par(previous_node_par)
    , iterations(par, 0)
    , kernels(par)
    , kernels_queues(par)
    , buffers(par)
    , buffers_queues(par)
    , buffers_events(par)
    , contexts(par)
    , contexts_queues(par)
    , batches_waiting_queue(par, std::queue<T *>())
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSinkCopy: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        mw_context_t empty_context;
        empty_context.received = 0;
        for (size_t i = 0; i < previous_node_par; ++i) {
            empty_context.EOS[i] = false;
        }

        cl_int status;
        for (size_t rid = 0; rid < par; ++rid) {
            kernels[rid] = ocl.createKernel("{{sink_name}}_" + std::to_string(rid));
            kernels_queues[rid] = ocl.createCommandQueue();

            // buffers
            buffers[rid] = clCreateBuffer(ocl.context,
                                          CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
                                          max_batch_size * sizeof(T),
                                          NULL, &status);
            clCheckErrorMsg(status, "Failed to create clBuffer (buffer_mem)");
            buffers_queues[rid] = ocl.createCommandQueue();

            // contexts
            contexts[rid] = clCreateBuffer(ocl.context,
                                           CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE,
                                           sizeof(mw_context_t),
                                           &empty_context, &status);
            clCheckErrorMsg(status, "Failed to create clBuffer (received_mem)");
            contexts_queues[rid] = ocl.createCommandQueue();

            batches_waiting_queue[rid].push(f_alloc<T>(max_batch_size));
        }
    }

    void pop_empty(const size_t rid,
                   const size_t batch_size,
                   size_t * received,
                   bool * last)
    {
        const cl_uint _batch_size = static_cast<cl_uint>(batch_size);

        cl_uint argi = 0;
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(buffers[rid]),  &buffers[rid]));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(_batch_size),   &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(contexts[rid]), &contexts[rid]));

        cl_event kernel_event;
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid], 0, NULL, &kernel_event));
        clFlush(kernels_queues[rid]);

        mw_context_t context;
        clCheckError(clEnqueueReadBuffer(contexts_queues[rid],
                                         contexts[rid],
                                         CL_TRUE, 0,
                                         sizeof(mw_context_t), &context,
                                         1, &kernel_event, NULL));
        clFlush(contexts_queues[rid]);
        clCheckError(clReleaseEvent(kernel_event));

        // received
        *received = context.received;

        // last
        bool _last = true;
        for (uint i = 0; i < previous_node_par; ++i) {
            _last &= context.EOS[i];
        }
        *last = _last;

        iterations[rid]++;
    }


    T * pop(const size_t rid,
            const size_t batch_size,
            size_t * received,
            bool * last)
    {
        const cl_uint _batch_size = static_cast<cl_uint>(batch_size);

        cl_uint argi = 0;
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(buffers[rid]),  &buffers[rid]));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(_batch_size),   &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(contexts[rid]), &contexts[rid]));

        cl_event kernel_event;
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid], 0, NULL, &kernel_event));
        clFlush(kernels_queues[rid]);

        cl_event read_events[2];
        mw_context_t context;
        clCheckError(clEnqueueReadBuffer(contexts_queues[rid],
                                         contexts[rid],
                                         CL_FALSE, 0,
                                         sizeof(mw_context_t), &context,
                                         1, &kernel_event, &read_events[0]));
        clFlush(contexts_queues[rid]);

        T * batch = batches_waiting_queue[rid].front();
        batches_waiting_queue[rid].pop();
        clCheckError(clEnqueueReadBuffer(buffers_queues[rid],
                                         buffers[rid],
                                         CL_FALSE, 0,
                                         sizeof(T) * batch_size, batch,
                                         1, &kernel_event, &read_events[1]));
        clFlush(buffers_queues[rid]);
        clCheckError(clReleaseEvent(kernel_event));

        clCheckError(clWaitForEvents(2, read_events));
        clCheckError(clReleaseEvent(read_events[0]));
        clCheckError(clReleaseEvent(read_events[1]));

        // received
        *received = context.received;

        // last
        bool _last = true;
        for (uint i = 0; i < previous_node_par; ++i) {
            _last &= context.EOS[i];
        }
        *last = _last;

        iterations[rid]++;

        return batch;
    }

    void put_batch(const size_t rid,
                   T * batch)
    {
        if (batch) batches_waiting_queue[rid].push(batch);
    }

    void launch_kernels() {}

    void finish()
    {
        for (size_t rid = 0; rid < par; ++rid) {
            clFinish(buffers_queues[rid]);
            clFinish(contexts_queues[rid]);
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

            if (contexts_queues[rid]) clReleaseCommandQueue(contexts_queues[rid]);
            if (contexts[rid]) clCheckError(clReleaseMemObject(contexts[rid]));

            if (buffers_queues[rid]) clReleaseCommandQueue(buffers_queues[rid]);
            if (buffers[rid]) clCheckError(clReleaseMemObject(buffers[rid]));

            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);
            if (kernels[rid]) clReleaseKernel(kernels[rid]);
        }
    }
};
#endif


template <typename T>
struct FSinkHybrid : FSink<T>
{
    OCL & ocl;
    size_t par;

    size_t max_batch_size;
    size_t number_of_buffers;
    size_t previous_node_par;

    std::vector<size_t> iterations;

    std::vector<cl_kernel> kernels;
    std::vector<cl_command_queue> kernels_queues;

    std::vector< std::vector< clSharedBuffer<T> > > buffers;
    std::vector< clSharedBuffer<mw_context_t> > contexts;


    FSinkHybrid(OCL & ocl,
                const size_t par,
                const size_t batch_size,
                const size_t N,
                const size_t previous_node_par)
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(N)
    , previous_node_par(previous_node_par)
    , iterations(par, 0)
    , kernels(par)
    , kernels_queues(par)
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSinkHybrid: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        for (size_t rid = 0; rid < par; ++rid) {
            kernels[rid] = ocl.createKernel("{{sink_name}}_" + std::to_string(rid));
            kernels_queues[rid] = ocl.createCommandQueue();

            // buffers
            std::vector< clSharedBuffer<T> > _buffs;
            for (size_t n = 0; n < number_of_buffers; ++n) {
                _buffs.push_back(clSharedBuffer<T>(ocl,
                                                   max_batch_size,
                                                   CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
                                                   false));
                _buffs[n].map(CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION);
            }
            buffers.push_back(_buffs);

            // contexts
            contexts.push_back(clSharedBuffer<mw_context_t>(ocl, 1, 0, false));
            contexts[rid].map(CL_MAP_WRITE_INVALIDATE_REGION);
            contexts[rid].ptr()->received = 0;
            for (size_t i = 0; i < previous_node_par; ++i) {
                contexts[rid].ptr()->EOS[i] = false;
            }
        }

        WMB(); // ensures that writes on buffers are completed
    }

    T * pop(const size_t rid,
            const size_t batch_size,
            size_t * received,
            bool * last)
    {
        const cl_uint _batch_size = static_cast<cl_uint>(batch_size);
        const size_t idx = iterations[rid] % number_of_buffers;

        cl_uint argi = 0;
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(*buffers[rid][idx].mem()), buffers[rid][idx].mem()));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(_batch_size),              &_batch_size));
        clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(*contexts[rid].mem()),     contexts[rid].mem()));

        cl_event kernel_event;
        clCheckError(clEnqueueTask(kernels_queues[rid], kernels[rid], 0, NULL, &kernel_event));
        clFlush(kernels_queues[rid]);

        clCheckError(clWaitForEvents(1, &kernel_event));
        clCheckError(clReleaseEvent(kernel_event));

        mw_context_t * context = contexts[rid].ptr();
        *received = context->received;

        // last
        bool _last = true;
        for (uint i = 0; i < previous_node_par; ++i) {
            _last &= context->EOS[i];
        }
        *last = _last;

        iterations[rid]++;

        return buffers[rid][idx].ptr();
    }

    void put_batch(const size_t rid,    // unused
                   T * batch)           // unused
    {
        (void)rid;
        (void)batch;
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
            contexts[rid].release();
            for (auto & b : buffers[rid]) {
                b.release();
            }

            if (kernels_queues[rid]) clReleaseCommandQueue(kernels_queues[rid]);
            if (kernels[rid]) clReleaseKernel(kernels[rid]);
        }
    }
};


template <typename T>
struct FSinkShared : FSink<T>
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

    FSinkShared(OCL & ocl,
                const size_t par,
                const size_t batch_size,
                const size_t N)
    : ocl(ocl)
    , par(par)
    , max_batch_size(next_pow2(batch_size))
    , number_of_buffers(next_pow2(N))
    , kernels(par)
    , kernels_queues(par)
    , header_indexes(par, 0)
    {
        if (batch_size != max_batch_size) {
            std::cout << "FSinkShared: `batch_size` is rounded to the next power of 2 ("
                      << batch_size << " -> " << max_batch_size << ")" << std::endl;
        }

        if (N != number_of_buffers) {
            std::cout << "FSinkShared: `N` is rounded to the next power of 2 ("
                      << N << " -> " << number_of_buffers << ")" << std::endl;
        }

        for (size_t rid = 0; rid < par; ++rid) {
            kernels[rid] = ocl.createKernel("{{sink_name}}_" + std::to_string(rid));
            kernels_queues[rid] = ocl.createCommandQueue();

            // headers
            headers.push_back(clSharedBuffer<header_t>(ocl,
                                                       number_of_buffers,
                                                       CL_MEM_READ_WRITE,
                                                       true));
            headers[rid].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            for (size_t h = 0; h < number_of_buffers; ++h) {
                headers[rid].ptr_volatile()[h] = header_new(false, false, 0);
            }

            // buffers
            buffers.push_back(clSharedBuffer<T>(ocl,
                                                number_of_buffers * max_batch_size,
                                                CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                false));
            buffers[rid].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);
        }

        WMB(); // ensures that writes on buffers are completed
    }

    T * pop(const size_t rid,
            const size_t batch_size,    // unused
            size_t * received,
            bool * last)
    {
        (void)batch_size;
        const size_t idx = header_indexes[rid];

        volatile header_t * h_ptr = &headers[rid].ptr_volatile()[idx];

        while (!header_ready(*h_ptr)) {
            SHARED_SLEEP_FUN();
        };

        T * batch = &buffers[rid].ptr()[idx * max_batch_size];
        *received = header_size(*h_ptr);
        *last = header_close(*h_ptr);

        return batch;
    }

    void put_batch(const size_t rid,
                   T * batch)           // unused
    {
        (void)batch;
        const size_t idx = header_indexes[rid];
        LMB();
        headers[rid].ptr_volatile()[idx] = header_new(false, false, 0);
        header_indexes[rid] = (idx + 1) % number_of_buffers;
    }

    void launch_kernels()
    {
        const cl_uint header_stride_exp = static_cast<cl_uint>(log_2(number_of_buffers));
        const cl_uint data_stride_exp = static_cast<cl_uint>(log_2(max_batch_size));
        for (size_t rid = 0; rid < par; ++rid) {
            size_t argi = 0;
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(headers[rid].buffer), &headers[rid].buffer));
            clCheckError(clSetKernelArg(kernels[rid], argi++, sizeof(buffers[rid].buffer), &buffers[rid].buffer));
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
