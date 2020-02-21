#include "playback.hpp"
#include <algorithm>
#include "song.hpp"
#include "misc.hpp"

PlaybackManager::PlaybackManager(std::shared_ptr<OPNAController> opnaCtrl,
								 std::weak_ptr<InstrumentsManager> instMan, std::weak_ptr<TickCounter> tickCounter,
								 std::weak_ptr<Module> mod,
								 bool isRetrieveChannel)
	: opnaCtrl_(opnaCtrl),
	  instMan_(instMan),
	  tickCounter_(tickCounter),
	  mod_(mod),
	  curSongNum_(0),
	  playOrderNum_(-1),
	  playStepNum_(-1),
	  playState_(0),
	  managerState_(PlaybackState::Stop),
	  isFindNextStep_(false),
	  isRetrieveChannel_(isRetrieveChannel)
{
	songStyle_ = mod.lock()->getSong(curSongNum_).getStyle();

	clearEffectMaps();
	clearNoteDelayCounts();
	clearDelayBeyondStepCounts();
}

void PlaybackManager::setSong(std::weak_ptr<Module> mod, int songNum)
{
	mod_ = mod;
	curSongNum_ = songNum;
	songStyle_ = mod_.lock()->getSong(curSongNum_).getStyle();

	/* opna mode is changed in BambooTracker class */

	size_t fmch = getFMChannelCount(songStyle_.type);
	isNoteDelay_[SoundSource::FM] = std::vector<bool>(fmch);
	keyOnBasedEffs_[SoundSource::FM] = EffectMemorySource(fmch);
	stepBeginBasedEffs_[SoundSource::FM] = EffectMemorySource(fmch);
	ntDlyCntFM_ = std::vector<int>(fmch);
	ntCutDlyCntFM_ = std::vector<int>(fmch);
	volDlyCntFM_ = std::vector<int>(fmch);
	volDlyValueFM_ = std::vector<int>(fmch, -1);
	tposeDlyCntFM_ = std::vector<int>(fmch);
	tposeDlyValueFM_ = std::vector<int>(fmch);

	isNoteDelay_[SoundSource::SSG] = std::vector<bool>(3);
	keyOnBasedEffs_[SoundSource::SSG] = EffectMemorySource(3);
	stepBeginBasedEffs_[SoundSource::SSG] = EffectMemorySource(3);
	ntDlyCntSSG_ = std::vector<int>(3);
	ntCutDlyCntSSG_ = std::vector<int>(3);
	volDlyCntSSG_ = std::vector<int>(3);
	volDlyValueSSG_ = std::vector<int>(3, -1);
	tposeDlyCntSSG_ = std::vector<int>(3);
	tposeDlyValueSSG_ = std::vector<int>(3);

	isNoteDelay_[SoundSource::DRUM] = std::vector<bool>(6);
	keyOnBasedEffs_[SoundSource::DRUM] = EffectMemorySource(6);
	stepBeginBasedEffs_[SoundSource::DRUM] = EffectMemorySource(6);
	ntDlyCntDrum_ = std::vector<int>(6);
	ntCutDlyCntDrum_ = std::vector<int>(6);
	volDlyCntDrum_ = std::vector<int>(6);
	volDlyValueDrum_ = std::vector<int>(6, -1);

	// TODO: adpcm
	isNoteDelay_[SoundSource::ADPCM] = std::vector<bool>(1);
	keyOnBasedEffs_[SoundSource::ADPCM] = EffectMemorySource(1);
	stepBeginBasedEffs_[SoundSource::ADPCM] = EffectMemorySource(1);
//	ntDlyCntDrum_ = std::vector<int>(6);
//	ntCutDlyCntDrum_ = std::vector<int>(6);
//	volDlyCntDrum_ = std::vector<int>(6);
//	volDlyValueDrum_ = std::vector<int>(6, -1);
}

/********** Play song **********/
void PlaybackManager::startPlaySong(int order)
{
	std::lock_guard<std::mutex> lock(mutex_);

	startPlay();
	playState_ = 0x01;
	managerState_ = PlaybackState::PlaySong;
	playStepNum_ = 0;
	playOrderNum_ = order;
	findNextStep();
	if (isRetrieveChannel_) retrieveChannelStates();
}

void PlaybackManager::startPlayFromStart()
{
	std::lock_guard<std::mutex> lock(mutex_);

	startPlay();
	playState_ = 0x01;
	managerState_ = PlaybackState::PlayFromStart;
	playOrderNum_ = 0;
	playStepNum_ = 0;
	findNextStep();
}

void PlaybackManager::startPlayPattern(int order)
{
	std::lock_guard<std::mutex> lock(mutex_);

	startPlay();
	playState_ = 0x11;
	managerState_ = PlaybackState::PlayPattern;
	playStepNum_ = 0;
	playOrderNum_ = order;
	findNextStep();
	if (isRetrieveChannel_) retrieveChannelStates();
}

void PlaybackManager::startPlayFromCurrentStep(int order, int step)
{
	std::lock_guard<std::mutex> lock(mutex_);

	startPlay();
	playState_ = 0x01;
	managerState_ = PlaybackState::PlayFromCurrentStep;
	playOrderNum_ = order;
	playStepNum_ = step;
	findNextStep();
	if (isRetrieveChannel_) retrieveChannelStates();
}

void PlaybackManager::startPlay()
{
	opnaCtrl_->reset();

	Song& song = mod_.lock()->getSong(curSongNum_);
	tickCounter_.lock()->setTempo(song.getTempo());
	tickCounter_.lock()->setSpeed(song.getSpeed());
	tickCounter_.lock()->setGroove(mod_.lock()->getGroove(song.getGroove()).getSequence());
	tickCounter_.lock()->setGrooveTrigger(song.isUsedTempo() ? GrooveTrigger::Invalid
															 : GrooveTrigger::ValidByGlobal);
	tickCounter_.lock()->resetCount();
	tickCounter_.lock()->setPlayState(true);

	clearEffectMaps();
	clearNoteDelayCounts();
	clearDelayBeyondStepCounts();
}

void PlaybackManager::stopPlaySong()
{
	std::lock_guard<std::mutex> lock(mutex_);
	stopPlay();
}

void PlaybackManager::stopPlay()
{
	// No mutex to call from PlaybackManager::streamCountUp
	opnaCtrl_->reset();

	tickCounter_.lock()->setPlayState(false);
	playState_ = 0;
	managerState_ = PlaybackState::Stop;
	playOrderNum_ = -1;
	playStepNum_ = -1;
}

bool PlaybackManager::isPlaySong() const
{
	return ((playState_ & 0x01) > 0);
}

PlaybackState PlaybackManager::getPlaybackState() const
{
	return managerState_;
}

int PlaybackManager::getPlayingOrderNumber() const
{
	return playOrderNum_;
}

int PlaybackManager::getPlayingStepNumber() const
{
	return playStepNum_;
}

/********** Stream events **********/
int PlaybackManager::streamCountUp()
{
	std::lock_guard<std::mutex> lock(mutex_);

	int state = tickCounter_.lock()->countUp();

	if (state > 0) {
		checkValidPosition();
		tickProcess(state);
	}
	else if (!state) {
		checkValidPosition();
		if (stepDown()) {
			stepProcess();
			if (!isFindNextStep_) findNextStep();
		}
		else {
			stopPlay();
		}
	}
	else {
		for (auto& attrib : songStyle_.trackAttribs) {
			opnaCtrl_->tickEvent(attrib.source, attrib.channelInSource);
		}
	}

	return state;
}

bool PlaybackManager::stepDown()
{
	if (playState_ & 0x02) {	// Foward current step
		if (nextReadOrder_ == -1) {
			isFindNextStep_ = false;
			return false;
		}
		else {
			playOrderNum_ = nextReadOrder_;
			playStepNum_ = nextReadStep_;
		}
	}
	else {	// First read
		playState_ |= 0x02;
	}

	return true;
}

