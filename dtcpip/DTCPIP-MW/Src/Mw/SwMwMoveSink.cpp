/*
 * The move sink object (SwMwMoveSink.cpp): create/destroy register the DTCP move session
 * with the SWS core, setUri resolves the move URI and hands it to the content object, and
 * the close/result pair is shared with the 0x2804 command.
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
#include "SwMwMoveSink.h"

#define SWMW_MOVESINK_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMoveSink.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the SWS core's move-sink hooks (the linked core) */
int SwMwCoreCheckMoveSink(int device);		/* 0x81002d94 */
int SwMwCoreMoveLabel(int device);		/* 0x81002d12 */
int SwMwCoreContentId(int device);		/* 0x81002d00 */
void *SwMwCoreContent(int device);		/* 0x81002cd0 */
int SwMwCoreRegisterMoveSink(int device, void *sink);	/* 0x81002d5c */
void SwMwCoreUnregisterMoveSink(int device, void *sink);	/* 0x81002d78 */

/* the support library's base object around the URI (0x8107b464/0x8107b44c) */
void *SwMwMoveSinkBaseConstruct(void *sink);
void *SwMwMoveSinkBaseDestruct(void *sink);
}

/* the move sink's lwmutex, "sce_sws_mutex" (0x810071cc/0x81007208) */
extern "C" int SwMwMoveSinkMutexCreate(SceKernelLwMutexWork *mutex)
{
	int result;

	result = sceKernelCreateLwMutex(mutex, "sce_sws_mutex", 0x2000, 0, NULL);
	if (result >= 0)
		return 0;
	return result;
}

extern "C" int SwMwMoveSinkMutexDelete(SceKernelLwMutexWork *mutex)
{
	return sceKernelDeleteLwMutex(mutex);
}

/* lock and unlock the move sink's mutex (0x810073ac/0x810073ce) */
extern "C" int SwMwMoveSinkMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwMoveSinkMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* create the channel's move sink on its device (0x81007222) */
extern "C" int SwMwMoveSinkCreate(int *out, int device)
{
	struct SwMwMoveSink *sink = NULL;
	int status;
	int failed;

	if (device == 0)
		return 12003;
	if (SwMwCoreCheckMoveSink(device) == 0)
		return 12003;
	sink = (struct SwMwMoveSink *)::operator new(0x280, std::nothrow);
	if (sink != NULL)
		sink = (struct SwMwMoveSink *)SwMwMoveSinkBaseConstruct(sink);
	if (sink == NULL)
		return 12008;
	sink->device = device;
	sink->moveLabel = SwMwCoreMoveLabel(device);
	sink->contentId = SwMwCoreContentId(device);
	sink->content = (struct SwMwContent *)SwMwCoreContent(device);
	sink->moveStatus = 0;
	sink->moveState = -1;
	sink->moveResult = 0;
	sink->notified = 0;
	sink->finished = 0;
	sink->result = 0;
	sink->sessionState[0] = 0;
	sink->sessionState[1] = 0;
	sink->sessionState[2] = 0;
	sink->sessionState[3] = 0;
	sink->sessionState[4] = 0;
	sink->sessionState[5] = -1;
	sink->sessionState[6] = 0;
	sink->sessionState[7] = 0;
	failed = 0;
	if (SwMwMoveSinkMutexCreate(&sink->mutex) != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 71);
		failed = 1;
	}
	if (failed != 0) {
		status = 12009;
	} else {
		status = SwMwCoreRegisterMoveSink(device, sink);
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 77);
			failed = 1;
		}
		if (failed == 0) {
			*out = (int)sink;
			return 0;
		}
		SwMwMoveSinkMutexDelete(&sink->mutex);
	}
	::operator delete(SwMwMoveSinkBaseDestruct(sink));
	return status;
}

/* destroy the move sink (0x8100771c) */
extern "C" int SwMwMoveSinkDestroy(int handle)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)handle;
	unsigned char value[4];

	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 227);
	if (sink != NULL) {
		SwMwCoreUnregisterMoveSink(sink->device, sink);
		SwMwMoveSinkClose((int)sink, value, 1);
		SwMwMoveSinkMutexDelete(&sink->mutex);
		::operator delete(SwMwMoveSinkBaseDestruct(sink));
	}
	return 0;
}

/* give the move sink its URI and start the move (0x810073ec) */
extern "C" int SwMwMoveSinkSetUri(int handle, void *uriText, int size)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)handle;
	struct SwMwUri text;
	unsigned short length;
	unsigned char address[32];
	unsigned char parsed[4];
	int status;
	int failed;

	length = (unsigned short)size;
	if (SwMwNetParseAddress(2, (const char *)uriText, &parsed) != 0) {
		status = SwMwNetResolveName((const char *)uriText, length, 2, &parsed);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 108);
			failed = 1;
		}
		if (failed != 0)
			return 12202;
		status = SwMwNetFormatAddress(2, &parsed, (char *)address, 32);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 114);
			failed = 1;
		}
		if (failed != 0)
			return 12202;
		uriText = address;
	}
	SwMwUriInit(&text, (const char *)uriText);
	status = SwMwUriSet(&sink->uri, &text, length);
	SwMwUriDone(&text);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 122);
		failed = 1;
	}
	if (failed == 0) {
		failed = 0;
		if (SwMwUriIsSet(&sink->uri) == 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 123);
			failed = 1;
		}
		if (failed != 0)
			return SwMwStatus(1);
		sink->moveStatus = 0;
		SwMwMoveSinkMutexLock(&sink->mutex);
		sink->notified = 0;
		sink->finished = 0;
		SwMwMoveSinkMutexUnlock(&sink->mutex);
		status = sink->content->setUri(&sink->moveStatus, &sink->uri, 0);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 139);
			failed = 1;
		}
		if (failed != 0)
			return SwMwStatus(status);
		sink->result = sink->moveStatus;
		return 12007;
	}
	return SwMwStatus(status);
}

/* the 0x2804 command: the move result (0x81007700) */
extern "C" int SwMwMoveSinkGetUriResult(int handle, void *out, int value)
{
	return SwMwMoveSinkClose(handle, out, value);
}
