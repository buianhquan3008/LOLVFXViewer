#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace lol
{
struct BinSourceLocation
{
    int line = 1;
    int column = 1;
};

struct BinObject;
struct BinValue;

using BinObjectPtr = std::shared_ptr<BinObject>;
using BinList = std::vector<BinValue>;

struct BinValue
{
    using Variant = std::variant<std::monostate, bool, int64_t, double, std::string, BinList, BinObjectPtr>;

    Variant data;
    BinSourceLocation location;

    BinValue() = default;
    explicit BinValue(Variant variant, BinSourceLocation sourceLocation = {}) : data(std::move(variant)), location(sourceLocation) {}

    bool IsNull() const { return std::holds_alternative<std::monostate>(data); }
    bool IsBool() const { return std::holds_alternative<bool>(data); }
    bool IsInt() const { return std::holds_alternative<int64_t>(data); }
    bool IsNumber() const { return IsInt() || std::holds_alternative<double>(data); }
    bool IsString() const { return std::holds_alternative<std::string>(data); }
    bool IsList() const { return std::holds_alternative<BinList>(data); }
    bool IsObject() const { return std::holds_alternative<BinObjectPtr>(data); }

    const bool* AsBool() const { return std::get_if<bool>(&data); }
    const int64_t* AsInt() const { return std::get_if<int64_t>(&data); }
    const double* AsDouble() const { return std::get_if<double>(&data); }
    const std::string* AsString() const { return std::get_if<std::string>(&data); }
    const BinList* AsList() const { return std::get_if<BinList>(&data); }
    const BinObjectPtr* AsObject() const { return std::get_if<BinObjectPtr>(&data); }
};

struct BinField
{
    std::string name;
    BinValue value;
    BinSourceLocation location;
};

struct BinObject
{
    std::string typeName;
    std::unordered_map<std::string, BinValue> fields;
    std::vector<BinField> orderedFields;
    BinSourceLocation location;

    const BinValue* FindField(const std::string& name) const;
};

struct BinParseError
{
    std::string message;
    BinSourceLocation location;
};
}
