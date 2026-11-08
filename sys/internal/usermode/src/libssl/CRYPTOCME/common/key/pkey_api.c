/* Filename and descriptive interface names are ours, in the recovered key
 * component. These types describe shared interface prefixes only.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "../object.h"
#include "../property.h"
#include "../asn1_props.h"
#include "pkey_ctx.h"
#include "pkey_pk.h"
#include "../../../memory.h"
#include "../../../format.h"
#include "../../../string_inline.h"
#include "../../../buffer/buffer.h"
#include "../../../bio/bio.h"
#include "../../../pem/pem.h"

static __inline SslPropertyList *sslNewPropertyListInline(void)
{
	SslPropertyList *list = sslAllocate(sizeof(*list));

	if (list == NULL)
		return NULL;
	sslFillMemory(list, 0, sizeof(*list));
	list->flags = SSL_PROPERTY_HEAP_OBJECT;
	return list;
}

static __inline BUF_MEM *sslNewBufMemInline(void)
{
	BUF_MEM *ret = (BUF_MEM *)sslAllocate(sizeof(BUF_MEM));

	if (ret == NULL)
		return NULL;
	ret->length = 0;
	ret->max = 0;
	ret->data = NULL;
	return ret;
}

static __inline void sslFreeBufMemInline(BUF_MEM *a)
{
	if (a == NULL)
		return;
	if (a->data != NULL) {
		sslFillMemory(a->data, 0, (unsigned int)a->max);
		sslFree(a->data);
	}
	sslFree(a);
}

#define sslNewPropertyList sslNewPropertyListInline
#define BUF_MEM_new sslNewBufMemInline
#define BUF_MEM_free sslFreeBufMemInline

static const unsigned char sslRsaEncryptionOidBytes[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
};

int sslGetPublicKeyInfo(void *key, int selector, void *result)
{
	SslPublicKeyHeader *object = key;

	if (result == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->GetKeyInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->GetKeyInfo(key, selector, result);
}

int sslGetPrivateKeyPemHeader(void *key, int algorithm, int format,
			      SceUInt32 capacity, char *header)
{
	SceUInt32 info[2] = { 0, 0 };
	const char *name = "ENCRYPTED PRIVATE KEY";

	if (header == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (capacity == 0)
		return 10016;
	if ((char)format == 1) {
		if (key != NULL && sslGetPublicKeyInfo(key, 12342, info) != 0)
			name = "PRIVATE KEY";
		sslCopyString(header, name, capacity);
		if (sslStringLengthInline(name) + 1 > capacity)
			header[capacity - 1] = '\0';
		return 0;
	}
	if (algorithm != 6)
		return 10018;
	sslCopyString(header, "RSA PRIVATE KEY", capacity);
	if (sslStringLengthInline("RSA PRIVATE KEY") + 1 > capacity) {
		header[capacity - 1] = '\0';
		return 10016;
	}
	return 0;
}

int sslGetPublicKeyPemHeader(int algorithm, SceUInt32 capacity, char *header)
{
	int status = 0;

	if (header == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (capacity == 0)
		return 10016;
	if (algorithm == 6) {
		sslCopyString(header, "PUBLIC KEY", capacity);
		if (sslStringLengthInline("PUBLIC KEY") + 1 > capacity) {
			header[capacity - 1] = '\0';
			status = 10016;
		}
	} else {
		status = 10018;
	}
	return status;
}

int sslDecodePublicKey(void *context, int format, int algorithm,
		       SceUInt32 length, const void *data,
		       SceUInt32 *consumed, void **result)
{
	SslKeyContext *keyContext = context;

	if (result == NULL || keyContext == NULL || keyContext->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (keyContext->module->DecodePublicKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->DecodePublicKey(context, format, algorithm,
						   length, data, consumed,
						   result);
}

int SceSslInternal_FEFE761C(void *context, int format, int algorithm,
			    SceUInt32 length, const void *data,
			    SceUInt32 *consumed, void **result)
{
	SslKeyContext *keyContext = context;

	if (result == NULL || keyContext == NULL || keyContext->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (keyContext->module->DecodePrivateKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->DecodePrivateKey(context, format, algorithm,
						    length, data, consumed,
						    result);
}

int sslDecodePrivateKeyDer(void *context, int format, int algorithm,
			   int mode, SceUInt32 length, const void *data,
			   SceUInt32 *consumed, void **result)
{
	SslPropertyList *properties = NULL;
	SslProperty *bitString = NULL;
	int status;

	(void)format;
	if ((char)mode == 1)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	if ((char)mode == 0) {
		status = SceSslInternal_FEFE761C(context, 0, algorithm, length,
						 data, consumed, result);
		if (status == 10028)
			status = 10020;
		return status;
	}
	if (algorithm != 6 || (char)mode != 2)
		return 10010;
	properties = sslNewPropertyList();
	if (properties == NULL) {
		status = 10005;
		goto cleanup;
	}
	status = sslDecodeSubjectPublicKeyInfoProperties(properties, data,
							 length, NULL);
	if (status == 0) {
		if (sslFindProperty(properties, 48, 3, NULL, &bitString, 0) != 0) {
			status = 10001;
			goto cleanup;
		}
		status = sslDecodePublicKey(context, 0, algorithm,
					    bitString->length - 1,
					    (const char *)bitString->data + 1,
					    NULL, result);
	}
cleanup:
	if (properties != NULL)
		sslFreePropertyList(properties);
	return status;
}

int sslDispatchGetKeyContextInfo(void *context, int selector, void *result)
{
	SslKeyContext *keyContext = context;

	if (result == NULL || keyContext == NULL || keyContext->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (keyContext->module->GetContextInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->GetContextInfo(context, selector, result);
}

void *sslGetKeyContextLibrary(void *context)
{
	void *library = NULL;

	if (sslDispatchGetKeyContextInfo(context, 1, &library) != 0)
		return NULL;
	return library;
}

int SceSslInternal_7D30F0A6(void *bio, void *context, void **result,
			    int algorithm, int flags)
{
	BUF_MEM *mem = NULL;
	SslPropertyList *properties = NULL;
	SslObjectHeader *pemContext = NULL;
	void *pemData = NULL;
	SceUInt32 pemLength;
	char chunk[256];
	const void *data;
	SceUInt32 length;
	int status;

	if (context == NULL || result == NULL || bio == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if ((flags & 0x1100) != 0) {
		status = sslCreatePemContext(sslGetKeyContextLibrary(context),
					     0, &pemContext);
		if (status != 0)
			goto done;
		status = sslGetPrivateKeyPemHeader(*result, algorithm,
						   (char)flags, sizeof(chunk),
						   chunk);
		if (status != 0)
			goto done;
		status = sslReadPemObject(pemContext, bio, chunk, &pemData,
					  &pemLength);
		if (status != 0)
			goto done;
		length = pemLength;
		data = pemData;
	} else {
		int total = 0;

		mem = BUF_MEM_new();
		if (mem == NULL) {
			status = 10005;
			goto cleanup;
		}
		for (;;) {
			int readBytes = BIO_read(bio, chunk, sizeof(chunk));

			if (readBytes <= 0)
				break;
			total = sslBufferAppend(mem, chunk, readBytes);
			if (total <= 0) {
				status = 10005;
				goto cleanup;
			}
		}
		length = (SceUInt32)total;
		data = mem->data;
	}
	properties = sslNewPropertyList();
	if (properties != NULL)
		status = sslDecodePrivateKeyDer(context, 0, algorithm, flags,
						length, data, NULL, result);
	else
		status = 10005;
cleanup:
	BUF_MEM_free(mem);
	if (properties != NULL)
		sslFreePropertyList(properties);
done:
	if (pemData != NULL) {
		sslFillMemory(pemData, 0, pemLength);
		sslFree(pemData);
	}
	if (pemContext != NULL)
		sslDestroyPemContext(pemContext);
	return status;
}

int SceSslInternal_46358909(void *key)
{
	SslPublicKeyHeader *object = key;

	if (object == NULL)
		return 0;
	if (object->context->module == NULL)
		return 0;
	if (object->context->module->DestroyKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->DestroyKey(key);
}

int sslReadPublicKeyBio(void *bio, void *context, void **result,
			int algorithm, int flags)
{
	BUF_MEM *mem = NULL;
	SslPropertyList *properties = NULL;
	SslObjectHeader *pemContext = NULL;
	void *pemData = NULL;
	SceUInt32 pemLength;
	SslProperty *oidProp = NULL;
	SslProperty *bitString = NULL;
	char chunk[256];
	const void *data;
	SceUInt32 length;
	int status;

	if (context == NULL || result == NULL || bio == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if ((flags & 0x1100) != 0) {
		status = sslCreatePemContext(sslGetKeyContextLibrary(context),
					     0, &pemContext);
		if (status != 0)
			goto done;
		status = sslGetPublicKeyPemHeader(algorithm, sizeof(chunk),
						  chunk);
		if (status != 0)
			goto done;
		status = sslReadPemObject(pemContext, bio, chunk, &pemData,
					  &pemLength);
		if (status != 0)
			goto done;
		length = pemLength;
		data = pemData;
	} else {
		int total = 0;

		mem = BUF_MEM_new();
		if (mem == NULL) {
			status = 10005;
			goto cleanup;
		}
		for (;;) {
			int readBytes = BIO_read(bio, chunk, sizeof(chunk));

			if (readBytes <= 0)
				break;
			total = sslBufferAppend(mem, chunk, readBytes);
			if (total <= 0) {
				status = 10005;
				goto cleanup;
			}
		}
		length = (SceUInt32)total;
		data = mem->data;
	}
	if (algorithm != 6) {
		status = 10010;
		goto cleanup;
	}
	if ((unsigned char)flags == 2) {
		properties = sslNewPropertyList();
		if (properties == NULL) {
			status = 10005;
			goto cleanup;
		}
		status = sslDecodeSubjectPublicKeyInfoProperties(properties, data,
								 length, NULL);
		if (status != 0)
			goto cleanup;
		status = sslFindProperty(properties, 48, 1, NULL, &oidProp, 0);
		if (status != 0)
			goto cleanup;
		if (sslCompareMemory(oidProp->data, sslRsaEncryptionOidBytes,
				     sizeof(sslRsaEncryptionOidBytes)) != 0) {
			status = 10001;
			goto cleanup;
		}
		if (sslFindProperty(properties, 48, 3, NULL, &bitString, 0) != 0) {
			status = 10001;
			goto cleanup;
		}
		status = sslDecodePublicKey(context, 0, algorithm,
					    bitString->length - 1,
					    (const char *)bitString->data + 1,
					    NULL, result);
		goto cleanup;
	}
	status = sslDecodePublicKey(context, 0, algorithm, length, data, NULL,
				    result);
	if (status == 0) {
		SslPublicKey *publicKey = *result;

		if (sslFindProperty(publicKey->properties, 16, 18, NULL,
				    &bitString, 0) == 0) {
			SceSslInternal_46358909(*result);
			status = 10001;
		}
	}
cleanup:
	BUF_MEM_free(mem);
	if (properties != NULL)
		sslFreePropertyList(properties);
done:
	if (pemData != NULL)
		sslFree(pemData);
	if (pemContext != NULL)
		sslDestroyPemContext(pemContext);
	return status;
}

static __inline int sslGetPublicKeyAlgorithmInline(void *key)
{
	int algorithm;

	if (sslGetPublicKeyInfo(key, SSL_KEY_INFO_ALGORITHM, &algorithm) != 0)
		return -1;
	return algorithm;
}

#define sslGetPublicKeyAlgorithm sslGetPublicKeyAlgorithmInline

int sslSetPublicKeyInfo(void *key, int selector, void *value)
{
	SslPublicKeyHeader *object = key;

	if (object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->SetKeyInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->SetKeyInfo(key, selector, value);
}

int sslCreatePublicKey(void *context, int algorithm, void **result)
{
	SslKeyContext *keyContext = context;

	if (result == NULL || keyContext == NULL || keyContext->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (keyContext->module->CreateKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->CreateKey(context, algorithm, result);
}

int sslCopyPublicKey(void *source, void *destination, int mode)
{
	SslPublicKeyHeader *object = source;

	if (destination == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->CopyKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->CopyKey(source, destination, mode);
}

int sslClonePublicKey(void *source, int mode, void **result)
{
	void *context;
	int algorithm;
	int status;

	if (source == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	context = NULL;
	algorithm = sslGetPublicKeyAlgorithm(source);
	status = sslGetPublicKeyInfo(source, SSL_KEY_INFO_CONTEXT, &context);
	if (status == 0) {
		status = sslCreatePublicKey(context, algorithm, result);
		if (status == 0)
			status = sslCopyPublicKey(source, *result, mode);
	}
	return status;
}

int sslRetainPublicKey(void *key)
{
	SslPublicKeyHeader *object = key;

	if (object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->RetainKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->RetainKey(key);
}

int sslEnumerateKeyComponents(void *key, SslKeyComponentCallback callback, void *arg)
{
	SslPublicKeyHeader *object = key;

	if (callback == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->EnumerateComponents == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->EnumerateComponents(key, callback, arg);
}

int sslGetPublicKeyBitLength(void *key)
{
	int bits = 0;

	if (sslGetPublicKeyInfo(key, SSL_KEY_INFO_BIT_LENGTH, &bits) != 0)
		return 0;
	return bits;
}

int sslComparePublicKeys(void *left, void *right)
{
	SslPublicKeyHeader *object = left;

	if (right == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->CompareKeys == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->CompareKeys(left, right);
}

int sslCreateKeyContext(SslLibrary *library, int parameter, SceUInt32 flags,
			void **result)
{
	SslKeyModuleFactory factory;
	SslKeyModuleFactory getMethods;
	const SslKeyModule *module;
	void *data = NULL;
	int status;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = sslGetLibraryMethod(library, SSL_KEY_MODULE_TYPE, 0, flags,
				     parameter, SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_KEY_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	module = getMethods(data);
	if (module == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return module->CreateContext(library, parameter, flags, result);
}
