#pragma once

#include <memory>
#include "module.hpp"
#include "instruments_manager.hpp"
#include "binary_container.hpp"

class ModuleIO
{
public:
	static void saveModule(BinaryContainer& ctr, const std::weak_ptr<Module> mod,
						   const std::weak_ptr<InstrumentsManager> instMan);
	static void loadModule(const BinaryContainer& ctr, std::weak_ptr<Module> mod,
						   std::weak_ptr<InstrumentsManager> instMan);

private:
	ModuleIO();

	static size_t loadModuleSectionInModule(std::weak_ptr<Module> mod, const BinaryContainer& ctr,
											size_t globCsr, uint32_t version);
	static size_t loadInstrumentSectionInModule(std::weak_ptr<InstrumentsManager> instMan,
												const BinaryContainer& ctr, size_t globCsr,
												uint32_t version);
	static size_t loadInstrumentPropertySectionInModule(std::weak_ptr<InstrumentsManager> instMan,
														const BinaryContainer& ctr, size_t globCsr,
														uint32_t version);
	static size_t loadInstrumentPropertyOperatorSequence(FMEnvelopeParameter param,
														 size_t instMemCsr,
														 std::shared_ptr<InstrumentsManager>& instManLocked,
														 const BinaryContainer& ctr, uint32_t version);
	static size_t loadGrooveSectionInModule(std::weak_ptr<Module> mod, const BinaryContainer& ctr,
											size_t globCsr, uint32_t version);
	static size_t loadSongSectionInModule(std::weak_ptr<Module> mod, const BinaryContainer& ctr,
										  size_t globCsr, uint32_t version);
};
