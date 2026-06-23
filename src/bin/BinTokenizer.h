#pragma once

#include "bin/BinAst.h"

#include <optional>
#include <string>
#include <vector>

namespace lol
{
enum class BinTokenType
{
    Identifier,
    String,
    Integer,
    Float,
    True,
    False,
    Null,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Colon,
    Equals,
    Comma,
    Semicolon,
    EndOfFile
};

struct BinToken
{
    BinTokenType type = BinTokenType::EndOfFile;
    std::string lexeme;
    BinSourceLocation location;
};

class BinTokenizer
{
public:
    std::vector<BinToken> Tokenize(const std::string& source, BinParseError& error) const;
};
}
