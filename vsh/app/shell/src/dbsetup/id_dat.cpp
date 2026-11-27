/*
 * id_dat.cpp: memory-card ux0:id.dat reader, writer, HMAC-SHA1 digest verifier,
 * MID generator, and OpenPSID-derived DID generator (0x813613E4..0x81362107).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <kernel/threadmgr.h>
extern "C" {
#include <kernel/openpsid.h>
#include <libsfmt19937.h>
}
#include <libsha1.h>
#include <libhmac.h>
#include <ini_file_processor/ini_file_processor.h>
#include "dbsetup_internal.h"

#define ID_DAT_INI_POOL_SIZE		4096U
#define ID_DAT_INI_OPEN_FLAGS		0x300006
#define ID_DAT_KEY_LEN			10U
#define ID_DAT_XOR_MASK			0x7FU

#define ID_DAT_FIELD_MID		0x01U
#define ID_DAT_FIELD_DID		0x02U
#define ID_DAT_FIELD_AID		0x04U
#define ID_DAT_FIELD_OID		0x08U
#define ID_DAT_FIELD_SVR		0x10U

extern "C" {
extern int vshSblAimgrIsCEX(void);
extern int sceSblPmMgrGetCurrentMode(unsigned char *outMode);
extern int vshSysconIsIduMode(void);
extern int vshNpDrmIsLooseAccountBind(void);
extern SceUInt32 SceThreadmgr_47F6DE49(void);
extern unsigned long long sce_paf_strtoull(const char *str, char **endptr, int base);
}

/* 0x81547530: cached result of account-binding mode check (-1 = uninitialized) */
static int s_accountBoundModeCache = -1;

static const char s_hexDigits[] = "0123456789abcdef";

/* 0x813620F8 */
static void *idDatIniAlloc(SceSize size, void *context)
{
	(void)context;
	return sce_paf_malloc(size);
}

/* 0x81362100 */
static void idDatIniFree(void *ptr, void *context)
{
	(void)context;
	sce_paf_free(ptr);
}

/* 0x813613E4 */
bool shellIdDatIsAccountBoundMode(void)
{
	int cached = s_accountBoundModeCache;
	if (cached < 0) {
		cached = 0;
		if (vshSblAimgrIsCEX() != 0) {
			unsigned char pmMode = 0;
			sceSblPmMgrGetCurrentMode(&pmMode);
			if (pmMode == 0 && vshSysconIsIduMode() == 0)
				cached = (vshNpDrmIsLooseAccountBind() != 1) ? 1 : 0;
		}
		s_accountBoundModeCache = cached;
	}
	return cached != 0;
}

/* 0x81361450: open or create <prefix>id.dat via SceIniFileProcessor */
static int idDatOpenIni(const char *devicePrefix, IniFileProcessor *processor, bool readOnly)
{
	char pathBuf[64];
	const char *targetPath = "ux0:id.dat";
	if (devicePrefix != NULL) {
		sce_paf_snprintf(pathBuf, sizeof(pathBuf), "%sid.dat", devicePrefix);
		targetPath = pathBuf;
	}

	IniAllocatorOps allocOps;
	allocOps.alloc = idDatIniAlloc;
	allocOps.free = idDatIniFree;

	IniConfig config;
	config.poolSize = ID_DAT_INI_POOL_SIZE;
	config.reserved = ID_DAT_INI_POOL_SIZE;
	config.ops = &allocOps;

	int err = processor->initialize(&config);
	if (err == 0) {
		const char *mode = readOnly ? "r" : "rw";
		err = processor->open(targetPath, mode, ID_DAT_INI_OPEN_FLAGS);
		if (err == SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME && !readOnly)
			return processor->create(targetPath, "rw", ID_DAT_INI_OPEN_FLAGS);
	}
	return err;
}

