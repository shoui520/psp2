/*
 * SceDtcpIp (vs0:sys/external/libSceDtcpIp.suprx): the exported API. Every call checks the
 * library's state, builds the worker's command and hands it over through the command
 * condition, then returns; the worker thread carries the command out against the
 * middleware application and writes the result back into the command block.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <appmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the worker still holds a command out; the caller takes its result when it is done */
#define SCE_DTCP_IP_COMMAND_STILL_RUNNING	1

/* the command the worker finished, as the API hands it back (the command's event) */
static int dtcpIpCommandEventId(int *outEvent, int command)
{
	switch (command) {
	case SCE_DTCP_IP_COMMAND_CONNECT:
		*outEvent = 0;
		break;
	case SCE_DTCP_IP_COMMAND_ACTIVATE:
		*outEvent = 1;
		break;
	case SCE_DTCP_IP_COMMAND_CREATE_SINK:
		*outEvent = 2;
		break;
	case SCE_DTCP_IP_COMMAND_CLOSE_SINK:
		*outEvent = 3;
		break;
	case SCE_DTCP_IP_COMMAND_SET_URI:
		*outEvent = 4;
		break;
	case SCE_DTCP_IP_COMMAND_CLEAR_URI:
		*outEvent = 5;
		break;
	case SCE_DTCP_IP_COMMAND_OPEN_STREAM:
		*outEvent = 6;
		break;
	case SCE_DTCP_IP_COMMAND_SEEK_TIME:
		*outEvent = 7;
		break;
	case SCE_DTCP_IP_COMMAND_SEEK_BYTE:
		*outEvent = 8;
		break;
	case SCE_DTCP_IP_COMMAND_READ:
		*outEvent = 9;
		break;
	case SCE_DTCP_IP_COMMAND_CLOSE_STREAM:
		*outEvent = 10;
		break;
	case SCE_DTCP_IP_COMMAND_STOP_STREAM:
		*outEvent = 11;
		break;
	default:
		return SCE_DTCP_IP_ERROR_GENERIC;
	}
	return 0;
}

/*
 * SceDtcpIp_6AE48FE3: start the library. Allocates the shared heap, creates the locks,
 * the event flag and the worker thread (and the HTTP worker with it), and moves to the
 * idle state. heapSize is the shared memory the library allocates from, priority and
 * affinity the worker threads'; reserved4 must be zero.
 */
int SceDtcpIp_6AE48FE3(SceSize heapSize, SceInt32 priority, SceInt32 affinity, int reserved1,
		       int reserved2, int reserved3, int reserved4)
{
	SceUID worker;
	int ret;
	SceUID eventFlag;

	if (heapSize == 0 || reserved4 != 0)
		return SCE_DTCP_IP_ERROR_PARAM;
	if (g_dtcpIp.state != SCE_DTCP_IP_STATE_STOPPED)
		return SCE_DTCP_IP_ERROR_SEQUENCE;

	ret = dtcpIpCreateHeap(heapSize);
	if (ret < 0)
		return ret;
	g_command.common.id = SCE_DTCP_IP_COMMAND_NONE;

	ret = dtcpIpCreateLocks();
	if (ret < 0) {
destroyHeap:
		dtcpIpDestroyHeap();
		return ret;
	}

	eventFlag = sceKernelCreateEventFlag("SceDtcpIpWorkerEvent", 0, 0, NULL);
	ret = eventFlag;
	if (eventFlag < 0) {
destroyLocks:
		dtcpIpDestroyLocks();
		goto destroyHeap;
	}
	g_dtcpIp.workerEvent = eventFlag;

	worker = sceKernelCreateThread("SceDtcpIpWorkerThread", dtcpIpWorkerThread, priority,
				       0x1000, 0, affinity, NULL);
	ret = worker;
	if (worker < 0) {
eventFlagDropped:
		if (g_dtcpIp.workerEvent != -1)
			sceKernelDeleteEventFlag(g_dtcpIp.workerEvent);
		g_dtcpIp.workerThread = -1;
		g_dtcpIp.state = SCE_DTCP_IP_STATE_STOPPED;
		g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_IDLE;
		goto destroyLocks;
	}
	g_dtcpIp.workerThread = worker;

	ret = dtcpIpStartHttpWorker(priority, affinity);
	if (ret < 0) {
workerDropped:
		if (g_dtcpIp.workerThread != -1)
			sceKernelDeleteThread(g_dtcpIp.workerThread);
		g_dtcpIp.workerThread = -1;
		goto eventFlagDropped;
	}

	ret = sceKernelStartThread(g_dtcpIp.workerThread, 8, &g_dtcpIp);
	if (ret < 0) {
		dtcpIpStopHttpWorker();
		goto workerDropped;
	}

	g_dtcpIp.state = SCE_DTCP_IP_STATE_IDLE;
	g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_IDLE;
	if (affinity == 0)
		affinity = SCE_KERNEL_CPU_MASK_USER_ALL;
	g_dtcpIp.streamThreadAffinity = affinity;
	return 0;
}

