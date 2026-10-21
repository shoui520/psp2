/*
 * Core archive file (CAF) contexts: the UID objects that hold a CAF
 * transform's state, and the transforms. A CAF header or segment is
 * authenticated with HMAC-SHA256 under a key the secure processor hands out
 * by selector (SceSblPostSsMgr); a segment may also be AES-128 CBC encrypted
 * or decrypted under a second such key.
 */

#include <stddef.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <sysmem/utils.h>
#include <string.h>
#include <sceerror.h>
#include <iofilemgr/iofilemgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <ss_mgr/ss_mgr.h>
#include <coredump/coredump.h>

#define CAF_HMAC_KEY_SIZE	32
#define CAF_HMAC_BLOCK_SIZE	64
#define CAF_DIGEST_SIZE		32
#define CAF_CIPHER_BLOCK_SIZE	16
#define CAF_CHAIN_SIZE		32	/* the largest Rijndael block the AES context takes */

/* a segment's cipher mode */
#define CAF_CIPHER_NONE		0
#define CAF_CIPHER_ENCRYPT	1
#define CAF_CIPHER_DECRYPT	2

/* the largest key selectors SceSblPostSsMgr accepts here */
#define CAF_HASH_KEY_SELECTOR_MAX	3
#define CAF_CIPHER_KEY_SELECTOR_MAX	4
#define CAF_CIPHER_KEY_SELECTOR_TOOL	0x10000001

/* a CAF transform's state behind a UID */
typedef struct SceCoredumpCafContextObject {
	SceObjectBase base;
	SceSha256Context sha;			/* the running HMAC-SHA256 */
	SceUInt8 outerKey[CAF_HMAC_BLOCK_SIZE];	/* the HMAC key ^ 0x5c */
	SceAesFullContext aes;			/* the segment cipher */
	SceUInt8 iv[CAF_CHAIN_SIZE];		/* the CBC chaining block */
	SceUInt32 cipherMode;
	SceUInt32 reserved2;
	SceUInt32 hashKeySelector;
	SceUInt32 cipherKeySelector;
	SceUInt32 hashStarted;
	SceUInt32 cipherStarted;
} SceCoredumpCafContextObject;

_Static_assert(offsetof(SceCoredumpCafContextObject, aes) == 0xd8, "CAF AES context");
_Static_assert(offsetof(SceCoredumpCafContextObject, iv) == 0x4a8, "CAF chaining block");
_Static_assert(offsetof(SceCoredumpCafContextObject, cipherMode) == 0x4c8,
	"CAF cipher mode");
_Static_assert(sizeof(SceCoredumpCafContextObject) == 1248, "CAF context object size");

/* the transform state after the object header */
#define CAF_STATE_SIZE	(sizeof(SceCoredumpCafContextObject) \
	- offsetof(SceCoredumpCafContextObject, sha))

SceClass cafContextClass;

static int _cafContextConstructor(void *object)
{
	SceCoredumpCafContextObject *context = object;

	memset(&context->sha, 0, CAF_STATE_SIZE);
	return 0;
}

static int _cafContextDestructor(void *object)
{
	SceCoredumpCafContextObject *context = object;

	memset(&context->sha, 0, CAF_STATE_SIZE);
	return 0;
}

int SceCoredumpForDriver_9336009B(void)
{
	return SceSblPostSsMgrForDriver_C977601E();
}

int SceCoredumpForDriver_C90F61AF(void)
{
	return SceSblPostSsMgrForDriver_4100B9E9();
}

static inline int _sceGetCafContextFromGUID(SceUID guid,
		SceCoredumpCafContextObject **context)
{
	SceCoredumpCafContextObject *object = NULL;
	int result;

	result = sceGUIDGetObject(guid, &object);
	if (result < 0) {
		SCE_DBG_PRINTF(11, 214, "sceGUIDGetObject(0x%08X) failed: 0x%08X\n",
			guid, result);
		return result;
	}
	*context = object;
	return result;
}

int sceCoredumpCafContextClassInit(void)
{
	int result;

	result = sceUIDClassInitClass(&cafContextClass, "SceUIDCafContextClass",
		SceSysmemForDriver_85336A1C(), sizeof(SceCoredumpCafContextObject),
		_cafContextConstructor, _cafContextDestructor);
	if (result < 0)
		SCE_DBG_ASSERT_LEVEL(0, 0, 244);
	return result;
}

SceUID sceCoredumpCafContextCreate(void)
{
	SceObjectBase *object;
	SceUID context;

	context = sceGUIDKernelCreate(&cafContextClass,
		"SceCoredumpCafContextObject", &object);
	if (context < 0)
		SCE_DBG_PRINTF(11, 261, "sceGUIDKernelCreate() failed: 0x%08X\n",
			context);
	return context;
}

