#include "rpc/UDPTransport.hpp"
#include "util/util.hpp"
#include <functional>
#include <optional>

UDPTransport::UDPTransport(Contact self, net::any_io_executor ex)
    : TransportEngine{self}, socket_{ex, {udp::v4(), self.port}}{}

awaitable<std::optional<Request>>
UDPTransport::receive() {
    try {
        std::vector<u8> bytes(4096);
        udp::endpoint sender;
        size_t n = co_await socket_.async_receive_from(net::buffer(bytes), sender, use_awaitable);
        bytes.resize(n);
        auto msg = RpcMessage::deserialize(std::move(bytes));
        if (!msg)
            co_return std::nullopt;

        if (msg->is_response) {
            auto it = channels_.find(msg->transactionID);
            if (it == channels_.end())
                LOG("UDPTransport::receive: non existing key");
            else
                it->second->try_send(boost::system::error_code{}, std::move(*msg));
            co_return std::nullopt;
        }

        Request req{
            .c{msg->senderID, sender.address().to_v4().to_uint(), sender.port()},
            .msg{std::move(*msg)}
        };

        req.send_response = [sender, this](RpcMessage msg) -> awaitable<void> {
            std::vector<u8> bytes = msg.serialize();
            try {
                co_await this->socket_.async_send_to(
                    net::buffer(bytes),
                    sender,
                    use_awaitable
                );
            } catch (const std::exception& e) {
                LOG("UDPTransport::receive lambda: " << e.what());
            } catch (...) {
                LOG("UDPTransport::receive lambda: unknown error");
            }
        };
        
        co_return req;
    } catch (const std::exception& e) {
        LOG("UDPTransport::receive: " << e.what());
        co_return std::nullopt;
    } catch (...) {
        LOG("UDPTransport::receive: unknown err");
        co_return std::nullopt;
    }
}

awaitable<std::optional<RpcMessage>>
UDPTransport::call_rpc(const Contact& target, const RpcMessage& rpc) {
    try {
        std::vector<u8> bytes = rpc.serialize();

        udp::endpoint addr{
            net::ip::make_address_v4(target.ip),
            target.port
        };

        auto ex = co_await net::this_coro::executor;
        auto channel = std::make_shared<Channel>(ex, 1);
        channels_[rpc.transactionID] = channel;

        co_await socket_.async_send_to(
            net::buffer(bytes),
            addr,
            use_awaitable
        );

        using namespace std::chrono_literals;
        RpcMessage msg = co_await channel->async_receive(net::cancel_after(60s, use_awaitable));
        channels_.erase(msg.transactionID);
        co_return msg;
    } catch (const std::exception& e) {
        LOG("UDPTransport::call_rpc: " << e.what());
        co_return std::nullopt;
    } catch (...) {
        LOG("UDPTransport::call_rpc: unknown err");
        co_return std::nullopt;
    }
}
