/*
 * main: the system and the devices set up, the program information printed (UserVer,
 * HwInfo, serial, LoaderVer, the firmware's checksum), then the devices started and the
 * tasks run; after each STOP mode the clocks, the timer slots and the host link are
 * restored and the tasks run again. Image 00 sets the clocks itself first, and has no
 * STOP mode.
 */

#include <stdio.h>

#if MOTION_IMAGE == 0
extern "C" {
#include "stm32f10x_lib.h"
}
#endif
#include "Board.h"
#include "MotionProcessor.h"
#include "System.h"

#if MOTION_IMAGE == 0
static void SetupClocks();
#endif

int main()
{
	const DeviceInfo *info;
	const u16 *checksum;

#if MOTION_IMAGE == 0
	SetupClocks();
#endif
	InitSystem();
	InitDevices();
	info = GetDeviceInfo();
	checksum = GetChecksum();
	printf("\n\n-- Motion Program Info --\n");
	printf("UserVer   : 0x%04X(%u)\n", info->userVersion, info->userVersion);
	printf("HwInfo    : 0x%04X(%u)\n", info->hwInfo, info->hwInfo);
	printf("serial    : %lu\n", info->serial);
	printf("LoaderVer : 0x%04X(%u)\n", info->loaderVersion, info->loaderVersion);
	printf("Firm csum : 0x%04X\n", *checksum);
	printf("-------------------------\n");
	StartSystem();
	StartDevices();
#if MOTION_IMAGE == 0
	RunTasks(0, 0);
	return 0;
#else
	for (;;) {
		RunTasks(0, 0);
		ResumeSystem();
		ResumeFromStop();
	}
#endif
}

#if MOTION_IMAGE == 0
/* a system clock of 16 MHz from the 24 MHz oscillator: / 6, then the PLL's * 4 (ST's
   RCC_Configuration of its examples, the name ours) */
static void RCC_Configuration()
{
	RCC_DeInit();
	RCC_HSEConfig(RCC_HSE_ON);
	if (RCC_WaitForHSEStartUp() == SUCCESS) {
		FLASH_SetLatency(FLASH_Latency_0);
		RCC_HCLKConfig(RCC_SYSCLK_Div1);
		RCC_PCLK2Config(RCC_HCLK_Div1);
		RCC_PCLK1Config(RCC_HCLK_Div1);
		RCC_ADCCLKConfig(RCC_PCLK2_Div2);
		RCC_PLLConfig(RCC_PLLSource_HSE_Div2, RCC_PLLMul_4);
		RCC_CFGR2 = 5;
		RCC_PLLCmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
			;
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		while (RCC_GetSYSCLKSource() != 0x08)	/* the PLL */
			;
		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 | RCC_AHBPeriph_DMA2, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA |
				       RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
				       RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1 |
				       RCC_APB2Periph_ADC2 | RCC_APB2Periph_TIM1 |
				       RCC_APB2Periph_USART1, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 |
				       RCC_APB1Periph_TIM4 | RCC_APB1Periph_SPI2 |
				       RCC_APB1Periph_I2C1 | RCC_APB1Periph_USB, ENABLE);
	}
}

static void SetupClocks()
{
	RCC_Configuration();
}
#endif
