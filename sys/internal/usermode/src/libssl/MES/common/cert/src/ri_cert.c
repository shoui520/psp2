/* Source path recovered from the firmware; descriptive private names are ours. */
#include <libssl/object.h>
#include "ri_cert.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"
#include "../../../../CRYPTOCME/common/key/pkey_ctx.h"
#include "../../../../CRYPTOCME/common/module/crypto/cry_api.h"

static const char certificateSource[] =
	"S:\\work\\MES\\common\\cert/src/ri_cert.c";

int sslApplyCertificateProperties(SslCertificate *certificate,
				 SslPropertyList *properties)
{
	SslCertificateName *issuer, *subject;
	SslCertificateKeyCache *key;
	SslCertificatePropertyCache *first, *second;
	int status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

	if (certificate->references != 1)
		return 10025;
	if (sslPackProperties(properties, 0) != 0)
		return 10005;
	if (certificate->backendMethods->ImportProperties != NULL)
		status = certificate->backendMethods->ImportProperties(certificate,
							      properties);
	if (status != 0)
		return status;
	certificate->flags &= ~1u;
	issuer = certificate->issuer;
	subject = certificate->subject;
	key = certificate->publicKey;
	first = certificate->propertyCaches[0];
	second = certificate->propertyCaches[1];
	if (issuer != NULL)
		issuer->flags &= ~1u;
	if (subject != NULL)
		subject->flags &= ~1u;
	if (key != NULL)
		key->flags &= ~1u;
	if (first != NULL)
		first->flags &= ~1u;
	if (second != NULL)
		second->flags &= ~1u;
	return 0;
}

int sslCanonicalizeCertificate(SslCertificate *certificate)
{
	SslPropertyList *properties;
	int status = 0;

	if (!(certificate->flags & 1))
		return 0;
	if (certificate->backendMethods->Prepare != NULL)
		status = certificate->backendMethods->Prepare(certificate);
	if (status != 0 && status != 10010)
		return status;
	properties = sslCopyPropertyList(NULL, certificate->properties,
					 SSL_PROPERTY_COPY_DATA);
	if (properties == NULL)
		return 10005;
	status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	if (certificate->backendMethods->ExportProperties != NULL)
		status = certificate->backendMethods->ExportProperties(certificate,
							      properties);
	if (status == 0)
		status = sslApplyCertificateProperties(certificate, properties);
	if (status != 0)
		sslFreePropertyList(properties);
	return status;
}

int sslCacheCertificatePublicKey(SslCertificate *certificate, int mode, void *source)
{
	void *replacement = NULL;
	void *key = NULL;
	void *context;
	int algorithm = sslGetPublicKeyAlgorithm(source);
	int status;

	status = sslGetPublicKeyInfo(source, SSL_KEY_INFO_CONTEXT, &context);
	if (status == 0) {
		status = sslCreatePublicKey(context, algorithm, &key);
		if (status == 0) {
			status = sslCopyPublicKey(source, key, mode == 1);
			if (status != 0)
				SceSslInternal_46358909(key);
			else
				replacement = key;
		}
	}
	if (status != 0)
		return status;
	if (certificate->publicKey != NULL) {
		if (certificate->publicKey->key != NULL)
			SceSslInternal_46358909(certificate->publicKey->key);
	} else {
		certificate->publicKey = sslAllocate(sizeof(*certificate->publicKey));
		if (certificate->publicKey == NULL) {
			if (replacement != NULL)
				SceSslInternal_46358909(replacement);
			return 10005;
		}
		certificate->publicKey->flags = 0;
	}
	certificate->publicKey->algorithm = algorithm;
	certificate->publicKey->key = replacement;
	return 0;
}

int sslFreeCertificateContext(void *object)
{
	SslCertificateContext *context = object;

	if (sslAddReference(&context->references, -1, 28,
			    certificateSource, 393) == 0) {
		if (context->keyContext != NULL)
			sslDestroyKeyContext(context->keyContext);
		if (context->cryptoContext != NULL && context->cryptoOwnership == 2)
			sslDestroyCryptoContext(context->cryptoContext);
		if (context->library != NULL)
			SceSslInternal_4725F9E7(context->library);
		sslFree(context);
	}
	return 0;
}

