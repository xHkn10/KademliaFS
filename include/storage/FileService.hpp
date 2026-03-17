#include "node/Node.hpp"
#include "storage/Metadata.hpp"
#include <fstream>
#include <optional>
#include <string>

class FileService {
private:
    Node& node_;

public:
    FileService(Node& node);

    awaitable<Metadata> upload_file(const std::string file_path);
    awaitable<bool> download_file(const Metadata& md, const std::string target_dir);

private:
    awaitable<std::optional<Key>> upload_chunk(std::ifstream& file, Metadata& md, int idx, int& pending);
    awaitable<void> download_chunk(const Key& key, int idx, int& pending, std::ofstream& out);
};
