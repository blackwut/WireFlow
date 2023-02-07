{%- set source_data_type = source.i_datatype -%}
{%- set sink_data_type = sink.o_datatype -%}
#include <vector>
#include <string.h>

#include "../ocl/ocl.hpp"
#include "../ocl/utils.hpp"
#include "../common/constants.h"
#include "../common/tuples.h"
#include "../device/includes/fsp.cl"

#include "fsource.hpp"
#include "fsink.hpp"

enum struct FPipeTransfer {
    COPY,
    SHARED,
    HYBRID
};

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

    F{{ n.name }}(OCL & ocl, const size_t par)
    : ocl(ocl)
    , par(par)
    {
        name = "{{ n.name }}";

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
                                         {{ b.get_total_size() }} * sizeof({{ b.datatype }}),
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
                                          {{ b.get_total_size() }} * sizeof({{ b.datatype }}), data.data(),
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

    F{{ n.name }}(OCL & ocl, const size_t par)
    : ocl(ocl)
    , par(par)
    {
        name = "{{ n.name }}";

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
                                         {{ b.get_total_size() }} * sizeof({{ b.datatype }}),
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
                                          {{ b.get_total_size() }} * sizeof({{ b.datatype }}), data.data(),
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

template <typename SourceType_t, typename SinkType_t = SourceType_t>
struct FPipeGraph
{
    OCL & ocl;

    {% for n in nodes if n.is_generator() %}
    F{{ n.name }} {{ n.name }}_node;
    {% endfor %}
    {% if source %}
    FSource<SourceType_t> * source_node;
    {% endif %}
    {% for n in nodes if not n.is_generator() %}
    F{{ n.name }} {{ n.name }}_node;
    {% endfor %}
    {% if sink %}
    FSink<SinkType_t> * sink_node;
    {% endif %}

    volatile uint64_t time_start;
    volatile uint64_t time_stop;

    FPipeGraph(OCL & ocl, FPipeTransfer transfer_type, std::vector<size_t> & pars{{ ", size_t source_batch_size, size_t source_buffers" if source else "" }}{{ ", size_t sink_batch_size, size_t sink_buffers" if sink else "" }})
    : ocl(ocl)
    {% if source %}
    {% set _idx = namespace(value=1) %}
    {% endif %}
    {% for n in nodes if n.is_generator() %}
    , {{n.name}}_node(ocl, pars[{{_idx.value}}])
    {% set _idx.value = _idx.value + 1 %}
    {% endfor %}
    {% for n in nodes if not n.is_generator() %}
    , {{n.name}}_node(ocl, pars[{{_idx.value}}])
    {% set _idx.value = _idx.value + 1 %}
    {% endfor %}
    {
        {% if source %}
        add_source(transfer_type, pars[0], source_batch_size, source_buffers);
        {% endif %}
        {% if sink %}
        add_sink(transfer_type, pars[{{_idx.value}}], sink_batch_size, sink_buffers, pars[{{_idx.value - 1}}]);
        {% endif %}
    }

    {% if source %}
    void add_source(FSource<SourceType_t> * source) { source_node = source; }

    void add_source_copy(const size_t par,
                         const size_t batch_size,
                         const size_t N)
    {
        add_source(new FSourceCopy<SourceType_t>(ocl, par, batch_size, N));
    }

    void add_source_hybrid(const size_t par,
                           const size_t batch_size,
                           const size_t N)
    {
        add_source(new FSourceHybrid<SourceType_t>(ocl, par, batch_size, N));
    }

    void add_source_shared(const size_t par,
                           const size_t batch_size,
                           const size_t N)
    {
        add_source(new FSourceShared<SourceType_t>(ocl, par, batch_size, N));
    }

    void add_source(FPipeTransfer transfer_type, size_t source_par, size_t source_batch_size, size_t source_buffers)
    {
        switch (transfer_type) {
            case FPipeTransfer::COPY:
                add_source_copy(source_par, source_batch_size, source_buffers);
                break;
            case FPipeTransfer::HYBRID:
                add_source_hybrid(source_par, source_batch_size, source_buffers);
                break;
            case FPipeTransfer::SHARED:
                add_source_shared(source_par, source_batch_size, source_buffers);
                break;
        }
    }
    {% endif %}

    {% if sink %}
    void add_sink(FSink<SinkType_t> * sink) { sink_node = sink; }

    void add_sink_copy(const size_t par,
                       const size_t batch_size,
                       const size_t N,
                       const size_t previous_node_par)
    {
        add_sink(new FSinkCopy<SinkType_t>(ocl, par, batch_size, N, previous_node_par));
    }

    void add_sink_hybrid(const size_t par,
                         const size_t batch_size,
                         const size_t N,
                         const size_t previous_node_par)
    {
        add_sink(new FSinkHybrid<SinkType_t>(ocl, par, batch_size, N, previous_node_par));
    }

    void add_sink_shared(const size_t par,
                         const size_t batch_size,
                         const size_t N)
    {
        add_sink(new FSinkShared<SinkType_t>(ocl, par, batch_size, N));
    }

    void add_sink(FPipeTransfer transfer_type, size_t sink_par, size_t sink_batch_size, size_t sink_buffers, size_t previous_node_par)
    {
        switch (transfer_type) {
            case FPipeTransfer::COPY:
                add_sink_copy(sink_par, sink_batch_size, sink_buffers, previous_node_par);
                break;
            case FPipeTransfer::HYBRID:
                add_sink_hybrid(sink_par, sink_batch_size, sink_buffers, previous_node_par);
                break;
            case FPipeTransfer::SHARED:
                add_sink_shared(sink_par, sink_batch_size, sink_buffers);
                break;
        }
    }
    {% endif %}

    void start()
    {
        // Run FPipeGraph
        time_start = current_time_ns();

        {% for n in nodes if n.is_generator() %}
        {{ n.name }}_node.launch_kernels();
        {% endfor %}
        {% if source %}
        source_node->launch_kernels();
        {% endif %}
        {% for n in nodes if not n.is_generator() %}
        {{ n.name }}_node.launch_kernels();
        {% endfor %}
        {% if sink %}
        sink_node->launch_kernels();
        {% endif %}
    }

    {% if source %}
    SourceType_t * get_batch(const size_t rid)
    {
        return source_node->get_batch(rid);
    }

    void push(SourceType_t * batch,
              const size_t batch_size,
              const size_t rid,
              const bool last = false)
    {
        source_node->push(batch, batch_size, rid, last);
    }
    {% endif %}

    {% if sink %}
    SinkType_t * pop(const size_t rid,
                     const size_t batch_size,
                     size_t * received,
                     bool * last)
    {
        return sink_node->pop(rid, batch_size, received, last);
    }

    void put_batch(const size_t rid,
                   SinkType_t * batch)
    {
        sink_node->put_batch(rid, batch);
    }
    {% endif %}

    void wait_and_stop()
    {
        {% for n in nodes if n.is_generator() %}
        {{ n.name }}_node.finish();
        {% endfor %}
        {% if source %}
        source_node->finish();
        {% endif %}
        {% for n in nodes if not n.is_generator() %}
        {{ n.name }}_node.finish();
        {% endfor %}
        {% if sink %}
        sink_node->finish();
        {% endif %}

        time_stop = current_time_ns();
    }

    uint64_t service_time_ns()
    {
        return (time_stop - time_start);
    }

    double service_time_ms()
    {
        return service_time_ns() * 1.0e-6;
    }

    double service_time_s()
    {
        return service_time_ns() * 1.0e-9;
    }

    void clean()
    {
        {% for n in nodes if n.is_generator() %}
        {{ n.name }}_node.clean();
        {% endfor %}
        {% if source %}
        source_node->clean();
        {% endif %}
        {% for n in nodes if not n.is_generator() %}
        {{ n.name }}_node.clean();
        {% endfor %}
        {% if sink %}
        sink_node->clean();
        {% endif %}

        // deleting object of abstract class type which has non-virtual destructor will cause undefined behavior
        {% if source %}
        // if (source_node) delete source_node;
        {% endif %}
        {% if sink %}
        // if (sink_node) delete sink_node;
        {% endif %}
    }
};
