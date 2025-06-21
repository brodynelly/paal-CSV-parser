#pragma once

#include <string>
#include <mongocxx/collection.hpp>
#include "ThreadPool.h"

// Function declaration
void watch_directory(const std::string& directory_path,
                     ThreadPool& pool,
                     const mongocxx::collection& pigs_collection,
                     const mongocxx::collection& posture_collection);