/* 0x8136150E: compute 40-char hex HMAC-SHA1 over hex(AID)+MID+DID */
static paf::string idDatComputeDigest(const ShellActivationInfo *info)
{
	paf::string message;
	const unsigned char *aidBytes = (const unsigned char *)&info->aid;
	for (int i = 0; i < 8; ++i) {
		message.append(1, s_hexDigits[aidBytes[i] >> 4]);
		message.append(1, s_hexDigits[aidBytes[i] & 0x0FU]);
	}

	paf::string suffix("+");
	suffix += info->mid.c_str();
	suffix += "+";
	suffix += info->did.c_str();
	message += suffix.c_str();

	unsigned char hmacKey[ID_DAT_KEY_LEN] = {
		0x31, 0x10, 0x05, 0x10, 0xE1, 0x39, 0xAA, 'z', 'F', 'I'
	};
	for (unsigned int i = 0; i < ID_DAT_KEY_LEN; ++i)
		hmacKey[i] ^= ID_DAT_XOR_MASK;

	unsigned char rawDigest[SCE_HMAC_SHA1_DIGEST_SIZE];
	sce_paf_memset(rawDigest, 0, sizeof(rawDigest));
	sceHmacSha1Digest(hmacKey, ID_DAT_KEY_LEN, message.c_str(),
			  (SceUInt32)message.size(), rawDigest);

	paf::string hexDigest;
	for (unsigned int i = 0; i < SCE_HMAC_SHA1_DIGEST_SIZE; ++i) {
		hexDigest.append(1, s_hexDigits[rawDigest[i] >> 4]);
		hexDigest.append(1, s_hexDigits[rawDigest[i] & 0x0FU]);
	}
	return hexDigest;
}

/* 0x8136191A: read MID/DID/AID/OID/SVR/DIG entries and verify HMAC digest */
static int idDatReadEntries(IniFileProcessor *processor, ShellActivationInfo *outInfo)
{
	paf::string storedDigest;
	char keyBuf[128];
	char valBuf[256];

	for (;;) {
		sce_paf_memset(keyBuf, 0, sizeof(keyBuf));
		sce_paf_memset(valBuf, 0, sizeof(valBuf));
		int err = processor->next(keyBuf, valBuf, sizeof(valBuf));
		if (err == INI_RESULT_STREAM_END || err == SCE_INI_FILE_PROCESSOR_ERROR_EMPTY)
			break;
		if (err != 0)
			return err;

		if (sce_paf_strcmp(keyBuf, "MID") == 0) {
			outInfo->mid = valBuf;
		} else if (sce_paf_strcmp(keyBuf, "DID") == 0) {
			outInfo->did = valBuf;
		} else if (sce_paf_strcmp(keyBuf, "AID") == 0) {
			char *endPtr = NULL;
			outInfo->aid = sce_paf_strtoull(valBuf, &endPtr, 16);
		} else if (sce_paf_strcmp(keyBuf, "OID") == 0) {
			outInfo->oid = valBuf;
		} else if (sce_paf_strcmp(keyBuf, "SVR") == 0) {
			char *endPtr = NULL;
			outInfo->svr = (int)sce_paf_strtoul(valBuf, &endPtr, 16);
		} else if (sce_paf_strcmp(keyBuf, "DIG") == 0) {
			storedDigest = valBuf;
		}
	}

	paf::string computedDigest = idDatComputeDigest(outInfo);
	bool digestMismatch = (storedDigest != computedDigest);
	outInfo->hasDigest = digestMismatch;
	if (digestMismatch)
		outInfo->oid.clear();
	return 0;
}

/* 0x81361B3E (internal path-taking form) */
int shellIdDatReadFromPath(ShellActivationInfo *outInfo, const char *devicePrefix)
{
	IniFileProcessor processor;
	int err = idDatOpenIni(devicePrefix, &processor, true);
	if (err == 0)
		err = idDatReadEntries(&processor, outInfo);
	processor.close();
	return err;
}

/* 0x81361B3E (console_info.h entry point) */
int shellActivationRead(ShellActivationInfo *info, int mode)
{
	const char *prefix = (mode == 0) ? NULL : (const char *)(intptr_t)mode;
	return shellIdDatReadFromPath(info, prefix);
}

