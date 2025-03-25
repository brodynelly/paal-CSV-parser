// models/CurrentLocation.h
#pragma once
#include <string>

struct CurrentLocation {
    std::string farmId;
    std::string barnId;
    std::string stallId;

    // 👇 THIS GOES RIGHT HERE
    CurrentLocation(const std::string& farm = "N/A",
                    const std::string& barn = "N/A",
                    const std::string& stall = "N/A")
        : farmId(farm), barnId(barn), stallId(stall) {}
};
