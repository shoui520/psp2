/*
 * SceDtcpIp: the library's five locks and three conditions. The whole library
 * serializes through them: the manager lock holds the session, the HTTP lock the
 * request, the command lock the worker's command block, and the HTTP worker and the
 * authentication each carry their own.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "dtcpip_internal.h"

/* the library's locks, in the order the library creates them */
static SceKernelLwMutexWork g_httpMutex;
static SceKernelLwMutexWork g_commandMutex;
static SceKernelLwMutexWork g_httpWorkerMutex;
static SceKernelLwMutexWork g_authMutex;
static SceKernelLwCondWork g_commandCond;
static SceKernelLwCondWork g_httpWorkerCond;
static SceKernelLwCondWork g_authCond;
static SceKernelLwMutexWork g_managerMutex;

int dtcpIpCreateLocks(void)
{
	int ret;

	ret = sceKernelCreateLwMutex(&g_managerMutex, "SceDtcpIpMgrLwMutex", 0, 0, NULL);
	if (ret >= 0) {
		ret = sceKernelCreateLwMutex(&g_httpMutex, "SceDtcpIpHttpLwMutex", 0, 0, NULL);
		if (ret < 0) {
managerDropped:
			sceKernelDeleteLwMutex(&g_managerMutex);
			return ret;
		}
		ret = sceKernelCreateLwMutex(&g_commandMutex, "SceDtcpIpCommandLwMutex", 0, 0, NULL);
		if (ret < 0) {
httpDropped:
			sceKernelDeleteLwMutex(&g_httpMutex);
			goto managerDropped;
		}
		ret = sceKernelCreateLwMutex(&g_httpWorkerMutex, "SceDtcpIpHttpWorkerThreadLwMutex", 0, 0,
					     NULL);
		if (ret < 0) {
commandDropped:
			sceKernelDeleteLwMutex(&g_commandMutex);
			goto httpDropped;
		}
		ret = sceKernelCreateLwMutex(&g_authMutex, "SceDtcpIpAuthLwMutex", 0, 0, NULL);
		if (ret < 0) {
httpWorkerDropped:
			sceKernelDeleteLwMutex(&g_httpWorkerMutex);
			goto commandDropped;
		}
		ret = sceKernelCreateLwCond(&g_commandCond, "SceDtcpIpCommandLwCond", 0, &g_commandMutex,
					    NULL);
		if (ret < 0) {
authDropped:
			sceKernelDeleteLwMutex(&g_authMutex);
			goto httpWorkerDropped;
		}
		ret = sceKernelCreateLwCond(&g_httpWorkerCond, "SceDtcpIpHttpWorkerThreadLwCond", 0,
					    &g_httpWorkerMutex, NULL);
		if (ret < 0) {
commandCondDropped:
			sceKernelDeleteLwCond(&g_commandCond);
			goto authDropped;
		}
		ret = sceKernelCreateLwCond(&g_authCond, "SceDtcpIpAuthLwCond", 0, &g_authMutex, NULL);
		if (ret < 0) {
			sceKernelDeleteLwCond(&g_httpWorkerCond);
			goto commandCondDropped;
		}
	}
	return ret;
}

int dtcpIpDestroyLocks(void)
{
	int ret = 0;
	int result;

	result = sceKernelDeleteLwMutex(&g_managerMutex);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwMutex(&g_httpMutex);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwMutex(&g_commandMutex);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwMutex(&g_httpWorkerMutex);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwMutex(&g_authMutex);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwCond(&g_commandCond);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwCond(&g_httpWorkerCond);
	if (result < 0)
		ret = result;
	result = sceKernelDeleteLwCond(&g_authCond);
	if (result < 0)
		ret = result;
	return ret;
}

int dtcpIpLock(unsigned int lock)
{
	SceKernelLwMutexWork *work;

	switch (lock) {
	case SCE_DTCP_IP_LOCK_MANAGER:
		work = &g_managerMutex;
		break;
	case SCE_DTCP_IP_LOCK_HTTP:
		work = &g_httpMutex;
		break;
	case SCE_DTCP_IP_LOCK_COMMAND:
		work = &g_commandMutex;
		break;
	case SCE_DTCP_IP_LOCK_HTTP_WORKER:
		work = &g_httpWorkerMutex;
		break;
	case SCE_DTCP_IP_LOCK_AUTH:
		work = &g_authMutex;
		break;
	default:
		work = NULL;
		break;
	}
	return sceKernelLockLwMutex(work, 1, NULL);
}

int dtcpIpUnlock(unsigned int lock)
{
	SceKernelLwMutexWork *work;

	switch (lock) {
	case SCE_DTCP_IP_LOCK_MANAGER:
		work = &g_managerMutex;
		break;
	case SCE_DTCP_IP_LOCK_HTTP:
		work = &g_httpMutex;
		break;
	case SCE_DTCP_IP_LOCK_COMMAND:
		work = &g_commandMutex;
		break;
	case SCE_DTCP_IP_LOCK_HTTP_WORKER:
		work = &g_httpWorkerMutex;
		break;
	case SCE_DTCP_IP_LOCK_AUTH:
		work = &g_authMutex;
		break;
	default:
		work = NULL;
		break;
	}
	return sceKernelUnlockLwMutex(work, 1);
}

int dtcpIpWaitCond(unsigned int cond, SceUInt *timeout)
{
	SceKernelLwCondWork *work;

	if (cond > 2) {
		if (cond <= 3) {
			work = &g_httpWorkerCond;
			return sceKernelWaitLwCond(work, timeout);
		}
		if (cond <= 4) {
			work = &g_authCond;
			return sceKernelWaitLwCond(work, timeout);
		}
		return sceKernelWaitLwCond(NULL, timeout);
	}
	if (cond <= 1)
		return sceKernelWaitLwCond(NULL, timeout);
	work = &g_commandCond;
	return sceKernelWaitLwCond(work, timeout);
}

int dtcpIpSignalCond(unsigned int cond)
{
	SceKernelLwCondWork *work;

	if (cond <= 2) {
		if (cond > 1)
			return sceKernelSignalLwCondAll(&g_commandCond);
		return sceKernelSignalLwCond(NULL);
	}
	if (cond <= 3) {
		work = &g_httpWorkerCond;
	} else {
		if (cond > 4)
			return sceKernelSignalLwCond(NULL);
		work = &g_authCond;
	}
	return sceKernelSignalLwCond(work);
}
