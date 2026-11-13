/* Filename and descriptive names are ours: CRYPTOCME's cipher object, made
 * for a descriptor, with its key, its chaining value and its information.
 */
#include "ll_ciph.h"
#include "../../object.h"
#include "../../../../memory.h"

int sslFreeCipherCore(SslCipherCore *core)
{
	const SslCipherDescriptor *descriptor;
	SslDirectMemoryContext *dmem;
	SceUInt32 allocated;

	if (core != NULL) {
		descriptor = core->descriptor;
		dmem = core->dmem;
		if (descriptor != NULL) {
			if (descriptor->Cleanup != NULL)
				descriptor->Cleanup(core);
			if (core->flags & SSL_CIPHER_CORE_STATE_ALLOCATED)
				sslFreeDirectMemory(core->state, dmem);
		}
		if (core->savedKey != NULL) {
			sslFillMemory(core->savedKey, 0, core->savedKeyLength);
			sslFreeDirectMemory(core->savedKey, dmem);
		}
		allocated = core->flags & SSL_CIPHER_CORE_ALLOCATED;
		sslFillMemory(core, 0, core->size);
		if (allocated)
			sslFreeDirectMemory(core, dmem);
		if (descriptor != NULL && descriptor->dmem != NULL)
			sslFreeDirectMemory((void *)descriptor,
					    descriptor->dmem);
	}
	return 0;
}

/* Makes the object in *result anew, or reuses it when it is large enough */
int sslCreateCipherCore(SslCipherCore **result,
			const SslCipherDescriptor *descriptor,
			SslDirectMemoryContext *dmem)
{
	SslCipherCore *core;
	void *memory;
	SceUInt32 stateWords = 0;
	SceUInt32 extraOffset = 0;
	SceUInt32 size;
	SceUInt32 *state;
	unsigned char *extra;
	int status;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	core = *result;
	if (dmem == NULL && core != NULL && core->dmem != NULL)
		dmem = core->dmem;
	size = sizeof(SslCipherCore);
	if (descriptor != NULL) {
		stateWords = sizeof(SslCipherCore) / 4;
		extraOffset = (sizeof(SslCipherCore) + descriptor->stateSize +
			       7) & ~7;
		size = extraOffset + descriptor->extraSize;
		if (descriptor->flags & SSL_CIPHER_ALIGNED)
			size += descriptor->extraSize + 8 != 0 ? 8 : 0;
	}
	if (core == NULL || core->size < size) {
		if (dmem == NULL)
			return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
		status = sslAllocateDirectMemory(&memory, size, dmem,
						 SSL_DMEM_ALLOCATE_ZERO);
		if (status != 0)
			return status;
		sslFreeCipherCore(core);
		core = memory;
		*result = core;
		core->dmem = dmem;
		core->flags |= SSL_CIPHER_CORE_ALLOCATED;
	}
	if (descriptor != NULL) {
		core->descriptor = descriptor;
		state = (SceUInt32 *)core + stateWords;
		core->encrypt = 1;
		extra = (unsigned char *)core + extraOffset;
		core->size = size;
		core->state = state;
		core->extra = extra;
		if (descriptor->flags & SSL_CIPHER_ALIGNED) {
			if ((SceUIntPtr)state & 0xf) {
				core->state = state + 2;
				extra += 8;
				core->extra = extra;
			}
			if ((SceUIntPtr)extra & 0xf)
				core->extra = extra + 8;
		}
		core->keyLength = descriptor->keyLength;
	}
	return 0;
}

