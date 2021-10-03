{%- set source_data_type = source.i_datatype -%}
{%- set sink_data_type = sink.o_datatype -%}
{%- set bandwidth_data_type = source.i_datatype if source else 'float' %}
{%- set shared_memory = true if transfer_mode == transferMode.SHARED else false %}
#include <vector>
#include <string.h>

#include "ocl.hpp"
#include "utils.hpp"
#include "tuples.h"
#include "../device/includes/fsp.cl"

{% for key, value in constants.items() %}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
{% endfor %}

{% if shared_memory %}
#define USE_MEMCPY 1

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

    T * ptr() { return _ptr; }
    volatile T * ptr_volatile() { return _ptr_volatile; }

    void release()
    {
        if (_ptr and buffer) clCheckError(clEnqueueUnmapMemObject(queue, buffer, _ptr, 0, NULL, NULL));
        if (_ptr_volatile and buffer) clCheckError(clEnqueueUnmapMemObject(queue, buffer, (void*)_ptr_volatile, 0, NULL, NULL));
        if (buffer) clCheckError(clReleaseMemObject(buffer));
    }
};
{% endif %}

{% macro create_node(n) -%}
struct F{{ n.name }}
{
    OCL & ocl;
    std::string name;
    size_t par;

    std::vector<std::string> kernel_names;
    std::vector<cl_command_queue> kernel_queues;
    std::vector<cl_kernel> kernels;

    {% for b in n.get_global_no_value_buffers() %}
    // {{ b.name }} buffer
    std::vector<cl_command_queue> {{ b.get_queues_name() }};
    std::vector<cl_mem> {{ b.get_buffers_name() }};

    {% endfor %}

    F{{ n.name }}(OCL & ocl)
    : ocl(ocl)
    {
        name = "{{ n.name }}";
        par = {{ n.par }};

        for (size_t i = 0; i < par; ++i) {
            kernel_queues.push_back(ocl.createCommandQueue());
        }

        // Kernels
        for (size_t i = 0; i < par; ++i) {
            kernels.push_back(ocl.createKernel(name + "_" + std::to_string(i)));
        }


        // Create Buffers and their Queues
        {% for b in n.get_global_no_value_buffers() %}
        // {{ b.name }} buffers
        {% set nums = ('par' if b.is_access_single() else 1) %}
        for (size_t i = 0; i < {{ nums }}; ++i) {
            {{ b.get_queues_name() }}.push_back(ocl.createCommandQueue());

            cl_int status;
            cl_mem buff = clCreateBuffer(ocl.context,
                                         CL_MEM_HOST_WRITE_ONLY | {{ b.get_flags() }},
                                         {{ b.size }} * sizeof({{ b.datatype }}),
                                         NULL, &status);
            clCheckErrorMsg(status, "Failed to create {{ b.name }}");

            {{ b.get_buffers_name() }}.push_back(buff);
        }
        {% endfor %}
    }

    {% for b in n.get_global_no_value_buffers() %}

    void prepare_{{ b.name }}(const std::vector<{{ b.datatype }}> & data{{ ', size_t replica_id' if b.is_access_single() else ''}})
    {
        clCheckError(clEnqueueWriteBuffer({{ b.get_queues_name('replica_id' if b.is_access_single() else '0') }},
                                          {{ b.get_buffers_name('replica_id' if b.is_access_single() else '0') }},
                                          CL_TRUE, 0,
                                          {{ b.size }} * sizeof({{ b.datatype }}), data.data(),
                                          0, NULL, NULL));
    }
    {% endfor %}


