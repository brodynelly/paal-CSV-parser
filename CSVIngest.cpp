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

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

namespace {

bool parseTimestamp(const std::string& timestamp_str,
                    std::chrono::system_clock::time_point& out) {
    std::tm tm = {};

    if (timestamp_str.length() == 19 &&
        timestamp_str[4] == '_' && timestamp_str[7] == '_' &&
        timestamp_str[10] == '_' && timestamp_str[13] == '_' &&
        timestamp_str[16] == '_') {
        try {
            tm.tm_year = std::stoi(timestamp_str.substr(0, 4)) - 1900;
            tm.tm_mon  = std::stoi(timestamp_str.substr(5, 2)) - 1;
            tm.tm_mday = std::stoi(timestamp_str.substr(8, 2));
            tm.tm_hour = std::stoi(timestamp_str.substr(11, 2));
            tm.tm_min  = std::stoi(timestamp_str.substr(14, 2));
            tm.tm_sec  = std::stoi(timestamp_str.substr(17, 2));
        } catch (const std::exception& e) {
            std::cerr << "❌ Timestamp parse fail: " << timestamp_str
                      << " - " << e.what() << std::endl;
            return false;
        }
    } else {
        std::istringstream ts_stream(timestamp_str);
        ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ts_stream.fail()) {
            std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
            return false;
        }
    }

    out = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return true;
}

bool parseRecordLine(const std::string& line,
                     const std::vector<int>& pig_ids,
                     std::chrono::system_clock::time_point& timestamp,
                     std::vector<std::pair<int, int>>& pigScores,
                     ProcessingStats* stats) {
    std::stringstream ss(line);
    std::string timestamp_str;
    std::getline(ss, timestamp_str, '\t');

    if (!parseTimestamp(timestamp_str, timestamp)) {
        if (stats) stats->errorCount++;
        return false;
    }

    for (size_t i = 0; i < pig_ids.size(); ++i) {
        std::string score_str;
        std::getline(ss, score_str, '\t');

        if (score_str.empty() || pig_ids[i] == -1) {
            continue;
        }

        try {
            int score = std::stoi(score_str);
            pigScores.emplace_back(pig_ids[i], score);
        } catch (const std::exception& e) {
            std::cerr << "❌ Parse error at pig index " << i
                      << ": " << e.what() << std::endl;
            if (stats) stats->errorCount++;
        }
    }

    return true;
}

void insertBatch(std::vector<bsoncxx::document::value>& batch,
                 mongocxx::collection& posture_collection,
                 ProcessingStats* stats) {
    if (batch.empty()) return;

    posture_collection.insert_many(batch,
        mongocxx::options::insert{}.ordered(false));

    if (stats) stats->recordsInserted += batch.size();
    batch.clear();
}

} // namespace

bool pig_exists(int pig_id, mongocxx::collection& pigs_collection) {
    auto result = pigs_collection.find_one(document{} << "pigId" << pig_id << finalize);
    return result ? true : false;
}

void insert_pig_if_needed(int pig_id, mongocxx::collection& pigs_collection) {
    if (!pig_exists(pig_id, pigs_collection)) {
        Pig new_pig(
            pig_id,
            "UNKNOWN_TAG",
            "UNKNOWN_BREED",
            0,
            CurrentLocation("UNKNOWN_FARM", "UNKNOWN_BARN", "UNKNOWN_STALL")
        );

        pigs_collection.insert_one(new_pig.to_bson().view());
        std::cout << "[🐖 New Pig Registered] Pig ID: " << pig_id << std::endl;
    }
}

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
    std::getline(file, header);

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
        }
    }

    std::unordered_set<int> checked_pigs;
    std::vector<bsoncxx::document::value> batch;
    const size_t BATCH_SIZE = batchSize > 0 ? batchSize : 1000;

    // Track statistics
    int recordsInserted = 0;
    int pigsRegistered = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::chrono::system_clock::time_point timestamp;
        std::vector<std::pair<int, int>> pigScores;

        if (!parseRecordLine(line, pig_ids, timestamp, pigScores, stats)) {
            continue;
        }

        for (const auto& [pig_id, score] : pigScores) {
            if (checked_pigs.find(pig_id) == checked_pigs.end()) {
                auto result = pigs_collection.find_one(document{} << "pigId" << pig_id << finalize);
                if (!result) {
                    Pig new_pig(pig_id);
                    pigs_collection.insert_one(new_pig.to_bson().view());
                    std::cout << "🆕 [Pig Created] pigId: " << pig_id << std::endl;
                    pigsRegistered++;
                    if (stats) stats->pigsRegistered++;
                }
                checked_pigs.insert(pig_id);
            }

            Posture posture(pig_id, timestamp, score);
            batch.push_back(posture.to_bson());
            recordsInserted++;

            if (batch.size() >= BATCH_SIZE) {
                insertBatch(batch, posture_collection, stats);
            }
        }
    }

    insertBatch(batch, posture_collection, stats);

    std::cout << "✅ Total Number of Pigs Found: " << pig_ids.size() << std::endl;
    std::cout << "✅ Total Records Inserted: " << recordsInserted << std::endl;
    std::cout << "✅ New Pigs Registered: " << pigsRegistered << std::endl;
    std::cout << "✅ Finished processing: " << filepath << std::endl;

    if (stats) stats->filesProcessed++;
}
