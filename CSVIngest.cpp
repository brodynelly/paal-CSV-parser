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
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

bool upsert_pig(int pig_id, mongocxx::collection& pigs_collection) {
    Pig new_pig(
        pig_id,
        "UNKNOWN_TAG",
        "UNKNOWN_BREED",
        0,
        CurrentLocation("UNKNOWN_FARM", "UNKNOWN_BARN", "UNKNOWN_STALL")
    );

    mongocxx::options::update opts;
    opts.upsert(true);

    auto result = pigs_collection.update_one(
        make_document(kvp("pigId", pig_id)),
        make_document(kvp("$setOnInsert", new_pig.to_bson().view())),
        opts);

    if (result && result->upserted_id()) {
        std::cout << "[🐖 New Pig Registered] Pig ID: " << pig_id << std::endl;
        return true;
    }
    return false;
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
        std::stringstream ss(line);
        std::string timestamp_str;
        std::getline(ss, timestamp_str, '\t');

        std::tm tm = {};

        // Parse timestamp in format YYYY_MM_DD_HH_MM_SS (e.g., 2022_08_22_02_20_00)
        if (timestamp_str.length() == 19 &&
            timestamp_str[4] == '_' && timestamp_str[7] == '_' && timestamp_str[10] == '_' &&
            timestamp_str[13] == '_' && timestamp_str[16] == '_') {

            try {
                tm.tm_year = std::stoi(timestamp_str.substr(0, 4)) - 1900; // Year since 1900
                tm.tm_mon = std::stoi(timestamp_str.substr(5, 2)) - 1;     // Month (0-11)
                tm.tm_mday = std::stoi(timestamp_str.substr(8, 2));         // Day (1-31)
                tm.tm_hour = std::stoi(timestamp_str.substr(11, 2));        // Hour (0-23)
                tm.tm_min = std::stoi(timestamp_str.substr(14, 2));         // Minute (0-59)
                tm.tm_sec = std::stoi(timestamp_str.substr(17, 2));         // Second (0-59)
            } catch (const std::exception& e) {
                std::cerr << "❌ Timestamp parse fail: " << timestamp_str << " - " << e.what() << std::endl;
                continue;
            }
        } else {
            // Try standard format as fallback
            std::istringstream ts_stream(timestamp_str);
            ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

            if (ts_stream.fail()) {
                std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
                continue;
            }
        }

        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        for (size_t i = 0; i < pig_ids.size(); ++i) {
            std::string score_str;
            std::getline(ss, score_str, '\t');

            if (score_str.empty() || pig_ids[i] == -1) continue;

            try {
                int score = std::stoi(score_str);
                int pig_id = pig_ids[i];

                if (checked_pigs.insert(pig_id).second) {
                    if (upsert_pig(pig_id, pigs_collection)) {
                        pigsRegistered++;
                        if (stats) stats->pigsRegistered++;
                    }
                }

                Posture posture(pig_id, tp, score);
                batch.push_back(posture.to_bson());
                recordsInserted++;

                if (batch.size() >= BATCH_SIZE) {
                    posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
                    if (stats) stats->recordsInserted += batch.size();
                    batch.clear();
                }

            } catch (const std::exception& e) {
                std::cerr << "❌ Parse error at pig index " << i << ": " << e.what() << std::endl;
                if (stats) stats->errorCount++;
                continue;
            }
        }
    }

    if (!batch.empty()) {
        posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
        if (stats) stats->recordsInserted += batch.size();
    }

    std::cout << "✅ Total Number of Pigs Found: " << pig_ids.size() << std::endl;
    std::cout << "✅ Total Records Inserted: " << recordsInserted << std::endl;
    std::cout << "✅ New Pigs Registered: " << pigsRegistered << std::endl;
    std::cout << "✅ Finished processing: " << filepath << std::endl;

    if (stats) stats->filesProcessed++;
}
