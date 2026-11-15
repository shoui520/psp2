/*
 * The release of the Bluetooth link controller's targets (0x433f0 - 0x436c8): the Bluetooth device and the
 * AMP are handed back to the host transport in steps (events 1 to 13 of btLmpLcReleaseContext), the
 * target's state in struct BtLcTarget says where it is. The file name and roles are inferred; the
 * function names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btcontext.h"
#include "robin/btrom.h"
#include "robin/eventpower.h"
#include "robin/eventroute.h"
#include "robin/hct.h"
#include "robin/power.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/amp.h"

extern void abort(void);

/* The route's device index of the target: 3 for the AMP (link 1), else 0. */
#define TARGET_DEVICE(link)	((link) == 1 ? 3 : 0)
/* The SDIO device that belongs to the target. */
#define TARGET_HANDLE(link)	((link) == 1 ? ampSdioDev : btSdioActiveHandle)

/* Whether the release is due: 1 when the target waits in the released state, else 0. */
int btLmpLcManageContext(int link)
{
	struct BtLcTarget *target = &btLmpLcTarget[link];
	unsigned int phase = target->phase;

	if (target->enabled != 0 &&
	    (target->timeout == 0 || 1000 * (unsigned int)target->timeout <= sysEventClockElapsedUs(target->since))) {
		if (phase != 0) {
			if (phase == 3)
				return 1;
		} else {
			btLmpLcFormatContext(link, 0, 4, 0);
		}
	}
	return 0;
}

void btLmpLcAcquireContext(int link)
{
	struct BtLcTarget *target;

	if (link == 1)
		sysEnterTertiaryEvent();
	else
		sysPrimaryRouteResume();
	target = &btLmpLcTarget[link];
	if (target->phase != 0) {
		if (target->phase == 3 && target->flag == 0)
			btLmpLcInspectContext(link, 0);
	} else if (target->idle != 0) {
		btLmpLcFormatContext(link, 0, 4, 0);
	}
}

/* One step of the release; event is the step (0 to 13), argument is the timeout of event 1. */
void btLmpLcReleaseContext(int link, int event, int argument)
{
	struct BtLcTarget *target = &btLmpLcTarget[link];
	struct {
		uint32_t reserved;
		uint16_t reserved4;
		uint16_t state;
	} info;
	unsigned int notify;

	switch (event) {
	case 0:
	case 10:
	case 12:
		return;
	case 1:
		target->enabled = 1;
		target->timeout = argument;
		return;
	case 2:
		target->enabled = 0;
		return;
	case 3:
		if (target->pending != 0) {
			hctSdioIdle();
			target->idle = 1;
		}
		return;
	case 4:
		target->phase = 1;
		if (target->idle == 0) {
			btLmpLcFormatContext(link, 1, 32, 1);
			return;
		}
		btLmpLcFormatContext(link, 1, 90, 0);
		target->flag = 1;
		target->idle = 0;
		mdevIoctl(btSdioActiveHandle, 0x1d, (uintptr_t)&info);
		if (info.state != 1)
			sysEventRouteSetMasked(sysEventRouteDevice(TARGET_DEVICE(link)), 1);
		return;
	case 5:
		target->phase = 2;
		return;
	case 6:
		btLmpLcAdjustContext(link);
		return;
	case 7:
		sysEventRouteSetMasked(sysEventRouteDevice(TARGET_DEVICE(link)), 0);
		notify = target->flag != 0;
		btLmpLcInspectContext(link, notify);
		target->since = sysEventClockNow();
		return;
	case 8:
		if (target->flag == 0 || target->mode == 0xff) {
			mdevIoctl(TARGET_HANDLE(link), 0x19, 0);
			btLmpLcInspectContext(link, 1);
		}
		return;
	case 9:
		mdevIoctl(TARGET_HANDLE(link), 9, 0);
		if (target->mode != 0xff)
			mdevIoctl(btSdioActiveHandle, 0x61, 0);
		btLmpLcInspectContext(link, 1);
		target->since = sysEventClockNow();
		return;
	case 11:
		if (target->flag != 0)
			btLmpLcTrackContext(link);
		return;
	case 13:
		abort();
	default:
		return;
	}
}

/* An event of the host transport that asks the controller to leave the idle state of the device. */
int btLmpLcSetupContext(int link, const uint8_t *event)
{
	unsigned int type = event[2];

	if ((event[0] == 255 && type == 90) || (type == 32 && event[3] == 1))
		return btLmpLcFormatContext(link, 0, 5, 0);
	return link;
}

int btLmpLcClearContext(int link, const uint8_t *event)
{
	return event[0] == 255 && event[2] == 90;
}

int btLmpLcConvertContext(int link, const uint8_t *event)
{
	return event[2] == 32 && event[3] == 2;
}

int btLmpLcVerifyContext(int link)
{
	return btLmpLcTarget[link].flag != 0;
}

/*
 * Binds the device event of the target's route to the channel for the kind of release the request
 * (a2 is 2) names, and arms the target with the request's mode and delay. 1 when it was.
 */
int btLmpLcSelectContext(int link, int kind, const uint8_t *request)
{
	unsigned int channel;
	unsigned int device;
	const int *sdio;

	if (kind != 2)
		return 0;
	if (request[0] == 255) {
		sdio = link == 1 ? ampSdioDev : NULL;
		if (sdio == NULL) {
			sdio = btSdioActiveHandle;
			if (sdio == NULL || sdio != btSdioDev)
				return 0;
		}
		switch (*sdio) {
		case 0:
			channel = 4;
			break;
		case 1:
			channel = 5;
			break;
		case 2:
			channel = 6;
			break;
		case 3:
			channel = 7;
			break;
		default:
			return 0;
		}
	} else if (request[0] == 16) {
		channel = 9;
	} else {
		channel = 8;
	}
	device = sysEventRouteDevice(TARGET_DEVICE(link));
	if (sysEventRouteBind(device, channel, request[0]) != 0) {
		struct BtLcTarget *target = &btLmpLcTarget[link];

		target->pending = 1;
		target->mode = request[0];
		target->delay = request[1];
		mdevIoctl(btSdioActiveHandle, 0x60, request[0]);
		sysEventRouteSetMasked(device, 0);
		return 1;
	}
	return 0;
}

/* Moves the target on to the released state (3) once it asked for it (2). */
int btLmpLcAdjustContext(int link)
{
	struct BtLcTarget *target = &btLmpLcTarget[link];

	if (target->phase == 2) {
		target->phase = 3;
		if (target->flag != 0) {
			if (target->mode == 0xff)
				target->since = sysEventClockNow();
			else
				mdevIoctl(btSdioActiveHandle, 0x61, 0);
			if (link == 0)
				return mdevIoctl(btSdioActiveHandle, 0x1e, 0);
		}
	} else {
		btLmpLcInspectContext(link, 0);
		target->since = sysEventClockNow();
	}
	return 0;
}
