/*
 * SceGpuEs4CoreDump: resetting the captured values, and the dump files'
 * names and output (Sony's file name unknown).
 */

#include <scetypes.h>
#include <iofilemgr/iofilemgr.h>
#include <stdio.h>
#include <string.h>
#include "gpucoredump.h"
#include <gpucoredump_es4/gpucoredump_es4_imports.h>

#define GPU_COREDUMP_TRACE_CHUNK	0x100000

void gpuCoreDumpResetMaster(GpuCoreDumpMasterList *list)
{
	SceUInt32 i;

	for (i = 0; i < list->count; i++) {
		list->values[i].offset = GPU_COREDUMP_POISON;
		list->values[i].value = GPU_COREDUMP_POISON;
	}
	*list->dumped = 0;
}

void gpuCoreDumpResetCore(GpuCoreDumpCoreList *list)
{
	SceUInt32 i;
	int core;

	for (i = 0; i < list->count; i++) {
		list->values[i].offset = GPU_COREDUMP_POISON;
		for (core = 0; core != GPU_COREDUMP_CORES; core++)
			list->values[i].value[core] = 0;
	}
	*list->dumped = 0;
}

/* name unknown: the dump folder on the host (registry), 1 if set */
int gpuCoreDumpGetHostPath(char *path)
{
	int ret;

	memset(path, 0, GPU_COREDUMP_PATH_MAX);
	ret = sceRegMgrGetKeyStr("/CONFIG/COREDUMP", "dest_hostpath", path, GPU_COREDUMP_PATH_MAX);
	path[GPU_COREDUMP_PATH_MAX - 1] = '\0';
	if (ret != 0)
		return 0;
	return strnlen(path, GPU_COREDUMP_PATH_MAX) - 1 < GPU_COREDUMP_PATH_MAX - 1;
}

SceUID gpuCoreDumpOpen(const char *device, const char *dir, const char *sub, const SceDateTime *time,
			      const char *ext, char *path)
{
	int len;

	len = snprintf(path, GPU_COREDUMP_PATH_MAX, "%s%s%sgpu_core_dump_%04d_%02d_%02d_%02d%02d%02d%06d.%s",
		       device, dir, sub, time->year, time->month, time->day, time->hour, time->minute,
		       time->second, time->microsecond, ext);
	if (len >= GPU_COREDUMP_PATH_MAX)
		return -1;
	path[GPU_COREDUMP_PATH_MAX - 1] = '\0';
	return sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606);
}

int gpuCoreDumpSave(const char *device, const char *dir, const char *sub, const SceDateTime *time,
			   char *path)
{
	SceUID fd;

	fd = gpuCoreDumpOpen(device, dir, sub, time, "bin", path);
	if (fd < 0)
		return -1;
	return gpuCoreDumpWriteSegments(fd, gpuCoreDumpSegments, GPU_COREDUMP_SEGMENTS);
}

/* the trace buffer, in chunks, a dot for each */
int gpuCoreDumpWriteTrace(SceUID fd, const SceUInt8 *data, SceSize size, GpuCoreDumpLog *log)
{
	int total = 0;
	int len;

	while (size != 0) {
		len = sceIoWrite(fd, data, size < GPU_COREDUMP_TRACE_CHUNK ? size : GPU_COREDUMP_TRACE_CHUNK);
		if (len < 0)
			return 0;
		gpuCoreDumpLogPrintf(log, ".");
		total += len;
		data += len;
		size -= len;
	}
	return total;
}
