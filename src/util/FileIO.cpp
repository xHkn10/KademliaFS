#include "util/FileIO.hpp"
#include <stdexcept>

#ifndef _WIN32
#include <sys/fcntl.h>
#include <unistd.h>

namespace util {
namespace file {

FileHandle open_read(const std::string& path) {
    return ::open(path.c_str(), O_RDONLY);
}

FileHandle open_write(const std::string& path) {
    return ::open(path.c_str(), O_WRONLY);
}

void close(FileHandle fd) {
    ::close(fd);
}

size_t pread(FileHandle fd, void* buf, size_t count, size_t offset) {
    return ::pread(fd, buf, count, offset);
}

size_t pwrite(FileHandle fd, const void* buf, size_t count, size_t offset) {
    return ::pwrite(fd, buf, count, offset);
}

}
}
#else
namespace util {
namespace file {

FileHandle open_read(const std::string& path) {
    throw std::runtime_error("Windows file IO not implemented yet");
}

FileHandle open_write(const std::string& path) {
    throw std::runtime_error("Windows file IO not implemented yet");
}

void close(FileHandle fd) {
    throw std::runtime_error("Windows file IO not implemented yet");
}

size_t pread(FileHandle fd, void* buf, size_t count, size_t offset) {
    throw std::runtime_error("Windows file IO not implemented yet");
}

size_t pwrite(FileHandle fd, const void* buf, size_t count, size_t offset) {
    throw std::runtime_error("Windows file IO not implemented yet");
}

}
}
#endif
