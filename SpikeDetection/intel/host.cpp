#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <unistd.h>
#include <atomic>

#ifdef MEASURE_LATENCY
#include "metric/sampler.hpp"
#include "metric/metric_group.hpp"
#endif

#include "includes/pipe.hpp"
#include "includes/dataset.hpp"


struct sink_batch
{
    uint32_t source_timestamp;
    uint32_t sink_timestamp;

    sink_batch(uint32_t source_timestamp, uint32_t sink_timestamp)
    : source_timestamp(source_timestamp)
    , sink_timestamp(sink_timestamp)
    {}
};


std::atomic<uint64_t> sent_tuples;          // total number of tuples sent by all sources
std::atomic<uint64_t> sent_batches;         // total number of batches sent by all sources
std::atomic<uint64_t> received_tuples;      // total number of tuples received by all sinks
std::atomic<uint64_t> received_batches;     // total number of batches received by all sinks


bool update_done(const uint64_t app_start_time,
                 const uint64_t app_run_time)
{
    return ((current_time_ns() - app_start_time) > app_run_time);
}


template <typename SourceType_t, typename SinkType_t = SourceType_t>
void source_thread(FPipeGraph<SourceType_t, SinkType_t> & pipe,
                   std::vector<SourceType_t> & dataset,
                   const size_t batch_size,
                   const uint64_t app_start_time,
                   const uint64_t app_run_time,
                   const size_t tid)
{
    const std::string start_str = "Source " + std::to_string(tid) + " started!\n";
    std::cout << start_str;

    uint64_t _sent_tuples = 0;
    uint64_t _sent_batches = 0;

    size_t next_tuple_idx = 0;
    bool done = update_done(app_start_time, app_run_time);
    while (!done) {
        SourceType_t * batch = pipe.get_batch(tid);

#ifdef MEASURE_LATENCY
        const uint32_t _timestamp = static_cast<uint32_t>(current_time_ns() - app_start_time);
        for (size_t i = 0; i < batch_size; ++i) {
            SourceType_t t = dataset[next_tuple_idx];
            t.timestamp = _timestamp;
            batch[i] = t;
            next_tuple_idx = (next_tuple_idx + 1) % dataset.size();
        }
#else
        for (size_t i = 0; i < batch_size; ++i) {
            batch[i] = dataset[next_tuple_idx];
            next_tuple_idx = (next_tuple_idx + 1) % dataset.size();
        }
#endif

        done = update_done(app_start_time, app_run_time);
        pipe.push(batch, batch_size, tid, done);

        _sent_tuples += batch_size;
        _sent_batches++;
    }

    sent_tuples.fetch_add(_sent_tuples);
    sent_batches.fetch_add(_sent_batches);

    const std::string end_str = "Source " + std::to_string(tid) + " ending!\n";
    std::cout << end_str;
}


template <typename SourceType_t, typename SinkType_t = SourceType_t>
void sink_thread(FPipeGraph<SourceType_t, SinkType_t> & pipe,
                 std::vector<sink_batch> & results,
                 const size_t batch_size,
                 const uint64_t app_start_time,
                 const size_t sampling_rate,
                 const size_t tid)
{
    const std::string start_str = "Sink " + std::to_string(tid) + " started!\n";
    std::cout << start_str;

    uint64_t _received_tuples = 0;
    uint64_t _received_batches = 0;

    bool last = false;
    while (!last) {
        size_t received = 0;
        SinkType_t * batch = pipe.pop(tid, batch_size, &received, &last);

        if (!last and received <= 0) {
            const std::string nothing_str = "\n\nSink " + std::to_string(tid) + " has received NOTHING!\n";
            std::cout << nothing_str;
        } else {
            #ifdef MEASURE_LATENCY
            const uint32_t _timestamp = static_cast<uint32_t>(current_time_ns() - app_start_time);
            for (size_t i = 0; i < received; i += sampling_rate) {
                results.emplace_back(batch[i].timestamp, _timestamp);
            }
            #endif
        }

        pipe.put_batch(tid, batch);

        _received_tuples += received;
        _received_batches++;
    }

    received_tuples.fetch_add(_received_tuples);
    received_batches.fetch_add(_received_batches);

    const std::string end_str = "Sink " + std::to_string(tid) + " ending!\n";
    std::cout << end_str;
}


