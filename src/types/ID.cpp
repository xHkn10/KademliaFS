#include "types/ID.hpp"
#include "util/util.hpp"
#include <array>
#include <random>

ID ID::operator^(const ID& other) const {
    std::array<u8, 20> res;
    for (size_t i = 0; i < res.size(); ++i)
        res[i] = bytes_[i] ^ other.bytes_[i];
    return ID{res};
}

bool ID::operator<(const ID& other) const {
    return bytes_ < other.bytes_;
}

bool ID::operator==(const ID& other) const {
    return bytes_ == other.bytes_;
}

const std::array<u8, 20>& ID::get_bytes() const {
    return bytes_;
}

ID::ID(const std::array<u8, 20>& bytes) : bytes_{bytes} {}

ID::ID() : bytes_{} {}

ID ID::get_random_ID() {
    auto& gen = util::get_gen();
    std::array<u8, 20> bytes;

    for (u8& byte : bytes)
        byte = std::uniform_int_distribution<unsigned int>{0, 255}(gen);

    return ID{bytes};
}
