#pragma once

#include "node/Node.hpp"
#include "storage/Metadata.hpp"
#include <fstream>
#include <optional>
#include <string>

template <typename Transport>
class FileService {
private:
    Contact boot_c;
    net::any_io_executor disk_ex;
    std::unique_ptr<Node> fs_node;
public:
    FileService(Contact, net::any_io_executor);
    awaitable<void> start(const u16 port);
    
    awaitable<bool> download_file(const Metadata&, const std::string target_dir);
    awaitable<Metadata> upload_file(const std::string file_path);

    awaitable<Metadata> get_metadata(const std::string);
    awaitable<bool> upload_metadata(const std::string, const Metadata&);
private:
    awaitable<std::optional<Key>> upload_chunk(std::ifstream& file, Metadata& md, int idx, int& pending);
    awaitable<void> download_chunk(const Key& key, int idx, int& pending, std::ofstream& out);
};

#include "FileService.tpp"
