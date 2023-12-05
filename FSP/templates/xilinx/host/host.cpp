{%- macro include_tuples(tuples) -%}
{% for t in tuples %}
#include "{{ t }}.hpp"
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
{{include_tuples(tuples)}}


std::mutex mutex_print;
pthread_barrier_t barrier;

std::atomic<size_t> tuples_sent;
std::atomic<size_t> tuples_received;
std::atomic<size_t> batches_sent;
std::atomic<size_t> batches_received;


template <typename T>
void mr_thread(
    const size_t idx,
    fx::OCL & ocl,
    const std::vector<T, fx::aligned_allocator<T> > & dataset,
    const size_t iterations,
    const size_t num_batches,
    const size_t batch_size
)
{
    fx::MemoryReader<T> mr(ocl, batch_size, num_batches, idx);
    size_t _tuples_sent = 0;
    size_t _batch_sent = 0;

    pthread_barrier_wait(&barrier);

    for (size_t it = 0; it < iterations; ++it) {
        // T * batch = mr.get_batch();
        // fx::fill_batch_with_dataset<T>(dataset, batch, batch_size);

        // mr.push(batch, batch_size, (it == (iterations - 1)));
        // _tuples_sent += batch_size;
        // _batch_sent++;

        const bool last_batch = it == (iterations - 1);
        size_t idx = tuples_sent % dataset.size();
        for (size_t i = 0; i < batch_size; ++i) {
            const bool last = last_batch && i == (batch_size - 1);
            mr.push(dataset[idx], last);

            idx++;
            if (idx == dataset.size()) {
                idx = 0;
            }
        }
        _tuples_sent += batch_size;
        _batch_sent++;
    }

    mr.finish();

    tuples_sent += _tuples_sent;
    batches_sent += _batch_sent;
}

template <typename T>
void mw_thread(
    const size_t idx,
    fx::OCL & ocl,
    const size_t num_batches,
    const size_t batch_size
)
{
    fx::MemoryWriter<T> mw(ocl, batch_size, num_batches, idx);
    size_t _tuples_received = 0;
    size_t _batch_received = 0;

    pthread_barrier_wait(&barrier);
    mw.prelaunch();

    bool done = false;
    while(!done) {
        size_t count = 0;
        T * batch = mw.pop(&count, &done);
        mw.put_batch(batch, batch_size);

        size_t num_tuples = count * ((512 / 8) / sizeof(T));
        _tuples_received += num_tuples;
        _batch_received++;
    }

    mw.finish();

    tuples_received += _tuples_received;
    batches_received += _batch_received;
}

