#pragma once
#include <string>
#include <cstdint>
#include <cctype>
#include <stdexcept>

namespace Solstice::Scripting {

    enum TokenType {
        ID, STRING, NUMBER,
        LET, FUNCTION, ENTRY, RETURN,
        MODULE, IMPORT, CLASS, NEW,
        NOEXPORT, INHERITS, CONSTRUCTOR, DESTRUCTOR,
        LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
        EQ, PLUS, PLUSPLUS, MINUS, MINUSMINUS, COMMA, SEMICOLON, COLON, DOT,
        // Comparisons
        EQ_EQ, NEQ, LT, GT, LE_OP, GE_OP,
        // Compound assignment
        PLUS_EQ, MINUS_EQ, MUL_EQ, DIV_EQ,
        // Logical operators
        AND_LOGIC, OR_LOGIC, NOT_LOGIC,
        // Arithmetic
        MUL, DIV, MOD_OP,
        // Bitwise
        AND_BIT, OR_BIT, XOR_BIT, NOT_BIT, SHL_OP, SHR_OP,
        // Control flow
        IF, ELSE, WHILE, FOR, IN, SWITCH, CASE, DEFAULT, BREAK, CONTINUE, MATCH,
        // Arrow functions
        ARROW,
        // Enums
        ENUM,
        // Lambda
        LAMBDA,
        // Range
        RANGE, // ..
        // String interpolation
        STRING_INTERPOLATION_START, STRING_INTERPOLATION_END,
        // Memory management
        DELETE_KW,
        END
    };

    struct Token {
        TokenType Type;
        std::string Text;
        int64_t IntValue = 0;
        double FloatValue = 0.0;
    };

    class Lexer {
    public:
        Lexer(const std::string& src) : m_Src(src) {}

        Token Next() {
            SkipWhitespace();
            if (m_Pos >= m_Src.size()) return {END, ""};

            char c = m_Src[m_Pos];

            if (isalpha(c) || c == '_' || c == '@') {
                size_t start = m_Pos;
                m_Pos++;
                while (m_Pos < m_Src.size() && (isalnum(m_Src[m_Pos]) || m_Src[m_Pos] == '_' || m_Src[m_Pos] == '.')) m_Pos++;
                std::string text = m_Src.substr(start, m_Pos - start);

                if (text == "let") return {LET, text};
                if (text == "if") return {IF, text};
                if (text == "else") return {ELSE, text};
                if (text == "while") return {WHILE, text};
                if (text == "for") return {FOR, text};
                if (text == "in") return {IN, text};
                if (text == "break") return {BREAK, text};
                if (text == "continue") return {CONTINUE, text};
                if (text == "match") return {MATCH, text};
                if (text == "function") return {FUNCTION, text};
                if (text == "@function") return {FUNCTION, text}; // backward compatibility
                if (text == "@Entry") return {ENTRY, text};
                if (text == "@noexport") return {NOEXPORT, text};
                if (text == "return") return {RETURN, text};
                if (text == "module") return {MODULE, text};
                if (text == "import") return {IMPORT, text};
                if (text == "class") return {CLASS, text};
                if (text == "new") return {NEW, text};
                if (text == "inherits") return {INHERITS, text};
                if (text == "constructor") return {CONSTRUCTOR, text};
                if (text == "destructor") return {DESTRUCTOR, text};
                if (text == "true") return {NUMBER, text, 1}; // Boolean true as number 1
                if (text == "false") return {NUMBER, text, 0}; // Boolean false as number 0
                if (text == "enum") return {ENUM, text};
                if (text == "lambda") return {LAMBDA, text};
                if (text == "delete") return {DELETE_KW, text};

                return {ID, text};
            }

            // Handle negative numbers: - followed by digit
            if (c == '-' && m_Pos + 1 < m_Src.size() && isdigit(m_Src[m_Pos + 1])) {
                m_Pos++; // Skip '-'
                size_t start = m_Pos;
                while (m_Pos < m_Src.size() && isdigit(m_Src[m_Pos])) m_Pos++;

                // Check for decimal point
                bool isFloat = false;
                if (m_Pos < m_Src.size() && m_Src[m_Pos] == '.') {
                    isFloat = true;
                    m_Pos++; // Skip '.'
                    while (m_Pos < m_Src.size() && isdigit(m_Src[m_Pos])) m_Pos++;
                }

                std::string text = "-" + m_Src.substr(start, m_Pos - start);
                if (isFloat) {
                    double floatVal = std::stod(text);
                    Token tok;
                    tok.Type = NUMBER;
                    tok.Text = text;
                    tok.FloatValue = floatVal;
                    tok.IntValue = (int64_t)floatVal; // For compatibility
                    return tok;
                } else {
                    return {NUMBER, text, std::stoll(text)};
                }
            }

            if (isdigit(c)) {
                size_t start = m_Pos;
                while (m_Pos < m_Src.size() && isdigit(m_Src[m_Pos])) m_Pos++;

                // Check for decimal point
                bool isFloat = false;
                if (m_Pos < m_Src.size() && m_Src[m_Pos] == '.') {
                    isFloat = true;
                    m_Pos++; // Skip '.'
                    while (m_Pos < m_Src.size() && isdigit(m_Src[m_Pos])) m_Pos++;
                }

                std::string text = m_Src.substr(start, m_Pos - start);
                if (isFloat) {
                    double floatVal = std::stod(text);
                    Token tok;
                    tok.Type = NUMBER;
                    tok.Text = text;
                    tok.FloatValue = floatVal;
                    tok.IntValue = (int64_t)floatVal; // For compatibility
                    return tok;
                } else {
                    return {NUMBER, text, std::stoll(text)};
                }
            }

            if (c == '"') {
                m_Pos++;
                size_t start = m_Pos;
                std::string text;
                bool inInterpolation = false;
                while (m_Pos < m_Src.size()) {
                    if (m_Src[m_Pos] == '"' && !inInterpolation) {
                        break;
                    } else if (m_Src[m_Pos] == '{' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos + 1] != '{') {
                        // String interpolation start
                        text += m_Src.substr(start, m_Pos - start);
                        m_Pos += 2; // Skip "{
                        return {STRING_INTERPOLATION_START, text};
                    } else if (m_Src[m_Pos] == '}' && inInterpolation) {
                        // String interpolation end
                        m_Pos++;
                        return {STRING_INTERPOLATION_END, ""};
                    } else {
                        m_Pos++;
                    }
                }
                text = m_Src.substr(start, m_Pos - start);
                if (m_Pos < m_Src.size()) m_Pos++; // Skip closing quote
                return {STRING, text};
            }

