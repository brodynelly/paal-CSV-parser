#include "Application.h"
#include "CSVIngest.h"
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace fs = std::filesystem;

// ProcessingStats methods
void ProcessingStats::addActiveFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(activeFilesMutex);
    activeFiles[filepath] = std::chrono::system_clock::now();
}

void ProcessingStats::removeActiveFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(activeFilesMutex);
    activeFiles.erase(filepath);
}

std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> ProcessingStats::getActiveFiles() {
    std::lock_guard<std::mutex> lock(activeFilesMutex);
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> result;
    for (const auto& file : activeFiles) {
        result.push_back(file);
    }
    return result;
}

// Application methods
Application::Application() : pool(nullptr), running(false) {
}

Application::~Application() {
    if (running) {
        stop();
    }

    if (watcherThread.joinable()) {
        watcherThread.join();
    }

    delete pool;
}

bool Application::initialize() {
    try {
        // Create MongoDB client
        mongocxx::uri uri(config.mongoUri);
        client = mongocxx::client(uri);

        // Access database and collections
        db = client[config.dbName];
        pigs_collection = db[config.pigsCollection];
        posture_collection = db[config.posturesCollection];

        // Create thread pool
        int threadCount = config.threadCount > 0 ? config.threadCount : std::thread::hardware_concurrency();
        pool = new ThreadPool(threadCount);

        // Create necessary directories
        createDirectories();

        std::cout << "✅ Connected to MongoDB database: " << config.dbName << std::endl;
        std::cout << "✅ Thread pool initialized with " << threadCount << " threads" << std::endl;

        running = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Initialization Error: " << e.what() << std::endl;
        return false;
    }
}

void Application::createDirectories() {
    if (!fs::exists(config.watchFolder)) {
        fs::create_directory(config.watchFolder);
    }

    if (config.archiveProcessedFiles && !fs::exists(config.archiveFolder)) {
        fs::create_directory(config.archiveFolder);
    }

    if (config.moveErrorFiles && !fs::exists(config.errorFolder)) {
        fs::create_directory(config.errorFolder);
    }
}

void Application::run() {
    // Start the file watcher in a separate thread
    startFileWatcher();

    // Main application loop
    while (running) {
        // Clear the screen
        std::cout << "\033[2J\033[1;1H";

        // Display the menu
        displayMenu();

        // Get user command
        std::string command;
        std::cout << "\nEnter command: ";
        std::getline(std::cin, command);

        // Handle the command
        handleCommand(command);
    }
}

void Application::displayMenu() {
    std::cout << "┌───────────────────────────────────────────────┐\n";
    std::cout << "│           PAAL CSV Parser - Main Menu         │\n";
    std::cout << "├───────────────────────────────────────────────┤\n";
    std::cout << "│ stats    - Display processing statistics      │\n";
    std::cout << "│ process  - Process a specific file            │\n";
    std::cout << "│ config   - Configure application settings     │\n";
    std::cout << "│ clear    - Clear the screen                   │\n";
    std::cout << "│ help     - Display this help menu             │\n";
    std::cout << "│ quit     - Exit the application               │\n";
    std::cout << "└───────────────────────────────────────────────┘\n";

    // Display current statistics
    displayStats();
}

void Application::displayStats() {
    std::cout << "\n┌───────────────────────────────────────────────┐\n";
    std::cout << "│               Current Statistics              │\n";
    std::cout << "├───────────────────────────────────────────────┤\n";
    std::cout << "│ Files Processed: " << std::setw(28) << stats.filesProcessed << " │\n";
    std::cout << "│ Records Inserted: " << std::setw(27) << stats.recordsInserted << " │\n";
    std::cout << "│ Pigs Registered: " << std::setw(28) << stats.pigsRegistered << " │\n";
    std::cout << "│ Error Count: " << std::setw(32) << stats.errorCount << " │\n";

    // Display active files
    auto activeFiles = stats.getActiveFiles();
    if (!activeFiles.empty()) {
        std::cout << "├───────────────────────────────────────────────┤\n";
        std::cout << "│ Active Files:                                 │\n";

        for (const auto& file : activeFiles) {
            std::string filename = fs::path(file.first).filename().string();
            if (filename.length() > 35) {
                filename = "..." + filename.substr(filename.length() - 32);
            }
            std::cout << "│ - " << std::left << std::setw(41) << filename << " │\n";
        }
    }

    std::cout << "└───────────────────────────────────────────────┘\n";
}

