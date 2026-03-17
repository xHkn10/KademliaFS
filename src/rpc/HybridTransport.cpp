#include <boost/asio/experimental/awaitable_operators.hpp>

#include "rpc/HybridTransport.hpp"
#include "rpc/RpcMessage.hpp"
#include "types/Contact.hpp"
#include "util/util.hpp"

#include <optional>

HybridTransport::HybridTransport(Contact self, net::any_io_executor ex)
    : TransportEngine{self}, channel_{ex, 64}, udp_{self, ex}, tcp_{self, ex} {
        net::co_spawn(ex, spawn_worker(udp_), net::detached);
        net::co_spawn(ex, spawn_worker(tcp_), net::detached);
}

template<typename T>
awaitable<void>
HybridTransport::spawn_worker(T& transport) {
    while (true) {
        auto req = co_await transport.receive();
        if (!req)
            continue;
        auto [ec] = co_await channel_.async_send(
            {}, std::move(*req), net::as_tuple(net::use_awaitable)
        );
        if (ec)
            LOG(ec.what());
    }
}

awaitable<std::optional<Request>>
HybridTransport::receive() {
    co_return co_await channel_.async_receive(use_awaitable);
}

awaitable<std::optional<RpcMessage>>
HybridTransport::call_rpc(const Contact& target, const RpcMessage& msg) {
    if (msg.type == RpcType::STORE || msg.type == RpcType::FIND_VALUE)
        co_return co_await tcp_.call_rpc(target, msg);
    co_return co_await udp_.call_rpc(target, msg);
}
