#pragma once

#include "metric.hpp"
#include "sampler.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <numeric>
// #include "rapidjson/prettywriter.h"
#include <sstream>

// using namespace rapidjson;


namespace util {

class MetricGroup {

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Sampler>> map_;
public:

    Metric get_metric(std::string name) {
        Metric metric(name);

        // consume all the groups
        auto &samplers = map_.at(name);
        while (!samplers.empty()) {
            auto sampler = samplers.back();
            metric.total(sampler.total());

            // add all the values from the sampler
            for (double value : sampler.values()) {
                metric.add(value);
            }

            // discard it
            samplers.pop_back();
        }

        return metric;
    }

    void add(std::string name, Sampler sampler) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &samplers = map_[name];
        samplers.push_back(sampler);
    }

    // XXX this consumes the groups
    void dump_all() {
        for (auto &it : map_) {
            Metric metric = get_metric(it.first);
            metric.dump();
        }
    }
};

MetricGroup metric_group;
extern MetricGroup metric_group;

}