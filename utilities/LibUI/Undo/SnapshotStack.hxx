#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace LibUI::Undo {

/// Deep-snapshot undo/redo stack for editor tools. Call ``PushBeforeChange`` with a copy of the document **before**
/// mutating; ``Undo`` / ``Redo`` swap the live document with stack entries.
template<typename T>
class SnapshotStack {
public:
    explicit SnapshotStack(std::size_t maxDepth = 48) : maxDepth_(maxDepth) {}

    void SetMaxDepth(std::size_t n) { maxDepth_ = n; }
    std::size_t MaxDepth() const { return maxDepth_; }

    void SetAfterApply(std::function<void(T&)> cb) { afterApply_ = std::move(cb); }
    void ClearAfterApply() { afterApply_ = {}; }

    void PushBeforeChange(const T& snapshot) {
        undo_.push_back(snapshot);
        TrimUndo();
        redo_.clear();
    }

    void PushBeforeChange(T&& snapshot) {
        undo_.push_back(std::move(snapshot));
        TrimUndo();
        redo_.clear();
    }

    bool Undo(T& current) {
        if (undo_.empty()) {
            return false;
        }
        redo_.push_back(std::move(current));
        current = std::move(undo_.back());
        undo_.pop_back();
        if (afterApply_) {
            afterApply_(current);
        }
        return true;
    }

    bool Redo(T& current) {
        if (redo_.empty()) {
            return false;
        }
        undo_.push_back(std::move(current));
        current = std::move(redo_.back());
        redo_.pop_back();
        if (afterApply_) {
            afterApply_(current);
        }
        return true;
    }

    void Clear() {
        undo_.clear();
        redo_.clear();
    }

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

private:
    void TrimUndo() {
        while (undo_.size() > maxDepth_) {
            undo_.erase(undo_.begin());
        }
    }

    std::vector<T> undo_;
    std::vector<T> redo_;
    std::size_t maxDepth_;
    std::function<void(T&)> afterApply_;
};

} // namespace LibUI::Undo
