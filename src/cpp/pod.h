#ifndef POD_H_
#define POD_H_


#include <nlohmann/json.hpp>
#include "bencode.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

using json = nlohmann::json;
namespace bc = bencode;

namespace lotuc::pod
{
  class Encoder
  {
  public:
    std::string format()
    {
      return "json";
    }

    std::string encode(json const &d)
    {
      return d.dump();
    }

    std::string encode(std::vector<std::string> const &statuses)
    {
      return json(statuses).dump();
    }

    json decode(std::string const &s)
    {
      return json::parse(s);
    }
  };

  class Transport
  {
  public:
    bc::data read()
    {
      return bc::decode_some(std::cin, bc::no_check_eof);
    }

    void write(bc::data const &data)
    {
      bc::encode_to(std::cout, data);
      std::cout << std::flush;
    }

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
                             bc::data const &ex_data,
                             bc::list const &status)
    {
      write(bc::dict{
        {         "id",         id },
        { "ex-message", ex_message },
        {    "ex-data",    ex_data },
        {     "status",     status }
      });
    }

    void send_invoke_callback(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",         id },
        {  "value",      value },
        { "status", bc::list() }
      });
    }

    void send_invoke_success(std::string const &id, bc::data const &value)
    {
      write(bc::dict{
        {     "id",                   id },
        {  "value",                value },
        { "status", bc::list({ "done" }) }
      });
    }
  };

  class Var;

  class Context
  {
  public:
    std::unique_ptr<Encoder> _encoder;
    std::unique_ptr<Transport> _transport;
    std::function<void()> _shutdown;
    std::map<std::string, std::unique_ptr<Var>> _vars;

    Context(std::unique_ptr<Encoder> encoder,
            std::unique_ptr<Transport> transport,
            std::function<void()> shutdown)
      : _encoder{ std::move(encoder) }
      , _transport{ std::move(transport) }
      , _shutdown{ std::move(shutdown) }
    {
    }

    void add_var(std::unique_ptr<Var> var);
    Var *find_var(std::string const &qualified_name);
    bc::data describe();
  };

  class Var
  {
  public:
    virtual ~Var() = default;

    virtual std::string ns() = 0;

    virtual std::string name() = 0;

    // edn string
    virtual std::string meta()
    {
      return "";
    }

    virtual void invoke(Context const &ctx, std::string const &id, json const &args) = 0;
  };

  inline void Context::add_var(std::unique_ptr<Var> var)
  {
    _vars[var->ns() + "/" + var->name()] = std::move(var);
  }

  inline Var *Context::find_var(std::string const &qualified_name)
  {
    if(_vars.contains(qualified_name))
    {
      return _vars[qualified_name].get();
    }
    return nullptr;
  }

  inline bc::data Context::describe()
  {
    bc::dict ops;
    {
      if(_shutdown)
      {
        ops = bc::dict{
          { "shutdown", bc::dict{} }
        };
      }
    }


    bc::list ns;
    {
      std::map<std::string, bc::list> ns_vars;
      for(auto const &v : _vars)
      {
        ns_vars[v.second->ns()].emplace_back(bc::dict{
          { "name", v.second->name() },
          { "meta", v.second->meta() }
        });
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
}

#endif // POD_H_
