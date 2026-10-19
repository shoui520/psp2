/*
 * SceGpuEs4CoreDump: text output, to tty0: or into a buffer (Sony's file
 * name unknown).
 */

#include <scetypes.h>
#include <iofilemgr/iofilemgr.h>
#include <stdarg.h>
#include <stdio.h>
#include "gpucoredump.h"

#define GPU_COREDUMP_LINE_MAX		0x200

/* to tty0:, attached to a process's console when pid is given */
void gpuCoreDumpLogOpen(GpuCoreDumpLog *log, SceUID pid)
{
	SceUID attach[2];

	log->pid = pid;
	log->fd = -1;
	log->cur = NULL;
	log->end = NULL;
	attach[0] = pid;
	attach[1] = 0;
	log->fd = sceIoOpen("tty0:", SCE_O_RDWR, 0606);
	if (log->fd >= 0 && pid != 0)
		sceIoIoctl(log->fd, 0x6400, attach, sizeof(attach), NULL, 0);
}

void gpuCoreDumpLogBuffer(GpuCoreDumpLog *log, char *buf, char *end)
{
	log->cur = buf;
	log->end = end;
	log->pid = 0;
	log->fd = -1;
}

void gpuCoreDumpLogClose(GpuCoreDumpLog *log)
{
	if (log->cur != NULL)
		return;
	if (log->pid != 0)
		sceIoIoctl(log->fd, 0x6401, NULL, 0, NULL, 0);
	if (log->fd != -1) {
		sceIoClose(log->fd);
		log->fd = -1;
	}
}

void gpuCoreDumpLogPrintf(GpuCoreDumpLog *log, const char *fmt, ...)
{
	char line[GPU_COREDUMP_LINE_MAX];
	va_list ap;
	int len;

	va_start(ap, fmt);
	if (log->cur != NULL) {
		if (log->cur + 1 <= log->end) {
			len = vsnprintf(log->cur, log->end - log->cur, fmt, ap);
			if (len >= 0) {
				log->cur += len;
				if (log->cur > log->end)
					log->cur = log->end;
			}
		}
	} else if (log->fd >= 0) {
		len = vsnprintf(line, sizeof(line), fmt, ap);
		line[sizeof(line) - 1] = '\0';
		sceIoWrite(log->fd, line, len);
	}
	va_end(ap);
}