int sslFreeCertificateName(void *object)
{
	SslCertificateName *name = object;

	if (name->properties != NULL) {
		sslFreePropertyList(name->properties);
		name->properties = NULL;
	}
	if (name->context != NULL)
		sslFreeCertificateContext(name->context);
	sslFree(name);
	return 0;
}

int sslClearCertificateCache(SslCertificate *certificate, int selector)
{
	SslCertificatePropertyCache *cache;
	int status = 0;

	switch (selector) {
	case 4:
		if (certificate->issuer != NULL) {
			status = sslFreeCertificateName(certificate->issuer);
			if (status == 0)
				certificate->issuer = NULL;
		}
		break;
	case 7:
		if (certificate->subject != NULL) {
			status = sslFreeCertificateName(certificate->subject);
			if (status == 0)
				certificate->subject = NULL;
		}
		break;
	case 9:
		if (certificate->publicKey != NULL &&
		    certificate->publicKey->key != NULL) {
			status = SceSslInternal_46358909(certificate->publicKey->key);
			if (status == 0) {
				certificate->publicKey->key = NULL;
				sslFree(certificate->publicKey);
				certificate->publicKey = NULL;
			}
		}
		break;
	case 10:
		cache = certificate->propertyCaches[0];
		if (cache != NULL) {
			if (cache->properties != NULL) {
				sslFreePropertyList(cache->properties);
				cache->properties = NULL;
			}
			sslFree(cache);
			certificate->propertyCaches[0] = NULL;
		}
		break;
	case 11:
		cache = certificate->propertyCaches[1];
		if (cache != NULL) {
			if (cache->properties != NULL) {
				sslFreePropertyList(cache->properties);
				cache->properties = NULL;
			}
			sslFree(cache);
			certificate->propertyCaches[1] = NULL;
		}
		break;
	}
	return status;
}

int sslInvalidateCertificateProperties(SslCertificate *certificate)
{
	sslDeleteProperty(certificate->properties, 96, 14);
	return sslDeleteProperty(certificate->properties, 96, 65);
}

int sslSetCertificateProperty(SslCertificate *certificate, int id,
			      SceUInt32 value, const void *data, SceUInt32 length)
{
	if (sslAddPropertyValue(certificate->properties, 96, id, value, data,
				length, SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE))
		return 10001;
	if ((id < 0x4700 || id > 0x480f) && id != 14 && id != 65)
		sslInvalidateCertificateProperties(certificate);
	return 0;
}

int sslFreeCertificate(void *object)
{
	SslCertificate *certificate = object;

	if (sslAddReference(&certificate->references, -1, 28,
			    certificateSource, 617) <= 0) {
		if (certificate->properties != NULL) {
			sslFreePropertyList(certificate->properties);
			certificate->properties = NULL;
		}
		sslClearCertificateCache(certificate, 4);
		sslClearCertificateCache(certificate, 7);
		sslClearCertificateCache(certificate, 9);
		sslClearCertificateCache(certificate, 10);
		sslClearCertificateCache(certificate, 11);
		if (certificate->context != NULL)
			sslFreeCertificateContext(certificate->context);
		sslFreePropertyList(&certificate->attributes);
		sslFree(certificate);
	}
	return 0;
}

int sslRetainCertificateContext(void *object)
{
	SslCertificateContext *context = object;

	sslAddReference(&context->references, 1, 28, certificateSource, 371);
	return 0;
}

int sslNewCertificate(void *object, int type, void **result)
{
	SslCertificateContext *context = object;
	SslCertificate *certificate;

	if ((context->supportedTypes & type) != (SceUInt32)type)
		return 10018;
	certificate = sslAllocate(sizeof(*certificate));
	if (certificate == NULL)
		return 10005;
	sslFillMemory(certificate, 0, sizeof(*certificate));
	certificate->methods = context->methods;
	certificate->properties = sslNewPropertyList();
	if (certificate->properties == NULL) {
		sslFree(certificate);
		return 10005;
	}
	certificate->type = type;
	certificate->references = 1;
	sslRetainCertificateContext(context);
	certificate->context = context;
	certificate->backendMethods = context->backendMethods;
	if (context->flags & 0x10)
		certificate->flags |= 0x10;
	sslInitPropertyList(&certificate->attributes);
	*result = certificate;
	return 0;
}

