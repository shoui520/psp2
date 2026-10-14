#ifndef UPDATE_MGR_SLSK_H
#define UPDATE_MGR_SLSK_H

#include <stdint.h>
#include <stddef.h>

typedef struct SlskHeader {
	uint32_t magic;
	uint32_t headerSize;
	uint32_t versionStringSize;
	uint32_t dynamicRevokeBlockSize;
	uint32_t bodySize;
	uint16_t encryptionKeyRevision;
	uint16_t signaturePublicKeyRevision;
	uint8_t formatExtension[8];
	uint8_t bodyDigest[32];
} SlskHeader;

typedef struct SlskPersonalization {
	uint8_t seeds[6][32];
	uint8_t hmac[32];
	uint8_t headerSignature[256];
} SlskPersonalization;

_Static_assert(sizeof(SlskHeader) == 64 &&
	       offsetof(SlskHeader, bodySize) == 16 &&
	       offsetof(SlskHeader, bodyDigest) == 32, "SLSK fixed header layout");
_Static_assert(sizeof(SlskPersonalization) == 480,
	       "SLSK personalization record size");

#endif