int main(int argc, char** argv) {

    argc--;
    argv++;

    std::string bitstream  = "./kernels/th111/hw/th111.xclbin";
    size_t iterations = 1;
    size_t mr_num_threads = 1;
    size_t mw_num_threads = 1;
    size_t mr_num_buffers = 1;
    size_t mw_num_buffers = 1;
    size_t mr_batch_size = 1 << 5;
    size_t mw_batch_size = 1 << 5;

    int argi = 0;
    if (argc > argi) bitstream      = argv[argi++];
    if (argc > argi) iterations     = atoi(argv[argi++]);
    if (argc > argi) mr_num_threads = atoi(argv[argi++]);
    if (argc > argi) mw_num_threads = atoi(argv[argi++]);
    if (argc > argi) mr_num_buffers = atoi(argv[argi++]);
    if (argc > argi) mw_num_buffers = atoi(argv[argi++]);
    if (argc > argi) mr_batch_size  = atoi(argv[argi++]);
    if (argc > argi) mw_batch_size  = atoi(argv[argi++]);

    const size_t transfer_size = sizeof({{mr.o_datatype}}) * mr_batch_size / 1024;
    std::cout
        << COUT_HEADER << "bitstream: "      <<                 bitstream      << "\n"
        << COUT_HEADER << "iterations: "     << COUT_INTEGER << iterations     << "\n"
        << COUT_HEADER << "mr_num_threads: " << COUT_INTEGER << mr_num_threads << "\n"
        << COUT_HEADER << "mw_num_threads: " << COUT_INTEGER << mw_num_threads << "\n"
        << COUT_HEADER << "mr_num_buffers: " << COUT_INTEGER << mr_num_buffers << "\n"
        << COUT_HEADER << "mw_num_buffers: " << COUT_INTEGER << mw_num_buffers << "\n"
        << COUT_HEADER << "mr_batch_size: "  << COUT_INTEGER << mr_batch_size  << "\n"
        << COUT_HEADER << "mw_batch_size: "  << COUT_INTEGER << mw_batch_size  << "\n"
        << COUT_HEADER << "transfer_size: "  << COUT_INTEGER << transfer_size  << " KB\n"
        << '\n';


    std::vector<std::vector<{{mr.o_datatype}}, fx::aligned_allocator<{{mr.o_datatype}}> >> datasets;
    for (size_t i = 0; i < mr_num_threads; ++i) {
        // TODO: generate or load dataset for each mr thread
        datasets.push_back(...);
    }


    // initialize barrier and counters
    pthread_barrier_init(&barrier, nullptr, mr_num_threads + mw_num_threads + 1); // +1 for main thread
    tuples_sent = 0;
    batches_sent = 0;
    tuples_received = 0;
    batches_received = 0;


    auto time_start = fx::high_resolution_time();

    fx::OCL ocl = fx::OCL(bitstream, 0, 0, true);

    std::vector<std::thread> mr_threads;
    for (size_t i = 0; i < mr_num_threads; ++i) {
        mr_threads.push_back(
            std::thread(
                mr_thread<{{mr.o_datatype}}>,
                i,
                std::ref(ocl),
                std::ref(datasets[i]),
                iterations,
                mr_num_buffers,
                mr_batch_size
            )
        );
    }

    std::vector<std::thread> mw_threads;
    for (size_t i = 0; i < mw_num_threads; ++i) {
        mw_threads.push_back(
            std::thread(
                mw_thread<{{mw.o_datatype}}>,
                i,
                std::ref(ocl),
                mw_num_buffers,
                mw_batch_size
            )
        );
    }


    pthread_barrier_wait(&barrier);
    auto time_start_compute = fx::high_resolution_time();
    for (auto& t : mr_threads) t.join();
    for (auto& t : mw_threads) t.join();
    auto time_end = fx::high_resolution_time();


    auto time_elapsed_compute_ns = fx::elapsed_time_ns(time_start_compute, time_end);
    auto time_elapsed_ms = fx::elapsed_time_ms(time_start, time_end);
    fx::print_performance<{{mr.o_datatype}}>(" MR (compute)", tuples_sent,     time_start_compute, time_end, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<{{mw.o_datatype}}>(" MW (compute)", tuples_received, time_start_compute, time_end, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<{{mr.o_datatype}}>("APP (compute)", tuples_received, time_start_compute, time_end, fx::COLOR_LIGHT_GREEN);
    fx::print_performance<{{mr.o_datatype}}>(" MR (overall)", tuples_sent,     time_start,         time_end);
    fx::print_performance<{{mw.o_datatype}}>(" MW (overall)", tuples_received, time_start,         time_end);
    fx::print_performance<{{mr.o_datatype}}>("APP (overall)", tuples_sent,     time_start,         time_end);
    std::cout << COUT_HEADER_SMALL << "Total HOST time: " << COUT_INTEGER << time_elapsed_ms << " ms\n";


    pthread_barrier_destroy(&barrier);
    ocl.clean();


    size_t tuples_sec = tuples_sent / (time_elapsed_compute_ns / 1e9);
    size_t bytes_sec = tuples_sec * sizeof({{mr.o_datatype}});

    bool print_header = false;
    std::ifstream infile("results.csv");
    if (!infile.good()) {
        print_header = true;
    }
    infile.close();

    std::ofstream outfile("results.csv", std::ios_base::app);
    if (print_header) {
        outfile << "bitstream,"
                << "iterations,"
                << "mr_num_threads,"
                << "mw_num_threads,"
                << "mr_num_buffers,"
                << "mw_num_buffers,"
                << "mr_batch_size,"
                << "mw_batch_size,"
                << "transfer_size,"
                << "time_elapsed_ms,"
                << "time_elapsed_compute_ns,"
                << "tuples_sent,"
                << "batches_sent,"
                << "tuples_received,"
                << "batches_received,"
                << "tuples_sec,"
                << "bytes_sec\n";
    }

    outfile << bitstream << ","
            << iterations << ","
            << mr_num_threads << ","
            << mw_num_threads << ","
            << mr_num_buffers << ","
            << mw_num_buffers << ","
            << mr_batch_size << ","
            << mw_batch_size << ","
            << transfer_size << ","
            << time_elapsed_ms << ","
            << time_elapsed_compute_ns << ","
            << tuples_sent << ","
            << batches_sent << ","
            << tuples_received << ","
            << batches_received << ","
            << tuples_sec << ","
            << bytes_sec << "\n";
    outfile.close();

    return 0;
}