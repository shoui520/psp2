/*
 * swagner's software key store (SWKeyManager/KeyManager/libkm/libswkeystore.c, from its
 * assertion strings): encrypted parameter table loading (ur0:dtcp/param_tbl.enc, SHA-512 +
 * MD5 key derivation + AES-128-CBC), VTRM block storage (tm0:dtcp/B00..B16, AES-128-CBC +
 * HMAC-SHA256 + SBL portability decryption), paramA/paramB/SRM management, and secure RTC
 * validity window tracking.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <libaes.h>
#include <libhmac.h>
#include <libmd5.h>
#include <libsha512.h>

#define LIBSWKEYSTORE_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\SWKeyManager\\KeyManager\\libkm\\libswkeystore.c"
#define KM_PARAM_TBL_PATH	"ur0:dtcp/param_tbl.enc"
#define KM_VTRM_DIR		"tm0:dtcp"
#define KM_PARAM_TBL_MAX	4
#define KM_SRM_LEN		1024
#define KM_DEFAULT_VALID_SEC	31470526ULL	/* 0x1E033BE */
#define KM_RENEW_THRESHOLD_SEC	28878526ULL	/* 0x1B8A6BE */

#define KM_ERR_NOT_ACTIVATED	13001
#define KM_ERR_KS_ID_MISMATCH	13002
#define KM_ERR_NO_SECURE_CLOCK	13003
#define KM_ERR_NOT_OPEN		13004
#define KM_ERR_EXPIRED		13005
#define KM_ERR_NO_PARAM_TBL	13101
#define KM_ERR_BAD_PARAM_TBL	13102
#define KM_ERR_DIGEST		13103
#define KM_ERR_DECRYPT		13104
#define KM_ERR_IO		13201
#define KM_ERR_FILE_IO		13202
#define KM_ERR_BUFFER_SMALL	13204
#define KM_ERR_BUFFER_LARGE	13205
#define KM_ERR_INVALID_STATE	13206
#define KM_ERR_INVALID_ARG	13207
#define KM_ERR_VTRM		13208
#define KM_ERR_ROLLBACK		13209
#define KM_ERR_CLOCK_BEFORE	13210

#define SCE_ERROR_ERRNO_ENOENT	(-2147418110)	/* 0x80010002 */
#define SCE_ERROR_ERRNO_EEXIST	(-2147418095)	/* 0x80010011 */
#define SCE_RTC_ERROR_INVALID_TICK	(-2145054718)

struct KmAesCbcContext {
	SceAesContext aes;
	unsigned char iv[32];
};

struct KmParamInfo {
	SceUInt64 genTime;
	SceUInt64 actTime;
	unsigned int size;
	unsigned int pad;
};

struct KmParamTbl {
	unsigned short ksId;
	unsigned short tblId;
	unsigned short nparam;
	unsigned short pad;
	struct KmParamInfo info[KM_PARAM_TBL_MAX];
	unsigned char *paramA[KM_PARAM_TBL_MAX];
	unsigned int paramBSize;
	unsigned char *paramB;
};

struct KmKeyDesc {
	unsigned char *fallbackKey;
	unsigned char *mainKey;
	unsigned char *iv;
};

struct KmStat {
	unsigned char ver;
	unsigned char certRest[3];
	unsigned int flags;
	SceUInt64 startTime;
	unsigned short tblId;
	unsigned short tblOff;
	unsigned short keyId;
	unsigned short srmVer;
	unsigned short srmLen;
	unsigned char kxLabel;
	unsigned char stHmac[12];
	unsigned char kxHmac[12];
	unsigned char tail[13];
};

struct KmTimeConfig {
	SceUInt64 validTime;
	SceUInt64 periodTime;
	SceUInt64 timeOffset;
	unsigned char reserved[40];
};

struct KmStatusEntry {
	unsigned short keyId;
	unsigned short pad[3];
	SceUInt64 actTime;
};

struct KmStatusInfo {
	unsigned short tblId;
	unsigned short tblOff;
	unsigned short keyId;
	unsigned short alignKey;
	unsigned int paramStat;
	unsigned int alignStat;
	SceUInt64 startTime;
	SceUInt64 validTime;
	unsigned short curTblId;
	unsigned short nparam;
	unsigned int alignHeader;
	struct KmStatusEntry entries[KM_PARAM_TBL_MAX];
};

struct KmActMgrInfo {
	unsigned short keyId;
	unsigned short pad[3];
	SceUInt64 genTime;
};

struct KmPortabilityBlock {
	int size;
	unsigned char data[32];
};

/* external libc, VshBridge and actmgr hooks */
void _Assert(const char *message, const char *func);
int vshRtcGetCurrentSecureTick(SceRtcTick *tick);
int vshSblSsDecryptWithPortability(int mode, void *iv, void *inBuf, void *outBuf);
int dtcpip_actmgr_update_info(const void *params, int count);

/* built-in default paramA[0], paramA[1] and paramB tables */
static unsigned char g_kmDefaultParamAFirst[244] = {
	0x00, 0x07, 0x20, 0x00, 0xd8, 0xe3, 0x04, 0x68, 0xa5, 0x02, 0xa8, 0x6c, 0x23, 0x6a, 0xbb, 0x13,
	0xf0, 0x34, 0x95, 0x7a, 0x05, 0x87, 0x00, 0x00, 0x61, 0x20, 0xd3, 0x1e, 0x94, 0xb9, 0x88, 0xf8,
	0xbc, 0xab, 0x5e, 0x91, 0x02, 0x07, 0x38, 0x00, 0x1d, 0x39, 0xac, 0x5b, 0x5d, 0xac, 0x7e, 0xfc,
	0x7c, 0x7d, 0x8f, 0x81, 0x22, 0x6d, 0x93, 0xf5, 0xe3, 0x54, 0xc7, 0x6a, 0x46, 0x4d, 0xb0, 0x8f,
	0xbd, 0xcb, 0xbe, 0x0d, 0x4b, 0xa2, 0x32, 0x34, 0x01, 0x10, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x50, 0x03, 0x15, 0x0a, 0x16, 0x89, 0xe3, 0xc3, 0x63, 0x72, 0x3c,
	0x03, 0x07, 0x58, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x4e, 0x66, 0x1a, 0x10, 0xf7, 0x19,
	0x4e, 0xba, 0xc1, 0x0d, 0x18, 0xf0, 0x41, 0x92, 0x69, 0x47, 0xe3, 0x43, 0x9e, 0x8a, 0x6a, 0xa1,
	0x3f, 0x79, 0x7d, 0x4e, 0x3d, 0x29, 0x1b, 0x2e, 0xa6, 0xc5, 0xdc, 0x72, 0xd6, 0x67, 0x92, 0x9f,
	0x25, 0x96, 0x69, 0xd4, 0x01, 0x9f, 0xd4, 0x23, 0x3f, 0xa7, 0x19, 0x13, 0xdd, 0x75, 0x4e, 0xdc,
	0x3a, 0xa9, 0x3a, 0xf3, 0x47, 0xcc, 0x49, 0xdf, 0x00, 0x4d, 0x20, 0xaf, 0xa3, 0xab, 0x1c, 0x18,
	0xbe, 0x2d, 0xe7, 0x67, 0xe8, 0x24, 0x9c, 0x3a, 0x6d, 0x6a, 0x1f, 0xa0, 0x05, 0x07, 0x34, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x9c, 0x1f, 0x35, 0xf1, 0x53, 0x5d, 0x71,
	0xe2, 0x70, 0x6b, 0xff, 0x79, 0xf7, 0x08, 0x4c, 0x0d, 0x69, 0x02, 0x04, 0x01, 0xea, 0x9e, 0x73,
	0x58, 0x06, 0x89, 0x94, 0xfc, 0x9d, 0xce, 0x70, 0x4a, 0x0c, 0xbf, 0xb2, 0x34, 0x97, 0x9d, 0xd6,
	0x38, 0xa4, 0x25, 0x9c,
};

static unsigned char g_kmDefaultParamASecond[244] = {
	0x00, 0x07, 0x20, 0x00, 0xd8, 0xe3, 0x04, 0x68, 0xa5, 0x02, 0xa8, 0x6c, 0x23, 0x6a, 0xbb, 0x13,
	0xf0, 0x34, 0x95, 0x7a, 0x05, 0x87, 0x00, 0x00, 0x61, 0x20, 0xd3, 0x1e, 0x94, 0xb9, 0x88, 0xf8,
	0xbc, 0xab, 0x5e, 0x91, 0x02, 0x07, 0x38, 0x00, 0xc2, 0x22, 0x53, 0x24, 0x50, 0x8a, 0x5b, 0xb2,
	0xdd, 0x40, 0xfd, 0xf7, 0x10, 0x59, 0xf1, 0x7f, 0xab, 0x2d, 0x8c, 0xf5, 0xd9, 0xed, 0x6e, 0x39,
	0xbc, 0x83, 0x8e, 0x93, 0x03, 0xac, 0x47, 0x2e, 0x01, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xa7, 0x5b, 0xa7, 0x49, 0x7e, 0x4a, 0xe9, 0x0e, 0x53, 0x0a, 0xd3, 0x32,
	0x03, 0x07, 0x58, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x4e, 0x74, 0x54, 0x72, 0xac, 0x1c,
	0x25, 0x7d, 0x55, 0x36, 0x65, 0x33, 0x93, 0x2c, 0xc3, 0x39, 0xb8, 0xe6, 0x7f, 0xfd, 0x0f, 0xd4,
	0x58, 0x9e, 0xda, 0x4c, 0x91, 0x47, 0x89, 0xb2, 0x12, 0x1a, 0x50, 0xb5, 0xb6, 0x15, 0x42, 0x34,
	0x24, 0x46, 0x35, 0x84, 0x00, 0x0b, 0x32, 0x5a, 0x28, 0x49, 0x8f, 0xe5, 0x0c, 0x0f, 0x40, 0x40,
	0x64, 0x89, 0x6a, 0x35, 0xa7, 0x61, 0xaf, 0x68, 0x00, 0x72, 0x10, 0x3f, 0x3c, 0xa3, 0xce, 0x98,
	0x78, 0xf8, 0xe7, 0xfa, 0x90, 0x76, 0xa8, 0x92, 0x0f, 0x61, 0x97, 0x9a, 0x05, 0x07, 0x34, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93, 0xb5, 0x5c, 0xdd, 0x99, 0x8c, 0xf5, 0x3e,
	0x6d, 0x04, 0x5b, 0x50, 0x17, 0x80, 0x0f, 0xd6, 0xbd, 0x86, 0xfb, 0xf4, 0x1d, 0xf4, 0xfa, 0x66,
	0xd8, 0xaf, 0x5d, 0xa3, 0x62, 0x04, 0x95, 0x50, 0x57, 0x1c, 0xaf, 0xdc, 0xff, 0x2b, 0xa3, 0xd9,
	0xb5, 0x79, 0xc7, 0x93,
};

