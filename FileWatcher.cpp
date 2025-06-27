#include "FileWatcher.h"
#include "CSVIngest.h"
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>


// This function is kept for backward compatibility
// The Application class now handles file watching with more features
void watch_directory(const std::string& dir_path, ThreadPool& pool,
                     mongocxx::collection pigs_collection,
                     mongocxx::collection posture_collection) {
    namespace fs = std::filesystem;
    std::unordered_map<std::string, fs::file_time_type> seen;

    std::cout << "⚠️ Warning: Using legacy file watching method.\n";
    std::cout << "   Consider using the new Application class for enhanced features.\n\n";

    while (true) {
        // Clean out records for files that have been removed
        for (auto it = seen.begin(); it != seen.end();) {
            if (!fs::exists(it->first)) {
                it = seen.erase(it);
            } else {
                ++it;
            }
        }

        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.path().extension() == ".csv") {
                std::string filepath = entry.path().string();
                auto modTime = fs::last_write_time(entry);
                auto it = seen.find(filepath);
                if (it == seen.end() || it->second != modTime) {
                    seen[filepath] = modTime;
                    pool.enqueue([filepath, pigs_collection, posture_collection]() {
                        // Using default values for stats and batchSize
                        parse_and_batch_insert(filepath, pigs_collection, posture_collection, nullptr, 1000);
                    });
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}