    void launch_kernels()
    {
        // TODO: checking if all prepare() buffers are done
        {% for b in n.get_global_no_value_buffers() %}
        for (auto & q : {{ b.get_queues_name() }}) {
            clFinish(q);
        }
        {% endfor %}

        {% for b in n.get_global_value_buffers() %}
        {{ b.get_declare_and_init() }};
        {% endfor %}

        {% if n.get_global_buffers() | count > 0 %}
        for (size_t i = 0; i < par; ++i) {
            cl_uint argi = 0;
            {% for b in n.get_global_no_value_buffers() %}
            clCheckError(clSetKernelArg(kernels[i], argi++, sizeof({{ b.get_buffers_name('i' if b.is_access_single() else '0') }}), &{{ b.get_buffers_name('i' if b.is_access_single() else '0') }}));
            {% endfor %}
            {% for b in n.get_global_value_buffers() %}
            clCheckError(clSetKernelArg(kernels[i], argi++, sizeof({{ b.datatype }}), &{{ b.name }}));
            {% endfor %}
        }
        {% endif %}

        for (size_t i = 0; i < par; ++i) {
            clCheckError(clEnqueueTask(kernel_queues[i], kernels[i], 0, NULL, NULL));
        }
    }

    void finish()
    {
        for (size_t i = 0; i < par; ++i) {
            clFinish(kernel_queues[i]);
        }
    }

    void clean()
    {
        finish();

        {% for b in n.get_global_no_value_buffers() %}
        for (auto & b : {{ b.get_buffers_name() }}) {
            if (b) clCheckError(clReleaseMemObject(b));
        }

        for (auto & q : {{ b.get_queues_name() }}) {
            if (q) clReleaseCommandQueue(q);
        }
        {% endfor %}

        for (auto & k : kernels) {
            if (k) clReleaseKernel(k);
        }

        for (auto & q : kernel_queues) {
            if (q) clReleaseCommandQueue(q);
        }
    }

};

{% endmacro %}

{% for n in nodes if not n.is_generator() %}
{{ create_node(n) }}
{% endfor %}

{% macro create_generator(n) -%}
struct F{{ n.name }}
{
    OCL & ocl;
    std::string name;
    size_t par;

    std::vector<cl_uint> sizes;

    std::vector<std::string> kernel_names;
    std::vector<cl_command_queue> kernel_queues;
    std::vector<cl_kernel> kernels;

    {% for b in n.get_global_no_value_buffers() %}
    // {{ b.name }} buffer
    std::vector<cl_command_queue> {{ b.get_queues_name() }};
    std::vector<cl_mem> {{ b.get_buffers_name() }};

    {% endfor %}

    F{{ n.name }}(OCL & ocl)
    : ocl(ocl)
    {
        name = "{{ n.name }}";
        par = {{ n.par }};

        sizes.resize(par);

        for (size_t i = 0; i < par; ++i) {
            kernel_queues.push_back(ocl.createCommandQueue());
        }

        // Kernels
        for (size_t i = 0; i < par; ++i) {
            kernels.push_back(ocl.createKernel(name + "_" + std::to_string(i)));
        }

        // Create Buffers and their Queues
        {% for b in n.get_global_no_value_buffers() %}
        // {{ b.name }} buffers
        {% set nums = ('par' if b.is_access_single() else 1) %}
        for (size_t i = 0; i < {{ nums }}; ++i) {
            {{ b.get_queues_name() }}.push_back(ocl.createCommandQueue());

            cl_int status;
            cl_mem buff = clCreateBuffer(ocl.context,
                                         CL_MEM_HOST_WRITE_ONLY | {{ b.get_flags() }},
                                         {{ b.size }} * sizeof({{ b.datatype }}),
                                         NULL, &status);
            clCheckErrorMsg(status, "Failed to create {{ b.name }}");

            {{ b.get_buffers_name() }}.push_back(buff);
        }
        {% endfor %}
    }

    void set_size(size_t replica_id, cl_uint size)
    {
        sizes[replica_id] = size;
    }

    void set_size_all(cl_uint size)
    {
        for (size_t i = 0; i < par; ++i) {
            sizes[i] = size;
        }
    }


    {% for b in n.get_global_no_value_buffers() %}
    void prepare_{{ b.name }}(const std::vector<{{ b.datatype }}> & data{{ ', size_t replica_id' if b.is_access_single() else ''}})
    {
        clCheckError(clEnqueueWriteBuffer({{ b.get_queues_name('replica_id' if b.is_access_single() else '0') }},
                                          {{ b.get_buffers_name('replica_id' if b.is_access_single() else '0') }},
                                          CL_TRUE, 0,
                                          {{ b.size }} * sizeof({{ b.datatype }}), data.data(),
                                          0, NULL, NULL));
    }

