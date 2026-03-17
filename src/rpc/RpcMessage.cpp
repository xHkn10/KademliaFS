#include "rpc/RpcMessage.hpp"
#include "util/util.hpp"
#include <optional>
#include <variant>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::vector<u8>
RpcMessage::serialize() const {
    std::vector<u8> res;
    res.reserve(128);

    auto append = [&](const auto& val, size_t sz) {
        const u8* p = reinterpret_cast<const u8*>(&val);
        res.insert(res.end(), p, p + sz);
    };

    append(type, sizeof(type));
    append(transactionID, sizeof(transactionID));
    append(senderID, sizeof(senderID));
    append(sender_port, sizeof(sender_port));
    append(is_response, sizeof(is_response));

    std::visit(overloaded {
        [&](const std::monostate& val) {
        },
        [&](const ID& val) {
            append(val, sizeof(ID));
        },
        [&](const std::pair<ID, std::vector<u8>>& val) {
            append(val.first, sizeof(ID));
            res.insert(res.end(), val.second.begin(), val.second.end());
        },
        [&](const std::vector<Contact>& val) {
            size_t bytes = val.size() * sizeof(Contact);
            const u8* p = reinterpret_cast<const u8*>(val.data());
            res.insert(res.end(), p, p + bytes);
        }
    }, data);

    return res;
}


std::optional<RpcMessage>
RpcMessage::deserialize(const std::vector<u8>& packet) {
    RpcMessage res;
    size_t cursor = 0;

    auto read = [&cursor, &packet](void* dst, size_t sz) {
        if (cursor + sz > packet.size())
            return false;
        std::memcpy(dst, packet.data() + cursor, sz);
        cursor += sz;
        return true;
    };

    if (!read(&res.type, sizeof(res.type)))
        return std::nullopt;
    if (!read(&res.transactionID, sizeof(res.transactionID)))
        return std::nullopt;
    if (!read(&res.senderID, sizeof(res.senderID)))
        return std::nullopt;
    if (!read(&res.sender_port, sizeof(res.sender_port)))
        return std::nullopt;
    if (!read(&res.is_response, sizeof(res.is_response)))
        return std::nullopt;

    switch (res.type) {
        case RpcType::PING:
            res.data = std::monostate{};
            break;
        case RpcType::PONG:
            res.data = std::monostate{};
            break;
        case RpcType::STORE: {
            if (res.is_response)
                res.data = std::monostate{};
            else {
                Key k;
                if (!read(&k, sizeof(Key)))
                    return std::nullopt;
                Value v(packet.size() - cursor);
                if (!read(v.data(), packet.size() - cursor))
                    return std::nullopt;
                res.data = std::pair{k, std::move(v)};
            }
            break;
        }
        case RpcType::FIND_NODE: {
            if (res.is_response) {
                std::vector<Contact> k_closest(
                    (packet.size() - cursor) / sizeof(Contact)
                );
                if (!read(k_closest.data(), packet.size() - cursor))
                    return std::nullopt;
                res.data = std::move(k_closest);
            } else {
                ID id;
                if (!read(&id, sizeof(ID)))
                    return std::nullopt;
                res.data = id;
            }
            break;
        }
        case RpcType::FIND_VALUE: {
            if (res.is_response) {
                Key key;
                if (!read(&key, sizeof(Key)))
                    return std::nullopt;
                std::vector<u8> val(packet.size() - cursor);
                if (!read(val.data(), packet.size() - cursor))
                    return std::nullopt;
                res.data = std::pair{key, std::move(val)};
            } else {
                Key key;
                if (!read(&key, sizeof(Key)))
                    return std::nullopt;
                res.data = key;
            }
        }
        break;
    }

    return res;
}

std::vector<Contact>&
RpcMessage::get_contacts() {
    return std::get<std::vector<Contact>>(data);
}

std::pair<Key, Value>&
RpcMessage::get_key_value() {
    return std::get<std::pair<Key, Value>>(data);
}

ID&
RpcMessage::get_id() {
    return std::get<ID>(data);
}

RpcMessage
RpcMessage::make_find_node_rpc(
    const ID& senderID, const ID& targetID, u16 port
) {
    RpcMessage res{};
    res.type = RpcType::FIND_NODE;
    res.transactionID = util::get_random_u32();
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = false;
    res.data = targetID;
    return res;
}

RpcMessage
RpcMessage::make_store_rpc(
    const ID& senderID, Key key, Value value, u16 sender_port
) {
    RpcMessage res{};
    res.type = RpcType::STORE;
    res.transactionID = util::get_random_u32();
    res.senderID = senderID;
    res.sender_port = sender_port;
    res.is_response = false;
    res.data = std::pair{std::move(key), std::move(value)};
    return res;
}

RpcMessage
RpcMessage::make_find_value_rpc(
    ID senderID, Key key, u16 port
) {
    RpcMessage res{};
    res.type = RpcType::FIND_VALUE;
    res.transactionID = util::get_random_u32();
    res.senderID = std::move(senderID);
    res.sender_port = port;
    res.is_response = false;
    res.data = std::move(key);
    return res;
}

RpcMessage
RpcMessage::make_ping_rpc(
    const ID& senderID, const ID& targetID, u16 port
) {
    RpcMessage res{};
    res.type = RpcType::PING;
    res.transactionID = util::get_random_u32();
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = false;
    res.data = std::monostate{};
    return res;
}

RpcMessage
RpcMessage::make_pong_rpc(
    const ID& senderID, u32 tID, u16 port
) {
    RpcMessage res{};
    res.type = RpcType::PONG;
    res.transactionID = tID;
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = true;
    res.data = std::monostate{};
    return res;
}

RpcMessage
RpcMessage::make_k_closest_reply(
    const ID& senderID,
    const std::vector<Contact>& k_closest,
    u32 tID,
    u16 port
) {
    RpcMessage res{};
    res.type = RpcType::FIND_NODE;
    res.transactionID = tID;
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = true;
    res.data = k_closest;
    return res;
}

RpcMessage
RpcMessage::make_key_value_reply(
    const ID& senderID,
    Key key,
    std::vector<u8> value,
    u32 tID,
    u16 port
) {
    RpcMessage res{};
    res.type = RpcType::FIND_VALUE;
    res.transactionID = tID;
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = true;
    res.data = std::pair{std::move(key), std::move(value)};
    return res;
}

RpcMessage
RpcMessage::make_store_ack_reply(
    const ID& senderID, u32 tID, u16 port
) {
    RpcMessage res{};
    res.type = RpcType::STORE;
    res.transactionID = tID;
    res.senderID = senderID;
    res.sender_port = port;
    res.is_response = true;
    res.data = std::monostate{};
    return res;
}
