/*
 * Board: the clocks, from the settings for the system clock and the oscillator the
 * board has, and the pins of each port.
 */

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Board.h"
#include "stm32f10x_it.h"

/* the system clock the program runs at */
#define SYSCLK_Value	((u32)16000000)

/* RCC_CFGR2 (the value line's PREDIV1, the oscillator's divider before the PLL); the
   library has no field for it */
#define RCC_CFGR2	(*(vu32 *)(RCC_BASE + 0x2C))

/* how a system clock is made from an oscillator */
struct ClockConfig {
	u32 sysclk;
	u32 hse;
	u32 prefetchBuffer;
	u32 latency;
	u32 pclk1;
	u32 adcclk;
	u32 source;		/* CLOCK_SOURCE_* */
	u32 reserved;
	u32 prediv1;		/* PLL: RCC_CFGR2 */
	u32 pllMul;
};

/* the system clock's source */
enum {
	CLOCK_SOURCE_HSE = 1,
	CLOCK_SOURCE_PLL = 2,
};

#define CLOCK_CONFIGS	12

static const ClockConfig clockConfigs[CLOCK_CONFIGS] = {
	{  8000000,  8000000, 0, 0, 0, 0, CLOCK_SOURCE_HSE, 0, 0, 0 },
	{  8000000, 16000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 3, RCC_PLLMul_2 },
	{  8000000, 24000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 5, RCC_PLLMul_2 },
	{ 12000000,  8000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 1, RCC_PLLMul_3 },
	{ 12000000, 16000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 3, RCC_PLLMul_3 },
	{ 12000000, 24000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 5, RCC_PLLMul_3 },
	{ 16000000,  8000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 1, RCC_PLLMul_4 },
	{ 16000000, 16000000, 0, 0, 0, 0, CLOCK_SOURCE_HSE, 0, 0, 0 },
	{ 16000000, 24000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 5, RCC_PLLMul_4 },
	{ 24000000,  8000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 1, RCC_PLLMul_6 },
	{ 24000000, 16000000, 0, 0, 0, 0, CLOCK_SOURCE_PLL, 0, 3, RCC_PLLMul_6 },
	{ 24000000, 24000000, 0, 0, 0, 0, CLOCK_SOURCE_HSE, 0, 0, 0 },
};

/* each port's pins */
static const GPIO_InitTypeDef portAPins[5] = {
	{ GPIO_Pin_0 | GPIO_Pin_6 | GPIO_Pin_7, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_11 |
	  GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_10, GPIO_Speed_50MHz, GPIO_Mode_IPU },
	{ GPIO_Pin_1, GPIO_Speed_50MHz, GPIO_Mode_Out_OD },
	{ GPIO_Pin_9, GPIO_Speed_50MHz, GPIO_Mode_AF_OD },
};

static const GPIO_InitTypeDef portBPins[4] = {
	{ GPIO_Pin_0 | GPIO_Pin_1, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_2 | GPIO_Pin_12, GPIO_Speed_50MHz, GPIO_Mode_IN_FLOATING },
	{ GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
	  GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_14, GPIO_Speed_50MHz, GPIO_Mode_AF_PP },
};

static const GPIO_InitTypeDef portCPins[5] = {
	{ GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_1 | GPIO_Pin_7 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
	  GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_10, GPIO_Speed_50MHz, GPIO_Mode_IPU },
	{ GPIO_Pin_6, GPIO_Speed_50MHz, GPIO_Mode_Out_OD },
	{ GPIO_Pin_8 | GPIO_Pin_9, GPIO_Speed_50MHz, GPIO_Mode_Out_PP },
};

