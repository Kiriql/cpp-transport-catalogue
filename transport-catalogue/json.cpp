#include "json.h"

#include <iterator>

namespace json {

namespace {

using namespace std::literals;

Node LoadNode(std::istream& input);
Node LoadString(std::istream& input);

std::string LoadLiteral(std::istream& input) {
    std::string s;
    while (std::isalpha(input.peek())) {
        s.push_back(static_cast<char>(input.get()));
    }
    return s;
}

Node LoadArray(std::istream& input) {
    std::vector<Node> result;

    for (char c; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }
    if (!input) {
        throw ParsingError("Array parsing error"s);
    }
    return Node(std::move(result));
}

Node LoadDict(std::istream& input) {
    Dict dict;

    for (char c; input >> c && c != '}';) {
        if (c == '"') {
            std::string key = LoadString(input).AsString();
            if (input >> c && c == ':') {
                if (dict.find(key) != dict.end()) {
                    throw ParsingError("Duplicate key '"s + key + "' have been found");
                }
                dict.emplace(std::move(key), LoadNode(input));
            }
            else {
                throw ParsingError(": is expected but '"s + c + "' has been found"s);
            }
        }
        else if (c != ',') {
            throw ParsingError(R"(',' is expected but ')"s + c + "' has been found"s);
        }
    }
    if (!input) {
        throw ParsingError("Dictionary parsing error"s);
    }
    return Node(std::move(dict));
}

Node LoadString(std::istream& input) {
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            throw ParsingError("String parsing error");
        }
        const char ch = *it;
        if (ch == '"') {
            ++it;
            break;
        }
        else if (ch == '\\') {
            ++it;
            if (it == end) {
                throw ParsingError("String parsing error");
            }
            const char escaped_char = *(it);
            switch (escaped_char) {
            case 'n':
                s.push_back('\n');
                break;
            case 't':
                s.push_back('\t');
                break;
            case 'r':
                s.push_back('\r');
                break;
            case '"':
                s.push_back('"');
                break;
            case '\\':
                s.push_back('\\');
                break;
            default:
                throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
            }
        }
        else if (ch == '\n' || ch == '\r') {
            throw ParsingError("Unexpected end of line"s);
        }
        else {
            s.push_back(ch);
        }
        ++it;
    }

    return Node(std::move(s));
}

Node LoadBool(std::istream& input) {
    const auto s = LoadLiteral(input);
    if (s == "true"sv) {
        return Node{ true };
    }
    else if (s == "false"sv) {
        return Node{ false };
    }
    else {
        throw ParsingError("Failed to parse '"s + s + "' as bool"s);
    }
}

Node LoadNull(std::istream& input) {
    if (auto literal = LoadLiteral(input); literal == "null"sv) {
        return Node{ nullptr };
    }
    else {
        throw ParsingError("Failed to parse '"s + literal + "' as null"s);
    }
}

Node LoadNumber(std::istream& input) {
    std::string parsed_num;

    // Ñ÷èòûâàåò â parsed_num î÷åðåäíîé ñèìâîë èç input
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw ParsingError("Failed to read number from stream"s);
        }
    };

    // Ñ÷èòûâàåò îäíó èëè áîëåå öèôð â parsed_num èç input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw ParsingError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }
    // Ïàðñèì öåëóþ ÷àñòü ÷èñëà
    if (input.peek() == '0') {
        read_char();
        // Ïîñëå 0 â JSON íå ìîãóò èäòè äðóãèå öèôðû
    }
    else {
        read_digits();
    }

    bool is_int = true;
    // Ïàðñèì äðîáíóþ ÷àñòü ÷èñëà
    if (input.peek() == '.') {
        read_char();
        read_digits();
        is_int = false;
    }

    // Ïàðñèì ýêñïîíåíöèàëüíóþ ÷àñòü ÷èñëà
    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
        is_int = false;
    }

    try {
        if (is_int) {
            // Ñíà÷àëà ïðîáóåì ïðåîáðàçîâàòü ñòðîêó â int
            try {
                return std::stoi(parsed_num);
            }
            catch (...) {
                // Â ñëó÷àå íåóäà÷è, íàïðèìåð, ïðè ïåðåïîëíåíèè
                // êîä íèæå ïîïðîáóåò ïðåîáðàçîâàòü ñòðîêó â double
            }
        }
        return std::stod(parsed_num);
    }
    catch (...) {
        throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
    }
}

