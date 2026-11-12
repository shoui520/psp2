/* Filename and private descriptive names are ours; library default module and reset glue. */
#include "lib_ctx.h"
#include "../key/pkey_ctx.h"
#include "../key/pkey_pk.h"
#include "../module/crypto/ctx.h"
#include "../module/dmem/dmem.h"
#include "../../../date.h"
#include "../../../MES/common/cert/src/ri_cert.h"
#include "../../../MES/common/module/crt_stor/src/small_stor.h"
#include "../../../pem/pem.h"
#include "../../../runtime.h"
#include "../../../err/err.h"

void sslResetTempRsaKeys(void);
int sslResetWeakCipherFlags(void);
void sslResetExDataAndRandomGlobals(void);
int sslLibraryModuleInit(void *ctx, int flag);
int sslLibraryModuleCleanup(void *ctx, int flag);

extern const SslCryptoImplementationMethods *sslGetCipherBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetDigestBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetRsaCipherBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetRsaSignBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetDigestSignBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetHmacBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetPrngBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetBasePrngBackendMethods(void);
extern const SslCryptoImplementationMethods *sslGetFips186PrngBackendMethods(void);

extern int sslDesEde3CbcBackendData[];
extern int sslRc4BackendData[];
extern int sslAesCbcBackendData[];
extern int sslCamelliaCbcBackendData[];
extern int sslMd5BackendData[];
extern int sslSha1BackendData[];
extern int sslSha224BackendData[];
extern int sslSha256BackendData[];
extern int sslSha384BackendData[];
extern int sslSha512BackendData[];
extern const int sslRsaPkcs1BackendData[];
extern const int sslRsaRawBackendData[];
extern const int sslRsaSignBackendData[];

static SslKeyModule sslPublicKeyModule = {
	"pk",
	{ 0xffffffffu, 1u },
	sslInitKeyContext,
	sslFreeKeyContext,
	sslGetKeyContextInfo,
	sslSetKeyContextInfo,
	sslCreatePublicKeyObject,
	sslFreePublicKey,
	sslGetPublicKeyField,
	sslSetPublicKeyField,
	sslCopyPublicKeyObject,
	sslDecodePrivateKeyObject,
	NULL,
	sslDecodePublicKeyObject,
	NULL,
	sslAddPublicKeyReference,
	sslEnumeratePublicKeyProperties,
	{ 0, 0, (SceUIntPtr)sslComparePublicKeyPropertyLists },
	sslComparePublicKeyObjects,
	NULL,
	sslAddKeyContextReference
};

static SslCryptoAlgorithmDescriptor sslDefaultCryptoAlgorithms[] = {
	{ 2, 0x2c, 0, sslGetCipherBackendMethods, sslDesEde3CbcBackendData },
	{ 2, 0x5, 0, sslGetCipherBackendMethods, sslRc4BackendData },
	{ 2, 0x8e, 0, sslGetCipherBackendMethods, sslAesCbcBackendData },
	{ 2, 0x96, 0, sslGetCipherBackendMethods, sslCamelliaCbcBackendData },
	{ 3, 0x4, 0, sslGetDigestBackendMethods, sslMd5BackendData },
	{ 3, 0x40, 0, sslGetDigestBackendMethods, sslSha1BackendData },
	{ 3, 0xa5, 0, sslGetDigestBackendMethods, sslSha224BackendData },
	{ 3, 0xa2, 0, sslGetDigestBackendMethods, sslSha256BackendData },
	{ 3, 0xa3, 0, sslGetDigestBackendMethods, sslSha384BackendData },
	{ 3, 0xa4, 0, sslGetDigestBackendMethods, sslSha512BackendData },
	{ 1, 0x13, 0x78, sslGetRsaCipherBackendMethods,
	  (void *)sslRsaPkcs1BackendData },
	{ 1, 0x6, 0x78, sslGetRsaCipherBackendMethods,
	  (void *)sslRsaRawBackendData },
	{ 6, 0x6, 0x6, sslGetRsaSignBackendMethods,
	  (void *)sslRsaSignBackendData },
	{ 6, 0x8, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 6, 0x41, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 6, 0x85, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 6, 0x82, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 6, 0x83, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 6, 0x84, 0x6, sslGetDigestSignBackendMethods, NULL },
	{ 7, 0x1002, 0x84, sslGetHmacBackendMethods, sslSha1BackendData },
	{ 7, 0x1001, 0x84, sslGetHmacBackendMethods, sslMd5BackendData },
	{ 4, 0x1000, 0, sslGetPrngBackendMethods, NULL },
	{ 4, 0x1055, 0, sslGetBasePrngBackendMethods, NULL },
	{ 4, 0x1026, 0, sslGetFips186PrngBackendMethods, NULL },
	{ 4, 0x1060, 0, sslGetPrngBackendMethods, NULL },
	{ 0, -1, 0xffffffffu, NULL, NULL }
};

