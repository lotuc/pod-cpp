#ifndef POD_H_
#define POD_H_

#include "bencode.hpp"

#include <memory>
#include <string>
#include <utility>

namespace bc = bencode;

namespace lotuc::pod
{
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

  template <typename T>
  class Encoder
  {
  public:
    virtual ~Encoder() = default;
    virtual std::string format() = 0;
    virtual std::string encode(T const &d) = 0;
    virtual std::string encode(std::vector<std::string> const &statuses) = 0;
    virtual T decode(std::string const &s) = 0;
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

  template <typename T>
  class Var;

  template <typename T>
  class Context
  {
  public:
    std::string _pod_id;
    std::unique_ptr<Encoder<T>> _encoder;
    std::unique_ptr<Transport> _transport;
    std::function<void()> _cleanup;
    std::map<std::string, std::unique_ptr<Var<T>>> _vars;

    Context(std::unique_ptr<Encoder<T>> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> cleanup)
      : _pod_id{}
      , _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _cleanup{ std::move(cleanup) }
    {
    }

    Context(std::string &pod_id,
            std::unique_ptr<Encoder<T>> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> cleanup)
      : _pod_id{ pod_id }
      , _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _cleanup{ std::move(cleanup) }
    {
    }

    void cleanup() const;
    void add_var(std::unique_ptr<Var<T>> var);
    Var<T> *find_var(std::string const &qualified_name);
    bc::data describe();

    void send_err(std::string const &id, std::string const &msg) const
    {
      _transport->send_err(id, msg);
    }

    void send_out(std::string const &id, std::string const &msg) const
    {
      _transport->send_out(id, msg);
    }

    void send_invoke_failure(std::string const &id,
                             std::string const &ex_message,
                             T const &ex_data,
                             std::vector<std::string> const &status) const
    {
      _transport->send_invoke_failure(id,
                                      ex_message,
                                      _encoder->encode(ex_data),
                                      _encoder->encode(status));
    }

    void send_invoke_callback(std::string const &id, T const &value) const
    {
      _transport->send_invoke_callback(id, _encoder->encode(value));
    }

    void send_invoke_success(std::string const &id, T const &value) const
    {
      _transport->send_invoke_success(id, _encoder->encode(value));
    }
  };

  template <typename T>
  class Pod
  {
  public:
    Context<T> &ctx;

    Pod(Context<T> &ctx)
      : ctx{ ctx }
    {
    }

    void start();
  };

  template <typename T>
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

    virtual void invoke(Context<T> const &ctx, std::string const &id, T const &args) = 0;
  };

  template <typename T>
  inline void Context<T>::add_var(std::unique_ptr<Var<T>> var)
  {
    _vars[var->ns() + "/" + var->name()] = std::move(var);
  }

  template <typename T>
  inline void Context<T>::cleanup() const
  {
    if(_cleanup)
    {
      _cleanup();
    }
  }

  template <typename T>
  inline Var<T> *Context<T>::find_var(std::string const &qualified_name)
  {
    auto v = _vars.find(qualified_name);
    if(v != _vars.end())
    {
      return v->second.get();
    }
    else
    {
      return nullptr;
    }
  }

  template <typename T>
  inline bc::data Context<T>::describe()
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

  template <typename T>
  inline void Pod<T>::start()
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

        if(Var<T> *var = ctx.find_var(qn); var)
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
