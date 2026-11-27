/*
 * sw_wgn_crypto.c — SRM/certificate/ECDSA/ECDH/RTT/Move-hash command handlers,
 * DTCP curve parameters, J-AES content-key derivation, key-pool scheduler,
 * DMA page mapping helpers, and TX PCP framing parser.
 */
#include "sw_wgn_internal.h"

#include <stdlib.h>
#include <string.h>

/* DTCP 160-bit prime-field ECC parameters */
static const struct SwEccCurveTable g_swEccCurveTable = {
	0,
	{
		0x94, 0xa8, 0xe1, 0x6a, 0xad, 0x4f, 0x50, 0xef, 0xe2, 0x9b,
		0x9d, 0x5c, 0x0c, 0x0b, 0x6d, 0x48, 0x1b, 0x27, 0xde, 0x53,
		0x00, 0x00, 0x00, 0x00,
	},
	{
		0x94, 0xa8, 0xe1, 0x6a, 0xad, 0x4f, 0x50, 0xef, 0xe2, 0x9b,
		0x9d, 0x5c, 0x0c, 0x0b, 0x6d, 0x48, 0x1b, 0x27, 0xde, 0x50,
		0x00, 0x00, 0x00, 0x00,
	},
	{
		0x4f, 0xd9, 0x18, 0xe5, 0xb0, 0xc4, 0x51, 0x3f, 0xa3, 0xef,
		0x30, 0x33, 0xfe, 0x08, 0xf0, 0x7c, 0x3b, 0x08, 0xc7, 0x6d,
		0x00, 0x00, 0x00, 0x00,
	},
	{
		0x02, 0x27, 0x8c, 0x8a, 0xd2, 0x47, 0xa4, 0x6b, 0x5c, 0x53,
		0xde, 0x4c, 0xc9, 0xa0, 0x07, 0x24, 0x88, 0xf0, 0x28, 0xa3,
		0x00, 0x00, 0x00, 0x00,
	},
	{
		0x78, 0x66, 0x89, 0x13, 0xde, 0xab, 0x22, 0x75, 0x9e, 0x58,
		0x7e, 0xb3, 0x93, 0xd7, 0xda, 0x46, 0xce, 0x24, 0xb1, 0x96,
		0x00, 0x00, 0x00, 0x00,
	},
	{
		0x4d, 0x2b, 0xa9, 0x96, 0xa7, 0xe3, 0xd9, 0xaf, 0x4d, 0x26,
		0x6a, 0xfe, 0x00, 0x44, 0x3b, 0x95, 0xd6, 0x87, 0x06, 0x06,
		0x00, 0x00, 0x00, 0x00,
	},
};

/* J-AES per-E-EMI Kc derivation constants (6 entries of 16 bytes; first 12 bytes used) */
static const unsigned char g_swJaesKcConst[6][16] = {
	{ 0xa3, 0x80, 0x6f, 0x97, 0x16, 0x35, 0xc1, 0x79, 0x6d, 0xea, 0xed, 0x24, 0x73, 0xcb, 0x04, 0xf3 },
	{ 0x28, 0xb5, 0x22, 0xc7, 0xcd, 0xb2, 0x2d, 0x49, 0x0e, 0xf8, 0xe3, 0xd4, 0x0c, 0x14, 0x57, 0x84 },
	{ 0x7c, 0x0d, 0x87, 0xc7, 0x6c, 0x19, 0x56, 0x62, 0xf9, 0x8c, 0x14, 0xe6, 0x89, 0xad, 0xf2, 0x3f },
	{ 0xe6, 0xed, 0x3e, 0x30, 0x78, 0xff, 0x0e, 0x5b, 0x3a, 0x27, 0x3c, 0xcb, 0x60, 0xfd, 0x37, 0x6c },
	{ 0x45, 0xb1, 0x46, 0xa0, 0x61, 0x1a, 0x50, 0x1a, 0xc3, 0x2b, 0xfa, 0xd4, 0x1e, 0xcd, 0x33, 0x42 },
	{ 0xde, 0x66, 0x5a, 0xde, 0xc0, 0x15, 0x61, 0x12, 0xcf, 0x26, 0x28, 0xeb, 0x51, 0x72, 0x90, 0x50 },
};

/* J-AES IV derivation constant (first 8 bytes used) */
static const unsigned char g_swJaesIvConst[8] = {
	0x95, 0xdc, 0x3a, 0x44, 0x90, 0x28, 0xeb, 0x3c,
};

#define readBe16(p) ((uint16_t)(((uint16_t)(p)[0] << 8) | (p)[1]))

#define readBe32(p) ( \
	((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | \
	((uint32_t)(p)[2] << 8) | (p)[3])

static int swLockMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelLockLwMutex(mtx, 1, NULL);
}

static int swUnlockMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelUnlockLwMutex(mtx, 1);
}

static int swCreateMtx(SceKernelLwMutexWork *mtx)
{
	int ret = sceKernelCreateLwMutex(mtx, "sce_sws_mutex", 0x2000u, 0, NULL);

	return (ret >= 0) ? 0 : ret;
}

static int swDeleteMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelDeleteLwMutex(mtx);
}

/* GET_CERT command handler */
int swCmdGetCert(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwNvpTable *nvp = ctx->shmNvp;

	memcpy(arg + 8, nvp->cert, nvp->certLen);
	*(uint32_t *)(arg + 4) = nvp->certLen;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_CERT: cdev_id=%d cert_size=%d\n", arg[0], nvp->certLen);
	arg[2] = 0;
	if (swWgnIsCommonKeyExpired(ctx) != 0)
		arg[2] = 27;
	return 0;
}

/* GET_SRM command handler */
int swCmdGetSrm(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwShmTable *tbl = ctx->shmTbl;
	uint32_t srmSize;

	swLockMtx(&ctx->semSrm);
	srmSize = tbl->srmSize;
	if (srmSize != 0) {
		if (srmSize > SW_MAX_SRM_SIZE)
			srmSize = 0;
		else
			memcpy(arg + 8, tbl->srm, srmSize);
	}
	swUnlockMtx(&ctx->semSrm);
	*(uint32_t *)(arg + 4) = srmSize;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_SRM: cdev_id=%d srm_size=%d\n", arg[0], srmSize);
	arg[2] = 0;
	return 0;
}

