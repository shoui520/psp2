/* SceCompat shared SRAM: host, guest boot, and IPL storage phases. */
#pragma once

#define sceIoIoctl	sceIoIoctl_vitasdk	/* vitasdk's user prototype; the kernel's is iofilemgr's */
#include <psp2/compat.h>
#undef sceIoIoctl
#include <stddef.h>
#include <stdint.h>

typedef struct SceCompatSharedRequest {
	uint32_t queueSelector;
	uint32_t encodedRequest;
} SceCompatSharedRequest;

typedef struct SceCompatSharedReturn {
	uint64_t value;
	uint64_t firstValue;
	uint64_t secondValue;
	unsigned char guestStorage[8];
} SceCompatSharedReturn;

typedef struct SceCompatSharedInterrupt {
	uint32_t pending;
	uint32_t guestStorage;
} SceCompatSharedInterrupt;

/*
 * This is one complete mapped storage object. Its boot phase contains executable
 * bytes supplied by the secure module; declarations here do not supply an image.
 * Guest-owned spans have no interpreted host fields. They are not padding or
 * assertions that the guest leaves the storage unused. Boot and IPL operations
 * also read, overwrite, and clear these spans as whole objects.
 */
typedef union SceCompatSharedSram {
	struct {
		unsigned char program[0xfbc];
		uint32_t bootCookie;
		unsigned char challenge[64];
	} boot;
	unsigned char iplCipherPage[4096];
	struct {
		unsigned char guestStorageBeforeExchange[0x100];
		uint32_t sharedValue;
		uint32_t hostToGuestValues[2];
		uint32_t guestToHostValues[3];
		unsigned char guestStorageBeforeController[8];
		SceCtrlDataPsp controller;
		unsigned char guestStorageAfterController[16];
		uint32_t publishedTimestamp;
		unsigned char guestStorageAfterTimestamp[44];
		uint32_t sharedSecondaryValue;
		uint32_t secondaryHostToGuestValues[2];
		uint32_t sharedTertiaryValue;
		uint32_t tertiaryHostToGuestValues[2];
		uint32_t guestToHostValue;
		unsigned char guestStorageBeforeResumeCookie[112];
		uint32_t resumeCookie;
		uint32_t scrambleSeed[8];
		unsigned char scrambleKey[16];
		unsigned char guestStorageAfterScrambleKey[80];
		unsigned char decryptWork[0xa4];
		unsigned char guestStorageAfterDecryptWork[28];
		unsigned char decryptHash[0xb4];
		unsigned char guestStorageAfterDecryptHash[0x40c];
		SceCompatSharedRequest requests[3];
		unsigned char guestStorageAfterRequests[40];
		SceCompatSharedReturn returns[3];
		unsigned char guestStorageAfterReturns[32];
		SceCompatSharedInterrupt interrupts[16];
		unsigned char guestStorageAfterInterrupts[0x6c0];
	} runtime;
} SceCompatSharedSram;

_Static_assert(sizeof(SceCompatSharedRequest) == 8, "compat shared request");
_Static_assert(offsetof(SceCompatSharedRequest, encodedRequest) == 4,
	"compat encoded request");
_Static_assert(sizeof(SceCompatSharedReturn) == 32, "compat shared return");
_Static_assert(offsetof(SceCompatSharedReturn, firstValue) == 8,
	"compat first return value");
_Static_assert(offsetof(SceCompatSharedReturn, secondValue) == 16,
	"compat second return value");
_Static_assert(offsetof(SceCompatSharedReturn, guestStorage) == 24,
	"compat return guest storage");
_Static_assert(sizeof(SceCompatSharedInterrupt) == 8, "compat shared interrupt");
_Static_assert(offsetof(SceCompatSharedInterrupt, guestStorage) == 4,
	"compat interrupt guest storage");
_Static_assert(sizeof(SceCompatSharedSram) == 4096, "compat shared SRAM");
_Static_assert(sizeof(((SceCompatSharedSram *)0)->boot) == 4096,
	"compat shared boot phase");
_Static_assert(sizeof(((SceCompatSharedSram *)0)->runtime) == 4096,
	"compat shared runtime and IPL storage phase");
_Static_assert(offsetof(SceCompatSharedSram, boot.bootCookie) == 0xfbc,
	"compat boot cookie");
_Static_assert(offsetof(SceCompatSharedSram, boot.challenge) == 0xfc0,
	"compat boot challenge");
_Static_assert(offsetof(SceCompatSharedSram, runtime.sharedValue) == 0x100,
	"compat shared value");
_Static_assert(offsetof(SceCompatSharedSram, runtime.hostToGuestValues) == 0x104,
	"compat host exchange values");
_Static_assert(offsetof(SceCompatSharedSram, runtime.guestToHostValues) == 0x10c,
	"compat guest exchange values");
_Static_assert(offsetof(SceCompatSharedSram, runtime.controller) == 0x120,
	"compat shared controller");
_Static_assert(offsetof(SceCompatSharedSram, runtime.publishedTimestamp) == 0x140,
	"compat published controller timestamp");
_Static_assert(offsetof(SceCompatSharedSram, runtime.sharedSecondaryValue) == 0x170,
	"compat shared secondary value");
_Static_assert(offsetof(SceCompatSharedSram, runtime.secondaryHostToGuestValues) == 0x174,
	"compat secondary host values");
_Static_assert(offsetof(SceCompatSharedSram, runtime.sharedTertiaryValue) == 0x17c,
	"compat shared tertiary value");
_Static_assert(offsetof(SceCompatSharedSram, runtime.tertiaryHostToGuestValues) == 0x180,
	"compat tertiary host values");
_Static_assert(offsetof(SceCompatSharedSram, runtime.guestToHostValue) == 0x188,
	"compat guest published value");
_Static_assert(offsetof(SceCompatSharedSram, runtime.resumeCookie) == 0x1fc,
	"compat retained resume cookie");
_Static_assert(offsetof(SceCompatSharedSram, runtime.scrambleSeed) == 0x200,
	"compat IPL scramble seed");
_Static_assert(offsetof(SceCompatSharedSram, runtime.scrambleKey) == 0x220,
	"compat IPL scramble key");
_Static_assert(offsetof(SceCompatSharedSram, runtime.decryptWork) == 0x280,
	"compat IPL decrypt workspace");
_Static_assert(offsetof(SceCompatSharedSram, runtime.decryptHash) == 0x340,
	"compat IPL decrypt hash workspace");
_Static_assert(offsetof(SceCompatSharedSram, runtime.requests) == 0x800,
	"compat shared request channels");
_Static_assert(offsetof(SceCompatSharedSram, runtime.returns) == 0x840,
	"compat shared return channels");
_Static_assert(offsetof(SceCompatSharedSram, runtime.interrupts) == 0x8c0,
	"compat shared interrupt channels");

int SceCompatForDriver_6EBD9E01(SceUInt32 mode);
