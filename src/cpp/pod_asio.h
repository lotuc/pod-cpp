#ifndef POD_IMPL_H_
#define POD_IMPL_H_

#include "bencode.hpp"
#include "pod.h"

#include "asio.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

using tcp = asio::ip::tcp;

namespace
{

  namespace fs = std::filesystem;

  std::string get_absolute_path(std::string const &path_string)
  {
    fs::path path_object(path_string);
    fs::path absolute_path = fs::absolute(path_object);
    return absolute_path.string();
  }

  std::string portfile()
  {
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif

    std::stringstream ss;
    ss << ".babashka-pod-" << pid << ".port";
    return ss.str();
  }

  void _spit_portfile(unsigned short port)
  {
    std::ofstream outfile(portfile());
    if(outfile.is_open())
    {
      outfile << port << '\n';
      outfile.flush();
      outfile.close();
    }
    else
    {
      throw "cannot write to port file";
    }
  }

  void _remove_portfile()
  {
    auto f = portfile();
    std::ifstream file(f);
    if(file.good())
    {
      std::remove(f.c_str());
    }
  }
}

namespace lotuc::pod
{
  using tcp = asio::ip::tcp;

  class TcpTransport : public Transport
  {
  public:
    unsigned short _port;
    tcp::acceptor _acceptor;
    tcp::iostream _stream;
    bool _accepted{};

    static void remove_portfile()
    {
      _remove_portfile();
    }

    TcpTransport(asio::io_context &io_context, unsigned short port = 0)
      : _port(port)
      , _acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
      _spit_portfile(_acceptor.local_endpoint().port());
      std::atexit(remove_portfile);
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

    bc::data read() override
    {
      _accept();
      auto v = bc::decode_some(_stream, bc::no_check_eof);
      return v;
    }

    void write(bc::data const &data) override
    {
      _accept();
      bc::encode_to(_stream, data);
      _stream.flush();
    }
  };
}

#endif // POD_IMPL_H_