static unsigned char g_kmDefaultParamB[72] = {
	0x00, 0x08, 0x40, 0x00, 0x5c, 0x28, 0x81, 0x8c, 0xb9, 0x93, 0xe6, 0xad, 0x0d, 0x7a, 0x6e, 0x3e,
	0xff, 0x5f, 0x6b, 0x58, 0xae, 0xf1, 0x9a, 0x0e, 0x7f, 0x51, 0x8b, 0x8b, 0x4b, 0xd8, 0xe9, 0x0a,
	0x0b, 0xca, 0x5e, 0x6d, 0x20, 0xc6, 0x6a, 0x7e, 0xfb, 0x19, 0x60, 0xf0, 0x33, 0xbd, 0xb8, 0xe0,
	0x6f, 0x0b, 0xf7, 0xad, 0x22, 0x00, 0x00, 0x00, 0xe0, 0xfd, 0xf9, 0x0a, 0x36, 0x0a, 0x04, 0x43,
	0x85, 0x75, 0x02, 0x9a, 0x01, 0x08, 0x00, 0x00,
};

/* SBL portability-wrapped keys for extra key material and param_tbl.enc */
static unsigned char g_kmKey0Alt[64] = {
	0x4d, 0x63, 0x78, 0xca, 0x53, 0x74, 0x54, 0xae, 0xbf, 0x5d, 0x9a, 0x20, 0xd5, 0x71, 0x2e, 0x58,
	0x05, 0x14, 0xc5, 0xda, 0xcf, 0x2b, 0x8a, 0x54, 0xd9, 0xbf, 0x8b, 0x29, 0xef, 0x8c, 0x2a, 0xc9,
	0x7d, 0x91, 0x09, 0x1a, 0x3d, 0x85, 0x5d, 0xd0, 0xfd, 0xcd, 0x4e, 0x97, 0x0d, 0x42, 0xd9, 0x17,
	0x80, 0x9d, 0xe2, 0x1a, 0x20, 0x18, 0x73, 0x96, 0x39, 0x40, 0x4c, 0x77, 0x08, 0x4e, 0x56, 0xd3,
};

static unsigned char g_kmKey0Primary[64] = {
	0xd4, 0x6b, 0x80, 0x4f, 0x11, 0xeb, 0x91, 0x7e, 0xc1, 0x92, 0x4a, 0x0f, 0x68, 0xad, 0x43, 0x51,
	0x5b, 0xea, 0xb5, 0xfb, 0xa1, 0x69, 0x59, 0x86, 0x8a, 0xa7, 0xc4, 0x52, 0x07, 0x23, 0xaf, 0xd0,
	0x94, 0xe9, 0x9c, 0x44, 0x35, 0x97, 0xda, 0x96, 0x6f, 0xc9, 0x2b, 0x51, 0x29, 0x3e, 0x5e, 0x6a,
	0x9d, 0xe7, 0x2b, 0x52, 0x14, 0x8c, 0xba, 0xa0, 0xcc, 0x2a, 0xb0, 0xf3, 0x66, 0xad, 0xd1, 0x17,
};

static unsigned char g_kmKey0Iv[16] = {
	0xb4, 0x14, 0xd6, 0x28, 0x27, 0xfe, 0x67, 0xe3, 0xf0, 0x19, 0x33, 0xbb, 0xdb, 0x23, 0x96, 0xa8,
};

static unsigned char g_kmKey1Alt[64] = {
	0x93, 0xd9, 0x22, 0x0f, 0x3f, 0x2c, 0xc7, 0x23, 0xda, 0x1e, 0x5e, 0x06, 0xab, 0xe6, 0x79, 0xb5,
	0xed, 0x04, 0x99, 0xdc, 0xc8, 0xeb, 0x3a, 0x32, 0xd7, 0xe2, 0x86, 0xfb, 0x30, 0xf0, 0x9a, 0x9c,
	0xee, 0x8b, 0xb1, 0xbf, 0x8f, 0x6b, 0x1d, 0xff, 0xa1, 0x36, 0x5b, 0x2e, 0xd5, 0x82, 0xd0, 0x8e,
	0x6c, 0xfa, 0x33, 0x29, 0x52, 0x3a, 0xad, 0x65, 0x7c, 0x25, 0x89, 0x52, 0xfd, 0x23, 0x63, 0x30,
};

static unsigned char g_kmKey1Primary[64] = {
	0xff, 0x59, 0xcd, 0xb9, 0x10, 0xbb, 0x49, 0xb2, 0x4d, 0x6a, 0x17, 0xe8, 0x22, 0x2e, 0xb0, 0x64,
	0x9e, 0x4d, 0x46, 0xf0, 0xd7, 0x48, 0x05, 0x58, 0x50, 0x45, 0x1b, 0x59, 0xd7, 0x5e, 0x89, 0x31,
	0x65, 0x2f, 0x5d, 0xf2, 0x7f, 0x63, 0xd5, 0xce, 0x7f, 0x38, 0x5a, 0xb8, 0x48, 0x90, 0xf5, 0xd5,
	0x99, 0xca, 0xf1, 0x96, 0x5a, 0xe6, 0xfe, 0x88, 0xb9, 0xeb, 0x8a, 0x85, 0xc6, 0xba, 0xd2, 0x80,
};

static unsigned char g_kmKey1Iv[16] = {
	0x54, 0x9f, 0xc8, 0x42, 0x11, 0xe9, 0x6d, 0x4d, 0x61, 0x94, 0xfa, 0x12, 0x56, 0x7d, 0x65, 0x8f,
};

