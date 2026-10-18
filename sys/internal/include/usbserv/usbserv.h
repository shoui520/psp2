#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

/* SceUsbServ's errors (vitasdk's names: no Sony header or record names them) */
#define SCE_USBSERV_ERROR_UNAVAILABLE		0x80248002
#define SCE_USBSERV_ERROR_INVALID_PARAM		0x80248003
#define SCE_USBSERV_ERROR_NOT_SUPPORTED		0x80248004
#define SCE_USBSERV_ERROR_FATAL			0x802480FF

int SceUsbServForDriver_6D738018(void);	/* vitasdk: ksceUsbServDisableEtherSuspend */
int SceUsbServForDriver_7AD36284(SceUInt port, SceBool mode);	/* vitasdk: ksceUsbServMacSelect */
int SceUsbServForDriver_853CB8E4(void);	/* vitasdk: ksceUsbServAccessoryDeactivate */
int SceUsbServForDriver_AA6D4409(void);	/* vitasdk: ksceUsbServAccessoryActivate */
SceBool SceUsbServForDriver_F0553A69(SceUInt port);	/* vitasdk: ksceUsbServMacGet */
int sceUsbServAccessoryActivate(void);
int sceUsbServAccessoryDeactivate(void);
int sceUsbServEtherDisable(void);
int sceUsbServEtherEnable(void);
