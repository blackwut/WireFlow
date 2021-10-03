#pragma once

#include <iostream>
#include <iomanip>

#include "opencl.hpp"
#include "common.hpp"
#include "utils.hpp"

struct Node
{
    std::string basename;
    size_t parallelism;

    Node(std::string basename, size_t parallelism)
    : basename(basename)
    , parallelism(parallelism)
    {}
};

struct OCL
{
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_program program;

    void init(const std::string filename, int platform_id = -1, int device_id = -1, bool show_build_time = false) {
        platform = (platform_id < 0) ? clPromptPlatform() : clSelectPlatform(platform_id);
        device = (device_id < 0) ? clPromptDevice(platform) : clSelectDevice(platform, device_id);
        context = clCreateContextFor(platform, device);

        volatile cl_ulong time_start = current_time_ns();
        program = clCreateBuildProgramFromBinary(context, device, filename);
        volatile cl_ulong time_end = current_time_ns();

        if (show_build_time) {
            const double time_total = (time_end - time_start) * 1.0e-9;
            std::cout << COUT_HEADER << "OpenCL init took: "
                      << COUT_FLOAT << time_total << " ms"
                      << std::endl;
        }
    }

    cl_command_queue createCommandQueue() {
        cl_int status;
        cl_command_queue queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
        clCheckErrorMsg(status, "Failed to create command queue");
        return queue;
    }

    cl_kernel createKernel(const std::string & kernel_name) {
        cl_int status;
        cl_kernel kernel = clCreateKernel(program, kernel_name.c_str(), &status);
        clCheckErrorMsg(status, "Failed to create kernel");
        return kernel;
    }

    std::vector<cl_kernel> createKernels(const std::vector<Node> nodes)
    {
        std::vector<cl_kernel> kernels;

        for (const Node & k : nodes) {
            for (size_t i = 0; i < k.parallelism; ++i) {
                kernels.push_back(createKernel(k.basename + std::to_string(i)));
            }
        }
        return kernels;
    }

    std::vector<cl_command_queue> getCommandQueues(const size_t n)
    {
        std::vector<cl_command_queue> queues(n);
        for (size_t i = 0; i < n; ++i) {
            queues[i] = createCommandQueue();
        }
        return queues;
    }

    void clean() {
        if (program) clReleaseProgram(program);
        if (context) clReleaseContext(context);
    }
};