/* parse SRM CRL chain, returning total length and generation count (xsrmc) */
static int swGetXsrmc(const unsigned char *srm, uint32_t size, unsigned char *outCount)
{
	uint32_t pos = 4;
	int count = -1;

	if (size > 4u) {
		while (pos < size) {
			uint16_t crlLen = readBe16(srm + pos);

			if (crlLen < 44u) {
				if ((g_swWgnLogMask & 1u) != 0)
					swMwTrace("sw: get_xsrmc: ERROR, invalid SRM, nCRLLength < 44\n");
				break;
			}
			if (pos + crlLen > size) {
				if ((g_swWgnLogMask & 1u) != 0)
					swMwTrace("sw: get_xsrmc: ERROR, invalid SRM, nCurrentSrmLen > nSrmSize\n");
				break;
			}
			pos += crlLen;
			++count;
		}
	}
	if (count < 0)
		return -1;
	*outCount = (unsigned char)count;
	return (int)pos;
}

/* UPD_SRM command handler */
int swCmdUpdSrm(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwShmTable *tbl = ctx->shmTbl;
	unsigned char ownXsrmg = (unsigned char)(ctx->shmNvp->cert[1] >> 4);
	unsigned char *srmData = arg + 8;
	int srmLen = *(int *)(arg + 4);
	uint32_t maxKeep;
	uint16_t targetVer, ownVer;
	unsigned char targetCnt = 0;
	unsigned char ownCnt = 0;
	int parsedLen;
	int ownSize;
	int doUpdate = 0;
	uint32_t updateSize = 0;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: UPD_SRM: srm_size=%d\n", srmLen);

	if (ownXsrmg == 0) {
		maxKeep = 128;
	} else if (ownXsrmg == 1) {
		maxKeep = 1024;
	} else {
		arg[2] = 13;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: upd_srm: ERROR, Own [Xsrmg] is strange.(= %d)\n", ownXsrmg);
		return 0;
	}

	if (srmLen < 48 || srmLen > SW_MAX_SRM_SIZE) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: upd_srm: ERROR, The size of SRM to be updated is strange.(= %d)\n",
				  srmLen);
		return 0;
	}

	if ((g_swWgnLogMask & 4u) != 0)
		swWgnDumpHex("upd_srm:\n", srmData, (uint32_t)srmLen);

	targetVer = readBe16(srmData + 2);
	parsedLen = swGetXsrmc(srmData, (uint32_t)srmLen, &targetCnt);
	if (parsedLen == -1) {
		arg[2] = 2;
		return 0;
	}

	swLockMtx(&ctx->semSrm);
	ownSize = (int)tbl->srmSize;
	if (ownSize == 0) {
		ownVer = 0;
		ownCnt = 0;
	} else if (ownSize < 48) {
		swUnlockMtx(&ctx->semSrm);
		arg[2] = 13;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: upd_srm: ERROR, The size of own SRM is strange.(= %d)\n",
				  ownSize);
		return 0;
	} else {
		ownVer = readBe16(tbl->srm + 2);
		if (swGetXsrmc(tbl->srm, (uint32_t)ownSize, &ownCnt) == -1) {
			swUnlockMtx(&ctx->semSrm);
			arg[2] = 13;
			return 0;
		}
	}

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: upd_srm: TargetXsrmv %x, OwnXsrmv %x, TargetXsrmc %d, OwnXsrmc %d, OwnXsrmgInOwnCert %x\n",
			  targetVer, ownVer, targetCnt, ownCnt, ownXsrmg);

	if (targetVer > ownVer) {
		doUpdate = 1;
		updateSize = (targetCnt > ownXsrmg) ? maxKeep : (uint32_t)parsedLen;
	} else if (targetVer == ownVer && targetCnt > ownCnt && ownXsrmg != ownCnt) {
		doUpdate = 1;
		updateSize = (targetCnt > ownXsrmg) ? maxKeep : (uint32_t)parsedLen;
	}

	if (doUpdate) {
		int vfy = swVerifySrmSignature(srmData, (int)updateSize, ctx->shmNvp->dtlaPubKey);

		if (vfy != 0) {
			arg[2] = 12;
		} else {
			uint32_t clearLen = (tbl->srmSize < SW_MAX_SRM_SIZE) ?
					    tbl->srmSize : SW_MAX_SRM_SIZE;

			memset(tbl->srm, 0, clearLen);
			tbl->srmSize = 0;
			memcpy(tbl->srm, srmData, updateSize);
			tbl->srmSize = updateSize;
			arg[2] = 0;
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: upd_srm: updated. size=%d\n", updateSize);
		}
	} else {
		arg[2] = 17;
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: upd_srm: no update. currnet size=%d\n", tbl->srmSize);
	}
	swUnlockMtx(&ctx->semSrm);
	return 0;
}

/* scan SRM entries to check whether 5-byte device ID (devHi:devLo) is revoked */
static int swRoutineChkRevoked(const unsigned char *srm, uint32_t srmSize,
			       unsigned char devHi, uint32_t devLo)
{
	const unsigned char *crl;
	uint32_t totalRead = 4;
	int found = 0;
	unsigned char status = 0;

	if (srm == NULL) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: routine_chk_revoked: ERROR, The pointer of SRM data is NULL.\n");
		return 13;
	}
	if (srmSize == 0)
		return 0;
	if (srmSize < 48u) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: routine_chk_revoked: ERROR, The size of SRM is strange.(0x%x)\n",
				  srmSize);
		return 13;
	}

	crl = srm + 4;
	while (totalRead < srmSize && !found) {
		uint16_t crlLen = readBe16(crl);
		const unsigned char *p;
		uint16_t consumed = 0;
		uint16_t bodyLen;

		if (crlLen < 44u) {
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: routine_chk_revoked: ERROR, The value of [CRL Length] is strange.(0x%x)\n",
					  crlLen);
			return 13;
		}
		totalRead += crlLen;
		bodyLen = (uint16_t)(crlLen - 42u);
		p = crl + 2;

		while (consumed != bodyLen && !found) {
			unsigned char hdr = *p++;
			unsigned char entryType = hdr & 0xE0u;
			unsigned char numEntries = hdr & 0x1Fu;
			unsigned int idx;

			++consumed;
			if (entryType == 0) {
				for (idx = 0; idx < numEntries; ++idx) {
					unsigned char hi = p[0];
					uint32_t lo = readBe32(p + 1);

					p += 5;
					consumed = (uint16_t)(consumed + 5u);
					if (hi == devHi && lo == devLo) {
						found = 1;
						status = 11;
						if ((g_swWgnLogMask & 1u) != 0)
							swMwTrace("sw: routine_chk_revoked: ERROR, The target device is to be revoked (single), means target's DevID is on SRM.\n");
						break;
					}
				}
			} else if (entryType == 0x20u) {
				for (idx = 0; idx < numEntries; ++idx) {
					unsigned char startHi = p[0];
					uint32_t startLo = readBe32(p + 1);
					uint16_t span = readBe16(p + 5);
					uint32_t endLo = startLo + span;
					unsigned char endHi;

					p += 7;
					consumed = (uint16_t)(consumed + 7u);
					if (endLo >= startLo) {
						endHi = startHi;
					} else if (startHi == 0xFFu) {
						swMwTrace("sw: routine_chk_revoked: WARN, The SRM is strange, because [number] is too wide.\n");
						endLo = 0xFFFFFFFFu;
						endHi = startHi;
					} else {
						endHi = (unsigned char)(startHi + 1u);
					}

					if (startHi == endHi) {
						if (startHi == devHi && devLo >= startLo && devLo <= endLo) {
							found = 1;
							status = 11;
							if ((g_swWgnLogMask & 1u) != 0)
								swMwTrace("sw: routine_chk_revoked: ERROR, The target device is to be revoked (series1), means target's DevID is on SRM.\n");
							break;
						}
					} else if (startHi == devHi) {
						if (devLo >= startLo) {
							found = 1;
							status = 11;
							if ((g_swWgnLogMask & 1u) != 0)
								swMwTrace("sw: routine_chk_revoked: ERROR, The target device is to be revoked (series2), means target's DevID is on SRM.\n");
							break;
						}
					} else if (endHi == devHi && devLo <= endLo) {
						found = 1;
						status = 11;
						if ((g_swWgnLogMask & 1u) != 0)
							swMwTrace("sw: routine_chk_revoked: ERROR, The target device is to be revoked (series3), means target's DevID is on SRM.\n");
						break;
					}
				}
			} else if (numEntries != 0) {
				swMwTrace("sw: routine_chk_revoked: WARN, The value of [EntryType] is strange (= 0x%x) and [NumberOfEntries] is not zero.\n",
					  entryType);
				break;
			}
		}
		crl += crlLen;
	}
	return status;
}