int sceCoredumpCafContextDestroy(SceUID context)
{
	int result;

	result = sceGUIDClose(context);
	if (result < 0)
		SCE_DBG_PRINTF(11, 272, "sceGUIDClose(0x%08X) failed: 0x%08X\n",
			context, result);
	return result;
}

/* name unknown: start the HMAC-SHA256 under key (inner pad hashed, outer pad kept) */
static inline void _cafHmacInit(SceCoredumpCafContextObject *context, const SceUInt8 *key)
{
	SceUInt8 innerKey[CAF_HMAC_BLOCK_SIZE + 1];
	int i;

	memset(innerKey, 0, sizeof(innerKey));
	memset(context->outerKey, 0, sizeof(context->outerKey));
	__builtin_memcpy(innerKey, key, CAF_HMAC_KEY_SIZE);
	__builtin_memcpy(context->outerKey, key, CAF_HMAC_KEY_SIZE);
	for (i = 0; i < CAF_HMAC_BLOCK_SIZE; i++) {
		innerKey[i] ^= 0x36;
		context->outerKey[i] ^= 0x5c;
	}
	SceKernelUtilsForDriver_D909FA2C(&context->sha);
	SceKernelUtilsForDriver_236A9097(&context->sha, innerKey, CAF_HMAC_BLOCK_SIZE);
	context->hashStarted = 1;
}

