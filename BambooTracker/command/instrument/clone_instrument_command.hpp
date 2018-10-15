#pragma once

#include "abstract_command.hpp"
#include "instruments_manager.hpp"

class cloneInstrumentCommand : public AbstractCommand
{
public:
	cloneInstrumentCommand(InstrumentsManager &manager, int num, int refNum);

	void redo() override;
	void undo() override;
	int getID() const override;

private:
	InstrumentsManager& manager_;
	int cloneInstNum_, refInstNum_;
};
