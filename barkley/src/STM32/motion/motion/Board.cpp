/*
 * Board: the program's devices; the clocks, from the settings for the system clock and
 * the oscillator the board has, and the pins of each port. In image 00 (branch U0652)
 * the clocks are main's and the pins System's.
 */

extern "C" {
#include "stm32f10x_lib.h"
}
#include "AK8975.h"
#include "AdcSampler.h"
#include "Board.h"
#include "HifSpi.h"
#include "I2cRegIf.h"
#include "MotionProcessor.h"
#include "System.h"
#include "stm32f10x_it.h"

/* the system clock the program runs at */
#define SYSCLK_Value	((u32)16000000)

/* the motion processor's parameters */
#if MOTION_IMAGE == 0
static const MotionConfig motionConfig = {
	{
		(const CalibBlock *)0x0800FC00, (const CalibBlock *)0x0800F800,
		(const CalibBlock *)0x0800F400, (const CalibBlock *)0x0800F400,
	},
	161, 0x8000,
	16, 12, 13, 12,
	{
		{  80,  80,  80, 16000,  5 },
		{  32, 160, 160, 16000,  2 },
		{  32, 160, 160, 16000,  2 },
		{  80,  80,  80, 16000,  5 },
		{ 160, 160, 160, 16000, 10 },
		{  80, 160, 160, 16000,  5 },
	},
};
#else
static const MotionConfig motionConfig = {
	{
		(const CalibBlock *)0x0800FC00, (const CalibBlock *)0x0800F800,
		(const CalibBlock *)0x0800F400, (const CalibBlock *)0x0800F400,
	},
	161, 32, 16, 101, 101, 11, 0x8000,
	16, 12, 13, 12,
	{
		{  80,  80,  80, 16000,  5 },
		{  32, 160, 160, 16000,  2 },
		{  32, 160, 160, 16000,  2 },
		{  80,  80,  80, 16000,  5 },
		{ 160, 160, 160, 16000, 10 },
		{  80, 160, 160, 16000,  5 },
		{  32, 160, 160, 16000,  2 },
		{  32, 160, 160, 16000,  2 },
	},
	{ 0, 0, 0, 0, 0, 0, 1, 2 },
	310, 32, 8000, 254,
};
#endif

/* the host link's SPI, by board type: the bits sent LSB or MSB first */
static const SPI_InitTypeDef spiConfigs[2] = {
	{ SPI_Direction_2Lines_FullDuplex, SPI_Mode_Slave, SPI_DataSize_8b, SPI_CPOL_High,
	  SPI_CPHA_2Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_4, SPI_FirstBit_LSB, 7 },
	{ SPI_Direction_2Lines_FullDuplex, SPI_Mode_Slave, SPI_DataSize_8b, SPI_CPOL_High,
	  SPI_CPHA_2Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_4, SPI_FirstBit_MSB, 7 },
};

/* the host link's thresholds, by measurement type */
#if MOTION_IMAGE == 0
static const u32 hifThresholds[MOTION_TYPES] = { 2, 5, 5, 1, 2, 5 };
#else
static const u32 hifThresholds[MOTION_TYPES] = { 2, 5, 5, 1, 2, 5, 5, 5 };
#endif

#define ADC_SAMPLES	16
#define HIF_FRAMES	10

static I2cRegIf i2cRegIf(0);
static ExtiPin magDrdy(1, 8, 0);
static AK8975 ak8975(&i2cRegIf, &magDrdy);
static TimerChannel3 sampleChannel(0);
static SampleTimer sampleTimer(0, &sampleChannel);
static Adc adc(0, ADC_ExternalTrigConv_T1_CC3);
static AdcSample adcSamples[2 * ADC_SAMPLES];
static AdcSampler sampler(&sampleTimer, &adc, adcSamples, ADC_SAMPLES);
static Listener listeners[3];
#if MOTION_IMAGE == 0
/* PC10: the board type */
static ExtiPin boardPin(2, 10, 1);
static MotionProcessor processor(&sampler, &ak8975, &boardPin, listeners, 3, &motionConfig);
#else
static ExtiPin drdy(2, 10, 1);
static GpioOut stopLed(2, 6, 1);
static GpioOut magPower(0, 1, 1);
static GpioOut gyroStandby(2, 8, 0);
static MotionProcessor processor(&sampler, &ak8975, &magPower, &drdy, &stopLed, &gyroStandby,
				 listeners, 3, &motionConfig);
#endif
static ExtiPin hostCs(1, 12, 1);
static SpiSlave spi(1, &hostCs);
static GpioOut hifLine0(2, 7, 1);
static GpioOut hifLine1(2, 6, 1);
static MotionFrame frames[HIF_FRAMES];
static HifSpi hif(&processor, frames, HIF_FRAMES, hifThresholds, &spi, &hifLine0, &hifLine1);

/* the samples at 16 kHz on TIM1's channel 3; the host link as the board type says */
void InitDevices()
{
	i2cRegIf.Init(4, 1, 3, 1, DMA_Priority_High, DMA_Priority_High);
	magDrdy.Init(4, 1);
	sampler.Init(4, 0, DMA_Priority_Medium, 0, SYSCLK_Value, 16000);
	hostCs.Init(4, 1);
	processor.ResetBoardType();
	spi.Init(&spiConfigs[processor.BoardType()], 4, 1, DMA_Priority_VeryHigh, DMA_Priority_High);
	processor.Init();
	hif.Init();
}

void StartDevices()
{
	processor.StartSampling();
	hif.Start();
}

const DeviceInfo *GetDeviceInfo()
{
	return processor.GetDeviceInfo();
}

const u16 *GetChecksum()
{
	return processor.GetChecksum();
}

#if MOTION_IMAGE != 0
/* the host link stopped for the STOP mode */
void PrepareStop()
{
	hif.Suspend();
}

void ResumeFromStop()
{
	hif.Resume();
}

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
#endif