/* CHK_REVOKED command handler */
int swCmdChkRevoked(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwShmTable *tbl = ctx->shmTbl;
	unsigned char devHi;
	uint32_t devLo;

	if ((g_swWgnLogMask & 4u) != 0)
		swWgnDumpHex("CHK_REVOKED: device_id ", arg + 4, 5u);
	devHi = arg[4];
	devLo = readBe32(arg + 5);
	swLockMtx(&ctx->semSrm);
	arg[2] = (unsigned char)swRoutineChkRevoked(tbl->srm, tbl->srmSize, devHi, devLo);
	swUnlockMtx(&ctx->semSrm);
	return 0;
}

/* EXP_KX command handler */
int swCmdExpKx(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char cdevId = arg[0];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: EXP_KX: cdev_id=%d\n", cdevId);

	swLockMtx(&ctx->srcMutex);
	if (cdevId == 0xFFu) {
		struct SwListNode *node;
		struct SwSrcCdev *src0;

		for (node = ctx->srcUsedList.next; node != &ctx->srcUsedList; node = node->next) {
			struct SwSrcCdev *src = (struct SwSrcCdev *)node;

			swLockMtx(&ctx->semCdev);
			if (src->shm->streamRef <= 0)
				swSrcExpireKxm(ctx, src);
			else
				swMwTrace("sw: EXP_KX(Source): WARN, cannot expire in streaming\n");
			swUnlockMtx(&ctx->semCdev);
		}
		src0 = swSrcGetCdev(ctx, 0);
		if (src0 != NULL) {
			swLockMtx(&ctx->semCdev);
			if (src0->shm->streamRef <= 0)
				swSrcExpireKx(ctx, src0);
			else
				swMwTrace("sw: EXP_KX(Source): WARN, cannot expire in streaming\n");
			swUnlockMtx(&ctx->semCdev);
		}
	} else {
		struct SwSrcCdev *src = swSrcGetCdev(ctx, cdevId);

		if (src != NULL) {
			if (src->shm->streamRef <= 0) {
				swLockMtx(&ctx->semCdev);
				if (src->isMove != 0)
					swSrcExpireKxm(ctx, src);
				else
					swSrcExpireKx(ctx, src);
				swUnlockMtx(&ctx->semCdev);
			} else {
				swMwTrace("sw: EXP_KX(Source): WARN, cannot expire in streaming\n");
			}
			swUnlockMtx(&ctx->srcMutex);
			arg[2] = 0;
			return 0;
		}
	}
	swUnlockMtx(&ctx->srcMutex);

	swLockMtx(&ctx->snkMutex);
	if (cdevId == 0xFFu) {
		struct SwListNode *node;

		for (node = ctx->snkUsedList.next; node != &ctx->snkUsedList; node = node->next) {
			struct SwSnkCdev *snk = (struct SwSnkCdev *)node;

			swLockMtx(&ctx->semCdev);
			if (snk->akeState == 0)
				swSnkExpireKx(snk);
			swUnlockMtx(&ctx->semCdev);
		}
		swUnlockMtx(&ctx->snkMutex);
		arg[2] = 0;
		return 0;
	}

	{
		struct SwSnkCdev *snk = swSnkGetCdev(ctx, cdevId);

		if (snk != NULL) {
			swLockMtx(&ctx->semCdev);
			if (snk->akeState == 0)
				swSnkExpireKx(snk);
			swUnlockMtx(&ctx->semCdev);
			swUnlockMtx(&ctx->snkMutex);
			arg[2] = 0;
			return 0;
		}
	}
	swUnlockMtx(&ctx->snkMutex);

	arg[2] = 2;
	if ((g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: EXP_KX: ERROR, invalid cdev_id\n");
	return 0;
}

/* ECDSA_VFY command handler */
int swCmdEcdsaVerify(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint32_t msgLen = *(uint32_t *)(arg + 4);
	unsigned char which = arg[3096];
	const unsigned char *pubKey;
	int ret;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: ECDSA_VFY:\n");
	if (msgLen > 0xC10u) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ECDSA_VFY: ERROR, invalid msg_len %d\n", msgLen);
		return 0;
	}
	if (which == 0) {
		pubKey = ctx->shmNvp->dtlaPubKey;
	} else if (which == 1) {
		pubKey = arg + 3140;
	} else {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ECDSA_VFY: ERROR, pub_key_id(means [which])=0x%x is invalid\n",
				  which);
		return 0;
	}

	ret = swDepEccEcdsaVerify(arg + 8, msgLen, arg + 3100, pubKey);
	if (ret == 0) {
		arg[2] = 0;
	} else if (ret == 1) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ECDSA_VFY: ERROR, verify failed\n");
		arg[2] = 1;
	} else {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ECDSA_VFY: ERROR, param is invalid on Crypto_ECC_EcDsaVerify\n");
		arg[2] = 2;
	}
	return 0;
}

