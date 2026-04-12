#pragma once
#include <cstddef>
#include <string>
#include <cstdint>

namespace util {
namespace file {
    using FileHandle = intptr_t;
    
    FileHandle open_read(const std::string& path);
    FileHandle open_write(const std::string& path);
    void close(FileHandle fd);
    
    int pread(FileHandle fd, void* buf, size_t count, size_t offset);
    int pwrite(FileHandle fd, const void* buf, size_t count, size_t offset);
}
}
