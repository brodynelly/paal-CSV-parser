#pragma once
#include <string>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <chrono>

class Posture {
    public:
        int pigId;
        std::chrono::system_clock::time_point timestamp;
        int score;
    
        Posture(int pig_id,
                const std::chrono::system_clock::time_point& timestamp,
                int score)
            : pigId(pig_id), timestamp(timestamp), score(score) {}
    
        bsoncxx::document::value to_bson() const;  
    };
    