void Application::handleCommand(const std::string& command) {
    std::string cmd = command;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return std::tolower(c); });

    if (cmd == "stats") {
        // Clear screen and show detailed stats
        std::cout << "\033[2J\033[1;1H";
        std::cout << "┌───────────────────────────────────────────────┐\n";
        std::cout << "│            Detailed Statistics                │\n";
        std::cout << "└───────────────────────────────────────────────┘\n\n";

        displayStats();

        std::cout << "\nPress Enter to return to the main menu...";
        std::cin.get();
    }
    else if (cmd == "process") {
        std::cout << "\033[2J\033[1;1H";
        std::cout << "┌───────────────────────────────────────────────┐\n";
        std::cout << "│            Process Specific File              │\n";
        std::cout << "└───────────────────────────────────────────────┘\n\n";

        std::cout << "Enter the path to the CSV file: ";
        std::string filepath;
        std::getline(std::cin, filepath);

        if (!filepath.empty()) {
            if (fs::exists(filepath)) {
                std::cout << "Processing file: " << filepath << std::endl;
                processFile(filepath);
                std::cout << "File processing initiated.\n";
            } else {
                std::cout << "❌ File does not exist: " << filepath << std::endl;
            }
        }

        std::cout << "\nPress Enter to return to the main menu...";
        std::cin.get();
    }
    else if (cmd == "config") {
        configureSettings();
    }
    else if (cmd == "clear") {
        // Do nothing, the screen will be cleared on the next loop iteration
    }
    else if (cmd == "help") {
        // Do nothing, the menu will be displayed on the next loop iteration
    }
    else if (cmd == "quit" || cmd == "exit") {
        std::cout << "Shutting down application...\n";
        running = false;
    }
    else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
}

void Application::configureSettings() {
    bool configuring = true;

    while (configuring && running) {
        std::cout << "\033[2J\033[1;1H";
        std::cout << "┌───────────────────────────────────────────────┐\n";
        std::cout << "│            Configuration Settings             │\n";
        std::cout << "├───────────────────────────────────────────────┤\n";
        std::cout << "│ 1. MongoDB URI: " << std::setw(30) << (config.mongoUri.length() > 30 ? "..." + config.mongoUri.substr(config.mongoUri.length() - 27) : config.mongoUri) << " │\n";
        std::cout << "│ 2. Database Name: " << std::setw(28) << config.dbName << " │\n";
        std::cout << "│ 3. Pigs Collection: " << std::setw(26) << config.pigsCollection << " │\n";
        std::cout << "│ 4. Postures Collection: " << std::setw(22) << config.posturesCollection << " │\n";
        std::cout << "│ 5. Watch Folder: " << std::setw(29) << config.watchFolder << " │\n";
        std::cout << "│ 6. Archive Folder: " << std::setw(27) << config.archiveFolder << " │\n";
        std::cout << "│ 7. Error Folder: " << std::setw(29) << config.errorFolder << " │\n";
        std::cout << "│ 8. Batch Size: " << std::setw(31) << config.batchSize << " │\n";
        std::cout << "│ 9. Thread Count: " << std::setw(29) << (config.threadCount == 0 ? "Auto" : std::to_string(config.threadCount)) << " │\n";
        std::cout << "│ 10. Archive Processed Files: " << std::setw(18) << (config.archiveProcessedFiles ? "Yes" : "No") << " │\n";
        std::cout << "│ 11. Move Error Files: " << std::setw(25) << (config.moveErrorFiles ? "Yes" : "No") << " │\n";
        std::cout << "│ 0. Return to Main Menu                        │\n";
        std::cout << "└───────────────────────────────────────────────┘\n";

        std::cout << "\nEnter option number to change (0 to return): ";
        std::string option;
        std::getline(std::cin, option);

        try {
            int opt = std::stoi(option);

            switch (opt) {
                case 0:
                    configuring = false;
                    break;
                case 1:
                    std::cout << "Enter new MongoDB URI: ";
                    std::getline(std::cin, config.mongoUri);
                    break;
                case 2:
                    std::cout << "Enter new Database Name: ";
                    std::getline(std::cin, config.dbName);
                    break;
                case 3:
                    std::cout << "Enter new Pigs Collection name: ";
                    std::getline(std::cin, config.pigsCollection);
                    break;
                case 4:
                    std::cout << "Enter new Postures Collection name: ";
                    std::getline(std::cin, config.posturesCollection);
                    break;
                case 5:
                    std::cout << "Enter new Watch Folder path: ";
                    std::getline(std::cin, config.watchFolder);
                    break;
                case 6:
                    std::cout << "Enter new Archive Folder path: ";
                    std::getline(std::cin, config.archiveFolder);
                    break;
                case 7:
                    std::cout << "Enter new Error Folder path: ";
                    std::getline(std::cin, config.errorFolder);
                    break;
                case 8: {
                    std::cout << "Enter new Batch Size: ";
                    std::string batchSizeStr;
                    std::getline(std::cin, batchSizeStr);
                    config.batchSize = std::stoi(batchSizeStr);
                    break;
                }
                case 9: {
                    std::cout << "Enter new Thread Count (0 for auto): ";
                    std::string threadCountStr;
                    std::getline(std::cin, threadCountStr);
                    config.threadCount = std::stoi(threadCountStr);
                    break;
                }
                case 10: {
                    std::cout << "Archive Processed Files? (y/n): ";
                    std::string answer;
                    std::getline(std::cin, answer);
                    config.archiveProcessedFiles = (std::tolower(answer[0]) == 'y');
                    break;
                }
                case 11: {
                    std::cout << "Move Error Files? (y/n): ";
                    std::string answer;
                    std::getline(std::cin, answer);
                    config.moveErrorFiles = (std::tolower(answer[0]) == 'y');
                    break;
                }
                default:
                    std::cout << "Invalid option. Press Enter to continue...";
                    std::cin.get();
                    break;
            }

            // If we changed a critical setting, we need to reinitialize
            if (opt >= 1 && opt <= 4) {
                std::cout << "Critical setting changed. Application needs to restart.\n";
                std::cout << "Press Enter to continue...";
                std::cin.get();

                // Stop the current application
                running = false;
                configuring = false;

                // The main function will need to reinitialize the application
            }
        }
        catch (const std::exception& e) {
            std::cout << "Invalid input. Press Enter to continue...";
            std::cin.get();
        }
    }
}

