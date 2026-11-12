/*
 * SceLibSsl user library interface, CA certificate store loader, certificate
 * field accessors, and quicksort helper.
 * Private helper names, struct names, and table names are ours, not Sony's.
 */

#include <fw.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libssl.h>
#include <libssl/sslinternal.h>
#include <libssl/name.h>
#include <libssl/object.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>

#include "memory.h"
#include "runtime.h"
#include "time.h"
#include "date.h"
#include "err/err.h"
#include "CRYPTOCME/common/library/lib_ctx.h"
#include "CRYPTOCME/common/module/crypto/cry_api.h"

int sceAppMgrConvertVs0UserDrivePath(const char *path, char *mountPoint, int size);
SceInt64 sceKernelGetSystemTimeWide(void);

typedef struct SceSslCaCertEntry {
	SceUChar8 sha1[21];
#if FW_SINCE(0x03650000)
	int store;
	SceUInt32 bit;
	SceUInt32 subjectHash;
#endif
} SceSslCaCertEntry;

typedef struct SceSslCaStoreEntry {
	int store;
	const char *name;
	int (*matchAll)(int store);
	int (*filterMask)(int store, int mask);
	int (*checkMask)(int store, int mask, int allowedMask);
	int allowedMask;
} SceSslCaStoreEntry;

typedef struct SceSslRevokedSerial {
	SceSize len;
	const SceUChar8 *data;
} SceSslRevokedSerial;

typedef struct SceSslDataItem {
	SceSize len;
	const SceUChar8 *data;
} SceSslDataItem;

typedef struct SceSslAvaPart {
	int type;
	SceSize len;
	const SceUChar8 *data;
} SceSslAvaPart;

typedef struct SceSslNameEntry {
	int index;
	void *ava;
	int reserved[3];
} SceSslNameEntry;

static SceUID s_sslMemBlockId = -1;
static int s_sslInitialized;
static void *s_sslMspace;
static SceKernelLwMutexWork s_sslMutex;
static SslLibrary *s_sslLibrary;
static int s_sslMutexCreated;

static int _sceSslCaStoreMatchAll(int store);
static int _sceSslCaStoreFilterMask(int store, int mask);
static int _sceSslCaStoreCheckMask(int store, int mask, int allowedMask);
static __attribute__((noinline)) int _sceSslGetCaCert(int store, int mask, const char ***pem, char *buf,
			    SceSize size, SceSize *outSize);
#if FW_SINCE(0x03650000)
static __attribute__((noinline)) int _sceSslGetCaCertBySubjectHashes(void *certName, const char ***pem,
					   char *buf, SceSize size,
					   SceSize *outSize, SceBool systemOnly);
#endif
static __attribute__((noinline)) int _sceSslGetRevokedSerials(int store, int mask, const void **serials,
				    int *count);

