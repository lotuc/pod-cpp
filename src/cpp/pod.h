#ifndef POD_H_
#define POD_H_

#include "bencode.hpp"

#include <condition_variable>
#include <cstddef>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include <mutex>

namespace bc = bencode;

// TODO
// 1. Metadata: https://github.com/babashka/pods?tab=readme-ov-file#metadata
//    - Dynamic metadata
//    - From pod client to pod
// 2. readers: https://github.com/babashka/pods?tab=readme-ov-file#readers

namespace lotuc::pod
{
  inline std::string getenv(std::string const &n)
  {
    char const *s = ::getenv(n.c_str());
    return s ? s : "";
  }

  /** babashka starts the pod with this environment set. */
  inline bool is_babashka_pod_env()
  {
    return getenv("BABASHKA_POD") == "true";
  }

  /** babashka starts the pod & expects socket transport with this environment. */
  inline bool is_babashka_transport_socket()
  {
    return getenv("BABASHKA_POD_TRANSPORT") == "socket";
  }

  struct ScopeGuard
  {
    // clang-format off

    std::function<void()> cleanup;

    ScopeGuard(std::function<void()> cleanup) : cleanup{ std::move(cleanup) } { }
    ~ScopeGuard() { cleanup(); }

    // clang-format on
  };

  template <typename T>
  class ExInfo : public std::exception
  {
  public:
    // clang-format off

    ExInfo(std::string const &message, T const &data) : _message{ message } , _data{ data } {}
    std::string const &message() const { return _message; }
    T const &data() const { return _data; }

    // clang-format on

  private:
    std::string _message;
    T _data;
  };

  template <typename T>
  struct PendingInvoke
  {
    std::string ns_name;
    std::string var_name;
    std::string id;
    T args;
    long long start_ts;
    std::future<void> fut;

    PendingInvoke<T>(std::string const &ns_name,
                     std::string const &var_name,
                     std::string const &id,
                     T const &args,
                     long long start_ts,
                     std::future<void> fut)
      : ns_name{ ns_name }
      , var_name{ var_name }
      , id{ id }
      , start_ts{ start_ts }
      , args{ args }
      , fut{ std::move(fut) }
    {
    }
  };

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
    virtual T decode(std::string const &s) = 0;

    virtual T empty_dict() = 0;
    virtual T empty_list() = 0;

