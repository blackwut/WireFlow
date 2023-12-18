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
#include "../common/input_t.hpp"
#include "../common/tuple_t.hpp"
#include "../common/result_t.hpp"
#include "includes/dataset.hpp"
#include "../../../common/generator_thread.hpp"
#include "../../../common/drainer_thread.hpp"
#include "../../../common/benchmark.hpp"


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

    std::string bitstream  = "./kernels/ffdc1111/hw/build/ffdc.xclbin";
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


    std::string model_filepath = "../../../Datasets/FD/model.txt";
    std::string dataset_filepath = "../../../Datasets/FD/credit-card.dat";

    std::cout << "Loading Markov Model...\n";
    std::vector<float> trans_prob_data = get_model<float>(model_filepath);
    std::cout << "Markov Model loaded!\n";

    std::cout << "Loading dataset...\n";
    std::vector<input_t, fx::aligned_allocator<input_t> > dataset = get_dataset<input_t>(dataset_filepath, STATE_SIZE);
    std::cout << "Datset loaded: " << dataset.size() << " tuples\n";

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
                stream_drainer_thread<result_t>,
                i,
                std::ref(ocl),
                drainer_num_buffers,
                drainer_batch_size,
                std::ref(app_start_time),
                sampling_rate
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

    // dump benchmark results
    dump_benchmark(
        "results.csv",
        "FraudFreqDetection",
        bitstream,
        {"MemoryReader", "Predictor", "Frequency" "MemoryWriter"},
        {generator_num_threads, generator_num_threads, generator_num_threads, drainer_num_threads},
        generator_num_buffers,
        generator_batch_size,
        drainer_num_buffers,
        drainer_batch_size,
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