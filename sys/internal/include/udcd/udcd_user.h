/*
 * SceUdcd's user-mode interface: the device state its syscalls report and the user-side
 * names of the syscalls (sceUdcdGetDeviceStateForUser is sceUdcdGetDeviceState to user
 * mode; the other syscalls keep the kernel's names and types,
 * udcd/udcd.h). The type's name is ours.
 */
#ifndef _UDCD_UDCD_USER_H
#define _UDCD_UDCD_USER_H

#ifndef SCE_UDCD_KERNEL
#define SCE_UDCD_USER
#endif
#include <udcd/udcd.h>

/* the device state as user mode sees it */
typedef struct UdcdUserDeviceState {
	unsigned int controller;		/* CONTROLLER_STOPPED/STARTED */
	unsigned int activation;		/* DEACTIVATED, ACTIVATED, IS_CHARGING */
	unsigned int cable;			/* CABLE_DISCONNECTED/CONNECTED */
	unsigned int connection;		/* CONNECTION_*, 1 while suspended */
	unsigned int charging;			/* USE_USB_CHARGING */
	unsigned int reserved;			/* not set */
} UdcdUserDeviceState;

#ifndef SCE_UDCD_KERNEL
#ifdef __cplusplus
extern "C" {
#endif

int sceUdcdGetDeviceState(UdcdUserDeviceState *state);

#ifdef __cplusplus
}
#endif
#endif

#endif
