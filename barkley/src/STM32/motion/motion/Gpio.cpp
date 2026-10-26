/*
 * Gpio: an ExtiPin's line is the pin's number, on the port the AFIO maps to it; its
 * triggers follow the edge asked for, turned round for an inverted pin. The line's
 * interrupt calls the handler with the pin's level.
 */

#include "Gpio.h"
#include "stm32f10x_it.h"

GPIO_TypeDef *const gpioPorts[7] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };

/* each pin's EXTI line */
static const u16 extiLines[16] = {
	EXTI_Line0, EXTI_Line1, EXTI_Line2, EXTI_Line3, EXTI_Line4, EXTI_Line5, EXTI_Line6,
	EXTI_Line7, EXTI_Line8, EXTI_Line9, EXTI_Line10, EXTI_Line11, EXTI_Line12,
	EXTI_Line13, EXTI_Line14, EXTI_Line15,
};

/* the triggers (RTSR, FTSR) for each edge, of a pin as it is and inverted */
static const struct {
	u32 rising;
	u32 falling;
} extiTriggers[2][3] = {
	{ { 0, 1 }, { 1, 0 }, { 1, 1 } },
	{ { 1, 0 }, { 0, 1 }, { 1, 1 } },
};

ExtiPin::ExtiPin(s8 port, u32 pin, s8 invert)
	: GpioIn(port, pin, invert), imr_(BitBand((u32)&EXTI->IMR, pin)), port_(port), pin_(pin),
	  line_(extiLines[pin])
{
	rtsr_ = BitBand((u32)&EXTI->RTSR, pin);
	ftsr_ = BitBand((u32)&EXTI->FTSR, pin);
}

/* the line from the pin, its handler; the priority given is not used */
void ExtiPin::Init(u8 preemption, u8 sub)
{
	GPIO_EXTILineConfig(port_, pin_);
	RegisterExtiHandler(pin_, IsrCallback, this, preemption_, sub_);
}

/* the line masked */
void ExtiPin::SetTrigger(s8 edge, Handler::Func func, void *arg)
{
	*rtsr_ = extiTriggers[GpioIn::invert_][edge].rising;
	*ftsr_ = extiTriggers[GpioIn::invert_][edge].falling;
	handler_.Set(func, arg);
	Mask();
}

void ExtiPin::IsrCallback(void *arg)
{
	static_cast<ExtiPin *>(arg)->Isr();
}

void ExtiPin::Isr()
{
	handler_(Get());
	EXTI_ClearITPendingBit(line_);
}

/* the line's interrupt's and the line's pending bits cleared */
void ExtiPin::ClearPending()
{
	switch (line_) {
	case EXTI_Line0:
		ClearPendingIrq(EXTI0_IRQChannel);
		break;
	case EXTI_Line1:
		ClearPendingIrq(EXTI1_IRQChannel);
		break;
	case EXTI_Line2:
		ClearPendingIrq(EXTI2_IRQChannel);
		break;
	case EXTI_Line3:
		ClearPendingIrq(EXTI3_IRQChannel);
		break;
	case EXTI_Line4:
		ClearPendingIrq(EXTI4_IRQChannel);
		break;
	case EXTI_Line5:
	case EXTI_Line6:
	case EXTI_Line7:
	case EXTI_Line8:
	case EXTI_Line9:
		ClearPendingIrq(EXTI9_5_IRQChannel);
		break;
	case EXTI_Line10:
	case EXTI_Line11:
	case EXTI_Line12:
	case EXTI_Line13:
	case EXTI_Line14:
	case EXTI_Line15:
		ClearPendingIrq(EXTI15_10_IRQChannel);
		break;
	}
	EXTI_ClearITPendingBit(line_);
}
