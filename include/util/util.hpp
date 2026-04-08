#pragma once

#include "openssl/sha.h"

#include "rpc/RpcMessage.hpp"
#include "types/ID.hpp"
#include "types/types.hpp"

#include <iostream>
#include <ostream>
#include <random>

#define LOG(x) std::cout << x << std::endl

inline std::ostream& operator<<(std::ostream& out, RpcType t) {
    switch (t) {
        case RpcType::PING:
            out << "PING";
            break;
        case RpcType::PONG:
            out << "PONG";
            break;
        case RpcType::STORE:
            out << "STORE";
            break;
        case RpcType::FIND_NODE:
            out << "FIND_NODE";
            break;
        case RpcType::FIND_VALUE:
            out << "FIND_VALUE";
            break;
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, ID id) {
    u16 sum = 0;
    for (const u8 b : id.bytes_)
        (sum += b) %= 255;
    out << sum;
    return out;
}

namespace util {
    inline auto& get_gen() {
        thread_local std::random_device rd;
        thread_local std::mt19937 gen{rd()};
        return gen;
    }

    inline unsigned int get_random_u32() {
        auto& gen = get_gen();
        return std::uniform_int_distribution<u32>{0, 0xFFFFFFFF}(gen);
    }

    inline Key hash(const std::vector<u8>& data) {
        Key hash;
        SHA1(data.data(), data.size(), hash.bytes_.data());
        return hash;
    }
}
