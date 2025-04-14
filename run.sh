#!/bin/bash

# Ensure build directory exists
mkdir -p build

# Build the project
cd build
cmake ..
make

# Return to the root directory
cd ..

# Run the application
./build/CsVParser