/*
 * SceDtcpIp_82672A89: stop the library. Pauses and tears the sink manager down, tells
 * the worker to exit and waits for it, then frees the HTTP worker, the event flag, the
 * locks and the heap.
 */
int SceDtcpIp_82672A89(void)
{
	int ret = 0;
	int result;
	int stat = 0;
	SceUInt threadTimeout;
	SceUInt timeout;
	union SceDtcpIpCommand command;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_STOPPED)
		return SCE_DTCP_IP_ERROR_SEQUENCE;

	dtcpIpSessionCancelled();
	if (g_dtcpIp.manager != NULL) {
		result = g_dtcpIp.manager->pauseStream();
		if (result != 0)
			ret = result;
		result = g_dtcpIp.manager->teardown();
		if (result != 0)
			ret = result;
	}

	timeout = 10000000;
	if (dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND) >= 0) {
		if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_RUNNING ||
		    g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_QUEUED)
			dtcpIpWaitCond(SCE_DTCP_IP_COND_COMMAND, &timeout);
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
	}

	command.common.id = SCE_DTCP_IP_COMMAND_EXIT;
	result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
	if (result >= 0) {
		if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
		    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
		     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
			sceClibMemcpy_safe(&g_command, &command, sizeof(command));
			g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
			result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
		} else {
			result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
		}
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
	}
	if (result != 0)
		ret = result;

	result = dtcpIpStopHttpWorker();
	if (result != 0)
		ret = result;

	if (g_dtcpIp.workerThread > 0) {
		stat = 0;
		threadTimeout = 10000000;
		result = sceKernelWaitThreadEnd(g_dtcpIp.workerThread, &stat, &threadTimeout);
		if (result < 0)
			ret = result;
		result = sceKernelDeleteThread(g_dtcpIp.workerThread);
		if (result < 0)
			ret = result;
		g_dtcpIp.workerThread = -1;
	}

	command.common.id = SCE_DTCP_IP_COMMAND_NONE;
	if (g_dtcpIp.workerEvent != -1) {
		result = sceKernelDeleteEventFlag(g_dtcpIp.workerEvent);
		if (result < 0)
			ret = result;
		g_dtcpIp.workerEvent = -1;
	}

	result = dtcpIpDestroyLocks();
	if (result < 0)
		ret = result;
	result = dtcpIpDestroyHeap();
	if (result < 0)
		ret = result;

	g_dtcpIp.state = SCE_DTCP_IP_STATE_STOPPED;
	g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_IDLE;
	return ret;
}

