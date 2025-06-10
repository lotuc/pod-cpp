#include "jsonrpc.h"

#include <asio/error.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>

namespace lotuc::pod
{
  class StdInOutLinedJsonTransport : public JsonRpcTransport
  {
    std::mutex write_lock;

    json read() override
    {
      std::string s{};
      while(s.empty())
      {
        std::getline(std::cin, s);
      }
      return json::parse(s);
    }

    void write(json const &v) override
    {
      std::lock_guard<std::mutex> lock(write_lock);
      std::cout << v.dump() << "\n" << std::flush;
    }
  };

  using tcp = asio::ip::tcp;

  class TcpLinedJsonTransport : public JsonRpcTransport
  {
  public:
    std::mutex write_lock;
    unsigned short _port;
    tcp::acceptor _acceptor;
    tcp::iostream _stream;
    volatile bool _accepted{};

    TcpLinedJsonTransport(asio::io_context &io_context, unsigned short port = 0)
      : _port(port)
      , _acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    void _accept()
    {
      if(_accepted)
      {
        return;
      }
      asio::error_code ec;
      auto _ = _acceptor.accept(_stream.socket(), ec);
      if(ec)
      {
        throw "accept exception: " + std::to_string(ec.value());
      }
      _accepted = true;
    }

    json read() override
    {
      _accept();
      std::string s{};
      while(s.empty())
      {
        std::getline(_stream, s);
      }
      return json::parse(s);
    }

    void write(json const &data) override
    {
      _accept();
      std::lock_guard<std::mutex> lock(write_lock);
      _stream << data.dump() << "\n";
      _stream.flush();
    }
  };

}
