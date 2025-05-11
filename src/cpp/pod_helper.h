#include "pod.h"
#include "pod_asio.h"

#define define_pod_var_(_class_name, _ns, _name)                                                   \
  class _class_name : public lotuc::pod::Var                                                       \
  {                                                                                                \
    std::string ns() override                                                                      \
    {                                                                                              \
      return _ns;                                                                                  \
    }                                                                                              \
    std::string name() override                                                                    \
    {                                                                                              \
      return _name;                                                                                \
    }                                                                                              \
    void invoke(lotuc::pod::Context const &ctx, std::string const &id, json const &args) override; \
  }

#define define_pod_var(_ns, _name) define_pod_var_(_ns##_##_name, #_ns, #_name)

namespace lotuc::pod
{
  static std::unique_ptr<Context>
  build_ctx(std::string &pod_id, std::function<void()> const &cleanup)
  {
    std::unique_ptr<Encoder> encoder;
    std::unique_ptr<Transport> transport;

    std::function<void()> cleanup_transport = nullptr;
    if(Pod::is_babashka_transport_socket())
    {
      asio::io_context io_context;
      encoder = std::make_unique<Encoder>();
      transport = std::make_unique<TcpTransport>(io_context);
      cleanup_transport = TcpTransport::remove_portfile;
    }
    else
    {
      encoder = std::make_unique<Encoder>();
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

    return std::make_unique<Context>(pod_id,
                                     std::move(encoder),
                                     std::move(transport),
                                     std::move(cleanup_all));
  }

  static std::unique_ptr<Context>
  build_ctx(char const *pod_id, std::function<void()> const &cleanup)
  {
    auto s = std::string(pod_id);
    return build_ctx(s, cleanup);
  }

  static std::unique_ptr<Context> build_ctx(std::string &pod_id)
  {
    return build_ctx(pod_id, nullptr);
  }

  static std::unique_ptr<Context> build_ctx(std::function<void()> const &cleanup)
  {
    return build_ctx("", cleanup);
  }
}
