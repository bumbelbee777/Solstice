#include "MemoryAnalysis.hxx"
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Solstice::Scripting {

namespace {

    enum class PtrState {
        Unknown,
        Live,
        Freed
    };

    PtrState MergeStates(PtrState a, PtrState b) {
        if (a == b) return a;
        if (a == PtrState::Unknown || b == PtrState::Unknown) return PtrState::Unknown;
        // Live vs Freed at join — conservative unknown
        return PtrState::Unknown;
    }

    void CollectFunctionStarts(const Program& program, std::set<size_t>& starts) {
        starts.insert(0);
        for (const auto& [name, ip] : program.Exports) {
            (void)name;
            starts.insert(ip);
        }
        for (const auto& [_, cm] : program.ClassInfo) {
            if (cm.ConstructorAddress != 0) starts.insert(cm.ConstructorAddress);
            if (cm.DestructorAddress != 0) starts.insert(cm.DestructorAddress);
        }
        for (size_t i = 0; i < program.Instructions.size(); ++i) {
            const Instruction& inst = program.Instructions[i];
            if (inst.Op == OpCode::CALL && std::holds_alternative<int64_t>(inst.Operand)) {
                starts.insert((size_t)std::get<int64_t>(inst.Operand));
            }
            if (inst.Op == OpCode::JMP && std::holds_alternative<int64_t>(inst.Operand)) {
                starts.insert((size_t)std::get<int64_t>(inst.Operand));
            }
            if (inst.Op == OpCode::JMP_IF && std::holds_alternative<int64_t>(inst.Operand)) {
                starts.insert((size_t)std::get<int64_t>(inst.Operand));
            }
        }
    }

    std::string NameForIP(const Program& program, size_t ip) {
        for (const auto& [name, eip] : program.Exports) {
            if (eip == ip) return name;
        }
        return "";
    }

    // Build intraprocedural CFG for [rangeBegin, rangeEnd) and run single-slot Ptr lattice.
    void AnalyzeRange(const Program& program, size_t rangeBegin, size_t rangeEnd, const std::string& funcLabel,
                      std::vector<MemoryIssue>& issues) {
        if (rangeBegin >= rangeEnd || rangeEnd > program.Instructions.size()) return;

        // Basic block leaders inside range
        std::set<size_t> leaders;
        leaders.insert(rangeBegin);
        for (size_t ip = rangeBegin; ip + 1 < rangeEnd; ++ip) {
            const Instruction& inst = program.Instructions[ip];
            if (inst.Op == OpCode::JMP && std::holds_alternative<int64_t>(inst.Operand)) {
                size_t t = (size_t)std::get<int64_t>(inst.Operand);
                if (t >= rangeBegin && t < rangeEnd) leaders.insert(t);
            } else if (inst.Op == OpCode::JMP_IF && std::holds_alternative<int64_t>(inst.Operand)) {
                size_t t = (size_t)std::get<int64_t>(inst.Operand);
                if (t >= rangeBegin && t < rangeEnd) leaders.insert(t);
                if (ip + 1 >= rangeBegin && ip + 1 < rangeEnd) leaders.insert(ip + 1);
            } else if (inst.Op == OpCode::RET || inst.Op == OpCode::HALT) {
                if (ip + 1 < rangeEnd) leaders.insert(ip + 1);
            }
        }

        std::vector<size_t> sortedLeaders(leaders.begin(), leaders.end());
        std::sort(sortedLeaders.begin(), sortedLeaders.end());

        // block id -> [start, end)
        std::vector<std::pair<size_t, size_t>> blocks;
        for (size_t i = 0; i < sortedLeaders.size(); ++i) {
            size_t b = sortedLeaders[i];
            size_t e = (i + 1 < sortedLeaders.size()) ? sortedLeaders[i + 1] : rangeEnd;
            if (b < rangeBegin || b >= rangeEnd) continue;
            e = std::min(e, rangeEnd);
            if (b < e) blocks.push_back({b, e});
        }
        if (blocks.empty()) return;

        std::unordered_map<size_t, int> leaderToBlock;
        for (int bi = 0; bi < (int)blocks.size(); ++bi) {
            leaderToBlock[blocks[(size_t)bi].first] = bi;
        }

        std::vector<std::vector<int>> succ((size_t)blocks.size());
        for (int bi = 0; bi < (int)blocks.size(); ++bi) {
            size_t start = blocks[(size_t)bi].first;
            size_t end = blocks[(size_t)bi].second;
            if (start >= end) continue;
            const Instruction& last = program.Instructions[end - 1];
            if (last.Op == OpCode::JMP && std::holds_alternative<int64_t>(last.Operand)) {
                size_t t = (size_t)std::get<int64_t>(last.Operand);
                if (t >= rangeBegin && t < rangeEnd) {
                    auto it = leaderToBlock.find(t);
                    if (it != leaderToBlock.end()) succ[(size_t)bi].push_back(it->second);
                }
            } else if (last.Op == OpCode::JMP_IF && std::holds_alternative<int64_t>(last.Operand)) {
                size_t t = (size_t)std::get<int64_t>(last.Operand);
                if (t >= rangeBegin && t < rangeEnd) {
                    auto it = leaderToBlock.find(t);
                    if (it != leaderToBlock.end()) succ[(size_t)bi].push_back(it->second);
                }
                if (end < rangeEnd) {
                    auto it = leaderToBlock.find(end);
                    if (it != leaderToBlock.end()) succ[(size_t)bi].push_back(it->second);
                }
            } else if (last.Op != OpCode::RET && last.Op != OpCode::HALT) {
                if (end < rangeEnd) {
                    auto it = leaderToBlock.find(end);
                    if (it != leaderToBlock.end()) succ[(size_t)bi].push_back(it->second);
                }
            }
        }

        // Iterative dataflow: IN[b] = merge(OUT[pred])
        std::vector<PtrState> in((size_t)blocks.size(), PtrState::Unknown);
        std::vector<PtrState> out((size_t)blocks.size(), PtrState::Unknown);

        auto transfer = [&](PtrState state, size_t start, size_t end) -> PtrState {
            for (size_t ip = start; ip < end; ++ip) {
                const Instruction& inst = program.Instructions[ip];
                switch (inst.Op) {
                    case OpCode::PTR_NEW:
                        state = PtrState::Live;
                        break;
                    case OpCode::PTR_RESET:
                        if (state == PtrState::Freed) {
                            MemoryIssue issue;
                            issue.kind = MemoryIssue::Kind::DoubleFree;
                            issue.functionName = funcLabel;
                            issue.instructionIndex = ip;
                            issues.push_back(issue);
                        }
                        state = PtrState::Freed;
                        break;
                    case OpCode::PTR_GET:
                        if (state == PtrState::Freed || state == PtrState::Unknown) {
                            MemoryIssue issue;
                            issue.kind = MemoryIssue::Kind::UseAfterFree;
                            issue.functionName = funcLabel;
                            issue.instructionIndex = ip;
                            issues.push_back(issue);
                        }
                        break;
                    default:
                        break;
                }
            }
            return state;
        };

        bool changed = true;
        while (changed) {
            changed = false;
            for (int bi = 0; bi < (int)blocks.size(); ++bi) {
                PtrState meet = PtrState::Unknown;
                bool first = true;
                // predecessors: blocks that have an edge to bi
                for (int pj = 0; pj < (int)blocks.size(); ++pj) {
                    for (int s : succ[(size_t)pj]) {
                        if (s == bi) {
                            if (first) {
                                meet = out[(size_t)pj];
                                first = false;
                            } else {
                                meet = MergeStates(meet, out[(size_t)pj]);
                            }
                        }
                    }
                }
                if (first) {
                    meet = PtrState::Unknown;
                }

                if (meet != in[(size_t)bi]) {
                    in[(size_t)bi] = meet;
                    changed = true;
                }

                PtrState newOut = transfer(in[(size_t)bi], blocks[(size_t)bi].first, blocks[(size_t)bi].second);
                if (newOut != out[(size_t)bi]) {
                    out[(size_t)bi] = newOut;
                    changed = true;
                }
            }
        }
    }

} // namespace

std::vector<MemoryIssue> MemoryAnalyzer::AnalyzeProgram(const Program& program) {
    std::vector<MemoryIssue> issues;

    std::set<size_t> starts;
    CollectFunctionStarts(program, starts);

    std::vector<size_t> sorted(starts.begin(), starts.end());
    std::sort(sorted.begin(), sorted.end());

    for (size_t i = 0; i < sorted.size(); ++i) {
        size_t rangeBegin = sorted[i];
        size_t rangeEnd = (i + 1 < sorted.size()) ? sorted[i + 1] : program.Instructions.size();
        std::string label = NameForIP(program, rangeBegin);
        if (label.empty()) {
            label = "ip:" + std::to_string(rangeBegin);
        }
        AnalyzeRange(program, rangeBegin, rangeEnd, label, issues);
    }

    return issues;
}

} // namespace Solstice::Scripting
