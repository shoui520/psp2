/*
 * Led: the board's two LEDs, shown together as a two-bit pattern (LedPair), and the
 * blinkers: one toggling on a periodic timer (Blinker), one counting the main loop's
 * idle passes (CountBlinker). The blinkers keep their state; they do not drive the LED.
 */
#ifndef LED_H
#define LED_H

#include "Gpio.h"
#include "Timer.h"

/* each pattern's levels of the two LEDs */
extern const s8 ledPatterns[4][2];

class Indicator {
public:
	Indicator() {}
	virtual void Show(s8 pattern) {}
};

class LedPair : public Indicator {
public:
	LedPair(Output *led0, Output *led1)
	{
		led0_ = led0;
		led1_ = led1;
	}

	virtual void Show(s8 pattern)
	{
		led0_->Set(ledPatterns[pattern][0]);
		led1_->Set(ledPatterns[pattern][1]);
	}

private:
	Output *led0_;
	Output *led1_;
};

class Blinker {
public:
	Blinker(Output *led, IPeriodicTimer *timer)
	{
		led_ = led;
		timer_ = timer;
		on_ = 0;
	}

	/* on and off once a period */
	void Start(u32 period)
	{
		timer_->Start(period / 2, ToggleCallback, this);
	}

private:
	static void ToggleCallback(void *arg)
	{
		static_cast<Blinker *>(arg)->Toggle();
	}

	void Toggle()
	{
		on_ ^= 1;
	}

	Output *led_;
	IPeriodicTimer *timer_;
	u32 on_;
};

class CountBlinker {
public:
	CountBlinker(Output *led, u32 period)
	{
		led_ = led;
		period_ = period;
		count_ = 0;
		on_ = 0;
	}

	/* on or off every period passes */
	void Tick()
	{
		count_++;
		if (count_ >= period_) {
			count_ = 0;
			on_ ^= 1;
		}
	}

private:
	Output *led_;
	u32 period_;
	u32 count_;
	u32 on_;
};

/* a count on a periodic timer; nothing uses it */
class Ticker {
public:
	Ticker(IPeriodicTimer *timer, u16 count)
	{
		count_ = count;
		timer_ = timer;
		ticks_ = 0;
	}

private:
	u16 count_;
	IPeriodicTimer *timer_;
	u32 ticks_;
};

#endif
