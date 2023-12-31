#ifndef __BENCHMARK_HPP__
#define __BENCHMARK_HPP__

#include <fstream>
#include <iostream>
#include <string>

#include "fspx_host.hpp"

void dump_benchmark(
    std::string filename,
    std::string bitstream,
    std::vector<std::string> operator_names,
    std::vector<size_t> operator_parallelisms,
    size_t mr_num_buffers,
    size_t mr_batch_size,
    size_t mw_num_buffers,
    size_t mw_batch_size,
    size_t sampling_rate,
    size_t tuple_sent_size,
    size_t tuples_sent,
    size_t batches_sent,
    size_t tuple_received_size, // unused
    size_t tuples_received,
    size_t batches_received,
    uint64_t elapsed_time_host,
    uint64_t elapsed_time_compute)
{
    (void)tuple_received_size;

    size_t tuples_sec = tuples_sent / (elapsed_time_compute / 1e9);
    size_t bytes_sec = tuples_sec * tuple_sent_size;

    bool print_header = false;
    std::ifstream infile(filename);
    if (!infile.good()) {
        print_header = true;
    }
    infile.close();

    std::ofstream outfile(filename, std::ios_base::app);
    if (print_header) {
        outfile << "Application" << ","
                << "Bitstream" << ",";

        for (auto name : operator_names) {
            outfile << name << ",";
        }

        outfile << "MR Buffers" << ","
                << "MW Buffers" << ","
                << "MR BatchSize" << ","
                << "MW BatchSize" << ","
                << "Sampling" << ","
                << "TupleSent" << ","
                << "BatchesSent" << ","
                << "TuplesReceived" << ","
                << "BatchesReceived" << ","
                << "Throughput (t/s)" << ","
                << "Throughput (B/s)" << ","
                << "Time (ms)" << ","
                << "Time Compute (ns)" << ","
                << "Samples" << ","
                << "Total" << ","
                << "Mean" << ","
                << "0" << ","
                << "5" << ","
                << "25" << ","
                << "50" << ","
                << "75" << ","
                << "95" << ","
                << "100"
                << std::endl;
    }

    auto latency_metric = fx::metric_group.get_metric("latency");

    outfile << "SpikeDetection" << ","
            << bitstream << ",";

    for (auto par : operator_parallelisms) {
        outfile << par << ",";
    }

    outfile << mr_num_buffers << ","
            << mw_num_buffers << ","
            << mr_batch_size << ","
            << mw_batch_size << ","
            << sampling_rate << ","
            << tuples_sent << ","
            << batches_sent << ","
            << tuples_received << ","
            << batches_received << ","
            << tuples_sec << ","
            << bytes_sec << ","
            << elapsed_time_host << ","
            << elapsed_time_compute << ","
            << latency_metric.getN() << ","
            << latency_metric.total() << ","
            << (uint64_t)(latency_metric.mean() / 1e3) << ","
            << (uint64_t)(latency_metric.min() / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.05) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.25) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.5) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.75) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.95) / 1e3) << ","
            << (uint64_t)(latency_metric.max() / 1e3)
            << std::endl;
    outfile.close();
}

#endif // __BENCHMARK_HPP__