void Application::startFileWatcher() {
    // Create a lambda function to watch the directory
    auto watchFunction = [this]() {
        std::unordered_set<std::string> seen;

        while (this->running) {
            try {
                for (const auto& entry : fs::directory_iterator(this->config.watchFolder)) {
                    if (entry.path().extension() == ".csv") {
                        std::string filepath = entry.path().string();
                        if (seen.find(filepath) == seen.end()) {
                            seen.insert(filepath);
                            this->processFile(filepath);
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "❌ File Watcher Error: " << e.what() << std::endl;
                this->stats.errorCount++;
            }

            // Sleep for a short time before checking again
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    };

    // Start the watcher thread
    watcherThread = std::thread(watchFunction);
}

void Application::processFile(const std::string& filepath) {
    // Add the file to active files
    stats.addActiveFile(filepath);

    // Create a lambda function to process the file
    auto processFunction = [this, filepath]() {
        try {
            // Process the file
            parse_and_batch_insert(filepath, this->pigs_collection, this->posture_collection, &(this->stats), this->config.batchSize);

            // Archive the file if needed
            if (this->config.archiveProcessedFiles) {
                this->archiveFile(filepath);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "❌ Processing Error: " << e.what() << std::endl;
            this->stats.errorCount++;

            // Move to error folder if needed
            if (this->config.moveErrorFiles) {
                this->moveToErrorFolder(filepath);
            }
        }

        // Remove the file from active files
        this->stats.removeActiveFile(filepath);
    };

    // Enqueue the task in the thread pool
    pool->enqueue(processFunction);
}

void Application::archiveFile(const std::string& filepath) {
    try {
        fs::path source(filepath);
        fs::path destination = fs::path(config.archiveFolder) / source.filename();

        // Create a unique filename if the file already exists
        if (fs::exists(destination)) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&timestamp), "%Y%m%d_%H%M%S_");
            destination = fs::path(config.archiveFolder) / (ss.str() + source.filename().string());
        }

        // Move the file
        fs::rename(source, destination);
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Archive Error: " << e.what() << std::endl;
        stats.errorCount++;
    }
}

void Application::moveToErrorFolder(const std::string& filepath) {
    try {
        fs::path source(filepath);
        fs::path destination = fs::path(config.errorFolder) / source.filename();

        // Create a unique filename if the file already exists
        if (fs::exists(destination)) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&timestamp), "%Y%m%d_%H%M%S_");
            destination = fs::path(config.errorFolder) / (ss.str() + source.filename().string());
        }

        // Move the file
        fs::rename(source, destination);
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Error Moving File: " << e.what() << std::endl;
        stats.errorCount++;
    }
}

// Function to update console with statistics
void updateConsoleStats(const ProcessingStats& stats) {
    // Clear the current line
    std::cout << "\r\033[K";

    // Print the statistics
    std::cout << "Files: " << stats.filesProcessed
              << " | Records: " << stats.recordsInserted
              << " | Pigs: " << stats.pigsRegistered
              << " | Errors: " << stats.errorCount;

    // Flush the output
    std::cout.flush();
}