/* parameter table descriptor */
static struct KmParamTbl g_kmParamTbl = {
	0x1002,
	0x1020,
	2,
	0,
	{
		{ 0, 0x5771ba80ULL, 0xf4, 0 },
		{ 0, 0x5951ee3eULL, 0xf4, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
	},
	{ g_kmDefaultParamAFirst, g_kmDefaultParamASecond, NULL, NULL },
	0x48,
	g_kmDefaultParamB,
};

static struct KmKeyDesc g_kmKeys[2] = {
	{ g_kmKey0Alt, g_kmKey0Primary, g_kmKey0Iv },
	{ g_kmKey1Alt, g_kmKey1Primary, g_kmKey1Iv },
};

/* file handle, open/init flags, timing config and key selection */
static SceUID g_kmParamFileFd = -1;
static unsigned char g_kmOpened = 0;
static unsigned char g_kmTimeConfigDirty = 1;
static struct KmTimeConfig g_kmTimeConfig;
static unsigned char g_kmStatDirty = 0;
static unsigned char g_kmKeyNeedsDecrypt[2] = { 1, 1 };
static unsigned int g_kmKeySlot = 2;

/* VTRM encryption keys and IV */
static unsigned char g_vtrmCurrentKey[32] = {
	0x37, 0x52, 0x48, 0x32, 0x61, 0x34, 0x76, 0x6f, 0x57, 0x53, 0x31, 0x77, 0x51, 0x54, 0x30, 0x75,
	0x37, 0x52, 0x48, 0x32, 0x61, 0x34, 0x76, 0x6f, 0x57, 0x53, 0x31, 0x77, 0x51, 0x54, 0x30, 0x75,
};

static unsigned char g_vtrmKey0[32] = {
	0x25, 0x5e, 0xa3, 0x63, 0xfc, 0x08, 0x1e, 0xb9, 0x63, 0x3a, 0x77, 0xfb, 0x06, 0x7d, 0x98, 0x28,
	0x70, 0x7f, 0x28, 0xbe, 0x2c, 0x3c, 0x9c, 0x46, 0x1c, 0x23, 0x1a, 0xa4, 0x9d, 0xf6, 0x43, 0xe1,
};

static unsigned char g_vtrmKey1[32] = {
	0x92, 0xfa, 0x00, 0x2f, 0x06, 0x37, 0x06, 0xd9, 0xd6, 0x9e, 0xfd, 0xf4, 0xe1, 0xea, 0x69, 0x93,
	0x30, 0xd1, 0xff, 0xf1, 0xd7, 0x5f, 0xbf, 0xf8, 0x9c, 0x81, 0x55, 0xe3, 0xbe, 0x83, 0x89, 0x2c,
};

static unsigned char g_vtrmIv[16] = {
	0x72, 0x6b, 0xfd, 0x7f, 0xe6, 0xe3, 0xc3, 0x7e, 0x0d, 0xfc, 0x10, 0xce, 0x7f, 0xa2, 0x0f, 0x79,
};

/* SRM scratch buffer and active key-store state */
static unsigned char g_kmSrmBuffer[KM_SRM_LEN];
static struct KmStat g_kmStat;

/* forward declarations for internal helpers */
int swKeyStoreLoadVtrm(void);
int swKeyStoreReadExtra(int keyIndex, void *buffer, unsigned int size, unsigned int *outSize);
int kmVtrmDestroyAll(void);
int kmVtrmReadStat(struct KmStat *stat);
int kmVtrmWriteStat(const struct KmStat *stat);
int kmVtrmReadSrm(unsigned char *srm, unsigned short len);
int kmVtrmWriteSrm(const unsigned char *srm, unsigned short len);
int kmVtrmReadTimeConfig(struct KmTimeConfig *cfg);
int kmVtrmWriteTimeConfig(const struct KmTimeConfig *cfg);
int kmVtrmGetBlockPath(SceUInt64 blockId, char *path);
int kmVtrmCheckBlock(SceUInt64 blockId, char *path, int *exists);
int kmRtcGetTime(SceUInt64 *outTime);
int kmGetStatus(struct KmStatusInfo *info);

/* open the software key store and load VTRM state */
int swKeyStoreOpen(void **outHandle)
{
	int status;

	if (outHandle == NULL)
		return KM_ERR_INVALID_ARG;
	if (g_kmOpened != 0) {
		*outHandle = &g_kmStat;
		return 0;
	}
	if (swKeyStoreLoadVtrm() < 0)
		return KM_ERR_VTRM;
	status = kmVtrmReadStat(&g_kmStat);
	if (status != 0)
		return status;
	*outHandle = &g_kmStat;
	g_kmOpened = 1;
	return 0;
}

/* check whether the 3-byte certificate remainder is populated */
int kmCertIsInstalled(void)
{
	unsigned char installed = 0;

	if (g_kmStat.certRest[0] != 0 && g_kmStat.certRest[1] != 0)
		return g_kmStat.certRest[2] != 0;
	return installed;
}

/* set the parameter state nibble to 1 */
unsigned int stat_init_stat(struct KmStat *sp)
{
	if (sp == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(99) : Assertion Failed (sp != 0)",
			"stat_init_stat");
	sp->flags = (sp->flags & 0xFFFFFFF0u) | 1u;
	return sp->flags;
}

/* install the 3-byte certificate remainder and persist VTRM state */
int swKeyStoreWriteCert(void *handle, const unsigned char *certRest, int size)
{
	int status;

	if (handle != &g_kmStat)
		return KM_ERR_INVALID_ARG;
	if (size == 0)
		return 0;
	if (certRest == NULL || size != 3)
		return KM_ERR_INVALID_ARG;
	if (g_kmOpened == 0)
		return KM_ERR_NOT_OPEN;
	if (kmCertIsInstalled() == 0) {
		stat_init_stat(&g_kmStat);
		g_kmStat.certRest[0] = certRest[0];
		g_kmStat.certRest[1] = certRest[1];
		g_kmStat.certRest[2] = certRest[2];
	}
	status = kmVtrmWriteStat(&g_kmStat);
	if (status != 0)
		return status;
	return 0;
}

/* open ur0:dtcp/param_tbl.enc */
int kmOpenParamFile(int flags, SceMode mode)
{
	int status = KM_ERR_IO;

	if (g_kmParamFileFd < 0) {
		g_kmParamFileFd = sceIoOpen(KM_PARAM_TBL_PATH, flags, mode);
		if (g_kmParamFileFd >= 0)
			return 0;
		printf("open error %s, ret=%x\n", KM_PARAM_TBL_PATH, g_kmParamFileFd);
	} else {
		printf("double open error %s\n", KM_PARAM_TBL_PATH);
	}
	return status;
}

/* close ur0:dtcp/param_tbl.enc */
int kmCloseParamFile(void)
{
	if (sceIoClose(g_kmParamFileFd) != 0)
		return KM_ERR_IO;
	g_kmParamFileFd = -1;
	return 0;
}

/* write ur0:dtcp/param_tbl.enc */
int kmWriteParamFile(const void *data, SceSize size)
{
	int status;
	SceSSize written;

	status = kmOpenParamFile(SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (status != 0)
		return status;
	written = sceIoWrite(g_kmParamFileFd, data, size);
	if (written != (SceSSize)size) {
		printf("write error %ld\n", (long)written);
		status = KM_ERR_IO;
	}
	kmCloseParamFile();
	return status;
}

/* read ur0:dtcp/param_tbl.enc into a newly allocated buffer */
int kmReadParamFile(void **outBuf, size_t *outSize)
{
	int status;
	int ret;
	void *buf;
	SceSSize bytesRead;
	SceIoStat stat;

	status = kmOpenParamFile(SCE_O_RDONLY, 4);
	if (status != 0)
		return status;

	status = KM_ERR_IO;
	ret = sceIoGetstatByFd(g_kmParamFileFd, &stat);
	if (ret >= 0) {
		if (stat.st_size != 0) {
			buf = malloc((size_t)stat.st_size);
			if (buf != NULL) {
				bytesRead = sceIoRead(g_kmParamFileFd, buf, (SceSize)stat.st_size);
				if (bytesRead == (SceSSize)stat.st_size) {
					*outBuf = buf;
					*outSize = (size_t)stat.st_size;
					status = 0;
				} else {
					printf("write error %ld\n", (long)bytesRead);
				}
			} else {
				printf("malloc error\n");
			}
		} else {
			printf("empty file\n");
		}
	} else {
		printf("getstat error %d\n", ret);
	}
	kmCloseParamFile();
	return status;
}

/* schedule AES-CBC key and copy IV for param_tbl decryption */
int kmAesCbcScheduleKey(struct KmAesCbcContext *ctx, SceUInt32 blockBits, SceUInt32 keyBits,
			const void *key, const unsigned char *iv)
{
	unsigned int i;
	int ret;

	ret = sceAesScheduleKey(&ctx->aes, blockBits, keyBits, key);
	if (ret < 0)
		return ret;
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		ctx->iv[i] = iv[i];
	return 0;
}

/* decrypt one 16-byte AES-128-CBC block for param_tbl decryption */
int kmAesCbcDecrypt128(struct KmAesCbcContext *ctx, const unsigned char *inBlock,
		       unsigned char *outBlock)
{
	unsigned char decrypted[16];
	unsigned char nextIv[16];
	int i;

	sceAesDecrypt(&ctx->aes, inBlock, decrypted);
	memcpy(nextIv, inBlock, 16);
	for (i = 0; i < 16; ++i)
		outBlock[i] = decrypted[i] ^ ctx->iv[i];
	memcpy(ctx->iv, nextIv, 16);
	return 0;
}

/* read, verify SHA-512, derive MD5 key/IV and AES-128-CBC decrypt param_tbl.enc */
int kmDecryptParamFile(void **outPlain, size_t *outPlainSize)
{
	int ret;
	int i;
	int status;
	unsigned char *plain;
	const unsigned char *salt;
	const unsigned char *srcBlock;
	unsigned char *dstBlock;
	unsigned char *raw;
	size_t rawSize;
	unsigned char passKey[16];
	unsigned char aesKey[16];
	unsigned char aesIv[16];
	struct KmAesCbcContext aesCtx;
	unsigned char digest[64];

	status = swKeyStoreReadExtra(1, passKey, 16, (unsigned int *)&rawSize);
	if (status != 0)
		return status;

	status = kmReadParamFile((void **)&raw, &rawSize);
	if (status != 0)
		return KM_ERR_NO_PARAM_TBL;

	if (rawSize < 0x50) {
		fprintf(stderr, "invalid file size=%d\n", (int)rawSize);
		free(raw);
		return status;
	}

	status = KM_ERR_DIGEST;
	rawSize -= 64;
	ret = sceSha512Digest(raw, (SceUInt32)rawSize, digest);
	if (ret < 0) {
		fprintf(stderr, "sceSha512Digest failed err=%d\n", ret);
		free(raw);
		return status;
	}

	if (memcmp(raw + rawSize, digest, 64) != 0) {
		fprintf(stderr, "digest error\n");
		free(raw);
		return status;
	}

	status = KM_ERR_DECRYPT;
	if (strncmp("Salted__", (const char *)raw, 8) != 0) {
		printf("could not find prefix for salt\n");
		free(raw);
		return status;
	}

	salt = raw + 8;
	rawSize -= 16;
	plain = (unsigned char *)malloc(rawSize);
	if (plain == NULL) {
		printf("malloc error\n");
		free(raw);
		return status;
	}

	memcpy(plain + 16, passKey, 16);
	memcpy(plain + 32, salt, 8);
	ret = sceMd5Digest(plain + 16, 24, aesKey);
	if (ret >= 0) {
		memcpy(plain, aesKey, 16);
		printf("key = ");
		for (i = 0; i < 16; ++i)
			printf("%02x", aesKey[i]);
		printf("\n");
		ret = sceMd5Digest(plain, 40, aesIv);
		if (ret >= 0) {
			printf("iv = ");
			for (i = 0; i < 16; ++i)
				printf("%02x", aesIv[i]);
			printf("\n");
			ret = kmAesCbcScheduleKey(&aesCtx, 128, 128, aesKey, aesIv);
			if (ret != 0) {
				fprintf(stderr, "sceAesCbcScheduleKey failed err=%d\n", ret);
			} else {
				unsigned int blocks = (unsigned int)(rawSize >> 4);
				int failed = 0;

				srcBlock = raw + 16;
				dstBlock = plain;
				for (i = 0; (unsigned int)i < blocks; ++i) {
					ret = kmAesCbcDecrypt128(&aesCtx, srcBlock, dstBlock);
					if (ret != 0) {
						fprintf(stderr, "sceAesCbcDecrypt128 failed err=%d\n", ret);
						failed = 1;
						break;
					}
					srcBlock += 16;
					dstBlock += 16;
				}
				if (!failed) {
					free(raw);
					*outPlain = plain;
					*outPlainSize = rawSize;
					return 0;
				}
			}
		}
	}

	free(plain);
	free(raw);
	return status;
}

/* big-endian 16-bit byte swap */
unsigned short kmSwap16(unsigned short val)
{
	return (unsigned short)(((val & 0x00ffu) << 8) | ((val & 0xff00u) >> 8));
}

/* big-endian 32-bit byte swap */
unsigned int kmSwap32(unsigned int val)
{
	return ((val & 0x000000ffu) << 24) | ((val & 0x0000ff00u) << 8) |
	       ((val & 0x00ff0000u) >> 8) | ((val & 0xff000000u) >> 24);
}

/* parse param_tbl.enc metadata and update actmgr key schedule */
int km_load_param_info(void)
{
	unsigned int offset = 0;
	int i;
	int status;
	unsigned short ksId;
	unsigned short tblId;
	unsigned short nparam;
	unsigned short keyIdRaw;
	unsigned char *plain = NULL;
	size_t plainSize;
	unsigned int genTimeRaw;
	struct KmActMgrInfo actInfo[KM_PARAM_TBL_MAX];

	printf("km_load_param_info\n");
	status = kmDecryptParamFile((void **)&plain, &plainSize);
	if (status != 0)
		return status;

	memcpy(&ksId, plain, 2);
	ksId = kmSwap16(ksId);
	if (ksId == g_kmParamTbl.ksId) {
		memcpy(&tblId, plain + 4, 2);
		tblId = kmSwap16(tblId);
		offset = 6;
		memcpy(&nparam, plain + offset, 2);
		nparam = kmSwap16(nparam);
		if (nparam <= KM_PARAM_TBL_MAX) {
			offset += 2;
			printf("%s: tbl_id=%04x nparam=%d\n", KM_PARAM_TBL_PATH, tblId, nparam);
			for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
				memcpy(&keyIdRaw, plain + offset, 2);
				actInfo[i].keyId = kmSwap16(keyIdRaw);
				printf("%s: key_id[%d]=%04x\n", KM_PARAM_TBL_PATH, i, actInfo[i].keyId);
				offset += 2;
			}
			if ((g_kmParamTbl.nparam & 1u) != 0)
				offset += 2;
			for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
				offset += 4;
				memcpy(&genTimeRaw, plain + offset, 4);
				offset += 8;
				actInfo[i].genTime = (SceUInt64)kmSwap32(genTimeRaw);
				printf("%s: info[%d] gen_time=%ld\n", KM_PARAM_TBL_PATH, i,
				       (long)actInfo[i].genTime);
			}
			if (dtcpip_actmgr_update_info(actInfo, g_kmParamTbl.nparam) < 0)
				status = KM_ERR_BAD_PARAM_TBL;
		} else {
			status = KM_ERR_BAD_PARAM_TBL;
		}
	} else {
		printf("%s: ks_id %04x != %04x\n", KM_PARAM_TBL_PATH, ksId, g_kmParamTbl.ksId);
		status = KM_ERR_KS_ID_MISMATCH;
	}
	free(plain);
	return status;
}

