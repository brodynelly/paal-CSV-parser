#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>

#include "ThreadPool.h"
#include "FileWatcher.h" 
#include "CSVIngest.h" 

namespace fs = std::filesystem;

int main() {
    try {
        // Initialize MongoDB driver instance
        mongocxx::instance instance{};

        // Define custom connection URI
        const std::string mongo_uri_str = "mongodb://PAAL:PAAL@localhost:27017/?directConnection=true";
        mongocxx::uri uri(mongo_uri_str);

        // Connect to MongoDB with explicit URI
        mongocxx::client client(uri);

        // Access pig_data database and collections
        mongocxx::database pig_db = client["paalab"];
        auto pigs_col = pig_db["pigs"];
        auto posture_col = pig_db["pigpostures"];

        // Create thread pool
        ThreadPool pool(std::thread::hardware_concurrency());

        // Create watch folder if missing
        std::string folder_path = "cold_folder";
        if (!fs::exists(folder_path)) {
            fs::create_directory(folder_path);
        }

        std::cout << "✅ Connected to MongoDB and watching folder: " << folder_path << std::endl;

        // Start watching the directory
        watch_directory(folder_path, pool, pigs_col, posture_col);
    }
    catch (const mongocxx::exception& e) {
        std::cerr << "❌ MongoDB Connection Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
