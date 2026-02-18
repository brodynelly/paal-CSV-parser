# PAAL CSV Parser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) [![CI](https://github.com/brodynelly/mizzou-digital-agricultural-labs-csv-parser/actions/workflows/ci.yml/badge.svg)](https://github.com/brodynelly/mizzou-digital-agricultural-labs-csv-parser/actions/workflows/ci.yml) ![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus&logoColor=white)

Server that watches a drop folder for CSV files from the PAAL AI model, spins up a thread pool to parse them, and writes pig posture data to MongoDB.

## Tech Stack

- **C++17** — thread pool, file watcher, CSV parsing
- **CMake 3.10+** — build system
- **MongoDB C++ Driver** (`libmongocxx`) — database writes
- **POSIX** — directory monitoring via filesystem events

## Features

- Parse CSV files with tab-separated values
- Support for timestamps in format `YYYY_MM_DD_HH_MM_SS`
- Automatic pig registration in the database
- Batch processing for efficient database operations
- Error handling and reporting

## Prerequisites

- C++17 compatible compiler
- CMake (version 3.10 or higher)
- MongoDB C++ Driver
- MongoDB server running locally or accessible remotely

## Installation

1. Clone the repository:
   ```bash
   git clone git@github.com:brodynelly/paal-CSV-parser.git
   cd paal-CSV-parser
   ```

2. Create a build directory and compile the project:
   ```bash
   mkdir -p build
   cd build
   cmake ..
   make
   ```

3. Ensure MongoDB is running on your system or accessible remotely.

## Usage

1. Place your CSV files in the `cold_folder` directory. The CSV files should:
   - Be tab-separated
   - Have a header row with column names
   - First column should be timestamps in format `YYYY_MM_DD_HH_MM_SS`
   - Other columns should be named `ID_X` where X is the pig ID number
   - Columns not following this pattern will be skipped with a warning
   - Values should be numeric scores

2. Run the application using the provided script:
   ```bash
   ./run.sh
   ```

   Or run the executable directly:
   ```bash
   ./build/CsVParser
   ```

3. The application will:
   - Parse all CSV files in the `cold_folder` directory
   - Register new pigs in the database if they don't exist
   - Store posture data for each pig
   - Report progress and any errors

## CSV File Format

The expected CSV format is:

```
Timestamp	ID_129	ID_131	ID_143	...
2022_08_22_02_20_00	9	9	9	...
2022_08_22_02_30_00	9	9	9	...
...
```

Where:
- The first column contains timestamps in the format `YYYY_MM_DD_HH_MM_SS`
- Each subsequent column represents a pig with ID in the header (e.g., `ID_129`)
- Values are numeric scores (typically 0-9)
- Fields are separated by tabs (`\t`)

## Database Structure

The application uses two MongoDB collections:

1. `pigs` - Stores information about each pig:
   - `pigId`: Unique identifier for the pig
   - Other metadata fields (tag, breed, etc.)

2. `postures` - Stores posture data:
   - `pigId`: Reference to the pig
   - `timestamp`: When the measurement was taken
   - `score`: The posture score value

## Troubleshooting

- **Timestamp parsing errors**: Ensure your timestamps are in the format `YYYY_MM_DD_HH_MM_SS`
- **Database connection issues**: Check that MongoDB is running and accessible
- **CSV parsing errors**: Verify that your CSV files use tabs as separators and follow the expected format

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Project Status

Active — deployed as a background service in the PAAL research environment at the University of Missouri.
