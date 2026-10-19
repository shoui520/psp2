/*
 * SceDbgUsb (os0:kd/dbgusb): SceDbgUsbForDriver, the debugger's USB link
 * that DECI4P's DfMgr sends and receives through.
 */
#pragma once

#include <scetypes.h>

int sceDbgUsbOpen(SceUID event);
int SceDbgUsbForDriver_08CF16D1(void);
int _sceDbgUsbPullData(void *data, unsigned int size);
int sceDbgUsbWrite(const void *data, unsigned int size);
