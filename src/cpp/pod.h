#ifndef POD_H_
#define POD_H_

#include "bencode.hpp"

#include <memory>
#include <string>
#include <utility>

namespace bc = bencode;

// TODO
// 1. Metadata: https://github.com/babashka/pods?tab=readme-ov-file#metadata
//    - Dynamic metadata
//    - From pod client to pod
// 2. Deferred namespace loading: https://github.com/babashka/pods?tab=readme-ov-file#deferred-namespace-loading
// 3. readers: https://github.com/babashka/pods?tab=readme-ov-file#readers

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
                             bc::data const &ex_data)
    {
      write(bc::dict{
        {         "id",                          id },
        { "ex-message",                  ex_message },
        {    "ex-data",                     ex_data },
        {     "status", bc::list{ "done", "error" } }
      });
    }

    void send_invoke_callback(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",         id },
        {  "value",      value },
        { "status", bc::list{} }
      });
    }

    void send_invoke_success(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",                 id },
        {  "value",              value },
        { "status", bc::list{ "done" } }
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

    /** https://github.com/babashka/pods?tab=readme-ov-file#describe
     *
     * If the pod supports `shutdown` op, we can customize the `cleanup`
     * function on `shutdown`.
     */
    //
    void cleanup() const;

    /** https://github.com/babashka/pods?tab=readme-ov-file#describe
     *
     * vars have two parts, `ns`, `name`, it can be invoked via qualified name
     * `<ns>/<name>`.
     */
    void add_var(std::unique_ptr<Var<T>> var);

    /** find the `Var` by the qualified name. */
    Var<T> *find_var(std::string const &qualified_name);

    /** https://github.com/babashka/pods?tab=readme-ov-file#describe
     *
     * Returns the description info.
     *
     * Current implementation of pod uses the first namespace's name as the
     * loaded `pod-id`. It's not a documented behavior, but here we're utilizing
     * it for customizing `pod-id`.
     */
    bc::data describe();

    /** https://github.com/babashka/pods?tab=readme-ov-file#out-and-err
     *
     * Sending message to stderr.
     */
    void send_err(std::string const &id, std::string const &msg) const
    {
      _transport->send_err(id, msg);
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#out-and-err
     *
     * Sending message to stdout.
     */
    void send_out(std::string const &id, std::string const &msg) const
    {
      _transport->send_out(id, msg);
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#error-handling
     *
     * Sending invoke failure response.
     */
    void send_invoke_failure(std::string const &id,
                             std::string const &ex_message,
                             T const &ex_data) const
    {
      _transport->send_invoke_failure(id, ex_message, _encoder->encode(ex_data));
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#invoke
     *
     * Sending invoke success response.
     */
    void send_invoke_success(std::string const &id, T const &value) const
    {
      _transport->send_invoke_success(id, _encoder->encode(value));
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#invoke
     *
     * Sending invoke callbacks. The callback response is a success response
     * empty `status` set.
     */
    void send_invoke_callback(std::string const &id, T const &value) const
    {
      _transport->send_invoke_callback(id, _encoder->encode(value));
    }
  };

  /** A simple pod implementation. */
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
  class VarInvoker
  {
  public:
    Context<T> const &ctx;
    std::string id;

    VarInvoker(Context<T> const &ctx, std::string const &id)
      : ctx(ctx)
      , id(id)
    {
    }
  };

  template <typename T>
  class Var
  {
  public:
    virtual ~Var() = default;

    virtual std::string ns() = 0;

    virtual std::string name() = 0;

    /** https://github.com/babashka/pods?tab=readme-ov-file#client-side-code
     *
     * Arbitrary code which will be executed under namespace `ns`.
     */
    virtual std::string code()
    {
      return "";
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#metadata
     *
     * Fixed Metadata on vars
     */
    virtual std::string meta()
    {
      return "";
    }

    class derefer
    {
    public:
      Context<T> &ctx;
      std::string id;
      T args;

      derefer(Context<T> &ctx, std::string const &id, T const &args)
        : ctx(ctx)
        , id(id)
        , args(args)
      {
      }

      virtual ~derefer() = default;

      virtual void deref() = 0;
    };

    virtual std::unique_ptr<derefer>
    make_derefer(Context<T> &ctx, std::string const &id, T const &args) = 0;
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
        auto v_ = bc::dict{
          { "name", v.second->name() },
          { "meta", v.second->meta() }
        };
        if(!v.second->code().empty())
        {
          v_["code"] = v.second->code();
        };
        ns_vars[v.second->ns()].emplace_back(v_);
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
      auto op = std::get<bc::string>(d["op"]);
      if(op == "describe")
      {
        auto v = ctx.describe();
        ctx._transport->write(v);
      }
      else if(op == "invoke")
      {
        auto id = std::get<bc::string>(d["id"]);
        auto qn = std::get<bc::string>(d["var"]);
        try
        {
          auto _args = std::get<bc::string>(d["args"]);
          if(Var<T> *var = ctx.find_var(qn); var)
          {
            auto args = ctx._encoder->decode(_args);
            auto derefer = var->make_derefer(ctx, id, args);
            derefer->deref();
          }
          else
          {
            ctx.send_err(id, "var not found");
          }
        }
        catch(std::exception const &e)
        {
          ctx._transport->send_invoke_failure(id, e.what(), bc::dict{});
        }
        catch(...)
        {
          ctx._transport->send_invoke_failure(id, "unkown exception", bc::dict{});
          throw;
        }
      }
      else if(op == "shutdown")
      {
        ctx.cleanup();
        break;
      }
      else
      {
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
