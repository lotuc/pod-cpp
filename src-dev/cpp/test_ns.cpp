#include "test_ns.h"

namespace test_pod
{
  void add_sync::derefer::deref()
  {
    int r{};
    for(auto const &a : args)
    {
      r += a.get<int>();
    }
    success(r);
  }

  void add_async::derefer::deref()
  {
    int r{};
    for(auto const &a : args)
    {
      r += a.get<int>();
    }
    success(r);
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
      callback(i);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    success();
  }

  void echo::derefer::deref()
  {
    success(args);
  }

  void error::derefer::deref()
  {
    error("Illegal arguments",
          {
            { "args", args }
    });
  }

  void print::derefer::deref()
  {
    send_stdout(args.dump() + "\n");
    success({});
  }

  void print_err::derefer::deref()
  {
    send_stderr(args.dump() + "\n");
    success({});
  }

  void return_nil::derefer::deref()
  {
    success({});
  }

  static void
  threaded_task(lotuc::pod::Var<json, C>::derefer *d, std::string const &id, std::string const &msg)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    d->callback(msg);
  }

  void multi_threaded_test::derefer::deref()
  {
    std::vector<std::thread> threads;
    threads.reserve(100);
    for(int i = 0; i < 100; i++)
    {
      threads.emplace_back(threaded_task, this, id, "hello from " + std::to_string(i));
    }
    for(auto &t : threads)
    {
      t.join();
    }
    success();
  }

  void mis_implementation::derefer::deref()
  {
    std::string typ = "no-finish-message-sent";
    if(args.size() == 1)
    {
      typ = args[0].get<std::string>();
    }
    if(typ == "no-finish-message-sent")
    {
      return;
    }
    else
    {
      throw std::runtime_error{ "unkown mis-implementation-type: " + typ };
    }
  }

  void sleep::derefer::deref()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(args[0].get<int>()));
    success();
  }

  void async_sleep::derefer::deref()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(args[0].get<int>()));
    success();
  }

  void counter_set::derefer::deref()
  {
    ctx.components.counter = args[0].get<int>();
    success();
  }

  void counter_get_inc::derefer::deref()
  {
    success(ctx.components.counter++);
  }
}
