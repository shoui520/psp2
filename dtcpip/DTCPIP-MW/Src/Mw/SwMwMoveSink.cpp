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
int SwMwCoreCheckMoveSink(int device);		/* */
int SwMwCoreDataLabel(int device);		/* */
int SwMwCoreMoveManager(int device);		/* */
void *SwMwCoreContent(int device);		/* */
int SwMwCoreRegisterMoveSink(int device, void *sink);	/* */
void SwMwCoreUnregisterMoveSink(int device, void *sink);	/* */
int SwMwCoreNotifyLabel(int device, int label, int value);	/* */
int SwMwCoreNotifyDevice(int device, int event);		/* */

/* the device's move record list (the core's move manager) */
int SwMwMoveListGet(int manager, void **list, int *count);	/* */
int SwMwMoveListFree(int manager, void *list);			/* */
int SwMwMoveRecordBegin(int manager, int id, int mode);		/* */
int SwMwMoveListRelease(int manager, int id);			/* */

/* the support library's base object around the URI */
void *SwMwMoveSinkBaseConstruct(void *sink);
void *SwMwMoveSinkBaseDestruct(void *sink);
}

/* construct/destruct the move sink's embedded URI */
extern "C" void *SwMwMoveSinkBaseConstruct(void *sinkPtr)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)sinkPtr;

	SwMwUriInit(&sink->uri, "");
	return sink;
}

extern "C" void *SwMwMoveSinkBaseDestruct(void *sinkPtr)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)sinkPtr;

	SwMwUriDone(&sink->uri);
	return sink;
}

/* the move sink's lwmutex, "sce_sws_mutex" */
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

/* lock and unlock the move sink's mutex */
extern "C" int SwMwMoveSinkMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwMoveSinkMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* create the channel's move sink on its device */
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
	sink->dataLabel = SwMwCoreDataLabel(device);
	sink->manager = SwMwCoreMoveManager(device);
	sink->content = (struct SwMwContent *)SwMwCoreContent(device);
	sink->moveStatus = 0;
	sink->moveState = -1;
	sink->moveResult = 0;
	sink->closed = 0;
	sink->status = 0;
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

/* destroy the move sink */
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

/* give the move sink its URI and start the move */
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
		sink->closed = 0;
		sink->status = 0;
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

/* the 0x2804 command: the move result */
extern "C" int SwMwMoveSinkGetUriResult(int handle, unsigned char *out, int value)
{
	return SwMwMoveSinkClose(handle, out, value);
}

/* the close/result handshake shared with the 0x2804 command */
extern "C" int SwMwMoveSinkClose(int handle, unsigned char *out, int mode)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)handle;
	int value;
	int status;

	if (mode != 0) {
		if (sink->result != 0) {
			if (sink->content->operation(&sink->uri, &sink->result) != 0)
				swMwTrace("[SWMW] Assertion Failed in %s:%d\n",
					  SWMW_MOVESINK_SOURCE, 161);
			sink->result = 0;
		}
		sink->closed = 1;
		return 12015;
	}
	SwMwMoveSinkMutexLock(&sink->mutex);
	if (sink->closed == 0) {
		status = 12007;
	} else {
		value = sink->status;
		if (value > 204) {
			if (value > 1005) {
				if (value == 1026)
					sink->result = 0;
				status = 12204;
			} else if (value <= 206) {
				status = 12203;
			} else if (value > 1004) {
				status = sink->status;
			} else {
				status = 12204;
			}
		} else if (value > 0) {
			status = 12204;
		} else if (value >= 0) {
			*out = (unsigned char)sink->moveState;
			status = 0;
		} else {
			status = 12204;
		}
	}
	SwMwMoveSinkMutexUnlock(&sink->mutex);
	return status;
}

