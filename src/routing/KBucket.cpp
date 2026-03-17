#include "routing/KBucket.hpp"
#include "types/Contact.hpp"

void KBucket::push(const Contact& contact) {
    bool has = false;
    for (const Contact& c : bucket_)
        if (c.id == contact.id)
            has = true;
    if (!has)
        bucket_.push_back(contact);
    if (bucket_.size() == 21)
        bucket_.pop_front();
}