/* store downloaded param_tbl.enc and refresh actmgr when not yet opened */
int swKeyStoreImportParamTbl(const void *data, SceSize size)
{
	char *cursor;
	int status;
	int ret;
	char dir[1024];

	printf("km_store_param_tbl size=%x\n", (unsigned int)size);
	if (data == NULL || size == 0 || size > 0x214)
		return KM_ERR_INVALID_ARG;

	strncpy(dir, KM_PARAM_TBL_PATH, 0x17);
	cursor = dir;
	for (;;) {
		++cursor;
		while (*cursor != '/' && *cursor != '\0')
			++cursor;
		if (*cursor == '\0')
			break;
		*cursor = '\0';
		ret = sceIoMkdir(dir, 6);
		if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
			printf("mkdir error %s, err=%x\n", dir, ret);
			return KM_ERR_IO;
		}
		*cursor = '/';
	}
	status = kmWriteParamFile(data, size);
	if (status == 0 && g_kmOpened == 0)
		status = km_load_param_info();
	return status;
}

/* read a big-endian 16-bit value at offset in a parameter buffer */
unsigned short kmReadBe16(const unsigned char *buf, int offset)
{
	unsigned short raw;

	memcpy(&raw, buf + offset, 2);
	return kmSwap16(raw);
}

/* load ur0:dtcp/param_tbl.enc into the active parameter table */
int kmLoadParamTbl(void)
{
	unsigned int offset = 0;
	int status;
	int i;
	int j;
	unsigned char *paramPtr;
	struct KmParamInfo *infoPtr;
	unsigned short ksId;
	unsigned short tblId;
	unsigned char *plain = NULL;
	size_t plainSize;
	unsigned int fields[3];

	status = kmDecryptParamFile((void **)&plain, &plainSize);
	if (status != 0)
		return status;

	memcpy(&ksId, plain, 2);
	ksId = kmSwap16(ksId);
	if (ksId == g_kmParamTbl.ksId) {
		offset = 4;
		memcpy(&tblId, plain + offset, 2);
		tblId = kmSwap16(tblId);
		if (tblId == g_kmParamTbl.tblId) {
			printf("%s: tbl_id=%04x has already loaded\n", KM_PARAM_TBL_PATH,
			       g_kmParamTbl.tblId);
		} else {
			offset += 2;
			memcpy(&g_kmParamTbl.nparam, plain + offset, 2);
			g_kmParamTbl.nparam = kmSwap16(g_kmParamTbl.nparam);
			offset += 2;
			printf("%s: tbl_id=%04x nparam=%d\n", KM_PARAM_TBL_PATH, tblId,
			       g_kmParamTbl.nparam);
			for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
				paramPtr = g_kmParamTbl.paramA[i];
				memcpy(paramPtr + 73, plain + offset, 2);
				printf("%s: key_id[%d]=%04x\n", KM_PARAM_TBL_PATH, i,
				       kmReadBe16(paramPtr, 73));
				offset += 2;
			}
			if ((g_kmParamTbl.nparam & 1u) != 0)
				offset += 2;
			for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
				infoPtr = &g_kmParamTbl.info[i];
				for (j = 0; j < 3; ++j) {
					memcpy(&fields[j], plain + offset, 4);
					offset += 4;
					fields[j] = kmSwap32(fields[j]);
				}
				printf("%s: info[%d] gen_time=%u\n", KM_PARAM_TBL_PATH, i, fields[0]);
				printf("%s: info[%d] act_time=%u\n", KM_PARAM_TBL_PATH, i, fields[1]);
				printf("%s: info[%d] size=%u\n", KM_PARAM_TBL_PATH, i, fields[2]);
				infoPtr->genTime = (SceUInt64)fields[0];
				infoPtr->actTime = (SceUInt64)fields[1];
				infoPtr->size = fields[2];
			}
			for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
				paramPtr = g_kmParamTbl.paramA[i];
				memcpy(paramPtr + 40, plain + offset, 32);
				offset += 32;
				memcpy(paramPtr + 100, plain + offset, 88);
				offset += 88;
			}
			g_kmParamTbl.tblId = tblId;
		}
	} else {
		printf("%s: ks_id %04x != %04x\n", KM_PARAM_TBL_PATH, ksId, g_kmParamTbl.ksId);
		status = KM_ERR_KS_ID_MISMATCH;
	}
	free(plain);
	return status;
}

/* check whether a newer parameter table ID is loaded */
int kmHasNewParamTbl(void)
{
	return g_kmStat.tblId < g_kmParamTbl.tblId;
}

/* synchronize stat with the currently loaded parameter table */
unsigned int stat_sync_param_tbl(struct KmStat *sp)
{
	int i;

	if (sp == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(107) : Assertion Failed (sp != 0)",
			"stat_sync_param_tbl");
	if ((sp->flags & 0xFu) != 1u) {
		for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
			if (sp->keyId == kmReadBe16(g_kmParamTbl.paramA[i], 73)) {
				sp->tblId = g_kmParamTbl.tblId;
				sp->tblOff = (unsigned short)i;
				return (unsigned int)i;
			}
		}
	}
	sp->tblId = g_kmParamTbl.tblId;
	sp->tblOff = 0;
	sp->keyId = kmReadBe16(g_kmParamTbl.paramA[0], 73);
	sp->flags = (sp->flags & 0xFFFFFFF0u) | 2u;
	return sp->flags;
}

/* compute difference between two non-zero timestamps */
SceInt64 km_difftime(SceInt64 endTime, SceInt64 startTime)
{
	if (endTime == 0)
		_Assert(LIBSWKEYSTORE_SOURCE "(87) : Assertion Failed (t1 != 0)",
			"km_difftime");
	if (startTime == 0)
		_Assert(LIBSWKEYSTORE_SOURCE "(88) : Assertion Failed (t0 != 0)",
			"km_difftime");
	return endTime - startTime;
}

/* write bytes into a parameter buffer at offset */
void *kmParamWriteBytes(unsigned char *param, const void *src, int offset, size_t len)
{
	return memcpy(param + offset, src, len);
}

/* write a single byte into a parameter buffer at offset */
void *kmParamWriteByte(unsigned char *param, int offset, unsigned char value)
{
	return memset(param + offset, value, 1);
}

/* write the 64-bit start time into paramA at offset 76 */
void kmParamWriteStartTime(unsigned char *param, SceUInt64 startTime)
{
	memcpy(param + 76, &startTime, sizeof(startTime));
}

/* assemble the current paramA buffer from stat and param table */
void *ld_cur_param(struct KmStat *sp, unsigned char *param, unsigned int *len)
{
	if (sp == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(221) : Assertion Failed (sp != 0)",
			"ld_cur_param");
	if (param == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(222) : Assertion Failed (param != 0)",
			"ld_cur_param");
	if (len == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(223) : Assertion Failed (len != 0)",
			"ld_cur_param");

	kmParamWriteBytes(param, g_kmParamTbl.paramA[sp->tblOff], 0,
			  g_kmParamTbl.info[sp->tblOff].size);
	*len = g_kmParamTbl.info[sp->tblOff].size;
	if ((sp->flags & 0x20u) != 0) {
		kmParamWriteByte(param, 75, 1);
		kmParamWriteStartTime(param, sp->startTime);
		kmParamWriteBytes(param, sp->stHmac, 84, 12);
	} else {
		kmParamWriteByte(param, 75, 0);
	}
	return kmParamWriteBytes(param, sp->certRest, 100, 3);
}

