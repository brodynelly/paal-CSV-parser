#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <unordered_set>

int main() {
    std::string filepath = "build/cold_folder/unknown1.csv";
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open CSV: " << filepath << std::endl;
        return 1;
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
                std::cout << "Found pig ID: " << id << std::endl;
            } catch (...) {
                std::cerr << "Failed to parse pig ID from header: " << cell << std::endl;
                pig_ids.push_back(-1); // placeholder for tracking bad header
            }
        }
    }

    constexpr std::size_t TIMESTAMP_LENGTH = 19;

    int successful_timestamps = 0;
    int failed_timestamps = 0;
    
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string timestamp_str;
        std::getline(ss, timestamp_str, '\t');

        std::tm tm = {};
        
        // Parse timestamp using explicit format definitions
        std::istringstream ts_stream(timestamp_str);
        if (timestamp_str.length() == TIMESTAMP_LENGTH) {
            ts_stream >> std::get_time(&tm, "%Y_%m_%d_%H_%M_%S");
        } else {
            ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        }
        if (!ts_stream.fail()) {
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            successful_timestamps++;
                
                // Print the first few successful timestamps
                if (successful_timestamps <= 5) {
                    std::cout << "✅ Successfully parsed timestamp: " << timestamp_str << std::endl;
                }
                
                // Process scores for each pig
                for (size_t i = 0; i < pig_ids.size(); ++i) {
                    std::string score_str;
                    std::getline(ss, score_str, '\t');
                    
                    if (score_str.empty() || pig_ids[i] == -1) continue;
                    
                    try {
                        int score = std::stoi(score_str);
                        
                        // Just for demonstration, print a few scores
                        if (successful_timestamps <= 3 && i < 3) {
                            std::cout << "  Pig ID " << pig_ids[i] << " score: " << score << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "❌ Parse error at pig index " << i << ": " << e.what() << std::endl;
                    }
                }
        } else {
            std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
            failed_timestamps++;
        }
    }

    std::cout << "✅ Total successful timestamps: " << successful_timestamps << std::endl;
    std::cout << "❌ Total failed timestamps: " << failed_timestamps << std::endl;
    std::cout << "✅ Total pig IDs found: " << pig_ids.size() << std::endl;
    
    return 0;
}
