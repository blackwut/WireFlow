
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
#include "input_t.hpp"
#include "tuple_t.hpp"
#include "includes/dataset.hpp"
#include "includes/generator_thread.hpp"
#include "includes/drainer_thread.hpp"
#include "includes/benchmark.hpp"


pthread_barrier_t barrier_setup;        // synchronize the setup of the threads
pthread_barrier_t barrier_start;        // synchronize the start of the computation
pthread_barrier_t barrier_end;          // synchronize the end of the computation

std::atomic<size_t> tuples_sent;        // tuples sent by the StreamGenerator threads
std::atomic<size_t> tuples_received;    // tuples received by the StreamDrainer threads
std::atomic<size_t> batches_sent;       // batches sent by the StreamGenerator threads
std::atomic<size_t> batches_received;   // batches received by the StreamDrainer threads


int main(int argc, char** argv) {

    argc--;
    argv++;

    std::string bitstream  = "./kernels/sdtc1111/hw/build/sdtc.xclbin";
    size_t app_run_time_s = 1;
    size_t sampling_rate  = 1024;
    size_t mr_num_threads = 1;
    size_t mw_num_threads = 1;
    size_t mr_num_buffers = 1;
    size_t mw_num_buffers = 1;
    size_t mr_batch_size = 1 << 5;
    size_t mw_batch_size = 1 << 5;

    int argi = 0;
    if (argc > argi) bitstream      = argv[argi++];
    if (argc > argi) app_run_time_s = atoi(argv[argi++]);
    if (argc > argi) sampling_rate  = atoi(argv[argi++]);
    if (argc > argi) mr_num_threads = atoi(argv[argi++]);
    if (argc > argi) mw_num_threads = atoi(argv[argi++]);
    if (argc > argi) mr_num_buffers = atoi(argv[argi++]);
    if (argc > argi) mw_num_buffers = atoi(argv[argi++]);
    if (argc > argi) mr_batch_size  = atoi(argv[argi++]);
    if (argc > argi) mw_batch_size  = atoi(argv[argi++]);

    const size_t transfer_size = sizeof(input_t) * mr_batch_size / 1024;
    std::cout
        << COUT_HEADER << "bitstream: "      <<                 bitstream      << "\n"
        << COUT_HEADER << "app_run_time_s: " << COUT_INTEGER << app_run_time_s << "\n"
        << COUT_HEADER << "sampling_rate: "  << COUT_INTEGER << sampling_rate  << "\n"
        << COUT_HEADER << "mr_num_threads: " << COUT_INTEGER << mr_num_threads << "\n"
        << COUT_HEADER << "mw_num_threads: " << COUT_INTEGER << mw_num_threads << "\n"
        << COUT_HEADER << "mr_num_buffers: " << COUT_INTEGER << mr_num_buffers << "\n"
        << COUT_HEADER << "mw_num_buffers: " << COUT_INTEGER << mw_num_buffers << "\n"
        << COUT_HEADER << "mr_batch_size: "  << COUT_INTEGER << mr_batch_size  << "\n"
        << COUT_HEADER << "mw_batch_size: "  << COUT_INTEGER << mw_batch_size  << "\n"
        << COUT_HEADER << "transfer_size: "  << COUT_INTEGER << transfer_size  << " KB\n"
        << '\n';


    uint64_t app_start_time = 0; // updated before synchronize barrier_start
    const uint64_t app_run_time_ns = app_run_time_s * uint64_t(1000000000L);
    std::vector<input_t, fx::aligned_allocator<input_t> > dataset = get_dataset<input_t>("../../Datasets/SD/sensors.dat", TEMPERATURE);

    // initialize barrier_start and counters
    pthread_barrier_init(&barrier_setup, nullptr, mr_num_threads + mw_num_threads + 1); // +1 for main thread
    pthread_barrier_init(&barrier_start, nullptr, mr_num_threads + mw_num_threads + 1); // +1 for main thread
    pthread_barrier_init(&barrier_end, nullptr, mr_num_threads + mw_num_threads + 1);   // +1 for main thread
    tuples_sent = 0;
    batches_sent = 0;
    tuples_received = 0;
    batches_received = 0;


    auto host_time_start = fx::high_resolution_time();
    fx::OCL ocl = fx::OCL(bitstream, 0, 0, true);

    std::vector<std::thread> mr_threads;
    for (size_t i = 0; i < mr_num_threads; ++i) {
        mr_threads.push_back(
            std::thread(
                stream_generator_thread<input_t>,
                i,
                std::ref(ocl),
                std::ref(dataset),
                mr_num_buffers,
                mr_batch_size,
                std::ref(app_start_time),
                app_run_time_ns
            )
        );
    }

    std::vector<std::thread> mw_threads;
    for (size_t i = 0; i < mw_num_threads; ++i) {
        mw_threads.push_back(
            std::thread(
                stream_drainer_thread<tuple_t>,
                i,
                std::ref(ocl),
                mw_num_buffers,
                mw_batch_size,
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

    for (auto& t : mr_threads) t.join();
    for (auto& t : mw_threads) t.join();
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

    // dump benchmark results
    dump_benchmark(
        "results.csv",
        bitstream,
        {"MemoryReader", "AverageCalculator", "SpikeDetector", "MemoryWriter"},
        {mr_num_threads, mr_num_threads, mr_num_threads, mw_num_threads},
        mr_num_buffers,
        mr_batch_size,
        mw_num_buffers,
        mw_batch_size,
        sampling_rate,
        sizeof(input_t),
        tuples_sent,
        batches_sent,
        sizeof(tuple_t),
        tuples_received,
        batches_received,
        elapsed_time_host,
        elapsed_time_compute
    );

    return 0;
}