static const GPIO_InitTypeDef portDPins[2] = {
	{ GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2, GPIO_Speed_50MHz, GPIO_Mode_IN_FLOATING },
	{ GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 |
	  GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
	  GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
};

static const GPIO_InitTypeDef portEPins[1] = {
	{ GPIO_Pin_0, GPIO_Speed_50MHz, GPIO_Mode_AIN },
};

static const GPIO_InitTypeDef portFPins[1] = {
	{ GPIO_Pin_0, GPIO_Speed_50MHz, GPIO_Mode_AIN },
};

static const GPIO_InitTypeDef portGPins[1] = {
	{ GPIO_Pin_0, GPIO_Speed_50MHz, GPIO_Mode_AIN },
};

#define countof(a)	(sizeof (a) / sizeof (a)[0])

/* the shared interrupts' priorities (preemption, sub): EXTI9_5, EXTI15_10, ADC1_2 */
static const u8 sharedIrqPriorities[6] = { 4, 1, 4, 1, 4, 1 };

/* IAR's vector table, the image's start */
extern "C" const u32 __vector_table[];

static void StartPll();
static void StartHse();
static void EnableClocks();

/* the settings for the system clock and the oscillator; none, or no oscillator, stops */
static void SetupClocks()
{
	const ClockConfig *config = clockConfigs;
	u8 i;

	for (i = 0; i < CLOCK_CONFIGS; i++, config++) {
		if (config->sysclk == SYSCLK_Value && config->hse == HSE_Value)
			break;
	}
	if (i == CLOCK_CONFIGS)
		for (;;)
			;
	RCC_DeInit();
	RCC_HSEConfig(RCC_HSE_ON);
	if (RCC_WaitForHSEStartUp() != SUCCESS)
		for (;;)
			;
	FLASH_PrefetchBufferCmd(config->prefetchBuffer);
	FLASH_SetLatency(config->latency);
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK2Config(RCC_HCLK_Div1);
	RCC_PCLK1Config(config->pclk1);
	RCC_ADCCLKConfig(config->adcclk);
	if (config->source == CLOCK_SOURCE_HSE) {
		StartHse();
	} else {
		RCC_PLLConfig(RCC_PLLSource_HSE_Div2, config->pllMul);
		RCC_CFGR2 = config->prediv1;
		StartPll();
	}
	EnableClocks();
}

static void StartPll()
{
	RCC_PLLCmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		;
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while (RCC_GetSYSCLKSource() != 0x08)	/* the PLL */
		;
}

static void StartHse()
{
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
	while (RCC_GetSYSCLKSource() != 0x04)	/* the HSE */
		;
}

static void EnableClocks()
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 | RCC_AHBPeriph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
			       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1 |
			       RCC_APB2Periph_ADC2 | RCC_APB2Periph_TIM1 | RCC_APB2Periph_USART1,
			       ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4 |
			       RCC_APB1Periph_SPI2 | RCC_APB1Periph_I2C1 | RCC_APB1Periph_USB,
			       ENABLE);
}

/* every pin; PC6 high before it is an output */
static void SetupGpio()
{
	u32 i;

	for (i = 0; i < countof(portAPins); i++)
		GPIO_Init(GPIOA, (GPIO_InitTypeDef *)&portAPins[i]);
	for (i = 0; i < countof(portBPins); i++)
		GPIO_Init(GPIOB, (GPIO_InitTypeDef *)&portBPins[i]);
	GPIO_SetBits(GPIOC, GPIO_Pin_6);
	for (i = 0; i < countof(portCPins); i++)
		GPIO_Init(GPIOC, (GPIO_InitTypeDef *)&portCPins[i]);
	for (i = 0; i < countof(portDPins); i++)
		GPIO_Init(GPIOD, (GPIO_InitTypeDef *)&portDPins[i]);
	for (i = 0; i < countof(portEPins); i++)
		GPIO_Init(GPIOE, (GPIO_InitTypeDef *)&portEPins[i]);
	for (i = 0; i < countof(portFPins); i++)
		GPIO_Init(GPIOF, (GPIO_InitTypeDef *)&portFPins[i]);
	for (i = 0; i < countof(portGPins); i++)
		GPIO_Init(GPIOG, (GPIO_InitTypeDef *)&portGPins[i]);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
}

void InitBoard()
{
	RCC_ClearFlag();
	SetupClocks();
	SetupGpio();
	SCB->VTOR = (u32)__vector_table;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);
	SetSharedIrqPriorities(sharedIrqPriorities);
}

void RestoreClocks()
{
	SetupClocks();
}
