#include "pod.h"
#include "pod_asio.h"
#include "pod_json.h"

#define define_pod_var_(T, _class_name, _ns, _name)                                                \
  class _class_name : public lotuc::pod::Var<T>                                                    \
  {                                                                                                \
    std::string ns() override                                                                      \
    {                                                                                              \
      return _ns;                                                                                  \
    }                                                                                              \
    std::string name() override                                                                    \
    {                                                                                              \
      return _name;                                                                                \
    }                                                                                              \
    void invoke(lotuc::pod::Context<T> const &ctx, std::string const &id, T const &args) override; \
  }

#define define_pod_var(T, _ns, _name) define_pod_var_(T, _ns##_##_name, #_ns, #_name)

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
}