/* SceDtcpIp_42679E06: connect to the middleware application */
int SceDtcpIp_42679E06(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IDLE) {
		command.common.id = SCE_DTCP_IP_COMMAND_CONNECT;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_F5CF6EBE: activate the middleware connection (fetch the device's keys) */
int SceDtcpIp_F5CF6EBE(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IDLE) {
		g_dtcpIp.mw.closed = 0;
		command.common.id = SCE_DTCP_IP_COMMAND_ACTIVATE;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_6D9B3939: close the middleware connection */
int SceDtcpIp_6D9B3939(void)
{
	int result = 0;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		result = SCE_DTCP_IP_ERROR_IPC;
	else if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		result = SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (result == 0) {
		if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IDLE) {
			result = dtcpIpCloseMwSession(&g_dtcpIp.mw);
			if (result == SCE_DTCP_IP_ERROR_IPC)
				g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_ERROR;
			else if (result == SCE_DTCP_IP_ERROR_IPC_TIMEOUT)
				g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_TIMEOUT;
		} else {
			return SCE_DTCP_IP_ERROR_SEQUENCE;
		}
	}
	return result;
}

/* SceDtcpIp_0AA185AC: the streaming connection's status word (while playing) */
int SceDtcpIp_0AA185AC(int *outInfo)
{
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING ||
	    g_dtcpIp.state == SCE_DTCP_IP_STATE_STREAM_DONE) {
		if (g_dtcpIp.manager != NULL)
			return g_dtcpIp.manager->getStreamInfo(outInfo);
		return SCE_DTCP_IP_ERROR_GENERIC;
	}
	return SCE_DTCP_IP_ERROR_SEQUENCE;
}

/* SceDtcpIp_55E2C3C4: the session buffer the middleware shares with the library */
int SceDtcpIp_55E2C3C4(int *outBuffer)
{
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING ||
	    g_dtcpIp.state == SCE_DTCP_IP_STATE_STREAM_DONE) {
		if (g_dtcpIp.manager != NULL)
			return g_dtcpIp.manager->getSessionBuffer(outBuffer);
		return SCE_DTCP_IP_ERROR_GENERIC;
	}
	return SCE_DTCP_IP_ERROR_SEQUENCE;
}

/* SceDtcpIp_30D1F84E: start playing the open stream */
int SceDtcpIp_30D1F84E(void)
{
	int result = 0;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		result = SCE_DTCP_IP_ERROR_IPC;
	else if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		result = SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (result == 0) {
		if (g_dtcpIp.state == SCE_DTCP_IP_STATE_STREAM_OPEN) {
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->startStream();
				if (result == 0)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_PLAYING;
			} else {
				return SCE_DTCP_IP_ERROR_GENERIC;
			}
		} else {
			return SCE_DTCP_IP_ERROR_SEQUENCE;
		}
	}
	return result;
}

/* SceDtcpIp_9697C0ED: pause the playing stream */
int SceDtcpIp_9697C0ED(void)
{
	int result = 0;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		result = SCE_DTCP_IP_ERROR_IPC;
	else if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		result = SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (result == 0) {
		if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING) {
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->pauseStream();
				g_dtcpIp.state = SCE_DTCP_IP_STATE_STREAM_OPEN;
				if (result == SCE_DTCP_IP_ERROR_IPC)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_ERROR;
				else if (result == SCE_DTCP_IP_ERROR_IPC_TIMEOUT)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_TIMEOUT;
			} else {
				return SCE_DTCP_IP_ERROR_GENERIC;
			}
		} else {
			return SCE_DTCP_IP_ERROR_SEQUENCE;
		}
	}
	return result;
}

/* SceDtcpIp_ADF09EF3: add a header to the sink's HTTP request (the uri is set) */
int SceDtcpIp_ADF09EF3(const char *name, const char *value, SceUInt32 mode)
{
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_SINK_OPEN ||
	    g_dtcpIp.state == SCE_DTCP_IP_STATE_URI_SET) {
		if (g_dtcpIp.manager != NULL)
			return g_dtcpIp.manager->addRequestHeader(name, value, mode);
		return SCE_DTCP_IP_ERROR_GENERIC;
	}
	return SCE_DTCP_IP_ERROR_SEQUENCE;
}

/* SceDtcpIp_C65C1309: remove a header from the sink's HTTP request */
int SceDtcpIp_C65C1309(const char *name)
{
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_SINK_OPEN ||
	    g_dtcpIp.state == SCE_DTCP_IP_STATE_URI_SET) {
		if (g_dtcpIp.manager != NULL)
			return g_dtcpIp.manager->removeRequestHeader(name);
		return SCE_DTCP_IP_ERROR_GENERIC;
	}
	return SCE_DTCP_IP_ERROR_SEQUENCE;
}