    virtual bool is_dict(T const &v) = 0;
    virtual T make_dict(std::string const &k, T const &v) = 0;
    virtual std::string encode(std::vector<std::string> const &status) = 0;
    virtual std::string encode(std::vector<PendingInvoke<T> *> const &pendings) = 0;
  };

  class BencodeTransport
  {
  public:
    virtual ~BencodeTransport() = default;
    virtual bc::data read() = 0;
    virtual void write(bc::data const &d) = 0;
  };

  template <typename T>
  class PodTransport
  {
  public:
    std::unique_ptr<BencodeTransport> _transport;
    std::unique_ptr<Encoder<T>> _encoder;

    PodTransport<T>(std::unique_ptr<BencodeTransport> transport,
                    std::unique_ptr<Encoder<T>> encoder)
      : _transport{ std::move(transport) }
      , _encoder{ std::move(encoder) }
    {
    }

    std::string format()
    {
      return _encoder->format;
    }

    bc::data read()
    {
      return _transport->read();
    }

    void write(bc::data const &d)
    {
      _transport->write(d);
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#out-and-err
     *
     * Sending message to stderr.
     */
    void send_stderr(std::string const &id, std::string const &msg) const
    {
      _transport->write(bc::dict{
        {  "id",  id },
        { "err", msg }
      });
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#out-and-err
     *
     * Sending message to stdout.
     */
    void send_stdout(std::string const &id, std::string const &msg) const
    {
      _transport->write(bc::dict{
        {  "id",  id },
        { "out", msg }
      });
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
        send_stderr(id, "automatically wrapping non-dict error data, try fix the implementation");
      }
      bc::data d = _encoder->encode(
        valid ? ex_data : _encoder->make_dict("ex-data", _encoder->encode(ex_data)));
      send_invoke_error_bc(id, ex_message, d);
    }

    void send_invoke_error_bc(std::string const &id,
                              std::string const &ex_message,
                              bc::data const &ex_data) const
    {
      _transport->write(bc::dict{
        {         "id",                          id },
        { "ex-message",                  ex_message },
        {    "ex-data",                     ex_data },
        {     "status", bc::list{ "done", "error" } }
      });
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#invoke
     *
     * Sending invoke success response.
     */
    void send_invoke_success(std::string const &id, T const &value) const
    {
      send_invoke_success_bc(id, _encoder->encode(value));
    }

    void send_invoke_success_bc(std::string const &id, bc::data const &value) const
    {
      _transport->write(bc::dict{
        {     "id",                 id },
        {  "value",              value },
        { "status", bc::list{ "done" } }
      });
    }

    /** https://github.com/babashka/pods/blob/47e55fe5e728578ff4dbf7d2a2caf00efea87b1e/test-pod/pod/test_pod.clj#L205
     *
     * Can success a call without a value
     */
    void send_invoke_success(std::string const &id) const
    {
      _transport->write(bc::dict{
        {     "id",                 id },
        { "status", bc::list{ "done" } }
      });
    }

    /** https://github.com/babashka/pods?tab=readme-ov-file#invoke
     *
     * Sending invoke callbacks. The callback response is a success response
     * empty `status` set.
     */
    void send_invoke_callback(std::string const &id, T const &value) const
    {
      send_invoke_callback_bc(id, _encoder->encode(value));
    }

    void send_invoke_callback_bc(std::string const &id, bc::data const &value) const
    {
      _transport->write(bc::dict{
        {     "id",         id },
        {  "value",      value },
        { "status", bc::list{} }
      });
    }
  };

  template <typename T, typename C>
  class Var;

  template <typename T, typename C>
  class Namespace
  {
  public:
    std::string const name;

    /** https://github.com/babashka/pods?tab=readme-ov-file#deferred-namespace-loading
     *
     * Deferred namespace loading.
     */
    bool const defer;
    std::function<void(Namespace<T, C> &)> const load_vars;

    std::map<std::string, std::unique_ptr<Var<T, C>>> _vars;
    void add_var(std::unique_ptr<Var<T, C>> var);

    bc::data describe();
    bc::data describe(bool force);

    Namespace(std::string const &name)
      : name(name)
      , defer{ false }
      , load_vars{}
    {
    }

    Namespace(std::string const &name, bool defer, std::function<void(Namespace<T, C> &)> load_vars)
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

  template <typename T, typename C>
  class Context : public PodTransport<T>
  {
  public:
    C &components;

    std::string const _pod_id;
    std::function<void()> const _cleanup;

    std::vector<std::string> _ns_names;
    std::map<std::string, std::unique_ptr<Namespace<T, C>>> _ns;

    Context(C &components,
            std::unique_ptr<Encoder<T>> encoder,
            std::unique_ptr<BencodeTransport> transport,
            std::function<void()> cleanup)
      : lotuc::pod::PodTransport<T>{ std::move(transport), std::move(encoder) }
      , _pod_id{}
      , components{ components }
      , _cleanup{ std::move(cleanup) }
    {
    }

    Context(std::string const &pod_id,
            C &components,
            std::unique_ptr<Encoder<T>> encoder,
            std::unique_ptr<BencodeTransport> transport,
            std::function<void()> cleanup)
      : lotuc::pod::PodTransport<T>{ std::move(transport), std::move(encoder) }
      , _pod_id{ pod_id }
      , components{ components }
      , _cleanup{ std::move(cleanup) }
    {
    }

    ~Context()
    {
      cleanup();
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
    void add_ns(std::unique_ptr<Namespace<T, C>> ns);

    /** find the `Var` by the qualified name. */
    std::pair<Namespace<T, C> const *, Var<T, C> const *>
    find_var(std::string const &qualified_name);

    /** find namespace by its name. */
    Namespace<T, C> *find_ns(std::string const &name);

    /** https://github.com/babashka/pods?tab=readme-ov-file#describe
     *
     * Returns the description info.
     *
     * Current implementation of pod uses the first namespace's name as the
     * loaded `pod-id`. It's not a documented behavior, but here we're utilizing
     * it for customizing `pod-id`.
     */
    bc::data describe(std::vector<std::unique_ptr<Namespace<T, C>>> &builtins);
  };

  /** A simple pod implementation. */
  template <typename T, typename C>
  class Pod
  {
  public:
    Context<T, C> &ctx;

    Pod(Context<T, C> &ctx)
      : ctx{ ctx }
    {
    }

    virtual std::vector<std::unique_ptr<Namespace<T, C>>> builtins()
    {
      return {};
    }

    virtual void invoke(Namespace<T, C> const &ns,
                        Var<T, C> const &var,
                        std::unique_ptr<typename Var<T, C>::derefer> derefer)
      = 0;

    void read_eval_loop()
    {
      auto &ctx = this->ctx;
      while(true)
      {
        auto d = std::get<bc::dict>(ctx.read());
        auto op = std::get<bc::string>(d["op"]);

        if(op == "invoke")
        {
          auto id = std::get<bc::string>(d["id"]);
          auto qn = std::get<bc::string>(d["var"]);
          std::optional<std::string> args = d.find("args") == d.cend()
            ? std::nullopt
            : std::make_optional(std::get<bc::string>(d["args"]));
          auto found = ctx.find_var(qn);
          auto ns = found.first;
          auto var = found.second;
          if(ns != nullptr && var != nullptr)
          {
            auto args_v
              = args.has_value() ? ctx._encoder->decode(args.value()) : ctx._encoder->empty_list();
            invoke(*ns, *var, std::move(var->make_derefer(ctx, id, args_v)));
          }
          else
          {
            ctx.send_invoke_error_bc(id, "var not found", bc::dict{});
          }
        }
        else if(op == "describe")
        {
          auto n = builtins();
          auto v = ctx.describe(n);
          ctx.write(v);
        }
        else if(op == "load-ns")
        {
          auto name = std::get<bc::string>(d["ns"]);
          auto ns = ctx.find_ns(name);
          auto r = ns->describe(true);
          auto id = d["id"];
          r["id"] = id;
          ctx.write(r);
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

  template <typename T, typename C>
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
      Context<T, C> &ctx;
      std::string id;
      T args;
      volatile bool done{ false };

      // clang-format off

      derefer(Context<T, C> &ctx, std::string const &id, T const &args)
        : ctx(ctx) , id{ id } , args{ args } { }

      void send_stdout(std::string const &msg) { ctx.send_stdout(id, msg); }
      void sendln_stdout(std::string const &msg) { ctx.send_stdout(id, msg + "\n"); }

      void send_stderr(std::string const &msg) { ctx.send_stderr(id, msg); }
      void sendln_stderr(std::string const &msg) { ctx.send_stderr(id, msg + "\n"); }

      void callback(T const &v) { ctx.send_invoke_callback(id, v); }

      void success() { ctx.send_invoke_success(id); done = true; }

      void success(T const &v) { ctx.send_invoke_success(id, v); done = true; }

      void error(std::string const &ex_message, T const &ex_data)
      {
        ctx.send_invoke_error(id, ex_message, ex_data);
        done = true;
      }

      void error(std::string const &ex_message)
      {
        ctx.send_invoke_error(id, ex_message, ctx._encoder->empty_dict());
        done = true;
      }

      // clang-format on

      virtual ~derefer() = default;

      /** Triggers evaluation of the var. When returned, we expect `done` turn true. */
      virtual void deref() = 0;
    };

    virtual std::unique_ptr<derefer>
    make_derefer(Context<T, C> &ctx, std::string const &id, T const &args) const = 0;
  };

  template <typename T, typename C>
  inline void Namespace<T, C>::add_var(std::unique_ptr<Var<T, C>> var)
  {
    auto n = var->name;
    _vars[n] = std::move(var);
  }

  template <typename T, typename C>
  inline bc::data Namespace<T, C>::describe()
  {
    return describe(false);
  }

  template <typename T, typename C>
  inline bc::data Namespace<T, C>::describe(bool force)
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

  template <typename T, typename C>
  inline void Context<T, C>::add_ns(std::unique_ptr<Namespace<T, C>> ns)
  {
    auto n = ns->name;
    if(!_ns.contains(n))
    {
      _ns_names.emplace_back(n);
    }
    _ns[n] = std::move(ns);
  }

  template <typename T, typename C>
  inline void Context<T, C>::cleanup() const
  {
    if(_cleanup)
    {
      _cleanup();
    }
  }

  template <typename T, typename C>
  inline Namespace<T, C> *Context<T, C>::find_ns(std::string const &name)
  {
    if(!_ns.contains(name))
    {
      throw std::runtime_error{ "namespace not found: " + name };
    }
    return _ns[name].get();
  }

  template <typename T, typename C>
  inline std::pair<Namespace<T, C> const *, Var<T, C> const *>
  Context<T, C>::find_var(std::string const &qualified_name)
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
    std::unique_ptr<Namespace<T, C>> &ns = _ns[_ns_name];

    auto _var_name = qualified_name.substr(pos + 1);
    if(!ns->_vars.contains(_var_name))
    {
      throw std::runtime_error{ "namespace var not found: " + qualified_name };
    }
    return std::make_pair(ns.get(), ns->_vars[_var_name].get());
  }

  template <typename T, typename C>
  inline bc::data Context<T, C>::describe(std::vector<std::unique_ptr<Namespace<T, C>>> &builtins)
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
          std::unique_ptr<Namespace<T, C>> &ns = _ns[_pod_id];
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
        std::unique_ptr<Namespace<T, C>> &ns = _ns[n];
        namespaces.emplace_back(ns->describe());
      }
    }

    return bc::dict{
      {     "format", this->format() },
      {        "ops",            ops },
      { "namespaces",     namespaces }
    };
  }

  //////////////////////////////////////////////////////////////////////////////

  class StdInOutTransport : public BencodeTransport
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

  class ConcurrencyLimiter
  {
  public:
    ConcurrencyLimiter(int max_concurrency)
      : _max_concurrency{ max_concurrency }
    {
    }

    void acquire()
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _condition.wait(lock, [this] { return _current_concurrency < _max_concurrency; });
      _current_concurrency++;
    }

    void release()
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _current_concurrency--;
      _condition.notify_one();
    }

  private:
    int _max_concurrency;
    int _current_concurrency{};
    std::mutex _mutex;
    std::condition_variable _condition;
  };

  // a default pod implementation.
  template <typename T, typename C>
  class PodImpl : public Pod<T, C>
  {
  public:
    ConcurrencyLimiter _concurrency_limiter;
    std::map<std::string, PendingInvoke<T>> _pendings;
    std::set<std::string> _builtin_ns_names{};

    class pendings_var : public Var<T, C>
    {
    public:
      PodImpl<T, C> &pod;

      pendings_var(PodImpl<T, C> &pod)
        : Var<T, C>("pendings", "{:doc \"pending invokes\"}", "", false)
        , pod{ pod }
      {
      }

      class derefer : public Var<T, C>::derefer
      {
      public:
        PodImpl<T, C> &pod;

        derefer(Context<T, C> &ctx, std::string const &id, T const &args, PodImpl<T, C> &pod)
          : Var<T, C>::derefer::derefer{ ctx, id, args }
          , pod{ pod }
        {
        }

        using lotuc::pod::Var<T, C>::derefer::derefer;

        void deref() override
        {
          std::vector<PendingInvoke<T> *> t;
          t.reserve(pod._pendings.size());
          for(auto &p : pod._pendings)
          {
            t.push_back(&p.second);
          }
          auto v = this->ctx._encoder->encode(t);
          this->ctx.send_invoke_success_bc(this->id, v);
          this->done = true;
        }
      };

      std::unique_ptr<typename Var<T, C>::derefer>
      make_derefer(Context<T, C> &ctx, std::string const &id, T const &args) const override
      {
        return std::make_unique<derefer>(ctx, id, args, this->pod);
      }
    };

    PodImpl(Context<T, C> &ctx)
      : PodImpl<T, C>::PodImpl{ ctx, 1024 }
    {
    }

    PodImpl(Context<T, C> &ctx, int max_concurrent)
      : Pod<T, C>::Pod{ ctx }
      , _concurrency_limiter{ max_concurrent }
    {
    }

    std::vector<std::unique_ptr<Namespace<T, C>>> builtins() override
    {
      std::vector<std::unique_ptr<Namespace<T, C>>> ret;
      auto ns = std::make_unique<Namespace<T, C>>("lotuc.babashka.pods");
      _builtin_ns_names.insert(ns->name);
      ns->add_var(std::make_unique<pendings_var>(*this));
      ret.push_back(std::move(ns));
      return ret;
    }

    static void
    do_invoke(Var<T, C> const *var, std::unique_ptr<typename Var<T, C>::derefer> derefer)
    {
      try
      {
        derefer->deref();
        if(!derefer->done)
        {
          derefer->error("illegal var implementation, deref returned without any notice");
        }
      }
      catch(ExInfo<T> const &e)
      {
        derefer->error(e.message(), e.data());
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

    static void watched_invoke(PodImpl<T, C> *pod,
                               Namespace<T, C> const *ns,
                               Var<T, C> const *var,
                               std::unique_ptr<typename Var<T, C>::derefer> derefer)
    {
      // Now we only got two logic "concurrency groups" here. The builtin one
      // and others. We only limit the concurrency runs for the non builtin
      // vars.

      // Notice for now, the backpressure is not put on the input stream. The
      // caller can only feel pressure when it waits the calling results.

      auto is_builtin = pod->_builtin_ns_names.contains(ns->name);
      if(!is_builtin)
      {
        pod->_concurrency_limiter.acquire();
      }
      ScopeGuard _release{ [pod, &is_builtin]() {
        if(!is_builtin)
        {
          pod->_concurrency_limiter.release();
        }
      } };

      auto duration = std::chrono::system_clock::now().time_since_epoch();
      auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
      std::string id = derefer->id;
      T args = derefer->args;

      auto fut = std::async(PodImpl<T, C>::do_invoke, var, std::move(derefer));

      // clang-format off
      ScopeGuard _cleanup{ [pod, &id]() { pod->_pendings.erase(id); } };
      pod->_pendings.insert({ id, PendingInvoke<T>{ ns->name, var->name, id, args, millis, std::move(fut) } });
      auto it = pod->_pendings.find(id);
      if(it != pod->_pendings.end()) { it->second.fut.get(); }
      // clang-format on
    }

    void invoke(Namespace<T, C> const &ns,
                Var<T, C> const &var,
                std::unique_ptr<typename Var<T, C>::derefer> derefer) override
    {
      std::thread(PodImpl<T, C>::watched_invoke, this, &ns, &var, std::move(derefer)).detach();
    }
  };
};

#define define_pod_var_code(T, C, _class_name, _meta, _code)                                     \
  class _class_name : public lotuc::pod::Var<T, C>                                               \
  {                                                                                              \
  public:                                                                                        \
    _class_name()                                                                                \
      : lotuc::pod::Var<T, C>(#_class_name, _meta, _code, false)                                 \
    {                                                                                            \
    }                                                                                            \
    std::unique_ptr<lotuc::pod::Var<T, C>::derefer> make_derefer(lotuc::pod::Context<T, C> &ctx, \
                                                                 std::string const &id,          \
                                                                 T const &args) const override   \
    {                                                                                            \
      throw std::runtime_error{ "code var" };                                                    \
    }                                                                                            \
  }

#define define_pod_var(T, C, _class_name, _name, _meta, _async)                                  \
  class _class_name : public lotuc::pod::Var<T, C>                                               \
  {                                                                                              \
  public:                                                                                        \
    _class_name()                                                                                \
      : lotuc::pod::Var<T, C>(_name, _meta, "", _async)                                          \
    {                                                                                            \
    }                                                                                            \
    class derefer : public lotuc::pod::Var<T, C>::derefer                                        \
    {                                                                                            \
      using lotuc::pod::Var<T, C>::derefer::derefer;                                             \
      void deref() override;                                                                     \
    };                                                                                           \
                                                                                                 \
    std::unique_ptr<lotuc::pod::Var<T, C>::derefer> make_derefer(lotuc::pod::Context<T, C> &ctx, \
                                                                 std::string const &id,          \
                                                                 T const &args) const override   \
    {                                                                                            \
      return std::make_unique<derefer>(ctx, id, args);                                           \
    }                                                                                            \
  }

#define define_pod_var_sync(T, C, _name, _meta) define_pod_var(T, C, _name, #_name, _meta, false)
#define define_pod_var_async(T, C, _name, _meta) define_pod_var(T, C, _name, #_name, _meta, true)

#endif // POD_H_
