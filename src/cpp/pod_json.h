#ifndef POD_HELPER_H_
#define POD_HELPER_H_

#include "pod.h"
#include "pod_asio_transport.h"
#include "pod_json_encoder.h"
#include "jsonrpc.h"

// JSON format, asio as tcp transport implementation.

namespace lotuc::pod
{


  template <typename C>
  inline std::unique_ptr<Context<json, C>> build_jsonrpc_ctx(std::string const &pod_id,
                                                             C &components,
                                                             JsonRpcTransport *jsonrpc_transport,
                                                             std::function<void()> const &cleanup)
  {
    std::unique_ptr<Encoder<json>> encoder;
    std::unique_ptr<BencodeTransport> transport;

    encoder = std::make_unique<JsonEncoder>();
    transport = std::make_unique<AdaptedBencodeTransport>(jsonrpc_transport);

    std::function<void()> cleanup_all{};
    if(cleanup)
    {
      cleanup_all = [cleanup]() {
        if(cleanup)
        {
          cleanup();
        }
      };
    }

    return std::make_unique<Context<json, C>>(pod_id,
                                              components,
                                              std::move(encoder),
                                              std::move(transport),
                                              std::move(cleanup_all));
  }

  template <typename C>
  inline std::unique_ptr<Context<json, C>>
  build_json_ctx(std::string const &pod_id, C &components, std::function<void()> const &cleanup)
  {
    std::unique_ptr<Encoder<json>> encoder;
    std::unique_ptr<BencodeTransport> transport;

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

    return std::make_unique<Context<json, C>>(pod_id,
                                              components,
                                              std::move(encoder),
                                              std::move(transport),
                                              std::move(cleanup_all));
  }

  template <typename C>
  inline std::unique_ptr<Context<json, C>>
  build_json_ctx(char const *pod_id, C &components, std::function<void()> const &cleanup)
  {
    auto s = std::string(pod_id);
    return build_json_ctx<C>(s, components, cleanup);
  }

  template <typename C>
  inline std::unique_ptr<Context<json, C>> build_json_ctx(std::string const &pod_id, C &components)
  {
    return build_json_ctx<C>(pod_id, components, nullptr);
  }

  template <typename C>
  inline std::unique_ptr<Context<json, C>>
  build_json_ctx(C &components, std::function<void()> const &cleanup)
  {
    return build_json_ctx<C>("", components, cleanup);
  }

  template <typename C>
  inline pod::PodImpl<json, C> build_pod(pod::Context<json, C> &ctx, int max_concurrent = 1024)
  {
    return PodImpl<json, C>{ ctx, max_concurrent };
  }
}

#endif // POD_HELPER_H_
