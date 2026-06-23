#include "bin/BinTokenizer.h"

#include <cctype>

namespace lol
{
namespace
{
bool IsIdentifierStart(char character)
{
    return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_' || character == '$';
}

bool IsIdentifierCharacter(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0 ||
        character == '_' ||
        character == '.' ||
        character == '/' ||
        character == '$' ||
        character == '-';
}
}

std::vector<BinToken> BinTokenizer::Tokenize(const std::string& source, BinParseError& error) const
{
    std::vector<BinToken> tokens;
    int line = 1;
    int column = 1;
    std::size_t index = 0;

    auto makeLocation = [&]() {
        return BinSourceLocation{line, column};
    };

    auto advance = [&]() -> char {
        const char character = source[index++];
        if (character == '\n')
        {
            ++line;
            column = 1;
        }
        else
        {
            ++column;
        }
        return character;
    };

    while (index < source.size())
    {
        const char character = source[index];
        if (character == ' ' || character == '\t' || character == '\r' || character == '\n')
        {
            advance();
            continue;
        }

        if (character == '#')
        {
            while (index < source.size() && source[index] != '\n')
            {
                advance();
            }
            continue;
        }

        if (character == '/' && index + 1 < source.size() && source[index + 1] == '/')
        {
            advance();
            advance();
            while (index < source.size() && source[index] != '\n')
            {
                advance();
            }
            continue;
        }

        const BinSourceLocation location = makeLocation();
        switch (character)
        {
        case '{':
            advance();
            tokens.push_back({BinTokenType::LeftBrace, "{", location});
            continue;
        case '}':
            advance();
            tokens.push_back({BinTokenType::RightBrace, "}", location});
            continue;
        case '[':
            advance();
            tokens.push_back({BinTokenType::LeftBracket, "[", location});
            continue;
        case ']':
            advance();
            tokens.push_back({BinTokenType::RightBracket, "]", location});
            continue;
        case ':':
            advance();
            tokens.push_back({BinTokenType::Colon, ":", location});
            continue;
        case '=':
            advance();
            tokens.push_back({BinTokenType::Equals, "=", location});
            continue;
        case ',':
            advance();
            tokens.push_back({BinTokenType::Comma, ",", location});
            continue;
        case ';':
            advance();
            tokens.push_back({BinTokenType::Semicolon, ";", location});
            continue;
        case '"':
            {
                advance();
                std::string value;
                bool terminated = false;
                while (index < source.size())
                {
                    const char current = advance();
                    if (current == '"')
                    {
                        terminated = true;
                        break;
                    }

                    if (current == '\\')
                    {
                        if (index >= source.size())
                        {
                            break;
                        }

                        const char escaped = advance();
                        switch (escaped)
                        {
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        case '\\': value.push_back('\\'); break;
                        case '"': value.push_back('"'); break;
                        default: value.push_back(escaped); break;
                        }
                    }
                    else
                    {
                        value.push_back(current);
                    }
                }

                if (!terminated)
                {
                    error.message = "Unterminated string literal";
                    error.location = location;
                    return {};
                }

                tokens.push_back({BinTokenType::String, std::move(value), location});
                continue;
            }
        default:
            break;
        }

        if (IsIdentifierStart(character))
        {
            std::string identifier;
            while (index < source.size() && IsIdentifierCharacter(source[index]))
            {
                identifier.push_back(advance());
            }

            BinTokenType type = BinTokenType::Identifier;
            if (identifier == "true")
            {
                type = BinTokenType::True;
            }
            else if (identifier == "false")
            {
                type = BinTokenType::False;
            }
            else if (identifier == "null")
            {
                type = BinTokenType::Null;
            }

            tokens.push_back({type, std::move(identifier), location});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '+')
        {
            std::string number;
            bool hasDecimalPoint = false;
            if (character == '-' || character == '+')
            {
                number.push_back(advance());
                if (index >= source.size() || std::isdigit(static_cast<unsigned char>(source[index])) == 0)
                {
                    error.message = "Expected digit after numeric sign";
                    error.location = location;
                    return {};
                }
            }

            while (index < source.size())
            {
                const char current = source[index];
                if (std::isdigit(static_cast<unsigned char>(current)) != 0)
                {
                    number.push_back(advance());
                    continue;
                }
                if (current == '.' && !hasDecimalPoint)
                {
                    hasDecimalPoint = true;
                    number.push_back(advance());
                    continue;
                }
                break;
            }

            tokens.push_back({hasDecimalPoint ? BinTokenType::Float : BinTokenType::Integer, std::move(number), location});
            continue;
        }

        error.message = std::string("Unexpected character '") + character + "'";
        error.location = location;
        return {};
    }

    tokens.push_back({BinTokenType::EndOfFile, "", {line, column}});
    return tokens;
}
}
