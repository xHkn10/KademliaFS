#include "rpc/TCPTransport.hpp"
// #include "rpc/UDPTransport.hpp"
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

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream f1(a, std::ios::binary), f2(b, std::ios::binary);
    std::vector buf1((std::istreambuf_iterator<char>(f1)), {});
    std::vector buf2((std::istreambuf_iterator<char>(f2)), {});
    return buf1 == buf2;
}

int main(void) {
    auto io = boost::asio::io_context{};
    const int n = 50;
    std::vector<Node> nodes;
    nodes.reserve(n);

    for (int i = 0; i < n; ++i) {
        const u16 port = 3169 + i;
        Contact c{ID::get_random_ID(), 0x7F000001, port};
        
        auto node_strand = net::make_strand(io);

        auto transport = std::make_unique<TCPTransport>(c, node_strand);
        nodes.emplace_back(std::move(c), std::move(transport), "/Users/hakanakbiyik/Projects/Kademlia/DB/db_"+std::to_string(i));

        net::co_spawn(node_strand, nodes.back().listen(), net::detached);
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

    std::string file = "/Users/hakanakbiyik/Projects/Kademlia/testfiles/testfile10MB.bin";
    std::string target_loc = "/Users/hakanakbiyik/Projects/Kademlia/downloads/downloaded.bin";
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
    };

    net::co_spawn(io, test_fs(), net::detached);
    io.run();
}