/* load paramA, advancing key period if expired and reporting renewal flag */
int swKeyStoreReadBlock(void *handle, unsigned char *param, unsigned int size,
			unsigned int *outSize)
{
	int status;
	unsigned int nextOff;
	unsigned int lastOff;
	SceUInt64 now;
	unsigned int paramStat;

	if (handle != &g_kmStat || param == NULL || outSize == NULL)
		return KM_ERR_INVALID_ARG;
	if (size < g_kmParamTbl.info[g_kmStat.tblOff].size)
		return KM_ERR_BUFFER_SMALL;
	if (g_kmOpened == 0)
		return KM_ERR_NOT_OPEN;

	status = kmLoadParamTbl();
	if (status != 0 && status != KM_ERR_NO_PARAM_TBL)
		return status;

	if (g_kmStat.tblId > g_kmParamTbl.tblId) {
		fprintf(stderr, "roll back %d > %d\n", g_kmStat.tblId, g_kmParamTbl.tblId);
		return KM_ERR_ROLLBACK;
	}
	if (kmCertIsInstalled() == 0) {
		g_kmStatDirty = 1;
		return KM_ERR_NOT_ACTIVATED;
	}
	if (g_kmTimeConfigDirty != 0) {
		g_kmTimeConfigDirty = 0;
		status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
		if (status != 0)
			return status;
	}
	if (kmHasNewParamTbl()) {
		stat_sync_param_tbl(&g_kmStat);
		g_kmStatDirty = 1;
	}
	status = kmRtcGetTime(&now);
	if (status != 0)
		return status;

	paramStat = g_kmStat.flags & 0xFu;
	if (paramStat <= 2u) {
		if (paramStat > 1u) {
			SceUInt64 curAct = g_kmParamTbl.info[g_kmStat.tblOff].actTime;

			if (now < curAct)
				return KM_ERR_CLOCK_BEFORE;
			if ((SceUInt64)km_difftime((SceInt64)now, (SceInt64)curAct) <
			    g_kmTimeConfig.periodTime) {
				g_kmStat.flags = (g_kmStat.flags & 0xFFFFFFF0u) | 3u;
				g_kmStat.flags &= ~0x20u;
				g_kmStat.startTime = 0;
				g_kmStat.flags &= ~0x40u;
				ld_cur_param(&g_kmStat, param, outSize);
				g_kmStatDirty = 0;
				return ((SceUInt64)km_difftime(
						(SceInt64)now,
						(SceInt64)g_kmParamTbl.info[g_kmParamTbl.nparam - 1].actTime) >=
					KM_RENEW_THRESHOLD_SEC &&
					g_kmStat.tblOff + 1 == g_kmParamTbl.nparam) ? 1 : 0;
			}
		}
	} else if (paramStat <= 3u &&
		   (SceUInt64)km_difftime((SceInt64)now, (SceInt64)g_kmStat.startTime) <
			   g_kmTimeConfig.validTime) {
		SceUInt64 midTime;

		ld_cur_param(&g_kmStat, param, outSize);
		if (g_kmStatDirty != 0) {
			status = kmVtrmWriteStat(&g_kmStat);
			if (status != 0)
				return status;
			g_kmStatDirty = 0;
		}
		midTime = (g_kmStat.startTime +
			   g_kmParamTbl.info[g_kmParamTbl.nparam - 1].actTime) >> 1;
		return ((SceUInt64)km_difftime((SceInt64)now, (SceInt64)midTime) >=
			KM_RENEW_THRESHOLD_SEC &&
			g_kmStat.tblOff + 1 == g_kmParamTbl.nparam) ? 1 : 0;
	}

	nextOff = (unsigned int)g_kmStat.tblOff + 1u;
	while (nextOff < (unsigned int)g_kmParamTbl.nparam) {
		SceUInt64 nextAct = g_kmParamTbl.info[nextOff].actTime;

		if (now < nextAct)
			return KM_ERR_CLOCK_BEFORE;
		if ((SceUInt64)km_difftime((SceInt64)now, (SceInt64)nextAct) <
		    g_kmTimeConfig.periodTime) {
			g_kmStat.flags = (g_kmStat.flags & 0xFFFFFFF0u) | 3u;
			g_kmStat.flags &= ~0x20u;
			g_kmStat.startTime = 0;
			g_kmStat.flags &= ~0x40u;
			g_kmStat.tblOff = (unsigned short)nextOff;
			g_kmStat.keyId = kmReadBe16(g_kmParamTbl.paramA[nextOff], 73);
			ld_cur_param(&g_kmStat, param, outSize);
			g_kmStatDirty = 0;
			return ((SceUInt64)km_difftime(
					(SceInt64)now,
					(SceInt64)g_kmParamTbl.info[g_kmParamTbl.nparam - 1].actTime) >=
				KM_RENEW_THRESHOLD_SEC &&
				g_kmStat.tblOff + 1 == g_kmParamTbl.nparam) ? 1 : 0;
		}
		++nextOff;
	}

	g_kmStat.flags = (g_kmStat.flags & 0xFFFFFFF0u) | 4u;
	lastOff = nextOff - 1u;
	if ((unsigned int)g_kmStat.tblOff != lastOff) {
		g_kmStat.tblOff = (unsigned short)lastOff;
		g_kmStat.keyId = kmReadBe16(g_kmParamTbl.paramA[lastOff], 73);
	}
	status = kmVtrmWriteStat(&g_kmStat);
	if (status != 0)
		return status;
	g_kmStatDirty = 0;
	return KM_ERR_EXPIRED;
}

/* read a single byte from a parameter buffer */
unsigned char kmParamReadByte(const unsigned char *buf, int offset)
{
	return buf[offset];
}

/* read the 64-bit start time from paramA at offset 76 */
void kmParamReadStartTime(SceUInt64 *outTime, const unsigned char *param)
{
	memcpy(outTime, param + 76, sizeof(*outTime));
}

/* copy bytes out of a parameter buffer */
void *kmParamReadBytes(void *dst, const unsigned char *param, int offset, size_t len)
{
	return memcpy(dst, param + offset, len);
}

/* store updated paramA start time and HMAC back into VTRM */
int swKeyStoreWriteBlock(void *handle, const unsigned char *param, unsigned int size)
{
	if (handle != &g_kmStat || param == NULL)
		return KM_ERR_INVALID_ARG;
	if (size > g_kmParamTbl.info[g_kmStat.tblOff].size)
		return KM_ERR_BUFFER_LARGE;
	if (g_kmOpened == 0)
		return KM_ERR_NOT_OPEN;
	if (kmParamReadByte(param, 75) != 1)
		return 0;
	if (g_kmStat.keyId != kmReadBe16(param, 73))
		return KM_ERR_INVALID_STATE;
	if ((g_kmStat.flags & 0xFu) != 3u)
		return KM_ERR_INVALID_STATE;
	g_kmStat.flags |= 0x20u;
	kmParamReadStartTime(&g_kmStat.startTime, param);
	kmParamReadBytes(g_kmStat.stHmac, param, 84, 12);
	return kmVtrmWriteStat(&g_kmStat);
}

/* write a 16-bit value into a parameter buffer at offset */
void *kmParamWriteU16(unsigned char *param, const unsigned short *value, int offset)
{
	return memcpy(param + offset, value, 2);
}

/* load paramB together with cached Kx HMAC and SRM data */
int swKeyStoreReadRecord(void *handle, unsigned char *param, unsigned int size,
			 unsigned int *outSize)
{
	if (handle != &g_kmStat || param == NULL || outSize == NULL)
		return KM_ERR_INVALID_ARG;
	if (size < g_kmParamTbl.paramBSize + (unsigned int)g_kmStat.srmLen)
		return KM_ERR_BUFFER_SMALL;
	if (g_kmOpened == 0)
		return KM_ERR_NOT_OPEN;

	kmParamWriteBytes(param, g_kmParamTbl.paramB, 0, g_kmParamTbl.paramBSize);
	*outSize = g_kmParamTbl.paramBSize;
	if ((g_kmStat.flags & 0x40u) != 0) {
		kmParamWriteByte(param, 54, 1);
		kmParamWriteBytes(param, &g_kmStat.kxLabel, 55, 1);
		kmParamWriteBytes(param, g_kmStat.kxHmac, 56, 12);
	}
	if ((g_kmStat.flags & 0x10u) != 0) {
		if (g_kmStat.srmLen == 0)
			_Assert(LIBSWKEYSTORE_SOURCE "(789) : Assertion Failed (stat.srm_len != 0)",
				"km_load_paramB");
		if (g_kmStat.srmLen > KM_SRM_LEN)
			_Assert(LIBSWKEYSTORE_SOURCE "(790) : Assertion Failed (stat.srm_len <= KM_SRM_LEN)",
				"km_load_paramB");
		memset(g_kmSrmBuffer, 0, KM_SRM_LEN);
		kmVtrmReadSrm(g_kmSrmBuffer, g_kmStat.srmLen);
		kmParamWriteBytes(param, g_kmSrmBuffer, 72, g_kmStat.srmLen);
		kmParamWriteU16(param, &g_kmStat.srmLen, 70);
		*outSize += (unsigned int)g_kmStat.srmLen;
	}
	return 0;
}

/* read a host-endian 16-bit value from a parameter buffer */
unsigned short kmParamReadU16(const unsigned char *param, int offset)
{
	unsigned short value;

	memcpy(&value, param + offset, 2);
	return value;
}

/* store paramB Kx label/HMAC and/or SRM payload into VTRM */
int swKeyStoreWriteRecord(void *handle, const unsigned char *param, unsigned int size,
			  unsigned char flags)
{
	unsigned char dirty = 0;

	if (handle != &g_kmStat || param == NULL)
		return KM_ERR_INVALID_ARG;
	if ((flags & 1u) == 0 && (flags & 2u) == 0)
		return KM_ERR_INVALID_ARG;
	if (size > g_kmParamTbl.paramBSize + KM_SRM_LEN)
		return KM_ERR_BUFFER_LARGE;
	if (g_kmOpened == 0)
		return KM_ERR_NOT_OPEN;

	if ((flags & 1u) != 0) {
		if (kmParamReadByte(param, 54) != 1)
			return 0;
		g_kmStat.flags |= 0x40u;
		kmParamReadBytes(&g_kmStat.kxLabel, param, 55, 1);
		kmParamReadBytes(g_kmStat.kxHmac, param, 56, 12);
		dirty = 1;
	}
	if ((flags & 2u) != 0 && kmParamReadU16(param, 70) != 0) {
		if ((g_kmStat.flags & 0x10u) != 0) {
			if (g_kmStat.srmLen != kmParamReadU16(param, 70)) {
				g_kmStat.srmLen = kmParamReadU16(param, 70);
				dirty = 1;
			}
		} else {
			g_kmStat.flags |= 0x10u;
			g_kmStat.srmLen = kmParamReadU16(param, 70);
			dirty = 1;
		}
		kmVtrmWriteSrm(param + 72, g_kmStat.srmLen);
	}
	if (dirty != 0)
		return kmVtrmWriteStat(&g_kmStat);
	return 0;
}

/* close the key store handle */
int swKeyStoreClose(void *handle)
{
	if (handle != &g_kmStat)
		return KM_ERR_INVALID_ARG;
	g_kmOpened = 0;
	return 0;
}

