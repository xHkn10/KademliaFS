#include "rpc/RpcMessage.hpp"
#include "rpc/TransportEngine.hpp"
#include "types/Contact.hpp"
#include "util/util.hpp"
#include "node/Node.hpp"

#include <boost/asio/experimental/channel.hpp>

#include <memory>
#include <optional>
#include <set>

Node::Node(Contact self, std::unique_ptr<TransportEngine> transport, std::string db_path)
    : transport_{std::move(transport)}, self_{self}, table_{self.id}, storage_{db_path} {}

awaitable<std::optional<RpcMessage>>
Node::call_rpc(Contact target, RpcMessage msg) {
    co_return co_await transport_->call_rpc(
        std::move(target), std::move(msg)
    );
}

awaitable<void>
Node::bootstrap(const std::vector<Contact>& boot_addrs) {
    for (const Contact& c : boot_addrs) {
        table_.insert(c);
        co_await ping(c);
    }

    // co_await node_lookup(self_.id);
    co_await async_node_lookup(self_.id);
}

// TODO make parallel
awaitable<std::vector<Contact>>
Node::node_lookup(const ID& target) {
    const int k = 20;

    std::vector<Contact> shortlist = table_.get_closest(target);
    std::set<ID> queried;

    while (true) {
        std::vector<Contact> cands;
        bool queried_any = false;

        for (int i = 0; i < k && i < shortlist.size(); ++i) {
            const Contact& c = shortlist[i];
            
            if (queried.contains(c.id))
                continue;            
            
            queried.insert(c.id);
            queried_any = true;

            auto res = co_await call_rpc(
                c, RpcMessage::make_find_node_rpc(
                    self_.id, target, self_.port
                )
            );
            if (!res)
                continue;

            auto contacts = res->get_contacts();
            cands.insert(cands.end(), contacts.begin(), contacts.end());
        }

        if (!queried_any)
            break;

        std::set<ID> seen;
        for (const Contact& c : shortlist)
            seen.insert(c.id);

        for (Contact c : cands)
            if (!seen.contains(c.id) && c.id != self_.id)
                shortlist.push_back(std::move(c));

        std::sort(shortlist.begin(), shortlist.end(),
        [&target](const auto& c1, const auto& c2) {
            return (c1.id ^ target) < (c2.id ^ target);
        });
        
        if (shortlist.size() > k * 2)
            shortlist.resize(k * 2);
    }

    if (shortlist.size() > k)
        shortlist.resize(k);
    
    co_return shortlist;
}

awaitable<std::vector<Contact>>
Node::async_node_lookup(const ID& target) {
    using channel = net::experimental::channel<void(boost::system::error_code, std::pair<ID, std::optional<RpcMessage>>)>;

    auto ex = co_await net::this_coro::executor;
    const int k = 20, alpha = 3;

    std::set<ID> queried, in_flight;
    std::vector<Contact> shortlist = table_.get_closest(target);

    channel results_channel{ex, alpha * 2};

    auto spawn_query = [&target, &ex, this, &results_channel](Contact c) {
        net::co_spawn(ex, [c = std::move(c), &target, this, &results_channel] ->awaitable<void> {
            auto res = co_await call_rpc(
                c, RpcMessage::make_find_node_rpc(
                    self_.id, target, self_.port
                )
            );
            co_await results_channel.async_send(
                {}, {std::move(c.id), std::move(res)}
            );
        }, net::detached);
    };

    while (true) {
        bool spawned_any = false;

        for (int i = 0; i < shortlist.size() && in_flight.size() < alpha; ++i) {
            Contact c = shortlist[i];
            if (queried.contains(c.id) || in_flight.contains(c.id))
                continue;
            spawned_any = true;
            queried.insert(c.id);
            in_flight.insert(c.id);
            spawn_query(std::move(c));
        }

        if (!spawned_any && in_flight.empty())
            break;

        auto [peer, response] = co_await results_channel.async_receive(use_awaitable);
        in_flight.erase(peer);
        if (!response)
            continue;

        {
            std::set<ID> seen;
            for (const Contact& c : shortlist)
                seen.insert(c.id);
            auto contacts = response->get_contacts();
            for (Contact& c : contacts)
                if (c.id != self_.id && !seen.contains(c.id))
                    shortlist.push_back(std::move(c));
        }

        std::sort(shortlist.begin(), shortlist.end(),
        [&target](const auto& c1, const auto& c2) {
            return (c1.id ^ target) < (c2.id ^ target);
        });
        
        if (shortlist.size() > 2 * k)
            shortlist.resize(2 * k);
    }

    if (shortlist.size() > k)
        shortlist.resize(k);

    co_return shortlist;
}


