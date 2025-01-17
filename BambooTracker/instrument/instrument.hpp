#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "instruments_manager.hpp"
#include "envelope_fm.hpp"
#include "lfo_fm.hpp"
#include "command_sequence.hpp"
#include "enum_hash.hpp"
#include "misc.hpp"

class InstrumentsManager;

enum class InstrumentType : int { FM, SSG, ADPCM, Drumkit };

class AbstractInstrument
{
public:
	virtual ~AbstractInstrument() = default;

	int getNumber() const;
	void setNumber(int n);
	virtual SoundSource getSoundSource() const = 0;
	virtual InstrumentType getType() const = 0;
	std::string getName() const;
	void setName(std::string name);
	bool isRegisteredWithManager() const;
	virtual AbstractInstrument* clone() = 0;

protected:
	InstrumentsManager* owner_;
	int number_;
	std::string name_;	// UTF-8
	AbstractInstrument(int number, std::string name, InstrumentsManager* owner);
};


class InstrumentFM : public AbstractInstrument
{
public:
	InstrumentFM(int number, std::string name, InstrumentsManager* owner);
	SoundSource getSoundSource() const override;
	InstrumentType getType() const override;
	AbstractInstrument* clone() override;

	void setEnvelopeNumber(int n);
	int getEnvelopeNumber() const;
	int getEnvelopeParameter(FMEnvelopeParameter param) const;
	bool getOperatorEnabled(int n) const;

	void setLFOEnabled(bool enabled);
	bool getLFOEnabled() const;
	void setLFONumber(int n);
	int getLFONumber() const;
	int getLFOParameter(FMLFOParameter param) const;

	void setOperatorSequenceEnabled(FMEnvelopeParameter param, bool enabled);
	bool getOperatorSequenceEnabled(FMEnvelopeParameter param) const;
	void setOperatorSequenceNumber(FMEnvelopeParameter param, int n);
	int getOperatorSequenceNumber(FMEnvelopeParameter param) const;
	std::vector<CommandSequenceUnit> getOperatorSequenceSequence(FMEnvelopeParameter param) const;
	std::vector<Loop> getOperatorSequenceLoops(FMEnvelopeParameter param) const;
	Release getOperatorSequenceRelease(FMEnvelopeParameter param) const;
	std::unique_ptr<CommandSequence::Iterator> getOperatorSequenceSequenceIterator(FMEnvelopeParameter param) const;

	void setArpeggioEnabled(FMOperatorType op, bool enabled);
	bool getArpeggioEnabled(FMOperatorType op) const;
	void setArpeggioNumber(FMOperatorType op, int n);
	int getArpeggioNumber(FMOperatorType op) const;
	SequenceType getArpeggioType(FMOperatorType op) const;
	std::vector<CommandSequenceUnit> getArpeggioSequence(FMOperatorType op) const;
	std::vector<Loop> getArpeggioLoops(FMOperatorType op) const;
	Release getArpeggioRelease(FMOperatorType op) const;
	std::unique_ptr<CommandSequence::Iterator> getArpeggioSequenceIterator(FMOperatorType op) const;

	void setPitchEnabled(FMOperatorType op, bool enabled);
	bool getPitchEnabled(FMOperatorType op) const;
	void setPitchNumber(FMOperatorType op, int n);
	int getPitchNumber(FMOperatorType op) const;
	SequenceType getPitchType(FMOperatorType op) const;
	std::vector<CommandSequenceUnit> getPitchSequence(FMOperatorType op) const;
	std::vector<Loop> getPitchLoops(FMOperatorType op) const;
	Release getPitchRelease(FMOperatorType op) const;
	std::unique_ptr<CommandSequence::Iterator> getPitchSequenceIterator(FMOperatorType op) const;

	void setEnvelopeResetEnabled(FMOperatorType op, bool enabled);
	bool getEnvelopeResetEnabled(FMOperatorType op) const;

private:
	int envNum_;
	bool lfoEnabled_;
	int lfoNum_;
	std::unordered_map<FMEnvelopeParameter, bool> opSeqEnabled_;
	std::unordered_map<FMEnvelopeParameter, int> opSeqNum_;
	std::unordered_map<FMOperatorType, bool> arpEnabled_;
	std::unordered_map<FMOperatorType, int> arpNum_;
	std::unordered_map<FMOperatorType, bool> ptEnabled_;
	std::unordered_map<FMOperatorType, int> ptNum_;

	std::unordered_map<FMOperatorType, bool> envResetEnabled_;
};


class InstrumentSSG : public AbstractInstrument
{
public:
	InstrumentSSG(int number, std::string name, InstrumentsManager* owner);
	SoundSource getSoundSource() const override;
	InstrumentType getType() const override;
	AbstractInstrument* clone() override;

	void setWaveformEnabled(bool enabled);
	bool getWaveformEnabled() const;
	void setWaveformNumber(int n);
	int getWaveformNumber() const;
	std::vector<CommandSequenceUnit> getWaveformSequence() const;
	std::vector<Loop> getWaveformLoops() const;
	Release getWaveformRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getWaveformSequenceIterator() const;