int sslNewCertificateFromData(void *context, int mode, int type, SceUInt32 length,
			      const void *data, SceUInt32 *consumed, void **result)
{
	void *object = NULL;
	SslCertificate *certificate;
	int status;

	status = sslNewCertificate(context, type, &object);
	if (status != 0)
		return status;
	certificate = object;
	status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	if (certificate->backendMethods->Parse != NULL)
		status = certificate->backendMethods->Parse(certificate,
				mode == 0 ? SSL_PROPERTY_COPY_DATA : 0,
				length, data, consumed);
	if (status == 0) {
		if (certificate->backendMethods->Finalize != NULL)
			status = certificate->backendMethods->Finalize(certificate);
		if (status == 10010)
			status = 0;
	}
	if (status != 0) {
		sslFreeCertificate(certificate);
		return status;
	}
	*result = certificate;
	return 0;
}

int sslResetCertificate(SslCertificate *certificate, int mode, const void *data,
			SceUInt32 length)
{
	void *object = NULL;
	SslCertificate *replacement;
	SslProperty *property;
	SceUInt32 consumed;
	int cursor = 0;
	int status;

	sslClearCertificateCache(certificate, 4);
	sslClearCertificateCache(certificate, 7);
	sslClearCertificateCache(certificate, 9);
	sslClearCertificateCache(certificate, 10);
	sslClearCertificateCache(certificate, 11);
	if (data == NULL) {
		sslFreePropertyList(certificate->properties);
		certificate->properties = sslNewPropertyList();
		if (certificate->properties == NULL)
			return 10005;
		sslInitPropertyList(&certificate->attributes);
		return 0;
	}
	status = sslNewCertificateFromData(certificate->context, mode,
					    certificate->type, length, data,
					    &consumed, &object);
	if (status != 0)
		return status;
	replacement = object;
	while (sslNextProperty(certificate->properties, &cursor, &property) == 0) {
		if (property->id >= 0x4700 && property->id <= 0x480f &&
		    sslAddProperty(replacement->properties, property,
				   SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0) {
			status = 10001;
			goto out;
		}
	}
	sslFreePropertyList(certificate->properties);
	certificate->properties = replacement->properties;
	replacement->properties = NULL;
	sslFreePropertyList(&certificate->attributes);
	certificate->attributes = replacement->attributes;
	replacement->attributes = (SslPropertyList){0};
out:
	sslFreeCertificate(replacement);
	return status;
}

int sslCopyCertificateName(SslCertificateName *source, int mode,
			   SslCertificateName **result)
{
	SslCertificateName *name;
	unsigned int flags = SSL_PROPERTY_COPY_DATA;

	name = sslAllocate(sizeof(*name));
	if (name == NULL)
		return 10005;
	sslFillMemory(name, 0, sizeof(*name));
	if (mode == 1)
		flags = SSL_PROPERTY_BORROW_ENTRY;
	name->properties = sslCopyPropertyList(NULL, source->properties, flags);
	if (name->properties == NULL) {
		sslFree(name);
		return 10005;
	}
	sslRetainCertificateContext(source->context);
	name->context = source->context;
	name->methods = source->methods;
	name->flags = source->flags;
	name->backendMethods = source->backendMethods;
	*result = name;
	return 0;
}

int sslSetCertificateSubject(SslCertificate *certificate, int mode,
			     SslCertificateName *source)
{
	SslCertificateName *name;
	int status;

	status = sslCopyCertificateName(source, mode, &name);
	if (status != 0)
		return status;
	sslClearCertificateCache(certificate, 7);
	certificate->subject = name;
	name->flags |= 1;
	certificate->flags |= 1;
	sslInvalidateCertificateProperties(certificate);
	return 0;
}

int sslSetCertificateIssuer(SslCertificate *certificate, int mode,
			    SslCertificateName *source)
{
	SslCertificateName *name;
	int status;

	status = sslCopyCertificateName(source, mode, &name);
	if (status != 0)
		return status;
	sslClearCertificateCache(certificate, 4);
	certificate->issuer = name;
	name->flags |= 1;
	certificate->flags |= 1;
	sslInvalidateCertificateProperties(certificate);
	return 0;
}

int sslSetCertificatePublicKey(SslCertificate *certificate, int mode, void *source)
{
	int status = sslCacheCertificatePublicKey(certificate, mode, source);

	if (status == 0) {
		certificate->publicKey->flags |= 1;
		certificate->flags |= 1;
		sslInvalidateCertificateProperties(certificate);
	}
	return status;
}

int sslSetCertificateVersion(SslCertificate *certificate, int version)
{
	SceUInt32 encodedVersion, tag;
	int status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

	if (certificate->references != 1)
		return 10025;
	if (certificate->backendMethods->EncodeVersion != NULL)
		status = certificate->backendMethods->EncodeVersion(certificate,
						version, &encodedVersion, &tag);
	if (status != 0)
		return status;
	certificate->flags |= 1;
	if (sslAddPropertyValue(certificate->properties, 96, 1, tag, NULL,
				encodedVersion,
				SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0)
		return 10001;
	sslInvalidateCertificateProperties(certificate);
	return 0;
}

int sslNewCertificateContext(SslLibrary *library, int unused, SceUInt32 types,
			     void **result)
{
	SslCertificateContext *context;
	int status;

	(void)unused;
	context = sslAllocate(sizeof(*context));
	if (context == NULL)
		return 10005;
	sslFillMemory(context, 0, sizeof(*context));
	context->references = 1;
	status = sslLibraryAddReference(library);
	if (status != 0)
		goto fail;
	context->supportedTypes = types;
	context->library = library;
	context->cryptoOwnership = 0;
	if (sslGetCachedLibraryObject(library, SSL_CRYPTO_MODULE_TYPE, 0, 0, 0,
				     (SslObjectHeader **)&context->cryptoContext) != 0) {
		sslCreateCryptoContext(library, 0, &context->cryptoContext);
		context->cryptoOwnership = 2;
	}
	status = sslCreateKeyContext(library, 0, 6, &context->keyContext);
	if (status != 0)
		goto fail;
	*result = context;
	return 0;
fail:
	sslFreeCertificateContext(context);
	return status;
}

int sslGetCertificateContextInfo(void *object, int selector, void *result)
{
	SslCertificateContext *context = object;

	switch (selector) {
	case 0:
		*(SslLibrary **)result = context->library;
		break;
	case 1:
		*(SceUInt32 *)result = context->supportedTypes;
		break;
	case 2:
		*(void **)result = context->cryptoContext;
		break;
	case 3:
		*(void **)result = context->keyContext;
		break;
	case 4:
		*(SceUInt32 *)result = context->flags;
		break;
	default:
		return 10018;
	}
	return 0;
}

int sslSetCertificateContextInfo(void *object, int selector, void *value)
{
	SslCertificateContext *context = object;

	switch (selector) {
	case 2:
		if (context->cryptoOwnership == 2)
			sslDestroyCryptoContext(context->cryptoContext);
		context->cryptoContext = value;
		context->cryptoOwnership = 0;
		break;
	case 3:
		if (context->keyContext != NULL)
			sslDestroyKeyContext(context->keyContext);
		context->keyContext = value;
		break;
	case 4:
		context->flags = *(SceUInt32 *)value;
		break;
	default:
		return 10018;
	}
	return 0;
}

int sslRetainCertificate(SslCertificate *certificate, int mode)
{
	int status = 0;

	if (mode != 1)
		return 10018;
	if (certificate->type != 10)
		status = sslCanonicalizeCertificate(certificate);
	if (status == 0)
		sslAddReference(&certificate->references, 1, 28, certificateSource, 784);
	return status;
}

int sslSerializeCertificate(SslCertificate *certificate, SceUInt32 capacity,
			    void *data, SceUInt32 *length)
{
	int status = sslCanonicalizeCertificate(certificate);

	if (status != 0)
		return status;
	if (data == NULL) {
		*length = certificate->encodedSize;
		return 0;
	}
	if (certificate->encodedSize > capacity)
		return 10016;
	if (certificate->backendMethods->Serialize == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return certificate->backendMethods->Serialize(certificate, capacity, data,
						       length);
}
