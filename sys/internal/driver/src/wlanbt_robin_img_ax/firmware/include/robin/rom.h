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
void __aeabi_memmove(void *dest, const void *source, size_t size);
void __aeabi_memset4(void *dest, size_t n, int value);
uint32_t __aeabi_uread4(const void *address);
size_t strlen(const char *string);
/* Mask ROM string helpers (0x03f066ed, 0x03f0672b); semantics inferred from callers. */
unsigned int romStringCopy(void *destination, const void *source, unsigned int limit);
/* EDCA parameters of a channel-access queue (contention windows as masks, AIFSN, TXOP). */
/*
 * MAC timing entry points around a channel change and a calibration
 * (wlan/mgmt.c); their ROM bodies are not available, so the names are
 * provisional.
 */
void romLibResolveState(void);			/* 0x03f3a521: before the radio is retuned */
int romLibScheduleState(void);			/* 0x03f3a52d: after the host changes the connection's keys or rates */
int romLibApplyState(void);			/* 0x03f3a515: after it */
void romLibScheduleContext(void);		/* 0x03f3a96b */
void romLibAcquireContext(void);		/* 0x03f3aa10: every time the MAC's events are read */
int romLibCheckContext(uintptr_t argument);	/* 0x03f3a878 */
int romLibDispatchContext(uintptr_t argument);	/* 0x03f3a8ba */
void romLibResolveContext(unsigned int slotTime);	/* 0x03f3a963: microseconds */
void romLibFormatContext(void);			/* 0x03f3a9c5 */
void romLibInspectContext(int value);		/* 0x03f3a9d5 */
void romLibTrackContext(void);			/* 0x03f3a9e5 */
void romLibUpdateContext(void *timing);		/* 0x03f3a831: fills a station TSF record */
unsigned int romRateIndex(unsigned int rate);	/* 0x03f3ab4f */
unsigned int romRateValue(unsigned int index);	/* 0x03f3abc5: compared to order rates */
unsigned int romRateBitrate(unsigned int index);	/* 0x03f3a2ab: compared to the link rate threshold */
unsigned int romStringLength(const void *string, unsigned int limit);
/* Callable 64-bit clock result; its absent ROM implementation is opaque. */
uint64_t romGetTimerTimestamp(void);
uint32_t romGetMicroseconds(void);		/* 0x03f3a7cb: the MAC's clock, low word, microseconds */
uint64_t romLibRecordState(uint32_t reference);	/* 0x03f3a76f: the current 64-bit time */
void romLibProcessContext(void *timing);	/* 0x03f3a7e3 */
void romLibNotifyState(void);			/* 0x03f3a551 */
uint64_t romMul64x32(uint64_t value, uint32_t factor);	/* 0x03f3f3ac (ARM) */
int romEdcaSetCwMin(unsigned int queue, unsigned int value);	/* 0x03f3a62f */
int romEdcaSetCwMax(unsigned int queue, unsigned int value);	/* 0x03f3a643 */
int romEdcaSetAifsn(unsigned int queue, unsigned int value);	/* 0x03f3a665 */
int romEdcaSetTxop(unsigned int queue, unsigned int value);	/* 0x03f3a64d */
/* Brackets an EDCA queue's parameter update (0x03f3acf3 before, 0x03f3ad03 after). */
void romLibDispatchEntry(unsigned int queue);
int romLibParseEntry(unsigned int queue);
/* The airtime in microseconds of a frame at a rate (0x03f3a2d1); arguments inferred from wlanTxAirtime(). */
unsigned int romLibConfigureState(unsigned int rate, unsigned int sequence, unsigned int length,
				  unsigned int wmm);
/* The MAC's wake and sleep requests (0x03f3a917, 0x03f3a907); no argument. */
void romMacWakeRequest(void);
void romMacSleepNotify(void);

/* Signals an interrupt to an open mdev device (called from the SDIO line). */
int mdevInterrupt(void *dev, int arg);
int osa_BufferFree(OsaBuffer *buffer);
/* The RAM callbacks forward the complete event register contract to ROM. */
void mdevForwardEvent(void *device, OsaBuffer *buffer, uintptr_t argument,
		      unsigned int event);
void *mdevOpenDevice(const char *name, unsigned int flags);
int mdevIoctl(void *device, unsigned int operation, uintptr_t argument);
#define SDIO_IOCTL_HOST_PENDING		0x46
void mdevReleaseDevice(void *device);

/* sqram/rom_ram.c */
int romRamInit(void);

#endif /* ROBIN_ROM_H */
