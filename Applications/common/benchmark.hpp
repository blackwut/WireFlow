#ifndef __BENCHMARK_HPP__
#define __BENCHMARK_HPP__

#include <fstream>
#include <iostream>
#include <string>

#include "fspx_host.hpp"

size_t get_tuples_sec(size_t tuples, uint64_t time)
{
    return tuples / (time / 1e9);
}

size_t get_bytes_sec(size_t tuples, uint64_t time, size_t tuple_size)
{
    return get_tuples_sec(tuples, time) * tuple_size;
}

void dump_benchmark(
    std::string filename,
    std::string appname,
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
    size_t tuple_received_size,
    size_t tuples_received,
    size_t batches_received,
    uint64_t elapsed_time_host,
    uint64_t elapsed_time_compute)
{
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
                << "Throughput MR (t/s)" << ","
                << "Throughput MR (B/s)" << ","
                << "Throughput MW (t/s)" << ","
                << "Throughput MW (B/s)" << ","
                << "Time (ms)" << ","
                << "Time Compute (ns)" << ","
                << "Samples" << ","
                << "Total" << ","
                << "Mean" << ","
                << "0 (us)" << ","
                << "5 (us)" << ","
                << "25 (us)" << ","
                << "50 (us)" << ","
                << "75 (us)" << ","
                << "95 (us)" << ","
                << "100 (us)"
                << std::endl;
    }

    auto latency_metric = fx::metric_group.get_metric("latency");

    outfile << appname << ","
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
            << get_tuples_sec(tuples_sent, elapsed_time_compute) << ","
            << get_bytes_sec(tuples_sent, elapsed_time_compute, tuple_sent_size) << ","
            << get_tuples_sec(tuples_received, elapsed_time_compute) << ","
            << get_bytes_sec(tuples_received, elapsed_time_compute, tuple_received_size) << ","
            << elapsed_time_host << ","
            << elapsed_time_compute << ","
            << latency_metric.getN() << ","
            << latency_metric.total() << ","
            << (uint64_t)(latency_metric.mean() / 1e3) << ","
            << (uint64_t)(latency_metric.min() / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.05) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.25) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.50) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.75) / 1e3) << ","
            << (uint64_t)(latency_metric.percentile(0.95) / 1e3) << ","
            << (uint64_t)(latency_metric.max() / 1e3)
            << std::endl;
    outfile.close();
}

#endif // __BENCHMARK_HPP__