/* 0x81361B94: update selected fields in <prefix>id.dat and refresh DIG */
int shellIdDatWriteEx(const NpIdDatUpdateInfo *info, SceUInt32 fieldMask,
		      SceUInt8 forceWrite, const char *devicePrefix,
		      SceUInt8 writeInvalidDigest)
{
	if (forceWrite == 0 && !shellIdDatIsAccountBoundMode())
		return 0;

	unsigned char pmMode = 0;
	sceSblPmMgrGetCurrentMode(&pmMode);
	if (pmMode != 0)
		return 0;

	IniFileProcessor processor;
	ShellActivationInfo current;
	int err = idDatOpenIni(devicePrefix, &processor, false);
	if (err == 0) {
		err = idDatReadEntries(&processor, &current);
		if (err == 0 && (forceWrite != 0 || current.aid == 0)) {
			bool digestFieldsChanged = false;

			if ((fieldMask & ID_DAT_FIELD_MID) != 0 &&
			    current.mid != info->memoryCardId) {
				err = processor.set("MID", info->memoryCardId.c_str());
				if (err == 0) {
					current.mid = info->memoryCardId;
					digestFieldsChanged = true;
				}
			}

			if (err == 0 && (fieldMask & ID_DAT_FIELD_DID) != 0 &&
			    current.did != info->deviceId) {
				err = processor.set("DID", info->deviceId.c_str());
				if (err == 0) {
					current.did = info->deviceId;
					digestFieldsChanged = true;
				}
			}

			if (err == 0 && (fieldMask & ID_DAT_FIELD_AID) != 0 &&
			    current.aid != info->accountId) {
				char aidBuf[64];
				sce_paf_memset(aidBuf, 0, sizeof(aidBuf));
				sce_paf_snprintf(aidBuf, sizeof(aidBuf), "%016llx",
						 (unsigned long long)info->accountId);
				err = processor.set("AID", aidBuf);
				if (err == 0) {
					current.aid = info->accountId;
					digestFieldsChanged = true;
				}
			}

			if (err == 0 && (fieldMask & ID_DAT_FIELD_OID) != 0 &&
			    current.oid != info->onlineId) {
				err = processor.set("OID", info->onlineId.c_str());
			}

			if (err == 0 && (fieldMask & ID_DAT_FIELD_SVR) != 0 &&
			    (SceUInt32)current.svr != info->sysVersion) {
				char svrBuf[64];
				sce_paf_memset(svrBuf, 0, sizeof(svrBuf));
				sce_paf_snprintf(svrBuf, sizeof(svrBuf), "%08X",
						 (unsigned int)info->sysVersion);
				err = processor.set("SVR", svrBuf);
			}

			if (err == 0 && digestFieldsChanged) {
				paf::string newDigest;
				if (writeInvalidDigest != 0)
					newDigest = "INVALID_DIGEST";
				else
					newDigest = idDatComputeDigest(&current);
				err = processor.set("DIG", newDigest.c_str());
			}

			if (err == 0)
				err = processor.flush();
		}
	}

	processor.close();
	return err;
}

/* 0x81361F44 */
int shellIdDatWrite(const NpIdDatUpdateInfo *info, SceUInt32 fieldMask,
		    SceUInt8 forceWrite, int unusedDevicePrefix)
{
	const char *prefix = (unusedDevicePrefix == 0) ? NULL
						       : (const char *)(intptr_t)unusedDevicePrefix;
	return shellIdDatWriteEx(info, fieldMask, forceWrite, prefix, 0);
}

/* 0x81361F56: generate a 32-hex-character random memory-card ID using SFMT-19937 */
void shellIdDatGenerateMid(paf::string *outMid)
{
	SceSfmt19937Context rngCtx;
	SceUInt64 randWords[2];

	SceUInt32 seedLow = SceThreadmgr_47F6DE49();
	if (sceSfmt19937InitGenRand(&rngCtx, seedLow) != 0)
		return;

	randWords[0] = sceSfmt19937GenRand64(&rngCtx);
	sceKernelDelayThread(((SceUInt32)randWords[0] % 100U) + 1U);

	SceUInt32 seedNext = SceThreadmgr_47F6DE49();
	if (sceSfmt19937InitGenRand(&rngCtx, seedNext) != 0)
		return;

	randWords[1] = sceSfmt19937GenRand64(&rngCtx);

	const unsigned char *rawBytes = (const unsigned char *)randWords;
	for (unsigned int i = 0; i < sizeof(randWords); ++i) {
		outMid->append(1, s_hexDigits[rawBytes[i] >> 4]);
		outMid->append(1, s_hexDigits[rawBytes[i] & 0x0FU]);
	}
}

/* 0x8136201C: derive 40-hex-character device ID as SHA-1(OpenPSID) */
int shellIdDatGenerateDid(paf::string *outDid)
{
	SceKernelOpenPsId openPsId;
	sce_paf_memset(&openPsId, 0, sizeof(openPsId));
	int err = sceKernelGetOpenPsId(&openPsId);
	if (err != 0)
		return err;

	unsigned char digest[SCE_SHA1_DIGEST_SIZE];
	sce_paf_memset(digest, 0, sizeof(digest));
	err = sceSha1Digest(&openPsId, sizeof(openPsId), digest);
	if (err != 0)
		return err;

	for (unsigned int i = 0; i < SCE_SHA1_DIGEST_SIZE; ++i) {
		outDid->append(1, s_hexDigits[digest[i] >> 4]);
		outDid->append(1, s_hexDigits[digest[i] & 0x0FU]);
	}
	return 0;
}
