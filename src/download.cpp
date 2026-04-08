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
#include <vector>
#include <thread>

namespace net = boost::asio;

bool files_equal(const std::string& a, const std::string& b) {
    std::ifstream f1(a, std::ios::binary), f2(b, std::ios::binary);
    std::vector buf1((std::istreambuf_iterator<char>(f1)), {});
    std::vector buf2((std::istreambuf_iterator<char>(f2)), {});
    return buf1 == buf2;
}

int main(void) {
    const int workers = std::thread::hardware_concurrency();
    
    net::thread_pool disk_pool{static_cast<size_t>(workers)};
    net::io_context io{1};

    Contact boot_c{ID{}, 0x7F000001, 3169};

    std::string file = "/Users/hakanakbiyik/Projects/Kademlia/testfiles/testfile10MB.bin";
    std::string target_loc = "/Users/hakanakbiyik/Projects/Kademlia/downloads/downloaded.bin";

    auto download = [&]() -> awaitable<void> {
        FileService<TCPTransport> fs{boot_c, disk_pool.executor()};
        co_await fs.start(8001);
        
        auto s = std::chrono::high_resolution_clock::now();
        Metadata md = co_await fs.get_metadata("dumb_file");
        if (md.chunks.size() == 0) {
            LOG("Couldn't download metadata");
            co_return;
        }
        bool b = co_await fs.download_file(md, target_loc);
        if (!b) {
            LOG("Couldn't download file");
            co_return;
        }
        auto e = std::chrono::high_resolution_clock::now();

        std::cout << "Downloaded" << std::endl;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(e - s).count();
        std::cout << "Download took " << duration << " seconds" << std::endl;

        if (!files_equal(file, target_loc))
            std::cout << "Files are not the same" << std::endl;
        else
            std::cout << "Files are fine" << std::endl;
    };

    net::co_spawn(io, download(), net::detached);
    io.run();
}
