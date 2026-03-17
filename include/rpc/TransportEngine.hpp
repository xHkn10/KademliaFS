#pragma once

#include "rpc/RpcMessage.hpp"

#include <boost/asio.hpp>

#include <functional>

namespace net = boost::asio;
using net::ip::udp;
using net::ip::tcp;
using net::awaitable;
using net::use_awaitable;
using net::awaitable;

struct Request {
    Contact c;
    RpcMessage msg;
    std::function<awaitable<void>(RpcMessage)> send_response;
};

class TransportEngine {
protected:
    Contact self_;
public:
    virtual ~TransportEngine() = default;
    inline TransportEngine(Contact self) : self_(self) {}
    virtual awaitable<std::optional<Request>> receive() = 0;
    virtual awaitable<std::optional<RpcMessage>> call_rpc(const Contact&, const RpcMessage&) = 0;
};
