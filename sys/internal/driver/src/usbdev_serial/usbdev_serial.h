/* Undocumented SceUsbDevSerial syscall interface (original names unknown). */
#pragma once
#include <stdint.h>

int SceUsbDevSerial_DAE5C362(unsigned mode);
int SceUsbDevSerial_9F497D4E(void);
int SceUsbDevSerial_C0FDECAF(void);
int SceUsbDevSerial_6F2B3960(void);
int SceUsbDevSerial_F0C3F3D4(void);
int SceUsbDevSerial_57FBB08C(void *buffer, unsigned size);
int SceUsbDevSerial_4437FF2D(const void *buffer, unsigned size);
int SceUsbDevSerial_918D359E(unsigned command, void *buffer, unsigned size);