    {% endfor %}


    void launch_kernels()
    {
        // TODO: checking if all prepare() buffers are done
        {% for b in n.get_global_no_value_buffers() %}
        for (auto & q : {{ b.get_queues_name() }}) {
            clFinish(q);
        }
        {% endfor %}

        {% for b in n.get_global_value_buffers() %}
        {{ b.get_declare_and_init() }};
        {% endfor %}

        for (size_t i = 0; i < par; ++i) {
            clCheckError(clSetKernelArg(kernels[i], 0, sizeof(cl_uint), &sizes[i]));
        }

        {% if n.get_global_buffers() | count > 0 %}
        for (size_t i = 0; i < par; ++i) {
            cl_uint argi = 1;
            {% for b in n.get_global_no_value_buffers() %}
            clCheckError(clSetKernelArg(kernels[i], argi++, sizeof({{ b.get_buffers_name('i' if b.is_access_single() else '0') }}), &{{ b.get_buffers_name('i' if b.is_access_single() else '0') }}));
            {% endfor %}
            {% for b in n.get_global_value_buffers() %}
            clCheckError(clSetKernelArg(kernels[i], argi++, sizeof({{ b.datatype }}), &{{ b.name }}));
            {% endfor %}
        }
        {% endif %}

        for (size_t i = 0; i < par; ++i) {
            clCheckError(clEnqueueTask(kernel_queues[i], kernels[i], 0, NULL, NULL));
        }
    }

    void finish()
    {
        for (size_t i = 0; i < par; ++i) {
            clFinish(kernel_queues[i]);
        }
    }

    void clean()
    {
        finish();

        {% for b in n.get_global_no_value_buffers() %}
        for (auto & b : {{ b.get_buffers_name() }}) {
            if (b) clCheckError(clReleaseMemObject(b));
        }

        for (auto & q : {{ b.get_queues_name() }}) {
            if (q) clReleaseCommandQueue(q);
        }
        {% endfor %}

        for (auto & k : kernels) {
            if (k) clReleaseKernel(k);
        }

        for (auto & q : kernel_queues) {
            if (q) clReleaseCommandQueue(q);
        }
    }

};

{% endmacro %}

{% for n in nodes if n.is_generator() %}
{{ create_generator(n) }}
{% endfor %}

{% if source %}
{% if shared_memory %}
struct FSource
{
    OCL & ocl;
    size_t par;

    size_t header_stride_exp;
    size_t data_stride_exp;

    size_t header_numels;
    size_t data_numels;

    std::vector<cl_command_queue> queues;
    std::vector<cl_kernel> kernels;

    std::vector<size_t> header_indexes;
    std::vector< clSharedBuffer<header_t> > headers;
    std::vector< clSharedBuffer<{{ source_data_type }}> > buffers;

    FSource(OCL & ocl,
            size_t par,
            size_t header_stride_exp,
            size_t data_stride_exp)
    : ocl(ocl)
    , par(par)
    , header_stride_exp(header_stride_exp)
    , data_stride_exp(data_stride_exp)
    , header_numels(1 << header_stride_exp)
    , data_numels(1 << data_stride_exp)
    , queues(par)
    , kernels(par)
    , header_indexes(par)
    {
        for (size_t p = 0; p < par; ++p) {
            queues[p] = ocl.createCommandQueue();
            kernels[p] = ocl.createKernel("{{ source.name }}_" + std::to_string(p));

            header_indexes[p] = 0;
        }

        for (size_t p = 0; p < par; ++p) {
            headers.push_back(clSharedBuffer<header_t>(ocl,
                                                       header_numels,
                                                       CL_MEM_READ_WRITE,
                                                       true));
            headers[p].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            for (size_t h = 0; h < header_numels; ++h) {
                // header_new(close, ready, size)
                headers[p].ptr_volatile()[h] = header_new(false, false, 0);
            }

            buffers.push_back(clSharedBuffer<{{ source_data_type }}>(ocl,
                                                     header_numels * data_numels,
                                                     CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                     false));
            buffers[p].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            memset(buffers[p].ptr(), 0, header_numels * data_numels * sizeof({{source_data_type}}));
            // for (size_t b = 0; b < header_numels * data_numels; ++b) {
            //     buffers[p].ptr()[b].device_id = 0;
            //     buffers[p].ptr()[b].property_value = 0.0f;
            // }
        }

        WMB(); // ensures that writes on buffers are completed
    }

