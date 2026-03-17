#include "types/types.hpp"
#include <vector>

struct Metadata {
    size_t sz;
    size_t chunk_sz;
    std::vector<Key> chunks;
};
