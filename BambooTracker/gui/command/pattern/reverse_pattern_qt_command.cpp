#include "reverse_pattern_qt_command.hpp"
#include "command_id.hpp"

ReversePatternQtCommand::ReversePatternQtCommand(PatternEditorPanel* panel, QUndoCommand* parent)
	: QUndoCommand(parent),
	  panel_(panel)
{
}

void ReversePatternQtCommand::redo()
{
	panel_->redrawByPatternChanged(true);
}

void ReversePatternQtCommand::undo()
{
	panel_->redrawByPatternChanged(true);
}

int ReversePatternQtCommand::id() const
{
	return CommandId::ReversePattern;
}
