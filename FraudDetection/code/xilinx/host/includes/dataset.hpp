#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <unordered_map>

#include "constants.hpp"
#include "tuple_t.hpp"

using key_map_t = std::unordered_map<std::string, size_t>;

// global variables
key_map_t entity_key_map; // contains a mapping between string keys and integer keys for each entity_id
std::vector<std::string> states;

uint32_t get_num_states()
{
    return states.size();
}

uint32_t get_state_id(const std::string val)
{
    for (uint32_t i = 0; i < states.size(); ++i) {
        if (states[i].compare(val) == 0) {
            return i;
        }
    }
    return states.size();
}

template <typename T>
std::vector<T> get_model(const std::string & model_filepath)
{
    size_t num_states = 0;
    std::vector<T>  state_trans_prob;

    std::ifstream file(model_filepath);
    if (file.is_open()) {
        std::string line;
        int line_count = 0;
        int row = 0;
        while (getline(file, line)) {
            if (line_count == 0) {
                std::istringstream iss(line);
                std::string token;
                while (getline(iss, token, ',')) {
                    states.insert(states.end(), token);
                }
                num_states = states.size();

                // initialize the one step state transition probability matrix
                state_trans_prob.reserve(num_states * num_states);
            } else {
                // read the states transition probabilities
                std::istringstream iss(line);
                std::string token;
                std::vector<std::string> prob_row;
                while (getline(iss, token, ',')) {
                    prob_row.push_back(token);
                }
                if (prob_row.size() != num_states) {
                    std::cerr << "[MarkovModel] Error defining the state transition probability matrix" << std::endl;
                    exit(-1);
                }
                // fill the one step state transition probability matrix
                for (size_t p = 0; p < num_states; p++) {
                    state_trans_prob.push_back(atof((prob_row.at(p)).c_str()));
                }
                row++;
            }
            line_count++;
        }
        file.close();
    }

    return state_trans_prob;
}

std::vector<std::pair<std::string, std::string>> map_and_parse_dataset(const std::string & dataset_filepath)
{
    size_t entity_unique_key = 0;
    std::vector<std::pair<std::string, std::string>> parsed_file;

    std::ifstream file(dataset_filepath);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string item;
            while (std::getline(iss, item, ',')) {
                tokens.push_back(item);
            }

            std::string entity_id = tokens[0];
            std::string record = tokens[2];
            // map keys
            if (entity_key_map.find(entity_id) == entity_key_map.end()) { // the key is not present in the hash map
                entity_key_map[entity_id] = entity_unique_key++;
            }
            // save parsed file
            parsed_file.push_back(make_pair(entity_id, record));
        }
        file.close();
    }

    std::cout << "Unique keys: " << entity_unique_key << std::endl;

    return parsed_file;
}

template <typename T>
std::vector<T, fx::aligned_allocator<T>> get_dataset(std::string dataset_filepath, int num_keys)
{
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, num_keys - 1);
    std::mt19937 rng;
    rng.seed(0);

    std::vector<std::pair<std::string, std::string>> parsed_file = map_and_parse_dataset(dataset_filepath);
    std::vector<T, fx::aligned_allocator<T>> dataset;

    for (size_t i = 0; i < parsed_file.size() / 4; ++i) {
        // create tuple
        std::string entity_id = parsed_file[i].first;
        std::string record = parsed_file[i].second;

        T t;
        t.key = (num_keys == 0 ? entity_key_map[entity_id] : dist(rng));
        t.state_id = get_state_id(record);
        t.score = 0;
        t.timestamp = 0;
        dataset.push_back(t);
    }

    return dataset;
}