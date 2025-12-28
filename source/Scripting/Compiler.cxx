#include "Compiler.hxx"
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
        NOEXPORT,
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
        // Range
        RANGE, // ..
        // String interpolation
        STRING_INTERPOLATION_START, STRING_INTERPOLATION_END,
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

            if (isalpha(c) || c == '@') {
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
                if (text == "switch") return {SWITCH, text};
                if (text == "case") return {CASE, text};
                if (text == "default") return {DEFAULT, text};
                if (text == "break") return {BREAK, text};
                if (text == "continue") return {CONTINUE, text};
                if (text == "match") return {MATCH, text};
                if (text == "@function") return {FUNCTION, text};
                if (text == "@Entry") return {ENTRY, text};
                if (text == "@noexport") return {NOEXPORT, text};
                if (text == "return") return {RETURN, text};
                if (text == "module") return {MODULE, text};
                if (text == "import") return {IMPORT, text};
                if (text == "class") return {CLASS, text};
                if (text == "new") return {NEW, text};
                if (text == "true") return {NUMBER, text, 1}; // Boolean true as number 1
                if (text == "false") return {NUMBER, text, 0}; // Boolean false as number 0

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

            std::string baseClass = "";
            if (m_Current.Type == COLON) {
                Advance();
                baseClass = m_Current.Text;
                Consume(ID, "Expected base class name");
            }

            Consume(LBRACE, "Expected {");
            // Simplified: classes are just namespaces for now
            // Future: actual object instantiation
            while (m_Current.Type != RBRACE && m_Current.Type != END) {
                if (m_Current.Type == LET) {
                    ParseGlobal(); // Should probably prefix with className
                } else if (m_Current.Type == FUNCTION) {
                    ParseFunction(); // Should probably prefix with className
                } else {
                    Advance();
                }
            }
            Consume(RBRACE, "Expected }");
        }

        void ParseGlobal() {
            Consume(LET, "Expected let");
            std::string name = m_Current.Text;
            Consume(ID, "Expected identifier");

            if (m_Current.Type == COLON) {
                Advance();
                Consume(ID, "Expected type"); // Skip type
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
            Consume(FUNCTION, "Expected @function");
            std::string name = m_Current.Text;
            Consume(ID, "Expected function name");

            m_Functions[name] = m_Program.Instructions.size();
            if (shouldExport) {
                m_ExportedFunctions.insert(name);
            }

            Consume(LPAREN, "Expected (");

            // Args
            m_Locals.clear();
            // Preserve globals in m_Locals? No, check globals if not in locals.
            // But we need to allocate registers for args.
            // We'll use high registers or just continue allocating?
            // We'll use high registers or just continue allocating?
            // For simplicity, let's reuse registers R10+ for locals.
            m_NextLocalReg = 10;

            while (m_Current.Type != RPAREN) {
                std::string argName = m_Current.Text;
                Consume(ID, "Expected arg name");
                m_Locals[argName] = m_NextLocalReg;
                // Pop arg from stack to register
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
            } else if (m_Current.Type == IF) {
                ParseIf();
            } else if (m_Current.Type == WHILE) {
                ParseWhile();
            } else if (m_Current.Type == FOR) {
                ParseFor();
            } else if (m_Current.Type == SWITCH) {
                ParseSwitch();
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

        void ParseSwitch() {
            Consume(SWITCH, "Expected switch");
            Consume(LPAREN, "Expected (");
            ParseExpression(); // Switch value
            Consume(RPAREN, "Expected )");
            Consume(LBRACE, "Expected {");

            size_t switchEnd = 0; // Will be set later
            std::vector<size_t> caseJumps;

            while (m_Current.Type != RBRACE && m_Current.Type != END) {
                if (m_Current.Type == CASE) {
                    Advance();
                    ParseExpression(); // Case value
                    Consume(COLON, "Expected :");

                    // Compare switch value with case value
                    m_Program.Add(OpCode::DUP); // Duplicate switch value
                    m_Program.Add(OpCode::EQ);
                    m_Program.Add(OpCode::JMP_IF, 0);
                    caseJumps.push_back(m_Program.Instructions.size() - 1);

                    // Jump over this case if not matched
                    m_Program.Add(OpCode::JMP, 0);
                    size_t skipCaseIdx = m_Program.Instructions.size() - 1;

                    // Case body
                    size_t caseStart = m_Program.Instructions.size();
                    for (size_t i = 0; i < caseJumps.size() - 1; ++i) {
                        m_Program.Instructions[caseJumps[i]].Operand = (int64_t)caseStart;
                    }

                    size_t breakTargetStart = m_BreakTargets.size();
                    while (m_Current.Type != CASE && m_Current.Type != DEFAULT &&
                           m_Current.Type != RBRACE && m_Current.Type != END) {
                        ParseStatement();
                    }

                    // Fix break targets for this case
                    if (m_Current.Type == CASE || m_Current.Type == DEFAULT || m_Current.Type == RBRACE) {
                        size_t nextCaseStart = m_Program.Instructions.size();
                        for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                            m_Program.Instructions[m_BreakTargets[i]].Operand = (int64_t)nextCaseStart;
                        }
                        m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());
                    }

                    m_Program.Instructions[skipCaseIdx].Operand = (int64_t)m_Program.Instructions.size();
                } else if (m_Current.Type == DEFAULT) {
                    Advance();
                    Consume(COLON, "Expected :");

                    size_t defaultStart = m_Program.Instructions.size();
                    // Fix remaining case jumps to default
                    for (size_t i = 0; i < caseJumps.size(); ++i) {
                        auto& operand = m_Program.Instructions[caseJumps[i]].Operand;
                        if (std::holds_alternative<int64_t>(operand) && std::get<int64_t>(operand) == 0) {
                            operand = (int64_t)defaultStart;
                        }
                    }

                    size_t breakTargetStart = m_BreakTargets.size();
                    while (m_Current.Type != RBRACE && m_Current.Type != END) {
                        ParseStatement();
                    }

                    // Fix break targets
                    switchEnd = m_Program.Instructions.size();
                    for (size_t i = breakTargetStart; i < m_BreakTargets.size(); ++i) {
                        m_Program.Instructions[m_BreakTargets[i]].Operand = (int64_t)switchEnd;
                    }
                    m_BreakTargets.erase(m_BreakTargets.begin() + breakTargetStart, m_BreakTargets.end());
                } else {
                    Advance();
                }
            }

            Consume(RBRACE, "Expected }");

            // Pop switch value
            m_Program.Add(OpCode::POP);

            if (switchEnd == 0) {
                switchEnd = m_Program.Instructions.size();
            }

            // Fix any remaining break targets
            for (size_t i = 0; i < m_BreakTargets.size(); ++i) {
                auto& operand = m_Program.Instructions[m_BreakTargets[i]].Operand;
                if (std::holds_alternative<int64_t>(operand) && std::get<int64_t>(operand) == 0) {
                    operand = (int64_t)switchEnd;
                }
            }
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
            if (m_Current.Type == NOT_LOGIC) {
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
                m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                m_Program.Add(OpCode::NEW_OBJ, className);
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
                                // This is Module.Function() - qualified function call
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

                                // Qualified calls are always treated as module calls (native or module function)
                                std::string qualifiedName = name + "." + funcName;
                                m_Program.Add(OpCode::PUSH_CONST, (int64_t)argCount);
                                m_Program.Add(OpCode::CALL, qualifiedName);
                                // Skip chaining since we've handled this as a function call
                            } else {
                                // Not LPAREN, so this is attribute access: Module.attr
                                // We've already consumed DOT and ID, so emit GET_ATTR
                                LoadVariable(name);
                                m_Program.Add(OpCode::GET_ATTR, funcName);
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
                Consume(ID, "Expected type");
            }

            Consume(EQ, "Expected =");

            ParseExpression();

            uint8_t reg = m_NextLocalReg++;
            m_Locals[name] = reg;
            m_Program.AddReg(OpCode::MOV_REG, reg);

            if (m_Current.Type == SEMICOLON) Advance();
        }
    };

    Program Compiler::Compile(const std::string& source) {
        Parser parser(source);
        return parser.Parse();
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

    Program Compiler::OptimizeProgram(const Program& program) {
        Program optimized = program;

        // Constant folding pass
        for (size_t i = 0; i < optimized.Instructions.size() - 1; ++i) {
            auto& inst = optimized.Instructions[i];
            auto& nextInst = optimized.Instructions[i + 1];

            // Fold constant arithmetic: PUSH_CONST a, PUSH_CONST b, ADD -> PUSH_CONST (a+b)
            if (inst.Op == OpCode::PUSH_CONST && nextInst.Op == OpCode::PUSH_CONST) {
                if (i + 2 < optimized.Instructions.size()) {
                    auto& opInst = optimized.Instructions[i + 2];
                    if (opInst.Op == OpCode::ADD || opInst.Op == OpCode::SUB ||
                        opInst.Op == OpCode::MUL || opInst.Op == OpCode::DIV) {
                        // Check if both are numbers
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
                                    if (b != 0) result = a / b;
                                    else continue; // Skip optimization if division by zero
                                    break;
                                default: continue;
                            }

                            // Replace with single PUSH_CONST
                            inst.Operand = result;
                            optimized.Instructions.erase(optimized.Instructions.begin() + i + 1,
                                                         optimized.Instructions.begin() + i + 3);
                            i--; // Re-check this position
                            continue;
                        }
                    }
                }
            }
        }

        // Dead code elimination (remove unreachable code after HALT)
        for (size_t i = 0; i < optimized.Instructions.size(); ++i) {
            if (optimized.Instructions[i].Op == OpCode::HALT) {
                // Remove all instructions after HALT
                optimized.Instructions.erase(optimized.Instructions.begin() + i + 1,
                                             optimized.Instructions.end());
                break;
            }
        }

        // Remove NOP instructions
        optimized.Instructions.erase(
            std::remove_if(optimized.Instructions.begin(), optimized.Instructions.end(),
                [](const Instruction& inst) { return inst.Op == OpCode::NOP; }),
            optimized.Instructions.end());

        return optimized;
    }

}
