/* Source path recovered from the firmware; descriptive private names are ours. */
#include <libssl/object.h>
#include <libssl/name.h>
#include "ri_cert.h"
#include "extension.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"
#include "../../../../CRYPTOCME/common/key/pkey_ctx.h"
#include "../../../../CRYPTOCME/common/module/crypto/cry_api.h"

static const char certificateSource[] =
	"S:\\work\\MES\\common\\cert/src/ri_cert.c";

static const unsigned char extensionCritical = 0xff;

int sslValidateCriticalCertificateExtensions(SslCertificate *certificate)
{
	SslCertificateContext *context = NULL;
	const SslObjectMethods *methods = NULL;
	SslCertificateExtension *extension = NULL;
	SslCertificateExtensionData oid;
	SceUInt32 index = 0;
	int critical = 0, propertyType, status;

	status = certificate->methods->GetCertificateInfo(certificate, 0x8005, &context);
	if (status != 0)
		return status;
	status = context->methods->GetContextInfo(context, 0, &methods);
	if (status != 0)
		return status;
	status = SceSslInternal_BF99D664(methods, 0, &extension);
	if (status == 0) {
		status = sslSetCertificateExtensionInfo(extension, 0x8006, &index);
		while (status == 0) {
			if (certificate->methods->GetCertificateInfo(certificate, 0x8001, extension) != 0)
				break;
			status = SceSslInternal_93EAE08A(extension, 0x8001, &critical);
			if (status != 0)
				break;
			if (critical == 1) {
				status = SceSslInternal_93EAE08A(extension, 0x8003, &oid);
				if (status != 0)
					break;
				if (sslFindCertificateExtensionPropertyType(oid.data, oid.length,
								   &propertyType) == 10008) {
					status = 10001;
					break;
				}
			}
			critical = 0;
			index++;
			status = sslSetCertificateExtensionInfo(extension, 0x8006, &index);
		}
	}
	if (extension != NULL)
		SceSslInternal_F030A0DE(extension);
	return status;
}

int sslGetCertificateExtension(SslPropertyList *list, int mode,
			       SslCertificateExtension *extension)
{
	SslProperty *property;
	SslCertificateExtensionData value, oid;
	SceUInt32 base = 3u * (SceUInt32)extension->index;
	int status;

	if (sslFindProperty(list, 108, (int)(base + 4u), NULL, &property, 0) != 0)
		return 10008;
	value.length = property->length;
	value.data = property->data;
	base = 3u * (SceUInt32)extension->index;
	if (sslFindProperty(list, 108, (int)(base + 2u), NULL, &property, 0) != 0)
		return 10008;
	oid.length = property->length;
	oid.data = property->data;
	status = sslSetCertificateExtensionData(extension, SSL_CERT_EXTENSION_OID,
					      mode, &oid);
	if (status != 0)
		return status;
	status = sslSetCertificateExtensionData(extension, SSL_CERT_EXTENSION_VALUE,
					      mode, &value);
	if (status != 0)
		return status;
	base = 3u * (SceUInt32)extension->index;
	extension->critical =
		sslFindProperty(list, 108, (int)(base + 3u), NULL, &property, 0) == 0 &&
		(property->length != 1 || *(unsigned char *)property->data != 0);
	return 0;
}

int sslGetCertificateExtensionCount(SslPropertyList *list, SceUInt32 *result)
{
	SslProperty *property;

	if (sslFindProperty(list, 108, 1, NULL, &property, 0) != 0)
		return 10008;
	*result = property->length;
	return 0;
}

int sslFindCertificateExtension(SslPropertyList *list, int mode,
				SslCertificateExtension *extension)
{
	const char *oid;
	SceUInt32 length;
	SslProperty *property;
	int index, relative;

	if (extension->type == SSL_CERT_EXTENSION_CUSTOM) {
		oid = extension->oid.data;
		if (oid == NULL || extension->oid.length == 0)
			return 10008;
		length = extension->oid.length;
	} else {
		if (sslGetCertificateExtensionOid(extension->type, &oid, &length) != 0)
			return 10008;
	}
	for (index = 0; index < list->count; index++) {
		property = list->entries[index];
		relative = (int)((SceUInt32)property->id - 2u);
		if (relative % 3 != 0 || property->length != length)
			continue;
		if (sslCompareMemory(property->data, oid, length) != 0)
			continue;
		property = list->entries[index];
		extension->index = (int)((SceUInt32)property->id - 2u) / 3;
		return sslGetCertificateExtension(list, mode, extension);
	}
	return 10008;
}

