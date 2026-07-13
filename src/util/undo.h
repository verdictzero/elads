// SPDX-License-Identifier: GPL-3.0-or-later
// elads — a minimal command-pattern undo/redo stack (GUI/GL-free, header-only).
//
// Editors record each mutation as a pair of {apply, revert} closures. This keeps the
// MapModel itself free of undo bookkeeping (see docs/design/03-data-model.md §undo).
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace elads::util {

class UndoManager {
public:
    using Action = std::function<void()>;

    // Record and immediately apply a command. Clears the redo stack.
    void perform(std::string name, Action apply, Action revert) {
        apply();
        undo_.push_back(Command{std::move(name), std::move(apply), std::move(revert)});
        redo_.clear();
    }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    bool undo() {
        if (undo_.empty())
            return false;
        Command c = std::move(undo_.back());
        undo_.pop_back();
        c.revert();
        redo_.push_back(std::move(c));
        return true;
    }

    bool redo() {
        if (redo_.empty())
            return false;
        Command c = std::move(redo_.back());
        redo_.pop_back();
        c.apply();
        undo_.push_back(std::move(c));
        return true;
    }

    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }
    void clear() {
        undo_.clear();
        redo_.clear();
    }

private:
    struct Command {
        std::string name;
        Action apply;
        Action revert;
    };
    std::vector<Command> undo_;
    std::vector<Command> redo_;
};

} // namespace elads::util