static SceSslCaStoreEntry s_caStores360[] = {
	{ 1, "SCE_SSLCERT_ISSUER_SCE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 2, "SCE_SSLCERT_ISSUER_VERISIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 3, "SCE_SSLCERT_ISSUER_GEOTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 4, "SCE_SSLCERT_ISSUER_THAWTE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 5, "SCE_SSLCERT_ISSUER_COMODO", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 6, "SCE_SSLCERT_ISSUER_GLOBALSIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 7, "SCE_SSLCERT_ISSUER_CYBERTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 8, "SCE_SSLCERT_ISSUER_ENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 9, "SCE_SSLCERT_ISSUER_DIGICERT", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 10, "SCE_SSLCERT_ISSUER_GODADDY", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7f },
	{ 11, "SCE_SSLCERT_ISSUER_RSA", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 12, "SCE_SSLCERT_ISSUER_STARTCOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 13, "SCE_SSLCERT_ISSUER_SECOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 14, "SCE_SSLCERT_ISSUER_TRUSTWAVE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ 15, "SCE_SSLCERT_ISSUER_AFFIRMTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ -1, NULL, NULL, NULL, NULL, 0 }
};

#if FW_SINCE(0x03650000)
static SceSslCaStoreEntry s_caStores365[] = {
	{ 1, "SCE_SSLCERT_ISSUER_SCE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 2, "SCE_SSLCERT_ISSUER_VERISIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3f },
	{ 3, "SCE_SSLCERT_ISSUER_GEOTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3f },
	{ 4, "SCE_SSLCERT_ISSUER_THAWTE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 5, "SCE_SSLCERT_ISSUER_COMODO", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 6, "SCE_SSLCERT_ISSUER_GLOBALSIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 7, "SCE_SSLCERT_ISSUER_CYBERTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 8, "SCE_SSLCERT_ISSUER_ENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 9, "SCE_SSLCERT_ISSUER_DIGICERT", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 10, "SCE_SSLCERT_ISSUER_GODADDY", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7f },
	{ 11, "SCE_SSLCERT_ISSUER_RSA", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 12, "SCE_SSLCERT_ISSUER_STARTCOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 13, "SCE_SSLCERT_ISSUER_SECOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 14, "SCE_SSLCERT_ISSUER_TRUSTWAVE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ 15, "SCE_SSLCERT_ISSUER_AFFIRMTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 16, "SCE_SSLCERT_ISSUER_KEYNECTIS_CERTPLUS", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ 17, "SCE_SSLCERT_ISSUER_UNIZETO_CERTUM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ 18, "SCE_SSLCERT_ISSUER_IDENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ -1, NULL, NULL, NULL, NULL, 0 }
};
#endif

static SceSslCaStoreEntry s_caStores100[] = {
	{ 1, "SCE_SSLCERT_ISSUER_SCE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 2, "SCE_SSLCERT_ISSUER_VERISIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 3, "SCE_SSLCERT_ISSUER_GEOTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 4, "SCE_SSLCERT_ISSUER_THAWTE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 5, "SCE_SSLCERT_ISSUER_COMODO", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 6, "SCE_SSLCERT_ISSUER_GLOBALSIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 7, "SCE_SSLCERT_ISSUER_CYBERTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 8, "SCE_SSLCERT_ISSUER_ENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 9, "SCE_SSLCERT_ISSUER_DIGICERT", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 10, "SCE_SSLCERT_ISSUER_GODADDY", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3f },
	{ 11, "SCE_SSLCERT_ISSUER_RSA", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ -1, NULL, NULL, NULL, NULL, 0 }
};

static SceSslCaStoreEntry s_caStores180[] = {
	{ 1, "SCE_SSLCERT_ISSUER_SCE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 2, "SCE_SSLCERT_ISSUER_VERISIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 3, "SCE_SSLCERT_ISSUER_GEOTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 4, "SCE_SSLCERT_ISSUER_THAWTE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 5, "SCE_SSLCERT_ISSUER_COMODO", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 6, "SCE_SSLCERT_ISSUER_GLOBALSIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 7, "SCE_SSLCERT_ISSUER_CYBERTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 8, "SCE_SSLCERT_ISSUER_ENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 9, "SCE_SSLCERT_ISSUER_DIGICERT", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 10, "SCE_SSLCERT_ISSUER_GODADDY", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7f },
	{ 11, "SCE_SSLCERT_ISSUER_RSA", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 12, "SCE_SSLCERT_ISSUER_STARTCOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 13, "SCE_SSLCERT_ISSUER_SECOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ -1, NULL, NULL, NULL, NULL, 0 }
};

static SceSslCaStoreEntry s_caStores300[] = {
	{ 1, "SCE_SSLCERT_ISSUER_SCE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 2, "SCE_SSLCERT_ISSUER_VERISIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1f },
	{ 3, "SCE_SSLCERT_ISSUER_GEOTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 4, "SCE_SSLCERT_ISSUER_THAWTE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 5, "SCE_SSLCERT_ISSUER_COMODO", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 6, "SCE_SSLCERT_ISSUER_GLOBALSIGN", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 7, "SCE_SSLCERT_ISSUER_CYBERTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 8, "SCE_SSLCERT_ISSUER_ENTRUST", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0xf },
	{ 9, "SCE_SSLCERT_ISSUER_DIGICERT", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 10, "SCE_SSLCERT_ISSUER_GODADDY", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7f },
	{ 11, "SCE_SSLCERT_ISSUER_RSA", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 12, "SCE_SSLCERT_ISSUER_STARTCOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x3 },
	{ 13, "SCE_SSLCERT_ISSUER_SECOM", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x7 },
	{ 14, "SCE_SSLCERT_ISSUER_TRUSTWAVE", _sceSslCaStoreMatchAll, _sceSslCaStoreFilterMask, _sceSslCaStoreCheckMask, 0x1 },
	{ -1, NULL, NULL, NULL, NULL, 0 }
};

static const SceUChar8 s_revokedSerial0[20] = {
	0x04, 0x7e, 0xcb, 0xe9, 0xfc, 0xa5, 0x5f, 0x7b,
	0xd0, 0x9e, 0xae, 0x36, 0xe1, 0x0c, 0xae, 0x1e
};
static const SceUChar8 s_revokedSerial1[20] = {
	0x00, 0xf5, 0xc8, 0x6a, 0xf3, 0x61, 0x62, 0xf1,
	0x3a, 0x64, 0xf5, 0x4f, 0x6d, 0xc9, 0x58, 0x7c, 0x06
};
static const SceUChar8 s_revokedSerial2[20] = {
	0x00, 0xd7, 0x55, 0x8f, 0xda, 0xf5, 0xf1, 0x10,
	0x5b, 0xb2, 0x13, 0x28, 0x2b, 0x70, 0x77, 0x29, 0xa3
};
static const SceUChar8 s_revokedSerial3[20] = {
	0x39, 0x2a, 0x43, 0x4f, 0x0e, 0x07, 0xdf, 0x1f,
	0x8a, 0xa3, 0x05, 0xde, 0x34, 0xe0, 0xc2, 0x29
};
static const SceUChar8 s_revokedSerial4[20] = {
	0x3e, 0x75, 0xce, 0xd4, 0x6b, 0x69, 0x30, 0x21,
	0x21, 0x88, 0x30, 0xae, 0x86, 0xa8, 0x2a, 0x71
};
static const SceUChar8 s_revokedSerial5[20] = {
	0x00, 0xe9, 0x02, 0x8b, 0x95, 0x78, 0xe4, 0x15,
	0xdc, 0x1a, 0x71, 0x0a, 0x2b, 0x88, 0x15, 0x44, 0x47
};
static const SceUChar8 s_revokedSerial6[20] = {
	0x00, 0x92, 0x39, 0xd5, 0x34, 0x8f, 0x40, 0xd1,
	0x69, 0x5a, 0x74, 0x54, 0x70, 0xe1, 0xf2, 0x3f, 0x43
};
static const SceUChar8 s_revokedSerial7[20] = {
	0x00, 0xb0, 0xb7, 0x13, 0x3e, 0xd0, 0x96, 0xf9,
	0xb5, 0x6f, 0xae, 0x91, 0xc8, 0x74, 0xbd, 0x3a, 0xc0
};
static const SceUChar8 s_revokedSerial8[20] = {
	0x00, 0xd8, 0xf3, 0x5f, 0x4e, 0xb7, 0x87, 0x2b,
	0x2d, 0xab, 0x06, 0x92, 0xe3, 0x15, 0x38, 0x2f, 0xb0
};
static const SceUChar8 s_revokedSerial9[20] = {
	0x30, 0x0f, 0x80, 0x27, 0x56, 0x66, 0xe8, 0x11,
	0x29, 0x9c, 0xf7, 0x7a, 0xa8, 0xa0, 0x32, 0x8b
};

static const SceSslRevokedSerial s_revokedSerials[] = {
	{ 16, s_revokedSerial0 },
	{ 17, s_revokedSerial1 },
	{ 17, s_revokedSerial2 },
	{ 16, s_revokedSerial3 },
	{ 16, s_revokedSerial4 },
	{ 17, s_revokedSerial5 },
	{ 17, s_revokedSerial6 },
	{ 17, s_revokedSerial7 },
	{ 17, s_revokedSerial8 },
	{ 16, s_revokedSerial9 },
	{ 0, NULL }
};

static const SceSslCaCertEntry s_caCertTable[] = {
	{ { 0xf4, 0xba, 0x0f, 0xc7, 0x4e, 0x45, 0xc1, 0x47, 0x68, 0x5c, 0xac, 0xb2, 0xc2, 0xa8, 0x0b, 0x48, 0x1e, 0xa6, 0xe2, 0x60, 0x00 }
#if FW_SINCE(0x03650000)
	  , 1, 0x1U, 0x8654b5aaU
#endif
	},
	{ { 0x5a, 0xf0, 0x8b, 0xf7, 0x6d, 0x25, 0xc3, 0x9c, 0x3d, 0x5b, 0x59, 0xbf, 0xae, 0x4f, 0xad, 0x1e, 0x18, 0x00, 0x29, 0xce, 0x00 }
#if FW_SINCE(0x03650000)
	  , 1, 0x2U, 0xee725923U
#endif
	},
	{ { 0xca, 0x67, 0x86, 0x4b, 0x35, 0xc5, 0xda, 0x9b, 0x29, 0x1c, 0xaf, 0x59, 0xe5, 0x9d, 0x96, 0xdd, 0x9d, 0x65, 0x4e, 0x52, 0x00 }
#if FW_SINCE(0x03650000)
	  , 1, 0x4U, 0x16c72ed1U
#endif
	},
	{ { 0xa5, 0x10, 0x8f, 0xa3, 0xca, 0x94, 0xf3, 0x6f, 0xcb, 0xa7, 0x0c, 0x23, 0x04, 0x35, 0x0b, 0xee, 0x60, 0x00, 0x36, 0xf5, 0x00 }
#if FW_SINCE(0x03650000)
	  , 1, 0x8U, 0x24739c20U
#endif
	},
	{ { 0x71, 0x21, 0xe9, 0xfc, 0x59, 0xc1, 0xf2, 0xdf, 0x55, 0x7f, 0x6b, 0x70, 0x02, 0x2b, 0xd4, 0x2c, 0x32, 0xe8, 0x01, 0xc3, 0x00 }
#if FW_SINCE(0x03650000)
	  , 1, 0x10U, 0xb3a344a3U
#endif
	},
	{ { 0xfa, 0x17, 0x38, 0x62, 0x62, 0xe6, 0x2b, 0x77, 0x61, 0xc9, 0x7b, 0xbd, 0x55, 0x9b, 0xce, 0x75, 0xb5, 0x1e, 0x52, 0xbe, 0x00 }
#if FW_SINCE(0x03650000)
	  , 7, 0x2U, 0x08b47b69U
#endif
	},
	{ { 0xc1, 0x03, 0x79, 0x05, 0x03, 0xbf, 0x8c, 0x2f, 0xf3, 0xf1, 0x19, 0xad, 0xee, 0x02, 0x7e, 0xbb, 0x42, 0x9b, 0x9d, 0x21, 0x00 }
#if FW_SINCE(0x03650000)
	  , 7, 0x1U, 0x76452fc1U
#endif
	},
	{ { 0x5e, 0x74, 0xdb, 0xdd, 0x2d, 0x5c, 0xb7, 0x9b, 0xc3, 0x4c, 0x3d, 0x0e, 0x10, 0x43, 0x38, 0x26, 0xe5, 0x5e, 0x63, 0xd4, 0x00 }
#if FW_SINCE(0x03650000)
	  , 7, 0x4U, 0xf0c31a3fU
#endif
	},
	{ { 0x10, 0x1b, 0x85, 0x0e, 0x94, 0xd6, 0x0c, 0x92, 0x6a, 0x55, 0x45, 0x44, 0x4d, 0x13, 0x33, 0x9c, 0x4a, 0xd4, 0xab, 0xbc, 0x00 }
#if FW_SINCE(0x03650000)
	  , 3, 0x1U, 0x1739b4b1U
#endif
	},
	{ { 0xe0, 0x51, 0x10, 0xdd, 0xb9, 0xbc, 0xb9, 0xe4, 0x78, 0x18, 0xea, 0x6e, 0x95, 0x5c, 0xc6, 0xba, 0x78, 0xec, 0x66, 0x27, 0x00 }
#if FW_SINCE(0x03650000)
	  , 3, 0x2U, 0x15b366b7U
#endif
	},
	{ { 0xf4, 0xfd, 0x3f, 0xaa, 0x7e, 0x30, 0xde, 0xb9, 0x08, 0xb5, 0x14, 0xb7, 0xb8, 0x6e, 0x12, 0xe1, 0x8b, 0xda, 0x6d, 0x79, 0x00 }
#if FW_SINCE(0x03650000)
	  , 3, 0x4U, 0xf691a2d6U
#endif
	},
	{ { 0x24, 0xb0, 0x02, 0xa4, 0x0d, 0xd2, 0x78, 0xa5, 0x27, 0x36, 0x7b, 0x53, 0x04, 0x5c, 0x37, 0xdc, 0xe8, 0x7b, 0xda, 0xce, 0x00 }
#if FW_SINCE(0x03650000)
	  , 8, 0x1U, 0xdb722da0U
#endif
	},
	{ { 0x3e, 0xcd, 0x25, 0xc1, 0xe2, 0x35, 0xb0, 0x53, 0x76, 0xff, 0x42, 0x39, 0x2e, 0x1b, 0x41, 0xc7, 0x19, 0x94, 0xe2, 0xb7, 0x00 }
#if FW_SINCE(0x03650000)
	  , 8, 0x2U, 0xe1e381d7U
#endif
	},
	{ { 0x48, 0xdf, 0x6f, 0x1a, 0x1f, 0xf9, 0x2b, 0x68, 0xb6, 0x45, 0xc7, 0x48, 0xa5, 0x5d, 0xac, 0x48, 0xe0, 0x7e, 0x81, 0xf9, 0x00 }
#if FW_SINCE(0x03650000)
	  , 8, 0x4U, 0x69a64a2cU
#endif
	},
	{ { 0x6e, 0x51, 0x32, 0xfb, 0x06, 0x08, 0x62, 0x4d, 0xf5, 0x7f, 0xfe, 0xfd, 0x63, 0xbf, 0xa9, 0xfa, 0x02, 0xe6, 0xfc, 0x33, 0x00 }
#if FW_SINCE(0x03650000)
	  , 8, 0x8U, 0xb3d05ccbU
#endif
	},
	{ { 0xb2, 0x07, 0x3e, 0x04, 0xc8, 0x72, 0x06, 0x81, 0x82, 0xa5, 0x27, 0xb2, 0x20, 0x02, 0x63, 0x73, 0x77, 0xcc, 0x48, 0x5f, 0x00 }
#if FW_SINCE(0x03650000)
	  , 6, 0x1U, 0xb6b557b7U
#endif
	},
	{ { 0x61, 0xfb, 0xcd, 0xe5, 0xac, 0x6a, 0x8d, 0xd1, 0x04, 0x60, 0x97, 0xf6, 0xae, 0xf8, 0xd0, 0x6e, 0xeb, 0xd1, 0xf3, 0xe2, 0x00 }
#if FW_SINCE(0x03650000)
	  , 6, 0x2U, 0x318b5ce0U
#endif
	},
	{ { 0x00, 0xea, 0x32, 0x1d, 0xbb, 0x54, 0xce, 0xa1, 0x67, 0x93, 0xff, 0xd4, 0x75, 0xf6, 0x7d, 0x1d, 0xae, 0x25, 0xd0, 0xf5, 0x00 }
#if FW_SINCE(0x03650000)
	  , 4, 0x1U, 0x7e688934U
#endif
	},
	{ { 0x27, 0x78, 0xdc, 0x1d, 0xeb, 0x34, 0x11, 0xe1, 0xa0, 0x7e, 0x74, 0xf7, 0xcf, 0x06, 0xbb, 0xd1, 0xc6, 0xdb, 0x49, 0x81, 0x00 }
#if FW_SINCE(0x03650000)
	  , 4, 0x2U, 0x39e97c3cU
#endif
	},
	{ { 0xd0, 0x58, 0xba, 0xbd, 0x58, 0x40, 0xd7, 0xd3, 0x5b, 0x89, 0xb4, 0x9a, 0x67, 0x84, 0x5e, 0xcc, 0xa9, 0xdd, 0x09, 0x77, 0x00 }
#if FW_SINCE(0x03650000)
	  , 2, 0x1U, 0x4964b748U
#endif
	},
	{ { 0xc9, 0x12, 0xb3, 0x48, 0x9f, 0x23, 0x32, 0x91, 0x8a, 0x15, 0x84, 0x51, 0xe0, 0x24, 0xef, 0xd9, 0x25, 0xd4, 0x07, 0x71, 0x00 }
#if FW_SINCE(0x03650000)
	  , 2, 0x2U, 0x9a607ac4U
#endif
	},
	{ { 0xc3, 0x9f, 0x41, 0xba, 0xc8, 0x9d, 0x0b, 0x2a, 0x1b, 0xc9, 0xd0, 0x48, 0xe1, 0x79, 0x5c, 0xd7, 0xef, 0x47, 0x01, 0x30, 0x00 }
#if FW_SINCE(0x03650000)
	  , 2, 0x4U, 0x248e837aU
#endif
	},
	{ { 0x4d, 0x69, 0xb4, 0x83, 0x8c, 0x07, 0x36, 0x39, 0x6e, 0xaa, 0xea, 0x58, 0xfe, 0x79, 0x22, 0x2e, 0x25, 0x09, 0x8f, 0x72, 0x00 }
#if FW_SINCE(0x03650000)
	  , 2, 0x8U, 0x87b2e9b9U
#endif
	},
	{ { 0xd3, 0x7f, 0xa1, 0x56, 0x46, 0x9d, 0x69, 0xa8, 0x53, 0x6a, 0x11, 0x46, 0x1f, 0xf4, 0x8c, 0x25, 0xf4, 0xee, 0x4c, 0xe9, 0x00 }
#if FW_SINCE(0x03650000)
	  , 2, 0x10U, 0xa372603fU
#endif
	},
	{ { 0xf4, 0x0c, 0x41, 0x3c, 0x6d, 0x17, 0xc4, 0xc4, 0x19, 0x5d, 0x30, 0xa9, 0xa1, 0x45, 0x4d, 0x18, 0x67, 0x10, 0x72, 0x7c, 0x00 }
#if FW_SINCE(0x03650000)
	  , 5, 0x1U, 0x6e4fb553U
#endif
	},
	{ { 0x14, 0x14, 0x7d, 0xfc, 0x4a, 0x9e, 0x90, 0xc7, 0xf8, 0x3d, 0x88, 0xdd, 0xa4, 0x0b, 0xee, 0x36, 0x0c, 0xf9, 0xaa, 0xdc, 0x00 }
#if FW_SINCE(0x03650000)
	  , 5, 0x2U, 0x5466b17cU
#endif
	},
	{ { 0xd7, 0x02, 0x17, 0x05, 0xe1, 0xce, 0x34, 0xef, 0x3b, 0xbf, 0x32, 0xf9, 0x2d, 0x99, 0xd5, 0x58, 0x36, 0x05, 0xc7, 0x4b, 0x00 }
#if FW_SINCE(0x03650000)
	  , 5, 0x4U, 0x19e2a714U
#endif
	},
	{ { 0x2f, 0x1d, 0x00, 0x01, 0xe4, 0x73, 0x94, 0xf4, 0xc4, 0xde, 0xec, 0x96, 0x45, 0xc5, 0xf2, 0xdf, 0x99, 0xf9, 0x1a, 0x95, 0x00 }
#if FW_SINCE(0x03650000)
	  , 9, 0x1U, 0xe312aadfU
#endif
	},
	{ { 0x0d, 0x82, 0x95, 0xdf, 0xdb, 0x8f, 0xb9, 0x08, 0x5e, 0xfa, 0xfa, 0x61, 0x53, 0x7d, 0x84, 0xe6, 0xfb, 0x9f, 0x38, 0x2d, 0x00 }
#if FW_SINCE(0x03650000)
	  , 9, 0x2U, 0x0bbec4f7U
#endif
	},
	{ { 0x1d, 0xfa, 0xc1, 0x7a, 0xdf, 0x38, 0x67, 0xc9, 0xa2, 0x8a, 0xcb, 0x32, 0x9d, 0xe8, 0xd1, 0x6d, 0x8b, 0x41, 0x2d, 0x8b, 0x00 }
#if FW_SINCE(0x03650000)
	  , 9, 0x4U, 0x01065180U
#endif
	},
	{ { 0xf0, 0xec, 0xb0, 0x51, 0x00, 0x08, 0xa3, 0x1a, 0xd3, 0x60, 0x9a, 0xc8, 0xc5, 0x54, 0x10, 0xc3, 0x9b, 0xf4, 0xba, 0xa2, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x1U, 0xca8920e4U
#endif
	},
	{ { 0x59, 0xb1, 0xa1, 0x35, 0x80, 0x9c, 0xfd, 0x2f, 0xff, 0x1d, 0xd0, 0x71, 0xbb, 0x5c, 0x2c, 0x25, 0xf6, 0xb7, 0xb9, 0x6f, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x2U, 0x8e6f231dU
#endif
	},
	{ { 0x9e, 0xa4, 0x01, 0x94, 0xcd, 0x36, 0x10, 0xf7, 0x46, 0xf9, 0xfa, 0xde, 0xe8, 0x6d, 0x8e, 0x57, 0xe7, 0x90, 0x5d, 0x2e, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x4U, 0x849ec18bU
#endif
	},
	{ { 0xe0, 0xae, 0x8c, 0x47, 0x8c, 0x3e, 0x0c, 0xbe, 0x1f, 0x2c, 0x5f, 0x16, 0x36, 0x4f, 0x20, 0xa9, 0x52, 0x92, 0x72, 0x60, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x8U, 0x84f8c014U
#endif
	},
	{ { 0xf4, 0xcd, 0x54, 0x8d, 0x1b, 0x42, 0xda, 0x3d, 0xa4, 0x50, 0x15, 0x9c, 0x74, 0x55, 0x1b, 0x89, 0x25, 0x5e, 0x80, 0x93, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x10U, 0x6d5d2941U
#endif
	},
	{ { 0x07, 0xe8, 0xb3, 0xd0, 0xfa, 0x86, 0x66, 0xf7, 0x14, 0x01, 0x98, 0xb9, 0xe8, 0x40, 0x2d, 0x10, 0xaf, 0x9b, 0xce, 0xd2, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x20U, 0x44a47d88U
#endif
	},
	{ { 0x8e, 0xd5, 0xb4, 0x05, 0xa5, 0x63, 0x53, 0x68, 0x8d, 0x9b, 0x79, 0x7f, 0x71, 0x69, 0x22, 0x95, 0xae, 0xb0, 0x50, 0x06, 0x00 }
#if FW_SINCE(0x03650000)
	  , 11, 0x1U, 0xeb23c8f4U
#endif
	},
	{ { 0xbe, 0x3c, 0xa7, 0x26, 0xaa, 0x92, 0xdd, 0x57, 0x70, 0x1a, 0x1d, 0x89, 0xde, 0x21, 0xfa, 0x08, 0x8b, 0xa5, 0xc0, 0x05, 0x00 }
#if FW_SINCE(0x03650000)
	  , 11, 0x2U, 0x4aeec985U
#endif
	},
	{ { 0xba, 0x65, 0x63, 0x29, 0xd2, 0xb5, 0x45, 0x1a, 0x26, 0xfd, 0xde, 0xac, 0x1f, 0xa8, 0xb9, 0xe5, 0x95, 0xed, 0xc4, 0x24, 0x00 }
#if FW_SINCE(0x03650000)
	  , 4, 0x4U, 0x6b8a1730U
#endif
	},
	{ { 0x06, 0xf9, 0x6e, 0x6b, 0x24, 0x00, 0x26, 0xf6, 0x33, 0x88, 0xdf, 0xd5, 0x69, 0x12, 0xe0, 0x92, 0x4f, 0x93, 0x17, 0x52, 0x00 }
#if FW_SINCE(0x03650000)
	  , 12, 0x1U, 0xd3a67341U
#endif
	},
	{ { 0x81, 0xd8, 0x33, 0x60, 0x32, 0xc9, 0x0e, 0x70, 0xc9, 0x10, 0x2f, 0x46, 0x12, 0x57, 0xb4, 0x6a, 0xbc, 0xcc, 0xb3, 0x68, 0x00 }
#if FW_SINCE(0x03650000)
	  , 12, 0x2U, 0xc50f980aU
#endif
	},
	{ { 0x2d, 0x99, 0xbf, 0xca, 0x85, 0x7c, 0x65, 0xf8, 0x0a, 0x55, 0x45, 0x20, 0xdd, 0xf3, 0x9f, 0x27, 0x10, 0xb3, 0x45, 0x64, 0x00 }
#if FW_SINCE(0x03650000)
	  , 10, 0x40U, 0x1fcda58dU
#endif
	},
	{ { 0x7a, 0xe1, 0xcc, 0x13, 0x5d, 0x65, 0xae, 0x61, 0x7e, 0xe5, 0xea, 0xb8, 0x82, 0xe0, 0x58, 0xd6, 0xea, 0x3c, 0x09, 0xc3, 0x00 }
#if FW_SINCE(0x03650000)
	  , 13, 0x1U, 0xfc66526dU
#endif
	},
	{ { 0xa3, 0x5c, 0x7d, 0xc1, 0x75, 0xd2, 0x15, 0xa7, 0xfa, 0x90, 0xc9, 0xe7, 0xa7, 0x86, 0xe7, 0xa1, 0x79, 0xeb, 0xf8, 0x91, 0x00 }
#if FW_SINCE(0x03650000)
	  , 13, 0x2U, 0xf98d64e8U
#endif
	},
	{ { 0x2f, 0x29, 0x1f, 0x9c, 0xac, 0x5b, 0xe0, 0xa1, 0x71, 0x36, 0x98, 0xc9, 0xd3, 0x80, 0x76, 0x01, 0x16, 0xb2, 0xae, 0x7f, 0x00 }
#if FW_SINCE(0x03650000)
	  , 13, 0x4U, 0xe3f30ef2U
#endif
	},
	{ { 0x4e, 0xa6, 0xa5, 0xca, 0x73, 0xab, 0x33, 0x57, 0x47, 0x7d, 0x1e, 0xff, 0x8d, 0xa1, 0x71, 0x71, 0x3e, 0xcd, 0xab, 0x70, 0x00 }
#if FW_SINCE(0x03650000)
	  , 3, 0x8U, 0x21e782f7U
#endif
	},
	{ { 0xe6, 0xa2, 0x30, 0x9e, 0xa2, 0xae, 0x13, 0xcd, 0xd2, 0x07, 0x88, 0x4c, 0x65, 0xf3, 0x6b, 0x6f, 0xe0, 0x80, 0x35, 0x3a, 0x00 }
#if FW_SINCE(0x03650000)
	  , 14, 0x1U, 0x168e9835U
#endif
	},
	{ { 0xbe, 0x80, 0xd2, 0x95, 0xf8, 0x23, 0x91, 0x4f, 0x7d, 0x79, 0x48, 0x4d, 0xdf, 0x16, 0xa1, 0x3f, 0xf5, 0xf9, 0x9f, 0x98, 0x00 }
#if FW_SINCE(0x03650000)
	  , 15, 0x1U, 0x1338f9f4U
#endif
	},
	{ { 0xf2, 0x68, 0x1f, 0x85, 0x06, 0x12, 0x72, 0xa4, 0xaf, 0xca, 0x7f, 0x1f, 0x69, 0x24, 0xe0, 0x85, 0xd6, 0x0e, 0xa7, 0xe6, 0x00 }
#if FW_SINCE(0x03650000)
	  , 15, 0x2U, 0x9160ccc3U
#endif
	},
	{ { 0x56, 0x48, 0x0a, 0x76, 0xe6, 0xdd, 0x4e, 0x41, 0x15, 0x1e, 0xf9, 0x6c, 0x9d, 0x14, 0x1b, 0x2e, 0x29, 0x9d, 0xcb, 0xbf, 0x00 }
#if FW_SINCE(0x03650000)
	  , 15, 0x4U, 0x23e0cd27U
#endif
	},
#if FW_SINCE(0x03650000)
	{ { 0x09, 0x8f, 0xe9, 0x30, 0xc8, 0x01, 0xe1, 0xf8, 0x76, 0x69, 0x85, 0x2b, 0x6c, 0xd5, 0x3f, 0x86, 0x58, 0x21, 0x5f, 0x8a, 0x00 }
	  , 6, 0x4U, 0x7f689cf5U
	},
	{ { 0x07, 0xb3, 0x32, 0xb7, 0xb2, 0x70, 0xa5, 0xf8, 0x03, 0xeb, 0xea, 0x49, 0x8e, 0x3f, 0x53, 0x41, 0x47, 0xfe, 0xee, 0x96, 0x00 }
	  , 17, 0x1U, 0xedae8879U
	},
	{ { 0x2e, 0xea, 0xe0, 0x13, 0x8a, 0x8f, 0x9f, 0xc2, 0x9e, 0x7e, 0x65, 0xd0, 0x5a, 0x11, 0x5b, 0x91, 0xa3, 0x63, 0x76, 0x4a, 0x00 }
	  , 16, 0x1U, 0x704ec523U
	},
	{ { 0xca, 0x9f, 0x39, 0xe3, 0xec, 0x76, 0x44, 0xb4, 0x75, 0x35, 0x28, 0xe2, 0x86, 0x44, 0x15, 0x00, 0x82, 0xd2, 0x0b, 0x53, 0x00 }
	  , 4, 0x8U, 0xfc2f48b2U
	},
	{ { 0x3b, 0x41, 0xe7, 0x7e, 0x48, 0x98, 0xa9, 0xb4, 0x52, 0xb9, 0x05, 0x22, 0xfd, 0x37, 0xae, 0xcd, 0x50, 0x90, 0x85, 0xea, 0x00 }
	  , 3, 0x10U, 0x148364e6U
	},
	{ { 0xb4, 0x9b, 0x76, 0x02, 0x55, 0x11, 0xee, 0x6b, 0x4a, 0x94, 0x92, 0xe2, 0x2c, 0xce, 0x07, 0x78, 0xfe, 0xf8, 0x59, 0xb3, 0x00 }
	  , 3, 0x20U, 0x4978107aU
	},
	{ { 0x28, 0x3a, 0x97, 0xd7, 0xc7, 0xb9, 0x5f, 0xe9, 0xe8, 0x70, 0xdc, 0xf4, 0x29, 0x54, 0xad, 0x78, 0x37, 0xa4, 0xef, 0xc6, 0x00 }
	  , 2, 0x20U, 0x6b1715a2U
	},
	{ { 0xd7, 0xaf, 0x81, 0x08, 0x23, 0xb8, 0x45, 0xc3, 0xd7, 0x7a, 0x16, 0x7f, 0x56, 0xb4, 0x7d, 0x4a, 0x75, 0x7b, 0x82, 0xca, 0x00 }
	  , 18, 0x1U, 0x4d68f46fU
	},
#endif
};

static __attribute__((noinline)) void *_sceSslMalloc(SceUInt32 size)
{
	return sceClibMspaceMalloc(s_sslMspace, size);
}

static __attribute__((noinline)) void _sceSslFree(void *ptr)
{
	sceClibMspaceFree(s_sslMspace, ptr);
}

static void *_sceSslRealloc(void *ptr, SceUInt32 size, SceUInt32 oldSize)
{
	(void)oldSize;
	return sceClibMspaceRealloc(s_sslMspace, ptr, size);
}

static SslTime *_sceSslGetCurrentTime(SslTime *time)
{
	SceDateTime dt;
	SceUInt64 sec;

	if (time != NULL) {
		if (sceRtcGetCurrentClock(&dt, 0) < 0 ||
		    sceRtcGetTime64_t(&dt, &sec) < 0) {
			sec = (SceUInt64)sceKernelGetSystemTimeWide();
			time->secondsHigh = (SceInt32)(sec >> 32);
			time->secondsLow = (SceUInt32)sec;
		} else {
			time->secondsHigh = (SceInt32)(sec >> 32);
			time->secondsLow = (SceUInt32)sec;
			time->milliseconds = 0;
		}
	}
	return time;
}

static int _sceSslLockCallback(int mode, int index, const char *file, int line)
{
	int ret;

	(void)index;
	(void)file;
	(void)line;
	if ((mode & 1) != 0) {
		ret = sceKernelLockLwMutex(&s_sslMutex, 1, NULL);
		if (ret >= 0)
			return 1;
	} else {
		ret = sceKernelUnlockLwMutex(&s_sslMutex, 1);
		if (ret >= 0)
			return 1;
	}
	return ret;
}

static SceUInt32 _sceSslThreadIdCallback(int arg)
{
	int id;

	(void)arg;
	id = sceKernelGetThreadId();
	return id < 0 ? 0 : (SceUInt32)id;
}

void SceSslInternal_C0B135F0(void)
{
}

void SceSslInternal_56FE376A(void)
{
}

int sceSslInit(SceSize poolSize)
{
	int ret;
	SceUID memid;
	void *base;
	void *mspace;
	SslLibraryModule *modules;

	if (s_sslInitialized == 1)
		return SCE_SSL_ERROR_ALREADY_INITED;
	if (poolSize == 0)
		return SCE_SSL_ERROR_INVALID_VALUE;

	memid = sceKernelAllocMemBlock("SceLibsslGlobalHeap",
				       SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
				       poolSize, NULL);
	if (memid < 0) {
		ret = memid;
		goto err_memblock;
	}
	ret = sceKernelGetMemBlockBase(memid, &base);
	if (ret < 0)
		goto err_memblock;
	mspace = sceClibMspaceCreate(base, poolSize);
	if (mspace == NULL) {
		ret = SCE_SSL_ERROR_OUT_OF_MEMORY;
		goto err_memblock;
	}
	s_sslMemBlockId = memid;
	s_sslMspace = mspace;

	ret = sceKernelCreateLwMutex(&s_sslMutex, "SceSsl", 2, 0, NULL);
	if (ret < 0)
		goto err_mspace;
	s_sslMutexCreated = 1;

	if (sslSetThreadCallback(_sceSslThreadIdCallback) <= 0 ||
	    sslSetLockCallback(_sceSslLockCallback) <= 0) {
		ret = SCE_SSL_ERROR_INTERNAL;
		goto err_mspace;
	}
	SceSslInternal_C1299FB6(_sceSslMalloc, _sceSslRealloc, _sceSslFree);
	modules = SceSslInternal_FB51AE89();
	if (SceSslInternal_09657E15(modules, 0, &s_sslLibrary) != 0 ||
	    (sslSetTimeCurrentCallback(_sceSslGetCurrentTime),
	     sslGetTimeCurrentCallback() == NULL)) {
		ret = SCE_SSL_ERROR_OUT_OF_MEMORY;
		goto err_mspace;
	}
	s_sslInitialized = 1;
	return 0;

err_mspace:
	sceClibMspaceDestroy(mspace);
	s_sslMspace = NULL;
err_memblock:
	if (memid != -1) {
		sceKernelFreeMemBlock(memid);
		s_sslMemBlockId = -1;
	}
	return ret;
}

__attribute__((noinline)) int sceSslInternalIsInitalized(void)
{
	if (s_sslInitialized != 0)
		return 0;
	return SCE_SSL_ERROR_BEFORE_INIT;
}

int SceSslInternal_33737FDC(void)
{
	return 0;
}

int sceSslTerm(void)
{
	int ret;

	if (s_sslInitialized == 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (s_sslLibrary != NULL) {
		SceSslInternal_4725F9E7(s_sslLibrary);
		s_sslLibrary = NULL;
	}
	if (s_sslMspace != NULL) {
		sceClibMspaceIsHeapEmpty(s_sslMspace);
		ret = sceClibMspaceDestroy(s_sslMspace);
		if (ret < 0)
			return ret;
		s_sslMspace = NULL;
	}
	if (s_sslMemBlockId != -1) {
		sceKernelFreeMemBlock(s_sslMemBlockId);
		s_sslMemBlockId = -1;
	}
	if (s_sslMutexCreated == 1) {
		sceKernelDeleteLwMutex(&s_sslMutex);
		s_sslMutexCreated = 0;
	}
	SceSslInternal_DEC6DD27();
	s_sslInitialized = 0;
	return 0;
}

__attribute__((noinline)) int SceSslInternal_2185BB05(void **lib)
{
	if (s_sslInitialized == 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	*lib = s_sslLibrary;
	return 0;
}

int sceSslGetMemoryPoolStats(SceSslMemoryPoolStats *currentStat)
{
	struct MspaceStats stats;
	int ret;

	if (s_sslInitialized == 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (currentStat == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;
	ret = sceClibMspaceMallocStats(s_sslMspace, &stats);
	if (ret >= 0) {
		currentStat->poolSize = stats.capacity;
		currentStat->maxInuseSize = stats.peakInUse;
		currentStat->currentInuseSize = stats.currentInUse;
	}
	return ret;
}

#if FW_SINCE(0x03650000)
int SceSslInternal_3746501F(void *store, const char ***pem, void *buf, SceSize size,
			    SceSize *outSize, SceBool systemOnly)
{
	return _sceSslGetCaCertBySubjectHashes(store, pem, (char *)buf, size,
					       outSize, systemOnly);
}
#endif

int sceSslInternalGetCertificateAuthority(int store, int index, const char ***pem,
					  void *buf, SceSize size, SceSize *outSize)
{
	return _sceSslGetCaCert(store, index, pem, (char *)buf, size, outSize);
}

int SceSslInternal_349B0E73(int store, int mask, const void **serials, int *count)
{
	return _sceSslGetRevokedSerials(store, mask, serials, count);
}

unsigned int SceSslInternal_422DFE70(void)
{
	return sslGetError();
}

__attribute__((noinline)) int SceSslInternal_461C5990(int library, int error)
{
	if (library == 0) {
		switch (error) {
		case 0:
			return 0;
		case 10001:
		case 10002:
		case 10003:
		case 10004:
		case 10006:
		case 10007:
		case 10008:
		case 10009:
		case 10010:
		case 10011:
		case 10012:
		case 10013:
		case 10014:
		case 10015:
		case 10016:
		case 10017:
		case 10018:
		case 10019:
		case 10020:
		case 10021:
		case 10022:
		case 10023:
		case 10025:
		case 10026:
		case 10028:
		case 10029:
		case 10030:
		case 10031:
		case 10032:
		case 10033:
		case 10034:
		case 10035:
		case 10036:
		case 10037:
			return error + (int)0x80432af0U;
		case 10005:
			return SCE_SSL_ERROR_OUT_OF_MEMORY;
		default:
			return (int)0x804352ffU;
		}
	}
	if (library == 1) {
		if ((sslGetError() & 0xfffU) == 0x21U)
			return SCE_SSL_ERROR_OUT_OF_MEMORY;
		return error;
	}
	return SCE_SSL_ERROR_INVALID_VALUE;
}

__attribute__((noinline)) void _sceSslCertDebugLog(const void *data, SceSize size)
{
	(void)data;
	(void)size;
}

static int _sceSslCaStoreMatchAll(int store)
{
	return store == 0;
}

static int _sceSslCaStoreCheckMask(int store, int mask, int allowedMask)
{
	(void)store;
	return (mask & ~allowedMask) == 0;
}

static int _sceSslLoadCaCert(int store, int mask, const char ***pem, char *buf,
			     SceSize size, SceSize *outSize, int doRead)
{
	char mountPoint[64];
	SceUChar8 digest[20];
	void *library;
	void *context;
	SslCryptoAlgorithm *algorithm;
	SceUInt32 digestLen;
	SceUID fd;
	int ret;
	int count;
	SceSize usedSize;
	char *head;
	char *tail;

	ret = sceAppMgrConvertVs0UserDrivePath("vs0:/data/external/cert/CA_LIST.cer",
					       mountPoint, sizeof(mountPoint));
	if (ret < 0)
		return ret;
	ret = sceIoOpen(mountPoint, SCE_O_RDONLY, 0);
	if (ret < 0)
		return ret;
	fd = ret;

	count = 1;
	usedSize = 0;
	head = NULL;
	tail = NULL;

	{
		int res;

		for (;;) {
			SceSize certSize;
			SceOff certOff;
			int certIdx;
			SceSize pad;
			SceSize strSize;
			SceSize alignedStr;
			SceSize entryTotal;
			char *node;
			int r;

			res = count - 1;

#define CA_BRANCH(bitMask, idx, sz, off) \
		if ((mask & (bitMask)) != 0) { \
			mask ^= (bitMask); \
			certIdx = (idx); \
			certSize = (sz); \
			certOff = (off); \
			strSize = (sz) + 1; \
			pad = 4 - (((sz) + 1) & 3); \
			alignedStr = (sz) + pad; \
			entryTotal = (sz) + pad + 9; \
			goto load_entry; \
		}

		switch (store) {
		case 1:
			CA_BRANCH(0x01, 0, 1410, 20)
			CA_BRANCH(0x02, 1, 1410, 1452)
			CA_BRANCH(0x04, 2, 1410, 2884)
			CA_BRANCH(0x08, 3, 1410, 4316)
			CA_BRANCH(0x10, 4, 1410, 5748)
			goto done_loop;
		case 2:
			CA_BRANCH(0x01, 19, 848, 26053)
			CA_BRANCH(0x02, 20, 1122, 26959)
			CA_BRANCH(0x04, 21, 1508, 28147)
			CA_BRANCH(0x08, 22, 1760, 29721)
			CA_BRANCH(0x10, 23, 1728, 31534)
#if FW_SINCE(0x03650000)
			CA_BRANCH(0x20, 56, 1508, 80300)
#endif
			goto done_loop;
		case 3:
			CA_BRANCH(0x01, 8, 1236, 10797)
			CA_BRANCH(0x02, 9, 1162, 12064)
			CA_BRANCH(0x04, 10, 948, 13269)
			CA_BRANCH(0x08, 45, 964, 65222)
#if FW_SINCE(0x03650000)
			CA_BRANCH(0x10, 54, 1490, 77431)
			CA_BRANCH(0x20, 55, 1290, 78966)
#endif
			goto done_loop;
		case 4:
			CA_BRANCH(0x01, 17, 1196, 23606)
			CA_BRANCH(0x02, 18, 1166, 24823)
			CA_BRANCH(0x04, 38, 1518, 54009)
#if FW_SINCE(0x03650000)
			CA_BRANCH(0x08, 53, 1530, 75851)
#endif
			goto done_loop;
		case 5:
			CA_BRANCH(0x01, 24, 1542, 33306)
			CA_BRANCH(0x02, 25, 1546, 34878)
			CA_BRANCH(0x04, 26, 1632, 36454)
			goto done_loop;
		case 6:
			CA_BRANCH(0x01, 15, 1282, 20883)
			CA_BRANCH(0x02, 16, 1376, 22192)
			CA_BRANCH(0x04, 50, 1250, 72057)
			goto done_loop;
		case 7:
			CA_BRANCH(0x01, 6, 1282, 8111)
			CA_BRANCH(0x02, 5, 890, 7190)
			CA_BRANCH(0x04, 7, 1340, 9421)
			goto done_loop;
		case 8:
			CA_BRANCH(0x01, 11, 1772, 14257)
			CA_BRANCH(0x02, 12, 1522, 16051)
			CA_BRANCH(0x04, 13, 1670, 17593)
			CA_BRANCH(0x08, 14, 1558, 19288)
			goto done_loop;
		case 9:
			CA_BRANCH(0x01, 27, 1390, 38139)
			CA_BRANCH(0x02, 28, 1372, 39562)
			CA_BRANCH(0x04, 29, 1360, 40963)
			goto done_loop;
		case 10:
			CA_BRANCH(0x01, 30, 1472, 42397)
			CA_BRANCH(0x02, 31, 1390, 43936)
			CA_BRANCH(0x04, 32, 1492, 45389)
			CA_BRANCH(0x08, 33, 1422, 46949)
			CA_BRANCH(0x10, 34, 1646, 48411)
			CA_BRANCH(0x20, 35, 1448, 50102)
			CA_BRANCH(0x40, 41, 1084, 60357)
			goto done_loop;
		case 11:
			CA_BRANCH(0x01, 36, 1084, 51606)
			CA_BRANCH(0x02, 37, 1254, 52715)
			goto done_loop;
		case 12:
			CA_BRANCH(0x01, 39, 2714, 55578)
			CA_BRANCH(0x02, 40, 1958, 58333)
			goto done_loop;
		case 13:
			CA_BRANCH(0x01, 42, 1084, 61500)
			CA_BRANCH(0x02, 43, 1244, 62619)
			CA_BRANCH(0x04, 44, 1282, 63898)
			goto done_loop;
		case 14:
			CA_BRANCH(0x01, 46, 1372, 66215)
			goto done_loop;
		case 15:
			CA_BRANCH(0x01, 47, 1224, 67614)
			CA_BRANCH(0x02, 48, 1224, 68865)
			CA_BRANCH(0x04, 49, 1922, 70113)
			goto done_loop;
#if FW_SINCE(0x03650000)
		case 16:
			CA_BRANCH(0x01, 52, 1320, 74499)
			goto done_loop;
		case 17:
			CA_BRANCH(0x01, 51, 1138, 73329)
			goto done_loop;
		case 18:
			CA_BRANCH(0x01, 57, 1220, 81837)
			goto done_loop;
#endif
		default:
			ret = SCE_ERROR_ERRNO_ENOTSUP;
			goto out_close;
		}
#undef CA_BRANCH

load_entry:
		if (sceIoLseek(fd, certOff, SCE_SEEK_SET) < 0)
			goto out_close;
		if (doRead == 1) {
			if (entryTotal > size - usedSize) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto out_close;
			}
			ret = sceIoRead(fd, buf, certSize);
			if (ret < 0)
				goto out_close;
			buf[certSize] = '\0';
			library = NULL;
			context = NULL;
			algorithm = NULL;
			digestLen = sizeof(digest);
			ret = SceSslInternal_2185BB05(&library);
			if (ret >= 0) {
				r = sslCreateCryptoContext((SslLibrary *)library, 0, &context);
				if (r != 0) {
					ret = SceSslInternal_461C5990(0, r);
				} else {
					r = sslCreateAlgorithm(
						(SslCryptoContextHeader *)context,
						3, 64, 0, &algorithm);
					if (r != 0) {
						ret = SceSslInternal_461C5990(0, r);
					} else {
						r = sslDigestInit(algorithm);
						if (r != 0) {
							ret = SceSslInternal_461C5990(0, r);
						} else {
							ret = sslDigest(algorithm, buf,
									certSize, digest,
									&digestLen);
							if (ret != 0)
								ret = SceSslInternal_461C5990(0, ret);
						}
					}
				}
			}
			if (algorithm != NULL)
				sslDestroyAlgorithm(algorithm);
			if (context != NULL)
				sslDestroyCryptoContext(context);
			if (ret < 0)
				goto out_close;
			_sceSslCertDebugLog(digest, sizeof(digest));
			if (sslCompareMemory(digest, s_caCertTable[certIdx].sha1,
					     sizeof(digest)) != 0) {
				ret = SCE_SSL_ERROR_INVALID_VALUE;
				goto out_close;
			}
			node = &buf[strSize + pad];
			usedSize = usedSize + alignedStr;
			if (tail != NULL)
				*((char **)tail + 1) = node;
			else
				head = node;
			tail = node;
			{
				char *p = node;

				p[3] = 0;
				p[4] = 0;
				p[5] = 0;
				p[6] = 0;
				p[7] = 0;
			}
			*(char **)node = buf;
			buf = node + 8;
		} else {
			usedSize = usedSize + certSize + pad;
		}
		usedSize += 9;
		count++;
	}

done_loop:
	if (doRead == 1) {
		char *cur = (char *)*pem;

		if (cur != NULL) {
			while (*((char **)cur + 1) != NULL)
				cur = *((char **)cur + 1);
			*((char **)cur + 1) = head;
		} else {
			*pem = (const char **)head;
		}
	}
	ret = res;
	}
	if (outSize != NULL)
		*outSize += usedSize;

out_close:
	if (fd != 0)
		sceIoClose(fd);
	return ret;
}

static int _sceSslCaStoreFilterMask(int store, int mask)
{
	if (store == 0) {
		SceLibKernel_9F793F84();
		return 0;
	}
	return mask;
}

static int _sceSslGetCaCert(int store, int mask, const char ***pem, char *buf,
			    SceSize size, SceSize *outSize)
{
	SceUInt32 sdkVer;
	SceUInt32 maxStore;
	int doRead;
	SceSslCaStoreEntry *table;
	SceSslCaStoreEntry *entry;
	SceSize totalSize;
	int totalCount;
	SceSize curSize;
	int ret;

	curSize = 0;
	sdkVer = SceLibKernel_9F793F84();
	if (buf != NULL && pem != NULL) {
		doRead = 1;
		*pem = NULL;
	} else {
		doRead = 0;
	}
	if (outSize != NULL)
		*outSize = 0;

#if FW_SINCE(0x03650000)
	if (sdkVer > 0x0364ffffU)
		maxStore = 18;
	else
#endif
	if (sdkVer >= 0x03600000U)
		maxStore = 15;
	else if (sdkVer >= 0x03000000U)
		maxStore = 14;
	else if (sdkVer >= 0x01800000U)
		maxStore = 13;
	else
		maxStore = 11;

	if ((SceUInt32)store > maxStore)
		return SCE_ERROR_ERRNO_ENOTSUP;

	if (store != 0 && mask == -1) {
		SceUInt32 ver = SceLibKernel_9F793F84();

		switch (store) {
		case 1:
			mask = 0x1f;
			break;
#if FW_SINCE(0x03650000)
		case 2:
			if (ver > 0x0364ffffU)
				mask = 0x3f;
			else
				mask = 0x1f;
			break;
#endif
		case 3:
			if (ver >= 0x02100000U)
				mask = 0xf;
			else
				mask = 0x7;
#if FW_SINCE(0x03650000)
			if (ver > 0x0364ffffU)
				mask = 0x3f;
#endif
			break;
		case 4:
			if (ver < 0x01800000U)
				mask = 0x3;
			else
				mask = 0x7;
#if FW_SINCE(0x03650000)
			if (ver > 0x0364ffffU)
				mask = 0xf;
#endif
			break;
		case 6:
			if (ver <= 0x0362ffffU)
				mask = 0x3;
			else
				mask = 0x7;
			break;
		case 10:
			if (ver >= 0x01800000U)
				mask = 0x7f;
			else
				mask = 0x3f;
			break;
		default:
			mask = 0;
			break;
		}
	}
#if FW_SINCE(0x03650000)
	if (sdkVer > 0x0364ffffU)
		table = s_caStores365;
	else
#endif
	if (sdkVer >= 0x03600000U)
		table = s_caStores360;
	else if (sdkVer >= 0x03000000U)
		table = s_caStores300;
	else if (sdkVer >= 0x01800000U)
		table = s_caStores180;
	else
		table = s_caStores100;

	if (store != 0) {
		for (entry = table; entry->store != -1; entry++) {
			if (entry->store == store)
				break;
		}
		if (entry->store == -1 ||
		    entry->checkMask(store, mask, entry->allowedMask) != 1)
			return SCE_ERROR_ERRNO_ENOTSUP;
	}

	totalSize = 0;
	totalCount = 0;
	for (entry = table; entry->store != -1; entry++) {
		int targetStore;

		if (store == entry->store) {
			targetStore = store;
		} else {
			if (entry->matchAll(store) == 0)
				goto next_entry;
			targetStore = entry->store;
		}
		ret = _sceSslLoadCaCert(targetStore, mask, pem, buf,
					size - totalSize, &curSize, doRead);
		if (ret < 0)
			return ret;
		totalCount += ret;
		buf = (char *)((SceUIntPtr)buf + curSize);
		totalSize += curSize;
		curSize = 0;
next_entry:
		__builtin_prefetch((const void *)((SceUIntPtr)entry + 5 * sizeof(*entry)));
	}
	if (outSize != NULL)
		*outSize = totalSize;
	return totalCount;
}

#if FW_SINCE(0x03650000)
static int _sceSslGetCaCertBySubjectHashes(void *certName, const char ***pem,
					   char *buf, SceSize size,
					   SceSize *outSize, SceBool systemOnly)
{
	int hasPem;
	int doRead;
	SceUInt32 hash;
	int err;
	int i;
	int ret;

	hasPem = pem != NULL ? 1 : 0;
	if (buf != NULL && hasPem != 0) {
		doRead = 1;
		*pem = NULL;
	} else {
		doRead = 0;
	}
	if (outSize != NULL)
		*outSize = 0;

	err = SceSslInternal_91C2531C(certName, 80, &hash);
	if (err != 0)
		return SceSslInternal_461C5990(0, err);

	for (i = 0; i < 58; i++) {
		if (hash == s_caCertTable[i].subjectHash)
			break;
	}
	if (i != 58 && (systemOnly == 0 || s_caCertTable[i].store == 1)) {
		ret = _sceSslLoadCaCert(s_caCertTable[i].store,
					(int)s_caCertTable[i].bit,
					pem, buf, size, outSize, doRead);
		if (ret != 0)
			return ret;
	}
	if (hasPem != 0)
		*pem = NULL;
	return 0;
}
#endif

static int _sceSslGetRevokedSerials(int store, int mask, const void **serials,
				    int *count)
{
	if (serials == NULL || count == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;
	if (store == 5 && (mask & 4) != 0) {
		*serials = s_revokedSerials;
		*count = 10;
	} else {
		*serials = NULL;
		*count = 0;
	}
	return 0;
}

int sceSslGetSerialNumber(SceSslCert *sslCert, const SceUChar8 **sboData,
			  SceSize *sboLen)
{
	SceSslDataItem item;
	int ret;

	if (sceSslInternalIsInitalized() < 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (sslCert == NULL || sboData == NULL || sboLen == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;
	ret = SceSslInternal_64BDF1BB(sslCert, 2, &item);
	if (ret != 0) {
		if (ret == SSL_INTERNAL_ERROR_NOT_FOUND)
			return SCE_SSL_ERROR_NOT_FOUND;
		return SCE_SSL_ERROR_INVALID_FORMAT;
	}
	*sboData = item.data;
	*sboLen = item.len;
	return 0;
}

SceSslCertName *sceSslGetSubjectName(SceSslCert *sslCert)
{
	void *name;

	if (sceSslInternalIsInitalized() < 0)
		return NULL;
	if (sceSslInternalIsInitalized() < 0)
		return NULL;
	if (sslCert == NULL)
		return NULL;
	if (SceSslInternal_08179EBD(sslCert, 1, &name) != 0)
		return NULL;
	return (SceSslCertName *)name;
}

SceSslCertName *sceSslGetIssuerName(SceSslCert *sslCert)
{
	void *name;

	if (sceSslInternalIsInitalized() < 0)
		return NULL;
	if (sceSslInternalIsInitalized() < 0)
		return NULL;
	if (sslCert == NULL)
		return NULL;
	if (SceSslInternal_81FA5DBE(sslCert, 1, &name) != 0)
		return NULL;
	return (SceSslCertName *)name;
}

int sceSslFreeSslCertName(SceSslCertName *certName)
{
	if (sceSslInternalIsInitalized() < 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (certName == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;
	if (SceSslInternal_CE37B69E(certName) != 0)
		return SCE_SSL_ERROR_INTERNAL;
	return 0;
}

int sceSslGetNotBefore(SceSslCert *sslCert, SceRtcTick *begin)
{
	void *library = NULL;
	void *context = NULL;
	void *date = NULL;
	SceUInt32 length;
	SceUInt64 sec;
	SceUChar8 raw[8];
	SceDateTime dt;
	int ret;

	if (sceSslInternalIsInitalized() < 0) {
		ret = SCE_SSL_ERROR_BEFORE_INIT;
	} else if (sslCert == NULL || begin == NULL) {
		ret = SCE_SSL_ERROR_INVALID_VALUE;
	} else {
		ret = SceSslInternal_2185BB05(&library);
		if (ret >= 0) {
			if (SceSslInternal_D3C049B9(library, 0, &context) != 0 ||
			    SceSslInternal_5E50CB55(context, &date) != 0 ||
			    SceSslInternal_2464F9D0(sslCert, date) != 0 ||
			    SceSslInternal_D1A82B1A(date, 3, raw, &length,
						    sizeof(raw)) != 0 ||
			    length != sizeof(raw)) {
				ret = SCE_SSL_ERROR_INTERNAL;
			} else {
				union {
					SceUInt64 u64;
					SceUChar8 bytes[8];
				} val;
				SceUChar8 t;

				__builtin_memcpy(&sec, raw, sizeof(sec));
				val.u64 = sec;
				t = val.bytes[0];
				val.bytes[0] = val.bytes[7];
				val.bytes[7] = t;
				t = val.bytes[1];
				val.bytes[1] = val.bytes[6];
				val.bytes[6] = t;
				t = val.bytes[2];
				val.bytes[2] = val.bytes[5];
				val.bytes[5] = t;
				t = val.bytes[3];
				val.bytes[3] = val.bytes[4];
				val.bytes[4] = t;
				sec = val.u64;
				ret = sceRtcSetTime64_t(&dt, val.u64);
				if (ret >= 0) {
					ret = sceRtcGetTick(&dt, begin);
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}
	if (date != NULL) {
		SceSslInternal_6CDB4EE8(date);
		date = NULL;
	}
	if (context != NULL)
		SceSslInternal_D4ADF069(context);
	return ret;
}

int sceSslGetNotAfter(SceSslCert *sslCert, SceRtcTick *limit)
{
	void *library = NULL;
	void *context = NULL;
	void *date = NULL;
	SceUInt32 length;
	SceUInt64 sec;
	SceUChar8 raw[8];
	SceDateTime dt;
	int ret;

	if (sceSslInternalIsInitalized() < 0) {
		ret = SCE_SSL_ERROR_BEFORE_INIT;
	} else if (sslCert == NULL || limit == NULL) {
		ret = SCE_SSL_ERROR_INVALID_VALUE;
	} else {
		ret = SceSslInternal_2185BB05(&library);
		if (ret >= 0) {
			if (SceSslInternal_D3C049B9(library, 0, &context) != 0 ||
			    SceSslInternal_5E50CB55(context, &date) != 0 ||
			    SceSslInternal_2074A6EB(sslCert, date) != 0 ||
			    SceSslInternal_D1A82B1A(date, 3, raw, &length,
						    sizeof(raw)) != 0 ||
			    length != sizeof(raw)) {
				ret = SCE_SSL_ERROR_INTERNAL;
			} else {
				union {
					SceUInt64 u64;
					SceUChar8 bytes[8];
				} val;
				SceUChar8 t;

				__builtin_memcpy(&sec, raw, sizeof(sec));
				val.u64 = sec;
				t = val.bytes[0];
				val.bytes[0] = val.bytes[7];
				val.bytes[7] = t;
				t = val.bytes[1];
				val.bytes[1] = val.bytes[6];
				val.bytes[6] = t;
				t = val.bytes[2];
				val.bytes[2] = val.bytes[5];
				val.bytes[5] = t;
				t = val.bytes[3];
				val.bytes[3] = val.bytes[4];
				val.bytes[4] = t;
				sec = val.u64;
				ret = sceRtcSetTime64_t(&dt, val.u64);
				if (ret >= 0) {
					ret = sceRtcGetTick(&dt, limit);
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}
	if (date != NULL) {
		SceSslInternal_6CDB4EE8(date);
		date = NULL;
	}
	if (context != NULL)
		SceSslInternal_D4ADF069(context);
	return ret;
}

int sceSslGetNameEntryCount(SceSslCertName *certName)
{
	int count = 0;

	if (sceSslInternalIsInitalized() < 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (certName == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;
	if (SceSslInternal_91C2531C(certName, 10, &count) != 0)
		return SCE_SSL_ERROR_INVALID_FORMAT;
	return count;
}

int sceSslGetNameEntryInfo(SceSslCertName *certName, SceInt32 entryNum,
			   char *oidname, SceSize maxOidnameLen,
			   char *value, SceSize maxValueLen,
			   SceSize *valueLen)
{
	SceSslAvaPart typePart;
	SceSslAvaPart valPart;
	SceSslNameEntry entry;
	SceUChar8 oid[16];
	void *ava;
	SceSize copyLen;
	int ret;

	if (sceSslInternalIsInitalized() < 0)
		return SCE_SSL_ERROR_BEFORE_INIT;
	if (certName == NULL || oidname == NULL || value == NULL || valueLen == NULL)
		return SCE_SSL_ERROR_INVALID_VALUE;

	entry.index = entryNum;
	if (SceSslInternal_91C2531C(certName, 11, &entry) != 0)
		return SCE_SSL_ERROR_INVALID_FORMAT;

	ava = entry.ava;
	if (SceSslInternal_B5A099D2(ava, 1, &typePart) != 0 ||
	    SceSslInternal_B5A099D2(ava, 2, &valPart) != 0) {
		ret = SCE_SSL_ERROR_INVALID_FORMAT;
	} else {
		ret = SceSslInternal_4D80D3F7(oid, 1, typePart.data, typePart.len);
		if (ret != 0) {
			ret = SCE_SSL_ERROR_OUT_OF_MEMORY;
		} else {
			SceSslInternal_3DE4F3DB(oid, 0, maxOidnameLen, oidname);
			copyLen = valPart.len;
			*valueLen = copyLen;
			if (copyLen > maxValueLen)
				copyLen = maxValueLen;
			sslCopyMemory(value, valPart.data, copyLen);
		}
	}
	if (ava != NULL)
		SceSslInternal_17BF2367(ava);
	return ret;
}

void _sceSslQuickSort(char *base, SceSize size,
		      int (*compare)(const void *, const void *),
		      int left, int right)
{
	void *pivot;
	void *temp;
	int i;
	int j;

	pivot = _sceSslMalloc(size);
	if (pivot == NULL)
		return;
	temp = _sceSslMalloc(size);
	if (temp == NULL) {
		_sceSslFree(pivot);
		return;
	}
	i = left;
	j = right;
	sceClibMemcpy_safe(pivot, base + ((left + right) / 2) * size, size);
	for (;;) {
		while (compare(base + i * size, pivot) < 0)
			i++;
		while (compare(pivot, base + j * size) < 0)
			j--;
		if (i > j)
			break;
		if (i != j) {
			sceClibMemcpy_safe(temp, base + i * size, size);
			sceClibMemcpy_safe(base + i * size, base + j * size, size);
			sceClibMemcpy_safe(base + j * size, temp, size);
		}
		i++;
		j--;
		if (i > j)
			break;
	}
	if (left < j)
		_sceSslQuickSort(base, size, compare, left, j);
	if (i < right)
		_sceSslQuickSort(base, size, compare, i, right);
	_sceSslFree(pivot);
	_sceSslFree(temp);
}
