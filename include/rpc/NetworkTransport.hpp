#pragma once

#include "rpc/RpcMessage.hpp"
#include "types/Contact.hpp"
#include "types/types.hpp"

#include "boost/asio.hpp"
#include <utility>

using boost::asio::ip::udp;
using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::use_awaitable;

class NetworkTransport {
public:
    udp::socket socket_;    

    ~NetworkTransport() = default;

    awaitable<void> send_msg(const Contact& target, const RpcMessage&);
    awaitable<RpcMessage> send_query_tcp(const Contact& target, const RpcMessage&, u16 port);
    awaitable<void> send_response_tcp(tcp::socket, RpcMessage);
    awaitable<std::pair<udp::endpoint, RpcMessage>> receive();
    void stop();
};
