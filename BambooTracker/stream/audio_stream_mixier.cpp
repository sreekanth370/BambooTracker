#include "audio_stream_mixier.hpp"
#include <algorithm>

const size_t AudioStreamMixier::NTSC_ = 60;
const size_t AudioStreamMixier::PAL_ = 50;

AudioStreamMixier::AudioStreamMixier(chip::Chip& chip, uint32_t rate, uint32_t duration, QObject* parent) :
	QIODevice(parent),
	chip_(chip),
	rate_(rate),
	duration_(duration),
	bufferSampleSize_(rate * duration / 1000),
	tickRate_(NTSC_),
	tickIntrCount_(rate / tickRate_),
	tickIntrCountRest_(0),
	isPlaySong_(false),
	specificTicksPerStep_(0),
	executingTicksPerStep_(0),
	tickCount_(0),
	tempo_(0),
	strictTicksPerStepByBpm_(0),
	tickDifSum_(0),
	isFirstRead_(true)
{
}

AudioStreamMixier::~AudioStreamMixier()
{
	if (hasRun()) stop();
}

void AudioStreamMixier::start()
{
	isFirstRead_ = true;
	open(QIODevice::ReadOnly);
}

void AudioStreamMixier::stop()
{
	close();
}

bool AudioStreamMixier::hasRun()
{
	return isOpen();
}

void AudioStreamMixier::setRate(uint32_t rate)
{
	rate_ = rate;
	setBufferSampleSize(rate, duration_);
	tickIntrCount_ = rate / tickRate_;
}

void AudioStreamMixier::setDuration(uint32_t duration)
{
	duration_ = duration;
	setBufferSampleSize(rate_, duration);
}

void  AudioStreamMixier::setBufferSampleSize(uint32_t rate, uint32_t duration)
{
	bufferSampleSize_ = rate * duration / 1000;
}

qint64 AudioStreamMixier::readData(char* data, qint64 maxlen)
{
	qint64 generatedCount;
	if (isFirstRead_) {   // Fill device buffer in first read
		generatedCount = maxlen >> 2;
		isFirstRead_ = false;
	}
	else {  // Fill appropriate sample counts
		generatedCount = std::min(bufferSampleSize_, (maxlen >> 2));
	}
	size_t requiredCount = static_cast<size_t>(generatedCount);
	int16_t* destPtr = reinterpret_cast<int16_t*>(data);

	size_t count;
	while (requiredCount) {
		if (!tickIntrCountRest_) {	// in tick point
			tickIntrCountRest_ = tickIntrCount_;    // Set distance to next tick

			if (isPlaySong_) {
				if (executingTicksPerStep_) {   //  Read by tick
					emit nextTickArrived();
				}
				else {  // Read by step (first tick in step)
					emit nextStepArrived();

					// Dummy set reading speed
					specificTicksPerStep_ = 6;
					tempo_ = 150;

					// Calculate executing ticks in step
					{
						strictTicksPerStepByBpm_ = 10.0 * tickRate_ * specificTicksPerStep_ / (tempo_ << 2);
						tickDifSum_ += strictTicksPerStepByBpm_ - static_cast<float>(specificTicksPerStep_);
						int castedTickDifSum = static_cast<int>(tickDifSum_);
						executingTicksPerStep_ = specificTicksPerStep_ + castedTickDifSum;
						tickDifSum_ -= castedTickDifSum;
					}
				}

				--executingTicksPerStep_;   // Mix by 1 tick
			}
		}

		count = std::min(tickIntrCountRest_, requiredCount);
		requiredCount -= count;
		tickIntrCountRest_ -= count;

		chip_.mix(destPtr, count);

		destPtr += (count << 1);	// Move head
	}

	return generatedCount << 2; // Return generated bytes count
}

qint64 AudioStreamMixier::writeData(const char *data, qint64 len)   // No use
{
	Q_UNUSED(data);
	Q_UNUSED(len);
	return 0;
}
