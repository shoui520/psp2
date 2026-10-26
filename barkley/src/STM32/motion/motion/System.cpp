/*
 * System: the program's own devices: the timer slots on SysTick, the LEDs, the console
 * on USART1 (its output discarded while PA4 is high), and the flag that ends the main
 * loop's dispatching when the MCU wakes from the STOP mode. Image 00 (branch U0652) has
 * no STOP mode and no console switch, and sets up the board's pins here.
 */

extern "C" {
#include "stm32f10x_lib.h"
}
#include "Board.h"
#include "BufferedUsartStream.h"
#include "Led.h"
#include "System.h"
#include "TimerSlot.h"
#include "stm32f10x_it.h"

const s8 ledPatterns[4][2] = { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };

static TimerSlotHandler *slots[4];
TimerSlot timerSlot(slots, 4);
static GpioOut led0(2, 12, 1);
static GpioOut led1(2, 11, 1);
static LedPair leds(&led0, &led1);
static u8 consoleBuffer[512];
BufferedUsartStream console(0, 0, consoleBuffer, sizeof(consoleBuffer), 128);
UsartStream debugStream(0, 0);
static PeriodicTimer blinkTimer;
static Blinker blinker(&led1, &blinkTimer);
#if MOTION_IMAGE != 0
static ExtiPin consoleOff(0, 4, 1);
#endif
static CountBlinker idleBlinker(&led0, 200000);
#if MOTION_IMAGE != 0
static PeriodicTimer tickTimer;
static Ticker ticker(&tickTimer, 0);
static bool stopped;
#endif

void InitDebug()
{
}

/* the board, the timer slots (a tick every 20000 cycles, 10 ms elapsed) and the console */
void InitSystem()
{
#if MOTION_IMAGE != 0
	SetStopped(false);
#endif
	InitBoard();
	timerSlot.Init(4, 1, 20000, 10);
	console.Init(3, 0, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No,
		     USART_HardwareFlowControl_None);
#if MOTION_IMAGE != 0
	console.SetDiscard(consoleOff.Get() != 0);
#endif
	InitDebug();
}

void StartSystem()
{
	timerSlot.Start();
	blinker.Start(1000);
}

/* the task run when no other is queued */
void IdleTask(void *arg)
{
	idleBlinker.Tick();
}

#if MOTION_IMAGE != 0
/* after the STOP mode: the clocks and the timer slots again */
void ResumeSystem()
{
	SetStopped(false);
	RestoreClocks();
	timerSlot.Resume();
}

void SetStopped(bool value)
{
	stopped = value;
}

bool IsStopped()
{
	return stopped;
}
#endif

#if MOTION_IMAGE == 0
/* each port's pins (PC7 is in two lists: an output last) */
static const GPIO_InitTypeDef portAPins[5] = {
	{ GPIO_Pin_0 | GPIO_Pin_6 | GPIO_Pin_7, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_11 |
	  GPIO_Pin_12 | GPIO_Pin_14, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_10 | GPIO_Pin_13 | GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPU },
	{ GPIO_Pin_1, GPIO_Speed_50MHz, GPIO_Mode_Out_OD },
	{ GPIO_Pin_9, GPIO_Speed_50MHz, GPIO_Mode_AF_PP },
};

static const GPIO_InitTypeDef portBPins[5] = {
	{ GPIO_Pin_0 | GPIO_Pin_1, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_12, GPIO_Speed_50MHz, GPIO_Mode_IN_FLOATING },
	{ GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_13 |
	  GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_4, GPIO_Speed_50MHz, GPIO_Mode_IPU },
	{ GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_14, GPIO_Speed_50MHz, GPIO_Mode_AF_OD },
};

static const GPIO_InitTypeDef portCPins[5] = {
	{ GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_4 | GPIO_Pin_5, GPIO_Speed_50MHz, GPIO_Mode_AIN },
	{ GPIO_Pin_1 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_13 | GPIO_Pin_14 |
	  GPIO_Pin_15, GPIO_Speed_50MHz, GPIO_Mode_IPD },
	{ GPIO_Pin_10, GPIO_Speed_50MHz, GPIO_Mode_IPU },
	{ GPIO_Pin_6, GPIO_Speed_50MHz, GPIO_Mode_Out_OD },
	{ GPIO_Pin_7 | GPIO_Pin_11 | GPIO_Pin_12, GPIO_Speed_50MHz, GPIO_Mode_Out_PP },
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

/* every pin; PC6, PC11 and PC12 high before they are outputs */
static void SetupGpio()
{
	u32 i;

	for (i = 0; i < countof(portAPins); i++)
		GPIO_Init(GPIOA, (GPIO_InitTypeDef *)&portAPins[i]);
	for (i = 0; i < countof(portBPins); i++)
		GPIO_Init(GPIOB, (GPIO_InitTypeDef *)&portBPins[i]);
	GPIO_SetBits(GPIOC, GPIO_Pin_6 | GPIO_Pin_11 | GPIO_Pin_12);
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
}

void InitBoard()
{
	SetupGpio();
	SCB->VTOR = (u32)__vector_table;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);
	SetSharedIrqPriorities(sharedIrqPriorities);
}
#endif