static SslCryptoLifecycleMethods sslDefaultCryptoMethods = {
	sslNewCryptoContext,
	sslFreeCryptoContext,
	sslNewAlgorithm,
	sslCloneAlgorithm,
	sslFreeAlgorithm,
	sslUnavailableCryptoOperation,
	sslGetImplementationInfo,
	sslSetImplementationInfo,
	sslCipherMethodEncryptInit,
	(SceUIntPtr)sslCipherMethodOperate,
	sslCipherMethodUpdate,
	(SceUIntPtr)sslCipherMethodFinal,
	sslCipherMethodDecryptInit,
	(SceUIntPtr)sslCipherMethodOperate,
	sslCipherMethodUpdate,
	(SceUIntPtr)sslCipherMethodFinal,
	(int (*)(SslCryptoAlgorithm *, void *))sslUnavailableCryptoOperation,
	(int (*)(SslCryptoAlgorithm *, const void *, SceUInt32, void *,
		 SceUInt32 *))sslUnavailableCryptoOperation,
	{
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation
	},
	sslVerifyMethodInit,
	sslVerifyMethodVerify,
	sslVerifyMethodUpdate,
	sslVerifyMethodFinal,
	sslAsymMethodInit,
	sslAsymMethodEncrypt,
	sslAsymMethodInit,
	sslAsymMethodDecrypt,
	sslDigestMethodInit,
	sslDigestMethodOperate,
	sslDigestMethodUpdate,
	sslDigestMethodFinal,
	sslMacMethodInit,
	(SceUIntPtr)sslMacMethodOperate,
	sslMacMethodUpdate,
	sslMacMethodFinal,
	{
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation
	},
	(int (*)(SslCryptoAlgorithm *))sslUnavailableCryptoOperation,
	(int (*)(SslCryptoAlgorithm *, void **))sslUnavailableCryptoOperation,
	{
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation
	},
	sslRandomMethodSeed,
	sslRandomMethodGenerate,
	{
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation,
		(SceUIntPtr)sslUnavailableCryptoOperation
	},
	sslRetainCryptoContext
};

static SslLibraryModule sslDefaultLibraryModules[] = {
	{ 1, 0, 0, 0, NULL, NULL, sslGetDefaultLibraryMethods, NULL },
	{ 6, 0, 0, 0, NULL, NULL, sslGetDefaultCryptoMethods,
	  sslGetDefaultCryptoAlgorithms },
	{ 16, 0, 6, 0, NULL, NULL,
	  (const void *(*)(void *))sslGetPublicKeyModule, NULL },
	{ 43, 0, 0, 0,
	  (int (*)(SslLibraryModule *, SslLibraryModule *, void **))
		  sslInitDirectMemoryModule,
	  (void (*)(SslLibraryModule *, SslLibraryModule *, void **))
		  sslCleanupDirectMemoryModule,
	  (const void *(*)(void *))sslGetDefaultDirectMemoryContext, NULL },
	{ 6, 0, 6, 0, NULL, NULL, NULL, sslGetDefaultMacAlgorithmMappings },
	{ 10, 0, 1, 0, NULL, NULL,
	  (const void *(*)(void *))sslGetCertificateModuleMethods, NULL },
	{ 5, 0, 0, 0, NULL, NULL,
	  (const void *(*)(void *))sslGetDateMethods, NULL },
	{ 28, 0, 0, 0,
	  (int (*)(SslLibraryModule *, SslLibraryModule *, void **))
		  sslLibraryModuleInit,
	  (void (*)(SslLibraryModule *, SslLibraryModule *, void **))
		  sslLibraryModuleCleanup,
	  NULL, NULL },
	{ 26, 0, 0, 0, NULL, NULL,
	  (const void *(*)(void *))sslGetSmallCertificateStoreMethods, NULL },
	{ 17, 0, 0, 0, NULL, NULL,
	  (const void *(*)(void *))sslGetPemModuleMethods, NULL },
	{ 0, 0, 0, 1, NULL,
	  (void (*)(SslLibraryModule *, SslLibraryModule *, void **))
		  sslCleanupLibraryModuleData,
	  NULL, NULL }
};

const SslKeyModule *sslGetPublicKeyModule(void)
{
	return &sslPublicKeyModule;
}

SslLibraryModule *SceSslInternal_FB51AE89(void)
{
	return sslDefaultLibraryModules;
}

void SceSslInternal_DEC6DD27(void)
{
	sslResetTempRsaKeys();
	sslResetLockState();
	sslResetDirectMemoryState();
	sslClearErrors();
	sslResetWeakCipherFlags();
	sslResetExDataAndRandomGlobals();
	sslResetDefaultLibrary();
}

void *sslGetDefaultCryptoAlgorithms(void *data)
{
	(void)data;
	return sslDefaultCryptoAlgorithms;
}

const void *sslGetDefaultCryptoMethods(void *data)
{
	(void)data;
	return &sslDefaultCryptoMethods;
}