void PlaybackManager::findNextStep()
{
	// Init
	nextReadOrder_ = playOrderNum_;
	nextReadStep_ = playStepNum_;

	// Search
	int ptnSize = static_cast<int>(getPatternSizeFromOrderNumber(curSongNum_, nextReadOrder_));
	if (!ptnSize || nextReadStep_ >= ptnSize - 1) {
		if (!(playState_ & 0x10)) {	// Not play pattern
			if (nextReadOrder_ >= static_cast<int>(getOrderSize(curSongNum_)) - 1) {
				nextReadOrder_ = 0;
			}
			else {
				++nextReadOrder_;
			}
		}
		nextReadStep_ = 0;
	}
	else {
		++nextReadStep_;
	}

	isFindNextStep_ = true;
}

void PlaybackManager::checkValidPosition()
{
	auto& song = mod_.lock()->getSong(curSongNum_);
	int orderSize = static_cast<int>(song.getOrderSize());
	if (playOrderNum_ >= orderSize) {
		playOrderNum_ = 0;
		playStepNum_ = 0;
		nextReadOrder_ = 0;
		nextReadStep_ = 0;
	}
	else if (playStepNum_ >= static_cast<int>(song.getPatternSizeFromOrderNumber(playOrderNum_))) {
		if (playOrderNum_ == orderSize - 1) {
			playOrderNum_ = 0;
			playStepNum_ = 0;
			nextReadOrder_ = 0;
			nextReadStep_ = 0;
		}
		else {
			++playOrderNum_;
			playStepNum_ = 0;
			nextReadOrder_ = playOrderNum_;
			nextReadStep_ = 0;
		}
	}
}

/// Register update order: volume -> instrument -> effect -> key on
void PlaybackManager::stepProcess()
{
	clearNoteDelayCounts();
	updateDelayEventCounts();

	auto& song = mod_.lock()->getSong(curSongNum_);

	// Store effects from the step to map
	for (auto& attrib : songStyle_.trackAttribs) {
		auto& step = song.getTrack(attrib.number)
					 .getPatternFromOrderNumber(playOrderNum_).getStep(playStepNum_);
		size_t uch = static_cast<size_t>(attrib.channelInSource);
		keyOnBasedEffs_[attrib.source].at(uch).clear();
		bool isDelay = false;
		bool (PlaybackManager::*storeEffectToMap)(int, Effect) = nullptr;
		switch (attrib.source) {
		case SoundSource::FM:	storeEffectToMap = &PlaybackManager::storeEffectToMapFM;	break;
		case SoundSource::SSG:	storeEffectToMap = &PlaybackManager::storeEffectToMapSSG;	break;
		case SoundSource::DRUM:	storeEffectToMap = &PlaybackManager::storeEffectToMapDrum;	break;
		case SoundSource::ADPCM:	/* TODO: adpcm */	break;
		}
		for (int i = 0; i < 4; ++i) {
			Effect&& eff = Effect::makeEffectData(attrib.source, step.getEffectID(i), step.getEffectValue(i));
			if (attrib.source != SoundSource::ADPCM)
			isDelay |= (this->*storeEffectToMap)(attrib.channelInSource, std::move(eff));
		}
		isNoteDelay_[attrib.source].at(uch) = isDelay;
	}

	// Execute step events
	bool isNextSet = executeStoredEffectsGlobal();
	for (auto& attrib : songStyle_.trackAttribs) {
		auto& step = song.getTrack(attrib.number)
					 .getPatternFromOrderNumber(playOrderNum_).getStep(playStepNum_);
		switch (attrib.source) {
		case SoundSource::FM:
			if (isNoteDelay_[SoundSource::FM].at(attrib.channelInSource)) {
				// Set effect
				executeStoredEffectsFM(attrib.channelInSource);
				checkFMNoteDelayAndEnvelopeReset(step, attrib.channelInSource);
				opnaCtrl_->tickEvent(SoundSource::FM, attrib.channelInSource);
			}
			else {
				executeFMStepEvents(step, attrib.channelInSource);
			}
			break;
		case SoundSource::SSG:
			if (isNoteDelay_[SoundSource::SSG].at(attrib.channelInSource)) {
				// Set effect
				executeStoredEffectsSSG(attrib.channelInSource);
				opnaCtrl_->tickEvent(SoundSource::SSG, attrib.channelInSource);
			}
			else {
				executeSSGStepEvents(step, attrib.channelInSource);
			}
			break;
		case SoundSource::DRUM:
			if (isNoteDelay_[SoundSource::DRUM].at(attrib.channelInSource)) {
				// Set effect
				executeStoredEffectsDrum(attrib.channelInSource);
				opnaCtrl_->tickEvent(SoundSource::DRUM, attrib.channelInSource);
			}
			else {
				executeDrumStepEvents(step, attrib.channelInSource);
			}
			break;
		case SoundSource::ADPCM:
			// TODO: adpcm
			break;
		}
	}
	opnaCtrl_->updateRegisterStates();	// Update for other changes

	isFindNextStep_ = isNextSet;
}

void PlaybackManager::executeFMStepEvents(Step& step, int ch, bool calledByNoteDelay)
{
	int noteNum = step.getNoteNumber();
	if (!calledByNoteDelay && noteNum != -1) clearFMDelayBeyondStepCounts(ch);	// Except no key event

	// Set volume
	int vol = step.getVolume();
	if (0 <= vol && vol < 0x80) {
		opnaCtrl_->setVolumeFM(ch, vol);
	}

	// Set instrument
	if (step.getInstrumentNumber() != -1) {
		if (auto inst = std::dynamic_pointer_cast<InstrumentFM>(
					instMan_.lock()->getInstrumentSharedPtr(step.getInstrumentNumber())))
			opnaCtrl_->setInstrumentFM(ch, inst);
	}
	else {
		opnaCtrl_->restoreFMEnvelopeFromReset(ch);
	}

	// Set effect
	executeStoredEffectsFM(ch);

	// Set key
	switch (noteNum) {
	case -1:	// None
		if (!calledByNoteDelay) {	// When this is called by note delay, tick event will be updated in readTick
			checkFMDelayEventsInTick(step, ch);
			opnaCtrl_->tickEvent(SoundSource::FM, ch);
		}
		break;
	case -2:	// Key off
		opnaCtrl_->keyOffFM(ch);
		break;
	case -3:	// Echo 0
		opnaCtrl_->keyOnFM(ch, 0);
		break;
	case -4:	// Echo 1
		opnaCtrl_->keyOnFM(ch, 1);
		break;
	case -5:	// Echo 2
		opnaCtrl_->keyOnFM(ch, 2);
		break;
	case -6:	// Echo 3
		opnaCtrl_->keyOnFM(ch, 3);
		break;
	default:	// Key on
	{
		std::pair<int, Note> octNote = noteNumberToOctaveAndNote(step.getNoteNumber());
		opnaCtrl_->keyOnFM(ch, octNote.second, octNote.first, 0);
		break;
	}
	}
}

void PlaybackManager::executeSSGStepEvents(Step& step, int ch, bool calledByNoteDelay)
{
	int noteNum = step.getNoteNumber();
	if (!calledByNoteDelay && noteNum != -1) clearSSGDelayBeyondStepCounts(ch);	// Except no key event

	// Set volume
	int vol = step.getVolume();
	if (0 <= vol && vol < 0x10) {
		opnaCtrl_->setVolumeSSG(ch, vol);
	}

	// Set instrument
	if (step.getInstrumentNumber() != -1) {
		if (auto inst = std::dynamic_pointer_cast<InstrumentSSG>(
					instMan_.lock()->getInstrumentSharedPtr(step.getInstrumentNumber())))
			opnaCtrl_->setInstrumentSSG(ch, inst);
	}

	// Set effect
	executeStoredEffectsSSG(ch);

	// Set key
	switch (noteNum) {
	case -1:	// None
		if (!calledByNoteDelay) {	// When this is called by note delay, tick event will be updated in readTick
			checkSSGDelayEventsInTick(step, ch);
			opnaCtrl_->tickEvent(SoundSource::SSG, ch);
		}
		break;
	case -2:	// Key off
		opnaCtrl_->keyOffSSG(ch);
		break;
	case -3:	// Echo 0
		opnaCtrl_->keyOnSSG(ch, 0);
		break;
	case -4:	// Echo 1
		opnaCtrl_->keyOnSSG(ch, 1);
		break;
	case -5:	// Echo 2
		opnaCtrl_->keyOnSSG(ch, 2);
		break;
	case -6:	// Echo 3
		opnaCtrl_->keyOnSSG(ch, 3);
		break;
	default:	// Key on
	{
		std::pair<int, Note> octNote = noteNumberToOctaveAndNote(step.getNoteNumber());
		opnaCtrl_->keyOnSSG(ch, octNote.second, octNote.first, 0);
		break;
	}
	}
}