/* decrypt and extract a 16-byte key from SBL portability-wrapped storage */
int swKeyStoreReadExtra(int keyIndex, void *buffer, unsigned int size, unsigned int *outSize)
{
	int i;
	struct KmPortabilityBlock inBlock;
	struct KmPortabilityBlock outBlock;
	struct KmPortabilityBlock altInBlock;
	struct KmPortabilityBlock altOutBlock;
	unsigned int slot;

	if (buffer == NULL || outSize == NULL)
		return KM_ERR_INVALID_ARG;
	if (size < 16u)
		return KM_ERR_BUFFER_SMALL;

	if (g_kmKeyNeedsDecrypt[keyIndex] != 0) {
		g_kmKeyNeedsDecrypt[keyIndex] = 0;
		for (i = 0; i < 2; ++i) {
			memcpy(inBlock.data, g_kmKeys[keyIndex].mainKey + 32 * i, 32);
			inBlock.size = 32;
			if (vshSblSsDecryptWithPortability(8, g_kmKeys[keyIndex].iv,
							   &inBlock, &outBlock) != 0)
				return KM_ERR_VTRM;
			memcpy(g_kmKeys[keyIndex].mainKey + 32 * i, outBlock.data, 32);
		}
		if (kmParamReadByte(g_kmKeys[keyIndex].mainKey, 0) == '.') {
			g_kmKeySlot = 0;
		} else {
			for (i = 0; i < 2; ++i) {
				memcpy(altInBlock.data, g_kmKeys[keyIndex].fallbackKey + 32 * i, 32);
				altInBlock.size = 32;
				if (vshSblSsDecryptWithPortability(8, g_kmKeys[keyIndex].iv,
								   &altInBlock, &altOutBlock) != 0)
					return KM_ERR_VTRM;
				memcpy(g_kmKeys[keyIndex].fallbackKey + 32 * i, altOutBlock.data, 32);
			}
			if (kmParamReadByte(g_kmKeys[keyIndex].fallbackKey, 0) == '.')
				g_kmKeySlot = 1;
			else
				g_kmKeySlot = 2;
		}
	}

	slot = g_kmKeySlot;
	if (slot > 1u) {
		if (slot <= 2u) {
			*outSize = 0;
			return KM_ERR_IO;
		}
	} else if (slot != 0) {
		kmParamWriteBytes((unsigned char *)buffer, g_kmKeys[keyIndex].fallbackKey + 41, 0, 8);
		kmParamWriteBytes((unsigned char *)buffer, g_kmKeys[keyIndex].fallbackKey + 19, 8, 8);
	} else {
		kmParamWriteBytes((unsigned char *)buffer, g_kmKeys[keyIndex].mainKey + 41, 0, 8);
		kmParamWriteBytes((unsigned char *)buffer, g_kmKeys[keyIndex].mainKey + 19, 8, 8);
	}
	*outSize = 16;
	return 0;
}

/* read current secure RTC time plus the configured VTRM time offset */
int kmRtcGetTime(SceUInt64 *outTime)
{
	int ret;
	int status;
	SceRtcTick tick;
	SceDateTime dateTime;

	if (outTime == NULL)
		_Assert(LIBSWKEYSTORE_SOURCE "(1007) : Assertion Failed (p != 0)",
			"km_rtc_get_time");

	ret = vshRtcGetCurrentSecureTick(&tick);
	if (ret < 0 ||
	    (ret = sceRtcSetTick(&dateTime, &tick)) < 0 ||
	    (ret = sceRtcGetTime64_t(&dateTime, outTime)) < 0) {
		printf("    cannot get secure clock\n");
		if (ret == SCE_RTC_ERROR_INVALID_TICK)
			return KM_ERR_NO_SECURE_CLOCK;
		return KM_ERR_IO;
	}
	if (g_kmTimeConfigDirty != 0) {
		g_kmTimeConfigDirty = 0;
		status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
		if (status != 0)
			return status;
	}
	*outTime += g_kmTimeConfig.timeOffset;
	return 0;
}

/* remove all VTRM blocks to uninstall certificate state */
int swKeyStoreResetAll(void)
{
	int ret;

	ret = kmVtrmDestroyAll();
	if (ret >= 0) {
		printf("Uninstalled cert\n");
		return 0;
	}
	printf("cellSsVtrmFree2() failed %#x\n", ret);
	return KM_ERR_VTRM;
}

/* query the currently active key ID from VTRM status */
int swKeyStoreGetPort(unsigned short *outKeyId)
{
	int status;
	struct KmStatusInfo info;

	status = kmGetStatus(&info);
	if (status != 0)
		return status;
	*outKeyId = info.keyId;
	return 0;
}

/* read full key-store status from VTRM and active param table */
int kmGetStatus(struct KmStatusInfo *info)
{
	int status;
	int i;
	struct KmStat stat;
	struct KmTimeConfig cfg;

	memset(&stat, 0, sizeof(stat));
	memset(&cfg, 0, sizeof(cfg));
	if (kmVtrmReadStat(&stat) != 0)
		return KM_ERR_VTRM;
	status = kmVtrmReadTimeConfig(&cfg);
	if (status != 0)
		return status;

	info->validTime = cfg.validTime;
	kmLoadParamTbl();
	info->tblId = stat.tblId;
	info->tblOff = stat.tblOff;
	info->keyId = stat.keyId;
	info->paramStat = stat.flags & 0xFu;
	if ((stat.flags & 0xFu) == 3u && (stat.flags & 0x20u) != 0)
		info->startTime = stat.startTime;
	info->curTblId = g_kmParamTbl.tblId;
	info->nparam = g_kmParamTbl.nparam;
	for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
		info->entries[i].keyId = kmReadBe16(g_kmParamTbl.paramA[i], 73);
		info->entries[i].actTime = g_kmParamTbl.info[i].actTime;
	}
	return 0;
}

/* remove ur0:dtcp/param_tbl.enc and its parent directory */
int kmRemoveParamTbl(void)
{
	char *cursor;
	int ret;
	char file[1024];

	strncpy(file, KM_PARAM_TBL_PATH, 0x17);
	ret = sceIoRemove(file);
	if (ret >= 0)
		printf("success to remove %s\n", KM_PARAM_TBL_PATH);
	else
		fprintf(stderr, "failed to remove %s, ret=%x\n", file, ret);

	cursor = file + strlen(file);
	for (;;) {
		--cursor;
		while (*cursor != '/' && cursor != file)
			--cursor;
		if (cursor == file)
			break;
		*cursor = '\0';
		ret = sceIoRmdir(file);
		if (ret < 0) {
			fprintf(stderr, "rmdir error %s, ret=%x\n", file, ret);
			ret = KM_ERR_IO;
			break;
		}
	}
	return ret;
}

/* update VTRM valid-time duration */
int kmSetValidTime(SceUInt64 validTime)
{
	int status;

	status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	g_kmTimeConfig.validTime = validTime;
	status = kmVtrmWriteTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	return 0;
}

/* update VTRM period-time duration */
int kmSetPeriodTime(SceUInt64 periodTime)
{
	int status;

	status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	g_kmTimeConfig.periodTime = periodTime;
	status = kmVtrmWriteTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	return 0;
}

/* update VTRM secure-clock offset directly */
int kmSetTimeOffset(SceInt64 timeOffset)
{
	int status;

	status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	g_kmTimeConfig.timeOffset = (SceUInt64)timeOffset;
	status = kmVtrmWriteTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	return 0;
}

/* set VTRM secure-clock offset relative to current secure RTC */
int kmSetVirtualTime(SceInt64 targetTime)
{
	int status;
	int ret;
	SceRtcTick tick;
	SceUInt64 pullTime;
	SceDateTime dateTime;

	status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;

	ret = vshRtcGetCurrentSecureTick(&tick);
	if (ret < 0 ||
	    (ret = sceRtcSetTick(&dateTime, &tick)) < 0 ||
	    (ret = sceRtcGetTime64_t(&dateTime, &pullTime)) < 0) {
		printf("    cannot get secure clock\n");
		if (ret == SCE_RTC_ERROR_INVALID_TICK)
			return KM_ERR_NO_SECURE_CLOCK;
		return KM_ERR_IO;
	}
	g_kmTimeConfig.timeOffset = (SceUInt64)(targetTime - (SceInt64)pullTime);
	status = kmVtrmWriteTimeConfig(&g_kmTimeConfig);
	if (status != 0)
		return status;
	return 0;
}

/* read VTRM valid-time duration */
int swKeyStoreGetDeviceId(SceUInt64 *outValidTime)
{
	int status;

	if (g_kmTimeConfigDirty != 0) {
		g_kmTimeConfigDirty = 0;
		status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
		if (status != 0)
			return status;
	}
	*outValidTime = g_kmTimeConfig.validTime;
	return 0;
}

/* read VTRM period-time duration */
int swKeyStoreGetSeed(SceUInt64 *outPeriodTime)
{
	int status;

	if (g_kmTimeConfigDirty != 0) {
		g_kmTimeConfigDirty = 0;
		status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
		if (status != 0)
			return status;
	}
	*outPeriodTime = g_kmTimeConfig.periodTime;
	return 0;
}

/* read VTRM secure-clock offset */
int swKeyStoreGetTimeOffset(SceInt64 *outOffset)
{
	int status;

	if (g_kmTimeConfigDirty != 0) {
		g_kmTimeConfigDirty = 0;
		status = kmVtrmReadTimeConfig(&g_kmTimeConfig);
		if (status != 0)
			return status;
	}
	*outOffset = (SceInt64)g_kmTimeConfig.timeOffset;
	return 0;
}

/* return parameter table entry count */
int kmGetParamCount(unsigned int *outCount)
{
	*outCount = g_kmParamTbl.nparam;
	return 0;
}

/* override activation time of an in-memory parameter entry */
int kmSetParamActTime(unsigned int actTime, int index)
{
	g_kmParamTbl.info[index].actTime = (SceUInt64)actTime;
	return 0;
}

/* extract upper 32-bit word of a 64-bit time value */
unsigned int kmTimeHighWord(SceUInt64 value)
{
	unsigned int words[2];

	memcpy(words, &value, sizeof(words));
	return words[1];
}

/* dump paramA debug fields to stdout */
void kmDumpParamA(const unsigned char *param)
{
	SceUInt64 startTime;
	unsigned char stHmac[12];

	kmParamReadStartTime(&startTime, param);
	kmParamReadBytes(stHmac, param, 84, 12);
	printf("    <<paramA>>\n");
	printf("    st_flag: 0x%x\n", kmParamReadByte(param, 75));
	printf("    st_hmac: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	       stHmac[0], stHmac[1], stHmac[2], stHmac[3],
	       stHmac[3], stHmac[5], stHmac[6], stHmac[7],
	       stHmac[8], stHmac[9], stHmac[10], stHmac[11]);
	printf("    start_time: 0x%llx %s\n", (unsigned long long)startTime,
	       ctime((const time_t *)&startTime));
}

/* dump paramB debug fields and SRM bytes to stdout */
void kmDumpParamB(const unsigned char *param)
{
	unsigned short srmLen;
	int i;
	unsigned char kxLabel;
	unsigned char kxHmac[12];

	kmParamReadBytes(&kxLabel, param, 55, 1);
	kmParamReadBytes(kxHmac, param, 56, 12);
	printf("    <<paramB>>\n");
	printf("    kx_flag: 0x%x\n", kmParamReadByte(param, 54));
	printf("    kx_label: 0x%x\n", kxLabel);
	printf("    kx_hmac: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	       kxHmac[0], kxHmac[1], kxHmac[2], kxHmac[3],
	       kxHmac[3], kxHmac[5], kxHmac[6], kxHmac[7],
	       kxHmac[8], kxHmac[9], kxHmac[10], kxHmac[11]);
	srmLen = kmParamReadU16(param, 70);
	printf("    srm_len: %d\n", srmLen);
	if (srmLen != 0) {
		printf("    srm: ");
		for (i = 0; i < (int)srmLen - 1; ++i)
			printf("0x%x, ", kmParamReadByte(param, i + 72));
		printf("0x%x\n", kmParamReadByte(param, (int)srmLen + 71));
	}
}

