#ifndef POD_HELPER_H_
#define POD_HELPER_H_

#include "pod.h"
#include "pod_asio.h"
#include "pod_json.h"

#define define_pod_var_code(T, _class_name, _meta, _code)                                          \
  class _class_name : public lotuc::pod::Var<T>                                                    \
  {                                                                                                \
  public:                                                                                          \
    _class_name()                                                                                  \
      : lotuc::pod::Var<T>(#_class_name, _meta, _code, false)                                      \
    {                                                                                              \
    }                                                                                              \
    std::unique_ptr<lotuc::pod::Var<T>::derefer>                                                   \
    make_derefer(lotuc::pod::Context<T> &ctx, std::string const &id, T const &args) const override \
    {                                                                                              \
      throw std::runtime_error{ "code var" };                                                      \
    }                                                                                              \
  }

#define define_pod_var(T, _class_name, _name, _meta, _async)                                       \
  class _class_name : public lotuc::pod::Var<T>                                                    \
  {                                                                                                \
  public:                                                                                          \
    _class_name()                                                                                  \
      : lotuc::pod::Var<T>(_name, _meta, "", _async)                                               \
    {                                                                                              \
    }                                                                                              \
    class derefer : public lotuc::pod::Var<T>::derefer                                             \
    {                                                                                              \
      using lotuc::pod::Var<T>::derefer::derefer;                                                  \
      void deref() override;                                                                       \
    };                                                                                             \
                                                                                                   \
    std::unique_ptr<lotuc::pod::Var<T>::derefer>                                                   \
    make_derefer(lotuc::pod::Context<T> &ctx, std::string const &id, T const &args) const override \
    {                                                                                              \
      return std::make_unique<derefer>(ctx, id, args);                                             \
    }                                                                                              \
  }

#define define_pod_var_sync(T, _name, _meta) define_pod_var(T, _name, #_name, _meta, false)
#define define_pod_var_async(T, _name, _meta) define_pod_var(T, _name, #_name, _meta, true)

namespace lotuc::pod
{
  static std::unique_ptr<Context<json>>
  build_json_ctx(std::string &pod_id, std::function<void()> const &cleanup)
  {
    std::unique_ptr<Encoder<json>> encoder;
    std::unique_ptr<Transport> transport;

    std::function<void()> cleanup_transport = nullptr;
    if(is_babashka_transport_socket())
    {
      asio::io_context io_context;
      encoder = std::make_unique<JsonEncoder>();
      transport = std::make_unique<TcpTransport>(io_context);
      cleanup_transport = TcpTransport::remove_portfile;
    }
    else
    {
      encoder = std::make_unique<JsonEncoder>();
      transport = std::make_unique<StdInOutTransport>();
    }

    std::function<void()> cleanup_all{};
    if(cleanup || cleanup_transport)
    {
      cleanup_all = [cleanup_transport, cleanup]() {
        if(cleanup_transport)
        {
          cleanup_transport();
        }
        if(cleanup)
        {
          cleanup();
        }
      };
    }

    return std::make_unique<Context<json>>(pod_id,
                                           std::move(encoder),
                                           std::move(transport),
                                           std::move(cleanup_all));
  }

  static std::unique_ptr<Context<json>>
  build_json_ctx(char const *pod_id, std::function<void()> const &cleanup)
  {
    auto s = std::string(pod_id);
    return build_json_ctx(s, cleanup);
  }

  static std::unique_ptr<Context<json>> build_json_ctx(std::string &pod_id)
  {
    return build_json_ctx(pod_id, nullptr);
  }

  static std::unique_ptr<Context<json>> build_json_ctx(std::function<void()> const &cleanup)
  {
    return build_json_ctx("", cleanup);
  }

  template <typename T>
  class SyncPod : public Pod<T>
  {
  public:
    using Pod<T>::Pod;

    void invoke(std::unique_ptr<typename Var<T>::derefer> derefer) override
    {
      derefer->deref();
      if(!derefer->done)
      {
        derefer->error("illegal var implementation, deref returned without any notice");
      }
    }
  };
}

#endif // POD_HELPER_H_
