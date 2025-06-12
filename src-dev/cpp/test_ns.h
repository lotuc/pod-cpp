#ifndef TEST_POD_H_
#define TEST_POD_H_

#include "pod.h"
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

using json = nlohmann::json;

// https://github.com/babashka/pods/blob/master/test-pod/pod/test_pod.clj
namespace test_pod
{

  struct C
  {
    int counter;
  };

  // invoking sync vars will block the Pod's read_eval_loop, while the async ones will not.

  // customize the var's name (notice the kebab case)
  define_pod_var(json, C, add_sync, "add-sync", "{:doc \"add the arguments\"}", false);
  define_pod_var(json, C, add_async, "add-async", "{:doc \"add the arguments\"}", true);

  // use the var class's name as the var's name
  define_pod_var_async(json, C, range_stream, "");
  define_pod_var_sync(json, C, echo, "");
  define_pod_var_sync(json, C, error, "");
  define_pod_var_sync(json, C, print, "");
  define_pod_var_sync(json, C, print_err, "");
  define_pod_var_sync(json, C, return_nil, "");
  define_pod_var_code(json, C, do_twice, "", "(defmacro do-twice [x] `(do ~x ~x))");
  define_pod_var_code(json, C, fn_call, "", "(defn fn-call [f x] (f x))");
  define_pod_var_sync(json, C, multi_threaded_test, "");
  define_pod_var_sync(json, C, mis_implementation, "");
  define_pod_var_sync(json, C, sleep, "{:doc \"(sleep ms)\"}");
  define_pod_var_async(json, C, async_sleep, "{:doc \"(sleep ms)\"}");
  define_pod_var_async(json, C, counter_set, "");
  define_pod_var_async(json, C, counter_get_inc, "");

  static void load_vars(lotuc::pod::Namespace<json, C> &ns)
  {
    ns.add_var(std::make_unique<add_sync>());
    ns.add_var(std::make_unique<add_async>());
    ns.add_var(std::make_unique<range_stream>());
    ns.add_var(std::make_unique<error>());
    ns.add_var(std::make_unique<echo>());
    ns.add_var(std::make_unique<error>());
    ns.add_var(std::make_unique<print>());
    ns.add_var(std::make_unique<print_err>());
    ns.add_var(std::make_unique<return_nil>());
    ns.add_var(std::make_unique<do_twice>());
    ns.add_var(std::make_unique<fn_call>());
    ns.add_var(std::make_unique<multi_threaded_test>());
    ns.add_var(std::make_unique<mis_implementation>());
    ns.add_var(std::make_unique<sleep>());
    ns.add_var(std::make_unique<async_sleep>());
    ns.add_var(std::make_unique<counter_set>());
    ns.add_var(std::make_unique<counter_get_inc>());
  }

  static std::unique_ptr<lotuc::pod::Namespace<json, C>> build_ns()
  {
    auto ns = std::make_unique<lotuc::pod::Namespace<json, C>>("test-pod");
    load_vars(*ns);
    return ns;
  }

  static std::unique_ptr<lotuc::pod::Namespace<json, C>> build_defer_ns()
  {
    return std::make_unique<lotuc::pod::Namespace<json, C>>("test-pod-defer", true, load_vars);
  }
}

#endif // TEST_POD_H_
