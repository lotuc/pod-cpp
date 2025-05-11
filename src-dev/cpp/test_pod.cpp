#include "pod.h"
#include "pod_asio.h"
#include "pod_helper.h"

#include <memory>

define_pod_var_(json, pod_test_pod_add_sync, "pod.test-pod", "add-sync");

void pod_test_pod_add_sync::invoke(lotuc::pod::Context<json> const &ctx,
                                   std::string const &id,
                                   json const &args)
{
  int r{};
  for(auto a : args)
  {
    r += a.get<int>();
  }
  json ret = r;
  ctx.send_invoke_success(id, ret);
}

define_pod_var(json, echo, echo);

void echo_echo::invoke(lotuc::pod::Context<json> const &ctx,
                       std::string const &id,
                       json const &args)
{
  ctx.send_invoke_success(id, args[0]);
}

int main(int argc, char **argv)
{
  namespace pod = lotuc::pod;

  std::cerr << "BABASHKA_POD: " << pod::getenv("BABASHKA_POD") << "\n";
  std::cerr << "BABASHKA_POD_TRANSPORT: " << pod::getenv("BABASHKA_POD_TRANSPORT") << "\n";

  std::string pod_id{};
  if(argc > 1)
  {
    pod_id = argv[1];
  }

  std::unique_ptr<pod::Context<json>> ctx = pod::build_json_ctx(pod_id);

  ctx->add_var(std::make_unique<pod_test_pod_add_sync>());
  ctx->add_var(std::make_unique<echo_echo>());

  pod::Pod<json> pod_{ *ctx };
  pod_.start();

  return 0;
}