int sslFindCertificateExtensionByType(SslPropertyList *list, int type, int mode,
				      SslCertificateExtension *extension)
{
	if (extension == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	extension->type = type;
	return sslFindCertificateExtension(list, mode, extension);
}

int sslStoreCertificateExtension(SslPropertyList *list, int mode,
				 SslCertificateExtension *extension)
{
	SslCertificateExtension probe = {0};
	SslProperty *property;
	SceUInt32 count = 0, base, length;
	unsigned int flags = SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE;
	const char *oid;
	int newCount = 1;

	if (extension == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	probe.type = extension->type;
	if (sslFindCertificateExtension(list, 1, &probe) == 0) {
		base = 3u * (SceUInt32)probe.index;
		newCount = -1;
	} else {
		if (sslFindProperty(list, 108, 1, NULL, &property, 0) == 0) {
			count = property->length;
			newCount = (int)(count + 1u);
		}
		base = 3u * count;
	}
	if (mode == 1)
		flags = SSL_PROPERTY_BORROW_ENTRY | SSL_PROPERTY_REPLACE;
	if (sslAddPropertyValue(list, 108, (int)(base + 4u), 0,
				extension->value.data, extension->value.length, flags) != 0)
		return 10005;
	length = extension->oid.length;
	oid = extension->oid.data;
	if (extension->type != SSL_CERT_EXTENSION_CUSTOM)
		sslGetCertificateExtensionOid(extension->type, &oid, &length);
	if (sslAddPropertyValue(list, 108, (int)(base + 2u), 0,
				oid, length, flags) != 0)
		return 10005;
	if (extension->critical != 0) {
		if (sslAddPropertyValue(list, 108, (int)(base + 3u), 0,
					&extensionCritical, 1, flags) != 0)
			return 10005;
	} else {
		sslDeleteProperty(list, 108, (int)(base + 3u));
	}
	if (newCount > 0 && sslAddPropertyValue(list, 108, 1, 0, NULL,
					       newCount, flags) != 0)
		return 10005;
	return 0;
}

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

int sslGetCertificateProperty(SslCertificate *certificate, SceUInt32 selector,
			       void *result)
{
	SslCertificatePropertyValue *value = result;
	SslCertificateExtensionData *buffer = result;
	SslCertificateIndexedData *indexed = result;
	SslProperty *property;
	int dirty = 0, allowMissing = 0, scalar = 0, status;

	if (selector >= 0x4700 && selector <= 0x480f)
		goto getBuffer;
	switch (selector) {
	case 2:
	case 18:
	case 23:
		goto getBuffer;
	case 4:
		dirty = certificate->issuer != NULL && (certificate->issuer->flags & 1);
		break;
	case 7:
		dirty = certificate->subject != NULL && (certificate->subject->flags & 1);
		break;
	case 9:
		dirty = certificate->publicKey != NULL && (certificate->publicKey->flags & 1);
		break;
	case 10:
	case 11:
		dirty = certificate->propertyCaches[selector - 10] != NULL &&
			(certificate->propertyCaches[selector - 10]->flags & 1);
		allowMissing = 1;
		break;
	case 12:
	case 16:
		allowMissing = 1;
		break;
	case 14:
	case 64:
	case 65:
	case 127:
		dirty = certificate->flags & 1;
		break;
	case 5:
	case 6:
	case 17:
	case 19:
	case 20:
	case 0x8009:
		break;
	case 25:
		*(SceUInt32 *)result = 64;
		allowMissing = 1;
		goto getScalar;
	case 0x4000:
	case 0x4001:
	case 0x4003:
	case 0x4004:
	case 0x4005:
	case 0x4010:
	case 0x4011:
	case 0x4015:
	case 0x8010:
		return sslFindCertificateExtensionByType(&certificate->attributes,
							 (int)selector, 1, result);
	case 0x8004:
	case 0x800e:
		*(int *)result = certificate->type;
		return 0;
	case 0x8005:
		*(SslCertificateContext **)result = certificate->context;
		return 0;
	case 0x800d:
		*(const SslCertificateMethods **)result = certificate->methods;
		return 0;
	case 0x8011:
		if (sslFindProperty(certificate->properties, 114, 1, NULL, &property, 0) != 0)
			return 10008;
		*(SceUInt32 *)result = property->length;
		return 0;
	case 0x8012:
		if (sslFindProperty(certificate->properties, 114,
				     (int)(indexed->index + 0x100u), NULL, &property, 0) == 0) {
			indexed->data = property->data;
			indexed->length = property->length;
			return 0;
		}
		break;
	case 0x8013:
		goto getScalar;
	default:
		return 10021;
	}
	if (dirty && certificate->type != 10) {
		status = sslCanonicalizeCertificate(certificate);
		if (status != 0)
			return status;
	}
	property = NULL;
	status = 10008;
	if (sslFindProperty(certificate->properties, 96, (int)selector, NULL, &property, 0) == 0) {
		value->value = property->value;
		value->data = property->data;
		value->length = property->length;
		status = 0;
	}
	goto finish;
getBuffer:
	property = NULL;
	if (sslFindProperty(certificate->properties, 96, (int)selector, NULL, &property, 0) != 0)
		return 10008;
	buffer->data = property->data;
	buffer->length = property->length;
	return 0;
getScalar:
	property = NULL;
	status = 10008;
	if (sslFindProperty(certificate->properties, 96, (int)selector, NULL, &property, 0) == 0) {
		*(SceUInt32 *)result = property->length;
		status = 0;
	}
	scalar = 1;
finish:
	if (allowMissing && status == 10008) {
		if (!scalar) {
			value->value = 0;
			value->length = 0;
			value->data = NULL;
		}
		status = 0;
	}
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

int sslGetCertificatePublicKey(SslCertificate *certificate, int mode, void **result)
{
	SslCertificateKeyCache *cache = certificate->publicKey;
	SslProperty *property;
	void *decoded = NULL, *key = NULL, *context, *source;
	int algorithm, status;

	if (cache != NULL && cache->key != NULL) {
		source = cache->key;
		algorithm = cache->algorithm;
		status = sslGetPublicKeyInfo(source, SSL_KEY_INFO_CONTEXT, &context);
		if (status == 0) {
			status = sslCreatePublicKey(context, algorithm, &key);
			if (status == 0) {
				status = sslCopyPublicKey(source, key, mode == 1);
				if (status != 0)
					SceSslInternal_46358909(key);
				else
					decoded = key;
			}
		}
	} else if (sslFindProperty(certificate->properties, 96, 9, NULL, &property, 0) != 0) {
		status = 10008;
	} else {
		status = 10009;
		if (certificate->backendMethods->DecodePublicKey != NULL)
			status = certificate->backendMethods->DecodePublicKey(certificate, property,
									 mode, &decoded);
		if (status == 0 && certificate->references == 1)
			status = sslCacheCertificatePublicKey(certificate, mode, decoded);
	}
	if (status == 0)
		*result = decoded;
	else if (decoded != NULL)
		SceSslInternal_46358909(decoded);
	return status;
}

int sslGetCertificateInfo(SslCertificate *certificate, int selector, void *result)
{
	SslCertificateExtension *extension = NULL;
	const SslObjectMethods *methods = NULL;
	SslCertificateExtension probe;
	SslCertificateInfoRequest *requests;
	SslProperty *property;
	SceUInt32 count, encodedVersion = 0;
	int index, present = 0, status, value;

	switch (selector) {
	case 1:
		property = NULL;
		if (sslFindProperty(certificate->properties, 96, 1, NULL, &property, 0) == 0) {
			present = 1;
			encodedVersion = property->length;
		}
		if (certificate->backendMethods->DecodeVersion == NULL)
			return 10009;
		return certificate->backendMethods->DecodeVersion(certificate, present,
								 encodedVersion, result);
	case 0x4201:
	case 0x4202:
	case 0x4207:
	case 0x4401:
	case 0x4402:
	case 0x4407:
		count = 0;
		sslFillMemory(&probe, 0, sizeof(probe));
		status = sslGetCertificateExtensionCount(&certificate->attributes, &count);
		if (status != 0)
			return status;
		for (index = 0; index < (int)count; index++) {
			sslSetCertificateExtensionInfo(&probe, 0x8006, &index);
			if (sslGetCertificateExtension(&certificate->attributes, 1, &probe) == 0 &&
			    SceSslInternal_93EAE08A(&probe, selector, result) == 0)
				return 0;
		}
		return 10008;
	case 0x8000:
		return sslGetCertificateExtensionCount(&certificate->attributes, result);
	case 0x8001:
		return sslGetCertificateExtension(&certificate->attributes, 1, result);
	case 0x8002:
		return sslFindCertificateExtension(&certificate->attributes, 1, result);
	case 0x8006:
		if (certificate->backendMethods->QueryNonzeroValue == NULL)
			return 10009;
		value = certificate->backendMethods->QueryNonzeroValue(certificate);
		*(int *)result = value;
		return value != 0 ? 0 : 10001;
	case 0x8007:
		if (certificate->backendMethods->GetSignatureAlgorithm == NULL)
			return 10009;
		value = certificate->backendMethods->GetSignatureAlgorithm(certificate);
		*(int *)result = value;
		return value != -1 ? 0 : 10001;
	case 0x8008:
		if (certificate->backendMethods->QuerySignedValue == NULL)
			return 10009;
		value = certificate->backendMethods->QuerySignedValue(certificate);
		*(int *)result = value;
		return value != -1 ? 0 : 10001;
	case 0x8009:
		return sslGetCertificatePublicKey(certificate, 1, result);
	case 0x800a:
		return SceSslInternal_81FA5DBE(certificate, 1, result);
	case 0x800b:
		return SceSslInternal_08179EBD(certificate, 1, result);
	case 0x800c:
		requests = result;
		while (requests->selector != 0) {
			status = sslGetCertificateInfo(certificate, requests->selector, requests->result);
			if (status != 0)
				return status;
			requests++;
		}
		return 0;
	case 0x8010:
		status = sslGetCertificateMethods(certificate, &methods);
		if (status != 0)
			return status;
		status = SceSslInternal_BF99D664(methods, 0, &extension);
		if (status != 0)
			return status;
		sslFindCertificateExtensionByType(&certificate->attributes, 0x4001, 1, extension);
		status = sslFindCertificateExtension(&certificate->attributes, 1, extension);
		if (status == 0)
			status = SceSslInternal_93EAE08A(extension, 0x4001, result);
		SceSslInternal_F030A0DE(extension);
		return status;
	case 0x8014:
		*(SceUInt32 *)result = certificate->flags;
		return 0;
	case 0x8015:
		if (certificate->backendMethods->GetSignatureAlgorithmEncoding == NULL)
			return 10009;
		return certificate->backendMethods->GetSignatureAlgorithmEncoding(certificate, result);
	default:
		return sslGetCertificateProperty(certificate, (SceUInt32)selector, result);
	}
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

int sslSetCertificateEncodedProperty(SslCertificate *certificate, int id,
				     SceUInt32 value, const void *data,
				     SceUInt32 length)
{
	SslPropertyList list;
	int status = 0;

	sslInitPropertyList(&list);
	if (id >= 0x4700 && id <= 0x480f) {
		value = 0;
	} else {
		switch (id) {
		case 0x800e:
			if (value != certificate->type)
				status = 10021;
			goto out;
		case 2:
		case 18:
			value = 2;
			certificate->flags |= 1;
			break;
		case 23:
			value = 3;
			certificate->flags |= 1;
			break;
		case 25:
			value = 6;
			certificate->flags |= 1;
			break;
		case 0x8013:
			value = 10;
			certificate->flags |= 1;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 9:
		case 10:
		case 11:
		case 12:
		case 16:
		case 17:
		case 19:
		case 20:
		case 127:
			certificate->flags |= 1;
			break;
		case 14:
		case 64:
		case 65:
			break;
		default:
			status = 10021;
			goto out;
		}
	}
	if (id == 127)
		status = sslResetCertificate(certificate, (int)value, data, length);
	else
		status = sslSetCertificateProperty(certificate, id, value, data, length);
	if (status != 0)
		goto out;
	if (id == 12 && certificate->backendMethods->Finalize != NULL)
		certificate->backendMethods->Finalize(certificate);
	status = sslClearCertificateCache(certificate, id);
out:
	sslFreePropertyList(&list);
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

int sslSetCertificateInfo(SslCertificate *certificate, int selector, void *value)
{
	SslCertificatePropertyValue *encoded = value;
	SslCertificateExtensionData *buffer = value;
	SslCertificateIndexedData *indexed = value;
	SslCertificateInfoRequest *requests;
	SslCertificateIndexedData probe;
	SslCertificateExtension *extension = NULL;
	const SslObjectMethods *methods;
	SceUInt32 count;
	int id = selector, status;

	if (selector == 0x4701)
		id = *(int *)value;
	if (certificate->references != 1)
		return 10025;
	if (selector >= 0x4700 && selector <= 0x480f && selector != 0x4701)
		return sslSetCertificateEncodedProperty(certificate, selector, 0,
						 buffer->data, buffer->length);
	switch (selector) {
	case 1:
		return sslSetCertificateVersion(certificate, *(int *)value);
	case 2:
	case 18:
	case 23:
		return sslSetCertificateEncodedProperty(certificate, selector, 0,
						 buffer->data, buffer->length);
	case 25:
	case 0x8013:
		return sslSetCertificateEncodedProperty(certificate, selector, 0,
						 NULL, *(SceUInt32 *)value);
	case 0x4701:
		if (sslDeleteProperty(certificate->properties, 96, id) != 0)
			return 10001;
		if (id < 0x4700 || id > 0x480f)
			sslInvalidateCertificateProperties(certificate);
		return 0;
	case 0x8001:
		status = sslStoreCertificateExtension(&certificate->attributes, 0, value);
		if (status == 0)
			certificate->flags |= 1;
		return status;
	case 0x8005:
		certificate->context = value;
		return 0;
	case 0x8009:
		return sslSetCertificatePublicKey(certificate, 0, value);
	case 0x800a:
		return sslSetCertificateIssuer(certificate, 0, value);
	case 0x800b:
		return sslSetCertificateSubject(certificate, 0, value);
	case 0x800c:
		requests = value;
		while (requests->selector != 0) {
			status = sslSetCertificateInfo(certificate, requests->selector, requests->result);
			if (status != 0)
				return status;
			requests++;
		}
		return 0;
	case 0x8010:
		status = sslGetCertificateMethods(certificate, &methods);
		if (status != 0)
			return status;
		status = SceSslInternal_BF99D664(methods, 0, &extension);
		if (status != 0)
			return status;
		status = sslSetCertificateExtensionInfo(extension, 0x4001, value);
		if (status == 0) {
			status = sslSetCertificateInfo(certificate, 0x8001, extension);
			certificate->flags |= 1;
		}
		SceSslInternal_F030A0DE(extension);
		return status;
	case 0x8012:
		count = 0;
		sslGetCertificateProperty(certificate, 0x8011, &count);
		if (indexed->index == (SceUInt32)-1) {
			count++;
		} else {
			probe.index = indexed->index;
			if (sslGetCertificateProperty(certificate, 0x8012, &probe) != 0)
				count++;
		}
		if (sslAddPropertyValue(certificate->properties, 114,
					(int)(indexed->index + 0x100u), 0,
					indexed->data, indexed->length,
					SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0)
			return 10001;
		if (sslAddPropertyValue(certificate->properties, 114, 1, 0, NULL,
					count, SSL_PROPERTY_REPLACE) != 0)
			return 10001;
		return 0;
	case 0x8014:
		certificate->flags = *(SceUInt32 *)value;
		return 0;
	default:
		if (value == NULL)
			return 10017;
		return sslSetCertificateEncodedProperty(certificate, selector, encoded->value,
						 encoded->data, encoded->length);
	}
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

int sslNewCertificateName(SslCertificateContext *context, SslCertificateName **result)
{
	SslCertificateName *name;

	name = sslAllocate(sizeof(*name));
	if (name == NULL)
		return 10005;
	sslFillMemory(name, 0, sizeof(*name));
	name->properties = sslNewPropertyList();
	if (name->properties == NULL) {
		sslFreeCertificateName(name);
		return 10005;
	}
	sslRetainCertificateContext(context);
	name->context = context;
	name->backendMethods = context->backendMethods;
	if (context->methods != NULL)
		name->methods = (const SslObjectMethods *)context->methods->nameMethods;
	*result = name;
	return 0;
}
