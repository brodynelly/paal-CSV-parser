
// Parses a CSV file and batch-inserts into MongoDB
#pragma once
#include <string>
#include <mongocxx/collection.hpp>

void parse_and_batch_insert(
    const std::string& filepath,
    mongocxx::collection pigs_collection,
    mongocxx::collection posture_collection);

