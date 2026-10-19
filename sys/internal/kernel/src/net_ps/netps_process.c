/* Shared SceNetPs process bookkeeping; original source filename unavailable. */

#include <net_ps/netps.h>
#include <net_ps/netps_thread.h>
#include "netps_process.h"
#include <stddef.h>
#include "net_ps_internal.h"

SceNetPsProcess *gNetPsProcessHead;
SceNetPsProcess *gNetPsProcessTail;

/* Name unknown; retrieve the process's default network policy. */
int sceNetPsGetProcessPolicy(SceUID processId, int *policy)
{
	SceNetPsProcess *process;

	if (processId == gNetPsInitialProcessId) {
		*policy = 2;
		return 0;
	}
	for (process = gNetPsProcessHead; process; process = process->next) {
		if (process->processId == processId) {
			*policy = process->policy;
			return 0;
		}
	}
	return -22;
}

/* Name unknown; set the process's default network policy. */
int sceNetPsSetProcessPolicy(SceUID processId, int policy)
{
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (process->processId == processId) {
			process->policy = policy;
			return 0;
		}
	}
	return -22;
}

/* Name unknown; retrieve the process's network errno. */
int sceNetPsGetProcessError(SceUID processId, int *error)
{
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (processId == process->processId) {
			*error = process->error;
			return 0;
		}
	}
	return -22;
}

/* Name unknown; set the process's network errno. */
int sceNetPsSetProcessError(SceUID processId, int error)
{
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (processId == process->processId) {
			process->error = error;
			return 0;
		}
	}
	return -22;
}

/* Name unknown; return resource accounting for the calling process. */
SceNetPsResourceCounts *sceNetPsGetProcessResourceCounts(void)
{
	SceUID processId = sceNetPsGetProcessId();
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (processId == process->processId)
			return &process->resources;
	}
	return NULL;
}

/* Name unknown; enumerate protocol contexts in process-list order. */
void *sceNetPsGetNextProtocolContext(void *context)
{
	SceNetPsProcess *process = gNetPsProcessHead;
	void *previous;

	if (!process)
		return NULL;
	if (context) {
		do {
			previous = process->protocolContext;
			process = process->next;
			if (!process)
				return NULL;
		} while (context != previous);
	}
	return process->protocolContext;
}

/* Name unknown; retrieve the process's protocol context. */
void *sceNetPsGetProtocolContext(SceUID processId)
{
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (processId == process->processId)
			return process->protocolContext;
	}
	return NULL;
}

/* Name unknown; install the process's protocol context. */
int sceNetPsSetProtocolContext(SceUID processId, void *context)
{
	SceNetPsProcess *process;

	for (process = gNetPsProcessHead; process; process = process->next) {
		if (processId == process->processId) {
			process->protocolContext = context;
			return 0;
		}
	}
	return -12;
}

/* Name unknown; allocate the network-owned record for a process. */
int sceNetPsCreateProcess(SceUID processId)
{
	SceNetPsProcess *process = sceNetPsMalloc(sizeof(*process), 3, 8);

	if (!process)
		return 12;
	process->processId = processId;
	process->prev = gNetPsProcessTail;
	if (gNetPsProcessTail)
		gNetPsProcessTail->next = process;
	else
		gNetPsProcessHead = process;
	gNetPsProcessTail = process;
	process->next = NULL;
	return 0;
}

/* Name unknown; unlink and release a process's network-owned record. */
int sceNetPsDeleteProcess(SceUID processId)
{
	SceNetPsProcess *process = gNetPsProcessHead;

	while (process && process->processId != processId)
		process = process->next;
	if (!process)
		return 0;
	if (process->next)
		process->next->prev = process->prev;
	else
		gNetPsProcessTail = process->prev;
	if (process->prev)
		process->prev->next = process->next;
	else
		gNetPsProcessHead = process->next;
	sceNetPsFree(process);
	return 0;
}
