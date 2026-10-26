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
#define _EXTI
#define _FLASH
#define _FLASH_PROG
#define _NVIC
#define _RCC

#define HSE_Value		((u32)24000000)
#define HSEStartUp_TimeOut	((u16)0x01FF)

#define assert_param(expr)	((void)0)

#endif
