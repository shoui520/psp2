/*
 * Start-up (ST's stm32f10x_vector for IAR): the vector table at 0x08003000, the stack's
 * top and the exceptions' and interrupts' handlers (stm32f10x_it; the two CAN interrupts
 * nothing uses stop here), and __low_level_init, which IAR's start-up calls before the
 * data is initialised (it returns 1: initialise).
 */

#include "motion.h"

typedef void (*IntFunc)(void);

typedef union {
	IntFunc __fun;
	void *__ptr;
} IntVector;

extern char cstackLimit[] __asm__("CSTACK$$Limit");

void __iar_program_start(void);

void NMIException(void);
void HardFaultException(void);
void MemManageException(void);
void BusFaultException(void);
void UsageFaultException(void);
void SVCHandler(void);
void DebugMonitor(void);
void PendSVC(void);
void SysTickHandler(void);
void WWDG_IRQHandler(void);
void PVD_IRQHandler(void);
void TAMPER_IRQHandler(void);
void RTC_IRQHandler(void);
void FLASH_IRQHandler(void);
void RCC_IRQHandler(void);
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void DMA1_Channel1_IRQHandler(void);
void DMA1_Channel2_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void);
void DMA1_Channel4_IRQHandler(void);
void DMA1_Channel5_IRQHandler(void);
void DMA1_Channel6_IRQHandler(void);
void DMA1_Channel7_IRQHandler(void);
void ADC1_2_IRQHandler(void);
void USB_HP_CAN_TX_IRQHandler(void);
void USB_LP_CAN_RX0_IRQHandler(void);
void CAN_RX1_IRQHandler(void);
void CAN_SCE_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void TIM1_BRK_IRQHandler(void);
void TIM1_UP_IRQHandler(void);
void TIM1_TRG_COM_IRQHandler(void);
void TIM1_CC_IRQHandler(void);
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);
void TIM4_IRQHandler(void);
void I2C1_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);
void I2C2_EV_IRQHandler(void);
void I2C2_ER_IRQHandler(void);
void SPI1_IRQHandler(void);
void SPI2_IRQHandler(void);
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART3_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void RTCAlarm_IRQHandler(void);
void USBWakeUp_IRQHandler(void);

const IntVector __vector_table[] __attribute__((section(".intvec"))) = {
	{ .__ptr = cstackLimit },
	{ __iar_program_start },
	{ NMIException },
	{ HardFaultException },
	{ MemManageException },
	{ BusFaultException },
	{ UsageFaultException },
	{ 0 }, { 0 }, { 0 }, { 0 },	/* reserved */
	{ SVCHandler },
	{ DebugMonitor },
	{ 0 },				/* reserved */
	{ PendSVC },
	{ SysTickHandler },
	{ WWDG_IRQHandler },
	{ PVD_IRQHandler },
	{ TAMPER_IRQHandler },
	{ RTC_IRQHandler },
	{ FLASH_IRQHandler },
	{ RCC_IRQHandler },
	{ EXTI0_IRQHandler },
	{ EXTI1_IRQHandler },
	{ EXTI2_IRQHandler },
	{ EXTI3_IRQHandler },
	{ EXTI4_IRQHandler },
	{ DMA1_Channel1_IRQHandler },
	{ DMA1_Channel2_IRQHandler },
	{ DMA1_Channel3_IRQHandler },
	{ DMA1_Channel4_IRQHandler },
	{ DMA1_Channel5_IRQHandler },
	{ DMA1_Channel6_IRQHandler },
	{ DMA1_Channel7_IRQHandler },
	{ ADC1_2_IRQHandler },
	{ USB_HP_CAN_TX_IRQHandler },
	{ USB_LP_CAN_RX0_IRQHandler },
	{ CAN_RX1_IRQHandler },
	{ CAN_SCE_IRQHandler },
	{ EXTI9_5_IRQHandler },
	{ TIM1_BRK_IRQHandler },
	{ TIM1_UP_IRQHandler },
	{ TIM1_TRG_COM_IRQHandler },
	{ TIM1_CC_IRQHandler },
	{ TIM2_IRQHandler },
	{ TIM3_IRQHandler },
	{ TIM4_IRQHandler },
	{ I2C1_EV_IRQHandler },
	{ I2C1_ER_IRQHandler },
	{ I2C2_EV_IRQHandler },
	{ I2C2_ER_IRQHandler },
	{ SPI1_IRQHandler },
	{ SPI2_IRQHandler },
	{ USART1_IRQHandler },
	{ USART2_IRQHandler },
	{ USART3_IRQHandler },
	{ EXTI15_10_IRQHandler },
	{ RTCAlarm_IRQHandler },
	{ USBWakeUp_IRQHandler },
};

void CAN_RX1_IRQHandler(void)
{
	while (1) {
	}
}

void CAN_SCE_IRQHandler(void)
{
	while (1) {
	}
}

int __low_level_init(void)
{
	return 1;
}
