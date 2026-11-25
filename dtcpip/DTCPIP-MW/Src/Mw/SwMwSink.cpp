/*
 * The sink object (SwMwSink.cpp): the per-sink content session behind the library's
 * 0x2201 connect. Its create/destroy register the sink with the SWS core, its close
 * carries the close/status handshake the 0x2202 and 0x2204 commands share, and its URI
 * (a support-library URI value) is handed to the content object.
 */

#include <stddef.h>
#include <string.h>

/* the SDK's <new> asks std::size_t of the freestanding headers */
namespace std {
using ::size_t;
}
#include <new>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwSink.h"

#define SWMW_SINK_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwSink.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the SWS core's sink hooks (the linked core) */
int SwMwCoreIsSinkConnected(int device);	/* 0x81002da6 */
void *SwMwCoreContent(int device);		/* 0x81002cd0 */
int SwMwCoreNotifyDevice(int device, int value);	/* 0x81003d46 */
int SwMwCoreRegisterSink(int device, void *sink);	/* 0x81002d24 */
void SwMwCoreUnregisterSink(int device, void *sink);	/* 0x81002d40 */

/* the support library's base object around the URI (0x8107b94c/0x8107b934) */
void *SwMwSinkBaseConstruct(void *sink);
void *SwMwSinkBaseDestruct(void *sink);
}

/* the sink's lwmutex, "sce_sws_mutex" (0x8100c894/0x8100c8d0) */
extern "C" int SwMwSinkMutexCreate(SceKernelLwMutexWork *mutex)
{
	int result;

	result = sceKernelCreateLwMutex(mutex, "sce_sws_mutex", 0x2000, 0, NULL);
	if (result >= 0)
		return 0;
	return result;
}

extern "C" int SwMwSinkMutexDelete(SceKernelLwMutexWork *mutex)
{
	return sceKernelDeleteLwMutex(mutex);
}

/* lock and unlock the sink's mutex (0x8100ca12/0x8100ca34) */
extern "C" int SwMwSinkMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwSinkMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* create the channel's sink on its device (0x8100c8ea) */
extern "C" int SwMwSinkCreate(int *out, int device)
{
	struct SwMwSink *sink = NULL;
	int status;
	int failed;

	sink = (struct SwMwSink *)::operator new(sizeof(struct SwMwSink), std::nothrow);
	if (sink != NULL)
		sink = (struct SwMwSink *)SwMwSinkBaseConstruct(sink);
	if (sink == NULL)
		return 12008;
	sink->device = device;
	sink->connected = SwMwCoreIsSinkConnected(device) == 0;
	sink->uriSet = 0;
	sink->content = (struct SwMwContent *)SwMwCoreContent(device);
	sink->closed = 0;
	sink->status = 0;
	sink->notified = 0;
	failed = 0;
	if (SwMwSinkMutexCreate(&sink->mutex) != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_SINK_SOURCE, 44);
		failed = 1;
	}
	if (failed != 0) {
		status = 12009;
	} else {
		status = SwMwCoreRegisterSink(device, sink);
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_SINK_SOURCE, 50);
			failed = 1;
		}
		if (failed == 0) {
			*out = (int)sink;
			return 0;
		}
		SwMwSinkMutexDelete(&sink->mutex);
	}
	::operator delete(SwMwSinkBaseDestruct(sink));
	return status;
}

/* destroy the sink (0x8100ce56) */
extern "C" int SwMwSinkDestroy(int handle)
{
	struct SwMwSink *sink = (struct SwMwSink *)handle;

	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE, 233);
	if (sink != NULL) {
		SwMwCoreUnregisterSink(sink->device, sink);
		SwMwSinkClose((int)sink, 1);
		SwMwSinkMutexDelete(&sink->mutex);
		::operator delete(SwMwSinkBaseDestruct(sink));
	}
	return 0;
}

/* the close/status handshake: other modes close, mode 0 answers the status (0x8100cd3c) */
extern "C" int SwMwSinkClose(int handle, int mode)
{
	struct SwMwSink *sink = (struct SwMwSink *)handle;
	int value;
	int status;

	if (mode != 0) {
		if (sink->notified != 0) {
			sink->notified = 0;
			if (sink->content->close(&sink->uri) != 0)
				swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE,
					  145);
		}
		sink->closed = 1;
		return 12015;
	}
	SwMwSinkMutexLock(&sink->mutex);
	if (sink->closed == 0) {
		status = 12007;
	} else {
		value = sink->status;
		if (value > 204) {
			if (value > 1005) {
				if (value == 1026)
					sink->notified = 0;
				status = 12204;
			} else if (value <= 206) {
				status = 12203;
			} else if (value > 1004) {
				status = value;
			} else {
				status = 12204;
			}
		} else if (value > 0) {
			if (value == 102)
				status = 12204;
			else
				status = 12204;
		} else if (value >= 0) {
			status = value;
		} else {
			status = 12204;
		}
	}
	SwMwSinkMutexUnlock(&sink->mutex);
	return status;
}

/* the 0x2204 command: the close status (0x8100ce3e) */
extern "C" int SwMwSinkGetUriResult(int handle, int value)
{
	return SwMwSinkClose(handle, value);
}

/* mark the sink closed with a status (0x8100ceba) */
extern "C" int SwMwSinkSetStatus(int handle, int status)
{
	struct SwMwSink *sink = (struct SwMwSink *)handle;

	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE, 258);
	SwMwSinkMutexLock(&sink->mutex);
	sink->closed = 1;
	sink->status = status;
	return SwMwSinkMutexUnlock(&sink->mutex);
}

/* the sink's URI state (0x8100cf1e) */
extern "C" int SwMwSinkGetUriState(int handle)
{
	struct SwMwSink *sink = (struct SwMwSink *)handle;

	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE, 281);
	return sink->uriSet;
}

/* notify the sink's device of a value (0x8100cf08) */
extern "C" int SwMwSinkDeviceNotify(int handle, int value)
{
	struct SwMwSink *sink = (struct SwMwSink *)handle;

	return SwMwCoreNotifyDevice(sink->device, value);
}

/* the content information the descriptor carries (0x8100c82e) */
extern "C" void SwMwContentGetInfo(void *descriptor, void *info)
{
	int *source = (int *)descriptor;

	memcpy(info, source + 1, 16);
	memcpy((int *)info + 4, source + 5, 16);
	((int *)info)[8] = source[9];
	memcpy((int *)info + 9, source + 10, 0x20);
}

/* the content descriptor's status (0x8100c882) */
extern "C" int SwMwContentGetStatus(void *descriptor)
{
	return *(int *)descriptor;
}
