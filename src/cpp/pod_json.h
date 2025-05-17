#ifndef POD_JSON_H_
#define POD_JSON_H_

#include "pod.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lotuc::pod
{
  class JsonEncoder : public Encoder<json>
  {
  public:
    JsonEncoder()
      : Encoder<json>{ "json" }
    {
    }

    bool is_dict(json const &v) override
    {
      return v.is_object();
    }

    json make_dict(std::string const &k, json const &v) override
    {
      return {
        { k, v }
      };
    }

    json empty_dict() override
    {
      return json::object();
    }

    json empty_list() override
    {
      return json::array();
    }

    std::string encode(json const &d) override
    {
      return d.dump();
    }

    std::string encode(std::vector<std::string> const &status) override
    {
      return json(status).dump();
    }

    std::string encode_pendings(
      std::map<std::string, std::pair<json, long long>> const &pending_args_start_ts) override
    {
      json r;
      for(auto &p : pending_args_start_ts)
      {
        r[p.first] = {
          {     "args",  p.second.first },
          { "start-ts", p.second.second }
        };
      }
      return r.dump();
    }

    json decode(std::string const &s) override
    {
      return json::parse(s);
    }
  };
}

#endif // POD_JSON_H_
