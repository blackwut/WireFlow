#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <ctime>

#define CL_TARGET_OPENCL_VERSION 200
#include "CL/opencl.h"
#include "CL/cl_ext_intelfpga.h"

/* Help functions */
bool fileExists(const std::string & filename)
{
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// Loads a file in binary form.
unsigned char * loadBinaryFile(const std::string & filename, size_t * size)
{
    FILE * fp = fopen(filename.c_str(),"rb");
    if (fp == NULL) {
        std::cerr << "Cannot open binary file `" << filename << "`." << std::endl;
        *size = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long fs = ftell(fp);
    if (fs < 0) {
        std::cerr << "Binary file empty." << std::endl;
        fclose(fp);
        *size = 0;
        return NULL;
    }

    unsigned char * buffer = new unsigned char[fs];
    rewind(fp);
    if (fread((void *)buffer, fs, 1, fp) == 0) {
        fclose(fp);
        *size = 0;
        delete[] buffer;
        return NULL;
    }

    fclose(fp);

    *size = (size_t)fs;
    return buffer;
}

std::string loadSourceFile(const std::string & filename, size_t * size) {

    std::ifstream f(filename);
    std::string buffer;

    f.seekg(0, std::ios::end);
    buffer.reserve(f.tellg());
    f.seekg(0, std::ios::beg);

    buffer.assign(std::istreambuf_iterator<char>(f), {});
    *size = buffer.size();
    return buffer;
}

bool isEmulator()
{
    char * emu = NULL;
    emu = getenv("CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA");
    return (emu != NULL);
}

void clCallback(const char * errinfo, const void *, size_t, void *)
{
    std::cerr << "Context callback: " << errinfo << std::endl;
}

static inline const char * clErrorToString(cl_int err) {
    switch (err) {
        case CL_SUCCESS                                  : return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND                         : return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE                     : return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE                   : return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE            : return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES                         : return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY                       : return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE             : return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP                         : return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH                    : return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED               : return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE                    : return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE                              : return "CL_MAP_FAILURE";
        case CL_MISALIGNED_SUB_BUFFER_OFFSET             : return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case CL_COMPILE_PROGRAM_FAILURE                  : return "CL_COMPILE_PROGRAM_FAILURE";
        case CL_LINKER_NOT_AVAILABLE                     : return "CL_LINKER_NOT_AVAILABLE";
        case CL_LINK_PROGRAM_FAILURE                     : return "CL_LINK_PROGRAM_FAILURE";
        case CL_DEVICE_PARTITION_FAILED                  : return "CL_DEVICE_PARTITION_FAILED";
        case CL_KERNEL_ARG_INFO_NOT_AVAILABLE            : return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
        case CL_INVALID_VALUE                            : return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE                      : return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM                         : return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE                           : return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT                          : return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES                 : return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE                    : return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR                         : return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT                       : return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR          : return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE                       : return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER                          : return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY                           : return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS                    : return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM                          : return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE               : return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME                      : return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION                : return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL                           : return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX                        : return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE                        : return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE                         : return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS                      : return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION                   : return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE                  : return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE                   : return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET                    : return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST                  : return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT                            : return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION                        : return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT                        : return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE                      : return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL                        : return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE                 : return "CL_INVALID_GLOBAL_WORK_SIZE";
        case CL_INVALID_PROPERTY                         : return "CL_INVALID_PROPERTY";
        case CL_INVALID_IMAGE_DESCRIPTOR                 : return "CL_INVALID_IMAGE_DESCRIPTOR";
        case CL_INVALID_COMPILER_OPTIONS                 : return "CL_INVALID_COMPILER_OPTIONS";
        case CL_INVALID_LINKER_OPTIONS                   : return "CL_INVALID_LINKER_OPTIONS";
        case CL_INVALID_DEVICE_PARTITION_COUNT           : return "CL_INVALID_DEVICE_PARTITION_COUNT";
        default                                          : return "CL_UNKNOWN_ERROR";
    }
}

#define clCheckError(status) _clCheckError(__FILE__, __LINE__, status, #status)
#define clCheckErrorMsg(status, message) _clCheckError(__FILE__, __LINE__, status, message)

void _clCheckError(const char * file, int line,
                   cl_int error, const char * message)
{
    if (error != CL_SUCCESS) {
        std::cerr << "ERROR: " << clErrorToString(error) << "\n"
                  << "Location: " << file << ":" << line << "\n"
                  << "Message: " << message
                  << std::endl;
        exit(error);
    }
}

/* OpenCL Platform */

std::vector<cl_platform_id> clGetPlatforms()
{
    cl_uint num_platforms;
    clCheckError(clGetPlatformIDs(0, NULL, &num_platforms));

    std::vector<cl_platform_id> platforms(num_platforms);
    clCheckError(clGetPlatformIDs(num_platforms, platforms.data(), NULL));

    return platforms;
}

std::string platformInfo(cl_platform_id platform, cl_platform_info info)
{
    size_t size;
    clCheckError(clGetPlatformInfo(platform, info, 0, NULL, &size));
    std::vector<char> param_value(size);
    clCheckError(clGetPlatformInfo(platform, info, size, param_value.data(), NULL));

    return std::string(param_value.begin(), param_value.end());
}

cl_platform_id clSelectPlatform(int selected_platform)
{
    const auto platforms = clGetPlatforms();
    if (selected_platform < 0 or size_t(selected_platform) >= platforms.size()) {
        std::cerr << "ERROR: Platform #" << selected_platform << " does not exist\n";
        exit(-1);
    }

    return platforms[selected_platform];
}

cl_platform_id clPromptPlatform()
{
    const auto platforms = clGetPlatforms();

    int selected_platform = -1;
    while (selected_platform < 0 or size_t(selected_platform) >= platforms.size()) {
        int platform_id = 0;
        for (const auto & p : platforms) {
            std::cout << "#" << platform_id++                  << " "
                      << platformInfo(p, CL_PLATFORM_NAME)     << " "
                      << platformInfo(p, CL_PLATFORM_VENDOR)   << " "
                      << platformInfo(p, CL_PLATFORM_VERSION)
                      << std::endl;
        }

        std::cout << "\nSelect a platform: " << std::flush;
        std::cin >> selected_platform;
        std::cout << std::endl;
    }

    return platforms[selected_platform];
}

/* OpenCL Device */

std::vector<cl_device_id> clGetDevices(cl_platform_id platform,
                                       cl_device_type device_type)
{
    cl_uint num_devices;
    clCheckError(clGetDeviceIDs(platform, device_type, 0, NULL, &num_devices));

    std::vector<cl_device_id> devices(num_devices);
    clCheckError(clGetDeviceIDs(platform, device_type, num_devices, devices.data(), NULL));

    return devices;
}

template <typename T>
T deviceInfo(cl_device_id device, cl_device_info info)
{
    size_t size;
    clCheckError(clGetDeviceInfo(device, info, 0, NULL, &size));

    T param_value;
    clCheckError(clGetDeviceInfo(device, info, size, &param_value, NULL));
    return param_value;
}

template <>
std::string deviceInfo<std::string>(cl_device_id device, cl_device_info info)
{
    size_t size;
    clCheckError(clGetDeviceInfo(device, info, 0, NULL, &size));

    std::vector<char> param_value;
    clCheckError(clGetDeviceInfo(device, info, size, param_value.data(), NULL));
    return std::string(param_value.begin(), param_value.end());
}

cl_device_id clSelectDevice(cl_platform_id platform, int selected_device)
{
    const auto devices = clGetDevices(platform, CL_DEVICE_TYPE_ALL);
    if (selected_device < 0 or size_t(selected_device) >= devices.size()) {
        std::cerr << "ERROR: Device #" << selected_device << "does not exist\n";
        exit(-1);
    }

    return devices[selected_device];
}

cl_device_id clPromptDevice(cl_platform_id platform)
{
    const auto devices = clGetDevices(platform, CL_DEVICE_TYPE_ALL);

    auto deviceTypeName = [](const cl_device_type type) {
        switch (type) {
            case CL_DEVICE_TYPE_CPU:            return "CPU";
            case CL_DEVICE_TYPE_GPU:            return "GPU";
            case CL_DEVICE_TYPE_ACCELERATOR:    return "ACCELERATOR";
            default:                            return "NONE";
        }
    };

    int selected_device = -1;
    while (selected_device < 0 or size_t(selected_device) >= devices.size()) {
        int device_id = 0;
        for (const auto & d : devices) {
            std::cout << "#" << device_id++
                      << " [" << deviceTypeName(deviceInfo<cl_device_type>(d, CL_DEVICE_TYPE)) << "] "
                      // << deviceInfo<std::string>(d, CL_DEVICE_NAME) << "\n"
                      // << "\tVendor:            " <<  deviceInfo<std::string>(d, CL_DEVICE_VENDOR)                 << "\n"
                      // << "\tMax Compute Units: " <<  deviceInfo<cl_uint>(d, CL_DEVICE_MAX_COMPUTE_UNITS)          << "\n"
                      << "\tGlobal Memory:     " << (deviceInfo<cl_ulong>(d, CL_DEVICE_GLOBAL_MEM_SIZE) >> 20)    << " MB\n"
                      // << "\tMax Clock Freq.:   " <<  deviceInfo<cl_uint>(d, CL_DEVICE_MAX_CLOCK_FREQUENCY)        << " MHz\n"
                      // << "\tMax Alloc. Memory: " << (deviceInfo<cl_ulong>(d, CL_DEVICE_MAX_MEM_ALLOC_SIZE) >> 20) << " MB\n"
                      // << "\tLocal Memory:      " << (deviceInfo<cl_ulong>(d, CL_DEVICE_LOCAL_MEM_SIZE) >> 10)     << " KB\n"
                      // << "\tAvailable:         " << (deviceInfo<cl_bool>(d, CL_DEVICE_AVAILABLE) ? "YES" : "NO")
                      << std::endl;
        }
        std::cout << "\nSelect a device: " << std::flush;
        std::cin >> selected_device;
        std::cout << std::endl;
    }

    return devices[selected_device];
}

// TODO: change name
cl_context clCreateContextFor(cl_platform_id platform, cl_device_id device)
{
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platform, 0};

    cl_int status;
    cl_context context = clCreateContext(properties, 1, &device, &clCallback, NULL, &status);
    clCheckErrorMsg(status, "Failed to create context");
    return context;
}

// Create a program for all devices associated with the context.
cl_program clCreateBuildProgramFromBinary(cl_context context, const cl_device_id device, const std::string & filename) {
    // Early exit for potentially the most common way to fail: AOCX does not exist
    if (!fileExists(filename)) {
        std::cerr << "AOCX file '" << filename << "' does not exist.\n";
        clCheckErrorMsg(CL_INVALID_PROGRAM, "Failed to load binary file");
    }

    // Load the binary
    size_t size;
    const unsigned char * binary = loadBinaryFile(filename, &size);
    if (binary == NULL or size == 0) {
        clCheckErrorMsg(CL_INVALID_PROGRAM, "Failed to load binary file");
    }

    cl_int status;
    cl_int binary_status;
    cl_program program = clCreateProgramWithBinary(context, 1, &device, &size, &binary, &binary_status, &status);
    clCheckErrorMsg(status, "Failed to create program with binary");
    clCheckErrorMsg(binary_status, "Failed to load binary for device");

    // It should be safe to delete the binary data
    if (binary != NULL) delete[] binary;

    status = clBuildProgram(program, 1, &device, "", NULL, NULL);
    if (status != CL_SUCCESS) {
        size_t log_size;
        clCheckError(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size));
        std::vector<char> param_value(log_size);
        clCheckError(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, param_value.data(), NULL));
        std::cout << std::string(param_value.begin(), param_value.end()) << std::endl;
        exit(-1);
    } else {
        std::cout << "Program built!" << std::endl;
    }

    return program;
}