/* SceDtcpIp_F45F5693: create the sink manager. sinkType picks the class (the sink or
 * the move sink); parameter is the class's parameter string the worker frees. */
int SceDtcpIp_F45F5693(unsigned int sinkType, const char *parameter)
{
	union SceDtcpIpCommand command;
	SceSize length;
	char *copy;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IDLE) {
		if (parameter == NULL)
			return SCE_DTCP_IP_ERROR_PARAM;
		if (sinkType > 1)
			return SCE_DTCP_IP_ERROR_PARAM;
		length = sceClibStrnlen(parameter, 0x3FFF) + 1;
		copy = dtcpIpAllocWith(dtcpIpHeapMalloc, length);
		if (copy == NULL)
			return SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
		command.createSink.id = SCE_DTCP_IP_COMMAND_CREATE_SINK;
		command.createSink.sinkType = sinkType;
		command.createSink.parameter = copy;
		sceClibStrncpy(copy, parameter, length);
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_DE90746D: close the sink manager */
int SceDtcpIp_DE90746D(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_SINK_OPEN) {
		command.common.id = SCE_DTCP_IP_COMMAND_CLOSE_SINK;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_67F85C9C: set the uri the sink requests (the port the request goes to) */
int SceDtcpIp_67F85C9C(const char *uri, unsigned short port)
{
	union SceDtcpIpCommand command;
	SceSize length;
	char *copy;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_SINK_OPEN) {
		if (uri == NULL)
			return SCE_DTCP_IP_ERROR_PARAM;
		length = sceClibStrnlen(uri, 0x3FFF) + 1;
		copy = dtcpIpAllocWith(dtcpIpHeapMalloc, length);
		if (copy == NULL)
			return SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
		sceClibStrncpy(copy, uri, length);
		command.setUri.id = SCE_DTCP_IP_COMMAND_SET_URI;
		command.setUri.uri = copy;
		command.setUri.port = port;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_82BE2239: clear the uri the sink requests */
int SceDtcpIp_82BE2239(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_URI_SET) {
		command.common.id = SCE_DTCP_IP_COMMAND_CLEAR_URI;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_5E57BB11: open the stream at the uri. mode picks the stream's type;
 * parameter is the by-value DTCP-IP parameter the request carries. */
int SceDtcpIp_5E57BB11(const char *uri, unsigned int mode, const void *parameter, int reserved)
{
	union SceDtcpIpCommand command;
	SceSize length;
	char *copy;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_URI_SET) {
		if (uri == NULL)
			return SCE_DTCP_IP_ERROR_PARAM;
		if (mode > 2)
			return SCE_DTCP_IP_ERROR_PARAM;
		length = sceClibStrnlen(uri, 0x3FFF) + 1;
		copy = dtcpIpAllocWith(dtcpIpHeapMalloc, length);
		if (copy == NULL)
			return SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
		sceClibStrncpy(copy, uri, length);
		command.openStream.id = SCE_DTCP_IP_COMMAND_OPEN_STREAM;
		command.openStream.uri = copy;
		command.openStream.mode = mode;
		sceClibMemset(command.openStream.parameter, 0, 12);
		if (parameter != NULL)
			sceClibMemcpy_safe(command.openStream.parameter, parameter, 12);
		command.openStream.reserved = reserved;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_7DAFA54B: seek the playing stream to a time range (NPT seconds) */
int SceDtcpIp_7DAFA54B(SceUInt64 start, SceUInt64 end)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING) {
		if (end != 0 && start > end)
			return SCE_DTCP_IP_ERROR_PARAM;
		command.range.id = SCE_DTCP_IP_COMMAND_SEEK_TIME;
		command.range.start = start;
		command.range.end = end;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_DBF71527: seek the playing stream to a byte range */
int SceDtcpIp_DBF71527(SceUInt64 start, SceUInt64 end)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING) {
		if (end != 0 && start > end)
			return SCE_DTCP_IP_ERROR_PARAM;
		command.range.id = SCE_DTCP_IP_COMMAND_SEEK_BYTE;
		command.range.start = start;
		command.range.end = end;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_D8304EAE: read the playing stream into the caller's buffer */
int SceDtcpIp_D8304EAE(void *buffer, SceSize size, int argument, int *outSize)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING) {
		if (buffer == NULL || size == 0 || outSize == NULL)
			return SCE_DTCP_IP_ERROR_PARAM;
		command.read.id = SCE_DTCP_IP_COMMAND_READ;
		command.read.buffer = buffer;
		command.read.size = size;
		command.read.argument = argument;
		command.read.outSize = outSize;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_8557758E: stop the playing stream */
int SceDtcpIp_8557758E(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_PLAYING) {
		command.common.id = SCE_DTCP_IP_COMMAND_STOP_STREAM;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/* SceDtcpIp_0584D86E: close the open stream */
int SceDtcpIp_0584D86E(void)
{
	union SceDtcpIpCommand command;
	int result;

	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_ERROR)
		return SCE_DTCP_IP_ERROR_IPC;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_IPC_TIMEOUT)
		return SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
	if (g_dtcpIp.state == SCE_DTCP_IP_STATE_STREAM_OPEN ||
	    g_dtcpIp.state == SCE_DTCP_IP_STATE_STREAM_DONE) {
		command.common.id = SCE_DTCP_IP_COMMAND_CLOSE_STREAM;
		result = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (result >= 0) {
			if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_IDLE ||
			    (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE &&
			     command.common.id == SCE_DTCP_IP_COMMAND_EXIT)) {
				sceClibMemcpy_safe(&g_command, &command, sizeof(command));
				g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_QUEUED;
				result = sceKernelSetEventFlag(g_dtcpIp.workerEvent, 1);
			} else {
				result = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		}
	} else {
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return result;
}

/*
 * SceDtcpIp_6C6AC8E8: wait for the command the worker is holding out on (130 seconds),
 * and take its id and result. The id is the command's event (0 for the connection, 1
 * for the activation, and so on).
 */
int SceDtcpIp_6C6AC8E8(int *outEvent, int *outResult)
{
	SceUInt timeout;
	int event;
	int ret;

	if (outEvent != NULL && outResult != NULL) {
		timeout = 130000000;
		if (g_dtcpIp.state != SCE_DTCP_IP_STATE_STOPPED) {
			ret = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
			if (ret >= 0) {
				if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE ||
				    g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_RUNNING ||
				    g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_QUEUED) {
					if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE) {
take:
						event = 12;
						if (dtcpIpCommandEventId(&event, g_command.common.id) == 0) {
							*outEvent = event;
							*outResult = g_command.common.result;
						}
						g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_IDLE;
						g_command.common.result = SCE_DTCP_IP_ERROR_SEQUENCE;
						g_command.common.id = SCE_DTCP_IP_COMMAND_NONE;
					} else {
						while (1) {
							ret = dtcpIpWaitCond(SCE_DTCP_IP_COND_COMMAND, &timeout);
							if (ret < 0)
								break;
							if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE)
								goto take;
						}
					}
				} else {
					ret = SCE_DTCP_IP_ERROR_SEQUENCE;
				}
				dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
				return ret;
			}
			return ret;
		}
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return SCE_DTCP_IP_ERROR_PARAM;
}

/* SceDtcpIp_9108E525: the command the worker last finished, without waiting */
int SceDtcpIp_9108E525(int *outEvent, int *outResult)
{
	int event;
	int ret;

	if (outEvent != NULL && outResult != NULL) {
		if (g_dtcpIp.state != SCE_DTCP_IP_STATE_STOPPED) {
			ret = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
			if (ret >= 0) {
				if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_DONE) {
					event = 12;
					if (dtcpIpCommandEventId(&event, g_command.common.id) == 0) {
						*outEvent = event;
						*outResult = g_command.common.result;
					}
					g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_IDLE;
					g_command.common.id = SCE_DTCP_IP_COMMAND_NONE;
					g_command.common.result = SCE_DTCP_IP_ERROR_SEQUENCE;
				} else {
					ret = SCE_DTCP_IP_ERROR_SEQUENCE;
					if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_RUNNING ||
					    g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_QUEUED)
						ret = SCE_DTCP_IP_COMMAND_STILL_RUNNING;
				}
				dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
				return ret;
			}
			return ret;
		}
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	return SCE_DTCP_IP_ERROR_PARAM;
}

/* SceDtcpIp_CDDCA8E4: open the settings application's clock setup page, whose dialog
 * the user sets the system clock with (an expired device certificate asks for it) */
int SceDtcpIp_CDDCA8E4(void)
{
	return sceAppMgrLaunchAppByUri(327680, "settings_dlg:updater");
}

/* SceDtcpIp_3D67BF8E: mark the middleware session closed, the connect asks for it */
int SceDtcpIp_3D67BF8E(void)
{
	if (g_dtcpIp.state != SCE_DTCP_IP_STATE_IDLE)
		return SCE_DTCP_IP_ERROR_SEQUENCE;
	dtcpIpSessionClosed();
	return 0;
}

/*
 * the worker thread: waits for the API's event flag, carries the command in the
 * command block out against the sink manager, and writes the result back. The
 * argument block is the library's context; its event flag is the second word.
 */
int dtcpIpWorkerThread(SceSize args, void *argp)
{
	struct SceDtcpIpContext *context = argp;
	int exitLoop = 1;
	int ret;
	int result;

	while (1) {
		SceUInt32 outBits = 0;

		ret = sceKernelWaitEventFlag(context->workerEvent, 0xFFFFFFFF,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					     &outBits, NULL);
		if (ret < 0)
			break;
		ret = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (ret < 0)
			break;
		if (g_dtcpIp.commandState == SCE_DTCP_IP_COMMAND_QUEUED)
			g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_RUNNING;
		ret = dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		if (ret < 0)
			break;

		switch (g_command.common.id) {
		case SCE_DTCP_IP_COMMAND_EXIT:
			exitLoop = 0;
			goto handled;
		case SCE_DTCP_IP_COMMAND_CONNECT:
			g_command.common.result = dtcpIpOpenMwSession();
			goto handled;
		case SCE_DTCP_IP_COMMAND_ACTIVATE:
			g_command.common.result = dtcpIpCheckActivation(&g_dtcpIp.mw);
			goto handled;
		case SCE_DTCP_IP_COMMAND_CREATE_SINK: {
			unsigned int sinkType = g_command.createSink.sinkType;
			const struct SceDtcpIpSinkManager *manager;

			dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
			dtcpIpSessionClear();
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			g_dtcpIp.manager = NULL;
			if (sinkType != 0) {
				if (sinkType > 1)
					manager = NULL;
				else
					manager = dtcpIpGetMoveSinkManager();
			} else {
				manager = dtcpIpGetSinkManager();
			}
			g_dtcpIp.manager = manager;
			result = SCE_DTCP_IP_ERROR_PARAM;
			if (manager != NULL) {
				result = manager->create();
				if (result >= 0) {
					result = manager->setParameter(g_command.createSink.parameter);
					if (result >= 0)
						g_dtcpIp.state = SCE_DTCP_IP_STATE_SINK_OPEN;
				}
			}
			g_command.common.result = result;
			dtcpIpFreeWith(dtcpIpHeapFree, g_command.createSink.parameter);
			goto handled;
		}
		case SCE_DTCP_IP_COMMAND_CLOSE_SINK: {
			int closeResult = 0;

			if (g_dtcpIp.manager != NULL) {
				closeResult = g_dtcpIp.manager->close();
				g_dtcpIp.manager = NULL;
			}
			dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
			dtcpIpSessionClear();
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			g_dtcpIp.state = SCE_DTCP_IP_STATE_IDLE;
			g_command.common.result = closeResult;
			goto handled;
		}
		case SCE_DTCP_IP_COMMAND_SET_URI: {
			char *uri = g_command.setUri.uri;
			unsigned short port = g_command.setUri.port;

			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->setUri(uri, port);
				if (result >= 0)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_URI_SET;
			}
			g_command.common.result = result;
			dtcpIpFreeWith(dtcpIpHeapFree, uri);
			goto handled;
		}
		case SCE_DTCP_IP_COMMAND_CLEAR_URI:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->clearUri();
				if (result >= 0)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_SINK_OPEN;
			}
			g_command.common.result = result;
			goto handled;
		case SCE_DTCP_IP_COMMAND_OPEN_STREAM: {
			char *uri = g_command.openStream.uri;
			unsigned short mode = g_command.openStream.mode;

			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->openStream(uri, mode,
								       &g_command.openStream.parameter,
								       g_command.openStream.reserved,
								       g_dtcpIp.streamThreadAffinity);
				if (result >= 0) {
					g_dtcpIp.state = SCE_DTCP_IP_STATE_STREAM_OPEN;
					result = 0;
				}
			}
			g_command.common.result = result;
			dtcpIpFreeWith(dtcpIpHeapFree, uri);
			goto handled;
		}
		case SCE_DTCP_IP_COMMAND_READ:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->read(g_command.read.buffer, g_command.read.size,
								 g_command.read.argument,
								 g_command.read.outSize);
				if (result == SCE_DTCP_IP_ERROR_HTTP_CONNECTION_CLOSED)
					g_dtcpIp.state = SCE_DTCP_IP_STATE_STREAM_DONE;
			}
			g_command.common.result = result;
			goto handled;
		case SCE_DTCP_IP_COMMAND_SEEK_BYTE:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL)
				result = g_dtcpIp.manager->seekByte(g_command.range.start, g_command.range.end);
			g_command.common.result = result;
			goto handled;
		case SCE_DTCP_IP_COMMAND_SEEK_TIME:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL)
				result = g_dtcpIp.manager->seekTime(g_command.range.start, g_command.range.end);
			g_command.common.result = result;
			goto handled;
		case SCE_DTCP_IP_COMMAND_STOP_STREAM:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL)
				result = g_dtcpIp.manager->stopStream();
			g_command.common.result = result;
			goto handled;
		case SCE_DTCP_IP_COMMAND_CLOSE_STREAM:
			result = SCE_DTCP_IP_ERROR_GENERIC;
			if (g_dtcpIp.manager != NULL) {
				result = g_dtcpIp.manager->closeStream();
				g_dtcpIp.state = SCE_DTCP_IP_STATE_URI_SET;
			}
			g_command.common.result = result;
			goto handled;
		default:
			g_command.common.result = SCE_DTCP_IP_ERROR_GENERIC;
			goto handled;
		}

