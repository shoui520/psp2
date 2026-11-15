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
unsigned int romLibAcquireState(unsigned int a, unsigned int b, unsigned int index);	/* 0x03f3a60f: the rate of a rate index (inferred from wlanTxCompletionFetch) */
void romLibDispatchState(void);			/* 0x03f3a3b5: first of the three calls that set the MAC's channel timing up */
void romLibParseState(const void *settings);	/* 0x03f3a3ed: takes two or four bytes (a register and its value) */
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
struct WlanRateControl;
unsigned int romLibNotifyEntry(unsigned int rate, struct WlanRateControl *rc, unsigned int limit); /* 0x03f3adfa */
/*
 * The ROM's rate tables: BuildEntry orders the rate indexes (the key wlanPsInspectBuffer() sorts by),
 * ComputeEntry gives the rate in Mbit/s, and the next six give the thresholds the rate selection
 * compares the transmissions of a window with: the fewest it needs, and the success percentages that
 * decide to move up or down (fmCoexReleaseTable()). All take a rate index, 255 and 0 where a limit is
 * asked for.
 */
unsigned int romLibBuildEntry(unsigned int rate);				/* 0x03f3ad62 */
unsigned int romLibComputeEntry(unsigned int rate);				/* 0x03f3ad9a */
unsigned int romLibApplyEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3add0 */
unsigned int romLibQueryEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3ada8 */
unsigned int romLibResolveEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3adde */
unsigned int romLibResetEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3adb4 */
unsigned int romLibScheduleEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3adec */
unsigned int romLibSyncEntry(unsigned int rate, unsigned int limit, unsigned int flag);	/* 0x03f3adc2 */
unsigned int romRateBitrate(unsigned int index);	/* 0x03f3a2ab: compared to the link rate threshold */
unsigned int romStringLength(const void *string, unsigned int limit);
/* Callable 64-bit clock result; its absent ROM implementation is opaque. */
uint64_t romGetTimerTimestamp(void);
uint32_t romGetMicroseconds(void);		/* 0x03f3a7cb: the MAC's clock, low word, microseconds */
uint64_t romLibRecordState(uint32_t reference);	/* 0x03f3a76f: the current 64-bit time */
void romLibProcessContext(void *timing);	/* 0x03f3a7e3 */
void romLibNotifyState(void);			/* 0x03f3a551 */
uint64_t romMul64x32(uint64_t value, uint32_t factor);	/* 0x03f3f3ac (ARM) */
uint64_t romMul32x32(uint32_t a, uint32_t b);		/* 0x03f3f3b4 (ARM) */
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
/*
 * The rate and timing routines of the ROM the transmit path calls (names
 * inferred from the callers: wlan/macrate.c); the arguments are what the
 * callers pass.
 */
unsigned int romLibManageContext(unsigned int rate, uint32_t rateSet);	/* 0x03f3aa0b */
unsigned int romLibUpdateState(unsigned int rate);			/* 0x03f3a2b9 */
int romLibCheckState(int a, int b, int c, int d, int e);		/* 0x03f3a347 */
void romLibPrepareState(uint32_t low, uint32_t high, uint32_t limit);	/* 0x03f3a36f */
void romLibSelectContext(unsigned int mode);				/* 0x03f3ab21 */
void romLibConvertContext(unsigned int rate, unsigned int response);	/* 0x03f3aaf1 */
unsigned int romLibHandleState(unsigned int index);			/* 0x03f3a2a5: the rate code of an index */
void romLibClearContext(unsigned int rate, unsigned int value);		/* 0x03f3aadb */
/*
 * The MAC's address and timer registers (names inferred from the callers in wlan/mgmtutil.c):
 * romLibResetState() writes the 48-bit address a station uses (high halfword first), romLibQueryState()
 * takes the address in an eight-byte buffer, romLibVerifyState() reads the 64-bit TSF.
 */
void romLibVerifyState(uint32_t *first, uint32_t *second);			/* 0x03f3a6b9: the 64-bit timer, as two words (inferred) */
void romLibQueryState(const void *address);					/* 0x03f3a4c9 */
void romLibResetState(uint32_t high, uint32_t low);				/* 0x03f3a4db */
void romLibBuildState(void);							/* 0x03f3a417 */
void romLibComputeContext(uint32_t mask);					/* 0x03f3a8ff */
void romLibParseContext(uint32_t mask);						/* 0x03f3a8d7 */
void romLibBuildContext(uint32_t mask);						/* 0x03f3a8e1 */
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