/* dump parameter table metadata to stdout */
void kmDumpParamTbl(void)
{
	int i;

	printf("    <<km_param_tbl>>\n");
	printf("    tbl_id: %d\n", g_kmParamTbl.tblId);
	printf("    nparam: %d\n", g_kmParamTbl.nparam);
	for (i = 0; i < (int)g_kmParamTbl.nparam; ++i) {
		printf("\tinfo[%d]\n", i);
		printf("\tgen_time: 0x%llx %lld\n",
		       (unsigned long long)g_kmParamTbl.info[i].genTime,
		       (long long)g_kmParamTbl.info[i].genTime);
		printf("\tact_time: 0x%llx %lld\n",
		       (unsigned long long)g_kmParamTbl.info[i].actTime,
		       (long long)g_kmParamTbl.info[i].actTime);
		printf("\tsize: %d\n", g_kmParamTbl.info[i].size);
	}
}

/* open a file with 0600 permissions when SCE_O_CREAT is set */
SceUID kmOpenFileMode(const char *path, int flags)
{
	if ((flags & SCE_O_CREAT) != 0)
		return sceIoOpen(path, flags, 0600);
	return sceIoOpen(path, flags, 0);
}

/* write a buffer to a file path */
int kmWriteFileSimple(const void *data, SceSize size, const char *path)
{
	SceUID fd;

	fd = kmOpenFileMode(path, SCE_O_WRONLY);
	if (fd == 0)
		return KM_ERR_FILE_IO;
	if (sceIoWrite(fd, data, size) != 1)
		return KM_ERR_NOT_ACTIVATED;
	if (sceIoClose(fd) == -1)
		return KM_ERR_FILE_IO;
	return 0;
}

/* return pointer to VTRM wrapped key slot 0 or 1 */
unsigned char *kmVtrmGetWrappedKey(unsigned char slot)
{
	if (slot != 0)
		return g_vtrmKey1;
	return g_vtrmKey0;
}

/* return pointer to VTRM IV */
unsigned char *kmVtrmGetIv(void)
{
	return g_vtrmIv;
}

/* decrypt a VTRM key slot via SBL portability */
int kmVtrmDecryptKeySlot(int slot, void *outKey)
{
	unsigned char *wrapped;
	unsigned char *iv;
	int i;
	struct KmPortabilityBlock inBlock;
	struct KmPortabilityBlock outBlock;

	wrapped = kmVtrmGetWrappedKey((unsigned char)slot);
	iv = kmVtrmGetIv();
	for (i = 0; i < 2; ++i) {
		memcpy(inBlock.data, wrapped + 32 * i, 32);
		inBlock.size = 32;
		if (vshSblSsDecryptWithPortability(8, iv, &inBlock, &outBlock) != 0)
			return -1;
		if (i == 0)
			memcpy(outKey, outBlock.data, 32);
	}
	return 0;
}

/* schedule AES-CBC key and IV for VTRM block crypto */
int kmVtrmAesCbcInit(struct KmAesCbcContext *ctx, SceUInt32 blockBits, SceUInt32 keyBits,
		     const void *key, const unsigned char *iv)
{
	unsigned int i;
	int ret;

	ret = sceAesScheduleKey(&ctx->aes, blockBits, keyBits, key);
	if (ret < 0)
		return ret;
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		ctx->iv[i] = iv[i];
	return 0;
}

/* encrypt one AES-CBC block for VTRM */
int kmVtrmAesCbcEncryptBlock(struct KmAesCbcContext *ctx, const unsigned char *inBlock,
			     unsigned char *outBlock)
{
	unsigned int i;
	unsigned char xored[32];

	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		xored[i] = inBlock[i] ^ ctx->iv[i];
	sceAesEncrypt(&ctx->aes, xored, outBlock);
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		ctx->iv[i] = outBlock[i];
	return 0;
}

/* encrypt a buffer using AES-128-CBC for VTRM */
int kmVtrmAesCbcEncrypt(const unsigned char *src, unsigned char *dst, unsigned int size,
			const void *key, const unsigned char *iv)
{
	unsigned int i;
	struct KmAesCbcContext ctx;

	if (src == NULL || dst == NULL || key == NULL || iv == NULL || (size & 0xFu) != 0)
		return -1;
	if (kmVtrmAesCbcInit(&ctx, 128, 128, key, iv) < 0)
		return -1;
	for (i = 0; i < (size >> 4); ++i) {
		if (kmVtrmAesCbcEncryptBlock(&ctx, src, dst) < 0)
			return -1;
		src += 16;
		dst += 16;
	}
	return 0;
}

/* decrypt one AES-CBC block for VTRM */
int kmVtrmAesCbcDecryptBlock(struct KmAesCbcContext *ctx, const unsigned char *inBlock,
			     unsigned char *outBlock)
{
	unsigned int i;
	unsigned char decrypted[32];
	unsigned char plainByte;

	sceAesDecrypt(&ctx->aes, inBlock, decrypted);
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i) {
		plainByte = ctx->iv[i] ^ decrypted[i];
		ctx->iv[i] = inBlock[i];
		outBlock[i] = plainByte;
	}
	return 0;
}

/* decrypt a buffer using AES-128-CBC for VTRM */
int kmVtrmAesCbcDecrypt(const unsigned char *src, unsigned char *dst, unsigned int size,
			const void *key, const unsigned char *iv)
{
	unsigned int i;
	struct KmAesCbcContext ctx;

	if (src == NULL || dst == NULL || key == NULL || iv == NULL || (size & 0xFu) != 0)
		return -1;
	if (kmVtrmAesCbcInit(&ctx, 128, 128, key, iv) < 0)
		return -1;
	for (i = 0; i < (size >> 4); ++i) {
		if (kmVtrmAesCbcDecryptBlock(&ctx, src, dst) < 0)
			return -1;
		src += 16;
		dst += 16;
	}
	return 0;
}

/* verify HMAC-SHA256 and AES-128-CBC decrypt a 112-byte VTRM block */
int kmVtrmUnprotectBlock(const unsigned char *inBuf, unsigned char *outBuf, int plainLen,
			 int unusedArg, unsigned char blockId)
{
	int ret;
	unsigned char mac[32];

	(void)plainLen;
	(void)unusedArg;
	if (sceHmacSha256Digest(g_vtrmCurrentKey, 32, inBuf, 80, mac) < 0)
		return -1;
	if (memcmp(mac, inBuf + 80, 32) != 0) {
		kmVtrmDestroyAll();
		return -1;
	}
	g_vtrmIv[7] = (unsigned char)(blockId + 86);
	ret = kmVtrmAesCbcDecrypt(inBuf, outBuf, 64, g_vtrmCurrentKey, g_vtrmIv);
	if (ret < 0)
		return -1;
	return ret;
}

/* AES-128-CBC encrypt and append marker + HMAC-SHA256 to a VTRM block */
int kmVtrmProtectBlock(const unsigned char *inBuf, unsigned char *outBuf, int plainLen,
		       int unusedArg, unsigned char blockId)
{
	static const unsigned char kMarkerTail[9] = {
		'g', '.', 'O', '3', 'z', '^', '<', '"', 'p'
	};
	int ret;
	unsigned char marker[16];
	unsigned char mac[32];

	(void)plainLen;
	(void)unusedArg;
	g_vtrmIv[7] = (unsigned char)(blockId + 86);
	if (kmVtrmAesCbcEncrypt(inBuf, outBuf, 64, g_vtrmCurrentKey, g_vtrmIv) < 0)
		return -1;

	memset(marker, 0, sizeof(marker));
	marker[0] = 1;
	marker[1] = 89;
	marker[2] = 82;
	marker[3] = 38;
	marker[4] = 44;
	marker[5] = 108;
	marker[6] = 25;
	memcpy(marker + 7, kMarkerTail, sizeof(kMarkerTail));
	memcpy(outBuf + 64, marker, sizeof(marker));

	ret = sceHmacSha256Digest(g_vtrmCurrentKey, 32, outBuf, 80, mac);
	if (ret < 0)
		return -1;
	memcpy(outBuf + 80, mac, sizeof(mac));
	return ret;
}

/* ensure tm0:dtcp exists and select active VTRM key */
int swKeyStoreLoadVtrm(void)
{
	int ret;
	int selected;
	SceIoStat stat;

	ret = sceIoGetstat(KM_VTRM_DIR, &stat);
	if (ret < 0) {
		if (ret != SCE_ERROR_ERRNO_ENOENT)
			return -1;
		if (sceIoMkdir(KM_VTRM_DIR, 6) < 0)
			return -1;
	}
	if (sceIoGetstat(KM_VTRM_DIR, &stat) < 0)
		return -1;
	if (kmVtrmDecryptKeySlot(0, g_vtrmKey0) < 0)
		return -1;
	ret = kmVtrmDecryptKeySlot(1, g_vtrmKey1);
	if (ret < 0)
		return -1;

	selected = -1;
	if (g_vtrmKey1[16] == 'h' && g_vtrmKey1[17] == 'i')
		selected = 1;
	if (g_vtrmKey0[16] == 'h' && g_vtrmKey0[17] == 'e')
		selected = 0;
	if (selected < 0)
		return -1;

	if (selected != 0)
		memcpy(g_vtrmCurrentKey, g_vtrmKey1, 32);
	else
		memcpy(g_vtrmCurrentKey, g_vtrmKey0, 32);
	return ret;
}

/* read and decrypt a 64-byte VTRM block */
int kmVtrmReadBlock(unsigned char *outBuf, SceUInt64 blockId)
{
	int ret;
	SceUID fd;
	int exists = 0;
	char file[16];
	unsigned char buf[112];

	ret = kmVtrmCheckBlock(blockId, file, &exists);
	if (ret < 0)
		return ret;
	if (exists == 0)
		return -2;
	fd = sceIoOpen(file, SCE_O_RDONLY, 4);
	if (fd < 0)
		return -1;
	if (sceIoRead(fd, buf, sizeof(buf)) < 0) {
		sceIoClose(fd);
		return -1;
	}
	ret = kmVtrmUnprotectBlock(buf, outBuf, 64, 0, (unsigned char)blockId);
	sceIoClose(fd);
	if (ret < 0)
		return -3;
	return ret;
}

