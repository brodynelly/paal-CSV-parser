#include "CSVIngest.h"
#include "Pig.h"
#include "PigPosture.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

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
    mongocxx::collection posture_collection) {

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open CSV: " << filepath << std::endl;
        return;
    }

    std::string header;
    std::getline(file, header);

    std::stringstream header_ss(header);
    std::vector<int> pig_ids;
    std::string cell;

    std::getline(header_ss, cell, ','); // Skip "Timestamp"
    while (std::getline(header_ss, cell, ',')) {
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
    const size_t BATCH_SIZE = 1000;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string timestamp_str;
        std::getline(ss, timestamp_str, ',');

        std::tm tm = {};
        std::istringstream ts_stream(timestamp_str);
        ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        if (ts_stream.fail()) {
            std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
            continue;
        }

        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        for (size_t i = 0; i < pig_ids.size(); ++i) {
            std::string score_str;
            std::getline(ss, score_str, ',');

            if (score_str.empty() || pig_ids[i] == -1) continue;

            try {
                int score = std::stoi(score_str);
                int pig_id = pig_ids[i];

                if (checked_pigs.find(pig_id) == checked_pigs.end()) {
                    auto result = pigs_collection.find_one(document{} << "pigId" << pig_id << finalize);
                    if (!result) {
                        Pig new_pig(pig_id);
                        pigs_collection.insert_one(new_pig.to_bson().view());
                        std::cout << "🆕 [Pig Created] pigId: " << pig_id << std::endl;
                    }
                    checked_pigs.insert(pig_id);
                }

                Posture posture(pig_id, tp, score);
                batch.push_back(posture.to_bson());

                if (batch.size() >= BATCH_SIZE) {
                    posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
                    batch.clear();
                }

            } catch (const std::exception& e) {
                std::cerr << "❌ Parse error at pig index " << i << ": " << e.what() << std::endl;
                continue;
            }
        }
    }

    if (!batch.empty()) {
        posture_collection.insert_many(batch, mongocxx::options::insert{}.ordered(false));
    }

    std::cout << " Total Amount of Pigs Fount" << pig_ids.size() << std::endl;
    std::cout << "✅ Finished processing: " << filepath << std::endl;
}
