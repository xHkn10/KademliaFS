#pragma once
#include <cstddef>
#include <string>

namespace util {
namespace file {
    using FileHandle = int;
    
    FileHandle open_read(const std::string& path);
    FileHandle open_write(const std::string& path);
    void close(FileHandle fd);
    
    size_t pread(FileHandle fd, void* buf, size_t count, size_t offset);
    size_t pwrite(FileHandle fd, const void* buf, size_t count, size_t offset);
}
}
