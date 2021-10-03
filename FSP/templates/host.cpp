{%- set source_data_type = source.i_datatype -%}
{%- set sink_data_type = sink.o_datatype -%}
{%- set bandwidth_data_type = source.i_datatype if source else 'float' %}
{%- set shared_memory = true if transfer_mode == transferMode.SHARED else false %}
#include <iostream>
#include <vector>
#include <thread>
#include <unistd.h>

#include <deque>
#include <unordered_map>

#include "pipe.hpp"

{% if source %}
std::vector<{{ source_data_type }}> load_dataset(const std::string & dataset_path,
                                 const size_t size)
{
    std::vector<{{ source_data_type }}> data;
    data.reserve(size);

    std::ifstream file(dataset_path);
    if (file.is_open()) {

        size_t n = 0;

        uint key;
        float value;
        while (n < size and file >> key && file >> value) {
            data.push_back({{ source_data_type }}{key, value});
            n++;
        }

        file.close();

    } else {
        abort();
    }

    std::vector<{{ source_data_type }}> dataset;
    dataset.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        dataset.push_back(data[i % data.size()]);
    }

    return dataset;
}

// std::vector<{{ source_data_type }}> load_dataset(const std::string & dataset_path,
//                                  const size_t size)
// {
//     // TODO: complete
//     exit(1);
// }
{% endif %}

{% if sink %}
bool check_results(const std::vector<input_t> & dataset,
                   const std::vector<tuple_t> results,
                   const size_t number_of_batches)
{
    const size_t win_size = 16;
    std::unordered_map<uint, std::deque<double>> win_values;
    std::unordered_map<uint, double> win_results;

    std::vector< std::pair<bool, tuple_t> > calculated_results;
    calculated_results.reserve(results.size());

    for (size_t i = 0; i < number_of_batches; ++i) {

        for (const auto & d : dataset) {

            const auto device_id = d.device_id;
            const auto temperature = d.temperature;

            double average;

            if (win_values.find(device_id) != win_values.end()) { // device_id key is already present

                // remove the oldest value from window
                if (win_values.at(device_id).size() > win_size - 1) {            // control window size (number of values stored) for each device_id
                    win_results.at(device_id) -= win_values.at(device_id).at(0); // decrement current window sum
                    win_values.at(device_id).pop_front();                        // remove the older item in the window
                }

                // add the new value to the window
                win_values.at(device_id).push_back(temperature);
                win_results.at(device_id) += temperature;

                average = win_results.at(device_id) / win_values.at(device_id).size();

            } else {    // device_id is not present
                win_values.insert(std::make_pair(device_id, std::deque<double>()));
                win_values.at(device_id).push_back(temperature);
                win_results.insert(std::make_pair(device_id, temperature));

                average = temperature;
            }

            if (fabsf(temperature - average) > (THRESHOLD * average)) {
                calculated_results.push_back(std::make_pair(false, tuple_t{device_id, temperature, average}));
            }
        }
    }

    if (calculated_results.size() != results.size()) {
        std::cout << "ERROR: results differ in number! (" << calculated_results.size() << ", " << results.size() << ")" << std::endl;
        return false;
    }

    // std::cout << "HOST RESULTS" << std::endl;
    // for (const auto a : calculated_results) {
    //     std::cout << "{" << a.second.device_id << ", " << a.second.temperature << ", " << a.second.average << "}\n";
    // }
    // std::cout << std::endl;

    for (const auto & d : results) {

        bool found = false;
        size_t tuple_index = -1;
        tuple_t tuple_found;
        tuple_t c;

        for (size_t i = 0; i < calculated_results.size(); ++i) {
            auto p = calculated_results[i];
            c = p.second;
            if (!p.first and
                c.device_id == d.device_id and
                c.temperature == d.temperature and
                approximatelyEqual(c.average, d.average))
            {
                p.first = true;
                found = true;
                tuple_index = i;
                tuple_found = c;
                break;
            }
        }

        if (!found and tuple_index != -1) {
            std::cout << "ERROR: results are wrong! index: " << tuple_index << "\n"
                      << "{" <<  c.device_id <<  ", " <<  c.temperature <<  ", " << c.average << "}"
                      << " != "
                      << "{" <<  d.device_id <<  ", " <<  d.temperature <<  ", " << d.average << "}"
                      << std::endl;
            return false;
        }
    }

    // for (const auto a : calculated_results) {
    //     if (!a.first) std::cout << "{" << a.second.device_id << ", " << a.second.temperature << ", " << a.second.average << "}\n";
    // }
    // std::cout << std::endl;

    return true;
}

