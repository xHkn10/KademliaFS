#include "rpc/RpcMessage.hpp"
#include "rpc/TransportEngine.hpp"
#include "util/util.hpp"
#include "node/Node.hpp"

#include <memory>
#include <optional>
#include <set>

// bla bla

Node::Node(Contact self, std::unique_ptr<TransportEngine> transport, std::string db_path)
    : transport_{std::move(transport)}, self_{self}, table_{self.id}, storage_{db_path} {}

awaitable<std::optional<RpcMessage>>
Node::call_rpc(Contact target, const RpcMessage& msg) {
    co_return co_await transport_->call_rpc(
        std::move(target), msg
    );
}

awaitable<void>
Node::bootstrap(const std::vector<Contact>& boot_addrs) {
    for (const Contact& c : boot_addrs) {
        table_.insert(c);
        co_await ping(c);
    }

    co_await node_lookup(self_.id);
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

            auto msg = RpcMessage::make_find_node_rpc(
                self_.id, target, self_.port
            );
            auto res = co_await call_rpc(c, msg);
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

        for (const Contact& c : cands)
            if (!seen.contains(c.id) && c.id != self_.id)
                shortlist.push_back(c);

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
        for (Contact& c : shortlist)
            seen.insert(c.id);

        for (Contact& c : cands)
            if (!seen.contains(c.id) && c.id != self_.id)
                shortlist.push_back(c);

        std::sort(shortlist.begin(), shortlist.end(),
        [&key](const auto& c1, const auto& c2){
            return (c1.id ^ key) < (c2.id ^ key);
        });

        if (shortlist.size() > 2 * k)
            shortlist.resize(2 * k);
    }

    co_return std::nullopt;
}

awaitable<std::vector<Contact>>
Node::find_node(const ID& id) {
    co_return co_await node_lookup(id);
}

awaitable<void>
Node::store(Value value) {
    auto ex = co_await net::this_coro::executor;
    Key key = util::hash(value);
    const size_t k = 20;

    std::vector<Contact> best_k = co_await node_lookup(key);
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
    
    std::vector<Contact> best_k = co_await node_lookup(key);
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
    val = co_await value_lookup(key);
    co_return val;
    co_return co_await value_lookup(key);
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
