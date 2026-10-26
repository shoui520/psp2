/*
 * Gpio: GPIO pins, read or driven through their bit-band aliases, and a pin that
 * interrupts on its EXTI line.
 */
#ifndef GPIO_H
#define GPIO_H

#include "stm32f10x_type.h"

class Input {
public:
	Input() {}
	virtual bool Get() const;
};

class Output {
public:
	Output() {}
	virtual void Set(bool on) {}
	virtual bool Get() const;
};

class GpioIn : public Input {
public:
	GpioIn(u8 port, u32 pin, bool level);
	virtual bool Get() const;

private:
	volatile u32 *bit_;
	bool level_;
};

class GpioOut : public Output {
public:
	GpioOut(u8 port, u32 pin, bool level);
	virtual void Set(bool on);
	virtual bool Get() const;

private:
	volatile u32 *bit_;
	bool level_;
};

class ExtiHandler {
public:
	ExtiHandler() {}
	virtual void Handle();
};

class ExtiPin : public GpioIn, public ExtiHandler {
public:
	ExtiPin(u8 port, u32 pin, bool level);
	void Init(u8 preemption, u8 sub);

private:
	u32 reserved_[8];
};

#endif