std::string get_aocx_filepath(const std::vector<size_t> & pars,
                              const FPipeTransfer transfer_type)
{
    std::stringstream ss;
    ss << "sd";
    if (transfer_type == FPipeTransfer::COPY)   ss << "c";
    if (transfer_type == FPipeTransfer::HYBRID) ss << "c";
    if (transfer_type == FPipeTransfer::SHARED) ss << "s";

#ifdef MEASURE_LATENCY
    ss << "l";
#else
    ss << "t";
#endif

    if (sizeof(FLOAT_T) == sizeof(float)) {
        ss << "f";
    } else {
        ss << "d";
    }

    for (auto & p : pars) {
        ss << std::to_string(p);
    }
    ss << ".aocx";

    return ss.str();
}


int main(int argc, char * argv[])
{
    const std::string program_name = std::string(argv[0]);

    int platform_id = 0;
    int device_id = 0;
    std::string aocx_filepath = "device.aocx";
    size_t source_buffers = 2;
    size_t source_batch_size = 1024;
    size_t sink_buffers = 2;
    size_t sink_batch_size = 1024;
    std::string dataset_filepath = "./dataset.dat";
    std::string pipe_pars = "1,1,1,1";
    size_t source_par = 1;
    size_t average_calculator_par = 1;
    size_t spike_detector_par = 1;
    size_t sink_par = 1;
    std::string transfer_type_str = "copy"; // "copy", "shared", "hybrid"
    FPipeTransfer transfer_type = FPipeTransfer::COPY;
    size_t app_run_time_s = 5;
    std::string results_filepath = "";
    size_t sampling_rate = 16;

    argc--;
    argv++;

    int argi = 0;
    if (argc > argi) platform_id       = atoi(argv[argi++]);
    if (argc > argi) device_id         = atoi(argv[argi++]);
    if (argc > argi) source_buffers    = atoi(argv[argi++]);
    if (argc > argi) source_batch_size = atoi(argv[argi++]);
    if (argc > argi) sink_buffers      = atoi(argv[argi++]);
    if (argc > argi) sink_batch_size   = atoi(argv[argi++]);
    if (argc > argi) dataset_filepath  = std::string(argv[argi++]);
    if (argc > argi) pipe_pars         = std::string(argv[argi++]);
    if (argc > argi) transfer_type_str = std::string(argv[argi++]);
    if (argc > argi) app_run_time_s    = atoi(argv[argi++]);
    if (argc > argi) results_filepath  = std::string(argv[argi++]);
    if (argc > argi) sampling_rate     = atoi(argv[argi++]);

    std::vector<size_t> pars;
    std::stringstream ss(pipe_pars);
    for (size_t i; ss >> i;) {
        pars.push_back(i);
        if (ss.peek() == ',')
            ss.ignore();
    }
    if (pars.size() != 4) {
        std::cout << "ERROR: `pipe_pars` is not provided properly!\n";
        exit(-1);
    }
    source_par = pars[0];
    average_calculator_par = pars[1];
    spike_detector_par = pars[2];
    sink_par = pars[3];

    // parsing `transfer_type_str`
    if (transfer_type_str.compare("copy") == 0)   transfer_type = FPipeTransfer::COPY;
    if (transfer_type_str.compare("shared") == 0) transfer_type = FPipeTransfer::SHARED;
    if (transfer_type_str.compare("hybrid") == 0) transfer_type = FPipeTransfer::HYBRID;

    if (sampling_rate == 0) sampling_rate = 1;

    aocx_filepath = get_aocx_filepath(pars, transfer_type);

    std::cout << COUT_HEADER << "platform_id: "       << COUT_INTEGER << platform_id       << '\n'
              << COUT_HEADER << "device_id: "         << COUT_INTEGER << device_id         << '\n'
              << COUT_HEADER << "aocx_filepath: "     << aocx_filepath                     << '\n'
              << COUT_HEADER << "source_buffers: "    << COUT_INTEGER << source_buffers    << '\n'
              << COUT_HEADER << "source_batch_size: " << COUT_INTEGER << source_batch_size << '\n'
              << COUT_HEADER << "sink_buffers: "      << COUT_INTEGER << sink_buffers      << '\n'
              << COUT_HEADER << "sink_batch_size: "   << COUT_INTEGER << sink_batch_size   << '\n'
              << COUT_HEADER << "dataset_filepath: "  << dataset_filepath                  << '\n'
              << COUT_HEADER << "transfer_type_str: " << transfer_type_str                 << '\n'
              << COUT_HEADER << "app_run_time_s: "    << COUT_INTEGER << app_run_time_s    << '\n'
              << COUT_HEADER << "results_filepath: "  << results_filepath                  << '\n'
              << COUT_HEADER << "sampling_rate: "     << COUT_INTEGER << sampling_rate     << '\n'
              << std::endl;

    // OpenCL init
    OCL ocl;
    ocl.init(aocx_filepath, platform_id, device_id, true);

    std::cout << "Device Temperature: " << clGetTemperature(ocl.device) << " degrees C" << std::endl;

    FPipeGraph<input_t, tuple_t> pipe(ocl, transfer_type, pars, source_batch_size, source_buffers, sink_batch_size, sink_buffers);

    std::vector<input_t> dataset = get_dataset<input_t>(dataset_filepath, TEMPERATURE);
    std::cout << dataset.size() << " tuples loaded!" << std::endl;

    uint64_t app_run_time_ns = app_run_time_s * uint64_t(1000000000);
    pipe.start();
    volatile uint64_t app_start_time_ns = current_time_ns();

    std::vector<std::thread> source_threads(source_par);
    for (size_t i = 0; i < source_par; ++i) {
        source_threads[i] = std::thread(source_thread<input_t, tuple_t>,
                                        std::ref(pipe),
                                        std::ref(dataset),
                                        source_batch_size,
                                        app_start_time_ns,
                                        app_run_time_ns,
                                        i);
    }

    std::vector<sink_batch> results;
    results.reserve(1 << 16);

    std::vector<std::thread> sink_threads(sink_par);
    for (size_t i = 0; i < sink_par; ++i) {
        sink_threads[i] = std::thread(sink_thread<input_t, tuple_t>,
                                      std::ref(pipe),
                                      std::ref(results),
                                      sink_batch_size,
                                      app_start_time_ns,
                                      sampling_rate,
                                      i);
    }

    pipe.wait_and_stop();
    
    for (size_t i = 0; i < source_par; ++i) {
        source_threads[i].join();
    }
    for (size_t i = 0; i < sink_par; ++i) {
        sink_threads[i].join();
    }


#ifdef MEASURE_LATENCY
    // computing latency for each tuple
    util::Sampler latency_sampler(0);
    if (results.size() > 0) { 
        for (sink_batch & b : results) {
            latency_sampler.add(b.sink_timestamp - b.source_timestamp);
        }
    }
    util::metric_group.add("latency_ns", latency_sampler);
#endif

    double elapsed_time_ms_pipe = pipe.service_time_ms();
    double elapsed_time_s_pipe = pipe.service_time_s();
    double throughput = sent_tuples / elapsed_time_s_pipe;
    double bandwidth_source = throughput * sizeof(input_t);
    double bandwidth_sink = (received_tuples * sizeof(tuple_t)) / elapsed_time_s_pipe;
    double bandwidth = bandwidth_source + bandwidth_sink;
    double drop_ratio = 1.0 - (received_tuples / double(sent_tuples));
    // Print results
    std::cout << COUT_HEADER << "Elapsed Time (pipe): " << COUT_FLOAT   << elapsed_time_ms_pipe         << " ms\n"
              << COUT_HEADER << "Sent Tuples: "         << COUT_INTEGER << sent_tuples                  << " tuples\n"
              << COUT_HEADER << "Received Tuples: "     << COUT_INTEGER << received_tuples              << " tuples\n"
              << COUT_HEADER << "Sent Batches: "        << COUT_INTEGER << sent_batches                 << " batches\n"
              << COUT_HEADER << "Received Batches: "    << COUT_INTEGER << received_batches             << " batches\n"
              << COUT_HEADER << "Throughput: "          << COUT_INTEGER << (uint64_t)throughput         << " tuples/s\n"
              << COUT_HEADER << "Bandwidth Source(s): " << COUT_FLOAT   << bandwidth_source / (1 << 20) << " MiB/s\n"
              << COUT_HEADER << "Bandwidth Sink(s): "   << COUT_FLOAT   << bandwidth_sink / (1 << 20)   << " MiB/s\n"
              << COUT_HEADER << "Bandwidth: "           << COUT_FLOAT   << bandwidth / (1 << 20)        << " MiB/s\n"
              << COUT_HEADER << "Drop Ratio: "          << COUT_FLOAT   << drop_ratio                   << "\n"
              << std::endl;

    if (!results_filepath.empty()) {

#ifdef MEASURE_LATENCY
        auto latency = util::metric_group.get_metric("latency_ns");
#endif

        char temp[256];
        std::string current_path = (getcwd(temp, sizeof(temp)) ? std::string( temp ) : std::string("None")) + program_name;


        const char delim = '\t';
        bool write_header_flag = (!fileExists(results_filepath));
        std::ofstream outfile;
        outfile.open(results_filepath, std::ios_base::app);
        if (outfile) {
            if (write_header_flag) {

                outfile << "path"               << delim
                    << "source_par"             << delim
                    << "average_calculator_par" << delim
                    << "spike_detector_par"     << delim
                    << "sink_par"               << delim
                    << "transfer_type"          << delim
                    << "source_buffers"         << delim
                    << "source_batch_size"      << delim
                    << "sink_buffers"           << delim
                    << "sink_batch_size"        << delim
                    << "time_ms"                << delim
                    << "throughput"             << delim
                    << "source_bandwidth"       << delim
                    << "sink_bandwidth"         << delim
                    << "total_bandwidth"        << delim
                    << "drop_ratio"             << delim
#ifdef MEASURE_LATENCY
                    << "latency_samples"        << delim
                    << "latency_mean"           << delim
                    << "latency_p05"            << delim
                    << "latency_p25"            << delim
                    << "latency_p50"            << delim
                    << "latency_p75"            << delim
                    << "latency_p95"
#endif
                    << '\n';
            }

            outfile << current_path                         << delim
                    << source_par                           << delim
                    << average_calculator_par               << delim
                    << spike_detector_par                   << delim
                    << sink_par                             << delim
                    << transfer_type_str                    << delim
                    << COUT_INTEGER << source_buffers       << delim
                    << COUT_INTEGER << source_batch_size    << delim
                    << COUT_INTEGER << sink_buffers         << delim
                    << COUT_INTEGER << sink_batch_size      << delim
                    << COUT_FLOAT   << elapsed_time_ms_pipe << delim
                    << COUT_INTEGER << (uint64_t)throughput << delim
                    << COUT_FLOAT   << bandwidth_source     << delim
                    << COUT_FLOAT   << bandwidth_sink       << delim
                    << COUT_FLOAT   << bandwidth            << delim
                    << COUT_FLOAT   << drop_ratio           << delim;
#ifdef MEASURE_LATENCY
            outfile << COUT_INTEGER << latency.getSamplesSize()    << delim
                    << COUT_INTEGER << (uint64_t)latency.getMean() << delim;
            for (auto p : {0.05, 0.25, 0.5, 0.75, 0.95}) {
                outfile << COUT_INTEGER << (uint64_t)latency.getPercentile(p) << delim;
            }
#endif
            outfile << '\n';
        }

        outfile.close();
    }

    pipe.clean();
    ocl.clean();

    return 0;
}
