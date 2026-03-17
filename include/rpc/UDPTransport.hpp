#pragma once

#include "rpc/TransportEngine.hpp"
#include <boost/asio/experimental/channel.hpp>

class UDPTransport : public TransportEngine {
    using Channel = net::experimental::channel<void(boost::system::error_code, RpcMessage)>;
private:
    udp::socket socket_;
    std::unordered_map<u32, std::shared_ptr<Channel>> channels_;
public:
    UDPTransport(Contact, net::any_io_executor);
    awaitable<std::optional<Request>> receive() override;
    awaitable<std::optional<RpcMessage>> call_rpc(const Contact&, const RpcMessage&) override;
};