awaitable<std::optional<Value>>
Node::value_lookup(const Key& key) {
    const int k = 20;

    std::vector<Contact> shortlist = table_.get_closest(key);
    std::set<ID> queried;
    
    while (true) {
        bool queried_any = false;
        std::vector<Contact> cands;

        for (int i = 0; i < k && i < shortlist.size(); ++i) {
            const Contact& c = shortlist[i];
            if (queried.contains(c.id))
                continue;
            queried_any = true;
            queried.insert(c.id);

            auto msg = co_await call_rpc(
                c,
                RpcMessage::make_find_value_rpc(self_.id, key, self_.port)
            );

            if (!msg)
                continue;

            if (msg->type == RpcType::FIND_VALUE) {
                if (msg->data.index() == 2)
                    co_return
                        std::get<std::pair<Key, Value>>(msg->data).second;
            } else if (msg->type == RpcType::FIND_NODE) {
                if (msg->data.index() == 3) {
                    auto& v = std::get<std::vector<Contact>>(msg->data);
                    cands.insert(cands.end(), v.begin(), v.end());
                }
            }
        }

        if (!queried_any)
            break;

        std::set<ID> seen;
        for (const Contact& c : shortlist)
            seen.insert(c.id);

        for (Contact& c : cands)
            if (!seen.contains(c.id) && c.id != self_.id)
                shortlist.push_back(std::move(c));

        std::sort(shortlist.begin(), shortlist.end(),
        [&key](const auto& c1, const auto& c2){
            return (c1.id ^ key) < (c2.id ^ key);
        });

        if (shortlist.size() > 2 * k)
            shortlist.resize(2 * k);
    }

    co_return std::nullopt;
}

awaitable<std::optional<Value>>
Node::async_value_lookup(const Key& key) {
    using channel = net::experimental::channel<void(boost::system::error_code, std::pair<ID, std::optional<RpcMessage>>)>;
    auto ex = co_await net::this_coro::executor;
    const int k = 20, alpha = 3;

    std::vector<Contact> shortlist = table_.get_closest(key);
    std::set<ID> queried, in_flight;

    auto results_channel = std::make_shared<channel>(ex, alpha * 2);

    auto spawn_query = [&ex, this, weak_channel = std::weak_ptr<channel>{results_channel}](Contact c, Key k) {
        net::co_spawn(ex, [c = std::move(c), k = std::move(k), this, weak_channel] -> awaitable<void> {
            auto res = co_await call_rpc(
                c, RpcMessage::make_find_value_rpc(
                    self_.id, k, self_.port
                )
            );
            if (auto chan = weak_channel.lock())
                co_await chan->async_send(
                    {}, {std::move(c.id), std::move(res)}
                );
        }, net::detached);
    };

    while (true) {
        bool spawned_any = false;
        for (int i = 0; i < shortlist.size() && in_flight.size() < alpha; ++i) {
            Contact c = shortlist[i];
            if (queried.contains(c.id) || in_flight.contains(c.id))
                continue;
            queried.insert(c.id);
            in_flight.insert(c.id);
            spawned_any = true;
            spawn_query(std::move(c), key);
        }

        if (!spawned_any && in_flight.empty())
            break;

        auto [peer, response] = co_await results_channel->async_receive(use_awaitable);
        in_flight.erase(peer);

        if (!response)
            continue;
        if (response->type == RpcType::FIND_VALUE && response->data.index() == 2)
            co_return
                std::get<std::pair<Key, Value>>(response->data).second;

        if (response->type == RpcType::FIND_NODE && response->data.index() == 3) {
            std::set<ID> seen;
            for (const Contact& c : shortlist)
                seen.insert(c.id);

            auto cands = response->get_contacts();
            for (Contact& c : cands)
                if (!seen.contains(c.id) && c.id != self_.id)
                    shortlist.push_back(std::move(c));

            std::sort(shortlist.begin(), shortlist.end(),
            [&key](const auto& c1, const auto& c2) {
                return (c1.id ^ key) < (c2.id ^ key);
            });
            
            if (shortlist.size() > 2 * k)
                shortlist.resize(2 * k);
        }
    }

    co_return std::nullopt;
}

