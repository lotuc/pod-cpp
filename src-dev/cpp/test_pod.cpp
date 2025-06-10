#include "test_ns.h"
#include "pod_json.h"

int main(int argc, char **argv)
{
  namespace pod = lotuc::pod;

  std::cerr << "BABASHKA_POD: " << pod::getenv("BABASHKA_POD") << "\n";
  std::cerr << "BABASHKA_POD_TRANSPORT: " << pod::getenv("BABASHKA_POD_TRANSPORT") << "\n";

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

  std::unique_ptr<pod::Context<json>> ctx = pod::build_json_ctx(pod_id);
  ctx->add_ns(test_pod::build_ns());
  ctx->add_ns(test_pod::build_defer_ns());
  pod::build_pod(*ctx, max_concurrent).read_eval_loop();
  return 0;
}
