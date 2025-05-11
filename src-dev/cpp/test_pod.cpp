#include "pod_helper.h"

#include <memory>

define_pod_var_deref(json, pod_test_pod_add_sync, "pod.test-pod", "add-sync", "");

void pod_test_pod_add_sync::derefer::deref()
{
  int r{};
  for(auto const &a : args)
  {
    r += a.get<int>();
  }
  ctx.send_invoke_success(id, r);
}

// https://github.com/babashka/pods/blob/master/test-pod/pod/test_pod.clj
define_pod_var_deref1(json, test_pod, echo, "");
define_pod_var_deref1(json, test_pod, error, "");
define_pod_var_deref1(json, test_pod, print, "");
define_pod_var_deref1(json, test_pod, print_err, "");
define_pod_var_deref1(json, test_pod, return_nil, "");
define_pod_var_code1(json, test_pod, do_twice, "", "(defmacro do-twice [x] `(do ~x ~x))");
define_pod_var_code1(json, test_pod, fn_call, "", "(defn fn-call [f x] (f x))");

void test_pod_echo::derefer::deref()
{
  ctx.send_invoke_success(id, args[0]);
}

void test_pod_error::derefer::deref()
{
  json ex_data = {
    { "args", args }
  };
  ctx.send_invoke_failure(id, "Illegal arguments", ex_data);
}

void test_pod_print::derefer::deref()
{
  ctx.send_out(id, args.dump() + "\n");
  ctx.send_invoke_success(id, {});
}

void test_pod_print_err::derefer::deref()
{
  ctx.send_err(id, args.dump() + "\n");
  ctx.send_invoke_success(id, {});
}

void test_pod_return_nil::derefer::deref()
{
  ctx.send_invoke_success(id, {});
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
  ctx->add_var(std::make_unique<test_pod_error>());
  ctx->add_var(std::make_unique<test_pod_echo>());
  ctx->add_var(std::make_unique<test_pod_error>());
  ctx->add_var(std::make_unique<test_pod_print>());
  ctx->add_var(std::make_unique<test_pod_print_err>());
  ctx->add_var(std::make_unique<test_pod_return_nil>());
  ctx->add_var(std::make_unique<test_pod_do_twice>());
  ctx->add_var(std::make_unique<test_pod_fn_call>());

  pod::Pod<json> pod_{ *ctx };
  pod_.start();

  return 0;
}
