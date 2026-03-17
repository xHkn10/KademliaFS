#include "rpc/TCPTransport.hpp"
#include "rpc/RpcMessage.hpp"
#include "util/util.hpp"

#include <optional>

TCPTransport::TCPTransport(Contact self, net::any_io_executor ex)
    : TransportEngine{self}, acceptor_{ex}, channel_{ex, 64} {
    acceptor_.open(tcp::v4());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(tcp::endpoint(tcp::v4(), self.port));
    acceptor_.listen();
    net::co_spawn(ex, accept_loop(), net::detached);
}

awaitable<std::optional<Request>>
TCPTransport::receive() {
    co_return co_await channel_.async_receive(use_awaitable);
}

awaitable<void>
TCPTransport::accept_loop() {
    auto ex = co_await net::this_coro::executor;
    while (true) {
        auto sock = std::make_shared<tcp::socket>(
            co_await acceptor_.async_accept(use_awaitable)
        );
        net::co_spawn(ex, read_and_push(std::move(sock)), net::detached);
    }
}

awaitable<void>
TCPTransport::read_and_push(std::shared_ptr<tcp::socket> sock) {
    u32 len;
    std::vector<u8> buffer;

    {
        auto [ec, n] = co_await net::async_read(
            *sock, net::buffer(&len, 4), net::as_tuple(use_awaitable)
        );
        if (ec) {
            LOG("TCPTransport::receive: " << ec.what());
            co_await channel_.async_send({}, std::nullopt, use_awaitable);
            co_return;
        }
        buffer.resize(len);
    }
    {
        auto [ec, n] = co_await net::async_read(
            *sock, net::buffer(buffer), net::as_tuple(use_awaitable)
        );
        if (ec) {
            LOG("TCPTransport::receive: " << ec.what());
            co_await channel_.async_send({}, std::nullopt, use_awaitable);
            co_return;
        }
    }

    Request req;
    auto msg = RpcMessage::deserialize(std::move(buffer));
    if (!msg)
        co_return;
    {
        auto endpoint = sock->remote_endpoint();
        req.c = {msg->senderID, endpoint.address().to_v4().to_uint(), msg->sender_port};
    }
    req.msg = std::move(*msg);

    req.send_response = [sock](RpcMessage msg) mutable -> awaitable<void> {
        std::vector<u8> bytes = msg.serialize();
        u32 len = bytes.size();
        try {
            co_await net::async_write(*sock, net::buffer(&len, 4), use_awaitable);
            co_await net::async_write(*sock, net::buffer(bytes), use_awaitable);
        } catch (const std::exception& e) {
            LOG("send_response lambda: " << e.what());
        } catch (...) {
            LOG("send_response lambda: unknown err");
        }
    };

    co_await channel_.async_send({}, std::move(req), use_awaitable);
}

awaitable<std::optional<RpcMessage>>
TCPTransport::call_rpc(const Contact& target, const RpcMessage& msg) {    
    auto ex = co_await net::this_coro::executor;
    tcp::socket sock{ex};
    
    try {
        std::vector<u8> bytes = msg.serialize();
        tcp::endpoint addr{
            net::ip::make_address_v4(target.ip), target.port
        };

        co_await sock.async_connect(addr, use_awaitable);
        u32 len = bytes.size();
        co_await net::async_write(sock, net::buffer(&len, 4), use_awaitable);
        co_await net::async_write(sock, net::buffer(bytes), use_awaitable);
    } catch (const std::exception& e) {
        LOG("TCPTransport::call_rpc: " << e.what());
        co_return std::nullopt;
    } catch (...) {
        LOG("TCPTransport::call_rpc: unknown err");
        co_return std::nullopt;
    }

    u32 response_len;
    {
        auto [ec, n] = co_await net::async_read(
            sock, net::buffer(&response_len, 4), net::as_tuple(use_awaitable)
        );
        if (ec == boost::asio::error::eof) {
            // LOG("TCPTransport::call_rpc: Peer hung up gracefully.");
            co_return std::nullopt;
        } else if (ec) {
            LOG("TCPTransport::call_rpc: " << ec.what());
            co_return std::nullopt;
        }
    }
    std::vector<u8> response(response_len);
    {
        auto [ec, n] = co_await net::async_read(
            sock, net::buffer(response), net::as_tuple(use_awaitable)
        );
        if (ec == boost::asio::error::eof) {
            // LOG("TCPTransport::call_rpc: Peer hung up gracefully.");
            co_return std::nullopt;
        } else if (ec) {
            LOG("TCPTransport::call_rpc: " << ec.what());
            co_return std::nullopt;
        }
    }
    
    co_return RpcMessage::deserialize(response);
}
