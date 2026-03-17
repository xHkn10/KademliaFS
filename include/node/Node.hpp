#pragma once

#include "routing/RoutingTable.hpp"
#include "rpc/NetworkTransport.hpp"
#include "rpc/RpcMessage.hpp"
#include "rpc/TransportEngine.hpp"
#include "storage/Storage.hpp"
#include "types/Contact.hpp"
#include "types/ID.hpp"
#include "types/types.hpp"

#include <memory>
#include <vector>

#include "boost/asio.hpp"

namespace net = boost::asio;
using net::awaitable;

class Node {
public:
    std::unique_ptr<TransportEngine> transport_;
    Contact self_;
    RoutingTable table_;
    Storage storage_;

public:
    Node(Contact, std::unique_ptr<TransportEngine>, std::string db_path);

    awaitable<void> listen();
    awaitable<void> bootstrap(const std::vector<Contact>&);
    awaitable<void> store(Value);
    awaitable<void> store(Key, Value);
    awaitable<std::optional<Value>> find_value(const ID&);
    awaitable<std::vector<Contact>> find_node(const ID&);
    awaitable<std::optional<RpcMessage>> ping(const Contact&);

private:
    awaitable<std::optional<RpcMessage>>
    call_rpc(Contact target, RpcMessage msg);

    awaitable<std::vector<Contact>> node_lookup(const ID&);
    awaitable<std::optional<Value>> value_lookup(const Key&);

    awaitable<std::vector<Contact>> async_node_lookup(const ID&);
    awaitable<std::optional<Value>> async_value_lookup(const Key&);

    awaitable<void> handle_request(Request);
    awaitable<void> handle_ping(Request);
    awaitable<void> handle_find_value(Request);
    awaitable<void> handle_find_node(Request);
    awaitable<void> handle_store(Request);
};
