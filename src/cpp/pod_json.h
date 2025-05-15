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

    std::string encode(json const &d) override
    {
      return d.dump();
    }

    std::string encode(std::vector<std::string> const &status) override
    {
      return json(status).dump();
    }

    json decode(std::string const &s) override
    {
      return json::parse(s);
    }
  };
}

#endif // POD_JSON_H_
