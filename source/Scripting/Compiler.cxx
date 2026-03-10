#include "Compiler.hxx"
#include "MemoryAnalysis.hxx"
#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <filesystem>

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

    class Parser {
    public:
        Parser(const std::string& src) : m_Lexer(src) {
            Advance();
        }

        Program Parse() {
            // Process module and imports first
            while (m_Current.Type == MODULE || m_Current.Type == IMPORT) {
                if (m_Current.Type == MODULE) ParseModule();
                else ParseImport();
            }

            // Emit jump to Entry (placeholder)
            m_Program.Add(OpCode::CALL, (int64_t)0);
            size_t entryCallIdx = m_Program.Instructions.size() - 1;
            m_Program.Add(OpCode::HALT);

            while (m_Current.Type != END) {
                if (m_Current.Type == LET) {
                    ParseGlobal();
                } else if (m_Current.Type == NOEXPORT || m_Current.Type == FUNCTION) {
                    bool shouldExport = true;
                    if (m_Current.Type == NOEXPORT) {
                        shouldExport = false;
                        Advance(); // Consume @noexport
                    }
                    ParseFunction(shouldExport);
                } else if (m_Current.Type == ENTRY) {
                    size_t entryAddr = m_Program.Instructions.size();
                    m_Program.Instructions[entryCallIdx].Operand = (int64_t)entryAddr;
                    m_Program.Exports["@Entry"] = entryAddr;
                    ParseEntry();
                } else if (m_Current.Type == CLASS) {
                    ParseClass();
                } else if (m_Current.Type == ENUM) {
                    ParseEnum();
                } else {
                    Advance();
                }
            }

            // Fill exports from functions (only exported ones)
            for (const auto& [name, addr] : m_Functions) {
                if (m_ExportedFunctions.count(name)) {
                    m_Program.Exports[name] = addr;
                }
            }
            
            // Store class metadata in program
            for (const auto& [className, classInfo] : m_Classes) {
                Program::ClassMetadata metadata;
                metadata.BaseClass = classInfo.BaseClass;
                metadata.ConstructorAddress = classInfo.ConstructorAddress;
                metadata.DestructorAddress = classInfo.DestructorAddress;
                m_Program.ClassInfo[className] = metadata;
            }

            return m_Program;
        }

    private:
        Lexer m_Lexer;
        Token m_Current;
        Program m_Program;

        std::string m_ModuleName;
        std::vector<std::string> m_Imports;

        std::map<std::string, uint8_t> m_Globals;
        std::map<std::string, uint8_t> m_Locals; // Map name to Register Index
        std::map<std::string, size_t> m_Functions;
        std::set<std::string> m_ExportedFunctions; // Functions that should be exported
        
        // Class metadata
        struct ClassInfo {
            std::string BaseClass;
            size_t ConstructorAddress = 0;
            size_t DestructorAddress = 0;
        };
        std::map<std::string, ClassInfo> m_Classes;
        
        uint8_t m_NextReg = 0;
        uint8_t m_NextLocalReg = 10;

        // Loop/switch context for break/continue
        std::vector<size_t> m_BreakTargets;
        std::vector<size_t> m_ContinueTargets;
        std::vector<size_t> m_LoopStarts; // For continue
        std::vector<size_t> m_LoopEnds; // For break

        void Advance() {
            m_Current = m_Lexer.Next();
        }

        void Consume(TokenType type, const std::string& err) {
            if (m_Current.Type == type) Advance();
            else {
                std::string tokenDesc = "END";
                if (m_Current.Type != END) {
                    tokenDesc = m_Current.Text.empty() ?
                        ("token type " + std::to_string((int)m_Current.Type)) :
                        ("'" + m_Current.Text + "'");
                }
                throw std::runtime_error(err + " Got: " + tokenDesc);
            }
        }

        Token PeekNext() {
            return m_Lexer.Peek();
        }

        void ParseModule() {
            Consume(MODULE, "Expected module");
            m_ModuleName = m_Current.Text;
            Consume(ID, "Expected module name");
            Consume(SEMICOLON, "Expected ;");
        }

        void ParseEnum() {
            Consume(ENUM, "Expected enum");
            std::string enumName = m_Current.Text;
            Consume(ID, "Expected enum name");
            Consume(LBRACE, "Expected {");
            Program::EnumMetadata meta;
            int64_t nextDiscriminant = 0;
            bool first = true;
            while (m_Current.Type != RBRACE && m_Current.Type != END) {
                if (m_Current.Type == COMMA && !first) {
                    Advance(); // skip comma
                }
                first = false;
                if (m_Current.Type != ID) break;
                std::string variantName = m_Current.Text;
                Advance();
                if (m_Current.Type == EQ) {
                    Advance();
                    if (m_Current.Type == NUMBER) {
                        nextDiscriminant = m_Current.IntValue;
                        if (m_Current.Text.find('.') != std::string::npos)
                            nextDiscriminant = (int64_t)m_Current.FloatValue;
                        Advance();
                    }
                }
                meta.variantToDiscriminant[variantName] = nextDiscriminant;
                nextDiscriminant++;
            }
            Consume(RBRACE, "Expected }");
            if (m_Current.Type == SEMICOLON) Advance();
            m_Program.EnumInfo[enumName] = meta;
        }

        void ParseImport() {
            Consume(IMPORT, "Expected import");
            std::string importName = m_Current.Text;
            Consume(ID, "Expected import name");
            m_Imports.push_back(importName);
            m_Program.Add(OpCode::IMPORT_MODULE, importName);
            Consume(SEMICOLON, "Expected ;");
        }

        void ParseClass() {
            Consume(CLASS, "Expected class");
            std::string className = m_Current.Text;
            Consume(ID, "Expected class name");

            ClassInfo classInfo;
            if (m_Current.Type == INHERITS) {
                Advance();
                classInfo.BaseClass = m_Current.Text;
                Consume(ID, "Expected base class name");
            }

            Consume(LBRACE, "Expected {");
            
            while (m_Current.Type != RBRACE && m_Current.Type != END) {
                if (m_Current.Type == LET) {
                    ParseGlobal(); // Should probably prefix with className
                } else if (m_Current.Type == FUNCTION) {
                    ParseFunction(); // Should probably prefix with className
                } else if (m_Current.Type == CONSTRUCTOR) {
                    // Parse constructor
                    Advance(); // Consume CONSTRUCTOR
                    std::string constructorName = className + "::constructor";
                    m_Functions[constructorName] = m_Program.Instructions.size();
                    classInfo.ConstructorAddress = m_Program.Instructions.size();
                    
                    Consume(LPAREN, "Expected (");
                    
                    m_Locals.clear();
                    m_NextLocalReg = 10;
                    
                    while (m_Current.Type != RPAREN) {
                        std::string argName = m_Current.Text;
                        Consume(ID, "Expected arg name");
                        m_Locals[argName] = m_NextLocalReg;
                        m_Program.AddReg(OpCode::MOV_REG, m_NextLocalReg);
                        m_NextLocalReg++;
                        
                        if (m_Current.Type == COMMA) Advance();
                    }
                    Consume(RPAREN, "Expected )");
                    Consume(LBRACE, "Expected {");
                    
                    while (m_Current.Type != RBRACE) {
                        ParseStatement();
                    }
                    Consume(RBRACE, "Expected }");
                    
                    m_Program.Add(OpCode::RET);
                } else if (m_Current.Type == DESTRUCTOR) {
                    // Parse destructor
                    Advance(); // Consume DESTRUCTOR
                    std::string destructorName = className + "::destructor";
                    m_Functions[destructorName] = m_Program.Instructions.size();
                    classInfo.DestructorAddress = m_Program.Instructions.size();
                    
                    Consume(LBRACE, "Expected {");
                    
                    m_Locals.clear();
                    m_NextLocalReg = 10;
                    
                    while (m_Current.Type != RBRACE) {
                        ParseStatement();
                    }
                    Consume(RBRACE, "Expected }");
                    
                    m_Program.Add(OpCode::RET);
                } else {
                    Advance();
                }
            }
            Consume(RBRACE, "Expected }");
            
            m_Classes[className] = classInfo;
        }

        void ParseGlobal() {
            Consume(LET, "Expected let");
            std::string name = m_Current.Text;
            Consume(ID, "Expected identifier");

            if (m_Current.Type == COLON) {
                Advance();
                // Optional type hint (including Ptr<T> generics)
                ParseTypeHint();
            }

            Consume(EQ, "Expected =");

            // Parse expression
            ParseExpression();

            // Store in register
            uint8_t reg = m_NextReg++;
            m_Globals[name] = reg;
            m_Program.AddReg(OpCode::MOV_REG, reg);

            if (m_Current.Type == SEMICOLON) Advance();
        }

        void ParseFunction(bool shouldExport = true) {
            Consume(FUNCTION, "Expected function");
            std::string name = m_Current.Text;
            Consume(ID, "Expected function name");

            m_Functions[name] = m_Program.Instructions.size();
            if (shouldExport) {
                m_ExportedFunctions.insert(name);
            }

            Consume(LPAREN, "Expected (");

            // Args
            m_Locals.clear();
            // For simplicity, reuse registers R10+ for locals.
            m_NextLocalReg = 10;
            size_t arity = 0;

            while (m_Current.Type != RPAREN) {
                std::string argName = m_Current.Text;
                Consume(ID, "Expected arg name");

                // Optional type hint for parameter, including Ptr<T>
                if (m_Current.Type == COLON) {
                    Advance();
                    ParseTypeHint();
                }
                m_Locals[argName] = m_NextLocalReg;
                // Pop arg from stack to register
                m_Program.AddReg(OpCode::MOV_REG, m_NextLocalReg);
                m_NextLocalReg++;
                arity++;

                if (m_Current.Type == COMMA) Advance();
            }
            Consume(RPAREN, "Expected )");

            m_Program.FunctionArity[m_Functions[name]] = arity;
            
            // Optional return type
            std::string returnType = "";
            if (m_Current.Type == ARROW) {
                Advance();
                returnType = m_Current.Text;
                Consume(ID, "Expected return type after ->");
            }
            
            Consume(LBRACE, "Expected {");

            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");

            // Implicit return if not present?
            m_Program.Add(OpCode::RET);
        }

        void ParseEntry() {
            Consume(ENTRY, "Expected @Entry");
            Consume(LBRACE, "Expected {");

            m_Locals.clear(); // Clear locals for Entry
            m_NextLocalReg = 10; // Start locals at R10

            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");
            m_Program.Add(OpCode::RET);
        }

        void ParseStatement() {
            if (m_Current.Type == LET) {
                ParseLocal();
            } else if (m_Current.Type == RETURN) {
                Advance();
                ParseExpression();
                Consume(SEMICOLON, "Expected ;");
                m_Program.Add(OpCode::RET);
            } else if (m_Current.Type == DELETE_KW) {
                // delete expr;
                Advance();
                ParseExpression();
                Consume(SEMICOLON, "Expected ;");
                m_Program.Add(OpCode::DELETE_VALUE);
            } else if (m_Current.Type == IF) {
                ParseIf();
            } else if (m_Current.Type == WHILE) {
                ParseWhile();
            } else if (m_Current.Type == FOR) {
                ParseFor();
            } else if (m_Current.Type == MATCH) {
                ParseMatch();
            } else if (m_Current.Type == BREAK) {
                Advance();
                Consume(SEMICOLON, "Expected ;");
                // Break will be handled by loop/switch context
                m_Program.Add(OpCode::JMP, 0); // Placeholder, will be filled by loop/switch
                m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
            } else if (m_Current.Type == CONTINUE) {
                Advance();
                Consume(SEMICOLON, "Expected ;");
                // Continue will be handled by loop context
                m_Program.Add(OpCode::JMP, 0); // Placeholder, will be filled by loop
                m_ContinueTargets.push_back(m_Program.Instructions.size() - 1);
            } else if (m_Current.Type == ID && (PeekNext().Type == EQ || PeekNext().Type == PLUS_EQ ||
                                                 PeekNext().Type == MINUS_EQ || PeekNext().Type == MUL_EQ ||
                                                 PeekNext().Type == DIV_EQ)) {
                // Assignment statement (including compound assignments)
                std::string name = m_Current.Text;
                Advance(); // Eat ID
                TokenType assignOp = m_Current.Type;
                Advance(); // Eat assignment operator

                if (assignOp == EQ) {
                    // Simple assignment
                    ParseExpression();
                    StoreVariable(name);
                } else {
                    // Compound assignment: x += y -> x = x + y
                    LoadVariable(name);
                    ParseExpression();
                    if (assignOp == PLUS_EQ) m_Program.Add(OpCode::ADD);
                    else if (assignOp == MINUS_EQ) m_Program.Add(OpCode::SUB);
                    else if (assignOp == MUL_EQ) m_Program.Add(OpCode::MUL);
                    else if (assignOp == DIV_EQ) m_Program.Add(OpCode::DIV);
                    StoreVariable(name);
                }
                Consume(SEMICOLON, "Expected ;");
            } else {
                ParseExpression();
                Consume(SEMICOLON, "Expected ;");
                m_Program.Add(OpCode::POP);
            }
        }

        void ParseIf() {
            Consume(IF, "Expected if");
            Consume(LPAREN, "Expected (");
            ParseExpression(); // Condition
            Consume(RPAREN, "Expected )");

            // JMP_IF to true block? No, usually JMP_FALSE to else/end.
            // But we have JMP_IF (true).
            // So:
            //   JMP_IF TrueLabel
            //   JMP FalseLabel
            // TrueLabel:
            //   Block
            //   JMP EndLabel
            // FalseLabel:
            //   (ElseBlock)
            // EndLabel:

            // Alternative: JMP_IF to SkipFalse. But JMP_IF jumps if TRUE.
            // We want to Jump if operands are NOT met?
            // Let's implement NOT logic or use JMP_IF correctly.
            // If we have JMP_IF_FALSE it is easier to jump over the block.
            // Since we only have JMP_IF:
            //   OpCode::NOT (Not implemented yet? Need to add or emulate)
            //   Or:
            //   JMP_IF BlockStart
            //   JMP BlockEnd
            // BlockStart:
            //   ...
            // BlockEnd:

            // Emulating Not: EQ(0) since 0 is false?
            // Let's just emit JMP_IF then JMP.

            m_Program.Add(OpCode::JMP_IF, 0); // To if-body
            size_t jumpToBodyIdx = m_Program.Instructions.size() - 1;

            m_Program.Add(OpCode::JMP, 0); // To else/end
            size_t jumpToElseIdx = m_Program.Instructions.size() - 1;

            // If Body
            size_t bodyStart = m_Program.Instructions.size();
            m_Program.Instructions[jumpToBodyIdx].Operand = (int64_t)bodyStart;

            Consume(LBRACE, "Expected {");
            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");

            // Jump over else
            m_Program.Add(OpCode::JMP, 0);
            size_t jumpOverElseIdx = m_Program.Instructions.size() - 1;

            // Else Start
            size_t elseStart = m_Program.Instructions.size();
            m_Program.Instructions[jumpToElseIdx].Operand = (int64_t)elseStart;

            if (m_Current.Type == ELSE) {
                Advance();
                Consume(LBRACE, "Expected {");
                while (m_Current.Type != RBRACE) {
                    ParseStatement();
                }
                Consume(RBRACE, "Expected }");
            }

            // End
            size_t endAddr = m_Program.Instructions.size();
            m_Program.Instructions[jumpOverElseIdx].Operand = (int64_t)endAddr;
        }

        void ParseWhile() {
            Consume(WHILE, "Expected while");
            size_t loopStart = m_Program.Instructions.size();
            m_LoopStarts.push_back(loopStart);

            Consume(LPAREN, "Expected (");
            ParseExpression(); // Condition
            Consume(RPAREN, "Expected )");

            m_Program.Add(OpCode::JMP_IF, 0); // To body
            size_t jumpToBodyIdx = m_Program.Instructions.size() - 1;

            m_Program.Add(OpCode::JMP, 0); // Exit loop
            size_t jumpOverBodyIdx = m_Program.Instructions.size() - 1;

            // Body
            size_t bodyStart = m_Program.Instructions.size();
            m_Program.Instructions[jumpToBodyIdx].Operand = (int64_t)bodyStart;

            Consume(LBRACE, "Expected {");
            size_t breakTargetStart = m_BreakTargets.size();
            size_t continueTargetStart = m_ContinueTargets.size();
            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");

            // Fix continue targets (jump back to loop start)
            for (size_t i = continueTargetStart; i < m_ContinueTargets.size(); ++i) {
                m_Program.Instructions[m_ContinueTargets[i]].Operand = (int64_t)loopStart;
            }
            m_ContinueTargets.erase(m_ContinueTargets.begin() + continueTargetStart, m_ContinueTargets.end());

            // Jump back to condition
            m_Program.Add(OpCode::JMP, (int64_t)loopStart);

            // Loop End
            size_t loopEnd = m_Program.Instructions.size();
            m_Program.Instructions[jumpOverBodyIdx].Operand = (int64_t)loopEnd;

            // Fix break targets (jump to loop end)
            for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                m_Program.Instructions[m_BreakTargets[i]].Operand = (int64_t)loopEnd;
            }
            m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());

            m_LoopStarts.pop_back();
        }

        void ParseFor() {
            Consume(FOR, "Expected for");
            Consume(LPAREN, "Expected (");

            // Check if it's iterator-based: for i in Array/Range
            if (m_Current.Type == ID) {
                std::string iterVar = m_Current.Text;
                Advance();
                if (m_Current.Type == IN) {
                    // Iterator-based for loop: for i in Array { ... }
                    Advance();
                    ParseExpression(); // Array or Range expression
                    Consume(RPAREN, "Expected )");

                    // Store iterator variable
                    uint8_t iterReg = m_NextLocalReg++;
                    m_Locals[iterVar] = iterReg;

                    // Store array on stack temporarily, then get length
                    m_Program.Add(OpCode::DUP); // Duplicate array
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                    m_Program.Add(OpCode::CALL, std::string("Array.Length"));

                    size_t loopStart = m_Program.Instructions.size();
                    m_LoopStarts.push_back(loopStart);

                    // Initialize index to 0
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                    m_Program.AddReg(OpCode::MOV_REG, iterReg);

                    // Check if index < length
                    m_Program.AddReg(OpCode::LOAD_REG, iterReg);
                    m_Program.Add(OpCode::DUP); // Duplicate array for access
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                    m_Program.Add(OpCode::CALL, std::string("Array.Length"));
                    m_Program.Add(OpCode::LT);

                    m_Program.Add(OpCode::JMP_IF, 0);
                    size_t jumpToBodyIdx = m_Program.Instructions.size() - 1;

                    m_Program.Add(OpCode::JMP, 0);
                    size_t jumpOverBodyIdx = m_Program.Instructions.size() - 1;

                    // Body
                    size_t bodyStart = m_Program.Instructions.size();
                    m_Program.Instructions[jumpToBodyIdx].Operand = (int64_t)bodyStart;

                    // Get current element: array[index]
                    m_Program.Add(OpCode::DUP); // Array
                    m_Program.AddReg(OpCode::LOAD_REG, iterReg);
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                    m_Program.Add(OpCode::CALL, std::string("Array.Get"));
                    // Element is now on stack, can be used in body

                    Consume(LBRACE, "Expected {");
                    size_t breakTargetStart = m_BreakTargets.size();
                    size_t continueTargetStart = m_ContinueTargets.size();
                    while (m_Current.Type != RBRACE) {
                        ParseStatement();
                    }
                    Consume(RBRACE, "Expected }");

                    // Fix continue targets
                    for (size_t i = continueTargetStart; i < m_ContinueTargets.size(); ++i) {
                        m_Program.Instructions[m_ContinueTargets[i]].Operand = (int64_t)loopStart;
                    }
                    m_ContinueTargets.erase(m_ContinueTargets.begin() + continueTargetStart, m_ContinueTargets.end());

                    // Increment index
                    m_Program.AddReg(OpCode::LOAD_REG, iterReg);
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                    m_Program.Add(OpCode::ADD);
                    m_Program.AddReg(OpCode::MOV_REG, iterReg);

                    // Jump back to condition
                    m_Program.Add(OpCode::JMP, (int64_t)loopStart);

                    // Loop end
                    size_t loopEnd = m_Program.Instructions.size();
                    m_Program.Instructions[jumpOverBodyIdx].Operand = (int64_t)loopEnd;

                    // Fix break targets
                    for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                        m_Program.Instructions[m_BreakTargets[i]].Operand = (int64_t)loopEnd;
                    }
                    m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());

                    // Pop array from stack
                    m_Program.Add(OpCode::POP);

                    m_LoopStarts.pop_back();
                    m_Locals.erase(iterVar);
                    return;
                }
            }

            // Traditional C-style for loop: for (init; condition; increment) { ... }
            // Parse initialization (optional)
            if (m_Current.Type != SEMICOLON) {
                if (m_Current.Type == LET) {
                    ParseLocal();
                } else {
                    ParseExpression();
                    m_Program.Add(OpCode::POP);
                }
            }
            Consume(SEMICOLON, "Expected ;");

            size_t loopStart = m_Program.Instructions.size();
            m_LoopStarts.push_back(loopStart);

            // Condition (optional)
            size_t conditionStart = m_Program.Instructions.size();
            if (m_Current.Type != SEMICOLON) {
                ParseExpression();
            } else {
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)1); // Always true
            }
            Consume(SEMICOLON, "Expected ;");

            m_Program.Add(OpCode::JMP_IF, 0);
            size_t jumpToBodyIdx = m_Program.Instructions.size() - 1;

            m_Program.Add(OpCode::JMP, 0);
            size_t jumpOverBodyIdx = m_Program.Instructions.size() - 1;

            // Increment (optional) - store for later
            size_t incrementStart = m_Program.Instructions.size();
            if (m_Current.Type != RPAREN) {
                ParseExpression();
                m_Program.Add(OpCode::POP);
            }
            Consume(RPAREN, "Expected )");

            // Body
            size_t bodyStart = m_Program.Instructions.size();
            m_Program.Instructions[jumpToBodyIdx].Operand = (int64_t)bodyStart;

            Consume(LBRACE, "Expected {");
            size_t breakTargetStart = m_BreakTargets.size();
            size_t continueTargetStart = m_ContinueTargets.size();
            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");

            // Fix continue targets (jump to increment)
            for (size_t i = continueTargetStart; i < m_ContinueTargets.size(); ++i) {
                m_Program.Instructions[m_ContinueTargets[i]].Operand = (int64_t)incrementStart;
            }
            m_ContinueTargets.erase(m_ContinueTargets.begin() + continueTargetStart, m_ContinueTargets.end());

            // Jump to increment, then back to condition
            m_Program.Add(OpCode::JMP, (int64_t)incrementStart);

            // Loop end
            size_t loopEnd = m_Program.Instructions.size();
            m_Program.Instructions[jumpOverBodyIdx].Operand = (int64_t)loopEnd;

            // Fix break targets
            for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                m_Program.Instructions[m_BreakTargets[i]].Operand = (int64_t)loopEnd;
            }
            m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());

            m_LoopStarts.pop_back();
        }

        void ParseMatch() {
            Consume(MATCH, "Expected match");
            ParseExpression(); // match value on stack
            Consume(LBRACE, "Expected {");

            size_t matchEnd = 0; // will patch after all arms
            size_t breakTargetStart = m_BreakTargets.size();

            while (m_Current.Type != RBRACE && m_Current.Type != END) {
                // Pattern: literal (NUMBER, STRING), ID (binding), or _ (wildcard)
                if (m_Current.Type == NUMBER) {
                    // Literal number: DUP, push literal, EQ, JMP_IF body, POP, JMP next
                    m_Program.Add(OpCode::DUP);
                    if (m_Current.Text.find('.') != std::string::npos) {
                        m_Program.Add(OpCode::PUSH_CONST, m_Current.FloatValue);
                    } else {
                        m_Program.Add(OpCode::PUSH_CONST, m_Current.IntValue);
                    }
                    Advance();
                    m_Program.Add(OpCode::EQ);
                    m_Program.Add(OpCode::JMP_IF, 0);
                    size_t jmpToBodyIdx = m_Program.Instructions.size() - 1;
                    m_Program.Add(OpCode::POP); // pop comparison result (0)
                    m_Program.Add(OpCode::JMP, 0);
                    size_t jmpToNextIdx = m_Program.Instructions.size() - 1;

                    Consume(ARROW, "Expected =>");
                    size_t bodyStart = m_Program.Instructions.size();
                    m_Program.Instructions[jmpToBodyIdx].Operand = (int64_t)bodyStart;

                    ParseMatchArmBody();
                    m_Program.Add(OpCode::JMP, 0);
                    m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
                    size_t nextArm = m_Program.Instructions.size();
                    m_Program.Instructions[jmpToNextIdx].Operand = (int64_t)nextArm;
                } else if (m_Current.Type == STRING) {
                    m_Program.Add(OpCode::DUP);
                    m_Program.Add(OpCode::PUSH_CONST, m_Current.Text);
                    Advance();
                    m_Program.Add(OpCode::EQ);
                    m_Program.Add(OpCode::JMP_IF, 0);
                    size_t jmpToBodyIdx = m_Program.Instructions.size() - 1;
                    m_Program.Add(OpCode::POP);
                    m_Program.Add(OpCode::JMP, 0);
                    size_t jmpToNextIdx = m_Program.Instructions.size() - 1;

                    Consume(ARROW, "Expected =>");
                    size_t bodyStart = m_Program.Instructions.size();
                    m_Program.Instructions[jmpToBodyIdx].Operand = (int64_t)bodyStart;

                    ParseMatchArmBody();
                    m_Program.Add(OpCode::JMP, 0);
                    m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
                    size_t nextArm = m_Program.Instructions.size();
                    m_Program.Instructions[jmpToNextIdx].Operand = (int64_t)nextArm;
                } else if (m_Current.Type == ID) {
                    std::string id = m_Current.Text;
                    Advance();
                    if (id == "_") {
                        // Wildcard: pop value, run body
                        m_Program.Add(OpCode::POP);
                        Consume(ARROW, "Expected =>");
                        ParseMatchArmBody();
                        m_Program.Add(OpCode::JMP, 0);
                        m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
                    } else if (m_Current.Type == DOT) {
                        // Enum variant pattern: EnumName.Variant
                        Advance(); // consume DOT
                        std::string variantName = m_Current.Text;
                        Consume(ID, "Expected variant name");
                        if (!m_Program.EnumInfo.count(id)) {
                            throw std::runtime_error("Unknown enum: " + id);
                        }
                        const auto& meta = m_Program.EnumInfo.at(id);
                        int64_t disc = 0;
                        if (meta.variantToDiscriminant.count(variantName))
                            disc = meta.variantToDiscriminant.at(variantName);
                        EnumVal ev;
                        ev.enumName = id;
                        ev.variant = variantName;
                        ev.discriminant = disc;
                        m_Program.Add(OpCode::DUP);
                        m_Program.Add(OpCode::PUSH_CONST, ev);
                        m_Program.Add(OpCode::EQ);
                        m_Program.Add(OpCode::JMP_IF, 0);
                        size_t jmpToBodyIdx = m_Program.Instructions.size() - 1;
                        m_Program.Add(OpCode::POP);
                        m_Program.Add(OpCode::JMP, 0);
                        size_t jmpToNextIdx = m_Program.Instructions.size() - 1;

                        Consume(ARROW, "Expected =>");
                        size_t bodyStart = m_Program.Instructions.size();
                        m_Program.Instructions[jmpToBodyIdx].Operand = (int64_t)bodyStart;

                        ParseMatchArmBody();
                        m_Program.Add(OpCode::JMP, 0);
                        m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
                        size_t nextArm = m_Program.Instructions.size();
                        m_Program.Instructions[jmpToNextIdx].Operand = (int64_t)nextArm;
                    } else {
                        // Binding: store top in local, run body
                        uint8_t reg = m_NextLocalReg++;
                        m_Locals[id] = reg;
                        m_Program.AddReg(OpCode::MOV_REG, reg);
                        Consume(ARROW, "Expected =>");
                        ParseMatchArmBody();
                        m_Program.Add(OpCode::JMP, 0);
                        m_BreakTargets.push_back(m_Program.Instructions.size() - 1);
                    }
                } else {
                    Advance();
                }
                if (m_Current.Type == COMMA) Advance();
            }

            Consume(RBRACE, "Expected }");
            matchEnd = m_Program.Instructions.size();
            m_Program.Add(OpCode::POP); // pop the match value

            for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                auto& operand = m_Program.Instructions[m_BreakTargets[i]].Operand;
                if (std::holds_alternative<int64_t>(operand) && std::get<int64_t>(operand) == 0) {
                    operand = (int64_t)matchEnd;
                }
            }
            m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());
        }

        void ParseMatchArmBody() {
            if (m_Current.Type == LBRACE) {
                Advance();
                while (m_Current.Type != RBRACE && m_Current.Type != END) {
                    ParseStatement();
                }
                Consume(RBRACE, "Expected }");
            } else {
                ParseExpression();
                if (m_Current.Type == SEMICOLON) Advance();
                m_Program.Add(OpCode::POP);
            }
        }

        void ParseLambdaExpression() {
            Consume(LAMBDA, "Expected lambda");
            Consume(LPAREN, "Expected (");
            std::vector<std::string> params;
            if (m_Current.Type == ID) {
                params.push_back(m_Current.Text);
                Advance();
                while (m_Current.Type == COMMA) {
                    Advance();
                    params.push_back(m_Current.Text);
                    Consume(ID, "Expected parameter name");
                }
            }
            Consume(RPAREN, "Expected )");
            Consume(ARROW, "Expected =>");

            std::map<std::string, uint8_t> savedLocals = m_Locals;
            uint8_t savedNextLocal = m_NextLocalReg;
            m_Locals.clear();
            m_NextLocalReg = 10;

            size_t startIP = m_Program.Instructions.size();
            for (const auto& p : params) {
                m_Locals[p] = m_NextLocalReg;
                m_Program.AddReg(OpCode::MOV_REG, m_NextLocalReg);
                m_NextLocalReg++;
            }

            if (m_Current.Type == LBRACE) {
                Advance();
                while (m_Current.Type != RBRACE && m_Current.Type != END) {
                    ParseStatement();
                }
                Consume(RBRACE, "Expected }");
            } else {
                ParseExpression();
                if (m_Current.Type == SEMICOLON) Advance();
            }
            m_Program.Add(OpCode::RET);

            m_Locals = savedLocals;
            m_NextLocalReg = savedNextLocal;

            ScriptFunc sf;
            sf.entryIP = startIP;
            sf.capture = nullptr;
            m_Program.Add(OpCode::PUSH_CONST, sf);
        }

        void ParseExpression() {
            ParseComparison();
        }

        void ParseComparison() {
            ParseLogicalOr();

            while (m_Current.Type == EQ_EQ || m_Current.Type == NEQ || m_Current.Type == LT ||
                   m_Current.Type == GT || m_Current.Type == LE_OP || m_Current.Type == GE_OP) {
                TokenType op = m_Current.Type;
                Advance();
                ParseLogicalOr();
                if (op == EQ_EQ) m_Program.Add(OpCode::EQ);
                else if (op == NEQ) m_Program.Add(OpCode::NEQ);
                else if (op == LT) m_Program.Add(OpCode::LT);
                else if (op == GT) m_Program.Add(OpCode::GT);
                else if (op == LE_OP) m_Program.Add(OpCode::LE);
                else if (op == GE_OP) m_Program.Add(OpCode::GE);
            }
        }

        void ParseLogicalOr() {
            ParseLogicalAnd();
            while (m_Current.Type == OR_LOGIC) {
                Advance();
                ParseLogicalAnd();
                // Emit logical OR: a || b = (a != 0) ? 1 : ((b != 0) ? 1 : 0)
                // Simplified: push 1 if either is non-zero
                m_Program.Add(OpCode::DUP);
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                m_Program.Add(OpCode::NEQ);
                m_Program.Add(OpCode::JMP_IF, 0);
                size_t skipIdx = m_Program.Instructions.size() - 1;
                m_Program.Add(OpCode::POP); // Remove first value
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                size_t endIdx = m_Program.Instructions.size();
                m_Program.Instructions[skipIdx].Operand = (int64_t)endIdx;
            }
        }

        void ParseLogicalAnd() {
            ParseBitwiseOr();
            while (m_Current.Type == AND_LOGIC) {
                Advance();
                ParseBitwiseOr();
                // Emit logical AND: a && b = (a != 0 && b != 0) ? 1 : 0
                m_Program.Add(OpCode::DUP);
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                m_Program.Add(OpCode::EQ);
                m_Program.Add(OpCode::JMP_IF, 0);
                size_t skipIdx = m_Program.Instructions.size() - 1;
                m_Program.Add(OpCode::POP);
                m_Program.Add(OpCode::POP);
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                size_t endIdx = m_Program.Instructions.size();
                m_Program.Instructions[skipIdx].Operand = (int64_t)endIdx;
            }
        }

        void ParseBitwiseOr() {
            ParseBitwiseXor();
            while (m_Current.Type == OR_BIT) {
                Advance();
                ParseBitwiseXor();
                m_Program.Add(OpCode::OR);
            }
        }

        void ParseBitwiseXor() {
            ParseBitwiseAnd();
            while (m_Current.Type == XOR_BIT) {
                Advance();
                ParseBitwiseAnd();
                m_Program.Add(OpCode::XOR);
            }
        }

        void ParseBitwiseAnd() {
            ParseShift();
            while (m_Current.Type == AND_BIT) {
                Advance();
                ParseShift();
                m_Program.Add(OpCode::AND);
            }
        }

        void ParseShift() {
            ParseAdditive();
            while (m_Current.Type == SHL_OP || m_Current.Type == SHR_OP) {
                TokenType op = m_Current.Type;
                Advance();
                ParseAdditive();
                if (op == SHL_OP) m_Program.Add(OpCode::SHL);
                else if (op == SHR_OP) m_Program.Add(OpCode::SHR);
            }
        }

        void ParseAdditive() {
            ParseTerm();
            while (m_Current.Type == PLUS || m_Current.Type == MINUS) {
                TokenType op = m_Current.Type;
                Advance();
                ParseTerm();
                if (op == PLUS) m_Program.Add(OpCode::ADD);
                else if (op == MINUS) m_Program.Add(OpCode::SUB);
            }
        }

        void ParseTerm() {
            // Handle unary operators
            if (m_Current.Type == AND_BIT) {
                // Unary &: function reference
                Advance();
                if (m_Current.Type != ID) throw std::runtime_error("Expected function name after &");
                std::string name = m_Current.Text;
                Advance();
                if (!m_Functions.count(name)) throw std::runtime_error("Unknown function: " + name);
                ScriptFunc sf;
                sf.entryIP = m_Functions[name];
                sf.capture = nullptr;
                m_Program.Add(OpCode::PUSH_CONST, sf);
            } else if (m_Current.Type == NOT_LOGIC) {
                Advance();
                ParseTerm();
                // Logical NOT: !a = (a == 0) ? 1 : 0
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                m_Program.Add(OpCode::EQ);
            } else if (m_Current.Type == NOT_BIT) {
                Advance();
                ParseTerm();
                m_Program.Add(OpCode::NOT);
            } else if (m_Current.Type == MINUS) {
                Advance();
                ParseTerm();
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)0);
                m_Program.Add(OpCode::SUB); // 0 - x = -x
            } else if (m_Current.Type == PLUSPLUS) {
                // Prefix increment
                Advance();
                if (m_Current.Type != ID) throw std::runtime_error("Expected identifier after ++");
                std::string name = m_Current.Text;
                Advance();
                LoadVariable(name);
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                m_Program.Add(OpCode::ADD);
                StoreVariable(name);
                LoadVariable(name);
            } else if (m_Current.Type == MINUSMINUS) {
                // Prefix decrement
                Advance();
                if (m_Current.Type != ID) throw std::runtime_error("Expected identifier after --");
                std::string name = m_Current.Text;
                Advance();
                LoadVariable(name);
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                m_Program.Add(OpCode::SUB);
                StoreVariable(name);
                LoadVariable(name);
            } else if (m_Current.Type == NUMBER) {
                // Check if it's a float by looking for decimal point in text
                if (m_Current.Text.find('.') != std::string::npos) {
                    m_Program.Add(OpCode::PUSH_CONST, m_Current.FloatValue);
                } else {
                    m_Program.Add(OpCode::PUSH_CONST, m_Current.IntValue);
                }
                Advance();
            } else if (m_Current.Type == STRING) {
                m_Program.Add(OpCode::PUSH_CONST, m_Current.Text);
                Advance();
            } else if (m_Current.Type == LBRACKET) {
                // Array literal: [1, 2, 3]
                Advance();
                // Create array using Array.New with initial values
                int argCount = 0;
                if (m_Current.Type != RBRACKET) {
                    ParseExpression();
                    argCount++;
                    while (m_Current.Type == COMMA) {
                        Advance();
                        ParseExpression();
                        argCount++;
                    }
                }
                Consume(RBRACKET, "Expected ]");
                // Call Array.New with the values on stack
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                m_Program.Add(OpCode::CALL, std::string("Array.New"));
            } else if (m_Current.Type == LBRACE) {
                Token peek = PeekNext();
                if (peek.Type == STRING || peek.Type == ID) {
                    // Dictionary literal: {"key": value, "key2": value2}
                    Advance();
                    m_Program.Add(OpCode::CALL, std::string("Dictionary.New"));
                    int pairCount = 0;
                    if (m_Current.Type != RBRACE) {
                        // Parse key-value pairs
                        do {
                            if (m_Current.Type == STRING || m_Current.Type == ID) {
                                std::string key = m_Current.Text;
                                Advance();
                                Consume(COLON, "Expected : after dictionary key");
                                ParseExpression();
                                // Call Dictionary.Set: dict, key, value
                                m_Program.Add(OpCode::DUP); // Duplicate dict
                                m_Program.Add(OpCode::PUSH_CONST, key);
                                m_Program.Add(OpCode::PUSH_CONST, (int64_t)2);
                                m_Program.Add(OpCode::CALL, std::string("Dictionary.Set"));
                                pairCount++;
                            }
                            if (m_Current.Type == COMMA) Advance();
                        } while (m_Current.Type == COMMA);
                    }
                    Consume(RBRACE, "Expected }");
                } else {
                    // Not a dictionary literal, treat as block (for statements)
                    throw std::runtime_error("Unexpected { in expression context");
                }
            } else if (m_Current.Type == LAMBDA) {
                ParseLambdaExpression();
            } else if (m_Current.Type == NEW) {
                Advance();
                std::string className = m_Current.Text;
                Consume(ID, "Expected class name after new");
                Consume(LPAREN, "Expected (");
                int argCount = 0;
                if (m_Current.Type != RPAREN) {
                    ParseExpression();
                    argCount++;
                    while (m_Current.Type == COMMA) {
                        Advance();
                        ParseExpression();
                        argCount++;
                    }
                }
                Consume(RPAREN, "Expected )");
                
                // Create object
                // Stack before: arg1, arg2, ..., argCount
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                m_Program.Add(OpCode::NEW_OBJ, className);
                // Stack after NEW_OBJ: object, arg1, arg2, ... (args pushed back by NEW_OBJ)
                
                // Call constructors in inheritance order (base first, then derived)
                std::vector<std::string> constructorChain;
                std::string currentClass = className;
                while (!currentClass.empty() && m_Classes.find(currentClass) != m_Classes.end()) {
                    if (m_Classes[currentClass].ConstructorAddress != 0) {
                        constructorChain.insert(constructorChain.begin(), currentClass); // Insert at beginning for base-first order
                    }
                    currentClass = m_Classes[currentClass].BaseClass;
                }
                
                // Call constructors in order (base first)
                for (const auto& cls : constructorChain) {
                    std::string constructorName = cls + "::constructor";
                    if (m_Functions.find(constructorName) != m_Functions.end()) {
                        // Duplicate object for each constructor call
                        m_Program.Add(OpCode::DUP);
                        // Push argCount
                        m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                        // Call constructor
                        m_Program.Add(OpCode::CALL, (int64_t)m_Functions[constructorName]);
                        // Constructor pops object and args, but we need object back
                        // For now, this is simplified - full implementation would need better stack management
                    }
                }
            } else if (m_Current.Type == ID) {
                std::string name = m_Current.Text;
                Advance();

                // Check for Module.Function() syntax: ID.DOT.ID.LPAREN
                if (m_Current.Type == DOT) {
                    // Peek to see if we have ID.LPAREN after the DOT
                    Token afterDot = m_Lexer.Peek(); // Should be ID
                    if (afterDot.Type == ID) {
                        // Peek one more to see if LPAREN follows
                        // We need to peek past the ID token
                        // Use a simple approach: advance and check
                        Advance(); // DOT
                        std::string funcName = m_Current.Text;
                        if (m_Current.Type == ID) {
                            Advance(); // ID (function name)
                            if (m_Current.Type == LPAREN) {
                                // Qualified call: Module.Function() or built-ins like Ptr.New(...)
                                Advance(); // LPAREN

                                // Args
                                int argCount = 0;
                                if (m_Current.Type != RPAREN) {
                                    ParseExpression();
                                    argCount++;
                                    while (m_Current.Type == COMMA) {
                                        Advance();
                                        ParseExpression();
                                        argCount++;
                                    }
                                }
                                Consume(RPAREN, "Expected )");

                                if (name == "Ptr") {
                                    // Pointer built-ins lowered directly to opcodes
                                    if (funcName == "New") {
                                        if (argCount != 1) {
                                            throw std::runtime_error("Ptr.New expects exactly 1 argument");
                                        }
                                        m_Program.Add(OpCode::PTR_NEW);
                                    } else if (funcName == "IsValid") {
                                        if (argCount != 1) {
                                            throw std::runtime_error("Ptr.IsValid expects exactly 1 argument");
                                        }
                                        m_Program.Add(OpCode::PTR_IS_VALID);
                                    } else if (funcName == "Get") {
                                        if (argCount != 1) {
                                            throw std::runtime_error("Ptr.Get expects exactly 1 argument");
                                        }
                                        m_Program.Add(OpCode::PTR_GET);
                                    } else if (funcName == "Reset") {
                                        if (argCount != 1) {
                                            throw std::runtime_error("Ptr.Reset expects exactly 1 argument");
                                        }
                                        m_Program.Add(OpCode::PTR_RESET);
                                    } else {
                                        // Fallback to regular qualified call
                                        std::string qualifiedName = name + "." + funcName;
                                        m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                                        m_Program.Add(OpCode::CALL, qualifiedName);
                                    }
                                } else {
                                    // Qualified calls are treated as module calls (native or module function)
                                    std::string qualifiedName = name + "." + funcName;
                                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                                    m_Program.Add(OpCode::CALL, qualifiedName);
                                }
                                // Skip chaining since we've handled this as a function call
                            } else {
                                // Not LPAREN: enum variant (EnumName.Variant) or attribute access
                                if (m_Program.EnumInfo.count(name)) {
                                    const auto& meta = m_Program.EnumInfo.at(name);
                                    int64_t disc = 0;
                                    if (meta.variantToDiscriminant.count(funcName))
                                        disc = meta.variantToDiscriminant.at(funcName);
                                    EnumVal ev;
                                    ev.enumName = name;
                                    ev.variant = funcName;
                                    ev.discriminant = disc;
                                    m_Program.Add(OpCode::PUSH_CONST, ev);
                                } else {
                                    // Attribute access
                                    LoadVariable(name);
                                    m_Program.Add(OpCode::GET_ATTR, funcName);
                                }
                                // Continue with any remaining chaining
                                while (m_Current.Type == DOT || m_Current.Type == LBRACKET) {
                                    if (m_Current.Type == DOT) {
                                        Advance();
                                        std::string attr = m_Current.Text;
                                        Consume(ID, "Expected attribute name");
                                        m_Program.Add(OpCode::GET_ATTR, attr);
                                    } else if (m_Current.Type == LBRACKET) {
                                        Advance();
                                        ParseExpression();
                                        Consume(RBRACKET, "Expected ]");
                                        m_Program.Add(OpCode::ARRAY_GET);
                                    }
                                }
                            }
                        } else {
                            // Not ID after DOT, unexpected
                            LoadVariable(name);
                        }
                    } else {
                        // Not ID after DOT, treat as normal variable access
                        LoadVariable(name);
                        // Chaining (dots and brackets)
                        while (m_Current.Type == DOT || m_Current.Type == LBRACKET) {
                            if (m_Current.Type == DOT) {
                                Advance();
                                std::string attr = m_Current.Text;
                                Consume(ID, "Expected attribute name");
                                m_Program.Add(OpCode::GET_ATTR, attr);
                            } else if (m_Current.Type == LBRACKET) {
                                Advance();
                                ParseExpression();
                                Consume(RBRACKET, "Expected ]");
                                m_Program.Add(OpCode::ARRAY_GET);
                            }
                        }
                    }
                } else if (m_Current.Type == LPAREN) {
                    // Function Call
                    Advance();
                    // Args
                    int argCount = 0;
                    if (m_Current.Type != RPAREN) {
                        ParseExpression();
                        argCount++;
                        while (m_Current.Type == COMMA) {
                            Advance();
                            ParseExpression();
                            argCount++;
                        }
                    }
                    Consume(RPAREN, "Expected )");

                    if (m_Functions.count(name)) {
                        m_Program.Add(OpCode::CALL, (int64_t)m_Functions[name]);
                    } else if (m_Globals.count(name) || m_Locals.count(name)) {
                        // First-class call: variable holds function value
                        m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                        LoadVariable(name);
                        m_Program.Add(OpCode::CALL_VALUE);
                    } else {
                        // Native: Push arg count
                        m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                        m_Program.Add(OpCode::CALL, name);
                    }
                } else {
                    LoadVariable(name);
                    // Chaining (dots and brackets)
                    while (m_Current.Type == DOT || m_Current.Type == LBRACKET) {
                        if (m_Current.Type == DOT) {
                            Advance();
                            std::string attr = m_Current.Text;
                            Consume(ID, "Expected attribute name");
                            m_Program.Add(OpCode::GET_ATTR, attr);
                        } else if (m_Current.Type == LBRACKET) {
                            Advance();
                            ParseExpression();
                            Consume(RBRACKET, "Expected ]");
                            m_Program.Add(OpCode::ARRAY_GET);
                        }
                    }
                }

                if (m_Current.Type == PLUSPLUS) {
                    // Post-increment
                    Advance();
                    m_Program.Add(OpCode::DUP);
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                    m_Program.Add(OpCode::ADD);
                    StoreVariable(name);
                }
            }
        }

        void LoadVariable(const std::string& name) {
            if (m_Locals.count(name)) {
                m_Program.AddReg(OpCode::LOAD_REG, m_Locals[name]);
            } else if (m_Globals.count(name)) {
                m_Program.AddReg(OpCode::LOAD_REG, m_Globals[name]);
            } else {
                throw std::runtime_error("Unknown variable: " + name);
            }
        }

        void StoreVariable(const std::string& name) {
            if (m_Locals.count(name)) {
                m_Program.AddReg(OpCode::MOV_REG, m_Locals[name]);
            } else if (m_Globals.count(name)) {
                m_Program.AddReg(OpCode::MOV_REG, m_Globals[name]);
            } else {
                throw std::runtime_error("Unknown variable: " + name);
            }
        }
        void ParseLocal() {
            Consume(LET, "Expected let");
            std::string name = m_Current.Text;
            Consume(ID, "Expected identifier");

            if (m_Current.Type == COLON) {
                Advance();
                ParseTypeHint();
            }

            Consume(EQ, "Expected =");

            ParseExpression();

            uint8_t reg = m_NextLocalReg++;
            m_Locals[name] = reg;
            m_Program.AddReg(OpCode::MOV_REG, reg);

            if (m_Current.Type == SEMICOLON) Advance();
        }

        // Parse and discard a type hint, including simple Ptr<T> generic forms.
        void ParseTypeHint() {
            if (m_Current.Type != ID) {
                throw std::runtime_error("Expected type name");
            }

            std::string typeName = m_Current.Text;
            Advance();

            // Special-case Ptr<T> so we consume the entire generic.
            if (typeName == "Ptr" && m_Current.Type == LT) {
                Advance(); // '<'

                // Consume inner type name (e.g., Player, Math.Vec3)
                if (m_Current.Type == ID) {
                    Advance();
                }

                if (m_Current.Type != GT) {
                    throw std::runtime_error("Expected > to close Ptr<T> type");
                }
                Advance(); // '>'
            }
        }
    };

    Program Compiler::Compile(const std::string& source) {
        Parser parser(source);
        Program prog = parser.Parse();

        // Run optimizer first so the analyzer sees the final instruction stream.
        prog = Compiler::OptimizeProgram(prog);

        // Run memory analysis for Ptr<T>-related issues.
        std::vector<MemoryIssue> issues = MemoryAnalyzer::AnalyzeProgram(prog);
        if (!issues.empty()) {
            const MemoryIssue& issue = issues.front();
            std::string kindStr = (issue.kind == MemoryIssue::Kind::UseAfterFree)
                                  ? "Use-after-free"
                                  : "Double-free";
            std::string msg = "Memory analysis error (" + kindStr +
                              ") at instruction " + std::to_string(issue.instructionIndex);
            throw std::runtime_error(msg);
        }

        return prog;
    }

    std::unordered_map<std::string, Program> Compiler::BatchCompile(const std::filesystem::path& directory) {
        std::unordered_map<std::string, Program> programs;

        if (!std::filesystem::exists(directory)) return programs;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.path().extension() == ".mw") {
                std::string moduleName = entry.path().stem().string();
                std::filesystem::path cachePath = entry.path();
                cachePath.replace_extension(".mwc");

                bool useCache = false;
                if (std::filesystem::exists(cachePath)) {
                    auto mwTime = std::filesystem::last_write_time(entry.path());
                    auto mwcTime = std::filesystem::last_write_time(cachePath);
                    // Use cache if source file hasn't been modified since cache was created
                    // (i.e., cache is newer or same age as source)
                    if (mwcTime >= mwTime) {
                        useCache = true;
                    }
                }

                if (useCache) {
                    std::ifstream cfile(cachePath, std::ios::binary);
                    if (!cfile.is_open()) {
                        // Cache file exists but can't be opened, recompile
                        useCache = false;
                    } else {
                        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(cfile)), std::istreambuf_iterator<char>());
                        if (buffer.empty()) {
                            // Empty cache file, recompile
                            useCache = false;
                        } else {
                            try {
                                programs[moduleName] = Program::Deserialize(buffer);
                                // Verify cache has exports (might be stale)
                                if (programs[moduleName].Exports.empty()) {
                                    std::cerr << "WARNING: Cached module " << moduleName << " has no exports, recompiling..." << std::endl;
                                    useCache = false;
                                    programs.erase(moduleName);
                                }
                            } catch (...) {
                                // Deserialization failed, recompile
                                std::cerr << "WARNING: Failed to deserialize cache for " << moduleName << ", recompiling..." << std::endl;
                                useCache = false;
                                programs.erase(moduleName);
                            }
                        }
                    }
                }

                if (!useCache) {
                    std::ifstream file(entry.path());
                    if (!file.is_open()) {
                        continue; // Skip if can't open source file
                    }
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    try {
                        Program prog = Compile(content);
                        programs[moduleName] = prog;

                        // Save to cache
                        std::vector<uint8_t> buffer;
                        prog.Serialize(buffer);
                        std::ofstream cfile(cachePath, std::ios::binary);
                        if (cfile.is_open()) {
                            cfile.write((const char*)buffer.data(), buffer.size());
                        }
                    } catch (const std::exception& e) {
                        // Log error with file name for debugging
                        std::cerr << "ERROR: Failed to compile " << entry.path().string() << ": " << e.what() << std::endl;
                        throw; // Re-throw to let caller handle it
                    }
                }
            }
        }
        return programs;
    }

    namespace {

        void RunConstantFolding(Program& program) {
            for (size_t i = 0; i + 2 < program.Instructions.size(); ++i) {
                auto& inst = program.Instructions[i];
                auto& nextInst = program.Instructions[i + 1];

                // Fold constant arithmetic: PUSH_CONST a, PUSH_CONST b, ADD/SUB/MUL/DIV
                if (inst.Op == OpCode::PUSH_CONST && nextInst.Op == OpCode::PUSH_CONST) {
                    auto& opInst = program.Instructions[i + 2];
                    if (opInst.Op == OpCode::ADD || opInst.Op == OpCode::SUB ||
                        opInst.Op == OpCode::MUL || opInst.Op == OpCode::DIV) {
                        if (std::holds_alternative<int64_t>(inst.Operand) &&
                            std::holds_alternative<int64_t>(nextInst.Operand)) {
                            int64_t a = std::get<int64_t>(inst.Operand);
                            int64_t b = std::get<int64_t>(nextInst.Operand);
                            int64_t result = 0;

                            switch (opInst.Op) {
                                case OpCode::ADD: result = a + b; break;
                                case OpCode::SUB: result = a - b; break;
                                case OpCode::MUL: result = a * b; break;
                                case OpCode::DIV:
                                    if (b != 0) {
                                        result = a / b;
                                    } else {
                                        continue; // Skip if division by zero
                                    }
                                    break;
                                default: continue;
                            }

                            inst.Operand = result;
                            program.Instructions.erase(program.Instructions.begin() + i + 1,
                                                       program.Instructions.begin() + i + 3);
                            if (i > 0) {
                                --i;
                            }
                        }
                    }
                }
            }
        }

        void RunDeadCodeElimination(Program& program) {
            for (size_t i = 0; i < program.Instructions.size(); ++i) {
                if (program.Instructions[i].Op == OpCode::HALT) {
                    program.Instructions.erase(program.Instructions.begin() + i + 1,
                                               program.Instructions.end());
                    break;
                }
            }
        }

        void RunPeepholePass(Program& program) {
            // Remove PUSH_CONST x; POP sequences where the value is unused.
            for (size_t i = 0; i + 1 < program.Instructions.size(); ++i) {
                const auto& a = program.Instructions[i];
                const auto& b = program.Instructions[i + 1];
                if (a.Op == OpCode::PUSH_CONST && b.Op == OpCode::POP) {
                    program.Instructions.erase(program.Instructions.begin() + i,
                                               program.Instructions.begin() + i + 2);
                    if (i > 0) {
                        --i;
                    }
                }
            }
        }

        void RunNopRemoval(Program& program) {
            program.Instructions.erase(
                std::remove_if(program.Instructions.begin(), program.Instructions.end(),
                               [](const Instruction& inst) { return inst.Op == OpCode::NOP; }),
                program.Instructions.end());
        }

    } // namespace

    Program Compiler::OptimizeProgram(const Program& program) {
        Program optimized = program;

        RunConstantFolding(optimized);
        RunDeadCodeElimination(optimized);
        RunPeepholePass(optimized);
        RunNopRemoval(optimized);

        return optimized;
    }

}
