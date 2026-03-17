#pragma once

#include "rpc/TransportEngine.hpp"
#include <boost/asio/experimental/channel.hpp>

class TCPTransport : public TransportEngine {
private:
    tcp::acceptor acceptor_;

    using Channel = net::experimental::channel<void(boost::system::error_code, std::optional<Request>)>;
    Channel channel_;
public:
    TCPTransport(Contact, net::any_io_executor);
    awaitable<std::optional<Request>> receive() override;
    awaitable<std::optional<RpcMessage>> call_rpc(const Contact&, const RpcMessage&) override;
private:
    awaitable<void> accept_loop();
    awaitable<void> read_and_push(std::shared_ptr<tcp::socket>);
};
