/*
 * Runtime: what the main loop runs: the tasks posted, then the idle task, until the
 * MCU wakes from the STOP mode; the timers on the timer slots; the reset two seconds
 * after it is asked for; the watchdog; and the C library's output, to the streams.
 */

extern "C" {
#include "stm32f10x_lib.h"
#include "cortexm3_macro.h"
}
#include <stdarg.h>
#include <stdio.h>
#include <yfuns.h>
#include "Debug.h"
#include "motion.h"
#include "BufferedUsartStream.h"
#include "Dispatcher.h"
#include "System.h"
#include "TimerSlot.h"

#define TASKS		11
#define TASK_WORDS	2

static Task tasks[TASKS];
static u32 taskData[TASKS][TASK_WORDS];
static Dispatcher dispatcher(tasks, taskData[0], TASKS, TASK_WORDS);
static Timer resetTimer;

/* by handle: stdin, stdout and stderr the console, 3 the debug port */
static Stream *const streams[5] = { &console, &console, &console, &debugStream, 0 };

size_t __write(int handle, const unsigned char *buffer, size_t size)
{
	if (handle >= 5)
		return (size_t)-1;
	return streams[handle]->Write(buffer, size);
}

bool PostTask(void (*func)(void *arg), void *arg)
{
	return dispatcher.Post((Task::Func)func, arg);
}

/* the tasks, and the idle function between them (IdleTask if none) */
void RunTasks(void (*idle)(void *arg), void *arg)
{
	Callback<void ()> callback;

	if (idle != 0)
		callback.Set(idle, arg);
	else
		callback.Set(IdleTask, 0);
	do {
		dispatcher.Dispatch();
		callback();
	} while (!IsStopped());
}

bool AddTimer(TimerSlotHandler *handler)
{
	return timerSlot.Add(handler);
}

bool RemoveTimer(TimerSlotHandler *handler)
{
	return timerSlot.Remove(handler);
}

static void ResetCallback(void *arg)
{
	NVIC_GenerateSystemReset();
}

void ScheduleReset()
{
	resetTimer.Start(2000, ResetCallback, 0);
}

/* the LSI's 40 kHz / 32, 1250 counts: about a second */
void StartWatchdog()
{
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	IWDG_SetPrescaler(IWDG_Prescaler_32);
	IWDG_SetReload(1249);
	IWDG_ReloadCounter();
	IWDG_Enable();
}

/* the timer slots' interrupt off; on waking, the main loop's dispatching ends */
void EnterStopMode()
{
	__SETPRIMASK();
	SysTick_ITConfig(DISABLE);
	__RESETPRIMASK();
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
	SetStopped(true);
}

/* the fatal error's message, to the debug port */
void Panic(const char *format, ...)
{
	char buffer[256];
	va_list ap;

	va_start(ap, format);
	__write(3, (const unsigned char *)buffer, vsprintf(buffer, format, ap));
}

/* the version and checksum the image carries, where the link configuration places
   them: at its head (0x08003200), and in its last word (0 in the image) */
extern "C" const u16 userVersion __attribute__((section(".userversion"))) = MOTION_USER_VERSION;
extern "C" const u32 firmwareChecksum __attribute__((section(".checksum"))) = 0;

static const u16 *UserVersionAddress()
{
	return &userVersion;
}

u16 UserVersion()
{
	return *UserVersionAddress();
}

static const u16 *FirmwareChecksumAddress()
{
	return (const u16 *)&firmwareChecksum;
}

u16 FirmwareChecksum()
{
	return *FirmwareChecksumAddress();
}