cl_program clCreateBuildProgramFromSource(cl_context context, cl_device_id device, const std::string & filename)
{
    if (!fileExists(filename)) {
        std::cerr << ".cl file '" << filename << "' does not exist.\n";
        clCheckErrorMsg(CL_INVALID_PROGRAM, "Failed to load source file");
    }
    
    size_t size;
    auto source = loadSourceFile(filename, &size);
    if (size == 0) {
        clCheckErrorMsg(CL_INVALID_PROGRAM, "Failed to load source file");
    }

    time_t now = time(NULL);
    std::string now_str(ctime(&now));
    source.append("//");
    source.append(now_str);

    cl_int status;
    const char * source_data = source.data();
    cl_program program = clCreateProgramWithSource(context, 1, &source_data, NULL, &status);
    clCheckErrorMsg(status, "Failed to create program with source");

    status = clBuildProgram(program, 1, &device, "-Werror", NULL, NULL);
    if (status != CL_SUCCESS) {
        size_t log_size;
        clCheckError(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size));
        std::vector<char> param_value(log_size);
        clCheckError(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, param_value.data(), NULL));
        std::cout << std::string(param_value.begin(), param_value.end()) << std::endl;
        exit(-1);
    }

    return program;
}

void clWriteAutorunKernelProfilingData(cl_device_id device, cl_program program)
{
    if (!isEmulator()) {
        cl_int status = clGetProfileDataDeviceIntelFPGA(device,     // device_id
                                                        program,    // program
                                                        CL_TRUE,    // read_enqueue_kernels
                                                        CL_TRUE,    // read_auto_enqueued
                                                        CL_TRUE,    // clear_counters_after_readback
                                                        0,          // param_value_size
                                                        NULL,       // param_value
                                                        NULL,       // param_value_size_ret
                                                        NULL);      // errcode_ret
        clCheckErrorMsg(status, "Failed to write Autorun Kernels profiling data");
    }
}

cl_ulong clTimeBetweenEventsNS(cl_event start, cl_event end)
{
    cl_ulong timeStart;
    cl_ulong timeEnd;
    clGetEventProfilingInfo(start, CL_PROFILING_COMMAND_START, sizeof(timeStart), &timeStart, NULL);
    clGetEventProfilingInfo(end, CL_PROFILING_COMMAND_END, sizeof(timeEnd), &timeEnd, NULL);

    return timeEnd - timeStart;
}

double clTimeBetweenEventsMS(cl_event start, cl_event end)
{
    return 1.e-6 * clTimeBetweenEventsNS(start, end);
}

double clTimeEventNS(cl_event event)
{
    return clTimeBetweenEventsNS(event, event);
}

double clTimeEventMS(cl_event event)
{
    return clTimeBetweenEventsMS(event, event);
}
