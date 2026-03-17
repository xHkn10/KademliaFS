#pragma once

#include "types.hpp"
#include <array>

struct ID {
    std::array<u8, 20> bytes_;
    ID operator^(const ID& other) const;
    bool operator<(const ID& other) const;
    bool operator==(const ID& other) const;
    ID(const std::array<u8, 20>& bytes);
    ID();
    const std::array<u8, 20>& get_bytes() const;
    static ID get_random_ID();
};