/* send the move's MV_FINALIZE and let the completion run */
extern "C" int SwMwMoveSinkFinalize(int handle, void *uriText, int size, int label)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)handle;
	struct SwMwMoveMessage unused;
	struct SwMwUri text;
	struct SwMwMoveMessage message;
	unsigned char address[16];
	unsigned char parsed[4];
	unsigned short length;
	struct SwMwMoveRecord *records;
	int count;
	int status;
	int failed;
	int index;

	SwMwMoveMessageInit(&unused);
	length = (unsigned short)size;
	if (sink == NULL || sink->content == NULL) {
		SwMwMoveMessageDone(&unused);
		return 12004;
	}
	status = SwMwNetResolveName((const char *)uriText, length, 2, &parsed);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 364);
		failed = 1;
	}
	if (failed == 0) {
		status = SwMwNetFormatAddress(2, &parsed, (char *)address, 32);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 370);
			failed = 1;
		}
		if (failed == 0) {
			SwMwUriInit(&text, (const char *)address);
			status = SwMwUriSet(&sink->uri, &text, length);
			SwMwUriDone(&text);
			failed = 0;
			if (status != 0) {
				swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 376);
				failed = 1;
			}
			if (failed != 0) {
				status = SwMwStatus(status);
				SwMwMoveMessageDone(&unused);
				return status;
			}
			failed = 0;
			if (SwMwUriIsSet(&sink->uri) == 0) {
				swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 379);
				failed = 1;
			}
			if (failed != 0) {
				status = SwMwStatus(1);
				SwMwMoveMessageDone(&unused);
				return status;
			}
			{
				void *recordsRaw = NULL;
				count = 0;
				status = SwMwMoveListGet(sink->manager, &recordsRaw, &count);
				records = (struct SwMwMoveRecord *)recordsRaw;
			}
			if (status != 0) {
				SwMwMoveMessageDone(&unused);
				return status;
			}
			if (count != 0) {
				for (index = 0; index < count; ++index) {
					if (records[index].requested == label)
						break;
				}
				if (index != count) {
					struct SwMwMoveRecord *entry = &records[index];

					sink->sessionState[5] = entry->id;
					sink->sessionState[6] = label;
					sink->sessionState[7] = 0;
					sink->moveState = entry->label;
					sink->finalized = 0;
					sink->sessionState[0] = 0;
					sink->sessionState[1] = 0;
					sink->sessionState[2] = 0;
					sink->sessionState[3] = 0;
					sink->sessionState[4] = 0;
					if (entry->kind == 1) {
						SwMwMoveMessageInit(&message);
						memset(&message, 0, sizeof(message));
						message.label = entry->label;
						memcpy(message.key, entry->key, sizeof(message.key));
						memcpy(message.first, entry->first, sizeof(message.first));
						memcpy(message.second, entry->second, sizeof(message.second));
						swMwTrace("sw mw:Send MV_FINALIZE: kxm_label=%d contentID=%d\n",
							  entry->label, label);
						status = sink->content->finalize(&sink->uri, &message);
						failed = 0;
						if (status != 0) {
							swMwTrace("[SWMW] ERROR in %s:%d\n",
								  SWMW_MOVESINK_SOURCE, 439);
							failed = 1;
						}
						if (failed != 0) {
							status = SwMwStatus(status);
							SwMwMoveMessageDone(&message);
							SwMwMoveListFree(sink->manager, records);
							SwMwMoveMessageDone(&unused);
							return status;
						}
						sink->finalized = 1;
						SwMwMoveMessageDone(&message);
					} else if (entry->kind == 2) {
						sink->finalized = 1;
						sink->sessionState[0] = 1;
						sink->sessionState[1] = 0;
					} else {
						SwMwMoveListFree(sink->manager, records);
						SwMwMoveMessageDone(&unused);
						return 12014;
					}
					SwMwMoveListFree(sink->manager, records);
					status = SwMwMoveSinkComplete((int)sink);
					SwMwMoveMessageDone(&unused);
					return status;
				}
				status = 12003;
			} else {
				status = 12003;
			}
			SwMwMoveListFree(sink->manager, records);
			SwMwMoveMessageDone(&unused);
			return status;
		}
	}
	SwMwMoveMessageDone(&unused);
	return 12202;
}