	void setToneNoiseEnabled(bool enabled);
	bool getToneNoiseEnabled() const;
	void setToneNoiseNumber(int n);
	int getToneNoiseNumber() const;
	std::vector<CommandSequenceUnit> getToneNoiseSequence() const;
	std::vector<Loop> getToneNoiseLoops() const;
	Release getToneNoiseRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getToneNoiseSequenceIterator() const;

	void setEnvelopeEnabled(bool enabled);
	bool getEnvelopeEnabled() const;
	void setEnvelopeNumber(int n);
	int getEnvelopeNumber() const;
	std::vector<CommandSequenceUnit> getEnvelopeSequence() const;
	std::vector<Loop> getEnvelopeLoops() const;
	Release getEnvelopeRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getEnvelopeSequenceIterator() const;

	void setArpeggioEnabled(bool enabled);
	bool getArpeggioEnabled() const;
	void setArpeggioNumber(int n);
	int getArpeggioNumber() const;
	SequenceType getArpeggioType() const;
	std::vector<CommandSequenceUnit> getArpeggioSequence() const;
	std::vector<Loop> getArpeggioLoops() const;
	Release getArpeggioRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getArpeggioSequenceIterator() const;

	void setPitchEnabled(bool enabled);
	bool getPitchEnabled() const;
	void setPitchNumber(int n);
	int getPitchNumber() const;
	SequenceType getPitchType() const;
	std::vector<CommandSequenceUnit> getPitchSequence() const;
	std::vector<Loop> getPitchLoops() const;
	Release getPitchRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getPitchSequenceIterator() const;

private:
	bool wfEnabled_;
	int wfNum_;
	bool tnEnabled_;
	int tnNum_;
	bool envEnabled_;
	int envNum_;
	bool arpEnabled_;
	int arpNum_;
	bool ptEnabled_;
	int ptNum_;
};


class InstrumentADPCM : public AbstractInstrument
{
public:
	InstrumentADPCM(int number, std::string name, InstrumentsManager* owner);
	SoundSource getSoundSource() const override;
	InstrumentType getType() const override;
	AbstractInstrument* clone() override;

	void setSampleNumber(int n);
	int getSampleNumber() const;
	int getSampleRootKeyNumber() const;
	int getSampleRootDeltaN() const;
	bool isSampleRepeatable() const;
	std::vector<uint8_t> getRawSample() const;
	size_t getSampleStartAddress() const;
	size_t getSampleStopAddress() const;

	void setEnvelopeEnabled(bool enabled);
	bool getEnvelopeEnabled() const;
	void setEnvelopeNumber(int n);
	int getEnvelopeNumber() const;
	std::vector<CommandSequenceUnit> getEnvelopeSequence() const;
	std::vector<Loop> getEnvelopeLoops() const;
	Release getEnvelopeRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getEnvelopeSequenceIterator() const;

	void setArpeggioEnabled(bool enabled);
	bool getArpeggioEnabled() const;
	void setArpeggioNumber(int n);
	int getArpeggioNumber() const;
	SequenceType getArpeggioType() const;
	std::vector<CommandSequenceUnit> getArpeggioSequence() const;
	std::vector<Loop> getArpeggioLoops() const;
	Release getArpeggioRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getArpeggioSequenceIterator() const;

	void setPitchEnabled(bool enabled);
	bool getPitchEnabled() const;
	void setPitchNumber(int n);
	int getPitchNumber() const;
	SequenceType getPitchType() const;
	std::vector<CommandSequenceUnit> getPitchSequence() const;
	std::vector<Loop> getPitchLoops() const;
	Release getPitchRelease() const;
	std::unique_ptr<CommandSequence::Iterator> getPitchSequenceIterator() const;

private:
	int sampNum_;
	bool envEnabled_;
	int envNum_;
	bool arpEnabled_;
	int arpNum_;
	bool ptEnabled_;
	int ptNum_;
};


class InstrumentDrumkit : public AbstractInstrument
{
public:
	InstrumentDrumkit(int number, std::string name, InstrumentsManager* owner);
	SoundSource getSoundSource() const override;
	InstrumentType getType() const override;
	AbstractInstrument* clone() override;

	std::vector<int> getAssignedKeys() const;

	void setSampleEnabled(int key, bool enabled);
	bool getSampleEnabled(int key) const;
	void setSampleNumber(int key, int n);
	int getSampleNumber(int key) const;
	int getSampleRootKeyNumber(int key) const;
	int getSampleRootDeltaN(int key) const;
	bool isSampleRepeatable(int key) const;
	std::vector<uint8_t> getRawSample(int key) const;
	size_t getSampleStartAddress(int key) const;
	size_t getSampleStopAddress(int key) const;

	void setPitch(int key, int pitch);
	int getPitch(int key) const;

private:
	struct KitProperty { int sampNum, pitch; };
	std::unordered_map<int, KitProperty> kit_;
};
