cmake_minimum_required(VERSION 3.15)
project(CsVParser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Find MongoDB C++ drivers
find_package(PkgConfig REQUIRED)
pkg_check_modules(MONGOCXX REQUIRED libmongocxx)
pkg_check_modules(BSONCXX REQUIRED libbsoncxx)

# Include paths
include_directories(
    ${MONGOCXX_INCLUDE_DIRS}
    ${BSONCXX_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

# Source files
set(SOURCES
    main.cpp
    CSVIngest.cpp
    FileWatcher.cpp
    ThreadPool.cpp
    Pig.cpp
    PigPosture.cpp
)

# Add executable
add_executable(CsVParser ${SOURCES})

# Link MongoDB + stdc++fs (for GCC's filesystem support)
target_link_libraries(CsVParser
    ${MONGOCXX_LIBRARIES}
    ${BSONCXX_LIBRARIES}
    pthread
)

# Required for GCC < 9 using std::filesystem
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_libraries(CsVParser stdc++fs)
endif()

# Include flags
add_definitions(${MONGOCXX_CFLAGS_OTHER})
add_definitions(${BSONCXX_CFLAGS_OTHER})
