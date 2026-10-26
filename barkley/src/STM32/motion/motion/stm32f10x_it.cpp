/*
 * The interrupt handlers (ST's stm32f10x_it): the faults stop, every other exception
 * and interrupt calls the handler registered for its vector. The EXTI lines that share
 * an interrupt (5-9, 10-15) have a handler each, called while the line is pending; ADC1
 * and ADC2 one each, called when an interrupt it enables is pending.
 */

extern "C" {
#include "stm32f10x_lib.h"
}
#include "stm32f10x_it.h"

/* by vector number: 16 exceptions (0 the stack), 43 interrupts */
static IrqHandler irqHandlers[59];
static IrqHandler extiHandlers[16];
static IrqHandler adcHandlers[2];

void SetSharedIrqPriorities(const u8 *priority)
{
	NVIC_InitTypeDef init;

	init.NVIC_IRQChannel = EXTI9_5_IRQChannel;
	init.NVIC_IRQChannelPreemptionPriority = priority[0];
	init.NVIC_IRQChannelSubPriority = priority[1];
	init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&init);
	init.NVIC_IRQChannel = EXTI15_10_IRQChannel;
	init.NVIC_IRQChannelPreemptionPriority = priority[2];
	init.NVIC_IRQChannelSubPriority = priority[3];
	init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&init);
	init.NVIC_IRQChannel = ADC1_2_IRQChannel;
	init.NVIC_IRQChannelPreemptionPriority = priority[4];
	init.NVIC_IRQChannelSubPriority = priority[5];
	init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&init);
}

void RegisterIrqHandler(s8 exception, IrqHandler::Func func, void *arg, u8 preemption, u8 sub)
{
	NVIC_InitTypeDef init;

	irqHandlers[exception].Set(func, arg);
	switch (exception) {
	case 11:
		NVIC_SystemHandlerPriorityConfig(SystemHandler_SVCall, preemption, sub);
		break;
	case 14:
		NVIC_SystemHandlerPriorityConfig(SystemHandler_PSV, preemption, sub);
		break;
	case 15:
		NVIC_SystemHandlerPriorityConfig(SystemHandler_SysTick, preemption, sub);
		break;
	default:
		if (exception >= 16) {
			init.NVIC_IRQChannel = exception - 16;
			init.NVIC_IRQChannelPreemptionPriority = preemption;
			init.NVIC_IRQChannelSubPriority = sub;
			init.NVIC_IRQChannelCmd = ENABLE;
			NVIC_Init(&init);
		}
		break;
	}
}

void RegisterExtiHandler(s8 line, IrqHandler::Func func, void *arg, u8 preemption, u8 sub)
{
	if (line < 5)
		RegisterIrqHandler(16 + EXTI0_IRQChannel + line, func, arg, preemption, sub);
	else
		extiHandlers[line].Set(func, arg);
	EXTI->IMR &= ~(1 << line);
}

