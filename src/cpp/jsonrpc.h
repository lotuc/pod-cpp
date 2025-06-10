#ifndef JSONRPC_H_
#define JSONRPC_H_

#include "bencode.hpp"
#include "pod.h"

#include <atomic>
#include <nlohmann/json.hpp>
#include <queue>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using json = nlohmann::json;

// Adapting the babashka pod to a JSON-RPC 2.0 server.

namespace lotuc::pod
{
  class JsonRpcTransport
  {
  public:
    virtual json read() = 0;
    virtual void write(json const &v) = 0;
    virtual ~JsonRpcTransport() = default;
  };

  class AdaptedBencodeTransport : public BencodeTransport
  {
  public:
    AdaptedBencodeTransport(JsonRpcTransport *transport)
      : _transport{ transport }
    {
    }

    JsonRpcTransport *_transport;
    std::mutex write_lock;
    std::atomic_int notification_id;
    std::queue<bc::data> batching{};

    bc::data get_id(json const &r)
    {
      if(r.contains("id"))
      {
        auto &id = r["id"];
        if(id.is_string())
        {
          return id.get<std::string>();
        }
        if(id.is_number_integer())
        {
          return id.get<int>();
        }
        else if(id.is_number_unsigned())
        {
          return id.get<unsigned int>();
        }
        else
        {
          throw std::runtime_error{ "unsupported id: " + id.dump() };
        }
      }
      else
      {
        return std::string("notification-") + std::to_string(notification_id.fetch_add(1));
      }
    }

    bc::data jsonrpc_call_to_pod_invoke(json const &r)
    {
      bc::dict res;
      auto method = r["method"].get<std::string>();
      if(method.starts_with("lotuc.babashka.pods/"))
      {
        // defaults to be describe
        if(method == "lotuc.babashka.pods/shutdown")
        {
          res["op"] = "shutdown";
          return res;
        }
        else if(method == "lotuc.babashka.pods/describe")
        {
          res["op"] = "describe";
          return res;
        }
        else if(method == "lotuc.babashka.pods/load-ns")
        {
          res["op"] = "load-ns";
          res["ns"] = r["params"].get<std::string>();
          res["id"] = get_id(r);
          return res;
        }
      }

      res["op"] = "invoke";
      res["var"] = r["method"].get<std::string>();
      res["args"] = r["params"].dump();
      res["id"] = get_id(r);

      return res;
    }

    json bc_data_to_json(bc::data const &d)
    {
      if(auto v = std::get_if<std::string>(&d); v)
      {
        return *v;
      }
      else if(auto v = std::get_if<bc::integer>(&d); v)
      {
        return *v;
      }
      else if(auto v = std::get_if<bc::list>(&d); v)
      {
        json r;
        for(auto &it : *v)
        {
          r.push_back(bc_data_to_json(it));
        }
        return r;
      }
      else if(auto v = std::get_if<bc::dict>(&d); v)
      {
        json r;
        for(auto &it : *v)
        {
          r[it.first] = bc_data_to_json(it.second);
        }
        return r;
      }
      else
      {
        return {};
      }
    }

    bc::data read() override
    {
      if(!batching.empty())
      {
        auto &tmp = batching.front();
        batching.pop();
        return tmp;
      }

      auto tmp = _transport->read();
      if(tmp.is_object())
      {
        return jsonrpc_call_to_pod_invoke(tmp);
      }
      else if(tmp.is_array() && !tmp.empty())
      {
        for(auto &v : tmp)
        {
          batching.emplace(jsonrpc_call_to_pod_invoke(v));
        }
      }
      return read();
    }

    void write(bc::data const &_data) override
    {
      std::lock_guard<std::mutex> lock(write_lock);
      auto data = std::get<bc::dict>(_data);


      json id{};
      if(data.count("id"))
      {
        auto _id = data["id"];
        if(auto v = std::get_if<bc::integer>(&_id); v)
        {
          id = *v;
        }
        else if(auto v = std::get_if<bc::string>(&_id); v)
        {
          id = *v;
        }
      }

      json res;


      if(data.count("status"))
      {
        auto status = data.count("status") ? std::get<bc::list>(data["status"]) : bc::list{};
        bool done{};
        bool error{};
        for(auto &s : status)
        {
          auto ss = std::get<std::string>(s);
          if(ss == "done")
          {
            done = true;
          }
          else if(ss == "error")
          {
            error = true;
          }
        }
        json tmp = {
          { "id", id }
        };

        if(error)
        {
          auto ex_message = data.count("ex-message") ? std::get<std::string>(data["ex-message"])
                                                     : "unkown error (no ex-message given)";
          auto ex_data = data.count("ex-data") ? json::parse(std::get<std::string>(data["ex-data"]))
                                               : json::object();
          tmp["error"] = {
            { "ex-message", ex_message },
            {    "ex-data",    ex_data }
          };
        }
        else
        {
          if(data.count("value"))
          {
            tmp["result"] = json::parse(std::get<std::string>(data["value"]));
          }
          else if(done)
          {
            tmp["result"] = json{};
          }
        }
        if(done || error)
        {
          res = tmp;
          res["jsonrpc"] = "2.0";
        }
        else
        {
          tmp["type"] = "partial";
          res = {
            { "jsonrpc",                              "2.0" },
            {  "method", "lotuc.babashka.pods/notification" },
            {  "params",                                tmp }
          };
        }
      }
      else if(data.count("out") || data.count("err"))
      {
        // converts to notification call.
        auto params = json::object();
        if(data.count("out"))
        {
          params["type"] = "stdout";
          params["out"] = std::get<std::string>(data["out"]);
        }
        if(data.count("err"))
        {
          params["type"] = "stderr";
          params["err"] = std::get<std::string>(data["err"]);
        }
        if(!id.is_null())
        {
          params["id"] = id;
        }
        res = {
          { "jsonrpc",                              "2.0" },
          {  "method", "lotuc.babashka.pods/notification" },
          {  "params",                             params }
        };
      }
      else
      {
        auto d = bc_data_to_json(data);
        if(!id.is_null())
        {
          d.erase("id");
          res = {
            { "jsonrpc", "2.0" },
            {      "id",    id },
            {  "result",     d }
          };
        }
        else
        {
          json params;
          auto type = d.contains("namespaces") ? "describe" : "unkown";
          params = {
            { "type", type },
            { "data",    d }
          };
          res = {
            { "jsonrpc",                              "2.0" },
            {  "method", "lotuc.babashka.pods/notification" },
            {  "params",                             params }
          };
        }
      }
      _transport->write(res);
    }
  };

}

#endif // JSONRPC_H_