void PlaybackManager::executeDrumStepEvents(Step& step, int ch, bool calledByNoteDelay)
{
	int noteNum = step.getNoteNumber();
	if (!calledByNoteDelay && noteNum != -1) clearDrumDelayBeyondStepCounts(ch);	// Except no key event

	// Set volume
	int vol = step.getVolume();
	if (0 <= vol && vol < 0x20) {
		opnaCtrl_->setVolumeDrum(ch, vol);
	}

	// Set effect
	executeStoredEffectsDrum(ch);

	// Set key
	switch (noteNum) {
	case -1:	// None
		if (!calledByNoteDelay) {	// When this is called by note delay, tick event will be updated in readTick
			checkDrumDelayEventsInTick(step, ch);
			opnaCtrl_->tickEvent(SoundSource::DRUM, ch);
		}
		break;
	case -2:	// Key off
		opnaCtrl_->setKeyOffFlagDrum(ch);
		break;
	default:	// Key on & Echo
		opnaCtrl_->setKeyOnFlagDrum(ch);
		break;
	}
}

bool PlaybackManager::executeStoredEffectsGlobal()
{
	bool changedNextPos = false;

	// Read step end based effects
	for (const auto& eff : stepEndBasedEffsGlobal_) {
		switch (eff.first) {
		case EffectType::PositionJump:
			changedNextPos |= effPositionJump(eff.second);
			break;
		case EffectType::SongEnd:
			effSongEnd();
			changedNextPos = true;
			break;
		case EffectType::PatternBreak:
			changedNextPos |= effPatternBreak(eff.second);
			break;
		default:
			break;
		}
	}
	stepEndBasedEffsGlobal_.clear();

	// Read step beginning based effects
	auto&& it = stepBeginBasedEffsGlobal_.find(EffectType::SpeedTempoChange);
	if (it != stepBeginBasedEffsGlobal_.end()) {
		if (it->second < 0x20) effSpeedChange(it->second);
		else effTempoChange(it->second);
	}
	for (const auto& eff : stepBeginBasedEffsGlobal_) {
		switch (eff.first) {
		case EffectType::Groove:
			if (eff.second < static_cast<int>(mod_.lock()->getGrooveCount()))
				effGrooveChange(eff.second);
			break;
		default:
			break;
		}
	}
	stepBeginBasedEffsGlobal_.clear();

	return changedNextPos;
}

bool PlaybackManager::storeEffectToMapFM(int ch, Effect eff)
{
	switch (eff.type) {
	case EffectType::Arpeggio:
	case EffectType::PortamentoUp:
	case EffectType::PortamentoDown:
	case EffectType::TonePortamento:
	case EffectType::Vibrato:
	case EffectType::Tremolo:
	case EffectType::Pan:
	case EffectType::VolumeSlide:
	case EffectType::Detune:
	case EffectType::NoteSlideUp:
	case EffectType::NoteSlideDown:
	case EffectType::NoteCut:
	case EffectType::TransposeDelay:
	case EffectType::VolumeDelay:
	case EffectType::FBControl:
	case EffectType::TLControl:
	case EffectType::MLControl:
	case EffectType::ARControl:
	case EffectType::DRControl:
	case EffectType::RRControl:
	case EffectType::RegisterAddress0:
	case EffectType::RegisterAddress1:
	case EffectType::RegisterValue:
	case EffectType::Brightness:
		keyOnBasedEffs_[SoundSource::FM].at(static_cast<size_t>(ch))[eff.type] = eff.value;
		return false;
	case EffectType::SpeedTempoChange:
	case EffectType::Groove:
		stepBeginBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	case EffectType::NoteDelay:
		if (eff.value < tickCounter_.lock()->getSpeed()) {
			stepBeginBasedEffs_[SoundSource::FM].at(static_cast<size_t>(ch))[EffectType::NoteDelay] = eff.value;
			return true;
		}
		return false;
	case EffectType::PositionJump:
	case EffectType::SongEnd:
	case EffectType::PatternBreak:
		stepEndBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	default:
		return false;
	}
}

