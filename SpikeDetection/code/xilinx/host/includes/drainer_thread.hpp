#ifndef __STREAM_DRAINER_THREAD_HPP__
#define __STREAM_DRAINER_THREAD_HPP__

#include <atomic>
#include <vector>
#include <pthread.h>

#include "fspx_host.hpp"

extern pthread_barrier_t barrier_setup;
extern pthread_barrier_t barrier_start;
extern pthread_barrier_t barrier_end;

extern std::atomic<size_t> tuples_received;
extern std::atomic<size_t> batches_received;


template <typename T>
void stream_drainer_thread(
    const size_t idx,
    fx::OCL & ocl,
    const size_t num_batches,
    const size_t batch_size,
    const uint64_t & app_start_time
)
{
    fx::StreamDrainer<T> stream_drainer(ocl, batch_size, num_batches, idx);
    size_t _tuples_received = 0;
    size_t _batch_received = 0;

    fx::Sampler latency_sampler(1024);

    stream_drainer.prelaunch();
    pthread_barrier_wait(&barrier_setup);
    pthread_barrier_wait(&barrier_start);

    bool done = false;
    while(!done) {
        size_t items_written = 0;
        T * batch = stream_drainer.pop(&items_written, &done);

        #if MEASURE_LATENCY
        const uint64_t current_time = fx::current_time_nsecs();
        for (size_t i = 0; i < items_written; ++i) {
            const double latency = (current_time - app_start_time) - batch[i].timestamp * uint64_t(100);
            latency_sampler.add(latency, current_time);
        }
        #else
        (void)app_start_time;
        #endif

        stream_drainer.put_batch(batch, batch_size);
        _tuples_received += items_written;
        _batch_received++;
    }
    stream_drainer.finish();
    pthread_barrier_wait(&barrier_end);

    tuples_received += _tuples_received;
    batches_received += _batch_received;
    fx::metric_group.add("latency", latency_sampler);
}

#endif // __STREAM_DRAINER_THREAD_HPP__