#pragma once

#include "KBucket.hpp"
#include "types/Contact.hpp"
#include "types/ID.hpp"
#include "types/types.hpp"

#include <array>

class RoutingTable {
private:
    std::array<KBucket, 160> table_;
    ID self_;

public:
    RoutingTable(const ID&);
    void insert(const Contact&);
    void remove(const Contact&);
    std::vector<Contact> get_closest(const ID&);
    size_t get_bucket_idx(const ID&);
};
