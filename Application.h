#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include "ThreadPool.h"

// Statistics structure to track application metrics
struct ProcessingStats {
    std::atomic<int> filesProcessed{0};
    std::atomic<int> recordsInserted{0};
    std::atomic<int> pigsRegistered{0};
    std::atomic<int> errorCount{0};
    std::unordered_map<std::string, std::chrono::system_clock::time_point> activeFiles;
    std::mutex activeFilesMutex;
    
    void addActiveFile(const std::string& filepath);
    void removeActiveFile(const std::string& filepath);
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> getActiveFiles();
};

// Configuration structure
struct AppConfig {
    std::string mongoUri = "mongodb://PAAL:PAAL@localhost:27017/?directConnection=true";
    std::string dbName = "paalab";
    std::string pigsCollection = "pigs";
    std::string posturesCollection = "pigpostures";
    std::string watchFolder = "cold_folder";
    std::string archiveFolder = "processed_files";
    std::string errorFolder = "error_files";
    int batchSize = 1000;
    int threadCount = 0; // 0 means use hardware concurrency
    bool archiveProcessedFiles = false;
    bool moveErrorFiles = true;
};

class Application {
public:
    Application();
    ~Application();
    
    // Initialize the application
    bool initialize();
    
    // Run the application
    void run();
    
    // Get application statistics
    ProcessingStats& getStats() { return stats; }
    
    // Get application configuration
    AppConfig& getConfig() { return config; }
    
    // Process a specific file
    void processFile(const std::string& filepath);
    
    // Stop the application
    void stop() { running = false; }
    
    // Check if the application is running
    bool isRunning() const { return running; }
    
private:
    // Display the main menu
    void displayMenu();
    
    // Handle user commands
    void handleCommand(const std::string& command);
    
    // Display application statistics
    void displayStats();
    
    // Configure application settings
    void configureSettings();
    
    // Start the file watcher
    void startFileWatcher();
    
    // Create necessary directories
    void createDirectories();
    
    // Move a file to the archive folder
    void archiveFile(const std::string& filepath);
    
    // Move a file to the error folder
    void moveToErrorFolder(const std::string& filepath);
    
    // MongoDB components
    mongocxx::instance instance{};
    mongocxx::client client;
    mongocxx::database db;
    mongocxx::collection pigs_collection;
    mongocxx::collection posture_collection;
    
    // Thread pool for processing files
    ThreadPool* pool;
    
    // Application statistics
    ProcessingStats stats;
    
    // Application configuration
    AppConfig config;
    
    // Control flag
    bool running;
    
    // File watcher thread
    std::thread watcherThread;
};

// Function to update the console with statistics
void updateConsoleStats(const ProcessingStats& stats);