/* encrypt and write a 64-byte VTRM block */
int kmVtrmWriteBlock(const unsigned char *inBuf, SceUInt64 blockId)
{
	SceSSize written;
	SceUID fd;
	char file[16];
	unsigned char buf[112];

	if (kmVtrmGetBlockPath(blockId, file) < 0)
		return -1;
	fd = sceIoOpen(file, SCE_O_RDWR | SCE_O_CREAT, 6);
	if (fd < 0)
		return -1;
	if (kmVtrmProtectBlock(inBuf, buf, 64, 0, (unsigned char)blockId) < 0) {
		sceIoClose(fd);
		return -4;
	}
	written = sceIoWrite(fd, buf, sizeof(buf));
	sceIoClose(fd);
	if (written < 0)
		return -1;
	return (int)written;
}

/* delete a single VTRM block file */
int kmVtrmDeleteBlock(SceUInt64 blockId)
{
	int ret;
	int exists = 0;
	char file[16];

	ret = kmVtrmCheckBlock(blockId, file, &exists);
	if (ret < 0)
		return ret;
	if (exists == 0)
		return -2;
	ret = sceIoRemove(file);
	if (ret < 0)
		return -1;
	return ret;
}

/* delete all files in tm0:dtcp and remove the directory */
int kmVtrmDestroyAll(void)
{
	SceUID dirFd;
	char file[128];
	SceIoDirent entry;

	dirFd = sceIoDopen(KM_VTRM_DIR);
	if (dirFd >= 0) {
		for (;;) {
			memset(file, 0, sizeof(file));
			strncpy(file, KM_VTRM_DIR, 9);
			if (sceIoDread(dirFd, &entry) <= 0)
				break;
			strncat(file, "/", 1);
			strncat(file, entry.d_name, 5);
			sceIoRemove(file);
		}
		sceIoDclose(dirFd);
		sceIoRmdir(KM_VTRM_DIR);
	}
	return 0;
}

/* read VTRM block 0 (KmStat), zero-initializing if absent */
int kmVtrmReadStat(struct KmStat *stat)
{
	int ret;

	ret = kmVtrmReadBlock((unsigned char *)stat, 0);
	if (ret < 0) {
		if (ret != -2)
			return KM_ERR_VTRM;
		memset(stat, 0, sizeof(*stat));
	}
	return 0;
}

/* write VTRM block 0 (KmStat) */
int kmVtrmWriteStat(const struct KmStat *stat)
{
	if (kmVtrmWriteBlock((const unsigned char *)stat, 0) >= 0)
		return 0;
	return KM_ERR_VTRM;
}

/* read SRM payload spanning VTRM blocks 1..N */
int kmVtrmReadSrm(unsigned char *srm, unsigned short len)
{
	unsigned short remain = len;
	int offset = 0;
	int blockId = 1;
	unsigned char block[64];

	while (remain >= 64u) {
		memset(block, 0, sizeof(block));
		if (kmVtrmReadBlock(block, (SceUInt64)blockId) < 0)
			return KM_ERR_VTRM;
		memcpy(srm + offset, block, 64);
		remain = (unsigned short)(remain - 64u);
		offset += 64;
		++blockId;
	}
	if (remain != 0) {
		memset(block, 0, sizeof(block));
		if (kmVtrmReadBlock(block, (SceUInt64)blockId) < 0)
			return KM_ERR_VTRM;
		memcpy(srm + offset, block, remain);
	}
	return 0;
}

/* write SRM payload spanning VTRM blocks 1..N, skipping unchanged blocks */
int kmVtrmWriteSrm(const unsigned char *srm, unsigned short len)
{
	unsigned short remain = len;
	int offset = 0;
	int blockId;
	int ret;
	unsigned char newBlock[64];
	unsigned char curBlock[64];

	for (blockId = 1; remain >= 64u; ++blockId) {
		memset(newBlock, 0, sizeof(newBlock));
		memcpy(newBlock, srm + offset, 64);
		memset(curBlock, 0, sizeof(curBlock));
		ret = kmVtrmReadBlock(curBlock, (SceUInt64)blockId);
		if (ret < 0 && ret != -2)
			return KM_ERR_VTRM;
		if (ret < 0 || memcmp(newBlock, curBlock, 64) != 0) {
			if (kmVtrmWriteBlock(newBlock, (SceUInt64)blockId) < 0)
				return KM_ERR_VTRM;
		}
		remain = (unsigned short)(remain - 64u);
		offset += 64;
	}
	if (remain != 0) {
		memset(newBlock, 0, sizeof(newBlock));
		memcpy(newBlock, srm + offset, remain);
		memset(curBlock, 0, sizeof(curBlock));
		ret = kmVtrmReadBlock(curBlock, (SceUInt64)blockId);
		if (ret < 0 && ret != -2)
			return KM_ERR_VTRM;
		if (ret < 0 || memcmp(newBlock, curBlock, 64) != 0) {
			if (kmVtrmWriteBlock(newBlock, (SceUInt64)blockId) < 0)
				return KM_ERR_VTRM;
		}
	}
	return 0;
}

/* read VTRM block 15 (timing config), setting defaults if absent */
int kmVtrmReadTimeConfig(struct KmTimeConfig *cfg)
{
	int ret;

	ret = kmVtrmReadBlock((unsigned char *)cfg, 15);
	if (ret < 0) {
		if (ret != -2)
			return KM_ERR_VTRM;
		cfg->validTime = KM_DEFAULT_VALID_SEC;
		cfg->periodTime = KM_DEFAULT_VALID_SEC;
		cfg->timeOffset = 0;
	}
	return 0;
}

/* write VTRM block 15 (timing config) */
int kmVtrmWriteTimeConfig(const struct KmTimeConfig *cfg)
{
	if (kmVtrmWriteBlock((const unsigned char *)cfg, 15) >= 0)
		return 0;
	return KM_ERR_VTRM;
}

/* dump VTRM block 0 fields to stdout */
int kmVtrmDumpStat(void)
{
	struct KmStat stat;

	if (kmVtrmReadBlock((unsigned char *)&stat, 0) < 0)
		return KM_ERR_VTRM;

	printf("    <<VTRM>>\n");
	printf("    ver: %d, param_stat: %d\n", stat.ver, stat.flags & 0xFu);
	printf("    is_srm_exist: %d, st_flag: 0x%x, kx_flag: 0x%x\n",
	       (stat.flags & 0x10u) != 0,
	       (stat.flags & 0x20u) != 0,
	       (stat.flags & 0x40u) != 0);
	printf("    cert_rest: 0x%x 0x%x 0x%x\n",
	       stat.certRest[0], stat.certRest[1], stat.certRest[2]);
	printf("    tbl_id: %d\n", stat.tblId);
	printf("    tbl_off: %d\n", stat.tblOff);
	printf("    id: %d\n", stat.keyId);
	kmTimeHighWord(stat.startTime);
	printf("    start_time: 0x%llx %lld\n",
	       (unsigned long long)stat.startTime,
	       (long long)stat.startTime);
	printf("    srm_ver: %d\n", stat.srmVer);
	printf("    srm_len: %d\n", stat.srmLen);
	printf("    kx_label: 0x%x\n", stat.kxLabel);
	printf("    st_hmac: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	       stat.stHmac[0], stat.stHmac[1], stat.stHmac[2], stat.stHmac[3],
	       stat.stHmac[3], stat.stHmac[5], stat.stHmac[6], stat.stHmac[7],
	       stat.stHmac[8], stat.stHmac[9], stat.stHmac[10], stat.stHmac[11]);
	printf("    kx_hmac: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	       stat.kxHmac[0], stat.kxHmac[1], stat.kxHmac[2], stat.kxHmac[3],
	       stat.kxHmac[3], stat.kxHmac[5], stat.kxHmac[6], stat.kxHmac[7],
	       stat.kxHmac[8], stat.kxHmac[9], stat.kxHmac[10], stat.kxHmac[11]);
	return 0;
}

/* map VTRM block index 0..16 to its tm0:dtcp/Bxx file path */
int kmVtrmGetBlockPath(SceUInt64 blockId, char *path)
{
	switch (blockId) {
	case 0:
		strncpy(path, "tm0:dtcp/B00", 13);
		break;
	case 1:
		strncpy(path, "tm0:dtcp/B01", 13);
		break;
	case 2:
		strncpy(path, "tm0:dtcp/B02", 13);
		break;
	case 3:
		strncpy(path, "tm0:dtcp/B03", 13);
		break;
	case 4:
		strncpy(path, "tm0:dtcp/B04", 13);
		break;
	case 5:
		strncpy(path, "tm0:dtcp/B05", 13);
		break;
	case 6:
		strncpy(path, "tm0:dtcp/B06", 13);
		break;
	case 7:
		strncpy(path, "tm0:dtcp/B07", 13);
		break;
	case 8:
		strncpy(path, "tm0:dtcp/B08", 13);
		break;
	case 9:
		strncpy(path, "tm0:dtcp/B09", 13);
		break;
	case 10:
		strncpy(path, "tm0:dtcp/B10", 13);
		break;
	case 11:
		strncpy(path, "tm0:dtcp/B11", 13);
		break;
	case 12:
		strncpy(path, "tm0:dtcp/B12", 13);
		break;
	case 13:
		strncpy(path, "tm0:dtcp/B13", 13);
		break;
	case 14:
		strncpy(path, "tm0:dtcp/B14", 13);
		break;
	case 15:
		strncpy(path, "tm0:dtcp/B15", 13);
		break;
	case 16:
		strncpy(path, "tm0:dtcp/B16", 13);
		break;
	default:
		return -1;
	}
	return 0;
}

/* resolve VTRM block path and test whether the file exists */
int kmVtrmCheckBlock(SceUInt64 blockId, char *path, int *exists)
{
	int ret;
	SceIoStat stat;

	*exists = 0;
	if (kmVtrmGetBlockPath(blockId, path) < 0)
		return -1;
	ret = sceIoGetstat(path, &stat);
	if (ret >= 0) {
		*exists = 1;
		return ret;
	}
	if (ret == SCE_ERROR_ERRNO_ENOENT) {
		*exists = 0;
		return 0;
	}
	return -1;
}