handled:
		ret = dtcpIpLock(SCE_DTCP_IP_LOCK_COMMAND);
		if (ret < 0)
			break;
		g_dtcpIp.commandState = SCE_DTCP_IP_COMMAND_DONE;
		ret = dtcpIpSignalCond(SCE_DTCP_IP_COND_COMMAND);
		if (ret < 0) {
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
			break;
		}
		if (g_command.common.result == SCE_DTCP_IP_ERROR_IPC)
			g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_ERROR;
		else if (g_command.common.result == SCE_DTCP_IP_ERROR_IPC_TIMEOUT)
			g_dtcpIp.state = SCE_DTCP_IP_STATE_IPC_TIMEOUT;
		ret = dtcpIpUnlock(SCE_DTCP_IP_LOCK_COMMAND);
		if (ret < 0 || exitLoop == 0)
			break;
	}

	if (ret == -2147319802)
		sceKernelExitDeleteThread(-2147319802);
	return ret;
}

/* The two 35-word configuration blocks the middleware's IPC create takes: the stream
 * connection's block is all zero, the activation's ends with its type 3 (read from the
 * original's data words at 0x8100921C and 0x81009190). */
const unsigned int g_dtcpIpIpcConfig[35] = { 0 };
const unsigned int g_dtcpIpActivateConfig[35] = { [34] = 3 };
