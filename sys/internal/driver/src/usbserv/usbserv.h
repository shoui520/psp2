#pragma once
#include <stdint.h>
#include <psp2kern/usbserv.h>
#include <psp2kern/usbd.h>
#include <psp2kern/udcd.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/debug.h>
#include <lowio/pervasive.h>
#include <lowio/pwm.h>
#include <psp2kern/lowio/gpio.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysevent.h>
#include <sysmem/procevent.h>
#include <sysmem/sysroot.h>
#include <modulemgr/modulemgr.h>

/* Names unknown: hardware operations identified by their import NIDs. */
int SceUdcdForDriver_1A5A3851(void);
int SceUdcdForDriver_2A234CAF(int port);
int SceUdcdForDriver_9E7F7EFE(void);
int SceUdcdForDriver_D01CFD22(void);
int SceUdcdForDriver_E9E75C49(int port);
int sceUsbServAccessoryActivate(void);
int sceUsbServAccessoryDeactivate(void);
