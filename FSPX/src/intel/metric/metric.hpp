#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <numeric>
// #include "rapidjson/prettywriter.h"
#include <sstream>

// using namespace rapidjson;

namespace util {

class Metric {

private:

    std::string name_;
    std::vector<double> samples_;
    int64_t total_;


public:

    Metric(const std::string &name)
    : name_(name)
    {}

    void add(double value) { samples_.push_back(value); }
    void total(int64_t total) { total_ = total; }

    int64_t getTotal() { return total_; }
    std::vector<double> getSamples() { return samples_; }
    uint64_t getSamplesSize() { return samples_.size(); }

    double getMean() {
        return (std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size());
    }

    double getPercentile(double p) {
        auto pointer = samples_.begin() + samples_.size() * p;
        std::nth_element(samples_.begin(), pointer, samples_.end());
        auto value = *pointer;
        return value;
    }

    void dump()
    {
        // StringBuffer buffer;
        // PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        // writer.StartObject();

        // writer.Key("name");
        // writer.String(name_.c_str());

        // writer.Key("samples");
        // writer.Uint(samples_.size());

        // writer.Key("total");
        // writer.Uint(total_);

        // writer.Key("mean");
        // writer.Double(std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size());

        // auto minmax = std::minmax_element(samples_.begin(), samples_.end());
        // double min = *minmax.first;
        // double max = *minmax.second;

        // writer.Key("0");
        // writer.Double(min);

        // // XXX no interpolation since we are dealing with *many* samples

        // // add percentiles
        // for (auto percentile : {0.05, 0.25, 0.5, 0.75, 0.95, 0.99}) {
        //     auto pointer = samples_.begin() + samples_.size() * percentile;
        //     std::nth_element(samples_.begin(), pointer, samples_.end());
        //     auto label = std::to_string(int(percentile * 100));
        //     auto value = *pointer;
        //     writer.Key(label.c_str());
        //     writer.Double(value);
        // }

        // writer.Key("100");
        // writer.Double(max);

        // writer.EndObject();

        // // format the file name
        // std::ostringstream file_name;
        // file_name << "metric_" << name_ << ".json";

        // // serialize the object to file
        // std::ofstream fs(file_name.str());
        // fs << buffer.GetString();
    }
};

}
