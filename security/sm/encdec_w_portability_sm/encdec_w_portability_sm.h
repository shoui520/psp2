/*
 * encdec_w_portability_sm.h: Internal types, request packet layouts, and
 * function declarations for PlayStation Vita encdec_w_portability_sm.self
 * (0x80b000..0x80cd00).
 *
 * Function and structure names are ours.
 */

#ifndef ENCDEC_W_PORTABILITY_SM_H
#define ENCDEC_W_PORTABILITY_SM_H

#include "sm_common.h"

#define SCE_SBL_ENCDEC_PORTABILITY_CMD_ENCRYPT		0x0001000au
#define SCE_SBL_ENCDEC_PORTABILITY_CMD_DECRYPT		0x0002000au

#define SCE_SBL_ERROR_ENCDEC_INVALID_ARG		((int)0x800f1716u)
#define SCE_SBL_ERROR_ENCDEC_NOT_SUPPORTED		((int)0x800f1725u)
#define SCE_SBL_ERROR_ENCDEC_CRYPTO_FAILED		((int)0x800f1727u)

typedef struct SceEncdecPortabilityReq {
	uint32_t keyId;					/* 0x00: 1..20 */
	uint32_t inSize;				/* 0x04: 16 or 32 */
	uint8_t inData[32];				/* 0x08..0x27 */
	uint32_t outSize;				/* 0x28: must equal inSize */
	uint8_t outData[32];				/* 0x2c..0x4b */
	uint8_t iv[16];					/* 0x4c..0x5b */
} SceEncdecPortabilityReq;

_Static_assert(sizeof(SceEncdecPortabilityReq) == 0x5c, "SceEncdecPortabilityReq size");

typedef struct SceEncdecPortabilityCommPacket {
	uint32_t pktSize;				/* 0x00 */
	uint32_t cmdId;					/* 0x04 */
	int32_t result;					/* 0x08 */
	uint32_t reserved0c[13];			/* 0x0c..0x3f */
	SceEncdecPortabilityReq req;			/* 0x40..0x9b */
} SceEncdecPortabilityCommPacket;

_Static_assert(sizeof(SceEncdecPortabilityCommPacket) == 0x9c, "SceEncdecPortabilityCommPacket size");

void smServiceInvokeCb(void *self, void *commBuf);
void encdecCmd1000aEncrypt(void *commBuf);
void encdecCmd2000aDecrypt(void *commBuf);

int bigmacStartAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);

#endif /* ENCDEC_W_PORTABILITY_SM_H */
