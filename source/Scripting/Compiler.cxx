#include "Compiler.hxx"
#include <vector>
#include <iostream>
#include <map>
#include <cctype>
#include <stdexcept>

namespace Solstice::Scripting {

    enum TokenType {
        ID, STRING, NUMBER,
        LET, FUNCTION, ENTRY, RETURN,
        LPAREN, RPAREN, LBRACE, RBRACE,
        EQ, PLUS, PLUSPLUS, COMMA, SEMICOLON, COLON,
        // Comparisons
        EQ_EQ, NEQ, LT, GT,
        IF, ELSE, WHILE,
        END
    };

    struct Token {
        TokenType Type;
        std::string Text;
        int64_t IntValue = 0;
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
                if (text == "@function") return {FUNCTION, text};
                if (text == "@Entry") return {ENTRY, text};
                if (text == "return") return {RETURN, text};
                
                return {ID, text};
            }

            if (isdigit(c)) {
                size_t start = m_Pos;
                while (m_Pos < m_Src.size() && isdigit(m_Src[m_Pos])) m_Pos++;
                std::string text = m_Src.substr(start, m_Pos - start);
                return {NUMBER, text, std::stoll(text)};
            }

            if (c == '"') {
                m_Pos++;
                size_t start = m_Pos;
                while (m_Pos < m_Src.size() && m_Src[m_Pos] != '"') m_Pos++;
                std::string text = m_Src.substr(start, m_Pos - start);
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

            m_Pos++;
            switch (c) {
                case '(': return {LPAREN, "("};
                case ')': return {RPAREN, ")"};
                case '{': return {LBRACE, "{"};
                case '}': return {RBRACE, "}"};
                case '=': return {EQ, "="};
                case '<': return {LT, "<"};
                case '>': return {GT, ">"};
                case '+': 
                    if (m_Pos < m_Src.size() && m_Src[m_Pos] == '+') {
                        m_Pos++;
                        return {PLUSPLUS, "++"};
                    }
                    return {PLUS, "+"};
                case ',': return {COMMA, ","};
                case ';': return {SEMICOLON, ";"};
                case ':': return {COLON, ":"};
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
            // Emit jump to Entry (placeholder)
            m_Program.Add(OpCode::CALL, (int64_t)0); 
            size_t entryCallIdx = m_Program.Instructions.size() - 1;
            m_Program.Add(OpCode::HALT);

            while (m_Current.Type != END) {
                if (m_Current.Type == LET) {
                    ParseGlobal();
                } else if (m_Current.Type == FUNCTION) {
                    ParseFunction();
                } else if (m_Current.Type == ENTRY) {
                    size_t entryAddr = m_Program.Instructions.size();
                    // Backpatch entry call
                    m_Program.Instructions[entryCallIdx].Operand = (int64_t)entryAddr;
                    ParseEntry();
                } else {
                    Advance(); // Skip unknown?
                }
            }
            return m_Program;
        }

    private:
        Lexer m_Lexer;
        Token m_Current;
        Program m_Program;
        
        std::map<std::string, uint8_t> m_Globals;
        std::map<std::string, uint8_t> m_Locals; // Map name to Register Index
        std::map<std::string, size_t> m_Functions;
        uint8_t m_NextReg = 0;
        uint8_t m_NextLocalReg = 10;

        void Advance() {
            m_Current = m_Lexer.Next();
        }

        void Consume(TokenType type, const std::string& err) {
            if (m_Current.Type == type) Advance();
            else throw std::runtime_error(err + " Got: " + m_Current.Text);
        }

        Token PeekNext() {
            return m_Lexer.Peek();
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

        void ParseFunction() {
            Consume(FUNCTION, "Expected @function");
            std::string name = m_Current.Text;
            Consume(ID, "Expected function name");
            
            m_Functions[name] = m_Program.Instructions.size();
            
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
            } else if (m_Current.Type == ID && PeekNext().Type == EQ) {
                // Assignment statement
                std::string name = m_Current.Text;
                Advance(); // Eat ID
                Consume(EQ, "Expected =");
                ParseExpression();
                StoreVariable(name);
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
            while (m_Current.Type != RBRACE) {
                ParseStatement();
            }
            Consume(RBRACE, "Expected }");
            
            // Jump back to condition
            m_Program.Add(OpCode::JMP, (int64_t)loopStart);
            
            // Loop End
            size_t loopEnd = m_Program.Instructions.size();
            m_Program.Instructions[jumpOverBodyIdx].Operand = (int64_t)loopEnd;
        }

        void ParseExpression() {
            ParseComparison();
        }

        void ParseComparison() {
            ParseAdditive();
            
            while (m_Current.Type == EQ_EQ || m_Current.Type == NEQ || m_Current.Type == LT || m_Current.Type == GT) {
                TokenType op = m_Current.Type;
                Advance();
                ParseAdditive();
                if (op == EQ_EQ) m_Program.Add(OpCode::EQ);
                else if (op == NEQ) m_Program.Add(OpCode::NEQ);
                else if (op == LT) m_Program.Add(OpCode::LT);
                else if (op == GT) m_Program.Add(OpCode::GT);
            }
        }
        
        void ParseAdditive() {
            ParseTerm();
            while (m_Current.Type == PLUS) {
                Advance();
                ParseTerm();
                m_Program.Add(OpCode::ADD);
            }
        }

        void ParseTerm() {
            if (m_Current.Type == NUMBER) {
                m_Program.Add(OpCode::PUSH_CONST, m_Current.IntValue);
                Advance();
            } else if (m_Current.Type == STRING) {
                m_Program.Add(OpCode::PUSH_CONST, m_Current.Text);
                Advance();
            } else if (m_Current.Type == ID) {
                std::string name = m_Current.Text;
                Advance();
                
                if (m_Current.Type == LPAREN) {
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
                } else if (m_Current.Type == PLUSPLUS) {
                    // Post-increment
                    Advance();
                    LoadVariable(name);
                    m_Program.Add(OpCode::DUP); // Return old value
                    m_Program.Add(OpCode::PUSH_CONST, (int64_t)1);
                    m_Program.Add(OpCode::ADD);
                    StoreVariable(name);
                } else {
                    LoadVariable(name);
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

}