    void push(const std::vector<{{ source_data_type }}> & batch,
              cl_uint batch_size,
              size_t rid,
              bool last = false)
    {
        size_t idx = header_indexes[rid];
        size_t rep = divide_up(batch_size, data_numels);

        for (size_t i = 0; i < rep; ++i) {
            const size_t diff = batch_size - (i * data_numels);
            const size_t n = (diff > data_numels ? data_numels : diff);

            while (header_ready(headers[rid].ptr_volatile()[idx])) {};

            memcpy(&(buffers[rid].ptr()[idx * data_numels]), &(batch[i * data_numels]), n * sizeof({{ source_data_type }}));
            WMB();
            // header_new(close, ready, size)
            headers[rid].ptr_volatile()[idx] = header_new(last and ((diff < data_numels) | (i == (rep - 1))),
                                                          true,
                                                          n);

            idx = (idx + 1) % header_numels;
        }

        header_indexes[rid] = idx;
    }

    void launch_kernels()
    {
        const cl_uint _header_stride_exp = header_stride_exp;
        const cl_uint _data_stride_exp = data_stride_exp;
        for (size_t p = 0; p < par; ++p) {
            size_t argi = 0;
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(headers[p].buffer), &headers[p].buffer));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(buffers[p].buffer), &buffers[p].buffer));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(cl_uint), &_header_stride_exp));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(cl_uint), &_data_stride_exp));
            clCheckError(clEnqueueTask(queues[p], kernels[p], 0, NULL, NULL));
        }
    }

    void finish()
    {
        for (size_t p = 0; p < par; ++p) {
            clFinish(queues[p]);
        }
    }

    void clean()
    {
        finish();

        for (auto & h : headers) {
            h.release();
        }

        for (auto & b : buffers) {
            b.release();
        }

        for (size_t p = 0; p < par; ++p) {
            if (kernels[p]) clReleaseKernel(kernels[p]);
            if (queues[p]) clReleaseCommandQueue(queues[p]);
        }
    }
};
{% else %}
struct FSource
{
    // TODO: use a Blocking Queue?
    OCL & ocl;
    size_t par;
    cl_uint max_batch_size;
    size_t number_of_buffers;
    std::vector<size_t> iterations;

    std::vector<cl_command_queue> source_queues;
    std::vector< std::vector<cl_event> > source_events;
    std::vector<cl_kernel> source_kernels;

    std::vector<cl_command_queue> buffer_queues;
    std::vector< std::vector<cl_mem> > buffer_mems;

    FSource(OCL & ocl,
            size_t par,
            cl_uint max_batch_size,
            size_t number_of_buffers)
    : ocl(ocl)
    , par(par)
    , max_batch_size(max_batch_size)
    , number_of_buffers(number_of_buffers)
    , iterations(par)
    , source_queues(par)
    , source_events(par, std::vector<cl_event>(number_of_buffers))
    , source_kernels(par)
    , buffer_queues(par)
    , buffer_mems(par, std::vector<cl_mem>(number_of_buffers))
    {
        for (size_t p = 0; p < par; ++p) {
            buffer_queues[p] = ocl.createCommandQueue();
            source_queues[p] = ocl.createCommandQueue();
            source_kernels[p] = ocl.createKernel("{{ source.name }}_" + std::to_string(p));
            iterations[p] = 0;
        }

        for (size_t p = 0; p < par; ++p) {
            for (size_t i = 0; i < number_of_buffers; ++i) {
                cl_int status;
                buffer_mems[p][i] = clCreateBuffer(ocl.context,
                                               CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                               max_batch_size * sizeof({{ source_data_type }}),
                                               NULL, &status);
                clCheckErrorMsg(status, "Failed to create clBuffer");
            }
        }
    }

