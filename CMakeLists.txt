cmake_minimum_required(VERSION 3.20)

project(Kademlia
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---- Build Configuration ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers (Useful for RocksDB integration to catch memory leaks)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

# ---- Find Dependencies ----
find_package(Boost REQUIRED CONFIG)
find_package(OpenSSL REQUIRED)

# --- RocksDB Configuration (macOS specific) ---
# Homebrew usually puts things in /opt/homebrew (Silicon) or /usr/local (Intel)
find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h
    PATHS /opt/homebrew/include /usr/local/include
)

find_library(ROCKSDB_LIB rocksdb
    PATHS /opt/homebrew/lib /usr/local/lib
)

if(NOT ROCKSDB_LIB OR NOT ROCKSDB_INCLUDE_DIR)
    message(FATAL_ERROR "RocksDB not found. Install it via 'brew install rocksdb'")
endif()

# RocksDB dependencies on macOS
find_library(BZ2_LIB bz2)
find_library(Z_LIB z)
find_library(LZ4_LIB lz4 PATHS /opt/homebrew/lib /usr/local/lib)
find_library(ZSTD_LIB zstd PATHS /opt/homebrew/lib /usr/local/lib)
find_library(SNAPPY_LIB snappy PATHS /opt/homebrew/lib /usr/local/lib)

# ---- Project Structure ----
include_directories(include ${ROCKSDB_INCLUDE_DIR})

file(GLOB_RECURSE LIB_SOURCES 
    "src/node/*.cpp"
    "src/routing/*.cpp"
    "src/rpc/*.cpp"
    "src/storage/*.cpp"
    "src/types/*.cpp"
)

# ---- Define Executables ----

# 3. Tests
add_executable(test_nodes src/test.cpp ${LIB_SOURCES})

# Link RocksDB and its mandatory compression dependencies
target_link_libraries(test_nodes PRIVATE 
    Boost::headers 
    OpenSSL::Crypto
    ${ROCKSDB_LIB}
    ${BZ2_LIB}
    ${Z_LIB}
    ${LZ4_LIB}
    ${ZSTD_LIB}
    ${SNAPPY_LIB}
)
