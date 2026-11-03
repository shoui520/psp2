/* Source path recovered from the firmware; descriptive private names are ours. */
#include <libssl/object.h>
#include "ri_cert.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"
#include "../../../../CRYPTOCME/common/key/pkey_ctx.h"
#include "../../../../CRYPTOCME/common/module/crypto/cry_api.h"

static const char certificateSource[] =
	"S:\\work\\MES\\common\\cert/src/ri_cert.c";

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

int sslCopyCertificateName(SslCertificateName *source, int mode, void **result)
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
	name->opaqueData = source->opaqueData;
	*result = name;
	return 0;
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