/* generate 40 random bytes (two 20-byte PRNG calls) */
static void swGenRandom40(struct SwWgnCtx *ctx, unsigned char out40[40])
{
	swWgnGenRandom20(ctx, out40);
	swWgnGenRandom20(ctx, out40 + 20);
}

/* ECDSA_SIG command handler */
int swCmdEcdsaSign(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint32_t msgLen = *(uint32_t *)(arg + 4);
	unsigned char privKey[32];
	unsigned char scratch[40];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: ECDSA_SIG:\n");
	if (msgLen > 0xC10u) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ECDSA_SIG: ERROR, invalid msg_len %d\n", msgLen);
		return 0;
	}

	SwMwReadKeyBinExtra(ctx->shmNvp->keyId, 16, scratch, 16);
	swDepAes128Ecb(ctx->shmNvp->privKeyEnc, 32, privKey, 32, scratch, 1);
	swGenRandom40(ctx, scratch);
	if (swDepEccEcdsaSign(arg + 8, msgLen, privKey, scratch, arg + 3096) != 0) {
		arg[2] = 1;
		swMwTrace("sw: ECDSA_SIG: ERROR, sign failed\n");
	} else {
		arg[2] = 0;
	}
	memcpy(privKey, scratch, sizeof(privKey));
	return 0;
}

/* GEN_ECDH_PARM command handler */
int swCmdGenEcdhParm(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char rnd[40];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_ECDH_PARM:\n");
	swGenRandom40(ctx, rnd);
	if (swDepEccEcdhPhase1(arg + 4, arg + 24, rnd) != 0) {
		arg[2] = 1;
		swMwTrace("sw: gen_ecdh_parm: ERROR, phase1 failed\n");
	} else {
		arg[2] = 0;
	}
	return 0;
}

/* GEN_RAND command handler */
int swCmdGenRand(struct SwWgnCtx *ctx, unsigned char *arg)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_RAND:\n");
	swWgnGenRandom20(ctx, arg + 8);
	*(uint32_t *)(arg + 4) = 20;
	arg[2] = 0;
	return 0;
}

/* compute RTT MAC: SHA-1((SHA-1(Kauth||Kauth) + N) mod 2^160) */
static void swComputeRttHash(const unsigned char kauth[12], uint16_t nBe, unsigned char outHash[20])
{
	unsigned char nBytes[2];
	unsigned char hash[20];
	unsigned char keyBuf[24];

	nBytes[0] = (unsigned char)(nBe & 0xFFu);
	nBytes[1] = (unsigned char)(nBe >> 8);
	memcpy(keyBuf, kauth, 12);
	memcpy(keyBuf + 12, kauth, 12);
	swDepSha1Digest(keyBuf, 24, hash);
	memset(keyBuf, 0, sizeof(keyBuf));
	swWgnAddBigEndian(hash, 20, nBytes, 2, hash);
	swDepSha1Digest(hash, 20, outHash);
	memset(hash, 0, sizeof(hash));
}

/* GET_RTT_HASH command handler */
int swCmdGetRttHash(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint16_t nSwapped;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_RTT_HASH: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);

	nSwapped = (uint16_t)(((uint16_t)arg[4] << 8) | arg[5]);
	if (arg[6] == 0) {
		struct SwSrcCdev *src = swSrcGetCdev(ctx, arg[0]);
		struct SwSrcSinkEntry *sink = swSrcGetSink(ctx, arg[1]);

		if (src != NULL && sink != NULL) {
			swComputeRttHash(sink->kauth, nSwapped, arg + 8);
			arg[2] = 0;
			return 0;
		}
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GET_RTT_HASH: ERROR, invalid cdev_id or ake_id\n");
		return 0;
	}
	if (arg[6] == 1) {
		struct SwSnkCdev *snk = swSnkGetCdev(ctx, arg[0]);

		if (snk != NULL && snk->akeId == arg[1]) {
			swComputeRttHash(snk->kauth, nSwapped, arg + 8);
			arg[2] = 0;
			return 0;
		}
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GET_RTT_HASH: ERROR, invalid cdev_id or ake_id\n");
		return 0;
	}
	arg[2] = 2;
	if ((g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: GET_RTT_HASH: ERROR, which=0x%x is invalid\n", arg[6]);
	return 0;
}

/* compute CKC MAC: SHA-1((SHA-1(Kx||Kx) + Nc + nonce) mod 2^160) */
void swComputeCkcMac(const unsigned char *kx, const unsigned char *nc,
		     const unsigned char *nonce, unsigned char *outMac)
{
	unsigned char hash[20];
	unsigned char keyBuf[24];

	memcpy(keyBuf, kx, 12);
	memcpy(keyBuf + 12, kx, 12);
	swDepSha1Digest(keyBuf, 24, hash);
	memset(keyBuf, 0, sizeof(keyBuf));
	swWgnAddBigEndian(hash, 20, nc, 8, hash);
	swWgnAddBigEndian(hash, 20, nonce, 8, hash);
	swDepSha1Digest(hash, 20, outMac);
	memset(hash, 0, sizeof(hash));
}

/* verify trailing 40-byte ECDSA signature on an SRM */
int swVerifySrmSignature(const unsigned char *srm, int srmLen, const unsigned char *pubKey)
{
	int ret = swDepEccEcdsaVerify(srm, (uint32_t)(srmLen - 40), srm + srmLen - 40, pubKey);

	if (ret != 0 && (g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: verify_srm: ERROR, EcDsaVerify() %d\n", ret);
	return ret;
}

/* compute DTCP Move MAC: SHA-1((SHA-1(Kxm||Kxm) + nonce) mod 2^160) */
static void swComputeMoveHash(const unsigned char kx[12], const unsigned char nonce[8],
			      unsigned char outMac[20])
{
	unsigned char hash[20];
	unsigned char keyBuf[24];

	memcpy(keyBuf, kx, 12);
	memcpy(keyBuf + 12, kx, 12);
	swDepSha1Digest(keyBuf, 24, hash);
	memset(keyBuf, 0, sizeof(keyBuf));
	swWgnAddBigEndian(hash, 20, nonce, 8, hash);
	swDepSha1Digest(hash, 20, outMac);
	memset(hash, 0, sizeof(hash));
}

/* GET_MOVE_HASH command handler */
int swCmdGetMoveHash(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *src;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_MOVE_HASH: cdev_id=%d\n", arg[0]);
	src = swSrcGetCdev(ctx, arg[0]);
	if (src != NULL) {
		swComputeMoveHash(src->shm->kx, arg + 4, arg + 12);
	} else {
		struct SwSnkCdev *snk = swSnkGetCdev(ctx, arg[0]);

		if (snk == NULL) {
			arg[2] = 2;
			return 0;
		}
		swComputeMoveHash(snk->shm->kx, arg + 4, arg + 12);
	}
	arg[2] = 0;
	return 0;
}

/* DEL_CDEV command handler */
int swCmdDelCdev(struct SwWgnCtx *ctx, int cdevId)
{
	struct SwSrcCdev *src;
	struct SwSnkCdev *snk;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: DEL_CDEV: cdev_id=%d\n", cdevId);
	src = swSrcGetCdev(ctx, cdevId);
	if (src != NULL) {
		swSrcFreeCdev(ctx, src);
		return 0;
	}
	snk = swSnkGetCdev(ctx, cdevId);
	if (snk != NULL) {
		swSnkFreeCdev(ctx, snk);
		return 0;
	}
	return 2;
}

/* query cognizant flag for source or sink cdev */
int swGetCdevCognizant(struct SwWgnCtx *ctx, unsigned char cdevId)
{
	struct SwSrcCdev *src = swSrcGetCdev(ctx, cdevId);

	if (src != NULL)
		return src->shm->cognizant;
	{
		struct SwSnkCdev *snk = swSnkGetCdev(ctx, cdevId);

		if (snk != NULL)
			return snk->shm->cognizant;
	}
	return 0;
}

int swCryptoSelfTestWrap(void)
{
	return swDepCryptoNoop(NULL);
}

const struct SwEccCurveTable *swGetEccCurveTableRaw(void)
{
	return &g_swEccCurveTable;
}

int swGetDeviceIduWrap(struct SwWgnCtx *ctx, unsigned char *outIdu)
{
	return swDepGetDeviceIdu((void *)(uintptr_t)ctx->magic, outIdu);
}

int swWgnSemGet(struct SwInfo *swInfo, const char *name, SceKernelLwMutexWork *mtx)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_semget: %s%d\n", name, swInfo->wgnInfo->devIndex);
	swCreateMtx(mtx);
	return 0;
}

void swWgnSemFree(struct SwInfo *swInfo, SceKernelLwMutexWork *mtx)
{
	(void)swInfo;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_semfree:\n");
	swDeleteMtx(mtx);
}

void *swWgnShmGet(struct SwInfo *swInfo, const char *name, size_t size, int create,
		  uint32_t *outHandle)
{
	(void)create;
	(void)outHandle;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_shmget: %s%d size=%d\n", name, swInfo->wgnInfo->devIndex,
			  (int)size);
	return malloc(size);
}

void swWgnShmFree(struct SwInfo *swInfo, uint32_t *handle, void *ptr, int size)
{
	(void)swInfo;
	(void)handle;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_shmfree: size=%d\n", size);
	free(ptr);
}

void *swWgnAllocTbl(struct SwInfo *swInfo, uint32_t magicId, int size)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_alloc_tbl: id=%x size=%d\n", magicId, size);
	return swAllocMem(swInfo->wgnInfo, (size_t)size, 0);
}

