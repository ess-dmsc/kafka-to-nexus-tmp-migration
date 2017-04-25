#include "helper.h"
#include <array>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

std::vector<char> gulp(std::string fname) {
  std::vector<char> ret;
  std::ifstream ifs(fname, std::ios::binary | std::ios::ate);
  if (!ifs.good())
    return ret;
  auto n1 = ifs.tellg();
  if (n1 <= 0)
    return ret;
  ret.resize(n1);
  ifs.seekg(0);
  ifs.read(ret.data(), n1);
  return ret;
}

std::vector<char> binary_to_hex(char const *data, int len) {
  std::vector<char> ret;
  ret.reserve(len * (64 + 5) / 32 + 32);
  for (uint32_t i1 = 0; i1 < len; ++i1) {
    uint8_t c = ((uint8_t)data[i1]) >> 4;
    if (c < 10)
      c += 48;
    else
      c += 97 - 10;
    ret.emplace_back(c);
    c = 0x0f & (uint8_t)data[i1];
    if (c < 10)
      c += 48;
    else
      c += 97 - 10;
    ret.emplace_back(c);
    if ((0x07 & i1) == 0x7) {
      ret.push_back(' ');
      if ((0x1f & i1) == 0x1f)
        ret.push_back('\n');
    }
  }
  return ret;
}

std::vector<std::string> split(std::string const &input, std::string token) {
  using std::vector;
  using std::string;
  vector<string> ret;
  if (token.size() == 0)
    return { input };
  string::size_type i1 = 0;
  while (true) {
    auto i2 = input.find(token, i1);
    if (i2 == string::npos)
      break;
    if (i2 > i1) {
      ret.push_back(input.substr(i1, i2 - i1));
    }
    i1 = i2 + 1;
  }
  if (i1 != input.size()) {
    ret.push_back(input.substr(i1));
  }
  return ret;
}

get_json_ret_int::operator bool() const { return ok == 0; }

get_json_ret_int::operator int() const { return v; }

std::string get_string(rapidjson::Value const *v, std::string path) {
  auto a = split(path, ".");
  uint32_t i1 = 0;
  for (auto &x : a) {
    bool num = true;
    for (char &c : x) {
      if (c < 48 || c > 57) {
        num = false;
        break;
      }
    }
    if (num) {
      if (!v->IsArray())
        return "";
      auto n1 = (uint32_t)strtol(x.c_str(), nullptr, 10);
      if (n1 >= v->Size())
        return "";
      auto &v2 = v->GetArray()[n1];
      if (i1 == a.size() - 1) {
        if (v2.IsString()) {
          return v2.GetString();
        }
      } else {
        v = &v2;
      }
    } else {
      if (!v->IsObject())
        return "";
      auto it = v->FindMember(x.c_str());
      if (it == v->MemberEnd()) {
        return "";
      }
      if (i1 == a.size() - 1) {
        if (it->value.IsString()) {
          return it->value.GetString();
        }
      } else {
        v = &it->value;
      }
    }
    ++i1;
  }
  return "";
}

get_json_ret_int get_int(rapidjson::Value const *v, std::string path) {
  auto a = split(path, ".");
  uint32_t i1 = 0;
  for (auto &x : a) {
    if (!v->IsObject())
      return { 1, 0 };
    auto it = v->FindMember(x.c_str());
    if (it == v->MemberEnd()) {
      return { 1, 0 };
    }
    if (i1 == a.size() - 1) {
      if (it->value.IsInt()) {
        return { 0, it->value.GetInt() };
      }
    } else {
      v = &it->value;
    }
    ++i1;
  }
  return { 1, 0 };
}
