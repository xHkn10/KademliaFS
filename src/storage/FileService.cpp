#include "storage/FileService.hpp"
#include "node/Node.hpp"
#include "rpc/NetworkTransport.hpp"
#include "util/util.hpp"

#include <chrono>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;
const static int CHUNK_SZ = 100'000;
const static int MAX_CONCURRENT = 5;

FileService::FileService(Node& node) : node_{node} {}

awaitable<Metadata>
FileService::upload_file(const std::string file_path) {
    Metadata md;
    std::ifstream file{file_path, std::ios::binary};

    std::error_code ec;
    auto filesize = fs::file_size(std::filesystem::path{file_path}, ec);
    if (ec) {
        LOG(ec.message());
        throw std::runtime_error(ec.message());
    }
    
    const int CHUNK_CNT = (filesize + CHUNK_SZ - 1) / CHUNK_SZ;

    md.sz = filesize;
    md.chunk_sz = CHUNK_SZ;
    md.chunks.resize(CHUNK_CNT);

    int nxt_idx = 0;
    int pending = CHUNK_CNT;
    
    auto upload_chunk = [&] -> awaitable<void> {
        while (true) {
            int idx = nxt_idx++;
            if (idx >= CHUNK_CNT)
                break;
            Value data(CHUNK_SZ);
            file.seekg(idx * CHUNK_SZ, std::ios::beg);
            file.read(reinterpret_cast<char*>(data.data()), CHUNK_SZ);
            data.resize(file.gcount());

            md.chunks[idx] = util::hash(data);
            co_await node_.store(std::move(data));
            --pending;
        }
    };

    auto ex = co_await net::this_coro::executor;
    for (int i = 0; i < MAX_CONCURRENT && i < CHUNK_SZ; ++i)
        net::co_spawn(ex, upload_chunk(), net::detached);
    
    net::steady_timer timer{ex};
    while (pending > 0) {
        LOG((double)(CHUNK_CNT - pending) / CHUNK_CNT);
        timer.expires_after(std::chrono::seconds{3});
        auto [ec] = co_await timer.async_wait(net::as_tuple(use_awaitable));        
    }

    co_return md;
}

awaitable<bool>
FileService::download_file(
    const Metadata& md, const std::string file_path
) {
    if (fs::exists(file_path))
        std::cout << "File already exists btw (will be overridden)" << std::endl;    

    auto ex = co_await net::this_coro::executor;

    std::ofstream out{file_path, std::ios::binary};
    fs::resize_file(file_path, md.sz);
    
    const int CHUNK_CNT = (md.sz + md.chunk_sz - 1) / md.chunk_sz;
    int pending = CHUNK_CNT;

    int nxt_idx = 0;
    auto download_chunk = [&] -> awaitable<void> {
        while (true) {
            int idx = nxt_idx++;
            if (idx >= CHUNK_CNT)
                break;
            auto data = co_await node_.find_value(md.chunks[idx]);
            if (!data) {
                LOG("FileService::download_file::download_chunk: Couldn't find the chunk to download");
                continue;
            }
            out.seekp(idx * CHUNK_SZ);
            out.write(reinterpret_cast<char*>(data->data()), data->size());
            --pending;
        }
    };

    for (int i = 0; i < MAX_CONCURRENT && i < CHUNK_SZ; ++i)
        net::co_spawn(ex, download_chunk(), net::detached);

    net::steady_timer timer{ex};
    while (pending > 0) {
        LOG((double)(CHUNK_CNT - pending) / CHUNK_CNT);
        timer.expires_after(std::chrono::seconds{3});
        auto [ec] = co_await timer.async_wait(net::as_tuple(use_awaitable));
    }

    out.close();
    co_return true;
}

