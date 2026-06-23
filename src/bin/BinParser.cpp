#include "bin/BinParser.h"

#include "bin/BinTokenizer.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace lol
{
namespace
{
class BinParserImpl
{
public:
    explicit BinParserImpl(std::vector<BinToken> tokens) : m_tokens(std::move(tokens)) {}

    bool Parse(BinObject& output, BinParseError& error)
    {
        if (Peek().type == BinTokenType::EndOfFile)
        {
            error.message = "Expected root object, found end of file";
            error.location = Peek().location;
            return false;
        }

        const std::optional<BinObject> root = ParseObject(error);
        if (!root)
        {
            return false;
        }

        if (Peek().type != BinTokenType::EndOfFile)
        {
            error.message = "Unexpected trailing tokens after root object";
            error.location = Peek().location;
            return false;
        }

        output = *root;
        return true;
    }

private:
    const BinToken& Peek(std::size_t offset = 0) const
    {
        const std::size_t clampedIndex = std::min(m_index + offset, m_tokens.size() - 1);
        return m_tokens[clampedIndex];
    }

    const BinToken& Advance()
    {
        return m_tokens[m_index++];
    }

    bool Match(BinTokenType type)
    {
        if (Peek().type != type)
        {
            return false;
        }
        Advance();
        return true;
    }

    bool Expect(BinTokenType type, const char* message, BinParseError& error)
    {
        if (Match(type))
        {
            return true;
        }

        error.message = message;
        error.location = Peek().location;
        return false;
    }

    bool IsFieldSeparator(const BinToken& token) const
    {
        return token.type == BinTokenType::Colon || token.type == BinTokenType::Equals;
    }

    std::optional<BinObject> ParseObject(BinParseError& error)
    {
        if (Peek().type != BinTokenType::Identifier)
        {
            error.message = "Expected object type identifier";
            error.location = Peek().location;
            return std::nullopt;
        }

        BinObject object;
        const BinToken objectToken = Advance();
        object.typeName = objectToken.lexeme;
        object.location = objectToken.location;

        if (!Expect(BinTokenType::LeftBrace, "Expected '{' after object type", error))
        {
            return std::nullopt;
        }

        while (Peek().type != BinTokenType::RightBrace)
        {
            if (Peek().type == BinTokenType::EndOfFile)
            {
                error.message = "Unterminated object, expected '}'";
                error.location = Peek().location;
                return std::nullopt;
            }

            if (Peek().type != BinTokenType::Identifier)
            {
                error.message = "Expected field name";
                error.location = Peek().location;
                return std::nullopt;
            }

            BinField field;
            const BinToken fieldToken = Advance();
            field.name = fieldToken.lexeme;
            field.location = fieldToken.location;

            if (!IsFieldSeparator(Peek()))
            {
                error.message = "Expected ':' or '=' after field name";
                error.location = Peek().location;
                return std::nullopt;
            }
            Advance();

            const std::optional<BinValue> value = ParseValue(error);
            if (!value)
            {
                return std::nullopt;
            }

            field.value = *value;
            object.fields[field.name] = field.value;
            object.orderedFields.push_back(field);

            Match(BinTokenType::Comma);
            Match(BinTokenType::Semicolon);
        }

        Advance();
        return object;
    }

    std::optional<BinValue> ParseValue(BinParseError& error)
    {
        const BinToken token = Peek();
        switch (token.type)
        {
        case BinTokenType::String:
            Advance();
            return BinValue(token.lexeme, token.location);
        case BinTokenType::Integer:
            Advance();
            return BinValue(std::stoll(token.lexeme), token.location);
        case BinTokenType::Float:
            Advance();
            return BinValue(std::stod(token.lexeme), token.location);
        case BinTokenType::True:
            Advance();
            return BinValue(true, token.location);
        case BinTokenType::False:
            Advance();
            return BinValue(false, token.location);
        case BinTokenType::Null:
            Advance();
            return BinValue(std::monostate{}, token.location);
        case BinTokenType::Identifier:
            if (Peek(1).type == BinTokenType::LeftBrace)
            {
                std::optional<BinObject> object = ParseObject(error);
                if (!object)
                {
                    return std::nullopt;
                }
                return BinValue(std::make_shared<BinObject>(std::move(*object)), token.location);
            }

            Advance();
            return BinValue(token.lexeme, token.location);
        case BinTokenType::LeftBracket:
            return ParseList(error);
        default:
            error.message = "Expected value";
            error.location = token.location;
            return std::nullopt;
        }
    }

    std::optional<BinValue> ParseList(BinParseError& error)
    {
        const BinSourceLocation location = Peek().location;
        Advance();

        BinList values;
        while (Peek().type != BinTokenType::RightBracket)
        {
            if (Peek().type == BinTokenType::EndOfFile)
            {
                error.message = "Unterminated list, expected ']'";
                error.location = Peek().location;
                return std::nullopt;
            }

            const std::optional<BinValue> value = ParseValue(error);
            if (!value)
            {
                return std::nullopt;
            }
            values.push_back(*value);

            if (Peek().type == BinTokenType::Comma)
            {
                Advance();
                continue;
            }

            if (Peek().type == BinTokenType::Semicolon)
            {
                Advance();
            }
        }

        Advance();
        return BinValue(values, location);
    }

    std::vector<BinToken> m_tokens;
    std::size_t m_index = 0;
};
}

const BinValue* BinObject::FindField(const std::string& name) const
{
    const auto iterator = fields.find(name);
    if (iterator == fields.end())
    {
        return nullptr;
    }
    return &iterator->second;
}

bool BinParser::Parse(const std::string& source, BinObject& output, BinParseError& error) const
{
    BinTokenizer tokenizer;
    std::vector<BinToken> tokens = tokenizer.Tokenize(source, error);
    if (tokens.empty())
    {
        return false;
    }

    BinParserImpl parser(std::move(tokens));
    return parser.Parse(output, error);
}

bool BinParser::ParseFile(const std::string& path, BinObject& output, BinParseError& error) const
{
    std::ifstream stream(path);
    if (!stream)
    {
        error.message = "Failed to open BIN text file";
        error.location = {};
        return false;
    }

    std::stringstream buffer;
    buffer << stream.rdbuf();
    return Parse(buffer.str(), output, error);
}
}
