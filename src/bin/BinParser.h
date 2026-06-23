#pragma once

#include "bin/BinAst.h"

#include <optional>
#include <string>

namespace lol
{
class BinParser
{
public:
    bool Parse(const std::string& source, BinObject& output, BinParseError& error) const;
    bool ParseFile(const std::string& path, BinObject& output, BinParseError& error) const;
};
}
