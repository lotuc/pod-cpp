#include "pod.h"
#include "pod_asio.h"
#include "pod_helper.h"

#include <memory>

define_pod_var_(pod_test_pod_add_sync, "pod.test-pod", "add-sync");

void pod_test_pod_add_sync::invoke(lotuc::pod::Context const &ctx,
                                   std::string const &id,
                                   json const &args)
{
  int r{};
  for(auto a : args)
  {
    r += a.get<int>();
  }
  ctx._transport->send_invoke_success(id, ctx._encoder->encode(r));
}

define_pod_var(echo, echo);

void echo_echo::invoke(lotuc::pod::Context const &ctx, std::string const &id, json const &args)
{
  auto res = ctx._encoder->encode(args[0]);
  ctx._transport->send_invoke_success(id, res);
}

int main(int argc, char **argv)
{
  namespace pod = lotuc::pod;

  std::cerr << "BABASHKA_POD: " << pod::Pod::getenv("BABASHKA_POD") << "\n";
  std::cerr << "BABASHKA_POD_TRANSPORT: " << pod::Pod::getenv("BABASHKA_POD_TRANSPORT") << "\n";

  std::string pod_id{};
  if(argc > 1)
  {
    pod_id = argv[1];
  }

  std::unique_ptr<pod::Context> ctx = pod::build_ctx(pod_id);

  ctx->add_var(std::make_unique<pod_test_pod_add_sync>());
  ctx->add_var(std::make_unique<echo_echo>());

  pod::Pod pod_{ *ctx };
  pod_.start();

  return 0;
}