            // Two char ops
            if (c == '=' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {EQ_EQ, "=="};
            }
            if (c == '!' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {NEQ, "!="};
            }
            if (c == '<' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {LE_OP, "<="};
            }
            if (c == '>' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {GE_OP, ">="};
            }
            if (c == '+' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {PLUS_EQ, "+="};
            }
            if (c == '-' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {MINUS_EQ, "-="};
            }
            if (c == '*' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {MUL_EQ, "*="};
            }
            if (c == '/' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '=') {
                m_Pos += 2; return {DIV_EQ, "/="};
            }
            if (c == '&' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '&') {
                m_Pos += 2; return {AND_LOGIC, "&&"};
            }
            if (c == '|' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '|') {
                m_Pos += 2; return {OR_LOGIC, "||"};
            }
            if (c == '&' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] != '&') {
                m_Pos++; return {AND_BIT, "&"};
            }
            if (c == '|' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] != '|') {
                m_Pos++; return {OR_BIT, "|"};
            }
            if (c == '^') {
                m_Pos++; return {XOR_BIT, "^"};
            }
            if (c == '~') {
                m_Pos++; return {NOT_BIT, "~"};
            }
            if (c == '<' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '<') {
                m_Pos += 2; return {SHL_OP, "<<"};
            }
            if (c == '>' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '>') {
                m_Pos += 2; return {SHR_OP, ">>"};
            }
            if (c == '.' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '.') {
                m_Pos += 2; return {RANGE, ".."};
            }
            if (c == '-' && m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '>') {
                m_Pos += 2; return {ARROW, "=>"};
            }

            m_Pos++;
            switch (c) {
                case '(': return {LPAREN, "("};
                case ')': return {RPAREN, ")"};
                case '{': return {LBRACE, "{"};
                case '}': return {RBRACE, "}"};
                case '[': return {LBRACKET, "["};
                case ']': return {RBRACKET, "]"};
                case '=': return {EQ, "="};
                case '<': return {LT, "<"};
                case '>': return {GT, ">"};
                case '+':
                    if (m_Pos < m_Src.size() && m_Src[m_Pos] == '+') {
                        m_Pos++;
                        return {PLUSPLUS, "++"};
                    }
                    return {PLUS, "+"};
                case '-':
                    if (m_Pos < m_Src.size() && m_Src[m_Pos] == '-') {
                        m_Pos++;
                        return {MINUSMINUS, "--"};
                    }
                    return {MINUS, "-"};
                case '*': return {MUL, "*"};
                case '/': return {DIV, "/"};
                case '%': return {MOD_OP, "%"};
                case '!': return {NOT_LOGIC, "!"};
                case ',': return {COMMA, ","};
                case ';': return {SEMICOLON, ";"};
                case ':': return {COLON, ":"};
                case '.': return {DOT, "."};
            }

            return {END, ""};
        }

        Token Peek() {
            size_t savedPos = m_Pos; // Lexer copy? No, m_Pos is size_t member.
            // But Next() modifies m_Pos.
            // We need to restore it.
            Token t = Next();
            m_Pos = savedPos;
            return t;
        }

    private:
        std::string m_Src;
        size_t m_Pos = 0;

        void SkipWhitespace() {
            while (m_Pos < m_Src.size() && (isspace(m_Src[m_Pos]) || m_Src[m_Pos] == '/')) {
                if (m_Src[m_Pos] == '/') {
                    if (m_Pos + 1 < m_Src.size() && m_Src[m_Pos+1] == '/') {
                        // Comment
                        while (m_Pos < m_Src.size() && m_Src[m_Pos] != '\n') m_Pos++;
                    } else {
                        break;
                    }
                } else {
                    m_Pos++;
                }
            }
        }
    };

}
