#pragma once

#include "rocksdb/db.h"

#include "types/ID.hpp"
#include "types/types.hpp"
#include "util/util.hpp"

#include <vector>

class Storage {
private:
    rocksdb::DB* db_;
    rocksdb::Options options;
public:
    Storage(std::string path);
    void store(const ID& key, const std::vector<u8>& value);
    std::optional<std::vector<u8>> retrieve(const ID& key);
};
