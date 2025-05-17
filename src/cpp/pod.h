#ifndef POD_H_
#define POD_H_

#include "bencode.hpp"

#include <cstddef>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <mutex>

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

  /** babashka starts the pod with this environment set. */
  static bool is_babashka_pod_env()
  {
    return getenv("BABASHKA_POD") == "true";
  }

  /** babashka starts the pod & expects socket transport with this environment. */
  static bool is_babashka_transport_socket()
  {
    return getenv("BABASHKA_POD_TRANSPORT") == "socket";
  }

  template <typename T>
  class Encoder
  {
  public:
    std::string format;

    Encoder(std::string const &format)
      : format{ format }
    {
    }

    virtual ~Encoder() = default;
    virtual std::string encode(T const &d) = 0;
    virtual std::string encode(std::vector<std::string> const &status) = 0;
    // pretty dirty
    virtual std::string
    encode_pendings(std::map<std::string, std::pair<T, long long>> const &pending_args_start_ts)
      = 0;
    virtual bool is_dict(T const &v) = 0;
    virtual T make_dict(std::string const &k, T const &v) = 0;
    virtual T empty_dict() = 0;
    virtual T empty_list() = 0;
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

    void
    send_invoke_error(std::string const &id, std::string const &ex_message, bc::data const &ex_data)
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

    void send_invoke_success(std::string const &id)
    {
      write(bc::dict{
        {     "id",                 id },
        { "status", bc::list{ "done" } }
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
    std::string const name;

    /** https://github.com/babashka/pods?tab=readme-ov-file#deferred-namespace-loading
     *
     * Deferred namespace loading.
     */
    bool const defer;
    std::function<void(Namespace<T> &)> const load_vars;

    std::map<std::string, std::unique_ptr<Var<T>>> _vars;
    void add_var(std::unique_ptr<Var<T>> var);

    bc::data describe();
    bc::data describe(bool force);

    Namespace(std::string const &name)
      : name(name)
      , defer{ false }
      , load_vars{}
    {
    }

    Namespace(std::string const &name, bool defer, std::function<void(Namespace<T> &)> load_vars)
      : name(name)
      , defer{ defer }
      , load_vars{ load_vars }
    {
      if(!defer)
      {
        load_vars(*this);
      }
    }
  };

  template <typename T>
  class Context
  {
  public:
    std::string const _pod_id;
    std::unique_ptr<Encoder<T>> const _encoder;
    std::unique_ptr<Transport> const _transport;
    std::function<void()> const _cleanup;

    std::vector<std::string> _ns_names;
    std::map<std::string, std::unique_ptr<Namespace<T>>> _ns;

    Context(std::unique_ptr<Encoder<T>> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> cleanup)
      : _pod_id{}
      , _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _cleanup{ std::move(cleanup) }
    {
    }

    Context(std::string const &pod_id,
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
    Var<T> const *find_var(std::string const &qualified_name);

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
    bc::data describe(std::vector<std::unique_ptr<Namespace<T>>> &builtins);

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
     * Sending invoke error response.
     */
    void
    send_invoke_error(std::string const &id, std::string const &ex_message, T const &ex_data) const
    {
      auto valid = _encoder->is_dict(ex_data);
      if(!valid)
      {
        send_err(id, "automatically wrapping non-dict error data, try fix the implementation");
      }
      auto d = _encoder->encode(valid ? ex_data
                                      : _encoder->make_dict("ex-data", _encoder->encode(ex_data)));
      _transport->send_invoke_error(id, ex_message, d);
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#invoke
     *
     * Sending invoke success response.
     */
    void send_invoke_success(std::string const &id, T const &value) const
    {
      _transport->send_invoke_success(id, _encoder->encode(value));
    }

    /** https://github.com/babashka/pods/blob/47e55fe5e728578ff4dbf7d2a2caf00efea87b1e/test-pod/pod/test_pod.clj#L205
     *
     * Can success a call without a value
     */
    void send_invoke_success(std::string const &id) const
    {
      _transport->send_invoke_success(id);
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

    virtual std::vector<std::unique_ptr<Namespace<T>>> builtins()
    {
      return {};
    }

    virtual void invoke(Var<T> const &var, std::unique_ptr<typename Var<T>::derefer> derefer) = 0;

    void read_eval_loop()
    {
      auto &ctx = this->ctx;
      while(true)
      {
        auto d = std::get<bc::dict>(ctx._transport->read());
        auto op = std::get<bc::string>(d["op"]);

        if(op == "invoke")
        {
          auto id = std::get<bc::string>(d["id"]);
          auto qn = std::get<bc::string>(d["var"]);
          std::optional<std::string> args = d.find("args") == d.cend()
            ? std::nullopt
            : std::make_optional(std::get<bc::string>(d["args"]));
          if(auto var = ctx.find_var(qn); var)
          {
            auto args_v
              = args.has_value() ? ctx._encoder->decode(args.value()) : ctx._encoder->empty_list();
            invoke(*var, std::move(var->make_derefer(ctx, id, args_v)));
          }
          else
          {
            ctx._transport->send_invoke_error(id, "var not found", bc::dict{});
          }
        }
        else if(op == "describe")
        {
          auto n = builtins();
          auto v = ctx.describe(n);
          ctx._transport->write(v);
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
  };

  template <typename T>
  class Var
  {
  public:
    std::string const name;

    /** https://github.com/babashka/pods?tab=readme-ov-file#metadata
     *
     * Fixed Metadata on vars
     */
    std::string const meta;

    /** https://github.com/babashka/pods?tab=readme-ov-file#client-side-code
     *
     * Arbitrary code which will be executed under namespace `ns`.
     */
    std::string const code;

    /** https://github.com/babashka/pods?tab=readme-ov-file#async
     *
     * reference implementation:
     * https://github.com/babashka/pods/blob/47e55fe5e728578ff4dbf7d2a2caf00efea87b1e/test-pod/pod/test_pod.clj#L123
     */
    bool const async;

    Var(std::string const &name, std::string const &meta, std::string const &code, bool async)
      : name{ name }
      , meta{ meta }
      , code{ code }
      , async{ async }
    {
    }

    virtual ~Var() = default;

    bc::data describe()
    {
      auto v = bc::dict{
        { "name", name }
      };
      if(!meta.empty())
      {
        v["meta"] = meta;
      }
      if(!code.empty())
      {
        v["code"] = code;
      };
      if(async)
      {
        v["async"] = "true";
      }
      return v;
    }

    class derefer
    {
    public:
      Context<T> &_ctx;
      std::string id;
      T args;
      volatile bool done{ false };

      derefer(Context<T> &ctx, std::string const &id, T const &args)
        : _ctx(ctx)
        , id{ id }
        , args{ args }
      {
      }

      void out(std::string const &msg)
      {
        _ctx.send_out(id, msg);
      }

      void err(std::string const &msg)
      {
        _ctx.send_err(id, msg);
      }

      void callback(T const &v)
      {
        _ctx.send_invoke_callback(id, v);
      }

      void success()
      {
        _ctx.send_invoke_success(id);
        done = true;
      }

      void success(T const &v)
      {
        _ctx.send_invoke_success(id, v);
        done = true;
      }

      void error(std::string const &ex_message, T const &ex_data)
      {
        _ctx.send_invoke_error(id, ex_message, ex_data);
        done = true;
      }

      void error(std::string const &ex_message)
      {
        _ctx.send_invoke_error(id, ex_message, _ctx._encoder->empty_dict());
        done = true;
      }

      virtual ~derefer() = default;

      /** Triggers evaluation of the var. When returned, we expect `done` turn true. */
      virtual void deref() = 0;
    };

    virtual std::unique_ptr<derefer>
    make_derefer(Context<T> &ctx, std::string const &id, T const &args) const = 0;
  };

  template <typename T>
  inline void Namespace<T>::add_var(std::unique_ptr<Var<T>> var)
  {
    auto n = var->name;
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
      { "name", name }
    };
    if(defer && !force)
    {
      v["defer"] = "true";
    }
    else
    {
      if(load_vars)
      {
        load_vars(*this);
      }
      bc::list vars;
      for(auto &p : _vars)
      {
        vars.emplace_back(p.second->describe());
      }
      v["vars"] = vars;
    }
    return v;
  }

  template <typename T>
  inline void Context<T>::add_ns(std::unique_ptr<Namespace<T>> ns)
  {
    auto n = ns->name;
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
  inline Var<T> const *Context<T>::find_var(std::string const &qualified_name)
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
  inline bc::data Context<T>::describe(std::vector<std::unique_ptr<Namespace<T>>> &builtins)
  {
    for(auto &ns : builtins)
    {
      add_ns(std::move(ns));
    }

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
      {     "format", _encoder->format },
      {        "ops",              ops },
      { "namespaces",       namespaces }
    };
  }

  //////////////////////////////////////////////////////////////////////////////

  class StdInOutTransport : public Transport
  {
    std::mutex write_lock;

    bc::data read() override
    {
      return bc::decode_some(std::cin, bc::no_check_eof);
    }

    void write(bc::data const &data) override
    {
      std::lock_guard<std::mutex> lock(write_lock);
      bc::encode_to(std::cout, data);
      std::cout << std::flush;
    }
  };

  template <typename T>
  class SyncPod : public Pod<T>
  {
  public:
    using Pod<T>::Pod;

    static void do_invoke(std::unique_ptr<typename Var<T>::derefer> derefer)
    {
      try
      {
        derefer->deref();
        if(!derefer->done)
        {
          derefer->error("illegal var implementation, deref returned without any notice");
        }
      }
      catch(std::exception const &e)
      {
        std::string ex_message = e.what();
        derefer->error(ex_message);
      }
      catch(...)
      {
        derefer->error("unkown exception");
      }
    }

    void invoke(Var<T> const &var, std::unique_ptr<typename Var<T>::derefer> derefer) override
    {
      do_invoke(std::move(derefer));
    }
  };

  template <typename T>
  class AsyncPod : public Pod<T>
  {
  public:
    std::map<std::string, std::pair<T, long long>> _pending_args_start_ts;
    std::map<std::string, std::future<void>> _pendings;

    class pendings_var : public Var<T>
    {
    public:
      AsyncPod<T> &pod;

      pendings_var(AsyncPod<T> &pod)
        : Var<T>("pendings", "{:doc \"pending invokes\"}", "", false)
        , pod{ pod }
      {
      }

      class derefer : public Var<T>::derefer
      {
      public:
        AsyncPod<T> &pod;

        derefer(Context<T> &ctx, std::string const &id, T const &args, AsyncPod<T> &pod)
          : Var<T>::derefer::derefer{ ctx, id, args }
          , pod{ pod }
        {
        }

        using lotuc::pod::Var<T>::derefer::derefer;

        void deref() override
        {
          std::map<std::string, std::pair<T, long long>> pendings{ pod._pending_args_start_ts };
          auto v = this->_ctx._encoder->encode_pendings(pendings);
          this->_ctx._transport->send_invoke_success(this->id, v);
          this->done = true;
        }
      };

      std::unique_ptr<typename Var<T>::derefer>
      make_derefer(Context<T> &ctx, std::string const &id, T const &args) const override
      {
        return std::make_unique<derefer>(ctx, id, args, this->pod);
      }
    };

    AsyncPod(Context<T> &ctx)
      : Pod<T>::Pod{ ctx }
    {
    }

    std::vector<std::unique_ptr<Namespace<T>>> builtins() override
    {
      std::vector<std::unique_ptr<Namespace<T>>> ret;
      auto ns = std::make_unique<Namespace<T>>("lotuc.babashka.pods");
      ns->add_var(std::make_unique<pendings_var>(*this));
      ret.push_back(std::move(ns));
      return ret;
    }

    static void do_invoke(AsyncPod<T> *pod, std::unique_ptr<typename Var<T>::derefer> derefer)
    {
      auto duration = std::chrono::system_clock::now().time_since_epoch();
      auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

      std::string id = derefer->id;
      T args = derefer->args;

      pod->_pending_args_start_ts[id] = { args, millis };
      auto t = std::async(SyncPod<T>::do_invoke, std::move(derefer));
      pod->_pendings[id] = std::move(t);
      pod->_pendings[id].get();
      pod->_pendings.erase(id);
      pod->_pending_args_start_ts.erase(id);
    }

    void invoke(Var<T> const &var, std::unique_ptr<typename Var<T>::derefer> derefer) override
    {
      if(var.async)
      {
        std::thread(AsyncPod<T>::do_invoke, this, std::move(derefer)).detach();
      }
      else
      {
        SyncPod<T>::do_invoke(std::move(derefer));
      }
    }
  };

}

#endif // POD_H_
