#ifndef __STREAM_GENERATOR_THREAD_HPP__
#define __STREAM_GENERATOR_THREAD_HPP__

#include <atomic>
#include <vector>
#include <pthread.h>

#include "fspx_host.hpp"

extern pthread_barrier_t barrier_setup;
extern pthread_barrier_t barrier_start;
extern pthread_barrier_t barrier_end;

extern std::atomic<size_t> tuples_sent;
extern std::atomic<size_t> batches_sent;


bool update_done(const uint64_t app_start_time,
                 const uint64_t app_run_time,
                 const uint64_t current_time = fx::current_time_nsecs())
{
    return ((current_time - app_start_time) > app_run_time);
}

template <typename T>
void stream_generator_thread(
    const size_t idx,
    fx::OCL & ocl,
    const std::vector<T, fx::aligned_allocator<T> > & dataset,
    const size_t num_batches,
    const size_t batch_size,
    const uint64_t & app_start_time,
    const uint64_t app_run_time
)
{
    std::vector<T, fx::aligned_allocator<T> > _dataset = dataset;
    size_t _tuples_sent = 0;
    size_t _batch_sent = 0;

    fx::StreamGenerator<T> stream_generator(ocl, batch_size, num_batches, idx);

    pthread_barrier_wait(&barrier_setup);
    pthread_barrier_wait(&barrier_start);

    size_t next_tuple_idx = 0;
    bool done = update_done(app_start_time, app_run_time);
    while (!done) {
        T * batch = stream_generator.get_batch();

        #if MEASURE_LATENCY == 1
        // 32bit timestamp with resolution of 100ns relative to app_start_time
        // this timestamp should be enough to store 2^32 / (10^9 / 100) = ~429 seconds
        // const uint32_t timestamp = static_cast<uint32_t>((fx::current_time_nsecs() - app_start_time) / uint64_t(100));

        // generate 32bit timestamp with resolution of 16ns relative to app_start_time
        // this timestamp should be enough to store 2^32 / (10^9 / 16) = ~68.72 seconds
        const uint32_t timestamp = static_cast<uint32_t>((fx::current_time_nsecs() - app_start_time) / uint64_t(16));
        #endif

        for (size_t i = 0; i < batch_size; ++i) {
            T t = dataset[next_tuple_idx];
            #if MEASURE_LATENCY == 1
            t.timestamp = timestamp;
            #endif
            batch[i] = t;
            next_tuple_idx = ((next_tuple_idx + 1) == dataset.size() ? 0 : next_tuple_idx + 1);
        }

        done = update_done(app_start_time, app_run_time);
        stream_generator.push(batch, batch_size, done);
        _tuples_sent += batch_size;
        _batch_sent++;
    }
    stream_generator.finish();
    pthread_barrier_wait(&barrier_end);

    tuples_sent += _tuples_sent;
    batches_sent += _batch_sent;
}

#endif // __STREAM_GENERATOR_THREAD_HPP__