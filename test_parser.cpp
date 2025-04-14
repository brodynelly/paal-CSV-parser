#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

int main() {
    std::string filepath = "cold_folder/unknown1.csv";
    
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
            } catch (...) {
                std::cerr << "Failed to parse pig ID from header: " << cell << std::endl;
                pig_ids.push_back(-1); // placeholder for tracking bad header
            }
        }
    }

    int successful_timestamps = 0;
    int failed_timestamps = 0;
    
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
                
                auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                successful_timestamps++;
                
                // Print the first few successful timestamps
                if (successful_timestamps <= 5) {
                    std::cout << "✅ Successfully parsed timestamp: " << timestamp_str << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ Timestamp parse fail: " << timestamp_str << " - " << e.what() << std::endl;
                failed_timestamps++;
            }
        } else {
            // Try standard format as fallback
            std::istringstream ts_stream(timestamp_str);
            ts_stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            
            if (ts_stream.fail()) {
                std::cerr << "❌ Timestamp parse fail: " << timestamp_str << std::endl;
                failed_timestamps++;
            } else {
                auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                successful_timestamps++;
                
                // Print the first few successful timestamps
                if (successful_timestamps <= 5) {
                    std::cout << "✅ Successfully parsed timestamp: " << timestamp_str << std::endl;
                }
            }
        }
    }

    std::cout << "✅ Total successful timestamps: " << successful_timestamps << std::endl;
    std::cout << "❌ Total failed timestamps: " << failed_timestamps << std::endl;
    std::cout << "✅ Total pig IDs found: " << pig_ids.size() << std::endl;
    
    return 0;
}
