#include "rpc/TCPTransport.hpp"
// #include "rpc/UDPTransport.hpp"
#include "rpc/HybridTransport.hpp"
#include "node/Node.hpp"
#include "rpc/NetworkTransport.hpp"
#include "types/Contact.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace net = boost::asio;

int main() {
    const int n = 50;
    const int workers = std::thread::hardware_concurrency();

    std::vector<std::shared_ptr<net::io_context>> ios;
    ios.reserve(workers);
    for (int i = 0; i < workers; ++i) {
        auto io = std::make_shared<net::io_context>(1);
        ios.push_back(io);
    }

    std::vector<Node> nodes;
    nodes.reserve(n);

    for (int i = 0; i < n; ++i) {
        const u16 port = 3169 + i;
        Contact c{i == 0 ? ID{} : ID::get_random_ID(), 0x7F000001, port};

        auto& io = *ios[i % workers];

        auto transport = std::make_unique<TCPTransport>(c, io.get_executor());
        nodes.emplace_back(std::move(c), std::move(transport), "/Users/hakanakbiyik/Projects/Kademlia/DB/db_"+std::to_string(port));

        net::co_spawn(io, nodes.back().listen(), net::detached);
    }

    auto boot = [&nodes] -> awaitable<void> {
        auto s = std::chrono::high_resolution_clock::now();
        std::cout << "BOOTSTRAPPING..." << std::endl;
        for (int i = 1; i < n; ++i)
            co_await nodes[i].bootstrap({nodes.front().self_});
        std::cout << "BOOTSTRAPPING DONE" << std::endl;
        auto e = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(e - s).count();
        std::cout << "Boot took " << duration << " seconds" << std::endl;
    };

    net::co_spawn(*ios[0], boot(), net::detached);

    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (int i = 0; i < workers; ++i)
        threads.push_back(std::thread{
            [io = ios[i]] {
                io->run();
            }
        });
    for (auto& t : threads)
        t.join();
}
