#ifndef PATTERN_POSITION_HPP
#define PATTERN_POSITION_HPP

struct PatternPosition
{
	int trackVisIdx, colInTrack, order, step;

	friend bool operator==(const PatternPosition& a, const PatternPosition& b);
	friend bool operator!=(const PatternPosition& a, const PatternPosition& b);

	void setCols(int trackVisIdx, int colInTrack);
	void setRows(int order, int step);

	int compareCols(const PatternPosition& b) const;
	int compareRows(const PatternPosition& b) const;

	bool isEqualCols(const PatternPosition& b) const;
	bool isEqualCols(const int trackVisIdx, const int colInTrack) const;
	bool isEqualRows(const PatternPosition& b) const;
	bool isEqualRows(const int order, const int step) const;

	static bool inRowRange(const PatternPosition& pos,
						   const PatternPosition& begin, const PatternPosition& last)
	{
		return (begin.compareRows(pos) <= 0 && pos.compareRows(last) <= 0);
	}

	static bool inRowRange(const PatternPosition& srcBegin, const PatternPosition& srcLast,
						   const PatternPosition& tgtBegin, const PatternPosition& tgtLast)
	{
		if (PatternPosition::inRowRange(srcBegin, tgtBegin, tgtLast)) return true;
		else if (PatternPosition::inRowRange(srcLast, tgtBegin, tgtLast)) return true;
		else return (srcBegin.compareRows(tgtBegin) < 0 && tgtLast.compareRows(srcLast));
	}
};

inline bool operator==(const PatternPosition& a, const PatternPosition& b)
{
	return ((a.trackVisIdx == b.trackVisIdx) && (a.colInTrack == b.colInTrack)
			&& (a.order == b.order) && (a.step == b.step));
}

inline bool operator!=(const PatternPosition& a, const PatternPosition& b)
{
	return !(a == b);
}

inline void PatternPosition::setCols(int track, int colInTrack)
{
	this->trackVisIdx = track;
	this->colInTrack = colInTrack;
}

inline void PatternPosition::setRows(int order, int step)
{
	this->order = order;
	this->step = step;
}

/// Return:
///		-2: this->track < b.track
///		-1: this->track == b.track && this->colInTrack < b.colInTrack
///		 0: this->track == b.track && this->colInTrack == b.colInTrack
///		 1: this->track == b.track && this->colInTrack > b.colInTrack
///		 2: this->track > b.track
inline int PatternPosition::compareCols(const PatternPosition& b) const
{
	if (trackVisIdx < b.trackVisIdx) return -2;
	else if (trackVisIdx > b.trackVisIdx) return 2;
	else {
		if (colInTrack < b.colInTrack) return -1;
		else if (colInTrack > b.colInTrack) return 1;
		else return 0;
	}
}

/// Return:
///		-2: this->order < b.order
///		-1: this->order == b.order && this->step < b.step
///		 0: *this == b
///		 1: this->order == b.order && this->step > b.step
///		 2: this->order > b.order
inline int PatternPosition::compareRows(const PatternPosition& b) const
{
	if (order < b.order) return -2;
	else if (order > b.order) return 2;
	else {
		if (step < b.step) return -1;
		else if (step > b.step) return 1;
		else return 0;
	}
}

inline bool PatternPosition::isEqualCols(const PatternPosition& b) const
{
	return isEqualCols(b.trackVisIdx, b.colInTrack);
}

inline bool PatternPosition::isEqualCols(const int trackVisIdx, const int colInTrack) const
{
	return (this->trackVisIdx == trackVisIdx && this->colInTrack == colInTrack);
}

inline bool PatternPosition::isEqualRows(const PatternPosition& b) const
{
	return isEqualRows(b.order, b.step);
}

inline bool PatternPosition::isEqualRows(const int order, const int step) const
{
	return (this->order == order && this->step == step);
}

#endif // PATTERN_POSITION_HPP