    void push(const std::vector<{{ source_data_type }}> & batch,
              cl_uint batch_size,
              size_t rid,
              bool last = false)
    {
        cl_uint last_push = (cl_uint)1 * last;
        const size_t it = iterations[rid];
        const size_t curr_idx = it % number_of_buffers;

        cl_event buffer_event;
        // push buffer
        if (it < number_of_buffers) {
            clCheckError(clEnqueueWriteBuffer(buffer_queues[rid],
                                              buffer_mems[rid][curr_idx],
                                              CL_FALSE, 0,
                                              batch_size * sizeof({{ source_data_type }}), batch.data(),
                                              0, NULL, &buffer_event));
            clFlush(buffer_queues[rid]);
        } else {
            clCheckError(clEnqueueWriteBuffer(buffer_queues[rid],
                                              buffer_mems[rid][curr_idx],
                                              CL_FALSE, 0,
                                              batch_size * sizeof({{ source_data_type }}), batch.data(),
                                              1, &source_events[rid][curr_idx], &buffer_event));
            clFlush(buffer_queues[rid]);

            clCheckError(clWaitForEvents(1, &source_events[rid][curr_idx]));
            if (source_events[rid][curr_idx]) clCheckError(clReleaseEvent(source_events[rid][curr_idx]));
            source_events[rid][curr_idx] = NULL;
        }

        // set kernel args
        cl_uint argi = 0;
        clCheckError(clSetKernelArg(source_kernels[rid], argi++, sizeof(buffer_mems[rid][curr_idx]), &buffer_mems[rid][curr_idx]));
        clCheckError(clSetKernelArg(source_kernels[rid], argi++, sizeof(batch_size), &batch_size));
        clCheckError(clSetKernelArg(source_kernels[rid], argi++, sizeof(last_push), &last_push));

        // launch kernel
        clCheckError(clEnqueueTask(source_queues[rid], source_kernels[rid], 1, &buffer_event, &source_events[rid][curr_idx]));

        clFlush(source_queues[rid]);
        clCheckError(clReleaseEvent(buffer_event));

        iterations[rid]++;
    }

    void finish()
    {
        for (size_t p = 0; p < par; ++p) {
            clFinish(buffer_queues[p]);
            clFinish(source_queues[p]);
        }
    }

    void clean()
    {
        finish();

        for (size_t p = 0; p < par; ++p) {

            for (auto & e : source_events[p]) {
                if (e) clCheckError(clReleaseEvent(e));
            }

            for (auto & b : buffer_mems[p]) {
                if (b) clCheckError(clReleaseMemObject(b));
            }
            if (buffer_queues[p]) clReleaseCommandQueue(buffer_queues[p]);

            if (source_kernels[p]) clReleaseKernel(source_kernels[p]);
            if (source_queues[p]) clReleaseCommandQueue(source_queues[p]);
        }
    }
};
{% endif %}
{% endif %}

// TODO: make the pop async with arbitrary degree (use n-buffering and a queue)
{% if sink %}
{% if shared_memory %}
struct FSink
{
    OCL & ocl;
    size_t par;

    size_t header_stride_exp;
    size_t data_stride_exp;

    size_t header_numels;
    size_t data_numels;

    std::vector<cl_command_queue> queues;
    std::vector<cl_kernel> kernels;

    std::vector<size_t> header_indexes;
    std::vector< clSharedBuffer<header_t> > headers;
    std::vector< clSharedBuffer<{{sink_data_type}}> > buffers;


