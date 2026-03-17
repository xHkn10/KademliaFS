#pragma once

#include <deque>
#include "types/Contact.hpp"

class KBucket {
public:
    std::deque<Contact> bucket_;
    void push(const Contact& contact);
};