void swWgnFreeTbl(struct SwInfo *swInfo, void *ptr, int size)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: wgn_free_tbl: size=%d\n", size);
	swFreeMem(swInfo->wgnInfo, ptr, size);
}

void swSetStreamStatus(struct SwInfo *swInfo, unsigned char streamId, uint32_t bits)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	swLockMtx(&st->statMutex);
	st->statusBits |= bits;
	swUnlockMtx(&st->statMutex);
}

void swSetStreamEemi(struct SwInfo *swInfo, unsigned char streamId, unsigned char eemi)
{
	swDepCoreSetCci(swInfo->depCore, streamId, eemi);
}

/* notify dep core of a PCP key/IV change */
void swSendKeyChange(struct SwInfo *swInfo, int streamId, uint32_t contentLen,
		     const void *key, const void *iv, const void *pcpHdr)
{
	struct {
		int hasKey;
		unsigned char key[16];
		unsigned char iv[16];
		uint32_t contentLen;
	} keyParam;

	keyParam.contentLen = contentLen;
	if (key != NULL) {
		keyParam.hasKey = 1;
		memcpy(keyParam.key, key, 16);
		memcpy(keyParam.iv, iv, 16);
	} else {
		keyParam.hasKey = 0;
	}
	if ((g_swWgnLogMask & 0x40u) != 0)
		swMwTrace("sw: key_chg: %d CA=%02x CL=%x\n", (unsigned char)streamId,
			  keyParam.hasKey, keyParam.contentLen);
	swDepCorePushHdr(swInfo->depCore, (unsigned char)streamId, &keyParam, pcpHdr);
}

/* increment 64-bit big-endian Nc by 1 */
void swIncrementNc(const unsigned char *inNc, unsigned char *outNc)
{
	int i;

	for (i = 8; i > 0 && inNc[i - 1] == 0xFFu; --i)
		;
	if (i > 0) {
		memcpy(outNc, inNc, (size_t)i);
		++outNc[i - 1];
	}
	if (8 - i > 0)
		memset(outNc + i, 0, (size_t)(8 - i));
}

void swKmGetKx(struct SwKeyPool *pool, void *outKx)
{
	if (pool == NULL)
		return;
	swLockMtx(&pool->mutex);
	memcpy(outKx, pool->kx, 12);
	swUnlockMtx(&pool->mutex);
}

/* J-AES content key Kc and IV derivation for E-EMI index 0..5 */
void swDeriveContentKeyIv(int eemiIdx, const unsigned char *kx, const unsigned char *nc,
			  unsigned char *outKeyIv)
{
	unsigned char aesKey[16];
	unsigned char input[16];
	unsigned char enc[16];
	int i;

	memcpy(aesKey, kx, 12);
	memcpy(aesKey + 12, g_swJaesKcConst[eemiIdx], 4);
	memcpy(input, g_swJaesKcConst[eemiIdx] + 4, 8);
	memcpy(input + 8, nc, 8);
	swDepAes128Ecb(input, 16, enc, 16, aesKey, 0);
	for (i = 0; i < 16; ++i)
		outKeyIv[i] = (unsigned char)(enc[i] ^ input[i]);

	memcpy(enc, g_swJaesIvConst, 8);
	memcpy(enc + 8, nc, 8);
	swDepAes128Ecb(enc, 16, outKeyIv + 16, 16, outKeyIv, 0);
	if ((g_swWgnLogMask & 0x200u) != 0)
		swMwTrace("sw: J-AES: 0x%x, 0x%x\n", *(uint32_t *)outKeyIv,
			  *(uint32_t *)(outKeyIv + 16));
}