    FSink(OCL & ocl,
          size_t par,
          size_t header_stride_exp,
          size_t data_stride_exp)
    : ocl(ocl)
    , par(par)
    , header_stride_exp(header_stride_exp)
    , data_stride_exp(data_stride_exp)
    , header_numels(1 << header_stride_exp)
    , data_numels(1 << data_stride_exp)
    , queues(par)
    , kernels(par)
    , header_indexes(par)
    {
        for (size_t p = 0; p < par; ++p) {
            queues[p] = ocl.createCommandQueue();
            kernels[p] = ocl.createKernel("sink_" + std::to_string(p));
            header_indexes[p] = 0;
        }

        for (size_t p = 0; p < par; ++p) {
            // headers
            headers.push_back(clSharedBuffer<header_t>(ocl,
                                                       header_numels,
                                                       CL_MEM_READ_WRITE,
                                                       true));
            headers[p].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            for (size_t h = 0; h < header_numels; ++h) {
                headers[p].ptr_volatile()[h] = header_new(false, false, 0);
            }


            // buffers
            buffers.push_back(clSharedBuffer<{{sink_data_type}}>(ocl,
                                                      header_numels * data_numels,
                                                      CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
                                                      false));
            buffers[p].map(CL_MAP_READ | CL_MAP_WRITE_INVALIDATE_REGION);

            memset(buffers[p].ptr(), 0, header_numels * data_numels * sizeof({{sink_data_type}}));
            // for (size_t b = 0; b < header_numels * data_numels; ++b) {
            //     buffers[p].ptr()[b].device_id = 0;
            //     buffers[p].ptr()[b].property_value = 0.0f;
            //     buffers[p].ptr()[b].incremental_average = 0.0f;
            // }
        }
        WMB();
    }

    std::vector<{{sink_data_type}}> pop(size_t rid,
                             cl_uint batch_size, // unused
                             cl_uint * received,
                             bool * shutdown)
    {
        size_t idx = header_indexes[rid];
        auto h_ptr = headers[rid].ptr_volatile();
        while (!header_ready(h_ptr[idx])) {};

        *shutdown = header_close(h_ptr[idx]);

        const size_t received_batch_size = header_size(h_ptr[idx]);

        std::vector<{{sink_data_type}}> batch(received_batch_size);
        memcpy(batch.data(), &(buffers[rid].ptr()[idx * data_numels]), received_batch_size * sizeof({{sink_data_type}}));
        LMB();
        h_ptr[idx] = header_new(false, false, 0);

        header_indexes[rid] = (idx + 1) % header_numels;

        *received = batch.size();
        return batch;
    }

    void launch_kernels()
    {
        const cl_uint _header_stride_exp = header_stride_exp;
        const cl_uint _data_stride_exp = data_stride_exp;
        for (size_t p = 0; p < par; ++p) {
            size_t argi = 0;
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(headers[p].buffer), &headers[p].buffer));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(buffers[p].buffer), &buffers[p].buffer));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(cl_uint), &_header_stride_exp));
            clCheckError(clSetKernelArg(kernels[p], argi++, sizeof(cl_uint), &_data_stride_exp));
            clCheckError(clEnqueueTask(queues[p], kernels[p], 0, NULL, NULL));
        }
    }

    void finish()
    {
        for (size_t p = 0; p < par; ++p) {
            clFinish(queues[p]);
        }
    }

    void clean()
    {
        finish();

        for (auto & h : headers) {
            h.release();
        }

        for (auto & b : buffers) {
            b.release();
        }

        for (size_t p = 0; p < par; ++p) {
            if (kernels[p]) clReleaseKernel(kernels[p]);
            if (queues[p]) clReleaseCommandQueue(queues[p]);
        }
    }
};
{% else %}
struct FSink
{
    OCL & ocl;
    size_t par;
    std::vector<cl_uint> iterations;
    cl_uint batch_size;
    std::vector<_sink_context_t> sink_contexts;

    std::vector<cl_command_queue> sink_queues;
    std::vector<cl_event> sink_events;
    std::vector<cl_kernel> sink_kernels;

    std::vector<cl_command_queue> buffer_queues;
    std::vector<cl_event> buffer_events;
    std::vector<cl_mem> buffer_mems;

    std::vector<cl_command_queue> context_queues;
    std::vector<cl_mem> context_mems;

