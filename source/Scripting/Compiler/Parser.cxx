#include "Parser.hxx"
#include "Lexer.hxx"
#include "../VM/BytecodeVM.hxx"
#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <filesystem>

namespace Solstice::Scripting {
namespace {
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
    uint8_t m_LastRegForPtrCheck = 255;

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

        std::string typeHint;
        if (m_Current.Type == COLON) {
            Advance();
            typeHint = ParseTypeHint();
        }

        Consume(EQ, "Expected =");

        // Parse expression
        ParseExpression();

        // Store in register
        uint8_t reg = m_NextReg++;
        m_Globals[name] = reg;
        if (!typeHint.empty()) {
            m_Program.RegisterTypes[reg] = typeHint;
        }
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

            std::string argType;
            if (m_Current.Type == COLON) {
                Advance();
                argType = ParseTypeHint();
            }
            m_Locals[argName] = m_NextLocalReg;
            if (!argType.empty()) {
                m_Program.RegisterTypes[m_NextLocalReg] = argType;
            }
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
                                    {
                                        size_t ptrIp = m_Program.Instructions.size();
                                        m_Program.Add(OpCode::PTR_IS_VALID);
                                        if (m_LastRegForPtrCheck != 255) {
                                            m_Program.PtrOperandRegs[ptrIp] = m_LastRegForPtrCheck;
                                        }
                                    }
                                } else if (funcName == "Get") {
                                    if (argCount != 1) {
                                        throw std::runtime_error("Ptr.Get expects exactly 1 argument");
                                    }
                                    {
                                        size_t ptrIp = m_Program.Instructions.size();
                                        m_Program.Add(OpCode::PTR_GET);
                                        if (m_LastRegForPtrCheck != 255) {
                                            m_Program.PtrOperandRegs[ptrIp] = m_LastRegForPtrCheck;
                                        }
                                    }
                                } else if (funcName == "Reset") {
                                    if (argCount != 1) {
                                        throw std::runtime_error("Ptr.Reset expects exactly 1 argument");
                                    }
                                    {
                                        size_t ptrIp = m_Program.Instructions.size();
                                        m_Program.Add(OpCode::PTR_RESET);
                                        if (m_LastRegForPtrCheck != 255) {
                                            m_Program.PtrOperandRegs[ptrIp] = m_LastRegForPtrCheck;
                                        }
                                    }
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
        m_LastRegForPtrCheck = 255;
        if (m_Locals.count(name)) {
            uint8_t r = m_Locals[name];
            m_LastRegForPtrCheck = r;
            m_Program.AddReg(OpCode::LOAD_REG, r);
        } else if (m_Globals.count(name)) {
            uint8_t r = m_Globals[name];
            m_LastRegForPtrCheck = r;
            m_Program.AddReg(OpCode::LOAD_REG, r);
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

        std::string localType;
        if (m_Current.Type == COLON) {
            Advance();
            localType = ParseTypeHint();
        }

        Consume(EQ, "Expected =");

        ParseExpression();

        uint8_t reg = m_NextLocalReg++;
        m_Locals[name] = reg;
        if (!localType.empty()) {
            m_Program.RegisterTypes[reg] = localType;
        }
        m_Program.AddReg(OpCode::MOV_REG, reg);

        if (m_Current.Type == SEMICOLON) Advance();
    }

    // Parse a type hint, including simple Ptr<T> generic forms; returns a normalized string.
    std::string ParseTypeHint() {
        if (m_Current.Type != ID) {
            throw std::runtime_error("Expected type name");
        }

        std::string typeName = m_Current.Text;
        Advance();

        if (typeName == "Ptr" && m_Current.Type == LT) {
            Advance(); // '<'

            std::string inner;
            if (m_Current.Type == ID) {
                inner = m_Current.Text;
                Advance();
            }

            if (m_Current.Type != GT) {
                throw std::runtime_error("Expected > to close Ptr<T> type");
            }
            Advance(); // '>'
            return inner.empty() ? std::string("Ptr") : ("Ptr." + inner);
        }
        return typeName;
    }
};

} // namespace

Program ParseProgramSource(const std::string& source) {
    Parser parser(source);
    return parser.Parse();
}

}
