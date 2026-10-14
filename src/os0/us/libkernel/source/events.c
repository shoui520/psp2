/* Event APIs and their compact syscall parameter records. */
#include <sce/libkernel_backends.h>
#include <psp2/types.h>

typedef struct SceKernelWaitEvent {
	SceUID eventId;
	SceUInt32 eventPattern;
} SceKernelWaitEvent;

typedef struct SceKernelResultEvent {
	SceUID eventId;
	SceInt32 result;
	SceUInt32 resultPattern;
	SceInt32 reserved;
	SceUInt64 userData;
} SceKernelResultEvent;

typedef struct SceKernelEventInfo {
	SceSize size;
	SceUID eventId;
	char name[32];
	SceUInt32 attributes;
	SceUInt32 eventPattern;
	SceUInt64 userData;
	SceUInt32 numWaitThreads;
	SceInt32 reserved;
} SceKernelEventInfo;

struct EventWaitOptions {
	SceUInt64 *userData;
	SceUInt32 *timeout;
};

struct MultipleEventWaitOptions {
	SceKernelResultEvent *results;
	SceUInt32 *timeout;
};

struct EventNotifyOptions {
	SceUInt64 userData;
	SceInt32 notifyArgument;
};

struct EventCancelOptions {
	SceUInt64 userData;
	SceUInt32 *waiting;
};

struct EventInfoRequest {
	SceSize size;
};


int sceKernelWaitEvent(SceUID id, SceUInt32 pattern, SceUInt32 *result,
		      SceUInt64 *userData, SceUInt32 *timeout)
{
	struct EventWaitOptions options = { userData, timeout };

	return _sceKernelWaitEvent(id, pattern, result, &options);
}

int sceKernelWaitEventCB(SceUID id, SceUInt32 pattern, SceUInt32 *result,
			SceUInt64 *userData, SceUInt32 *timeout)
{
	struct EventWaitOptions options = { userData, timeout };

	return _sceKernelWaitEventCB(id, pattern, result, &options);
}

int sceKernelPollEvent(SceUID id, SceUInt32 pattern, SceUInt32 *result,
		      SceUInt64 *userData)
{
	return _sceKernelPollEvent(id, pattern, result, userData);
}

int sceKernelCancelEvent(SceUID id, SceUInt32 *waiting)
{
	return _sceKernelCancelEvent(id, waiting);
}

int sceKernelWaitMultipleEvents(SceKernelWaitEvent *events, SceUInt32 count,
		SceUInt32 mode, SceKernelResultEvent *results, SceUInt32 *timeout)
{
	struct MultipleEventWaitOptions options = { results, timeout };

	return _sceKernelWaitMultipleEvents(events, count, mode, &options);
}

int sceKernelWaitMultipleEventsCB(SceKernelWaitEvent *events, SceUInt32 count,
		SceUInt32 mode, SceKernelResultEvent *results, SceUInt32 *timeout)
{
	struct MultipleEventWaitOptions options = { results, timeout };

	return _sceKernelWaitMultipleEventsCB(events, count, mode, &options);
}

int sceKernelSetEventWithNotifyCallback(SceUID id, SceUInt32 pattern,
				      SceUInt64 userData, SceInt32 argument)
{
	struct EventNotifyOptions options = { userData, argument };

	return _sceKernelSetEventWithNotifyCallback(id, pattern, &options);
}

int sceKernelPulseEventWithNotifyCallback(SceUID id, SceUInt32 pattern,
					SceUInt64 userData, SceInt32 argument)
{
	struct EventNotifyOptions options = { userData, argument };

	return _sceKernelPulseEventWithNotifyCallback(id, pattern, &options);
}

int sceKernelCancelEventWithSetPattern(SceUID id, SceUInt32 pattern,
				     SceUInt64 userData, SceUInt32 *waiting)
{
	struct EventCancelOptions options = { userData, waiting };

	return _sceKernelCancelEventWithSetPattern(id, pattern, &options);
}

int sceKernelGetEventPattern(SceUID id, SceUInt32 *pattern)
{
	return _sceKernelGetEventPattern(id, pattern);
}

int sceKernelGetEventInfo(SceUID id, SceKernelEventInfo *info)
{
	struct EventInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetEventInfo(id, info, &request);
}
