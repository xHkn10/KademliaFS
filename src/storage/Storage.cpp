#include "storage/Storage.hpp"
#include "types/ID.hpp"
#include "util/util.hpp"

#include <stdexcept>
#include <vector>

Storage::Storage(std::string path) {
    options.create_if_missing = true;
    auto status = rocksdb::DB::Open(options, path, &db_);
    if (!status.ok()) {
        LOG("Storage: Couldn't open DB " << status.ToString());
        throw std::runtime_error(status.ToString());
    }
}

void Storage::store(const ID& key, const std::vector<u8>& value) {
    rocksdb::Slice k{reinterpret_cast<const char*>(key.bytes_.data()), key.bytes_.size()};
    rocksdb::Slice v{reinterpret_cast<const char*>(value.data()), value.size()};
    auto status = db_->Put(rocksdb::WriteOptions(), k, v);
    if (!status.ok())
        LOG("store:  " << status.ToString());
}

std::optional<std::vector<u8>>
Storage::retrieve(const ID& key) {
    rocksdb::Slice k{reinterpret_cast<const char*>(key.bytes_.data()), key.bytes_.size()};
    std::string buffer;
    auto status = db_->Get(rocksdb::ReadOptions(), k, &buffer);

    if (status.ok())
        return std::vector<u8>(buffer.begin(), buffer.end());
    else
        return std::nullopt;
}
