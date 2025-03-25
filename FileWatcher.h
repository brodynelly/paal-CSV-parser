#pragma once

#include <string>
#include <mongocxx/collection.hpp>
#include "ThreadPool.h"

// Function declaration
void watch_directory(const std::string& directory_path,
                     ThreadPool& pool,
                     mongocxx::collection pigs_collection,
                     mongocxx::collection posture_collection);