    FSink(OCL & ocl,
          size_t par,
          cl_uint batch_size)
    : ocl(ocl)
    , par(par)
    , iterations(par)
    , batch_size(batch_size)
    , sink_contexts(par)
    , sink_queues(par)
    , sink_events(par)
    , sink_kernels(par)
    , buffer_queues(par)
    , buffer_events(par)
    , buffer_mems(par)
    , context_queues(par)
    , context_mems(par)
    {
        _sink_context_t empty_context;
        empty_context.received = 0;
        for (uint i = 0; i < {{ sink.i_degree }}; ++i) {
            empty_context.EOS[i] = false;
        }

        for (size_t p = 0; p < par; ++p) {
            sink_queues[p] = ocl.createCommandQueue();
            sink_kernels[p] = ocl.createKernel("{{ sink.name }}_0");
            buffer_queues[p] = ocl.createCommandQueue();
            context_queues[p] = ocl.createCommandQueue();
            iterations[p] = 0;

            cl_int status;
            buffer_mems[p] = clCreateBuffer(ocl.context,
                                            CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
                                            batch_size * sizeof({{ sink_data_type }}),
                                            NULL, &status);
            clCheckErrorMsg(status, "Failed to create clBuffer (buffer_mem)");
            context_mems[p] = clCreateBuffer(ocl.context,
                                             CL_MEM_COPY_HOST_PTR | CL_MEM_READ_WRITE,
                                             sizeof(_sink_context_t),
                                             &empty_context, &status);
            clCheckErrorMsg(status, "Failed to create clBuffer (received_mem)");

            // set kernel args
            cl_uint argi = 0;
            clCheckError(clSetKernelArg(sink_kernels[p], argi++, sizeof(buffer_mems[p]), &buffer_mems[p]));
            clCheckError(clSetKernelArg(sink_kernels[p], argi++, sizeof(batch_size), &batch_size));
            clCheckError(clSetKernelArg(sink_kernels[p], argi++, sizeof(context_mems[p]), &context_mems[p]));
        }
    }

    // return true if it is the last pop to be done
    std::vector<{{ sink_data_type }}> pop(cl_uint rid,
                                          cl_uint batch_size,
                                          cl_uint * received,
                                          bool * shutdown)
    {
        if (iterations[rid] < 1) {
            clCheckError(clEnqueueTask(sink_queues[rid], sink_kernels[rid], 0, NULL, &sink_events[rid]));
        } else {
            clCheckError(clEnqueueTask(sink_queues[rid], sink_kernels[rid], 1, &buffer_events[rid], &sink_events[rid]));
        }
        clFlush(sink_queues[rid]);
        if (buffer_events[rid]) clCheckError(clReleaseEvent(buffer_events[rid]));
        buffer_events[rid] = NULL;

        clCheckError(clEnqueueReadBuffer(context_queues[rid],
                                         context_mems[rid],
                                         CL_TRUE, 0,
                                         sizeof(_sink_context_t), &sink_contexts[rid],
                                         1, &sink_events[rid], NULL));


        size_t received_h = sink_contexts[rid].received;
        std::vector<{{ sink_data_type }}> batch(received_h);
        if (received_h > 0) {
            clCheckError(clEnqueueReadBuffer(buffer_queues[rid],
                                             buffer_mems[rid],
                                             CL_TRUE, 0,
                                             received_h * sizeof({{ sink_data_type }}), batch.data(),
                                             1, &sink_events[rid], &buffer_events[rid]));
        }

        clCheckError(clReleaseEvent(sink_events[rid]));
        sink_events[rid] = NULL;

        *received = received_h;

        bool shutdown_h = true;
        for (uint i = 0; i < {{ sink.i_degree}}; ++i) {
            shutdown_h &= sink_contexts[rid].EOS[i];
        }
        *shutdown = shutdown_h;

        iterations[rid]++;

        return batch;
    }

    void finish()
    {
        for (size_t p = 0; p < par; ++p) {
            clFinish(buffer_queues[p]);
            clFinish(context_queues[p]);
            clFinish(sink_queues[p]);
        }
    }

    void clean()
    {
        finish();

        for (size_t p = 0; p < par; ++p) {
            if (sink_kernels[p]) clReleaseKernel(sink_kernels[p]);
            if (sink_queues[p]) clReleaseCommandQueue(sink_queues[p]);

            if (buffer_mems[p]) clCheckError(clReleaseMemObject(buffer_mems[p]));
            if (buffer_queues[p]) clReleaseCommandQueue(buffer_queues[p]);

            if (context_mems[p]) clCheckError(clReleaseMemObject(context_mems[p]));
            if (context_queues[p]) clReleaseCommandQueue(context_queues[p]);

        }
    }
};
{% endif %}
{% endif %}


