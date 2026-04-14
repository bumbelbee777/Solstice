#include "Compiler.hxx"
#include <algorithm>
#include <variant>

namespace Solstice::Scripting {

namespace {

void RunConstantFolding(Program& program) {
    for (size_t i = 0; i + 2 < program.Instructions.size(); ++i) {
        auto& inst = program.Instructions[i];
        auto& nextInst = program.Instructions[i + 1];

        if (inst.Op == OpCode::PUSH_CONST && nextInst.Op == OpCode::PUSH_CONST) {
            auto& opInst = program.Instructions[i + 2];
            if (opInst.Op == OpCode::ADD || opInst.Op == OpCode::SUB || opInst.Op == OpCode::MUL ||
                opInst.Op == OpCode::DIV) {
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
                                continue;
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

bool HasCodeAfterHalt(const Program& program, size_t haltIndex) {
    for (const auto& [_, ip] : program.Exports) {
        if (ip > haltIndex) return true;
    }
    for (const auto& [_, cm] : program.ClassInfo) {
        if (cm.ConstructorAddress > haltIndex || cm.DestructorAddress > haltIndex) return true;
    }
    for (size_t i = 0; i < program.Instructions.size(); ++i) {
        const Instruction& inst = program.Instructions[i];
        if (inst.Op == OpCode::CALL && std::holds_alternative<int64_t>(inst.Operand)) {
            if ((size_t)std::get<int64_t>(inst.Operand) > haltIndex) return true;
        }
        if (inst.Op == OpCode::JMP && std::holds_alternative<int64_t>(inst.Operand)) {
            if ((size_t)std::get<int64_t>(inst.Operand) > haltIndex) return true;
        }
        if (inst.Op == OpCode::JMP_IF && std::holds_alternative<int64_t>(inst.Operand)) {
            if ((size_t)std::get<int64_t>(inst.Operand) > haltIndex) return true;
        }
    }
    return false;
}

void RunDeadCodeElimination(Program& program) {
    for (size_t i = 0; i < program.Instructions.size(); ++i) {
        if (program.Instructions[i].Op == OpCode::HALT) {
            if (HasCodeAfterHalt(program, i)) {
                break;
            }
            program.Instructions.erase(program.Instructions.begin() + i + 1, program.Instructions.end());
            break;
        }
    }
}

void RunPeepholePass(Program& program) {
    for (size_t i = 0; i + 1 < program.Instructions.size(); ++i) {
        const auto& a = program.Instructions[i];
        const auto& b = program.Instructions[i + 1];
        if (a.Op == OpCode::PUSH_CONST && b.Op == OpCode::POP) {
            program.Instructions.erase(program.Instructions.begin() + i, program.Instructions.begin() + i + 2);
            if (i > 0) {
                --i;
            }
        } else if (a.Op == OpCode::DUP && b.Op == OpCode::POP) {
            program.Instructions.erase(program.Instructions.begin() + i, program.Instructions.begin() + i + 2);
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
    RunConstantFolding(optimized);
    RunPeepholePass(optimized);
    RunNopRemoval(optimized);

    return optimized;
}

} // namespace Solstice::Scripting