/* name unknown: finish the HMAC-SHA256 into digest and clear the transform state */
static inline int _cafHmacFinal(SceCoredumpCafContextObject *context, void *digest)
{
	SceUInt8 innerDigest[CAF_DIGEST_SIZE];

	if (context->hashStarted == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	SceKernelUtilsForDriver_4899CD4B(&context->sha, innerDigest);
	SceKernelUtilsForDriver_D909FA2C(&context->sha);
	SceKernelUtilsForDriver_236A9097(&context->sha, context->outerKey, CAF_HMAC_BLOCK_SIZE);
	SceKernelUtilsForDriver_236A9097(&context->sha, innerDigest, CAF_DIGEST_SIZE);
	SceKernelUtilsForDriver_4899CD4B(&context->sha, digest);
	memset(&context->sha, 0, CAF_STATE_SIZE);
	return 0;
}

int SceCoredumpForDriver_7C8120C5(SceUID context, SceUInt32 hashKeySelector)
{
	SceCoredumpCafContextObject *object;
	SceUInt8 key[CAF_HMAC_KEY_SIZE];
	int result;

	if (hashKeySelector > CAF_HASH_KEY_SELECTOR_MAX)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;
	__builtin_memset(key, 0, sizeof(key));
	result = SceSblPostSsMgrForDriver_2C463AF1(hashKeySelector, sizeof(key), key);
	if (result < 0)
		return result;
	object->hashKeySelector = hashKeySelector;
	_cafHmacInit(object, key);
	return 0;
}

int SceCoredumpForDriver_AE2C2793(SceUID context, const void *data, SceSize size)
{
	SceCoredumpCafContextObject *object;
	int result;

	if (data == NULL && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;
	if (object->hashStarted == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	return SceKernelUtilsForDriver_236A9097(&object->sha, data, size);
}

int SceCoredumpForDriver_65AA4991(SceUID context, void *digest, SceSize size)
{
	SceCoredumpCafContextObject *object;
	int result;

	if (digest == NULL && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (digest != NULL && size < CAF_DIGEST_SIZE)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;
	return _cafHmacFinal(object, digest);
}

int SceCoredumpForDriver_E1BCBE8F(void *iv, SceSize size)
{
	if (iv == NULL || size != CAF_CIPHER_BLOCK_SIZE)
		return SCE_ERROR_ERRNO_EINVAL;
	return sceSblRngPseudoRandomNumber(iv, size);
}

/* name unknown: the chaining block from a block of the cipher's size */
static inline void _cafSetChain(SceCoredumpCafContextObject *context, const SceUInt8 *block,
				SceSize size)
{
	SceSize i;

	for (i = 0; i < size; i++)
		context->iv[i] = block[i];
}

int SceCoredumpForDriver_07185515(SceUID context, SceUInt32 hashKeySelector,
		SceUInt32 cipherMode, SceUInt32 cipherKeySelector, const void *iv, SceSize size)
{
	SceCoredumpCafContextObject *object;
	SceUInt8 hashKey[CAF_HMAC_KEY_SIZE];
	SceUInt8 cipherKey[CAF_CIPHER_BLOCK_SIZE];
	int result;

	if (iv == NULL && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (size != CAF_CIPHER_BLOCK_SIZE || cipherMode > CAF_CIPHER_DECRYPT
	    || hashKeySelector > CAF_HASH_KEY_SELECTOR_MAX)
		return SCE_ERROR_ERRNO_EINVAL;
	if (cipherKeySelector != CAF_CIPHER_KEY_SELECTOR_TOOL
	    && cipherKeySelector > CAF_CIPHER_KEY_SELECTOR_MAX)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;
	__builtin_memset(hashKey, 0, sizeof(hashKey));
	result = SceSblPostSsMgrForDriver_2C463AF1(hashKeySelector, sizeof(hashKey), hashKey);
	if (result < 0)
		return result;
	__builtin_memset(cipherKey, 0, sizeof(cipherKey));
	result = SceSblPostSsMgrForDriver_CB5436BD(cipherKeySelector, size, cipherKey);
	if (result < 0)
		return result;
	object->hashKeySelector = hashKeySelector;
	object->cipherKeySelector = cipherKeySelector;
	__builtin_memcpy(object->iv, iv, CAF_CIPHER_BLOCK_SIZE);
	object->cipherMode = cipherMode;
	_cafHmacInit(object, hashKey);
	if (SceKernelUtilsForDriver_F12B6451(&object->aes, 128, 128, cipherKey) < 0)
		return SCE_ERROR_ERRNO_EINVAL;
	/* as Sony's: the chaining block is set from itself */
	_cafSetChain(object, object->iv, object->aes.encryption.blockWords * 4);
	object->cipherStarted = 1;
	return 0;
}

int SceCoredumpForDriver_FB7AEBFE(SceUID context, const void *source, void *destination,
		SceSize size)
{
	SceCoredumpCafContextObject *object;
	const SceUInt8 *src = source;
	SceUInt8 *dst = destination;
	SceUInt8 block[CAF_CHAIN_SIZE];
	SceSize blocks, n, i, length;
	int result;

	if ((destination == NULL || source == NULL) && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;

	if (object->cipherMode != CAF_CIPHER_ENCRYPT
	    && object->cipherMode != CAF_CIPHER_DECRYPT) {
		if (SceKernelUtilsForDriver_236A9097(&object->sha, source, size) != 0)
			return SCE_ERROR_ERRNO_EINVAL;
		return 0;
	}
	if ((size & (CAF_CIPHER_BLOCK_SIZE - 1)) != 0 || object->hashStarted == 0
	    || object->cipherStarted == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	blocks = size / CAF_CIPHER_BLOCK_SIZE;

	if (object->cipherMode == CAF_CIPHER_DECRYPT) {
		/* the ciphertext is authenticated */
		if (SceKernelUtilsForDriver_236A9097(&object->sha, source, size) != 0)
			return SCE_ERROR_ERRNO_EINVAL;
		for (n = 0; n < blocks; n++) {
			SceKernelUtilsForDriver_D8678061(&object->aes, src, block);
			for (i = 0; i < object->aes.encryption.blockWords * 4; i++) {
				SceUInt8 chain = object->iv[i];

				object->iv[i] = src[i];
				dst[i] = chain ^ block[i];
			}
			src += CAF_CIPHER_BLOCK_SIZE;
			dst += CAF_CIPHER_BLOCK_SIZE;
		}
		return 0;
	}

	if (blocks != 0) {
		length = object->aes.encryption.blockWords * 4;
		for (n = 0; n < blocks; n++) {
			for (i = 0; i < length; i++)
				block[i] = src[i] ^ object->iv[i];
			SceKernelUtilsForDriver_C2A61770(&object->aes, block, dst);
			length = object->aes.encryption.blockWords * 4;
			_cafSetChain(object, dst, length);
			src += CAF_CIPHER_BLOCK_SIZE;
			dst += CAF_CIPHER_BLOCK_SIZE;
		}
	}
	/* the ciphertext is authenticated */
	if (SceKernelUtilsForDriver_236A9097(&object->sha, destination, size) != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	return 0;
}

int SceCoredumpForDriver_DF17420A(SceUID context, void *digest, SceSize size)
{
	SceCoredumpCafContextObject *object;
	int result;

	if (digest == NULL && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (digest != NULL && size < CAF_DIGEST_SIZE)
		return SCE_ERROR_ERRNO_EINVAL;
	result = _sceGetCafContextFromGUID(context, &object);
	if (result < 0)
		return result;
	return _cafHmacFinal(object, digest);
}

int SceCoredumpForDriver_AD070837(void)
{
	return sceIoRemove("ux0:data/crash_report.caf");
}
