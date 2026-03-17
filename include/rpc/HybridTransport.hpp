#include "rpc/TransportEngine.hpp"
#include "rpc/UDPTransport.hpp"
#include "rpc/TCPTransport.hpp"
#include <boost/asio/experimental/channel.hpp>

class HybridTransport : public TransportEngine {
using Channel = net::experimental::channel<void(boost::system::error_code, std::optional<Request>)>;
private:
    Channel channel_;
    UDPTransport udp_;
    TCPTransport tcp_;
public:
    HybridTransport(Contact, net::any_io_executor);
    
    awaitable<std::optional<Request>> 
    receive() override;
    
    awaitable<std::optional<RpcMessage>>
    call_rpc(const Contact&, const RpcMessage& msg) override;

private:
    template<typename T> 
    awaitable<void> spawn_worker(T& transport);
};