struct FPipeGraph
{
    OCL & ocl;
    size_t source_batch_size;
    size_t sink_batch_size;

    {% for n in nodes if n.is_generator() %}
    F{{ n.name }} {{ n.name }}_node;
    {% endfor %}
    {% if source %}
    FSource source_node;
    {% endif %}
    {% for n in nodes if not n.is_generator() %}
    F{{ n.name }} {{ n.name }}_node;
    {% endfor %}
    {% if sink %}
    FSink sink_node;
    {% endif %}

    cl_ulong time_start;
    cl_ulong time_stop;

{% if shared_memory %}
    FPipeGraph(OCL & ocl,
               size_t source_header_stride_exp,
               size_t source_data_stride_exp,
               size_t sink_header_stride_exp,
               size_t sink_data_stride_exp)
    : ocl(ocl)
    , source_batch_size(1 << source_data_stride_exp)
    , sink_batch_size(1 << sink_data_stride_exp)
    {% for n in nodes if n.is_generator() %}
    , {{ n.name }}_node(ocl)
    {% endfor %}
    {% if source %}
    , source_node(ocl, {{ source.par }}, source_header_stride_exp, source_data_stride_exp)
    {% endif %}
    {% for n in nodes if not n.is_generator() %}
    , {{ n.name }}_node(ocl)
    {% endfor %}
    {% if sink %}
    , sink_node(ocl, {{ sink.par }}, sink_header_stride_exp, sink_data_stride_exp)
    {% endif %}
{% else %}
    FPipeGraph(OCL & ocl,
               size_t source_batch_size,
               size_t sink_batch_size,
               size_t number_of_buffers)
    : ocl(ocl)
    , source_batch_size(source_batch_size)
    , sink_batch_size(sink_batch_size)
    {% for n in nodes if n.is_generator() %}
    , {{ n.name }}_node(ocl)
    {% endfor %}
    {% if source %}
    , source_node(ocl, {{ source.par }}, source_batch_size, number_of_buffers)
    {% endif %}
    {% for n in nodes if not n.is_generator() %}
    , {{ n.name }}_node(ocl)
    {% endfor %}
    {% if sink %}
    , sink_node(ocl, {{ sink.par }}, sink_batch_size)
    {% endif %}
{% endif %}
    {
        {% for n in nodes if n.is_generator() %}
        {{ n.name }}_node.set_size_all(source_batch_size);
        {% endfor %}
    }

    void start()
    {
        // Run FPipeGraph
        time_start = current_time_ns();
        {% if shared_memory %}
        {% if source %}
        source_node.launch_kernels();
        {% endif %}
        {% endif %}

        {% for n in nodes %}
        {{ n.name }}_node.launch_kernels();
        {% endfor %}

        {% if shared_memory %}
        {% if sink %}
        sink_node.launch_kernels();
        {% endif %}
        {% endif %}
    }

    {% if source %}
    void push(const std::vector<{{ source_data_type }}> & batch,
              cl_uint batch_size,
              size_t replica_id,
              bool last = false)
    {
        source_node.push(batch, batch_size, replica_id, last);
    }
    {% endif %}

    {% if sink %}
    std::vector<{{ sink_data_type }}> pop(cl_uint rid,
                                          cl_uint batch_size,
                                          cl_uint * received,
                                          bool * shutdown)
    {
        return sink_node.pop(rid, batch_size, received, shutdown);
    }
    {% endif %}

    void wait_and_stop()
    {
        {% if source %}
        source_node.finish();
        {% endif %}
        {% for n in nodes %}
        {{ n.name }}_node.finish();
        {% endfor %}
        {% if sink %}
        sink_node.finish();
        {% endif %}
        time_stop = current_time_ns();
    }

    double service_time_ms()
    {
        return (time_stop - time_start) / 1.0e6;
    }

    double service_time_s()
    {
        return (time_stop - time_start) / 1.0e9;
    }

    void clean()
    {
        {% if source %}
        source_node.clean();
        {% endif %}
        {% for n in nodes %}
        {{ n.name }}_node.clean();
        {% endfor %}
        {% if sink %}
        sink_node.clean();
        {% endif %}
    }
};