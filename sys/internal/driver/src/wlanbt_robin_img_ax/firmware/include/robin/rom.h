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
void __aeabi_memclr(void *dest, size_t n);
void __aeabi_memcpy(void *dest, const void *source, size_t size);
void __aeabi_memcpy4(void *dest, const void *source, size_t size);
void __aeabi_memset4(void *dest, size_t n, int value);
uint32_t __aeabi_uread4(const void *address);
size_t strlen(const char *string);
/* Mask ROM string helpers (0x03f066ed, 0x03f0672b); semantics inferred from callers. */
unsigned int romStringCopy(void *destination, const void *source, unsigned int limit);
/* EDCA parameters of a channel-access queue (contention windows as masks, AIFSN, TXOP). */
int romEdcaSetCwMin(unsigned int queue, unsigned int value);	/* 0x03f3a62f */
int romEdcaSetCwMax(unsigned int queue, unsigned int value);	/* 0x03f3a643 */
int romEdcaSetAifsn(unsigned int queue, unsigned int value);	/* 0x03f3a665 */
int romEdcaSetTxop(unsigned int queue, unsigned int value);	/* 0x03f3a64d */
unsigned int romRateIndex(unsigned int rate);	/* 0x03f3ab4f */
unsigned int romStringLength(const void *string, unsigned int limit);
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

/* sqram/rom_ram.c */
int romRamInit(void);

#endif /* ROBIN_ROM_H */