extern "C" {

void NMIException(void)
{
}

void HardFaultException(void)
{
	while (1) {
	}
}

void MemManageException(void)
{
	while (1) {
	}
}

void BusFaultException(void)
{
	while (1) {
	}
}

void UsageFaultException(void)
{
	while (1) {
	}
}

void SVCHandler(void)
{
	irqHandlers[11]();
}

void DebugMonitor(void)
{
	irqHandlers[12]();
}

void PendSVC(void)
{
	irqHandlers[14]();
}

void SysTickHandler(void)
{
	irqHandlers[15]();
}

#define IRQ_HANDLER(name, channel)		\
	void name(void)				\
	{					\
		irqHandlers[16 + (channel)]();	\
	}

IRQ_HANDLER(WWDG_IRQHandler, WWDG_IRQChannel)
IRQ_HANDLER(PVD_IRQHandler, PVD_IRQChannel)
IRQ_HANDLER(TAMPER_IRQHandler, TAMPER_IRQChannel)
IRQ_HANDLER(RTC_IRQHandler, RTC_IRQChannel)
IRQ_HANDLER(FLASH_IRQHandler, FLASH_IRQChannel)
IRQ_HANDLER(RCC_IRQHandler, RCC_IRQChannel)
IRQ_HANDLER(EXTI0_IRQHandler, EXTI0_IRQChannel)
IRQ_HANDLER(EXTI1_IRQHandler, EXTI1_IRQChannel)
IRQ_HANDLER(EXTI2_IRQHandler, EXTI2_IRQChannel)
IRQ_HANDLER(EXTI3_IRQHandler, EXTI3_IRQChannel)
IRQ_HANDLER(EXTI4_IRQHandler, EXTI4_IRQChannel)
IRQ_HANDLER(DMA1_Channel1_IRQHandler, DMA1_Channel1_IRQChannel)
IRQ_HANDLER(DMA1_Channel2_IRQHandler, DMA1_Channel2_IRQChannel)
IRQ_HANDLER(DMA1_Channel3_IRQHandler, DMA1_Channel3_IRQChannel)
IRQ_HANDLER(DMA1_Channel4_IRQHandler, DMA1_Channel4_IRQChannel)
IRQ_HANDLER(DMA1_Channel5_IRQHandler, DMA1_Channel5_IRQChannel)
IRQ_HANDLER(DMA1_Channel6_IRQHandler, DMA1_Channel6_IRQChannel)
IRQ_HANDLER(DMA1_Channel7_IRQHandler, DMA1_Channel7_IRQChannel)
IRQ_HANDLER(USB_HP_CAN_TX_IRQHandler, USB_HP_CAN_TX_IRQChannel)
IRQ_HANDLER(USB_LP_CAN_RX0_IRQHandler, USB_LP_CAN_RX0_IRQChannel)
IRQ_HANDLER(TIM1_BRK_IRQHandler, TIM1_BRK_IRQChannel)
IRQ_HANDLER(TIM1_UP_IRQHandler, TIM1_UP_IRQChannel)
IRQ_HANDLER(TIM1_TRG_COM_IRQHandler, TIM1_TRG_COM_IRQChannel)
IRQ_HANDLER(TIM1_CC_IRQHandler, TIM1_CC_IRQChannel)
IRQ_HANDLER(TIM2_IRQHandler, TIM2_IRQChannel)
IRQ_HANDLER(TIM3_IRQHandler, TIM3_IRQChannel)
IRQ_HANDLER(TIM4_IRQHandler, TIM4_IRQChannel)
IRQ_HANDLER(I2C1_EV_IRQHandler, I2C1_EV_IRQChannel)
IRQ_HANDLER(I2C1_ER_IRQHandler, I2C1_ER_IRQChannel)
IRQ_HANDLER(I2C2_EV_IRQHandler, I2C2_EV_IRQChannel)
IRQ_HANDLER(I2C2_ER_IRQHandler, I2C2_ER_IRQChannel)
IRQ_HANDLER(SPI1_IRQHandler, SPI1_IRQChannel)
IRQ_HANDLER(SPI2_IRQHandler, SPI2_IRQChannel)
IRQ_HANDLER(USART1_IRQHandler, USART1_IRQChannel)
IRQ_HANDLER(USART2_IRQHandler, USART2_IRQChannel)
IRQ_HANDLER(USART3_IRQHandler, USART3_IRQChannel)
IRQ_HANDLER(RTCAlarm_IRQHandler, RTCAlarm_IRQChannel)
IRQ_HANDLER(USBWakeUp_IRQHandler, USBWakeUp_IRQChannel)

void EXTI9_5_IRQHandler(void)
{
	u32 mask = EXTI_Line5;

	for (int line = 5; line < 10; line++) {
		if (EXTI_GetITStatus(mask) != RESET)
			extiHandlers[line]();
		mask <<= 1;
	}
}

void EXTI15_10_IRQHandler(void)
{
	u32 mask = EXTI_Line10;

	for (int line = 10; line < 16; line++) {
		if (EXTI_GetITStatus(mask) != RESET)
			extiHandlers[line]();
		mask <<= 1;
	}
}

void ADC1_2_IRQHandler(void)
{
	u32 sr;
	u32 cr1;

	sr = ADC1->SR;
	cr1 = ADC1->CR1;
	if (((sr & ADC_FLAG_AWD) && (cr1 & (u8)ADC_IT_AWD)) ||
	    ((sr & ADC_FLAG_EOC) && (cr1 & (u8)ADC_IT_EOC)) ||
	    ((sr & ADC_FLAG_JEOC) && (cr1 & (u8)ADC_IT_JEOC)))
		adcHandlers[0]();
	sr = ADC2->SR;
	cr1 = ADC2->CR1;
	if (((sr & ADC_FLAG_AWD) && (cr1 & (u8)ADC_IT_AWD)) ||
	    ((sr & ADC_FLAG_EOC) && (cr1 & (u8)ADC_IT_EOC)) ||
	    ((sr & ADC_FLAG_JEOC) && (cr1 & (u8)ADC_IT_JEOC)))
		adcHandlers[1]();
}

}