// bool check_results(const std::vector<{{ source_data_type }}> & dataset,
//                    const std::vector<{{ sink_data_type }}> results,
//                    const size_t number_of_batches)
// {
//     // TODO: complete
//     exit(2);
// }
{% endif %}

{% if source %}
void source_thread(FPipeGraph & pipe,
                   std::vector<{{ source_data_type }}> & dataset,
                   size_t number_of_batches,
                   size_t rid)
{
    std::cout << "Source " << rid << " started!\n";
    for (size_t i = 0; i < number_of_batches; ++i) {
        pipe.push(dataset, dataset.size(), rid, i == (number_of_batches - 1));
    }
}
{% endif %}

{% if sink %}
void sink_thread(FPipeGraph & pipe,
                 std::vector<{{ sink_data_type }}> & results,
                 size_t batch_size,
                 size_t rid)
{
    std::cout << "Sink " << rid << " started!\n";

    bool shutdown = false;

    while (!shutdown) {
        cl_uint filled_size = 0;
        std::vector<{{ sink_data_type }}> data = pipe.pop(rid, batch_size, &filled_size, &shutdown);
        if (!shutdown and filled_size <= 0) {
            std::cout << "\n\nSink " << rid << "has not received anything!\n\n";
        } else {
            for (const auto d : data) {
                results.push_back(d);
            }
        }
    }
}
{% endif %}
//TODO: fare in modo di usare la nuova FPIPE per shared memory
int main(int argc, char * argv[])
{
{% if shared_memory %}
    std::string aocx_filename = "device.aocx";
    int platform_id = 1;
    int device_id = 0;
    size_t number_of_batches = 4;
    size_t source_header_stride_exp = 4;
    size_t source_data_stride_exp = 4;
    size_t sink_header_stride_exp = 4;
    size_t sink_data_stride_exp = 4;
    bool checkResults = false;
    std::string dataset_path = "./dataset.dat";
    std::string results_path;

    argc--;
    argv++;

    int argi = 0;
    if (argc > argi) platform_id              = atoi(argv[argi++]);
    if (argc > argi) device_id                = atoi(argv[argi++]);
    if (argc > argi) number_of_batches        = atoi(argv[argi++]);
    if (argc > argi) source_header_stride_exp = atoi(argv[argi++]);
    if (argc > argi) source_data_stride_exp   = atoi(argv[argi++]);
    if (argc > argi) sink_header_stride_exp   = atoi(argv[argi++]);
    if (argc > argi) sink_data_stride_exp     = atoi(argv[argi++]);
    if (argc > argi) checkResults             = atoi(argv[argi++]);
    if (argc > argi) dataset_path             = std::string(argv[argi++]);
    if (argc > argi) results_path             = std::string(argv[argi++]);

    std::cout << COUT_HEADER << "platform_id: "              << COUT_INTEGER << platform_id              << '\n';
    std::cout << COUT_HEADER << "device_id: "                << COUT_INTEGER << device_id                << '\n';
    std::cout << COUT_HEADER << "number_of_batches: "        << COUT_INTEGER << number_of_batches        << '\n';
    std::cout << COUT_HEADER << "source_header_stride_exp: " << COUT_INTEGER << source_header_stride_exp << '\n';
    std::cout << COUT_HEADER << "source_data_stride_exp: "   << COUT_INTEGER << source_data_stride_exp   << '\n';
    std::cout << COUT_HEADER << "sink_header_stride_exp: "   << COUT_INTEGER << sink_header_stride_exp   << '\n';
    std::cout << COUT_HEADER << "sink_data_stride_exp: "     << COUT_INTEGER << sink_data_stride_exp     << '\n';
    std::cout << COUT_HEADER << "check_results: "            << (checkResults ? "True" : "False")        << '\n';
    std::cout << COUT_HEADER << "dataset_path: "             << dataset_path                             << '\n';
    std::cout << COUT_HEADER << "results_path: "             << results_path                             << '\n';

    size_t source_batch_size = (1 << source_data_stride_exp);
    size_t sink_batch_size = (1 << sink_data_stride_exp);

    // OpenCL init
    OCL ocl;
    ocl.init(aocx_filename, platform_id, device_id, true);

    FPipeGraph pipe(ocl, source_header_stride_exp, source_data_stride_exp, sink_header_stride_exp, sink_data_stride_exp);
{% else %}
    std::string aocx_filename = "device.aocx";
    int platform_id = 1;
    int device_id = 0;
    size_t number_of_batches = 4;
    size_t source_batch_size = 1024;
    size_t source_buffers = 2;
    size_t sink_batch_size = 128;
    bool checkResults = false;
    std::string dataset_path = "./dataset.dat";
    std::string results_path;


    argc--;
    argv++;

    int argi = 0;
    if (argc > argi) platform_id       = atoi(argv[argi++]);
    if (argc > argi) device_id         = atoi(argv[argi++]);
    if (argc > argi) number_of_batches = atoi(argv[argi++]);
    if (argc > argi) source_batch_size = atoi(argv[argi++]);
    if (argc > argi) source_buffers    = atoi(argv[argi++]);
    if (argc > argi) sink_batch_size   = atoi(argv[argi++]);
    if (argc > argi) checkResults      = atoi(argv[argi++]);
    if (argc > argi) dataset_path      = std::string(argv[argi++]);
    if (argc > argi) results_path      = std::string(argv[argi++]);



    std::cout << COUT_HEADER << "platform_id: "       << COUT_INTEGER << platform_id       << '\n';
    std::cout << COUT_HEADER << "device_id: "         << COUT_INTEGER << device_id         << '\n';
    std::cout << COUT_HEADER << "number_of_batches: " << COUT_INTEGER << number_of_batches << '\n';
    std::cout << COUT_HEADER << "source_batch_size: " << COUT_INTEGER << source_batch_size << '\n';
    std::cout << COUT_HEADER << "source_buffers: "    << COUT_INTEGER << source_buffers    << '\n';
    std::cout << COUT_HEADER << "sink_batch_size: "   << COUT_INTEGER << sink_batch_size   << '\n';
    std::cout << COUT_HEADER << "check_results: "     << (checkResults ? "True" : "False") << '\n';
    std::cout << COUT_HEADER << "dataset_path: "      << dataset_path                      << '\n';
    std::cout << COUT_HEADER << "results_path: "      << results_path                      << '\n';


    // OpenCL init
    OCL ocl;
    ocl.init(aocx_filename, platform_id, device_id, true);

    FPipeGraph pipe(ocl, source_batch_size, sink_batch_size, source_buffers);
{% endif %}

    // empty data
    {% for n in nodes %}
    {% for b in n.get_global_no_value_buffers() %}
    std::vector<{{ b.datatype }}> {{ b.name }}_data({{ b.size }});
    {% endfor %}
    {% endfor %}

    // prepare all buffers for all internal nodes
    {% for n in nodes %}
    {% for b in n.get_global_no_value_buffers() %}
    {% if b.is_access_single() %}
    for (size_t i = 0; i < {{ n.par }}; ++i) {
        pipe.{{ n.name }}_node.prepare_{{ b.name }}({{ b.name }}_data, i);
    }
    {% else %}
    pipe.{{ n.name }}_node.prepare_{{ b.name }}({{ b.name }}_data);
    {% endif%}
    {% endfor %}
    {% endfor %}

    {% for n in nodes if n.is_generator() %}
    pipe.{{ n.name }}_node.set_size_all(source_batch_size * number_of_batches);
    {% endfor %}

    {% if source %}
    std::vector<{{ source_data_type }}> dataset = load_dataset(dataset_path, source_batch_size);
    {% endif %}

    pipe.start();

    {% if source %}
    std::vector<std::thread> source_threads({{ source.par }});
    for (size_t i = 0; i < {{ source.par }}; ++i) {
        source_threads[i] = std::thread(source_thread, std::ref(pipe), std::ref(dataset), number_of_batches, i);
    }
    {% endif %}

    {% if sink %}
    // gathers from sink
    std::vector<{{ sink_data_type }}> results;

    std::vector<std::thread> sink_threads({{ sink.par }});
    for (size_t i = 0; i < {{ sink.par }}; ++i) {
        sink_threads[i] = std::thread(sink_thread, std::ref(pipe), std::ref(results), sink_batch_size, i);
    }
    {% endif %}

    {% if source %}
    for (size_t i = 0; i < {{ source.par }}; ++i) {
        source_threads[i].join();
    }
    {% endif %}
    {% if sink %}
    for (size_t i = 0; i < {{ sink.par }}; ++i) {
        sink_threads[i].join();
    }
    {% endif %}
    pipe.wait_and_stop();

    {% if sink %}
    if (checkResults) {
        if (check_results(dataset, results, number_of_batches)) {
            std::cout << "CHECK RESULTS: passed!" << std::endl;
        } else {
            std::cout << "ERROR in CHECK RESULTS!" << std::endl;
        }
    }
    {% endif %}

    double elapsed_time = pipe.service_time_ms();
    size_t throughput = (source_batch_size * number_of_batches) / pipe.service_time_s();
    {% if source %}
    double bandwidth_source = (source_batch_size * number_of_batches * sizeof({{ source_data_type }})) / pipe.service_time_s();
    {% else %}
    double bandwidth_source = (source_batch_size * number_of_batches * sizeof(tuple_t)) / pipe.service_time_s();
    {% endif %}
    {% if sink %}
    double bandwidth_sink = (results.size() * sizeof({{ sink_data_type }})) / pipe.service_time_s();
    {% else %}
    double bandwidth_sink = 0.0;
    {% endif %}
    {% if source and sink %}
    double bandwidth = ((source_batch_size * number_of_batches * sizeof({{ source_data_type }})) + (results.size() * sizeof({{sink_data_type}}))) / pipe.service_time_s();
    {% else %}
    double bandwidth = bandwidth_source;
    {% endif %}
    // Print results
    std::cout << COUT_HEADER << "Elapsed Time: "     << COUT_FLOAT   << elapsed_time                 << " ms\n"
              << COUT_HEADER << "Throughput: "       << COUT_INTEGER << throughput                   << " tuples/s\n"
              << COUT_HEADER << "Bandwidth Source: " << COUT_FLOAT   << bandwidth_source / (1 << 20) << " MiB/s\n"
              << COUT_HEADER << "Bandwidth Sink: "   << COUT_FLOAT   << bandwidth_sink / (1 << 20)   << " MiB/s\n"
              << COUT_HEADER << "Bandwidth: "        << COUT_FLOAT   << bandwidth / (1 << 20)        << " MiB/s\n"
              << std::endl;


    {% if shared_memory %}
    // double elapsed_time = pipe.service_time_ms();
    // size_t throughput = ((1 << source_data_stride_exp) * number_of_batches) / pipe.service_time_s();
    // double bandwidth_source = ((1 << source_data_stride_exp) * number_of_batches * sizeof({{ source_data_type }})) / pipe.service_time_s();
    // double bandwidth_sink = (results.size() * sizeof({{ sink_data_type }})) / pipe.service_time_s();
    // double bandwidth_source = (((1 << source_data_stride_exp) * number_of_batches * sizeof({{ source_data_type }})) + (results.size() * sizeof({{sink_data_type}}))) / pipe.service_time_s();

    // Print results
    // std::cout << COUT_HEADER << "Elapsed Time: " << COUT_FLOAT   << elapsed_time          << " ms\n"
    //           << COUT_HEADER << "Throughput: "   << COUT_INTEGER << throughput            << " tuples/s\n"
    //           << COUT_HEADER << "Bandwidth: "    << COUT_FLOAT   << bandwidth / (1 << 20) << " MiB/s\n"
    //           << std::endl;

    if (!results_path.empty()) {
        char temp[256];
        std::string current_path = getcwd(temp, sizeof(temp)) ? std::string( temp ) : std::string("None");

        const char delim = ';';
        std::ofstream outfile;
        outfile.open(results_path, std::ios_base::app);
        if (outfile) {
            outfile << current_path << delim
                    {% if source %}
                    << pipe.source_node.par                            << delim
                    {% endif %}
                    {% for n in nodes %}
                    << pipe.{{ n.name }}_node.par                      << delim
                    {% endfor %}
                    {% if sink %}
                    << pipe.sink_node.par                              << delim
                    {% endif %}
                    << COUT_FLOAT   << elapsed_time                    << delim
                    << COUT_INTEGER << throughput                      << delim
                    << COUT_FLOAT   << bandwidth_source                << delim
                    << COUT_FLOAT   << bandwidth_sink                  << delim
                    << COUT_FLOAT   << bandwidth                       << delim
                    << COUT_INTEGER << number_of_batches               << delim
                    << COUT_INTEGER << (1 << source_data_stride_exp)   << delim
                    << COUT_INTEGER << (1 << source_header_stride_exp) << delim
                    << COUT_INTEGER << (1 << sink_data_stride_exp)     << delim
                    << '\n';
        }

        outfile.close();
    }
    {% else %}
    // double elapsed_time = pipe.service_time_ms();
    // size_t throughput = (source_batch_size * number_of_batches) / pipe.service_time_s();
    // double bandwidth_source = (source_batch_size * number_of_batches * sizeof({{ source_data_type }})) / pipe.service_time_s();
    // double bandwidth_sink = (results.size() * sizeof({{ sink_data_type }})) / pipe.service_time_s();
    // double bandwidth_source = ((source_batch_size * number_of_batches * sizeof({{ source_data_type }})) + (results.size() * sizeof({{sink_data_type}}))) / pipe.service_time_s();
    // // Print results
    // std::cout << COUT_HEADER << "Elapsed Time: "     << COUT_FLOAT   << elapsed_time                 << " ms\n"
    //           << COUT_HEADER << "Throughput: "       << COUT_INTEGER << throughput                   << " tuples/s\n"
    //           << COUT_HEADER << "Bandwidth Source: " << COUT_FLOAT   << bandwidth_source / (1 << 20) << " MiB/s\n"
    //           << COUT_HEADER << "Bandwidth Sink: "   << COUT_FLOAT   << bandwidth_sink / (1 << 20)   << " MiB/s\n"
    //           << COUT_HEADER << "Bandwidth: "        << COUT_FLOAT   << bandwidth / (1 << 20)        << " MiB/s\n"
    //           << std::endl;

    if (!results_path.empty()) {

        char temp[256];
        std::string current_path = getcwd(temp, sizeof(temp)) ? std::string( temp ) : std::string("None");

        const char delim = ';';
        std::ofstream outfile;
        outfile.open(results_path, std::ios_base::app);
        if (outfile) {
            outfile << current_path << delim
                    {% if source %}
                    << pipe.source_node.par              << delim
                    {% endif %}
                    {% for n in nodes %}
                    << pipe.{{ n.name }}_node.par        << delim
                    {% endfor %}
                    {% if sink %}
                    << pipe.sink_node.par                << delim
                    {% endif %}
                    << COUT_FLOAT   << elapsed_time      << delim
                    << COUT_INTEGER << throughput        << delim
                    << COUT_FLOAT   << bandwidth_source  << delim
                    << COUT_FLOAT   << bandwidth_sink    << delim
                    << COUT_FLOAT   << bandwidth         << delim
                    << COUT_INTEGER << number_of_batches << delim
                    << COUT_INTEGER << source_batch_size << delim
                    << COUT_INTEGER << source_buffers    << delim
                    << COUT_INTEGER << sink_batch_size   << delim
                    << '\n';
        }

        outfile.close();
    }
    {% endif %}

    pipe.clean();
    ocl.clean();

    return 0;
}