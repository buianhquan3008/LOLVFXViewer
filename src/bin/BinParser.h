#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lol
{
struct BinObject;
using BinObjectPtr = std::shared_ptr<BinObject>;
using BinValue = std::variant<std::monostate, bool, int64_t, double, std::string, std::vector<std::string>, BinObjectPtr>;

struct BinObject
{
    std::string typeName;
    std::unordered_map<std::string, BinValue> fields;
};

class BinParser
{
public:
    virtual ~BinParser() = default;
    virtual bool Parse(const std::string& source, BinObject& output, std::string& error) = 0;
};
}
