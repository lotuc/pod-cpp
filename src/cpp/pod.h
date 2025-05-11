#ifndef POD_H_
#define POD_H_

#include <nlohmann/json.hpp>
#include "bencode.hpp"

#include <memory>
#include <string>
#include <utility>

using json = nlohmann::json;
namespace bc = bencode;

namespace lotuc::pod
{
  class Encoder
  {
  public:
    std::string format()
    {
      return "json";
    }

    std::string encode(json const &d)
    {
      return d.dump();
    }

    std::string encode(std::vector<std::string> const &statuses)
    {
      return json(statuses).dump();
    }

    json decode(std::string const &s)
    {
      return json::parse(s);
    }
  };

  class Transport
  {
  public:
    virtual ~Transport() = default;
    virtual bc::data read() = 0;
    virtual void write(bc::data const &d) = 0;

    void send_err(std::string const &id, std::string const &msg)
    {
      write(bc::dict{
        {  "id",  id },
        { "err", msg }
      });
    }

    void send_out(std::string const &id, std::string const &msg)
    {
      write(bc::dict{
        {  "id",  id },
        { "out", msg }
      });
    }

    void send_invoke_failure(std::string const &id,
                             std::string const &ex_message,
                             bc::data const &ex_data,
                             bc::list const &status)
    {
      write(bc::dict{
        {         "id",         id },
        { "ex-message", ex_message },
        {    "ex-data",    ex_data },
        {     "status",     status }
      });
    }

    void send_invoke_callback(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",         id },
        {  "value",      value },
        { "status", bc::list() }
      });
    }

    void send_invoke_success(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",                   id },
        {  "value",                value },
        { "status", bc::list({ "done" }) }
      });
    }
  };

  class Var;

  class Context
  {
  public:
    std::string _pod_id;
    std::unique_ptr<Encoder> _encoder;
    std::unique_ptr<Transport> _transport;
    std::function<void()> _cleanup;
    std::map<std::string, std::unique_ptr<Var>> _vars;

    Context(std::unique_ptr<Encoder> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> cleanup)
      : _pod_id{}
      , _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _cleanup{ std::move(cleanup) }
    {
    }

    Context(std::string &pod_id,
            std::unique_ptr<Encoder> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> cleanup)
      : _pod_id{ pod_id }
      , _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _cleanup{ std::move(cleanup) }
    {
    }

    void cleanup() const;
    void add_var(std::unique_ptr<Var> var);
    Var *find_var(std::string const &qualified_name);
    bc::data describe();
  };

  class Pod
  {
  public:
    static std::string getenv(std::string const &n)
    {
      char const *s = ::getenv(n.c_str());
      return s ? s : "";
    }

    static bool is_babashka_pod_env()
    {
      return getenv("BABASHKA_POD") == "true";
    }

    static bool is_babashka_transport_socket()
    {
      return getenv("BABASHKA_POD_TRANSPORT") == "socket";
    }

    Context &ctx;

    Pod(Context &ctx)
      : ctx{ ctx }
    {
    }

    void start();
  };

  class Var
  {
  public:
    virtual ~Var() = default;

    virtual std::string ns() = 0;

    virtual std::string name() = 0;

    // edn string
    virtual std::string meta()
    {
      return "";
    }

    virtual void invoke(Context const &ctx, std::string const &id, json const &args) = 0;
  };

  inline void Context::add_var(std::unique_ptr<Var> var)
  {
    _vars[var->ns() + "/" + var->name()] = std::move(var);
  }

  inline void Context::cleanup() const
  {
    if(_cleanup)
    {
      _cleanup();
    }
  }

  inline Var *Context::find_var(std::string const &qualified_name)
  {
    if(_vars.contains(qualified_name))
    {
      return _vars[qualified_name].get();
    }
    return nullptr;
  }

  inline bc::data Context::describe()
  {
    bc::dict ops;
    {
      if(_cleanup)
      {
        ops = bc::dict{
          { "shutdown", bc::dict{} }
        };
      }
    }


    bc::list ns;
    {
      std::map<std::string, bc::list> ns_vars;
      if(!_pod_id.empty())
      {
        ns_vars[_pod_id] = bc::list{};
      }
      for(auto const &v : _vars)
      {
        ns_vars[v.second->ns()].emplace_back(bc::dict{
          { "name", v.second->name() },
          { "meta", v.second->meta() }
        });
      }
      // the first namespace returned is pod-id
      if(!_pod_id.empty())
      {
        ns.emplace_back(bc::dict{
          { "name",          _pod_id },
          { "vars", ns_vars[_pod_id] }
        });
        ns_vars.erase(_pod_id);
      }
      for(auto v : ns_vars)
      {
        ns.emplace_back(bc::dict{
          { "name",  v.first },
          { "vars", v.second }
        });
      }
    }

    return bc::dict{
      {     "format", _encoder->format() },
      {        "ops",                ops },
      { "namespaces",                 ns }
    };
  }

  inline void Pod::start()
  {
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
      else if(op == "shutdown")
      {
        ctx.cleanup();
        break;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  class StdInOutTransport : public Transport
  {
    bc::data read() override
    {
      return bc::decode_some(std::cin, bc::no_check_eof);
    }

    void write(bc::data const &data) override
    {
      bc::encode_to(std::cout, data);
      std::cout << std::flush;
    }
  };
}

#endif // POD_H_
