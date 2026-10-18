/* Private USB serial layouts and recovered interface declarations. */
#pragma once
#include <stdint.h>
#include <udcd/udcd.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <usbstor/usbstor.h>
#include <acmgr/acmgr.h>

int sceUsbSerialStart(void);
int sceUsbSerialClose(void);
int sceUsbSerialSetup(unsigned mode);
int SceUsbSerial_2C08272C(void);
int sceUsbSerialRecv(void *buffer, unsigned size, unsigned mode, int timeout);
int sceUsbSerialSend(const void *buffer, unsigned size, unsigned mode, int timeout);
int SceUsbSerial_60D93778(unsigned mask);
int SceUsbSerial_0ACC8A92(unsigned mask);
int SceUsbSerial_405DB744(int timeout);
int sceUsbSerialGetRecvBufferSize(void);
int SceUsbSerial_923535FA(void);
int SceUsbSerial_A281DF87(void);
int SceUsbSerial_642F8F19(void);
int sceUsbSerialStatus(void);
