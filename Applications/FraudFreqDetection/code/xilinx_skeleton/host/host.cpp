#include "fspx_host.hpp"

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>
#include <array>
#include <deque>
#include <string>
#include <chrono>


#include "input_t.hpp"
#include "result_t.hpp"
#include "../../../common/benchmark.hpp"

int main(int argc, char** argv) {

    argc--;
    argv++;

    size_t par = 6;
    uint64_t size = 1 << 30;
    unsigned int max_key = 1 << 10;

    int argi = 0;
    if (argc > argi) par     = std::stoull(argv[argi++]);
    if (argc > argi) size    = std::stoull(argv[argi++]);
    if (argc > argi) max_key = std::stoull(argv[argi++]);

    auto host_overallStart = fx::high_resolution_time();

    std::string bitstreamFilename  = "./kernels/ffds" + std::to_string(par) + std::to_string(par) + std::to_string(par) + std::to_string(par) + "/hw/build/ffds.xclbin";

    fx::OCL ocl = fx::OCL(bitstreamFilename, 0, 0, true);

    // run a kernel and take its time, bandwidth
    cl_command_queue queue = ocl.createCommandQueue(true);
    cl_kernel kernel = ocl.createKernel("compute");
    fx::clCheckError(clSetKernelArg(kernel, 0, sizeof(uint64_t), &size));
    fx::clCheckError(clSetKernelArg(kernel, 1, sizeof(unsigned int), &max_key));

    cl_event event;
    auto host_kernelStart = fx::high_resolution_time();
    clEnqueueTask(queue, kernel, 0, NULL, &event);
    printf("Kernel enqueued\n");
    clFinish(queue);

    auto host_end = fx::high_resolution_time();

    double items = par * size;
    double time_ns = fx::clTimeEventNS(event);
    double time_s = time_ns / 1.0e9;

    double bandwidth_gbs = (double)(items * sizeof(input_t)) / time_ns;
    double tuplePerSecond = (double)(items) / time_s;
    double KTuplesPerSecond = tuplePerSecond / 1000.0;
    double MTuplesPerSecond = tuplePerSecond / 1000000.0;

    std::cout << "Time Host (overall): " << fx::elapsed_time_ms(host_overallStart, host_end) << " ms" << '\n';
    std::cout << "Time Host (kernel): " << fx::elapsed_time_ms(host_kernelStart, host_end) << " ms" << '\n';
    std::cout << "Time: " << time_s << " s" << '\n';
    std::cout << "Bandwidth: " << bandwidth_gbs << " GB/s" << '\n';
    std::cout << "Tuples per second: " << tuplePerSecond << " T/s\n";
    std::cout << "KTuples per second: " << KTuplesPerSecond << " KT/s\n";
    std::cout << "MTuples per second: " << MTuplesPerSecond << " MT/s\n";

    ocl.clean();

    // Fake latency metric to avoid segfault
    fx::Sampler latency_sampler(1);
    fx::metric_group.add("latency", latency_sampler);

    // dump benchmark results
    dump_benchmark(
        "results.csv",
        "FraudFreqDetectionSkeleton",
        bitstreamFilename,
        {"MemoryReader", "Predictor", "Frequency", "MemoryWriter"},
        {par, par, par},
        0,
        0,
        0,
        0,
        0,
        sizeof(input_t),
        items,
        0,
        sizeof(result_t),
        0,
        0,
        fx::elapsed_time_ms(host_overallStart, host_end),
        fx::elapsed_time_ms(host_kernelStart, host_end)
    );

    return 0;
}
