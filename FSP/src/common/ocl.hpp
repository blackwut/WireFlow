#pragma once

#include <iostream>
#include <iomanip>

#include "opencl.hpp"
#include "common.hpp"
#include "utils.hpp"

struct OCL
{
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_program program;

    void init(const std::string & filename,
              int platform_id = -1,
              int device_id = -1,
              bool show_build_time = false)
    {
        std::cout << "Loading " << filename << std::flush;

        platform = (platform_id < 0) ? clPromptPlatform() : clSelectPlatform(platform_id);
        device = (device_id < 0) ? clPromptDevice(platform) : clSelectDevice(platform, device_id);
        context = clCreateContextFor(platform, device);

        volatile cl_ulong time_start = current_time_ns();
        program = clCreateBuildProgramFromBinary(context, device, filename);
        volatile cl_ulong time_end = current_time_ns();

        if (show_build_time) {
            const double time_total = (time_end - time_start) * 1.0e-6;
            std::cout << ": "
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

    void clean() {
        if (program) clReleaseProgram(program);
        if (context) clReleaseContext(context);
    }
};