/* send the move's MV_COMPLETE and finish the session */
extern "C" int SwMwMoveSinkComplete(int handle)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)handle;
	int value;
	int status;

	if (sink == NULL || sink->content == NULL)
		return 12004;
	if (sink->finalized == 0)
		return 12004;
	SwMwMoveSinkMutexLock(&sink->mutex);
	if (sink->sessionState[0] == 0) {
		SwMwMoveSinkMutexUnlock(&sink->mutex);
		return 12007;
	}
	value = sink->sessionState[1];
	if (value != 0) {
		if (value == 1033) {
			sink->finalized = 0;
			sink->sessionState[0] = 0;
			sink->sessionState[1] = 0;
		} else if (value == 1019) {
			sink->content->abort(&sink->uri, (unsigned char)sink->moveState);
			SwMwCoreNotifyLabel(sink->device, sink->sessionState[6], 41);
			SwMwMoveListRelease(sink->manager, sink->sessionState[5]);
		}
		SwMwMoveSinkMutexUnlock(&sink->mutex);
		return value;
	}
	SwMwMoveSinkMutexUnlock(&sink->mutex);
	if (sink->sessionState[2] == 0) {
		status = SwMwMoveRecordBegin(sink->manager, sink->sessionState[5], 2);
		if (status != 0)
			return status;
		swMwTrace("sw mw:Send MV_COMPLETE: kxm_label=%d contentID=%d\n",
			  sink->moveState, sink->sessionState[6]);
		status = sink->content->complete(&sink->uri, (unsigned char)sink->moveState);
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESINK_SOURCE, 529);
			return SwMwStatus(status);
		}
		sink->sessionState[2] = 1;
	}
	SwMwMoveSinkMutexLock(&sink->mutex);
	if (sink->sessionState[3] == 0) {
		SwMwMoveSinkMutexUnlock(&sink->mutex);
		return 12007;
	}
	value = sink->sessionState[4];
	if (value != 0) {
		if (value == 1033) {
			sink->sessionState[2] = 0;
			sink->sessionState[3] = 0;
			sink->sessionState[4] = 0;
		}
		SwMwMoveSinkMutexUnlock(&sink->mutex);
		return value;
	}
	SwMwMoveSinkMutexUnlock(&sink->mutex);
	sink->content->abort(&sink->uri, (unsigned char)sink->moveState);
	sink->moveState = -1;
	status = SwMwMoveListRelease(sink->manager, sink->sessionState[5]);
	if (status != 0)
		return status;
	sink->sessionState[5] = -1;
	return 0;
}

/* store authentication result and kxm label under lock */
extern "C" int SwMwMoveSinkOnAuthResult(struct SwMwMoveSink *sink, int status,
					unsigned char kxmLabel)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 253);
	SwMwMoveSinkMutexLock(&sink->mutex);
	sink->moveState = kxmLabel;
	sink->closed = 1;
	sink->status = status;
	return SwMwMoveSinkMutexUnlock(&sink->mutex);
}

/* forward a device notification to the core manager */
extern "C" int SwMwMoveSinkNotifyDevice(struct SwMwMoveSink *sink, int event)
{
	return SwMwCoreNotifyDevice(sink->device, event);
}

/* record a move cancellation received from the source */
extern "C" int SwMwMoveSinkOnCancel(struct SwMwMoveSink *sink)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 278);
	SwMwMoveSinkMutexLock(&sink->mutex);
	sink->moveResult = 1;
	sink->sessionState[0] = 1;
	sink->sessionState[1] = 1522;
	return SwMwMoveSinkMutexUnlock(&sink->mutex);
}

/* record the MV_FINALIZE result and notify label 42 on success */
extern "C" int SwMwMoveSinkOnFinalizeResult(struct SwMwMoveSink *sink, int status)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 299);
	if (status == 0)
		SwMwCoreNotifyLabel(sink->device, sink->sessionState[6], 42);
	SwMwMoveSinkMutexLock(&sink->mutex);
	sink->sessionState[0] = 1;
	sink->sessionState[1] = status;
	return SwMwMoveSinkMutexUnlock(&sink->mutex);
}

/* record the MV_COMPLETE result under lock */
extern "C" int SwMwMoveSinkOnCompleteResult(struct SwMwMoveSink *sink, int status)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 320);
	SwMwMoveSinkMutexLock(&sink->mutex);
	sink->sessionState[3] = 1;
	sink->sessionState[4] = status;
	return SwMwMoveSinkMutexUnlock(&sink->mutex);
}

/* return the active authentication id */
extern "C" int SwMwMoveSinkGetAuthId(struct SwMwMoveSink *sink)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 573);
	return sink->moveStatus;
}

/* return the active kxm label */
extern "C" int SwMwMoveSinkGetKxmLabel(struct SwMwMoveSink *sink)
{
	if (sink == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 584);
	return sink->moveState;
}
