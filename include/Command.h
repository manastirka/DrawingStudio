#pragma once

#include <QUndoCommand>

/**
 * Command pattern for undo/redo functionality
 * Each action that can be undone should inherit from this
 */
class Command : public QUndoCommand {
public:
  virtual ~Command() = default;

  // Execute the command
  virtual void execute() = 0;

  // QUndoCommand interface
  void redo() override { execute(); }
  void undo() override = 0;

  // Get command description for UI
  virtual QString description() const = 0;

  // Some commands can be merged (e.g., continuous drawing)
  virtual bool canMergeWith(const Command *other) const { return false; }
  virtual void mergeWith(const Command *other) {}

  // QUndoCommand id for merging
  int id() const override { return -1; }
  bool mergeWith(const QUndoCommand *other) override {
    const Command *cmd = dynamic_cast<const Command *>(other);
    if (cmd && canMergeWith(cmd)) {
      // Const cast needed because mergeWith is non-const in our interface
      const_cast<Command *>(this)->mergeWith(cmd);
      return true;
    }
    return false;
  }
};

using CommandPtr = std::unique_ptr<Command>;