void swKmPrepareKeys(struct SwKeyPool *pool, uint32_t count, const unsigned char *initNc)
{
	struct SwKeySlot *prev = NULL;
	int slotIdx;
	uint32_t i;

	if (count == 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_prepare_keys: FATAL ERROR, init_key_num must be gt 0\n");
		return;
	}
	slotIdx = pool->rear;
	for (i = 0; i < count; ++i) {
		struct SwKeySlot *slot = &pool->slot[slotIdx];
		int j;

		memset(slot, 0, sizeof(*slot));
		if (i != 0)
			swIncrementNc(prev->nc, slot->nc);
		else
			memcpy(slot->nc, initNc, 8);
		slot->flags |= 0x4000u;
		for (j = 0; j < 6; ++j) {
			swDeriveContentKeyIv(j, pool->kx, slot->nc, slot->keyIv[j]);
			slot->flags |= (1u << j);
		}
		pool->rear = (unsigned char)slotIdx;
		slotIdx = (slotIdx + 1) % 3;
		if (slotIdx == pool->front)
			break;
		prev = slot;
	}
}

struct SwKeyPool *swKmAllocPool(struct SwWgnCtx *ctx, const unsigned char *kx)
{
	struct SwKeyPool *pool = (struct SwKeyPool *)swWgnAllocTbl(ctx->swInfo, 0x31313131u,
								   sizeof(*pool));

	if (pool != NULL) {
		memset(pool, 0, sizeof(*pool));
		swCreateMtx(&pool->mutex);
		memcpy(pool->kx, kx, 12);
		pool->lastEemi = 0xFFu;
	}
	return pool;
}

void swKmSetStreamId(struct SwKeyPool *pool, unsigned char streamId)
{
	pool->streamId = streamId;
}

void swKmFreePool(struct SwWgnCtx *ctx, struct SwKeyPool *pool)
{
	if (pool == NULL) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_free_pool: ERROR, pool is null\n");
		return;
	}
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: km_free_pool\n");
	swDeleteMtx(&pool->mutex);
	swWgnFreeTbl(ctx->swInfo, pool, sizeof(*pool));
}

/* background scheduler tick: pre-derive next Kc/IV or next Nc slot */
void swKmSched(struct SwWgnCtx *ctx)
{
	int i;

	for (i = 0; i < SW_MAX_STREAMS; ++i) {
		struct SwKeyPool *pool;

		if (ctx->strmTbl[i].active == 0)
			continue;
		pool = ctx->strmTbl[i].keyPool;
		if (pool == NULL)
			continue;

		swLockMtx(&pool->mutex);
		{
			struct SwKeySlot *slot = &pool->slot[pool->rear];

			if ((slot->flags & 0x4000u) != 0) {
				if ((slot->flags & 0x3Fu) == 0x3Fu) {
					unsigned char nextRear = (unsigned char)((pool->rear + 1u) % 3u);

					if (nextRear != pool->front) {
						struct SwKeySlot *nextSlot = &pool->slot[nextRear];

						pool->rear = nextRear;
						memset(nextSlot, 0, sizeof(*nextSlot));
						swIncrementNc(slot->nc, nextSlot->nc);
						nextSlot->flags |= 0x4000u;
						if ((g_swWgnLogMask & 0x200u) != 0)
							swMwTrace("sw: km_sched: Nc created, %d, f=%d, r=%d, nc=0x%x,%x\n",
								  pool->streamId, pool->front, pool->rear,
								  *(uint32_t *)nextSlot->nc,
								  *(uint32_t *)(nextSlot->nc + 4));
					}
				} else {
					int j;

					for (j = 0; j < 6 && (slot->flags & (1u << j)) != 0; ++j)
						;
					swDeriveContentKeyIv(j, pool->kx, slot->nc, slot->keyIv[j]);
					slot->flags |= (1u << j);
					if ((g_swWgnLogMask & 0x200u) != 0)
						swMwTrace("sw: km_sched: Kc created, %d, f=%d, r=%d, stat=0x%x\n",
							  pool->streamId, pool->front, pool->rear,
							  slot->flags);
				}
			}
		}
		swUnlockMtx(&pool->mutex);
	}
}

void swKmGetCurrentNc(struct SwKeyPool *pool, unsigned char *outNc)
{
	struct SwKeySlot *slot;

	swLockMtx(&pool->mutex);
	slot = &pool->slot[pool->front];
	if ((slot->flags & 0x4000u) == 0)
		swMwTrace("sw: km_get_current_nc: WARN, Nc does not exist yet\n");
	memcpy(outNc, slot->nc, 8);
	swUnlockMtx(&pool->mutex);
}

