#ifndef POD_H_
#define POD_H_

#include "bencode.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bc = bencode;

// TODO
// 1. Metadata: https://github.com/babashka/pods?tab=readme-ov-file#metadata
//    - Dynamic metadata
//    - From pod client to pod
// 2. readers: https://github.com/babashka/pods?tab=readme-ov-file#readers

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
  class Namespace
  {
  public:
    bool _defer;
    std::function<void(Namespace<T> &)> _load_vars;
    std::string _name;
    std::vector<std::string> _var_names;
    std::map<std::string, std::unique_ptr<Var<T>>> _vars;
    void add_var(std::unique_ptr<Var<T>> var);

    bc::data describe();
    bc::data describe(bool force);

    Namespace(std::string const &name)
      : _name(name)
      , _defer{ false }
    {
    }

    Namespace(std::string const &name, bool defer, std::function<void(Namespace<T> &)> load_vars)
      : _name(name)
      , _defer{ defer }
      , _load_vars{ load_vars }
    {
      if(!defer)
      {
        _load_vars(*this);
      }
    }
  };

  template <typename T>
  class Context
  {
  public:
    std::string _pod_id;
    std::unique_ptr<Encoder<T>> _encoder;
    std::unique_ptr<Transport> _transport;
    std::function<void()> _cleanup;

    std::vector<std::string> _ns_names;
    std::map<std::string, std::unique_ptr<Namespace<T>>> _ns;

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
    void add_ns(std::unique_ptr<Namespace<T>> ns);

    /** find the `Var` by the qualified name. */
    Var<T> *find_var(std::string const &qualified_name);

    /** find namespace by its name. */
    Namespace<T> *find_ns(std::string const &name);

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

    bc::data describe()
    {
      auto v = bc::dict{
        { "name", name() }
      };
      if(!meta().empty())
      {
        v["meta"] = meta();
      }
      if(!code().empty())
      {
        v["code"] = code();
      };
      return v;
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
  inline void Namespace<T>::add_var(std::unique_ptr<Var<T>> var)
  {
    auto n = var->name();
    if(!_vars.contains(n))
    {
      _var_names.emplace_back(n);
    }
    _vars[n] = std::move(var);
  }

  template <typename T>
  inline bc::data Namespace<T>::describe()
  {
    return describe(false);
  }

  template <typename T>
  inline bc::data Namespace<T>::describe(bool force)
  {
    bc::dict v = bc::dict{
      { "name", _name }
    };
    if(_defer && !force)
    {
      v["defer"] = "true";
    }
    else
    {
      if(_load_vars)
      {
        _load_vars(*this);
      }
      bc::list vars;
      for(auto &n : _var_names)
      {
        std::unique_ptr<Var<T>> &var = _vars[n];
        vars.emplace_back(var->describe());
      }
      v["vars"] = vars;
    }
    return v;
  }

  template <typename T>
  inline void Context<T>::add_ns(std::unique_ptr<Namespace<T>> ns)
  {
    auto n = ns->_name;
    if(!_ns.contains(n))
    {
      _ns_names.emplace_back(n);
    }
    _ns[n] = std::move(ns);
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
  inline Namespace<T> *Context<T>::find_ns(std::string const &name)
  {
    if(!_ns.contains(name))
    {
      throw std::runtime_error{ "namespace not found: " + name };
    }
    return _ns[name].get();
  }

  template <typename T>
  inline Var<T> *Context<T>::find_var(std::string const &qualified_name)
  {
    auto pos = qualified_name.find('/');
    if(pos == std::string::npos)
    {
      throw std::runtime_error{ "invalid qualified name: " + qualified_name };
    }
    auto _ns_name = qualified_name.substr(0, pos);
    if(!_ns.contains(_ns_name))
    {
      throw std::runtime_error{ "namespace not found: " + qualified_name };
    }
    std::unique_ptr<Namespace<T>> &ns = _ns[_ns_name];

    auto _var_name = qualified_name.substr(pos + 1);
    if(!ns->_vars.contains(_var_name))
    {
      throw std::runtime_error{ "namespace var not found: " + qualified_name };
    }
    return ns->_vars[_var_name].get();
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

    bc::list namespaces;
    {
      if(!_pod_id.empty())
      {
        if(_ns.contains(_pod_id))
        {
          std::unique_ptr<Namespace<T>> &ns = _ns[_pod_id];
          namespaces.emplace_back(ns->describe());
        }
        else
        {
          namespaces.emplace_back(bc::dict{
            { "name", _pod_id }
          });
        }
      }
      for(auto &n : _ns_names)
      {
        if(n == _pod_id)
        {
          continue;
        }
        std::unique_ptr<Namespace<T>> &ns = _ns[n];
        namespaces.emplace_back(ns->describe());
      }
    }

    return bc::dict{
      {     "format", _encoder->format() },
      {        "ops",                ops },
      { "namespaces",         namespaces }
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
      else if(op == "load-ns")
      {
        auto name = std::get<bc::string>(d["ns"]);
        auto ns = ctx.find_ns(name);
        auto r = ns->describe(true);
        auto id = d["id"];
        r["id"] = id;
        ctx._transport->write(r);
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
