/*
 * System: what the program's objects share: the task dispatcher the main loop runs,
 * the timer slots, the watchdog, the STOP mode.
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include "stm32f10x_type.h"

class BufferedUsartStream;
class TimerSlot;
class TimerSlotHandler;
class UsartStream;

extern TimerSlot timerSlot;
extern BufferedUsartStream console;
extern UsartStream debugStream;

void InitSystem();
void StartSystem();
void IdleTask(void *arg);
void ResumeSystem();
void SetStopped(bool value);
bool IsStopped();

bool PostTask(void (*func)(void *arg), void *arg);
void RunTasks(void (*idle)(void *arg), void *arg);
bool AddTimer(TimerSlotHandler *handler);
bool RemoveTimer(TimerSlotHandler *handler);
void ScheduleReset();
void StartWatchdog();
void EnterStopMode();
void PrepareStop();
u16 UserVersion();
u16 FirmwareChecksum();

#endif
