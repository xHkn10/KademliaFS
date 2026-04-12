#pragma once

#include "rpc/TransportEngine.hpp"
#include "storage/FileService.hpp"
#include "storage/Metadata.hpp"

#include <boost/asio/experimental/parallel_group.hpp>

#include <cstddef>
#include <fstream>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include "util/FileIO.hpp"
#include <system_error>

namespace fs = std::filesystem;

template<typename Transport>
class FileService;

const static int CHUNK_SZ = 100'000;
const static int MAX_CONCURRENT = 5;

template<typename Transport>
FileService<Transport>::FileService(Contact c, net::any_io_executor disk_ex)
    : boot_c{c}, disk_ex{disk_ex} {}

template<typename Transport>
awaitable<void>
FileService<Transport>::start(const u16 port) {
    auto ex = co_await net::this_coro::executor;
    Contact c{ID::get_random_ID(), 0x7F000001, port};
    auto transport = std::make_unique<Transport>(c, ex);
    
    fs_node = std::make_unique<Node>(
        std::move(c), 
        std::move(transport), 
        "/Users/hakanakbiyik/Projects/Kademlia/DB/db_"+std::to_string(port)
    );
    co_await fs_node->bootstrap({boot_c});
}

template<typename Transport>
awaitable<Metadata>
FileService<Transport>::get_metadata(const std::string name) {
    Metadata md{};
    std::vector<u8> name_v(name.begin(), name.end());
    ID id = util::hash(name_v);
    auto val = co_await fs_node->find_value(id);
    if (!val) {
        LOG("Couldn't find metadata");
        co_return md;
    }
    std::memcpy(&md.sz, val->data(), sizeof(size_t));
    std::memcpy(&md.chunk_sz, val->data() + sizeof(size_t), sizeof(size_t));
    
    size_t rem_bytes = val->size() - 2 * sizeof(size_t);
    md.chunks.resize(rem_bytes / sizeof(Key));
    std::memcpy(md.chunks.data(), val->data() + 2 * sizeof(size_t), rem_bytes);
    co_return md;
}

template<typename Transport>
awaitable<bool>
FileService<Transport>::upload_metadata(const std::string name, const Metadata& md) {
    std::vector<u8> name_v(name.begin(), name.end());
    Key k = util::hash(name_v);
    Value v(2 * sizeof(size_t) + md.chunks.size() * sizeof(Key));
    std::memcpy(v.data(), &md, 2 * sizeof(size_t));
    std::memcpy(v.data() + 2 * sizeof(size_t), md.chunks.data(), md.chunks.size() * sizeof(Key));

    co_await fs_node->store(k, v);

    co_return true;
}

template<typename Transport>
awaitable<bool>
FileService<Transport>::download_file(
    const Metadata& md, const std::string file_path
) {
    if (fs::exists(file_path))
        std::cout << "File already exists btw (will be overridden)" << std::endl;    

    {
        std::ofstream out{file_path, std::ios::binary};
        fs::resize_file(file_path, md.sz);
    }

    auto fd = util::file::open_write(file_path);
    if (fd == -1) {
        LOG("Couldn't open " << file_path);
        co_return false;
    }
    
    const int CHUNK_CNT = (md.sz + md.chunk_sz - 1) / md.chunk_sz;
    auto node_ex = co_await net::this_coro::executor;

    int nxt_idx = 0;
    auto download_chunk = [&] -> awaitable<void> {
        while (true) {
            int idx = nxt_idx++;
            if (idx >= CHUNK_CNT)
                break;
            auto data = co_await fs_node->find_value(md.chunks[idx]);
            if (!data) {
                LOG("FileService::download_file::download_chunk: Couldn't find the chunk to download");
                continue;
            }
            
            co_await net::post(disk_ex, use_awaitable);
            util::file::pwrite(fd, data->data(), data->size(), idx * CHUNK_SZ);
            co_await net::post(node_ex, use_awaitable);

            if (nxt_idx % 10 == 0)
                LOG((double)nxt_idx / CHUNK_CNT);
        }
    };

    auto group = net::experimental::make_parallel_group(
        [&](auto token) {return net::co_spawn(node_ex, download_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, download_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, download_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, download_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, download_chunk(), token);}
    );
    co_await group.async_wait(
        net::experimental::wait_for_all(), use_awaitable
    );

    util::file::close(fd);
    co_return true;
}

template<typename Transport>
awaitable<Metadata>
FileService<Transport>::upload_file(const std::string file_path) {
    Metadata md;
    std::ifstream file{file_path, std::ios::binary};

    std::error_code ec;
    auto filesize = fs::file_size(std::filesystem::path{file_path}, ec);
    if (ec) {
        LOG(ec.message());
        throw std::runtime_error(ec.message());
    }
    auto fd = util::file::open_read(file_path);
    if (fd == -1) {
        LOG("Couldn't open " << file_path);
        co_return md;
    }
    
    const int CHUNK_CNT = (filesize + CHUNK_SZ - 1) / CHUNK_SZ;

    md.sz = filesize;
    md.chunk_sz = CHUNK_SZ;
    md.chunks.resize(CHUNK_CNT);

    int nxt_idx = 0;
    auto node_ex = co_await net::this_coro::executor;
    
    auto upload_chunk = [&] -> awaitable<void> {
        while (true) {
            int idx = nxt_idx++;
            if (idx >= CHUNK_CNT)
                break;
            Value data(CHUNK_SZ);

            co_await net::post(disk_ex, use_awaitable);
            auto n = util::file::pread(fd, data.data(), CHUNK_SZ, idx * CHUNK_SZ);
            data.resize(n);
            md.chunks[idx] = util::hash(data);
            co_await net::post(node_ex, use_awaitable);

            co_await fs_node->store(std::move(data));

            if (nxt_idx % 10 == 0)
                LOG((double)nxt_idx / CHUNK_CNT);
        }
    };

    auto group = net::experimental::make_parallel_group(
        [&](auto token) {return net::co_spawn(node_ex, upload_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, upload_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, upload_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, upload_chunk(), token);},
        [&](auto token) {return net::co_spawn(node_ex, upload_chunk(), token);}
    );
    co_await group.async_wait(
        net::experimental::wait_for_all(), use_awaitable
    );

    util::file::close(fd);
    co_return md;
}
