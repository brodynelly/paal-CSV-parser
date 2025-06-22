#include "CSVIngest.h"
#include "Pig.h"
#include "PigPosture.h"
#include "Application.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <stdexcept>

constexpr std::size_t TIMESTAMP_LENGTH = 19;

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

void parse_and_batch_insert(const std::string& filepath,
    mongocxx::collection pigs_collection,
    mongocxx::collection posture_collection,
    ProcessingStats* stats,
    int batchSize) {

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open CSV: " << filepath << std::endl;
        if (stats) stats->errorCount++;
        return;
    }

    std::string header;
    if (!std::getline(file, header) || header.empty()) {
        std::cerr << "⚠️ CSV file has no header or is empty: " << filepath << std::endl;
        if (stats) stats->errorCount++;
        return;
    }

    std::stringstream header_ss(header);
    std::vector<int> pig_ids;
    std::string cell;

    std::getline(header_ss, cell, '\t'); // Skip "Timestamp"
    while (std::getline(header_ss, cell, '\t')) {
        if (cell.rfind("ID_", 0) == 0) {
            try {
                int id = std::stoi(cell.substr(3));
                pig_ids.push_back(id);
            } catch (...) {
                std::cerr << "⚠️ Failed to parse pig ID from header: " << cell << std::endl;
                pig_ids.push_back(-1); // placeholder for tracking bad header
            }
        } else {
            std::cerr << "⚠️ Unexpected header column: " << cell << " - skipping" << std::endl;
            pig_ids.push_back(-1); // maintain alignment for data rows
        }
    }

    std::unordered_set<int> checked_pigs;
    std::vector<bsoncxx::document::value> batch;
    const size_t BATCH_SIZE = batchSize > 0 ? batchSize : 1000;

    // Helper lambda to flush a batch safely by inserting documents one by one
    auto flush_batch = [&]() {
        for (auto& doc : batch) {
            try {
                posture_collection.insert_one(doc.view());
                if (stats) stats->recordsInserted++;
            } catch (const std::exception& e) {
                std::cerr << "❌ Failed to insert record during flush: " << e.what() << std::endl;
                if (stats) stats->errorCount++;
            }
        }
        batch.clear();
    };

    // Track statistics
    int recordsInserted = 0;
    int pigsRegistered = 0;

    std::string line;
    bool dataRowsFound = false;
    while (std::getline(file, line)) {
        dataRowsFound = true;
        std::stringstream ss(line);
        std::string timestamp_str;
        std::getline(ss, timestamp_str, '\t');

        // Collect all score fields first to validate field count
        std::vector<std::string> score_fields;
        std::string field;
        while (std::getline(ss, field, '\t')) {
            score_fields.push_back(field);
        }

        if (score_fields.size() != pig_ids.size()) {
            std::cerr << "⚠️ Malformed line: expected " << pig_ids.size()
                      << " fields but found " << score_fields.size()
                      << ". Skipping line." << std::endl;
            if (stats) stats->errorCount++;
            continue;
        }

        std::tm tm = {};

        // Parse timestamp using explicit format definitions
        std::istringstream ts_stream(timestamp_str);
        if (timestamp_str.length() == TIMESTAMP_LENGTH) {
            ts_stream >> std::get_time(&tm, "%Y_%m_%d_%H_%M_%S");
        } else {
            ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        }
        if (ts_stream.fail()) {
            std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
            if (stats) stats->errorCount++;
            continue;
        }

        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        for (size_t i = 0; i < pig_ids.size(); ++i) {
            const std::string& score_str = score_fields[i];

            if (score_str.empty() || pig_ids[i] == -1) continue;

            try {
                int score = std::stoi(score_str);
                int pig_id = pig_ids[i];

                if (checked_pigs.find(pig_id) == checked_pigs.end()) {
                    auto result = pigs_collection.find_one(document{} << "pigId" << pig_id << finalize);
                    if (!result) {
                        Pig new_pig(pig_id);
                        try {
                            pigs_collection.insert_one(new_pig.to_bson().view());
                            std::cout << "🆕 [Pig Created] pigId: " << pig_id << std::endl;
                            pigsRegistered++;
                            if (stats) stats->pigsRegistered++;
                        } catch (const std::exception& e) {
                            flush_batch();
                            throw std::runtime_error(std::string("Failed to insert pig ") + std::to_string(pig_id) + ": " + e.what());
                        }
                    }
                    checked_pigs.insert(pig_id);
                }

                Posture posture(pig_id, tp, score);
                batch.push_back(posture.to_bson());
                recordsInserted++;

                if (batch.size() >= BATCH_SIZE) {
                    try {
                        posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
                        if (stats) stats->recordsInserted += batch.size();
                        batch.clear();
                    } catch (const std::exception& e) {
                        std::cerr << "❌ Batch insert failed: " << e.what() << ". Attempting to flush." << std::endl;
                        flush_batch();
                        throw std::runtime_error(std::string("Batch insert failed: ") + e.what());
                    }
                }

            } catch (const std::exception& e) {
                std::cerr << "❌ Parse error at pig index " << i << ": " << e.what() << std::endl;
                if (stats) stats->errorCount++;
                continue;
            }
        }
    }

    if (!batch.empty()) {
        try {
            posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
            if (stats) stats->recordsInserted += batch.size();
            batch.clear();
        } catch (const std::exception& e) {
            std::cerr << "❌ Final batch insert failed: " << e.what() << ". Attempting to flush." << std::endl;
            flush_batch();
            throw std::runtime_error(std::string("Final batch insert failed: ") + e.what());
        }
    }

    if (!dataRowsFound) {
        std::cerr << "⚠️ CSV file contains no data rows: " << filepath << std::endl;
        if (stats) stats->errorCount++;
    }

    std::cout << "✅ Total Number of Pigs Found: " << pig_ids.size() << std::endl;
    std::cout << "✅ Total Records Inserted: " << recordsInserted << std::endl;
    std::cout << "✅ New Pigs Registered: " << pigsRegistered << std::endl;
    std::cout << "✅ Finished processing: " << filepath << std::endl;

    if (stats) stats->filesProcessed++;
}
