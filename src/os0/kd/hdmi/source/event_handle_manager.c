/*
 * SceHdmi event handle manager (Sony's event_handle_manager.c): passes the
 * device layer's events on to the callbacks registered through
 * SceHdmiForDriver. Device events 0 and 1 are the unplug and plug of the sink
 * ("%s Ch UnPluged", "%s Ch Pluged"); the EDID is dropped or read first.
 */
#include <psp2kern/types.h>
#include "fw.h"
#include "sce/debug.h"
#include "hdmi_internal.h"

#define HDMI_EVENT_COUNT	6

static HdmiEventCallback eventCallbacks[HDMI_EVENT_COUNT];

static void onDeviceEvent2(void) /* name unknown */
{
	if (eventCallbacks[2] != NULL)
		eventCallbacks[2]();
}

static void onDeviceEvent3(void) /* name unknown */
{
	if (eventCallbacks[3] != NULL)
		eventCallbacks[3]();
}

static void onDeviceEvent4(void) /* name unknown */
{
	if (eventCallbacks[4] != NULL)
		eventCallbacks[4]();
}

static void onDeviceEvent5(void) /* name unknown */
{
	if (eventCallbacks[5] != NULL)
		eventCallbacks[5]();
}

static void onPlug(void) /* name unknown */
{
	edidManagerRead();
	if (eventCallbacks[1] != NULL)
		eventCallbacks[1]();
}

static void onUnplug(void) /* name unknown */
{
	edidManagerClear();
	if (eventCallbacks[0] != NULL)
		eventCallbacks[0]();
}

int eventHandleManagerInitialize(void) /* name unknown */
{
	int ret;

	ret = sceHdmiDeviceRegisterEventCallback(0, onUnplug);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceRegisterEventCallback(1, onPlug);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceRegisterEventCallback(2, onDeviceEvent2);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceRegisterEventCallback(3, onDeviceEvent3);
	if (ret < 0)
		return ret;
	ret = sceHdmiDeviceRegisterEventCallback(4, onDeviceEvent4);
	if (ret < 0)
		return ret;
	return sceHdmiDeviceRegisterEventCallback(5, onDeviceEvent5);
}

int _sceHdmiRegisterEventCallback(SceUInt32 event, HdmiEventCallback callback)
{
	if (event >= HDMI_EVENT_COUNT) {
		SCE_DBG_ASSERT_FAIL(115);
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	if (callback == NULL) {
		SCE_DBG_ASSERT_FAIL(120);
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	eventCallbacks[event] = callback;
	return 0;
}

int _sceHdmiUnRegisterEventCallback(SceUInt32 event)
{
	if (event >= HDMI_EVENT_COUNT) {
		SCE_DBG_ASSERT_FAIL(132);
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	}
	/* Sony clears the first entry, whatever the event. */
	eventCallbacks[0] = NULL;
	return 0;
}