Node LoadNode(std::istream& input) {
    char c;
    if (!(input >> c)) {
        throw ParsingError("Unexpected EOF"s);
    }
    switch (c) {
    case '[':
        return LoadArray(input);
    case '{':
        return LoadDict(input);
    case '"':
        return LoadString(input);
    case 't':
        // Àòðèáóò [[fallthrough]] (ïðîâàëèòüñÿ) íè÷åãî íå äåëàåò, è ÿâëÿåòñÿ
        // ïîäñêàçêîé êîìïèëÿòîðó è ÷åëîâåêó, ÷òî çäåñü ïðîãðàììèñò ÿâíî çàäóìûâàë
        // ðàçðåøèòü ïåðåõîä ê èíñòðóêöèè ñëåäóþùåé âåòêè case, à íå ñëó÷àéíî çàáûë
        // íàïèñàòü break, return èëè throw.
        // Â äàííîì ñëó÷àå, âñòðåòèâ t èëè f, ïåðåõîäèì ê ïîïûòêå ïàðñèíãà
        // ëèòåðàëîâ true ëèáî false
        [[fallthrough]];
    case 'f':
        input.putback(c);
        return LoadBool(input);
    case 'n':
        input.putback(c);
        return LoadNull(input);
    default:
        input.putback(c);
        return LoadNumber(input);
    }
}

struct PrintContext {
    std::ostream& out;
    int indent_step = 4;
    int indent = 0;

    void PrintIndent() const {
        for (int i = 0; i < indent; ++i) {
            out.put(' ');
        }
    }

    PrintContext Indented() const {
        return { out, indent_step, indent_step + indent };
    }
};

void PrintNode(const Node& value, const PrintContext& ctx);

template <typename Value>
void PrintValue(const Value& value, const PrintContext& ctx) {
    ctx.out << value;
}

void PrintString(const std::string& value, std::ostream& out) {
    out.put('"');
    for (const char c : value) {
        switch (c) {
        case '\r':
            out << "\\r"sv;
            break;
        case '\n':
            out << "\\n"sv;
            break;
        case '"':
            // Ñèìâîëû " è \ âûâîäÿòñÿ êàê \" èëè \\, ñîîòâåòñòâåííî
            [[fallthrough]];
        case '\\':
            out.put('\\');
            [[fallthrough]];
        default:
            out.put(c);
            break;
        }
    }
    out.put('"');
}

template <>
void PrintValue<std::string>(const std::string& value, const PrintContext& ctx) {
    PrintString(value, ctx.out);
}

template <>
void PrintValue<std::nullptr_t>(const std::nullptr_t&, const PrintContext& ctx) {
    ctx.out << "null"sv;
}

// Â ñïåöèàëèçàöè øàáëîíà PrintValue äëÿ òèïà bool ïàðàìåòð value ïåðåäà¸òñÿ
// ïî êîíñòàíòíîé ññûëêå, êàê è â îñíîâíîì øàáëîíå.
// Â êà÷åñòâå àëüòåðíàòèâû ìîæíî èñïîëüçîâàòü ïåðåãðóçêó:
// void PrintValue(bool value, const PrintContext& ctx);
template <>
void PrintValue<bool>(const bool& value, const PrintContext& ctx) {
    ctx.out << (value ? "true"sv : "false"sv);
}

template <>
void PrintValue<Array>(const Array& nodes, const PrintContext& ctx) {
    std::ostream& out = ctx.out;
    out << "[\n"sv;
    bool first = true;
    auto inner_ctx = ctx.Indented();
    for (const Node& node : nodes) {
        if (first) {
            first = false;
        }
        else {
            out << ",\n"sv;
        }
        inner_ctx.PrintIndent();
        PrintNode(node, inner_ctx);
    }
    out.put('\n');
    ctx.PrintIndent();
    out.put(']');
}

template <>
void PrintValue<Dict>(const Dict& nodes, const PrintContext& ctx) {
    std::ostream& out = ctx.out;
    out << "{\n"sv;
    bool first = true;
    auto inner_ctx = ctx.Indented();
    for (const auto& [key, node] : nodes) {
        if (first) {
            first = false;
        }
        else {
            out << ",\n"sv;
        }
        inner_ctx.PrintIndent();
        PrintString(key, ctx.out);
        out << ": "sv;
        PrintNode(node, inner_ctx);
    }
    out.put('\n');
    ctx.PrintIndent();
    out.put('}');
}

void PrintNode(const Node& node, const PrintContext& ctx) {
    std::visit(
        [&ctx](const auto& value) {
            PrintValue(value, ctx);
        },
        node.GetValue());
}

}  // namespace

Document Load(std::istream& input) {
    return Document{ LoadNode(input) };
}

void Print(const Document& doc, std::ostream& output) {
    PrintNode(doc.GetRoot(), PrintContext{ output });
}

} // namespace json