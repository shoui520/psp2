/*
 * The debugger's SceCoredump callbacks: the progress and the result of a
 * core dump it triggered are queued for the worker (Sony's
 * coredump_handler.c).
 */

#include <coredump/coredump.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include "dbgp_internal.h"

/* the process status bit set while a core dump of it runs */
#define DBGP_PROC_COREDUMP	0x80000

/* the path lengths the result fits in proc->coredumpPath */
#define COREDUMP_PATH_MIN	2
#define COREDUMP_PATH_MAX	0x532

/* the last progress the worker reports; 100 comes with the result */
#define COREDUMP_PROGRESS_MAX	99

int coredumpHandlerResultCallback(int task_id, SceUID pid, int result,
				  const char *path, SceSize path_len,
				  int encrypted)
{
	CoredumpInfo info;
	DbgpProcCB *proc;
	int ret;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	dbgpSetProcStatus(proc, DBGP_PROC_COREDUMP, 0);
	if (proc->pid != pid)
		return 0;
	if (path == NULL)
		return SCE_PRINTF_WITH_INFO(15, 55, "0x%08x\n", path_len);
	if (path_len - COREDUMP_PATH_MIN > COREDUMP_PATH_MAX - COREDUMP_PATH_MIN)
		return SCE_PRINTF_WITH_INFO(15, 63, "0x%08x\n", path_len);
	info.pid = proc->pid;
	info.task_id = task_id;
	info.type = COREDUMP_INFO_RESULT;
	info.progress = 0;
	info.result = result;
	info.path = proc->coredumpPath;
	info.pathSize = path_len;
	strncpy(proc->coredumpPath, path, path_len);
	ret = coredumpInfoSave(proc, &info);
	if (ret < 0)
		return SCE_PRINTF_WITH_INFO(15, 83, "0x%08x\n", ret);
	return ret;
}

int coredumpHandlerProgressCallback(int task_id, SceUID pid, int progress)
{
	CoredumpInfo info;
	DbgpProcCB *proc;
	int ret;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid ||
	    (SceUInt32)progress > COREDUMP_PROGRESS_MAX)
		return (int)proc;
	info.pid = proc->pid;
	info.task_id = task_id;
	info.progress = progress;
	info.type = COREDUMP_INFO_PROGRESS;
	info.result = 0;
	info.path = NULL;
	info.pathSize = 0;
	ret = coredumpInfoSave(proc, &info);
	if (ret < 0)
		return SCE_PRINTF_WITH_INFO(15, 126, "0x%08x\n", ret);
	return ret;
}
