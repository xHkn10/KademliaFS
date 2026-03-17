#include "routing/RoutingTable.hpp"
#include "types/Contact.hpp"
#include <algorithm>
#include <vector>

RoutingTable::RoutingTable(const ID& id) {
    self_ = id;
}

size_t
RoutingTable::get_bucket_idx(const ID& id) {
    const size_t sz = id.bytes_.size();

    for (size_t i = 0; i < sz; ++i) {
        u8 byte = id.bytes_[i];
        if (byte == 0)
            continue;
        for (int p = 7; p >= 0; --p)
            if (byte & (1 << p))
                return i * 8 + (7 - p);
    }

    return -1;
}

std::vector<Contact>
RoutingTable::get_closest(const ID& target) {
    int idx = get_bucket_idx(target ^ self_);

    if (idx == -1)
        idx = 0;

    std::vector<Contact> res;
    res.insert(
        res.end(),
        table_[idx].bucket_.begin(), table_[idx].bucket_.end()
    );

    for (int d = 1; (idx - d >= 0 || idx + d < 160) && res.size() < 20; ++d) {
        if (idx - d >= 0) {
            for (const Contact& c : table_[idx - d].bucket_) {
                res.push_back(c);
            }
        }
        if (idx + d < 160) {
            for (const Contact& c : table_[idx + d].bucket_) {
                res.push_back(c);
            }
        }
    }

    std::sort(res.begin(), res.end(),
        [&target](const auto& c1, const auto& c2) {
            return (c1.id ^ target) < (c2.id ^ target);
        });

    if (res.size() > 20)
        res.resize(20);

    return res;
}


void
RoutingTable::insert(const Contact& contact) {
    if (contact.id != self_) {
        ID id = self_ ^ contact.id;
        table_[get_bucket_idx(id)].push(contact);
    }
}
