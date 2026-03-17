#include "rpc/TCPTransport.hpp"
#include "rpc/UDPTransport.hpp"
#include "rpc/HybridTransport.hpp"
#include "storage/FileService.hpp"
#include "node/Node.hpp"
#include "rpc/NetworkTransport.hpp"
#include "types/Contact.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace net = boost::asio;

std::map<std::pair<int, RpcType>, int> heat_map;

void print_heat_map() {
    std::cout << "STORES" << std::endl;
    for (auto [k, v] : heat_map)
        if (k.second == RpcType::STORE)
            std::cout << "port " << k.first << ": " << v << std::endl;
    std::cout << std::endl;
    std::cout << "FIND VALUES" << std::endl;
    for (auto [k, v] : heat_map)
        if (k.second == RpcType::FIND_VALUE)
            std::cout << "port " << k.first << ": " << v << std::endl;
    std::cout << std::endl;
    std::cout << "FIND NODES" << std::endl;
    for (auto [k, v] : heat_map)
        if (k.second == RpcType::FIND_NODE)
            std::cout << "port " << k.first << ": " << v << std::endl;
    std::cout << std::endl;
    std::cout << "PINGS" << std::endl;
    for (auto [k, v] : heat_map)
        if (k.second == RpcType::PING)
            std::cout << "port " << k.first << ": " << v << std::endl;
    std::cout << std::endl;
    std::cout << "PONGS" << std::endl;
    for (auto [k, v] : heat_map)
        if (k.second == RpcType::PONG)
            std::cout << "port " << k.first << ": " << v << std::endl;
    std::cout << std::endl;
}

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream f1(a, std::ios::binary), f2(b, std::ios::binary);
    std::vector buf1((std::istreambuf_iterator<char>(f1)), {});
    std::vector buf2((std::istreambuf_iterator<char>(f2)), {});
    return buf1 == buf2;
}

int main(void) {
    auto io = boost::asio::io_context{};
    const int n = 250;
    std::vector<Node> nodes;
    nodes.reserve(n);

    for (int i = 0; i < n; ++i) {
        const u16 port = 3169 + i;
        Contact c{ID::get_random_ID(), 0x7F000001, port};
        
        auto transport = std::make_unique<TCPTransport>(c, io.get_executor());
        nodes.emplace_back(std::move(c), std::move(transport), "./DB/db_"+std::to_string(i));
        net::co_spawn(io, nodes.back().listen(), net::detached);
    }

    auto ping_test = [&]() -> awaitable<void> {
        for (int i = 0; i < 10; ++i) {
            int rand1 = std::rand() % n;
            int rand2;
            do {
                rand2 = std::rand() % n;
            } while (rand1 == rand2);
            
            std::cout
            << "Ping from "
            << nodes[rand1].self_.port
            << " to "
            << nodes[rand2].self_.port
            << std::endl;

            co_await nodes[rand1].ping(nodes[rand2].self_);
        }
    };

    std::string file = "testfiles/testfile10MB.bin";
    std::string target_loc = "downloads/downloaded.bin";
    auto test_fs = [&]() -> awaitable<void> {
        {
            auto s = std::chrono::high_resolution_clock::now();
            std::cout << "BOOTSTRAPPING..." << std::endl;
            for (int i = 1; i < n; ++i)
                co_await nodes[i].bootstrap({nodes.front().self_});
            std::cout << "BOOTSTRAPPING DONE" << std::endl;
            auto e = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(e - s).count();
            std::cout << "Boot took " << duration << " seconds" << std::endl;
        }
        {
            auto s = std::chrono::high_resolution_clock::now();
            FileService fs{nodes[3]};
            auto md = co_await fs.upload_file(file);
    
            std::cout << "Uploaded" << std::endl;
    
            co_await fs.download_file(md, target_loc);
    
            std::cout << "Downloaded" << std::endl;
            auto e = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(e - s).count();
            std::cout << "Took " << duration << " seconds" << std::endl;
    
            if (!files_equal(file, target_loc))
                std::cout << "Files are not the same" << std::endl;
            else
                std::cout << "Files are fine" << std::endl;
        }

        // print_heat_map(); 
    };

    net::co_spawn(io, test_fs(), net::detached);

    io.run();
}
