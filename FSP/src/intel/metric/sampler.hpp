#pragma once

#include <sys/time.h>
#include <vector>
#include <iostream>

namespace util {

class Sampler {

private:

    const int64_t samples_per_second_;
    uint64_t epoch_;
    int64_t counter_;
    int64_t total_;
    std::vector<double> samples_;

    uint64_t _current_time_ns() {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        return (t.tv_sec) * uint64_t(1000000000) + t.tv_nsec;
    }

public:

    Sampler(int64_t samples_per_second = 0)
    : samples_per_second_(samples_per_second)
    , epoch_(_current_time_ns())
    , counter_(0)
    , total_(0)
    {}

    void add(double value, uint64_t timestamp = 0) {
        ++total_;

        auto seconds = (timestamp - epoch_) / 1e9;
        if (samples_per_second_ == 0 || counter_ <= samples_per_second_ * seconds) {
            samples_.push_back(value);
            ++counter_;
        }
    }

    const std::vector<double> & values() const { return samples_; }

    int64_t total() const { return total_; }
};

}
