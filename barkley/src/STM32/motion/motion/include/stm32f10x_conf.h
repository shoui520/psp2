/*
 * The firmware library's configuration (ST's stm32f10x_conf.h): the peripherals the
 * program uses, the external oscillator (24 MHz) and how long RCC_WaitForHSEStartUp
 * polls for it. Parameter checks are off (DEBUG undefined): no library function in the
 * images checks its arguments.
 */
#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

#include "stm32f10x_type.h"

#define _ADC
#define _ADC1
#define _ADC2
#define _DMA
#define _DMA1_Channel1
#define _DMA1_Channel2
#define _DMA1_Channel3
#define _DMA1_Channel4
#define _DMA1_Channel5
#define _DMA1_Channel6
#define _DMA1_Channel7
#define _EXTI
#define _FLASH
#define _FLASH_PROG
#define _GPIO
#define _AFIO
#define _GPIOA
#define _GPIOB
#define _GPIOC
#define _GPIOD
#define _GPIOE
#define _GPIOF
#define _GPIOG
#define _I2C
#define _I2C1
#define _I2C2
#define _NVIC
#define _RCC
#define _SPI
#define _SPI1
#define _SPI2
#define _USART
#define _USART1
#define _USART2
#define _USART3

#define HSE_Value		((u32)24000000)
#define HSEStartUp_TimeOut	((u16)0x01FF)

#define assert_param(expr)	((void)0)

#endif
