/*
 * Non-ThreadX Mask ROM entry points used by the firmware (addresses in
 * boot/rom_syms.ld). AEABI helpers keep their ABI names; mdev entry names
 * describe their use and are provisional until a source names them.
 */
#ifndef ROBIN_ROM_H
#define ROBIN_ROM_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"

void __aeabi_memclr4(void *dest, size_t n);
void __aeabi_memcpy(void *dest, const void *source, size_t size);
/* Callable 64-bit clock result; its absent ROM implementation is opaque. */
uint64_t romGetTimerTimestamp(void);

/* Signals an interrupt to an open mdev device (called from the SDIO line). */
int mdevInterrupt(void *dev, int arg);
int osa_BufferFree(OsaBuffer *buffer);
/* The RAM callbacks forward the complete event register contract to ROM. */
void mdevForwardEvent(void *device, OsaBuffer *buffer, uintptr_t argument,
		      unsigned int event);
void *mdevOpenDevice(const char *name, unsigned int flags);
int mdevIoctl(void *device, unsigned int operation, uintptr_t argument);
void mdevReleaseDevice(void *device);

#endif /* ROBIN_ROM_H */
