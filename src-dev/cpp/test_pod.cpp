#include "pod.h"
#include "pod_helper.h"

#include <climits>
#include <memory>

// https://github.com/babashka/pods/blob/master/test-pod/pod/test_pod.clj
namespace test_pod
{
  define_pod_var(json, add_sync, "add-sync", "{:doc \"add the arguments\"}", false);
  define_pod_var_async(json, range_stream, "");
  define_pod_var_sync(json, echo, "");
  define_pod_var_sync(json, error, "");
  define_pod_var_sync(json, print, "");
  define_pod_var_sync(json, print_err, "");
  define_pod_var_sync(json, return_nil, "");
  define_pod_var_code(json, do_twice, "", "(defmacro do-twice [x] `(do ~x ~x))");
  define_pod_var_code(json, fn_call, "", "(defn fn-call [f x] (f x))");

  static void load_vars(lotuc::pod::Namespace<json> &ns)
  {
    ns.add_var(std::make_unique<add_sync>());
    ns.add_var(std::make_unique<range_stream>());
    ns.add_var(std::make_unique<error>());
    ns.add_var(std::make_unique<echo>());
    ns.add_var(std::make_unique<error>());
    ns.add_var(std::make_unique<print>());
    ns.add_var(std::make_unique<print_err>());
    ns.add_var(std::make_unique<return_nil>());
    ns.add_var(std::make_unique<do_twice>());
    ns.add_var(std::make_unique<fn_call>());
  }

  static std::unique_ptr<lotuc::pod::Namespace<json>> build_ns()
  {
    auto ns = std::make_unique<lotuc::pod::Namespace<json>>("test-pod");
    load_vars(*ns);
    return ns;
  }

  static std::unique_ptr<lotuc::pod::Namespace<json>> build_defer_ns()
  {
    return std::make_unique<lotuc::pod::Namespace<json>>("test-pod-defer", true, load_vars);
  }
}

namespace test_pod
{
  void add_sync::derefer::deref()
  {
    int r{};
    for(auto const &a : args)
    {
      r += a.get<int>();
    }
    ctx.send_invoke_success(id, r);
  }

  void range_stream::derefer::deref()
  {
    int start = 0, end = 0, step = 1;
    size_t n = args.size();
    if(n == 0)
    {
      start = 0;
      end = INT_MAX;
      step = 1;
    }
    else if(n == 1)
    {
      start = 0;
      end = args[0].get<int>();
      step = 1;
    }
    else if(n == 2)
    {
      start = args[0].get<int>();
      end = args[1].get<int>();
      step = 1;
    }
    else
    {
      start = args[0].get<int>();
      end = args[1].get<int>();
      step = args[2].get<int>();
    }
    for(int i = start; i < end; i += step)
    {
      ctx.send_invoke_callback(id, i);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ctx.send_invoke_success(id);
  }

  void echo::derefer::deref()
  {
    ctx.send_invoke_success(id, args[0]);
  }

  void error::derefer::deref()
  {
    json ex_data = {
      { "args", args }
    };
    ctx.send_invoke_error(id, "Illegal arguments", ex_data);
  }

  void print::derefer::deref()
  {
    ctx.send_out(id, args.dump() + "\n");
    ctx.send_invoke_success(id, {});
  }

  void print_err::derefer::deref()
  {
    ctx.send_err(id, args.dump() + "\n");
    ctx.send_invoke_success(id, {});
  }

  void return_nil::derefer::deref()
  {
    ctx.send_invoke_success(id, {});
  }
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
  ctx->add_ns(test_pod::build_ns());
  ctx->add_ns(test_pod::build_defer_ns());
  ctx->describe();

  pod::Pod<json> pod_{ *ctx };
  pod_.start();

  return 0;
}
