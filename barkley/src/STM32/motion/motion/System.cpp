/*
 * System: the program's own devices: the timer slots on SysTick, the LEDs, the console
 * on USART1 (its output discarded while PA4 is high), and the flag that ends the main
 * loop's dispatching when the MCU wakes from the STOP mode.
 */

#include "Board.h"
#include "BufferedUsartStream.h"
#include "Led.h"
#include "System.h"
#include "TimerSlot.h"

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
static ExtiPin consoleOff(0, 4, 1);
static CountBlinker idleBlinker(&led0, 200000);
static PeriodicTimer tickTimer;
static Ticker ticker(&tickTimer, 0);
static bool stopped;

void InitDebug()
{
}

/* the board, the timer slots (a tick every 20000 cycles, 10 ms elapsed) and the console */
void InitSystem()
{
	SetStopped(false);
	InitBoard();
	timerSlot.Init(4, 1, 20000, 10);
	console.Init(3, 0, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No,
		     USART_HardwareFlowControl_None);
	console.SetDiscard(consoleOff.Get() != 0);
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
