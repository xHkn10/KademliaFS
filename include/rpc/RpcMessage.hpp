#pragma once

#include "types/Contact.hpp"
#include "types/ID.hpp"
#include "types/types.hpp"

#include <optional>
#include <variant>
#include <vector>

enum class RpcType : u8 {
    PING,
    PONG,
    STORE,
    FIND_NODE,
    FIND_VALUE
};

struct RpcMessage {
    RpcType type;
    u32 transactionID;
    ID senderID;
    u16 sender_port;
    bool is_response;
    std::variant<
        std::monostate,
        ID,
        std::pair<Key, Value>,
        std::vector<Contact>
    > data;

    RpcMessage() = default;

    std::vector<u8> serialize() const;
    static std::optional<RpcMessage> deserialize(const std::vector<u8>&);
    
    std::vector<Contact>& get_contacts();
    std::pair<ID, std::vector<u8>>& get_key_value();
    ID& get_id();

    static RpcMessage make_find_node_rpc(const ID& senderID, const ID&, u16);
    static RpcMessage make_store_rpc(const ID& senderID, Key, Value, u16);
    static RpcMessage make_find_value_rpc(ID senderID, Key, u16);
    static RpcMessage make_ping_rpc(const ID& senderID, const ID&, u16);
    
    static RpcMessage make_pong_rpc(const ID& senderID, u32, u16);
    static RpcMessage make_k_closest_reply(const ID& senderID, const std::vector<Contact>&, u32, u16);
    static RpcMessage make_key_value_reply(const ID& senderID, ID, Value, u32, u16);
    static RpcMessage make_store_ack_reply(const ID& senderID, u32, u16);
};
