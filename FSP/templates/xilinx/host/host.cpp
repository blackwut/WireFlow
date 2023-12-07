{%- macro include_tuples(tuples) -%}
{% for t in tuples %}
#include "../common/{{ t }}.hpp"
{% endfor %}
{%- endmacro %}
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <pthread.h>
#include <mutex>
#include <fstream>


#include "fspx_host.hpp"
#include "../common/constants.hpp"
{{include_tuples(tuples)}}


pthread_barrier_t barrier_setup;        // synchronize the setup of the threads
pthread_barrier_t barrier_start;        // synchronize the start of the computation
pthread_barrier_t barrier_end;          // synchronize the end of the computation

std::atomic<size_t> tuples_sent;        // tuples sent by the StreamGenerator threads
std::atomic<size_t> tuples_received;    // tuples received by the StreamDrainer threads
std::atomic<size_t> batches_sent;       // batches sent by the StreamGenerator threads
std::atomic<size_t> batches_received;   // batches received by the StreamDrainer threads


bool update_done(const uint64_t app_start_time,
                 const uint64_t app_run_time,
                 const uint64_t current_time = fx::current_time_nsecs())
{
    return ((current_time - app_start_time) > app_run_time);
}

template <typename T>
void stream_generator_thread(
    const size_t idx,
    fx::OCL & ocl,
    const std::vector<T, fx::aligned_allocator<T> > & dataset,
    const size_t num_batches,
    const size_t batch_size,
    const uint64_t & app_start_time,
    const uint64_t app_run_time
)
{
    std::vector<T, fx::aligned_allocator<T> > _dataset = dataset;
    size_t _tuples_sent = 0;
    size_t _batch_sent = 0;

    fx::StreamGenerator<T> stream_generator(ocl, batch_size, num_batches, idx);

    pthread_barrier_wait(&barrier_setup);
    pthread_barrier_wait(&barrier_start);

    size_t next_tuple_idx = 0;
    bool done = update_done(app_start_time, app_run_time);
    while (!done) {
        T * batch = stream_generator.get_batch();
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i] = dataset[next_tuple_idx];
            next_tuple_idx = ((next_tuple_idx + 1) == dataset.size() ? 0 : next_tuple_idx + 1);
        }

        done = update_done(app_start_time, app_run_time);
        stream_generator.push(batch, batch_size, done);
        _tuples_sent += batch_size;
        _batch_sent++;
    }
    stream_generator.finish();
    pthread_barrier_wait(&barrier_end);

    tuples_sent += _tuples_sent;
    batches_sent += _batch_sent;
}

template <typename T>
void stream_drainer_thread(
    const size_t idx,
    fx::OCL & ocl,
    const size_t num_batches,
    const size_t batch_size,
    const uint64_t & app_start_time
)
{
    (void) app_start_time;

    fx::StreamDrainer<T> stream_drainer(ocl, batch_size, num_batches, idx);
    size_t _tuples_received = 0;
    size_t _batch_received = 0;

    stream_drainer.prelaunch();
    pthread_barrier_wait(&barrier_setup);
    pthread_barrier_wait(&barrier_start);

    bool done = false;
    while(!done) {
        size_t items_written = 0;
        T * batch = stream_drainer.pop(&items_written, &done);
        stream_drainer.put_batch(batch, batch_size);
        _tuples_received += items_written;
        _batch_received++;
    }
    stream_drainer.finish();
    pthread_barrier_wait(&barrier_end);

    tuples_received += _tuples_received;
    batches_received += _batch_received;
}

