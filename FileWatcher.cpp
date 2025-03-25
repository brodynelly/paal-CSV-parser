#include "FileWatcher.h"
#include "CSVIngest.h" 
#include <filesystem>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>


void watch_directory(const std::string& dir_path, ThreadPool& pool,
                     mongocxx::collection pigs_collection,
                     mongocxx::collection posture_collection) {
    namespace fs = std::filesystem;
    std::unordered_set<std::string> seen;

    while (true) {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.path().extension() == ".csv") {
                std::string filepath = entry.path().string();
                if (seen.find(filepath) == seen.end()) {
                    seen.insert(filepath);
                    pool.enqueue([filepath, pigs_collection, posture_collection]() {
                        parse_and_batch_insert(filepath, pigs_collection, posture_collection);
                    });
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}