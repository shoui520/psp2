/* RTC and scheduling callbacks shared by Sony's SQLite VFS implementations. */

#include <string.h>
#include <time.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include "vfs_time.h"

/* Seed with the current tick, then the process ID. */
int psp2Randomness(struct sqlite3_vfs *vfs, int size, char *buffer)
{
	SceRtcTick tick;
	SceUID pid;
	unsigned int count, extra;

	memset(buffer, 0, size);
	sceRtcGetCurrentTick(&tick);
	count = (unsigned int)size > sizeof(tick) ? sizeof(tick) : size;
	memcpy(buffer, &tick, count);
	if ((int)count < size) {
		pid = sceKernelGetProcessId();
		extra = size - count > sizeof(pid) ? sizeof(pid) : size - count;
		memcpy(buffer + count, &pid, extra);
		count += extra;
	}
	return count;
}

int psp2Sleep(struct sqlite3_vfs *vfs, int microseconds)
{
	sceKernelDelayThread(microseconds);
	return microseconds;
}

/* Current UTC time as a Julian day number. */
int psp2CurrentTime(struct sqlite3_vfs *vfs, double *time)
{
	SceDateTime now;
	time_t seconds = 0;

	sceRtcGetCurrentClock(&now, 0);
	sceRtcGetTime_t(&now, &seconds);
	*time = seconds / 86400.0 + 2440587.5 +
		(int)now.microsecond / 86400000000.0;
	return 0;
}
