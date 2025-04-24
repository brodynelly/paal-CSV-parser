#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>

#include "Application.h"

namespace fs = std::filesystem;

int main() {
    // Display welcome banner
    std::cout << "\033[2J\033[1;1H"; // Clear screen
    std::cout << "┌───────────────────────────────────────────────┐\n";
    std::cout << "│                                               │\n";
    std::cout << "│            PAAL CSV Parser v1.1.0             │\n";
    std::cout << "│                                               │\n";
    std::cout << "│      A robust CSV parser for pig posture      │\n";
    std::cout << "│      data with MongoDB integration            │\n";
    std::cout << "│                                               │\n";
    std::cout << "│      Developed by Brody Nelson @ PAALABS      │\n";
    std::cout << "│                                               │\n";
    std::cout << "└───────────────────────────────────────────────┘\n\n";

    // Create and initialize the application
    Application app;

    try {
        if (app.initialize()) {
            std::cout << "✅ Application initialized successfully\n";
            std::cout << "✅ Press Enter to continue...\n";
            std::cin.get();

            // Run the application
            app.run();
        } else {
            std::cerr << "❌ Failed to initialize application\n";
            return EXIT_FAILURE;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Application Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Application shutdown complete.\n";
    return EXIT_SUCCESS;
}
