#include "pod.h"
#include "test_ns.h"
#include "pod_json.h"
#include "jsonrpc_transport.h"
#include <memory>
#include <sstream>

int main(int argc, char **argv)
{
  namespace pod = lotuc::pod;
  std::string pod_id{};
  int max_concurrent{ 2 };
  if(argc > 1)
  {
    pod_id = argv[1];
  }
  if(argc > 2)
  {
    std::stringstream ss(argv[2]);
    ss >> max_concurrent;
  }

  std::unique_ptr<pod::JsonRpcTransport> transport;
  if(pod::getenv("USE_TCP") == "true")
  {
    asio::io_context io_context;
    int port{ 0 };
    std::string s = pod::getenv("PORT");
    if(!s.empty())
    {
      std::stringstream ss{ s };
      ss >> port;
    }
    transport = std::make_unique<pod::TcpLinedJsonTransport>(io_context, port);
  }
  else
  {
    transport = std::make_unique<pod::StdInOutLinedJsonTransport>();
  }
  std::unique_ptr<pod::Context<json>> ctx
    = pod::build_jsonrpc_ctx(pod_id, transport.get(), nullptr);
  ctx->add_ns(test_pod::build_ns());
  ctx->add_ns(test_pod::build_defer_ns());
  pod::build_pod(*ctx, max_concurrent).read_eval_loop();
  return 0;
}
