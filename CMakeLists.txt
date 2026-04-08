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

# Sanitizers (Useful for RocksDB integration to catch memory leaks, mostly supported on Clang/GNU)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

# ---- Find Dependencies ----
find_package(Boost REQUIRED CONFIG)
find_package(OpenSSL REQUIRED)
find_package(RocksDB CONFIG)

if(RocksDB_FOUND)
    message(STATUS "Found RocksDB via CMake Configuration")
    set(ROCKSDB_TARGET RocksDB::rocksdb)
else()
    message(STATUS "RocksDB CMake config not found, falling back to manual find")
    # --- RocksDB Configuration (fallback for Homebrew/Linux) ---
    find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h
        HINTS /opt/homebrew/include /usr/local/include
    )
    
    find_library(ROCKSDB_LIB rocksdb
        HINTS /opt/homebrew/lib /usr/local/lib
    )
    
    if(NOT ROCKSDB_LIB OR NOT ROCKSDB_INCLUDE_DIR)
        message(FATAL_ERROR "RocksDB not found. Install it via your package manager (e.g., 'vcpkg install rocksdb', 'brew install rocksdb')")
    endif()
    
    # Create an imported target for consistency
    add_library(RocksDB::rocksdb UNKNOWN IMPORTED)
    set_target_properties(RocksDB::rocksdb PROPERTIES
        IMPORTED_LOCATION "${ROCKSDB_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ROCKSDB_INCLUDE_DIR}"
    )
    
    # Manual compression dependencies often required when linking statically or manually
    find_library(BZ2_LIB bz2 HINTS /opt/homebrew/lib /usr/local/lib)
    find_library(Z_LIB z HINTS /opt/homebrew/lib /usr/local/lib)
    find_library(LZ4_LIB lz4 HINTS /opt/homebrew/lib /usr/local/lib)
    find_library(ZSTD_LIB zstd HINTS /opt/homebrew/lib /usr/local/lib)
    find_library(SNAPPY_LIB snappy HINTS /opt/homebrew/lib /usr/local/lib)
    
    # Gather found compression libs
    set(ROCKSDB_DEPS "")
    foreach(_lib ${BZ2_LIB} ${Z_LIB} ${LZ4_LIB} ${ZSTD_LIB} ${SNAPPY_LIB})
        if(_lib)
            list(APPEND ROCKSDB_DEPS "${_lib}")
        endif()
    endforeach()
    
    if(ROCKSDB_DEPS)
        set_property(TARGET RocksDB::rocksdb APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${ROCKSDB_DEPS})
    endif()
endif()

# ---- Project Structure ----
# Notice: RocksDB::rocksdb already propagates include directories
include_directories(include)

file(GLOB_RECURSE LIB_SOURCES 
    "src/node/*.cpp"
    "src/routing/*.cpp"
    "src/rpc/*.cpp"
    "src/storage/*.cpp"
    "src/types/*.cpp"
)

# ---- Define Executables ----

add_executable(spawn src/spawn_nodes.cpp ${LIB_SOURCES})
add_executable(upload src/upload.cpp ${LIB_SOURCES})
add_executable(download src/download.cpp ${LIB_SOURCES})

# Link targets - leveraging modern CMake target-based dependencies
target_link_libraries(spawn PRIVATE 
    Boost::headers 
    OpenSSL::Crypto
    RocksDB::rocksdb
)
target_link_libraries(upload PRIVATE 
    Boost::headers 
    OpenSSL::Crypto
    RocksDB::rocksdb
)
target_link_libraries(download PRIVATE 
    Boost::headers 
    OpenSSL::Crypto
    RocksDB::rocksdb
)
