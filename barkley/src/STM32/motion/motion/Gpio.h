/*
 * Gpio: GPIO pins, read or driven through their bit-band aliases (a pin may be
 * inverted: its level is the pin's state XOR invert), and a pin that interrupts on its
 * EXTI line. In image 00 an output's level cannot be read back, and the line's pending
 * bit is not cleared but by its interrupt.
 */
#ifndef GPIO_H
#define GPIO_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "BitBand.h"
#include "Callback.h"

/* the ports' registers, by number (GPIOA-GPIOG) */
extern GPIO_TypeDef *const gpioPorts[7];

class Input {
public:
	Input() {}

	virtual s8 Get() const
	{
		return 0;
	}
};

class Output {
public:
	Output() {}
	virtual void Set(s8 level) {}
#if MOTION_IMAGE != 0

	virtual s8 Get() const
	{
		return 0;
	}
#endif
};

class GpioIn : public Input {
public:
	GpioIn(s8 port, u32 pin, s8 invert)
	{
		bit_ = BitBand((u32)&gpioPorts[port]->IDR, pin);
		invert_ = invert;
	}

	virtual s8 Get() const
	{
		return invert_ ^ *bit_;
	}

protected:
	vu32 *bit_;
	s8 invert_;
};

class GpioOut : public Output {
public:
	GpioOut(s8 port, u32 pin, s8 invert)
	{
		bit_ = BitBand((u32)&gpioPorts[port]->ODR, pin);
		invert_ = invert;
	}

	virtual void Set(s8 level)
	{
		*bit_ = level ^ invert_;
	}
#if MOTION_IMAGE != 0

	virtual s8 Get() const
	{
		return invert_ ^ *bit_;
	}
#endif

private:
	vu32 *bit_;
	s8 invert_;
};

/* the edges an EXTI line triggers on, of the level (the pin's state XOR invert) */
enum {
	EXTI_EDGE_FALLING,
	EXTI_EDGE_RISING,
	EXTI_EDGE_BOTH,
};

/* a pin's EXTI line: the edge it triggers on and the handler it calls with the level,
   its interrupt masked or not, its pending bit */
class ExtiHandler : public Input {
public:
	typedef Callback<void (s8)> Handler;

	ExtiHandler() {}
	virtual void SetTrigger(s8 edge, Handler::Func func, void *arg) {}
	virtual void Enable() {}
	virtual void Disable() {}
	virtual void Unmask() {}
	virtual void Mask() {}
#if MOTION_IMAGE != 0
	virtual void ClearPending() {}
#endif

	void Start(s8 edge, Handler::Func func, void *arg)
	{
		SetTrigger(edge, func, arg);
		Enable();
	}
};

class ExtiPin : public GpioIn, public ExtiHandler {
public:
	ExtiPin(s8 port, u32 pin, s8 invert);
	void Init(u8 preemption, u8 sub);

	virtual s8 Get() const
	{
		return GpioIn::Get();
	}

	virtual void SetTrigger(s8 edge, Handler::Func func, void *arg);

	virtual void Enable()
	{
		Unmask();
	}

	virtual void Disable()
	{
		Mask();
	}

	virtual void Unmask()
	{
		*imr_ = 1;
	}

	virtual void Mask()
	{
		*imr_ = 0;
	}

#if MOTION_IMAGE != 0
	virtual void ClearPending();
#endif

private:
	static void IsrCallback(void *arg);
	void Isr();

	vu32 *imr_;		/* the line's bits, bit-band: interrupt mask */
	vu32 *rtsr_;		/* rising trigger */
	vu32 *ftsr_;		/* falling trigger */
	u8 port_;
	u32 pin_;
	u8 preemption_;		/* the interrupt's priority; nothing sets it */
	u8 sub_;
	u32 line_;		/* EXTI_Line0-15 */
	Handler handler_;
};

#endif
