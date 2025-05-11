#include "pod.h"
#include <memory>

namespace lotuc::pod
{

  class EchoVar : public Var
  {
    std::string ns() override
    {
      return "echo";
    }

    std::string name() override
    {
      return "echo";
    }

    std::string meta() override
    {
      return "{:doc \"(echo/echo <val>)\"}";
    }

    void invoke(Context const &ctx, std::string const &id, json const &args) override
    {
      auto res = ctx._encoder->encode(args[0]);
      ctx._transport->send_invoke_success(id, res);
    }
  };
}

int main()
{
  using namespace lotuc::pod;
  Context ctx(std::make_unique<Encoder>(), std::make_unique<Transport>(), nullptr);
  ctx.add_var(std::make_unique<EchoVar>());
  while(true)
  {
    auto d = ctx._transport->read();
    auto op = std::get<std::string>(d["op"]);
    if(op == "describe")
    {
      auto v = ctx.describe();
      ctx._transport->write(v);
    }
    else if(op == "invoke")
    {
      auto id = std::get<std::string>(d["id"]);
      auto qn = std::get<std::string>(d["var"]);
      auto _args = std::get<std::string>(d["args"]);

      if(Var *var = ctx.find_var(qn); var)
      {
        auto args = ctx._encoder->decode(_args);
        var->invoke(ctx, id, args);
      }
      else
      {
        ctx._transport->send_err(id, "var not found");
      }
    }
  }

  return 0;
}
