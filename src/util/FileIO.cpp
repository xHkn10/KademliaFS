#include "util/FileIO.hpp"
#include <stdexcept>

#ifndef _WIN32
#include <sys/fcntl.h>
#include <unistd.h>

namespace util {
namespace file {

FileHandle open_read(const std::string& path) {
    return (FileHandle)::open(path.c_str(), O_RDONLY);
}

FileHandle open_write(const std::string& path) {
    return (FileHandle)::open(path.c_str(), O_WRONLY);
}

void close(FileHandle fd) {
    ::close((int)fd);
}

int pread(FileHandle fd, void* buf, size_t count, size_t offset) {
    return (int)::pread((int)fd, buf, count, offset);
}

int pwrite(FileHandle fd, const void* buf, size_t count, size_t offset) {
    return (int)::pwrite((int)fd, buf, count, offset);
}

}
}
#else
#include <windows.h>

namespace util {
namespace file {

FileHandle open_read(const std::string& path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return (FileHandle)h;
}

FileHandle open_write(const std::string& path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return (FileHandle)h;
}

void close(FileHandle fd) {
    if (fd != -1) {
        CloseHandle((HANDLE)fd);
    }
}

int pread(FileHandle fd, void* buf, size_t count, size_t offset) {
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
    DWORD bytesRead = 0;
    if (ReadFile((HANDLE)fd, buf, (DWORD)count, &bytesRead, &ov)) {
        return (int)bytesRead;
    }
    return -1;
}

int pwrite(FileHandle fd, const void* buf, size_t count, size_t offset) {
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
    DWORD bytesWritten = 0;
    if (WriteFile((HANDLE)fd, buf, (DWORD)count, &bytesWritten, &ov)) {
        return (int)bytesWritten;
    }
    return -1;
}

}
}
#endif
