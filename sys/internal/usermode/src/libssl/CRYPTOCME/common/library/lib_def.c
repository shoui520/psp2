/* Filename and private descriptive names are ours; library default module and reset glue. */
#include "lib_ctx.h"
#include "../key/pkey_ctx.h"
#include "../key/pkey_pk.h"
#include "../module/dmem/dmem.h"
#include "../../../runtime.h"
#include "../../../err/err.h"

void sslResetTempRsaKeys(void);
int sslResetWeakCipherFlags(void);
void sslResetExDataAndRandomGlobals(void);

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

const SslKeyModule *sslGetPublicKeyModule(void)
{
	return &sslPublicKeyModule;
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
