
// Parses a CSV file and batch-inserts into MongoDB
#pragma once
#include <string>
#include <mongocxx/collection.hpp>
#include <functional>
#include <atomic>

// Forward declaration of ProcessingStats
struct ProcessingStats;

// Callback function type for progress updates
using ProgressCallback = std::function<void(int, int)>;

void parse_and_batch_insert(
    const std::string& filepath,
    mongocxx::collection pigs_collection,
    mongocxx::collection posture_collection,
    ProcessingStats* stats = nullptr,
    int batchSize = 1000);

