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
#include <thread>

namespace net = boost::asio;

int main(void) {
    const int workers = std::thread::hardware_concurrency();
    
    net::thread_pool disk_pool{static_cast<size_t>(workers)};
    net::io_context io{1};

    Contact boot_c{ID{}, 0x7F000001, 3169};

    std::string file = "testfiles/testfile10MB.bin";
    auto upload = [&]() -> awaitable<void> {
        FileService<TCPTransport> fs{boot_c, disk_pool.executor()};
        co_await fs.start(7001);

        auto s = std::chrono::high_resolution_clock::now();
        Metadata md = co_await fs.upload_file(file);
        co_await fs.upload_metadata("dumb_file", md);
        auto e = std::chrono::high_resolution_clock::now();

        std::cout << "Uploaded" << std::endl;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(e - s).count();
        std::cout << "Upload took " << duration << " seconds" << std::endl;

        auto ex = co_await net::this_coro::executor;
        net::steady_timer timer{ex};
        timer.expires_after(std::chrono::seconds{100000000});
        co_await timer.async_wait(use_awaitable);
    };

    net::co_spawn(io, upload(), net::detached);
    io.run();
}