void swKmGetNextNc(struct SwKeyPool *pool, unsigned char *outNc)
{
	swLockMtx(&pool->mutex);
	if (pool->rear == pool->front) {
		struct SwKeySlot *prev = &pool->slot[pool->front];
		unsigned char nextIdx = (unsigned char)((pool->front + 1u) % 3u);
		struct SwKeySlot *nextSlot = &pool->slot[nextIdx];

		pool->front = nextIdx;
		pool->rear = nextIdx;
		memset(nextSlot, 0, sizeof(*nextSlot));
		swIncrementNc(prev->nc, nextSlot->nc);
		nextSlot->flags |= 0x4000u;
		memcpy(outNc, nextSlot->nc, 8);
	} else {
		struct SwKeySlot *slot;

		pool->front = (unsigned char)((pool->front + 1u) % 3u);
		slot = &pool->slot[pool->front];
		if ((slot->flags & 0x4000u) == 0 && (g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_get_next_nc: FATAL ERROR, no Nc exists\n");
		memcpy(outNc, slot->nc, 8);
	}
	swUnlockMtx(&pool->mutex);
}

static __inline int swEemiToKeyIdx(unsigned char eemi)
{
	switch (eemi) {
	case 0x0Cu:
		return 0;
	case 0x0Au:
		return 1;
	case 0x08u:
		return 2;
	case 0x06u:
		return 3;
	case 0x04u:
		return 4;
	case 0x02u:
		return 5;
	default:
		return -1;
	}
}

void swKmGetCurrentKey(struct SwKeyPool *pool, unsigned char eemi,
		       void **outNc, void **outKey, void **outIv)
{
	int keyIdx = swEemiToKeyIdx(eemi);
	struct SwKeySlot *slot;

	if (keyIdx < 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_get_current_nc_key2: ERROR unknown E-EMI=0x%x\n", eemi);
		return;
	}

	swLockMtx(&pool->mutex);
	slot = &pool->slot[pool->front];
	if ((slot->flags & 0x4000u) == 0 && (g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: km_get_current_nc_key: FATAL ERROR, no Nc exists\n");
	if ((slot->flags & (1u << keyIdx)) == 0) {
		swDeriveContentKeyIv(keyIdx, pool->kx, slot->nc, slot->keyIv[keyIdx]);
		slot->flags |= (1u << keyIdx);
		if ((g_swWgnLogMask & 0x200u) != 0)
			swMwTrace("sw: km_get_current: key created, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
				  pool->streamId, pool->front, pool->rear,
				  pool->lastEemi, slot->flags);
	}
	*outKey = slot->keyIv[keyIdx];
	*outIv = slot->keyIv[keyIdx] + 16;
	*outNc = slot->nc;
	swUnlockMtx(&pool->mutex);
}

void swKmGetNextKey(struct SwKeyPool *pool, unsigned char eemi,
		    void **outNc, void **outKey, void **outIv)
{
	int keyIdx = swEemiToKeyIdx(eemi);
	struct SwKeySlot *slot;

	if (keyIdx < 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_get_next_nc_key: FATAL ERROR, unknown E-EMI=0x%x\n", eemi);
		return;
	}

	swLockMtx(&pool->mutex);
	if (pool->rear != pool->front) {
		unsigned char nextIdx = (unsigned char)((pool->front + 1u) % 3u);

		pool->front = nextIdx;
		slot = &pool->slot[nextIdx];
		if ((slot->flags & 0x4000u) == 0 && (g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: km_get_next_nc_key2: FATAL ERROR, no Nc exists\n");
		if ((slot->flags & (1u << keyIdx)) != 0) {
			*outKey = slot->keyIv[keyIdx];
			*outIv = slot->keyIv[keyIdx] + 16;
			*outNc = slot->nc;
			if ((g_swWgnLogMask & 0x200u) != 0)
				swMwTrace("sw: km_get_next: key stock ok, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
					  pool->streamId, pool->front, pool->rear,
					  eemi, slot->flags);
			swUnlockMtx(&pool->mutex);
			return;
		}
	} else {
		struct SwKeySlot *prev = &pool->slot[pool->front];
		unsigned char nextIdx = (unsigned char)((pool->front + 1u) % 3u);

		slot = &pool->slot[nextIdx];
		memset(slot, 0, sizeof(*slot));
		swIncrementNc(prev->nc, slot->nc);
		slot->flags |= 0x4000u;
		pool->rear = nextIdx;
		pool->front = nextIdx;
	}

	swDeriveContentKeyIv(keyIdx, pool->kx, slot->nc, slot->keyIv[keyIdx]);
	slot->flags |= (1u << keyIdx);
	*outKey = slot->keyIv[keyIdx];
	*outIv = slot->keyIv[keyIdx] + 16;
	*outNc = slot->nc;
	if ((g_swWgnLogMask & 0x200u) != 0)
		swMwTrace("sw: km_get_next: key created, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
			  pool->streamId, pool->front, pool->rear, eemi, slot->flags);
	swUnlockMtx(&pool->mutex);
}

int swKmGetThisKey(struct SwKeyPool *pool, unsigned char eemi, const unsigned char *nc,
		   void **outKey, void **outIv)
{
	int keyIdx;
	struct SwKeySlot *slot;
	uint32_t hasNc;
	int status;

	if (eemi == 0) {
		keyIdx = -1;
	} else {
		keyIdx = swEemiToKeyIdx(eemi);
		if (keyIdx < 0)
			return -1;
	}

	swLockMtx(&pool->mutex);
	slot = &pool->slot[pool->front];
	hasNc = slot->flags & 0x4000u;
	if (hasNc != 0 && memcmp(slot->nc, nc, 8) == 0) {
		if (eemi == 0) {
			swUnlockMtx(&pool->mutex);
			return 0;
		}
		if (pool->lastEemi == eemi)
			status = 0;
		else if (pool->lastEemi == 0xFFu)
			status = 4;
		else
			status = 1;
		pool->lastEemi = eemi;
		if ((slot->flags & (1u << keyIdx)) == 0) {
			swDeriveContentKeyIv(keyIdx, pool->kx, nc, slot->keyIv[keyIdx]);
			slot->flags |= (1u << keyIdx);
		}
		*outKey = slot->keyIv[keyIdx];
		*outIv = slot->keyIv[keyIdx] + 16;
		swUnlockMtx(&pool->mutex);
		return status;
	}

	status = 2;
	if (pool->lastEemi == 0xFFu) {
		if (eemi != 0) {
			pool->lastEemi = eemi;
			status = 4;
		}
	} else {
		pool->lastEemi = eemi;
	}

	if (hasNc != 0) {
		if (pool->rear == pool->front) {
			unsigned char expectedNc[8];

			swIncrementNc(slot->nc, expectedNc);
			if (memcmp(expectedNc, nc, 8) != 0) {
				status = 3;
				swMwTrace("sw: km_get_this: WARN, Nc gap occurred\n");
				if ((g_swWgnLogMask & 4u) != 0)
					swWgnDumpHex(" detected nc ", nc, 8u);
				if ((g_swWgnLogMask & 4u) != 0)
					swWgnDumpHex(" expected nc ", expectedNc, 8u);
			}
		} else {
			unsigned char nextIdx = (unsigned char)((pool->front + 1u) % 3u);
			struct SwKeySlot *nextSlot = &pool->slot[nextIdx];

			pool->front = nextIdx;
			if (memcmp(nextSlot->nc, nc, 8) == 0) {
				if (eemi != 0) {
					if ((nextSlot->flags & (1u << keyIdx)) != 0) {
						*outKey = nextSlot->keyIv[keyIdx];
						*outIv = nextSlot->keyIv[keyIdx] + 16;
						if ((g_swWgnLogMask & 0x200u) != 0)
							swMwTrace("sw: km_get_this: key stock ok, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
								  pool->streamId, pool->front, pool->rear,
								  eemi, nextSlot->flags);
					} else {
						swDeriveContentKeyIv(keyIdx, pool->kx, nc,
								     nextSlot->keyIv[keyIdx]);
						nextSlot->flags |= (1u << keyIdx);
						*outKey = nextSlot->keyIv[keyIdx];
						*outIv = nextSlot->keyIv[keyIdx] + 16;
						if ((g_swWgnLogMask & 0x200u) != 0)
							swMwTrace("sw: km_get_this: key created, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
								  pool->streamId, pool->front, pool->rear,
								  pool->lastEemi, nextSlot->flags);
					}
				}
				swUnlockMtx(&pool->mutex);
				return status;
			}
			status = 3;
			swMwTrace("sw: km_get_this: WARN, Nc gap occurr\n");
			if ((g_swWgnLogMask & 4u) != 0)
				swWgnDumpHex(" detected nc ", nc, 8u);
			if ((g_swWgnLogMask & 4u) != 0)
				swWgnDumpHex(" expected nc ", nextSlot->nc, 8u);
		}
	} else {
		status = 5;
	}

	pool->rear = 0;
	pool->front = 0;
	slot = &pool->slot[0];
	memcpy(slot->nc, nc, 8);
	slot->flags = 0x4000u;
	if (eemi != 0) {
		swDeriveContentKeyIv(keyIdx, pool->kx, nc, slot->keyIv[keyIdx]);
		slot->flags |= (1u << keyIdx);
		*outKey = slot->keyIv[keyIdx];
		*outIv = slot->keyIv[keyIdx] + 16;
		if ((g_swWgnLogMask & 0x200u) != 0)
			swMwTrace("sw: km_get_this: key created2, %d, f=%d, r=%d, E-EMI=%d, stat=0x%x\n",
				  pool->streamId, pool->front, pool->rear,
				  pool->lastEemi, slot->flags);
	}
	swUnlockMtx(&pool->mutex);
	return status;
}

void swKmResetKx(struct SwKeyPool *pool, const unsigned char *newKx)
{
	if (pool == NULL)
		return;
	swLockMtx(&pool->mutex);
	if (newKx != NULL)
		memcpy(pool->kx, newKx, 12);
	else
		memset(pool->kx, 0, 12);
	pool->rear = pool->front;
	pool->slot[pool->front].flags = 0x4000u;
	swUnlockMtx(&pool->mutex);
}

/* DMA virtual-address scatter-gather mapping (identity mapping in user mode) */
int swDmaMapSg(struct SwScatterEntry *sgList, int count, int dir)
{
	int i;

	(void)dir;
	for (i = 0; i < count; ++i) {
		sgList[i].dmaAddr = sgList[i].page + sgList[i].offset;
		sgList[i].dmaLen = sgList[i].length;
	}
	return count;
}

void swDmaUnmapSg(struct SwScatterEntry *sgList, int count, int dir)
{
	(void)sgList;
	(void)count;
	(void)dir;
}

void *swDmaAllocZero(struct SwWgnInfo *info, size_t size)
{
	void *ptr = malloc(size);

	if (ptr != NULL) {
		memset(ptr, 0, size);
		swLockMtx(&info->stats.memMutex);
		info->stats.allocBytes += (int)size;
		swUnlockMtx(&info->stats.memMutex);
	}
	return ptr;
}

void swDmaFreeZero(struct SwWgnInfo *info, void *ptr, int size)
{
	swLockMtx(&info->stats.memMutex);
	info->stats.allocBytes -= size;
	swUnlockMtx(&info->stats.memMutex);
	free(ptr);
}

static int swFillChunkTable(uintptr_t baseAddr, int numChunks, uintptr_t *outPages)
{
	int i;

	for (i = 0; i < numChunks; ++i) {
		if (outPages != NULL)
			outPages[i] = baseAddr;
		baseAddr += SW_PAGE_CHUNK_SIZE;
	}
	return i;
}

int swDmaPinUserPages(struct SwWgnInfo *info, struct SwDmaPageMap *map,
		      const uint32_t *iov, int dir)
{
	int got;

	map->firstOffset = 0;
	map->totalLen = iov[1];
	map->direction = dir;
	map->numPages = (int)(((map->totalLen + (SW_PAGE_CHUNK_SIZE - 1u)) &
			       ~(SW_PAGE_CHUNK_SIZE - 1u)) >> 18);
	map->pages = (uintptr_t *)swAllocMem(info, (size_t)(4 * map->numPages), 0);
	if (map->pages == NULL) {
		swMwTrace("sw: alloc maplist fail\n");
		return 7;
	}
	got = swFillChunkTable(iov[0], map->numPages, map->pages);
	if (got == map->numPages)
		return 0;
	swMwTrace("sw: can't get user pages\n");
	swFreeMem(info, map->pages, 4 * map->numPages);
	return (got < 0) ? got : 11;
}

void swDmaUnpinUserPages(struct SwWgnInfo *info, struct SwDmaPageMap *map)
{
	swFreeMem(info, map->pages, 4 * map->numPages);
	map->pages = NULL;
}

/* parse incoming 14-byte DTCP PCP headers from TX buffer for sink decryption */
static size_t swPcpConsumeHeader(struct SwInfo *swInfo, unsigned char streamId,
				 const unsigned char *src, size_t len)
{
	struct SwPcpParser *pcp = &swInfo->stream[streamId].pcp;
	size_t remain = len;

	while (remain != 0) {
		uint32_t need = 14u - pcp->hdrPos;

		if (remain < need) {
			memcpy(pcp->hdr + pcp->hdrPos, src, remain);
			pcp->hdrPos += (uint32_t)remain;
			remain = 0;
		} else {
			uint32_t contentLen;

			memcpy(pcp->hdr + pcp->hdrPos, src, need);
			src += need;
			remain -= need;
			pcp->curOffset += need;
			pcp->hdrPos = 0;
			contentLen = readBe32(pcp->hdr + 10);
			pcp->payloadRemain = (contentLen + 15u) & ~15u;
			swSnkPcpDetect(swInfo->wgnCtx, streamId, pcp->hdr, contentLen);
			if (pcp->payloadRemain != 0) {
				pcp->inPayload = 1;
				return remain;
			}
		}
	}
	return remain;
}

int swPcpProcessTxSgl(struct SwInfo *swInfo, unsigned char streamId,
		      const struct SwDmaSgl *sgl, uint32_t len, uint32_t flags)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	struct SwPcpParser *pcp = &st->pcp;
	size_t avail = len - pcp->curOffset;
	size_t chunk;
	size_t leftover;

	if (pcp->inPayload == 0) {
		if (st->txP != st->txR)
			return -1;
		avail = swPcpConsumeHeader(swInfo, streamId,
					   (const unsigned char *)sgl->vaddr + pcp->curOffset,
					   avail);
		if (avail == 0) {
			pcp->curOffset = 0;
			swNotifyTxRp(swInfo, streamId, 1);
			if ((flags & 0x20000000u) != 0)
				st->txLast = 1;
			return 0;
		}
	}

	chunk = (pcp->payloadRemain <= avail) ? pcp->payloadRemain : avail;
	leftover = avail - chunk;
	if (leftover == 0)
		flags |= 0x200000u;
	swTxQueuePush(st, (uintptr_t)sgl->vaddr + pcp->curOffset, (uint32_t)chunk, flags);
	pcp->payloadRemain -= (uint32_t)chunk;
	if (pcp->payloadRemain == 0)
		pcp->inPayload = 0;
	if (leftover != 0) {
		pcp->curOffset += (uint32_t)chunk;
		return -1;
	}
	pcp->curOffset = 0;
	return 0;
}