awaitable<std::vector<Contact>>
Node::find_node(const ID& id) {
    // co_return co_await node_lookup(id);
    co_return co_await async_node_lookup(id);
}

awaitable<void>
Node::store(Value value) {
    auto ex = co_await net::this_coro::executor;
    Key key = util::hash(value);
    const size_t k = 20;

    // std::vector<Contact> best_k = co_await node_lookup(key);
    std::vector<Contact> best_k = co_await async_node_lookup(key);
    best_k.push_back(self_); 
    std::sort(
        best_k.begin(), best_k.end(),
        [&key](const auto& c1, const auto& c2) {
            return (c1.id ^ key) < (c2.id ^ key);
        }
    );

    if (best_k.size() > k)
        best_k.resize(k);

    for (const Contact& c : best_k) {
        if (c.id == self_.id)
            storage_.store(key, value);
        else
            net::co_spawn(
                ex, call_rpc(
                    c,
                    RpcMessage::make_store_rpc(
                        self_.id, key, value, self_.port
                    )),
                    net::detached
            );
    }
}

awaitable<void>
Node::store(Key key, Value value) {
    auto ex = co_await net::this_coro::executor;
    const size_t k = 20;
    
    // std::vector<Contact> best_k = co_await node_lookup(key);
    std::vector<Contact> best_k = co_await async_node_lookup(key);

    best_k.push_back(self_);
    std::sort(
        best_k.begin(), best_k.end(),
        [&key](const auto& c1, const auto& c2) {
            return (c1.id ^ key) < (c2.id ^ key);
        }
    );

    if (best_k.size() > k)
        best_k.resize(k);

    for (const Contact& c : best_k) {
        if (c.id == self_.id)
            storage_.store(key, value);
        else
            net::co_spawn(
                ex, call_rpc(
                    c,
                    RpcMessage::make_store_rpc(
                        self_.id, key, value, self_.port
                    )),
                    net::detached
            );
    }
}

awaitable<std::optional<RpcMessage>>
Node::ping(const Contact& c) {
    co_return co_await call_rpc(
        c, RpcMessage::make_ping_rpc(self_.id, c.id, self_.port)
    );
}

awaitable<std::optional<Value>>
Node::find_value(const ID& key) {
    auto val = storage_.retrieve(key);
    if (val)
        co_return *val;
    // co_return co_await value_lookup(key);
    co_return co_await async_value_lookup(key);
}

awaitable<void>
Node::listen() {
    auto ex = co_await net::this_coro::executor;
    while (true) {
        auto req = co_await transport_->receive();
        if (!req || req->msg.is_response)
            continue;
        net::co_spawn(
            ex, handle_request(std::move(*req)), net::detached
        );
    }
}

awaitable<void>
Node::handle_request(
    Request req
) {
    table_.insert(req.c);

    switch (req.msg.type) {
        case RpcType::PING:
            co_await handle_ping(std::move(req));
            break;
        case RpcType::PONG:
            break;
        case RpcType::STORE:
            co_await handle_store(std::move(req));
            break;
        case RpcType::FIND_NODE:
            co_await handle_find_node(std::move(req));
            break;
        case RpcType::FIND_VALUE:
            co_await handle_find_value(std::move(req));
            break;
    }
}

awaitable<void> 
Node::handle_ping(Request req) {
    co_await req.send_response(
        RpcMessage::make_pong_rpc(
            self_.id, req.msg.transactionID, self_.port
        )
    );
}

awaitable<void>
Node::handle_store(Request req) {
    // TODO
    auto [k, v] = req.msg.get_key_value();
    storage_.store(k, v);

    co_await req.send_response(
        RpcMessage::make_store_ack_reply(
            self_.id, req.msg.transactionID, self_.port
        )
    );
}

awaitable<void>
Node::handle_find_node(Request req) {
    co_await req.send_response(
        RpcMessage::make_k_closest_reply(
            self_.id, table_.get_closest(req.msg.get_id()), req.msg.transactionID, self_.port
        )
    );
}

awaitable<void>
Node::handle_find_value(Request req) {
    ID key = req.msg.get_id();
    
    auto val = storage_.retrieve(key);
    if (val) {
        co_await req.send_response(
            RpcMessage::make_key_value_reply(
                self_.id, key, *val, req.msg.transactionID, self_.port
            )
        );
    } else {
        co_await req.send_response(
            RpcMessage::make_k_closest_reply(
                self_.id, table_.get_closest(key),
                req.msg.transactionID, self_.port
            )
        );
    }
}
