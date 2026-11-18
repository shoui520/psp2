/*
 * The application's callbacks ("CALL:"): one callback for a set of events, and one handler
 * the task queue and the device report their state changes to.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "mtp_internal.h"

#define MTP_TAG_CALL		"CALL:"
#define MTP_CALLBACK_EVENTS	0x33F
#define MTP_CALLBACK_MAX	1

typedef struct MtpCallback {
	SceUInt32 events;
	MtpCallbackFunc func;
	void *arg;
} MtpCallback;

static MtpCallback s_callbacks[MTP_CALLBACK_MAX] = { { 0, NULL, NULL } };
static MtpEventHandler s_eventHandler = NULL;
static void *s_eventHandlerArg = NULL;

int mtpCallbackRegister(SceUInt32 events, MtpCallbackFunc func, void *arg)
{
	int i;

	if ((events & ~MTP_CALLBACK_EVENTS) != 0) {
		MTP_ERR(MTP_TAG_CALL, 45, "Unsupported event: 0x%x\n", events);
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	for (i = 0; i < MTP_CALLBACK_MAX; i++) {
		if (s_callbacks[i].func == NULL) {
			s_callbacks[i].events = events;
			s_callbacks[i].func = func;
			s_callbacks[i].arg = arg;
			return 0;
		}
	}
	return SCE_MTP_ERROR_DEVICE;
}

int mtpCallbackUnregister(SceUInt32 events, MtpCallbackFunc func)
{
	int i;

	if ((events & ~MTP_CALLBACK_EVENTS) != 0) {
		MTP_ERR(MTP_TAG_CALL, 65, "Unsupported event: 0x%x\n", events);
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	for (i = 0; i < MTP_CALLBACK_MAX; i++) {
		if (s_callbacks[i].func == func) {
			s_callbacks[i].events = 0;
			s_callbacks[i].func = NULL;
			s_callbacks[i].arg = NULL;
			return 0;
		}
	}
	return SCE_MTP_ERROR_NOT_FOUND;
}

int mtpCallbackNotify(int event)
{
	int i;

	if ((event & ~MTP_CALLBACK_EVENTS) != 0) {
		MTP_ERR(MTP_TAG_CALL, 84, "Unsupported event: 0x%x\n", event);
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	for (i = 0; i < MTP_CALLBACK_MAX; i++) {
		if ((s_callbacks[i].events & event) != 0 && s_callbacks[i].func != NULL)
			s_callbacks[i].func(event, s_callbacks[i].arg);
	}
	return 0;
}

void mtpCallbackSetEventHandler(MtpEventHandler handler, void *arg)
{
	s_eventHandler = handler;
	s_eventHandlerArg = arg;
}

void mtpCallbackClearEventHandler(void)
{
	s_eventHandler = NULL;
	s_eventHandlerArg = NULL;
}

int mtpCallbackInvokeEvent(int event)
{
	if (s_eventHandler != NULL)
		return s_eventHandler(event, s_eventHandlerArg);
	return event;
}