void PlaybackManager::executeStoredEffectsFM(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	bool isNoteDelay = false;

	// Read step beginning based effects
	auto& stepBeginBasedEffs = stepBeginBasedEffs_[SoundSource::FM].at(uch);
	for (const auto& eff : stepBeginBasedEffs) {
		switch (eff.first) {
		case EffectType::NoteDelay:
			ntDlyCntFM_.at(uch) = eff.second;
			isNoteDelay = true;
			break;
		default:
			break;
		}
	}
	stepBeginBasedEffs.clear();

	// Read note on and step beginning based effects
	if (!isNoteDelay) {
		auto& keyOnBasedEffs = keyOnBasedEffs_[SoundSource::FM].at(uch);
		for (auto& eff : keyOnBasedEffs) {
			switch (eff.first) {
			case EffectType::Arpeggio:
				opnaCtrl_->setArpeggioEffectFM(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::PortamentoUp:
				opnaCtrl_->setPortamentoEffectFM(ch, eff.second);
				break;
			case EffectType::PortamentoDown:
				opnaCtrl_->setPortamentoEffectFM(ch, -eff.second);
				break;
			case EffectType::TonePortamento:
				opnaCtrl_->setPortamentoEffectFM(ch, eff.second, true);
				break;
			case EffectType::Vibrato:
				opnaCtrl_->setVibratoEffectFM(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::Tremolo:
				opnaCtrl_->setTremoloEffectFM(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::Pan:
				if (-1 < eff.second && eff.second < 4) opnaCtrl_->setPanFM(ch, eff.second);
				break;
			case EffectType::VolumeSlide:
			{
				int hi = eff.second >> 4;
				int low = eff.second & 0x0f;
				if (hi && !low) opnaCtrl_->setVolumeSlideFM(ch, hi, true);	// Slide up
				else if (!hi) opnaCtrl_->setVolumeSlideFM(ch, low, false);	// Slide down
				break;
			}
			case EffectType::Detune:
				opnaCtrl_->setDetuneFM(ch, eff.second - 0x80);
				break;
			case EffectType::NoteSlideUp:
				opnaCtrl_->setNoteSlideFM(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::NoteSlideDown:
				opnaCtrl_->setNoteSlideFM(ch, eff.second >> 4, -(eff.second & 0x0f));
				break;
			case EffectType::NoteCut:
				ntCutDlyCntFM_.at(uch) = eff.second;
				break;
			case EffectType::TransposeDelay:
				tposeDlyCntFM_.at(uch) = (eff.second & 0x70) >> 4;
				tposeDlyValueFM_.at(uch) = ((eff.second & 0x80) ? -1 : 1) * (eff.second & 0x0f);
				break;
			case EffectType::VolumeDelay:
			{
				int count = eff.second >> 8;
				if (count > 0) {
					volDlyCntFM_.at(uch) = count;
					volDlyValueFM_.at(uch) = eff.second & 0x00ff;
				}
				break;
			}
			case EffectType::FBControl:
				if (-1 < eff.second && eff.second < 8) opnaCtrl_->setFBControlFM(ch, eff.second);
				break;
			case EffectType::TLControl:
			{
				int op = eff.second >> 8;
				int val = eff.second & 0x00ff;
				if (0 < op && op < 5 && -1 < val && val < 128) opnaCtrl_->setTLControlFM(ch, op - 1, val);
				break;
			}
			case EffectType::MLControl:
			{
				int op = eff.second >> 4;
				int val = eff.second & 0x0f;
				if (0 < op && op < 5 && -1 < val && val < 16) opnaCtrl_->setMLControlFM(ch, op - 1, val);
				break;
			}
			case EffectType::ARControl:
			{
				int op = eff.second >> 8;
				int val = eff.second & 0x00ff;
				if (0 < op && op < 5 && -1 < val && val < 32) opnaCtrl_->setARControlFM(ch, op - 1, val);
				break;
			}
			case EffectType::DRControl:
			{
				int op = eff.second >> 8;
				int val = eff.second & 0x00ff;
				if (0 < op && op < 5 && -1 < val && val < 32) opnaCtrl_->setDRControlFM(ch, op - 1, val);
				break;
			}
			case EffectType::RRControl:
			{
				int op = eff.second >> 4;
				int val = eff.second & 0x0f;
				if (0 < op && op < 5 && -1 < val && val < 16) opnaCtrl_->setRRControlFM(ch, op - 1, val);
				break;
			}
			case EffectType::RegisterAddress0:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(0, eff.second);
				break;
			}
			case EffectType::RegisterAddress1:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(1, eff.second);
				break;
			}
			case EffectType::RegisterValue:
			{
				if (-1 < eff.second) opnaCtrl_->sendRegisterValue(eff.second);
				break;
			}
			case EffectType::Brightness:
			{
				if (0 < eff.second) opnaCtrl_->setBrightnessFM(ch, eff.second - 0x80);
				break;
			}
			default:
				break;
			}
		}
		keyOnBasedEffs.clear();
	}
}

bool PlaybackManager::storeEffectToMapSSG(int ch, Effect eff)
{
	switch (eff.type) {
	case EffectType::Arpeggio:
	case EffectType::PortamentoUp:
	case EffectType::PortamentoDown:
	case EffectType::TonePortamento:
	case EffectType::Vibrato:
	case EffectType::Tremolo:
	case EffectType::VolumeSlide:
	case EffectType::Detune:
	case EffectType::NoteSlideUp:
	case EffectType::NoteSlideDown:
	case EffectType::NoteCut:
	case EffectType::TransposeDelay:
	case EffectType::ToneNoiseMix:
	case EffectType::NoisePitch:
	case EffectType::VolumeDelay:
	case EffectType::HardEnvHighPeriod:
	case EffectType::HardEnvLowPeriod:
	case EffectType::AutoEnvelope:
	case EffectType::RegisterAddress0:
	case EffectType::RegisterAddress1:
	case EffectType::RegisterValue:
		keyOnBasedEffs_[SoundSource::SSG].at(static_cast<size_t>(ch))[eff.type] = eff.value;
		return false;
	case EffectType::SpeedTempoChange:
	case EffectType::Groove:
		stepBeginBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	case EffectType::NoteDelay:
		if (eff.value < tickCounter_.lock()->getSpeed()) {
			stepBeginBasedEffs_[SoundSource::SSG].at(static_cast<size_t>(ch))[EffectType::NoteDelay] = eff.value;
			return true;
		}
		return false;
	case EffectType::PositionJump:
	case EffectType::SongEnd:
	case EffectType::PatternBreak:
		stepEndBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	default:
		return false;
	}
}

void PlaybackManager::executeStoredEffectsSSG(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	bool isNoteDelay = false;

	// Read step beginning based effects
	auto& stepBeginBasedEffs = stepBeginBasedEffs_[SoundSource::SSG].at(uch);
	for (const auto& eff : stepBeginBasedEffs) {
		switch (eff.first) {
		case EffectType::NoteDelay:
			ntDlyCntSSG_.at(uch) = eff.second;
			isNoteDelay = true;
			break;
		default:
			break;
		}
	}
	stepBeginBasedEffs.clear();

	// Read note on and step beginning based effects
	if (!isNoteDelay) {
		auto& keyOnBasedEffs = keyOnBasedEffs_[SoundSource::SSG].at(uch);
		for (const auto& eff : keyOnBasedEffs) {
			switch (eff.first) {
			case EffectType::Arpeggio:
				opnaCtrl_->setArpeggioEffectSSG(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::PortamentoUp:
				opnaCtrl_->setPortamentoEffectSSG(ch, eff.second);
				break;
			case EffectType::PortamentoDown:
				opnaCtrl_->setPortamentoEffectSSG(ch, -eff.second);
				break;
			case EffectType::TonePortamento:
				opnaCtrl_->setPortamentoEffectSSG(ch, eff.second, true);
				break;
			case EffectType::Vibrato:
				opnaCtrl_->setVibratoEffectSSG(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::Tremolo:
				opnaCtrl_->setTremoloEffectSSG(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::VolumeSlide:
			{
				int hi = eff.second >> 4;
				int low = eff.second & 0x0f;
				if (hi && !low) opnaCtrl_->setVolumeSlideSSG(ch, hi, true);	// Slide up
				else if (!hi) opnaCtrl_->setVolumeSlideSSG(ch, low, false);	// Slide down
				break;
			}
			case EffectType::Detune:
				opnaCtrl_->setDetuneSSG(ch, eff.second - 0x80);
				break;
			case EffectType::NoteSlideUp:
				opnaCtrl_->setNoteSlideSSG(ch, eff.second >> 4, eff.second & 0x0f);
				break;
			case EffectType::NoteSlideDown:
				opnaCtrl_->setNoteSlideSSG(ch, eff.second >> 4, -(eff.second & 0x0f));
				break;
			case EffectType::NoteCut:
				ntCutDlyCntSSG_.at(uch) = eff.second;
				break;
			case EffectType::TransposeDelay:
				tposeDlyCntSSG_.at(uch) = (eff.second & 0x70) >> 4;
				tposeDlyValueSSG_.at(uch) = ((eff.second & 0x80) ? -1 : 1) * (eff.second & 0x0f);
				break;
			case EffectType::ToneNoiseMix:
				if (-1 < eff.second && eff.second < 4) opnaCtrl_->setToneNoiseMixSSG(ch, eff.second);
				break;
			case EffectType::NoisePitch:
				if (-1 < eff.second && eff.second < 32) opnaCtrl_->setNoisePitchSSG(ch, eff.second);
				break;
			case EffectType::HardEnvHighPeriod:
				opnaCtrl_->setHardEnvelopePeriod(ch, true, eff.second);
				break;
			case EffectType::HardEnvLowPeriod:
				opnaCtrl_->setHardEnvelopePeriod(ch, false, eff.second);
				break;
			case EffectType::VolumeDelay:
			{
				int count = eff.second >> 8;
				if (count > 0) {
					volDlyCntSSG_.at(uch) = count;
					volDlyValueSSG_.at(uch) = eff.second & 0x00ff;
				}
				break;
			}
			case EffectType::AutoEnvelope:
				opnaCtrl_->setAutoEnvelopeSSG(ch, (eff.second >> 4) - 8, eff.second & 0x0f);
				break;
			case EffectType::RegisterAddress0:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(0, eff.second);
				break;
			}
			case EffectType::RegisterAddress1:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(1, eff.second);
				break;
			}
			case EffectType::RegisterValue:
			{
				if (-1 < eff.second) opnaCtrl_->sendRegisterValue(eff.second);
				break;
			}
			default:
				break;
			}
		}
		keyOnBasedEffs.clear();
	}
}

bool PlaybackManager::storeEffectToMapDrum(int ch, Effect eff)
{
	switch (eff.type) {
	case EffectType::Pan:
	case EffectType::NoteCut:
	case EffectType::MasterVolume:
	case EffectType::VolumeDelay:
	case EffectType::RegisterAddress0:
	case EffectType::RegisterAddress1:
	case EffectType::RegisterValue:
		keyOnBasedEffs_[SoundSource::DRUM].at(static_cast<size_t>(ch))[eff.type] = eff.value;
		return false;
	case EffectType::SpeedTempoChange:
	case EffectType::Groove:
		stepBeginBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	case EffectType::NoteDelay:
		if (eff.value < tickCounter_.lock()->getSpeed()) {
			stepBeginBasedEffs_[SoundSource::DRUM].at(static_cast<size_t>(ch))[EffectType::NoteDelay] = eff.value;
			return true;
		}
		return false;
	case EffectType::PositionJump:
	case EffectType::SongEnd:
	case EffectType::PatternBreak:
		stepEndBasedEffsGlobal_[eff.type] = eff.value;
		return false;
	default:
		return false;
	}
}

void PlaybackManager::executeStoredEffectsDrum(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	bool isNoteDelay = false;

	// Read step beginning based effects
	auto& stepBeginBasedEffs = stepBeginBasedEffs_[SoundSource::DRUM].at(uch);
	for (const auto& eff : stepBeginBasedEffs) {
		switch (eff.first) {
		case EffectType::NoteDelay:
			ntDlyCntDrum_.at(uch) = eff.second;
			isNoteDelay = true;
			break;
		default:
			break;
		}
	}
	stepBeginBasedEffs.clear();

	// Read key on and step beginning based effects
	if (!isNoteDelay) {
		auto& keyOnBasedEffs = keyOnBasedEffs_[SoundSource::DRUM].at(uch);
		for (const auto& eff : keyOnBasedEffs) {
			switch (eff.first) {
			case EffectType::Pan:
				if (-1 < eff.second && eff.second < 4) opnaCtrl_->setPanDrum(ch, eff.second);
				break;
			case EffectType::NoteCut:
				ntCutDlyCntDrum_.at(uch) = eff.second;
				break;
			case EffectType::MasterVolume:
				if (-1 < eff.second && eff.second < 64) opnaCtrl_->setMasterVolumeDrum(eff.second);
				break;
			case EffectType::VolumeDelay:
			{
				int count = eff.second >> 8;
				if (count > 0) {
					volDlyCntDrum_.at(uch) = count;
					volDlyValueDrum_.at(uch) = eff.second & 0x00ff;
				}
				break;
			}
			case EffectType::RegisterAddress0:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(0, eff.second);
				break;
			}
			case EffectType::RegisterAddress1:
			{
				if (-1 < eff.second && eff.second < 0x6c) opnaCtrl_->sendRegisterAddress(1, eff.second);
				break;
			}
			case EffectType::RegisterValue:
			{
				if (-1 < eff.second) opnaCtrl_->sendRegisterValue(eff.second);
				break;
			}
			default:
				break;
			}
		}
		keyOnBasedEffs.clear();
	}
}

bool PlaybackManager::effPositionJump(int nextOrder)
{
	if (nextOrder < static_cast<int>(getOrderSize(curSongNum_))) {
		nextReadOrder_ = nextOrder;
		nextReadStep_ = 0;
		return true;
	}
	return false;
}

void PlaybackManager::effSongEnd()
{
	nextReadOrder_ = -1;
	nextReadStep_ = -1;
}

bool PlaybackManager::effPatternBreak(int nextStep)
{
	if (playOrderNum_ == static_cast<int>(getOrderSize(curSongNum_)) - 1
			&& nextStep < static_cast<int>(getPatternSizeFromOrderNumber(curSongNum_, 0))) {
		nextReadOrder_ = 0;
		nextReadStep_ = nextStep;
		return true;
	}
	else if (nextStep < static_cast<int>(getPatternSizeFromOrderNumber(curSongNum_, playOrderNum_ + 1))) {
		nextReadOrder_ = playOrderNum_ + 1;
		nextReadStep_ = nextStep;
		return true;
	}
	return false;
}

void PlaybackManager::effSpeedChange(int speed)
{
	tickCounter_.lock()->setSpeed(speed ? speed : 1);
	tickCounter_.lock()->setGrooveTrigger(GrooveTrigger::Invalid);
}

void PlaybackManager::effTempoChange(int tempo)
{
	tickCounter_.lock()->setTempo(tempo);
	tickCounter_.lock()->setGrooveTrigger(GrooveTrigger::Invalid);
}

void PlaybackManager::effGrooveChange(int num)
{
	tickCounter_.lock()->setGroove(mod_.lock()->getGroove(num).getSequence());
	tickCounter_.lock()->setGrooveTrigger(GrooveTrigger::ValidByLocal);
}

void PlaybackManager::tickProcess(int rest)
{
	if (!(playState_ & 0x02)) return;	// When it has not read first step

	updateDelayEventCounts();

	auto& song = mod_.lock()->getSong(curSongNum_);
	for (auto& attrib : songStyle_.trackAttribs) {
		auto& curStep = song.getTrack(attrib.number)
						.getPatternFromOrderNumber(playOrderNum_).getStep(playStepNum_);
		int ch = attrib.channelInSource;
		switch (attrib.source) {
		case SoundSource::FM:		checkFMDelayEventsInTick(curStep, ch);		break;
		case SoundSource::SSG:		checkSSGDelayEventsInTick(curStep, ch);		break;
		case SoundSource::DRUM:		checkDrumDelayEventsInTick(curStep, ch);	break;
		case SoundSource::ADPCM:	/* TODO: adpcm */	break;
		}

		if (rest == 1 && nextReadOrder_ != -1 && attrib.source == SoundSource::FM) {
			// Channel envelope reset before next key on
			auto& step = song.getTrack(attrib.number)
						 .getPatternFromOrderNumber(nextReadOrder_).getStep(nextReadStep_);
			for (int i = 0; i < 4; ++i) {
				auto&& eff = Effect::makeEffectData(attrib.source, step.getEffectID(i), step.getEffectValue(i));
				if (eff.type == EffectType::NoteDelay && eff.value > 0) {	// Note delay check
					opnaCtrl_->tickEvent(attrib.source, ch);
					return;
				}
			}
			envelopeResetEffectFM(step, ch);
		}
		else {
			opnaCtrl_->tickEvent(attrib.source, ch);
		}
	}
	opnaCtrl_->updateRegisterStates();
}

void PlaybackManager::checkFMDelayEventsInTick(Step& step, int ch)
{
	size_t uch = static_cast<size_t>(ch);
	// Check volume delay
	if (!volDlyCntFM_.at(uch))
		opnaCtrl_->setTemporaryVolumeFM(ch, volDlyValueFM_.at(uch));
	// Check note cut
	if (!ntCutDlyCntFM_.at(uch))
		opnaCtrl_->keyOffFM(ch);
	// Check transpose delay
	if (!tposeDlyCntFM_.at(uch))
		opnaCtrl_->setTransposeEffectFM(ch, tposeDlyValueFM_.at(uch));
	// Check note delay and envelope reset
	checkFMNoteDelayAndEnvelopeReset(step, ch);
}

void PlaybackManager::checkFMNoteDelayAndEnvelopeReset(Step& step, int ch)
{
	int cnt = ntDlyCntFM_.at(static_cast<size_t>(ch));
	if (!cnt) {
		executeFMStepEvents(step, ch, true);
	}
	else if (cnt == 1) {
		// Channel envelope reset before next key on
		envelopeResetEffectFM(step, ch);
	}
}

void PlaybackManager::envelopeResetEffectFM(Step& step, int ch)
{
	int n = step.getNoteNumber();
	if ((n >= 0 || n < -2)
			&& opnaCtrl_->enableFMEnvelopeReset(ch)) {	// Key on or echo buffer access
		for (int i = 0; i < 4; ++i) {
			auto&& eff = Effect::makeEffectData(	// "SoundSource::FM" is dummy
							 SoundSource::FM, step.getEffectID(i), step.getEffectValue(i));
			if (eff.type == EffectType::TonePortamento) {
				if (eff.value) opnaCtrl_->tickEvent(SoundSource::FM, ch);
				else opnaCtrl_->resetFMChannelEnvelope(ch);
				return;
			}
		}
		if (opnaCtrl_->isTonePortamentoFM(ch)) opnaCtrl_->tickEvent(SoundSource::FM, ch);
		else opnaCtrl_->resetFMChannelEnvelope(ch);
	}
	else {
		opnaCtrl_->tickEvent(SoundSource::FM, ch);
	}
}

void PlaybackManager::checkSSGDelayEventsInTick(Step& step, int ch)
{
	size_t uch = static_cast<size_t>(ch);
	// Check volume delay
	if (!volDlyCntSSG_.at(uch))
		opnaCtrl_->setTemporaryVolumeSSG(ch, volDlyValueSSG_.at(uch));
	// Check note cut
	if (!ntCutDlyCntSSG_.at(uch))
		opnaCtrl_->keyOffSSG(ch);
	// Check transpose delay
	if (!tposeDlyCntSSG_.at(uch))
		opnaCtrl_->setTransposeEffectSSG(ch, tposeDlyValueSSG_.at(uch));
	// Check note delay
	if (!ntDlyCntSSG_.at(uch))
		executeSSGStepEvents(step, ch, true);
}

void PlaybackManager::checkDrumDelayEventsInTick(Step& step, int ch)
{
	size_t uch = static_cast<size_t>(ch);
	// Check volume delay
	if (!volDlyCntDrum_.at(uch))
		opnaCtrl_->setTemporaryVolumeDrum(ch, volDlyValueDrum_.at(uch));
	// Check note cut
	if (!ntCutDlyCntDrum_.at(uch))
		opnaCtrl_->setKeyOnFlagDrum(ch);
	// Check note delay
	if (!ntDlyCntDrum_.at(uch))
		executeDrumStepEvents(step, ch, true);
}

void PlaybackManager::clearEffectMaps()
{
	stepBeginBasedEffsGlobal_.clear();
	stepEndBasedEffsGlobal_.clear();

	for (auto& maps: keyOnBasedEffs_) {
		for (EffectMemory& map : maps.second) map.clear();
	}
	for (auto& maps: stepBeginBasedEffs_) {
		for (EffectMemory& map : maps.second) map.clear();
	}
}

void PlaybackManager::clearNoteDelayCounts()
{
	std::fill(ntDlyCntFM_.begin(), ntDlyCntFM_.end(), -1);
	std::fill(ntDlyCntSSG_.begin(), ntDlyCntSSG_.end(), -1);
	std::fill(ntDlyCntDrum_.begin(), ntDlyCntDrum_.end(), -1);
}

void PlaybackManager::clearDelayBeyondStepCounts()
{
	std::fill(ntCutDlyCntFM_.begin(), ntCutDlyCntFM_.end(), -1);
	std::fill(volDlyCntFM_.begin(), volDlyCntFM_.end(), -1);
	std::fill(volDlyValueFM_.begin(), volDlyValueFM_.end(), -1);
	std::fill(tposeDlyCntFM_.begin(), tposeDlyCntFM_.end(), -1);
	std::fill(tposeDlyValueFM_.begin(), tposeDlyValueFM_.end(), 0);

	std::fill(ntCutDlyCntSSG_.begin(), ntCutDlyCntSSG_.end(), -1);
	std::fill(volDlyCntSSG_.begin(), volDlyCntSSG_.end(), -1);
	std::fill(volDlyValueSSG_.begin(), volDlyValueSSG_.end(), -1);
	std::fill(tposeDlyCntSSG_.begin(), tposeDlyCntSSG_.end(), -1);
	std::fill(tposeDlyValueSSG_.begin(), tposeDlyValueSSG_.end(), 0);

	std::fill(ntCutDlyCntDrum_.begin(), ntCutDlyCntDrum_.end(), -1);
	std::fill(volDlyCntDrum_.begin(), volDlyCntDrum_.end(), -1);
	std::fill(volDlyValueDrum_.begin(), volDlyValueDrum_.end(), -1);
}

void PlaybackManager::clearFMDelayBeyondStepCounts(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	ntCutDlyCntFM_.at(uch) = -1;
	volDlyCntFM_.at(uch) = -1;
	volDlyValueFM_.at(uch) = -1;
	tposeDlyCntFM_.at(uch) = -1;
	tposeDlyValueFM_.at(uch) = 0;
}

void PlaybackManager::clearSSGDelayBeyondStepCounts(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	ntCutDlyCntSSG_.at(uch) = -1;
	volDlyCntSSG_.at(uch) = -1;
	volDlyValueSSG_.at(uch) = -1;
	tposeDlyCntSSG_.at(uch) = -1;
	tposeDlyValueSSG_.at(uch) = 0;
}

void PlaybackManager::clearDrumDelayBeyondStepCounts(int ch)
{
	size_t uch = static_cast<size_t>(ch);
	ntCutDlyCntDrum_.at(uch) = -1;
	volDlyCntDrum_.at(uch) = -1;
	volDlyValueDrum_.at(uch) = -1;
}

void PlaybackManager::updateDelayEventCounts()
{
	auto f = [](int x) { return (x == -1) ? x : --x; };
	std::transform(ntDlyCntFM_.begin(), ntDlyCntFM_.end(), ntDlyCntFM_.begin(), f);
	std::transform(ntDlyCntSSG_.begin(), ntDlyCntSSG_.end(), ntDlyCntSSG_.begin(), f);
	std::transform(ntDlyCntDrum_.begin(), ntDlyCntDrum_.end(), ntDlyCntDrum_.begin(), f);
	std::transform(ntCutDlyCntFM_.begin(), ntCutDlyCntFM_.end(), ntCutDlyCntFM_.begin(), f);
	std::transform(ntCutDlyCntSSG_.begin(), ntCutDlyCntSSG_.end(), ntCutDlyCntSSG_.begin(), f);
	std::transform(ntCutDlyCntDrum_.begin(), ntCutDlyCntDrum_.end(), ntCutDlyCntDrum_.begin(), f);
	std::transform(volDlyCntFM_.begin(), volDlyCntFM_.end(), volDlyCntFM_.begin(), f);
	std::transform(volDlyCntSSG_.begin(), volDlyCntSSG_.end(), volDlyCntSSG_.begin(), f);
	std::transform(volDlyCntDrum_.begin(), volDlyCntDrum_.end(), volDlyCntDrum_.begin(), f);
	std::transform(tposeDlyCntFM_.begin(), tposeDlyCntFM_.end(), tposeDlyCntFM_.begin(), f);
	std::transform(tposeDlyCntSSG_.begin(), tposeDlyCntSSG_.end(), tposeDlyCntSSG_.begin(), f);
}

void PlaybackManager::checkPlayPosition(int maxStepSize)
{
	if (isPlaySong() && playStepNum_ >= maxStepSize) {
		playStepNum_ = maxStepSize - 1;
		findNextStep();
	}
}

void PlaybackManager::setChannelRetrieving(bool enabled)
{
	isRetrieveChannel_ = enabled;
}

void PlaybackManager::retrieveChannelStates()
{
	size_t fmch = getFMChannelCount(songStyle_.type);

	std::vector<int> tonesCntFM(fmch, 0), tonesCntSSG(3, 0);
	std::vector<std::vector<int>> toneFM(fmch), toneSSG(3);
	for (size_t i = 0; i < fmch; ++i) {
		toneFM.at(i) = std::vector<int>(3, -1);
	}
	for (size_t i = 0; i < 3; ++i) {
		toneSSG.at(i) = std::vector<int>(3, -1);
	}
	std::vector<bool> isSetInstFM(fmch, false), isSetVolFM(fmch, false), isSetArpFM(fmch, false);
	std::vector<bool> isSetPrtFM(fmch, false), isSetVibFM(fmch, false), isSetTreFM(fmch, false);
	std::vector<bool> isSetPanFM(fmch, false), isSetVolSldFM(fmch, false), isSetDtnFM(fmch, false);
	std::vector<bool> isSetFBCtrlFM(fmch, false), isSetTLCtrlFM(fmch, false), isSetMLCtrlFM(fmch, false);
	std::vector<bool> isSetARCtrlFM(fmch, false), isSetDRCtrlFM(fmch, false), isSetRRCtrlFM(fmch, false);
	std::vector<bool> isSetBrightFM(fmch, false);
	std::vector<bool> isSetInstSSG(3, false), isSetVolSSG(3, false), isSetArpSSG(3, false), isSetPrtSSG(3, false);
	std::vector<bool> isSetVibSSG(3, false), isSetTreSSG(3, false), isSetVolSldSSG(3, false), isSetDtnSSG(3, false);
	std::vector<bool> isSetTNMixSSG(3, false);
	std::vector<bool> isSetVolDrum(6, false), isSetPanDrum(6, false);
	bool isSetMVolDrum = false;
	bool isSetNoisePitchSSG = false;
	bool isSetHardEnvPeriodHighSSG = false;
	bool isSetHardEnvPeriodLowSSG = false;
	bool isSetAutoEnvSSG = false;
	/// bit0: step
	/// bit1: tempo
	/// bit2: groove
	uint8_t speedStates = 0;

	int o = playOrderNum_;
	int s = playStepNum_;
	bool isPrevPos = false;
	Song& song = mod_.lock()->getSong(curSongNum_);

	while (true) {
		for (auto it = songStyle_.trackAttribs.rbegin(), e = songStyle_.trackAttribs.rend(); it != e; ++it) {
			Step& step = song.getTrack(it->number).getPatternFromOrderNumber(o).getStep(s);
			int ch = it->channelInSource;
			size_t uch = static_cast<size_t>(ch);

			switch (it->source) {
			case SoundSource::FM:
			{
				// Volume
				int vol = step.getVolume();
				if (!isSetVolFM[uch] && 0 <= vol && vol < 0x80) {
					isSetVolFM[uch] = true;
					if (isPrevPos)
						opnaCtrl_->setVolumeFM(ch, step.getVolume());
				}
				// Instrument
				if (!isSetInstFM[uch] && step.getInstrumentNumber() != -1) {
					if (auto inst = std::dynamic_pointer_cast<InstrumentFM>(
								instMan_.lock()->getInstrumentSharedPtr(step.getInstrumentNumber()))) {
						isSetInstFM[uch] = true;
						if (isPrevPos)
							opnaCtrl_->setInstrumentFM(ch, inst);
					}
				}
				// Effects
				for (int i = 3; i > -1; --i) {
					Effect eff = Effect::makeEffectData(SoundSource::FM, step.getEffectID(i), step.getEffectValue(i));
					switch (eff.type) {
					case EffectType::Arpeggio:
						if (!isSetArpFM[uch]) {
							isSetArpFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setArpeggioEffectFM(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::PortamentoUp:
						if (!isSetPrtFM[uch]) {
							isSetPrtFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectFM(ch, eff.value);
						}
						break;
					case EffectType::PortamentoDown:
						if (!isSetPrtFM[uch]) {
							isSetPrtFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectFM(ch, -eff.value);
						}
						break;
					case EffectType::TonePortamento:
						if (!isSetPrtFM[uch]) {
							isSetPrtFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectFM(ch, eff.value, true);
						}
						break;
					case EffectType::Vibrato:
						if (!isSetVibFM[uch]) {
							isSetVibFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setVibratoEffectFM(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::Tremolo:
						if (!isSetTreFM[uch]) {
							isSetTreFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setTremoloEffectFM(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::Pan:
						if (-1 < eff.value && eff.value < 4 && !isSetPanFM[uch]) {
							isSetPanFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setPanFM(ch, eff.value);
						}
						break;
					case EffectType::VolumeSlide:
						if (!isSetVolSldFM[uch]) {
							isSetVolSldFM[uch] = true;
							if (isPrevPos) {
								int hi = eff.value >> 4;
								int low = eff.value & 0x0f;
								if (hi && !low) opnaCtrl_->setVolumeSlideFM(ch, hi, true);	// Slide up
								else if (!hi) opnaCtrl_->setVolumeSlideFM(ch, low, false);	// Slide down
							}
						}
						break;
					case EffectType::SpeedTempoChange:
						if (!(speedStates & 0x4)) {
							if (eff.value < 0x20 && !(speedStates & 0x1)) {	// Speed change
								speedStates |= 0x1;
								if (isPrevPos) effSpeedChange(eff.value);
							}
							else if (!(speedStates & 0x2)) {			// Tempo change
								speedStates |= 0x2;
								if (isPrevPos) effTempoChange(eff.value);
							}
						}
						break;
					case EffectType::Groove:
						if (eff.value < static_cast<int>(mod_.lock()->getGrooveCount()) && !speedStates) {
							speedStates |= 0x4;
							if (isPrevPos) effGrooveChange(eff.value);
						}
						break;
					case EffectType::Detune:
						if (!isSetDtnFM[uch]) {
							isSetDtnFM[uch] = true;
							if (isPrevPos) opnaCtrl_->setDetuneFM(ch, eff.value - 0x80);
						}
						break;
					case EffectType::FBControl:
						if (!isSetFBCtrlFM[uch]) {
							isSetFBCtrlFM[uch] = true;
							if (isPrevPos) {
								if (-1 < eff.value && eff.value < 8) opnaCtrl_->setFBControlFM(ch, eff.value);
							}
						}
						break;
					case EffectType::TLControl:
						if (!isSetTLCtrlFM[uch]) {
							isSetTLCtrlFM[uch] = true;
							if (isPrevPos) {
								int op = eff.value >> 8;
								int val = eff.value & 0x00ff;
								if (0 < op && op < 5 && -1 < val && val < 128)
									opnaCtrl_->setTLControlFM(ch, op - 1, val);
							}
						}
						break;
					case EffectType::MLControl:
						if (!isSetMLCtrlFM[uch]) {
							isSetMLCtrlFM[uch] = true;
							if (isPrevPos) {
								int op = eff.value >> 4;
								int val = eff.value & 0x0f;
								if (0 < op && op < 5 && -1 < val && val < 16)
									opnaCtrl_->setMLControlFM(ch, op - 1, val);
							}
						}
						break;
					case EffectType::ARControl:
						if (!isSetARCtrlFM[uch]) {
							isSetARCtrlFM[uch] = true;
							if (isPrevPos) {
								int op = eff.value >> 8;
								int val = eff.value & 0x00ff;
								if (0 < op && op < 5 && -1 < val && val < 32)
									opnaCtrl_->setARControlFM(ch, op - 1, val);
							}
						}
						break;
					case EffectType::DRControl:
						if (!isSetDRCtrlFM[uch]) {
							isSetDRCtrlFM[uch] = true;
							if (isPrevPos) {
								int op = eff.value >> 8;
								int val = eff.value & 0x00ff;
								if (0 < op && op < 5 && -1 < val && val < 32)
									opnaCtrl_->setDRControlFM(ch, op - 1, val);
							}
						}
						break;
					case EffectType::RRControl:
						if (!isSetRRCtrlFM[uch]) {
							isSetRRCtrlFM[uch] = true;
							if (isPrevPos) {
								int op = eff.value >> 4;
								int val = eff.value & 0x0f;
								if (0 < op && op < 5 && -1 < val && val < 16)
									opnaCtrl_->setRRControlFM(ch, op - 1, val);
							}
						}
						break;
					case EffectType::Brightness:
						if (!isSetBrightFM[uch]) {
							isSetBrightFM[uch] = true;
							if (isPrevPos) {
								if (0 < eff.value) opnaCtrl_->setBrightnessFM(ch, eff.value - 0x80);
							}
						}
						break;
					default:
						break;
					}
				}
				// Tone
				int t = step.getNoteNumber();
				if (isPrevPos && t != -1 && t != -2) {
					--tonesCntFM[uch];
					for (auto it2 = toneFM[uch].rbegin();
						 it2 != toneFM[uch].rend(); ++it2) {
						if (*it2 == -1 || *it2 == tonesCntFM[uch]) {
							if (t >= 0) {
								*it2 = t;
							}
							else if (t < -2) {
								*it2 = tonesCntFM[uch] - t + 2;
							}
							break;
						}
					}
				}
				break;
			}
			case SoundSource::SSG:
			{
				// Volume
				int vol = step.getVolume();
				if (!isSetVolSSG[uch] && 0 <= vol && vol < 0x10) {
					isSetVolSSG[uch] = true;
					if (isPrevPos)
						opnaCtrl_->setVolumeSSG(ch, vol);
				}
				// Instrument
				if (!isSetInstSSG[uch] && step.getInstrumentNumber() != -1) {
					if (auto inst = std::dynamic_pointer_cast<InstrumentSSG>(
								instMan_.lock()->getInstrumentSharedPtr(step.getInstrumentNumber()))) {
						isSetInstSSG[uch] = true;
						if (isPrevPos)
							opnaCtrl_->setInstrumentSSG(ch, inst);
					}
				}
				// Effects
				for (int i = 3; i > -1; --i) {
					Effect eff = Effect::makeEffectData(SoundSource::SSG, step.getEffectID(i), step.getEffectValue(i));
					switch (eff.type) {
					case EffectType::Arpeggio:
						if (!isSetArpSSG[uch]) {
							isSetArpSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setArpeggioEffectSSG(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::PortamentoUp:
						if (!isSetPrtSSG[uch]) {
							isSetPrtSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectSSG(ch, eff.value);
						}
						break;
					case EffectType::PortamentoDown:
						if (!isSetPrtSSG[uch]) {
							isSetPrtSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectSSG(ch, -eff.value);
						}
						break;
					case EffectType::TonePortamento:
						if (!isSetPrtSSG[uch]) {
							isSetPrtSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setPortamentoEffectSSG(ch, eff.value, true);
						}
						break;
					case EffectType::Vibrato:
						if (!isSetVibSSG[uch]) {
							isSetVibSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setVibratoEffectSSG(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::Tremolo:
						if (!isSetTreSSG[uch]) {
							isSetTreSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setTremoloEffectSSG(ch, eff.value >> 4, eff.value & 0x0f);
						}
						break;
					case EffectType::VolumeSlide:
						if (!isSetVolSldSSG[uch]) {
							isSetVolSldSSG[uch] = true;
							if (isPrevPos) {
								int hi = eff.value >> 4;
								int low = eff.value & 0x0f;
								if (hi && !low) opnaCtrl_->setVolumeSlideSSG(ch, hi, true);	// Slide up
								else if (!hi) opnaCtrl_->setVolumeSlideSSG(ch, low, false);	// Slide down
							}
						}
						break;
					case EffectType::SpeedTempoChange:
						if (!(speedStates & 0x4)) {
							if (eff.value < 0x20 && !(speedStates & 0x1)) {	// Speed change
								speedStates |= 0x1;
								if (isPrevPos) effSpeedChange(eff.value);
							}
							else if (!(speedStates & 0x2)) {			// Tempo change
								speedStates |= 0x2;
								if (isPrevPos) effTempoChange(eff.value);
							}
						}
						break;
					case EffectType::Groove:
						if (eff.value < static_cast<int>(mod_.lock()->getGrooveCount()) && !speedStates) {
							speedStates |= 0x4;
							if (isPrevPos) effGrooveChange(eff.value);
						}
						break;
					case EffectType::Detune:
						if (!isSetDtnSSG[uch]) {
							isSetDtnSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setDetuneSSG(ch, eff.value - 0x80);
						}
						break;
					case EffectType::ToneNoiseMix:
						if (-1 < eff.value && eff.value < 4 && !isSetTNMixSSG[uch]) {
							isSetTNMixSSG[uch] = true;
							if (isPrevPos) opnaCtrl_->setToneNoiseMixSSG(ch, eff.value);
						}
						break;
					case EffectType::NoisePitch:
						if (-1 < eff.value && eff.value < 32 && !isSetNoisePitchSSG) {
							isSetNoisePitchSSG = true;
							if (isPrevPos) opnaCtrl_->setNoisePitchSSG(ch, eff.value);
						}
						break;
					case EffectType::HardEnvHighPeriod:
						if (!isSetHardEnvPeriodHighSSG) {
							isSetHardEnvPeriodHighSSG = true;
							if (isPrevPos) opnaCtrl_->setHardEnvelopePeriod(ch, true, eff.value);
						}
						break;
					case EffectType::HardEnvLowPeriod:
						if (!isSetHardEnvPeriodLowSSG) {
							isSetHardEnvPeriodLowSSG = true;
							if (isPrevPos) opnaCtrl_->setHardEnvelopePeriod(ch, false, eff.value);
						}
						break;
					case EffectType::AutoEnvelope:
						if (!isSetAutoEnvSSG) {
							isSetAutoEnvSSG = true;
							if (isPrevPos) opnaCtrl_->setAutoEnvelopeSSG(ch, (eff.value >> 4) - 8, eff.value & 0x0f);
						}
						break;
					default:
						break;
					}
				}
				// Tone
				int t = step.getNoteNumber();
				if (isPrevPos && t != -1 && t != -2) {
					--tonesCntSSG[uch];
					for (auto it2 = toneSSG[uch].rbegin();
						 it2 != toneSSG[uch].rend(); ++it2) {
						if (*it2 == -1 || *it2 == tonesCntSSG[uch]) {
							if (t >= 0) {
								*it2 = t;
							}
							else if (t < -2) {
								*it2 = tonesCntSSG[uch] - t + 2;
							}
							break;
						}
					}
				}
				break;
			}
			case SoundSource::DRUM:
			{
				// Volume
				int vol = step.getVolume();
				if (!isSetVolDrum[uch] && 0 <= vol && vol < 0x20) {
					isSetVolDrum[uch] = true;
					if (isPrevPos)
						opnaCtrl_->setVolumeDrum(ch, vol);
				}
				// Effects
				for (int i = 3; i > -1; --i) {
					Effect eff = Effect::makeEffectData(SoundSource::DRUM, step.getEffectID(i), step.getEffectValue(i));
					switch (eff.type) {
					case EffectType::Pan:
						if (-1 < eff.value && eff.value < 4 && !isSetPanDrum[uch]) {
							isSetPanDrum[uch] = true;
							if (isPrevPos) opnaCtrl_->setPanDrum(ch, eff.value);
						}
						break;
					case EffectType::SpeedTempoChange:
						if (!(speedStates & 0x4)) {
							if (eff.value < 0x20 && !(speedStates & 0x1)) {	// Speed change
								speedStates |= 0x1;
								if (isPrevPos) effSpeedChange(eff.value);
							}
							else if (!(speedStates & 0x2)) {			// Tempo change
								speedStates |= 0x2;
								if (isPrevPos) effTempoChange(eff.value);
							}
						}
						break;
					case EffectType::Groove:
						if (eff.value < static_cast<int>(mod_.lock()->getGrooveCount()) && !speedStates) {
							speedStates |= 0x4;
							if (isPrevPos) effGrooveChange(eff.value);
						}
						break;
					case EffectType::MasterVolume:
						if (-1 < eff.value && eff.value < 64 && !isSetMVolDrum) {
							isSetMVolDrum = true;
							if (isPrevPos) opnaCtrl_->setMasterVolumeDrum(eff.value);
						}
						break;
					default:
						break;
					}
				}
				break;
			}
			case SoundSource::ADPCM:
			{
				// TODO: adpcm
				break;
			}
			}
		}

		// Move position
		isPrevPos = true;
		if (--s < 0) {
			if (--o < 0) break;
			s = static_cast<int>(getPatternSizeFromOrderNumber(curSongNum_, o)) - 1;
		}
	}

	// Echo & sequence reset
	for (size_t ch = 0; ch < fmch; ++ch) {
		for (size_t i = 0; i < 3; ++i) {
			if (toneFM.at(ch).at(i) >= 0) {
				std::pair<int, Note> octNote = noteNumberToOctaveAndNote(toneFM[ch][i]);
				opnaCtrl_->updateEchoBufferFM(static_cast<int>(ch), octNote.first, octNote.second, 0);
			}
		}
		opnaCtrl_->haltSequencesFM(static_cast<int>(ch));
	}
	for (size_t ch = 0; ch < 3; ++ch) {
		for (size_t i = 0; i < 3; ++i) {
			if (toneSSG.at(ch).at(i) >= 0) {
				std::pair<int, Note> octNote = noteNumberToOctaveAndNote(toneSSG[ch][i]);
				opnaCtrl_->updateEchoBufferSSG(static_cast<int>(ch), octNote.first, octNote.second, 0);
			}
		}
		opnaCtrl_->haltSequencesSSG(static_cast<int>(ch));
	}
}

size_t PlaybackManager::getOrderSize(int songNum) const
{
	return mod_.lock()->getSong(songNum).getOrderSize();
}

size_t PlaybackManager::getPatternSizeFromOrderNumber(int songNum, int orderNum) const
{
	return mod_.lock()->getSong(songNum).getPatternSizeFromOrderNumber(orderNum);
}
