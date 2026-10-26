/*
 * Average: the sums the sensor samples are averaged in, and the divider that says when
 * an average is due.
 */
#ifndef AVERAGE_H
#define AVERAGE_H

#include "stm32f10x_type.h"

template <typename T> struct AverageLimits;

template <> struct AverageLimits<u16> {
	typedef u32 Sum;
	static const Sum Max = 0xFFFF;
	static const Sum Min = 0;
};

template <> struct AverageLimits<s16> {
	typedef s32 Sum;
	static const Sum Max = 0x7FFF;
	static const Sum Min = -0x8000;
};

/* sums samples of 16 - bits bits, and gives their average as a 16-bit value */
template <typename T>
class Averager {
public:
	typedef typename AverageLimits<T>::Sum Sum;

	Averager() : count_(0) {}

	void Init(u8 bits)
	{
		shift_ = 16 - bits;
		sum_ = 0;
		count_ = 0;
	}

	void Add(Sum value)
	{
		sum_ += value;
		count_++;
	}

	void Reset()
	{
		sum_ = 0;
		count_ = 0;
	}

	Sum Count() const
	{
		return count_;
	}

	T Get();

private:
	Sum sum_;
	Sum count_;
	Sum shift_;
};

template <typename T>
T Averager<T>::Get()
{
	Sum average;

	if (count_ == 0)
		return 0;
	average = ((sum_ << shift_) + count_ / 2) / count_;
	if (average > AverageLimits<T>::Max)
		average = AverageLimits<T>::Max;
	else if (average < AverageLimits<T>::Min)
		average = AverageLimits<T>::Min;
	sum_ = 0;
	count_ = 0;
	return average;
}

/* true once every period ticks */
class Divider {
public:
	Divider() {}

	void Set(u32 period, u32 count)
	{
		period_ = period;
		count_ = count;
	}

	/* true once in each period */
	bool Tick()
	{
		if (++count_ >= period_)
			count_ = 0;
		return count_ == 0;
	}

private:
	u32 period_;
	u32 count_;
};

#endif