int main(int argc, char** argv) {

    argc--;
    argv++;

    std::string bitstream  = "./kernels/{{ dest_dir }}/hw/build/{{ dest_dir }}.xclbin";
    size_t app_run_time_s = 1;
    size_t sampling_rate  = 1024;
    size_t generator_num_threads = 1;
    size_t drainer_num_threads = 1;
    size_t generator_num_buffers = 1;
    size_t drainer_num_buffers = 1;
    size_t generator_batch_size = 1 << 5;
    size_t drainer_batch_size = 1 << 5;

    int argi = 0;
    if (argc > argi) bitstream             = argv[argi++];
    if (argc > argi) app_run_time_s        = atoi(argv[argi++]);
    if (argc > argi) sampling_rate         = atoi(argv[argi++]);
    if (argc > argi) generator_num_threads = atoi(argv[argi++]);
    if (argc > argi) drainer_num_threads   = atoi(argv[argi++]);
    if (argc > argi) generator_num_buffers = atoi(argv[argi++]);
    if (argc > argi) drainer_num_buffers   = atoi(argv[argi++]);
    if (argc > argi) generator_batch_size  = atoi(argv[argi++]);
    if (argc > argi) drainer_batch_size    = atoi(argv[argi++]);

    const size_t transfer_size = sizeof(input_t) * generator_batch_size / 1024;
    std::cout
        << COUT_HEADER << "bitstream: "             <<                 bitstream             << "\n"
        << COUT_HEADER << "app_run_time_s: "        << COUT_INTEGER << app_run_time_s        << "\n"
        << COUT_HEADER << "sampling_rate: "         << COUT_INTEGER << sampling_rate         << "\n"
        << COUT_HEADER << "generator_num_threads: " << COUT_INTEGER << generator_num_threads << "\n"
        << COUT_HEADER << "drainer_num_threads: "   << COUT_INTEGER << drainer_num_threads   << "\n"
        << COUT_HEADER << "generator_num_buffers: " << COUT_INTEGER << generator_num_buffers << "\n"
        << COUT_HEADER << "drainer_num_buffers: "   << COUT_INTEGER << drainer_num_buffers   << "\n"
        << COUT_HEADER << "generator_batch_size: "  << COUT_INTEGER << generator_batch_size  << "\n"
        << COUT_HEADER << "drainer_batch_size: "    << COUT_INTEGER << drainer_batch_size    << "\n"
        << COUT_HEADER << "transfer_size: "         << COUT_INTEGER << transfer_size         << " KB\n"
        << '\n';


    uint64_t app_start_time = 0; // updated before synchronize barrier_start
    const uint64_t app_run_time_ns = app_run_time_s * uint64_t(1000000000L);

    std::vector<{{mr.i_datatype}}, fx::aligned_allocator<{{mr.i_datatype}}> > dataset = get_dataset<{{mr.i_datatype}}>(...);

    // initialize barrier_start and counters
    pthread_barrier_init(&barrier_setup, nullptr, generator_num_threads + drainer_num_threads + 1); // +1 for main thread
    pthread_barrier_init(&barrier_start, nullptr, generator_num_threads + drainer_num_threads + 1); // +1 for main thread
    pthread_barrier_init(&barrier_end,   nullptr, generator_num_threads + drainer_num_threads + 1); // +1 for main thread
    tuples_sent = 0;
    batches_sent = 0;
    tuples_received = 0;
    batches_received = 0;


    auto host_time_start = fx::high_resolution_time();
    fx::OCL ocl = fx::OCL(bitstream, 0, 0, true);

    std::vector<std::thread> generator_threads;
    for (size_t i = 0; i < generator_num_threads; ++i) {
        generator_threads.push_back(
            std::thread(
                stream_generator_thread<input_t>,
                i,
                std::ref(ocl),
                std::ref(dataset),
                generator_num_buffers,
                generator_batch_size,
                std::ref(app_start_time),
                app_run_time_ns
            )
        );
    }

    std::vector<std::thread> drainer_threads;
    for (size_t i = 0; i < drainer_num_threads; ++i) {
        drainer_threads.push_back(
            std::thread(
                stream_drainer_thread<tuple_t>,
                i,
                std::ref(ocl),
                drainer_num_buffers,
                drainer_batch_size,
                std::ref(app_start_time)
            )
        );
    }

    pthread_barrier_wait(&barrier_setup);
    app_start_time = fx::current_time_nsecs();
    auto time_start_compute = fx::high_resolution_time();
    pthread_barrier_wait(&barrier_start);
    pthread_barrier_wait(&barrier_end);
    auto time_end_compute = fx::high_resolution_time();

    for (auto& t : generator_threads) t.join();
    for (auto& t : drainer_threads) t.join();
    auto host_time_end = fx::high_resolution_time();


    auto elapsed_time_compute = fx::elapsed_time_ns(time_start_compute, time_end_compute);
    auto elapsed_time_host = fx::elapsed_time_ms(host_time_start, host_time_end);
    fx::print_performance<input_t>(" MR (compute)", tuples_sent,     time_start_compute, time_end_compute, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<tuple_t>(" MW (compute)", tuples_received, time_start_compute, time_end_compute, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<input_t>("APP (compute)", tuples_sent,     time_start_compute, time_end_compute, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<input_t>(" MR (host)   ", tuples_sent,     host_time_start,    host_time_end);
    fx::print_performance<tuple_t>(" MW (host)   ", tuples_received, host_time_start,    host_time_end);
    fx::print_performance<input_t>("APP (host)   ", tuples_sent,     host_time_start,    host_time_end);
    std::cout << COUT_HEADER_SMALL << "Total HOST time: " << COUT_INTEGER << elapsed_time_host << " ms\n";


    // cleaning up
    pthread_barrier_destroy(&barrier_setup);
    pthread_barrier_destroy(&barrier_start);
    pthread_barrier_destroy(&barrier_end);
    ocl.clean();

    return 0;
}