int sslSetCipherCoreKey(SslCipherCore *core, const void *key,
			SceUInt32 keyLength, const unsigned char *iv, int flags)
{
	const SslCipherDescriptor *descriptor;
	void *saved;
	int status = 0;

	if (core == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	descriptor = core->descriptor;
	if (descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (keyLength != 0) {
		if (key == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		core->keyLength = keyLength;
	}
	if (core->keyLength == 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (!(flags & SSL_CIPHER_KEEP_SCHEDULE)) {
		status = descriptor->SetKey(core, key, iv, flags);
		if (status != 0) {
			core->keyLength = 0;
			return status;
		}
	}
	if (flags & SSL_CIPHER_SAVE_KEY) {
		saved = core->savedKey;
		if (saved != key) {
			if (saved != NULL && core->savedKeyLength < keyLength) {
				sslFillMemory(saved, 0, core->savedKeyLength);
				sslFreeDirectMemory(core->savedKey, core->dmem);
				core->savedKeyLength = 0;
				saved = NULL;
				core->savedKey = NULL;
			}
			if (keyLength != 0) {
				if (saved == NULL) {
					status = sslAllocateDirectMemory(
						&core->savedKey, keyLength,
						core->dmem, 0);
					if (status != 0)
						return status;
					saved = core->savedKey;
				}
				core->savedKeyLength = keyLength;
				sslCopyMemory(saved, key, keyLength);
			}
		}
		core->flags |= SSL_CIPHER_CORE_KEY_SAVED;
	}
	return status;
}

int sslProcessCipherCore(SslCipherCore *core, unsigned char *output,
			 const unsigned char *input, SceUInt32 length)
{
	if (core->keyLength != 0)
		return core->descriptor->Process(core, output, input, length);
	return SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
}

/* The descriptor's numbers; what it does not hold, its GetInfo answers */
int sslGetCipherDescriptorInfo(const SslCipherDescriptor *descriptor,
			       SslCipherCore *core, int selector,
			       SceUInt32 *value, void **pointer)
{
	SceUInt32 number = 0;
	const void *data = NULL;
	int status;

	if (descriptor == NULL) {
		if (core == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		descriptor = core->descriptor;
		if (descriptor == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
	}
	switch (selector) {
	case SSL_CIPHER_INFO_BLOCK_SIZE:
		number = descriptor->blockSize;
		break;
	case SSL_CIPHER_INFO_MIN_KEY_LENGTH:
		number = descriptor->minKeyLength;
		break;
	case SSL_CIPHER_INFO_MAX_KEY_LENGTH:
		number = descriptor->maxKeyLength;
		break;
	case SSL_CIPHER_INFO_KEY_LENGTH:
		number = descriptor->keyLength;
		break;
	case SSL_CIPHER_INFO_IV_LENGTH:
		number = descriptor->ivLength;
		break;
	case SSL_CIPHER_INFO_STATE_SIZE:
		number = descriptor->stateSize;
		break;
	case SSL_CIPHER_INFO_OID:
		data = &descriptor->oid;
		break;
	case SSL_CIPHER_INFO_VERSION:
		number = descriptor->version;
		break;
	case SSL_CIPHER_INFO_NAME:
		data = descriptor->name;
		break;
	case SSL_CIPHER_INFO_SAVABLE:
		number = 1;
		if (descriptor->GetInfo == NULL)
			break;
		/* fall through */
	default:
		status = SSL_CRYPTO_ERROR_UNHANDLED;
		if (descriptor->GetInfo != NULL)
			status = descriptor->GetInfo(descriptor, core,
						     selector, value, pointer);
		return status;
	}
	if (value != NULL)
		*value = number;
	if (pointer != NULL)
		*pointer = (void *)data;
	return 0;
}

int sslGetCipherCoreInfo(SslCipherCore *core, int selector, SceUInt32 *value,
			 void **pointer)
{
	SceUInt32 number = 0;
	void *data = NULL;
	int status = 0;

	if (core == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	switch (selector) {
	case SSL_CIPHER_INFO_KEY_LENGTH:
		number = core->keyLength;
		break;
	case SSL_CIPHER_INFO_PARAMETER:
		number = core->parameter;
		break;
	case SSL_CIPHER_INFO_SET_PARAMETER:
		if (value == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		core->parameter = *value;
		break;
	case SSL_CIPHER_INFO_IV:
		data = core->iv;
		break;
	case SSL_CIPHER_INFO_INITIAL_IV:
		data = core->initialIv;
		break;
	case SSL_CIPHER_INFO_SAVED_KEY:
		data = core->savedKey;
		break;
	case SSL_CIPHER_INFO_DIRECTION:
		number = core->encrypt;
		break;
	case SSL_CIPHER_INFO_DESCRIPTOR:
		data = (void *)core->descriptor;
		break;
	default:
		if (core->descriptor == NULL)
			return SSL_CRYPTO_ERROR_NO_METHODS;
		return sslGetCipherDescriptorInfo(core->descriptor, core,
						  selector, value, pointer);
	}
	if (value != NULL)
		*value = number;
	if (pointer != NULL)
		*pointer = data;
	return status;
}

SceUInt32 sslGetCipherCoreFlags(SslCipherCore *core)
{
	return core->descriptor->flags;
}

int sslSetCipherCoreInfo(SslCipherCore *core, int selector, SceUInt32 value,
			 void **pointer)
{
	return sslGetCipherCoreInfo(core, selector, &value, pointer);
}

int sslQueryCipherCoreInfo(SslCipherCore *core, int selector,
			   SceUInt32 *value, void **pointer)
{
	return sslGetCipherCoreInfo(core, selector, value, pointer);
}

int sslQueryCipherDescriptorInfo(const SslCipherDescriptor *descriptor,
				 SslCipherCore *core, int selector,
				 SceUInt32 *value, void **pointer)
{
	return sslGetCipherDescriptorInfo(descriptor, core, selector, value,
					  pointer);
}

/* The key and the chaining value in the descriptor's state schema */
int sslSaveCipherCoreState(SslCipherCore *core, void *buffer,
			   SceUInt32 *length, SceUInt32 max)
{
	const SslCipherDescriptor *descriptor;

	if (core == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	descriptor = core->descriptor;
	if (descriptor->stateSchema == NULL ||
	    !(descriptor->flags & SSL_CIPHER_SAVABLE))
		return SSL_CRYPTO_ERROR_UNHANDLED;
	if (core->flags & SSL_CIPHER_CORE_KEY_SAVED)
		return sslEncodeObjectState(core, descriptor->stateSchema,
					    buffer, length, max);
	return SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
}

int sslRestoreCipherCoreState(SslCipherCore *core, const void *buffer,
			      SceUInt32 length)
{
	const SslCipherDescriptor *descriptor;
	int status;

	if (core == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	descriptor = core->descriptor;
	if (descriptor->stateSchema == NULL ||
	    !(descriptor->flags & SSL_CIPHER_SAVABLE))
		return SSL_CRYPTO_ERROR_UNHANDLED;
	status = sslSetCipherCoreKey(core, NULL, 0, NULL,
				     SSL_CIPHER_ENCRYPT | SSL_CIPHER_KEEP_IV);
	if (status != 0)
		return status;
	status = sslDecodeObjectState(core, core->descriptor->stateSchema,
				      buffer, length, NULL, core->dmem);
	if (core->savedKeyLength == 0 && core->savedKey != NULL)
		core->savedKeyLength = core->keyLength;
	if (status == 0 && core->keyLength != 0)
		return sslSetCipherCoreKey(core, core->savedKey,
					   core->keyLength, NULL,
					   core->encrypt | SSL_CIPHER_KEEP_IV |
						   SSL_CIPHER_SAVE_KEY);
	return status;
}
