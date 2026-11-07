/* Filename is ours; common/cert/src follows recovered MES source paths. */
#include <stddef.h>
#include "../../../../CRYPTOCME/common/object.h"
#include <libssl/name.h>
#include "ri_cert.h"
#include "../../../../asn1.h"

#define SSL_ASN1_NAME_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslCertificateNameEncodingSchema[] = {
	/* Name SEQUENCE and the two property slots per attribute. */
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x85, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	/* Look for the next relative distinguished name. */
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x40, 0x40, 0x88, 0x87),
	SSL_ASN1_NAME_COMMAND(0x70, 0x80, 0x2, 0x0),
	/* SET and AttributeTypeAndValue SEQUENCE. */
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x11, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	/* Attribute OID, then its encoded value. */
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x30, 0x6, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x37, 0x68, 0x87, 0x0),
	/* Return to the next relative distinguished name. */
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x70, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

static const SceUInt32 sslCertificateNameSchema[] = {
	/* Name SEQUENCE and the initial property indices. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x85, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x80, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x70, 0x80, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x1, 0x0),
	/* Each relative distinguished name is a SET of attribute SEQUENCEs. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x11, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x3, 0x0),
	SSL_ASN1_NAME_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x0, 0x1, 0x0),
	/* Attribute OID and value become consecutive properties. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x88, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x88),
	SSL_ASN1_NAME_COMMAND(0x40, 0x42, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x88, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x88),
	SSL_ASN1_NAME_COMMAND(0x40, 0x42, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Advance through attributes and relative distinguished names. */
	SSL_ASN1_NAME_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x71, 0x84, 0x3, 0x0),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x71, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x83, 0x80, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x40, 0x43, 0x68, 0xf),
	SSL_ASN1_NAME_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

#undef SSL_ASN1_NAME_COMMAND

int sslCreateCertificateContext(SslLibrary *library, int parameter,
				SceUInt32 flags, SslObjectHeader **result)
{
	return sslConstructLibraryObject(library, 10, 0, flags, parameter, result);
}

int sslDestroyCertificateContext(void *context)
{
	SslCertificateHeader *object = context;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->DestroyContext != NULL)
			status = object->methods->DestroyContext(context);
	}
	return status;
}

int sslSetCertificateContextOption(void *context, int selector, void *value)
{
	SslCertificateHeader *object = context;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && value != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->SetContextInfo != NULL)
			status = object->methods->SetContextInfo(context, selector, value);
	}
	return status;
}

int sslCreateCertificate(void *context, int flags, void **result)
{
	SslCertificateHeader *object = context;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->CreateCertificate != NULL)
			status = object->methods->CreateCertificate(context, flags, result);
	}
	return status;
}

int SceSslInternal_144FA354(void *certificate)
{
	SslCertificateHeader *object = certificate;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->DestroyCertificate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->DestroyCertificate(certificate);
}

int sslRetainCertificateObject(SslCertificate *certificate)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (certificate->methods->RetainCertificate != NULL)
			status = certificate->methods->RetainCertificate(certificate, 1, 0);
	}
	return status;
}

int sslDuplicateCertificateObject(SslCertificate *certificate)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (certificate->methods->RetainCertificate != NULL)
			status = certificate->methods->RetainCertificate(certificate, 1, 0);
	}
	return status;
}

int SceSslInternal_64BDF1BB(void *certificate, int selector, void *result)
{
	SslCertificate *object = certificate;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->GetCertificateInfo != NULL)
			status = object->methods->GetCertificateInfo(object, selector,
								     result);
	}
	return status;
}

int sslSetCertificateObjectInfo(SslCertificate *certificate, int selector,
				void *value)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL && value != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (certificate->methods->SetCertificateInfo != NULL)
			status = certificate->methods->SetCertificateInfo(certificate,
									  selector, value);
	}
	return status;
}

int sslCreateCertificateNameObject(SslCertificateContext *context, void **result)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (context != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (context->methods->nameMethods->Create != NULL)
			status = context->methods->nameMethods->Create(context, result);
	}
	return status;
}

int SceSslInternal_CE37B69E(void *name)
{
	SslObjectHeader *object = name;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->Destroy(name);
}

int sslCopyCertificateNameObject(SslCertificateName *source, int mode,
				 SslCertificateName **result)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (source != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (source->methods->Copy != NULL)
			status = source->methods->Copy(source, mode, result);
	}
	return status;
}

int sslCreateCertificateNameComponentObject(SslCertificateName *name,
					    SslCertificateNameComponent **result)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (name != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (name->methods->CreateComponent != NULL)
			status = name->methods->CreateComponent(name, result);
	}
	return status;
}

int SceSslInternal_17BF2367(void *component)
{
	SslCertificateNameComponent *object = component;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->DestroyComponent != NULL)
			status = object->methods->DestroyComponent(object);
	}
	return status;
}

int SceSslInternal_B5A099D2(void *component, int selector, void *result)
{
	SslCertificateNameComponent *object = component;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->GetComponentInfo != NULL)
			status = object->methods->GetComponentInfo(object, selector,
								   result);
	}
	return status;
}

int SceSslInternal_306E2A22(void *certificate, void *key, void *cryptoContext)
{
	SslCertificate *object = certificate;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && key != NULL && cryptoContext != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->keyMethods != NULL &&
		    object->methods->keyMethods->VerifySignature != NULL)
			status = object->methods->keyMethods->VerifySignature(object, key,
									      cryptoContext);
	}
	return status;
}

int sslSerializeCertificateObject(SslCertificate *certificate, SceUInt32 capacity,
				  void *data, SceUInt32 *length)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL && length != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (certificate->methods->SerializeCertificate != NULL)
			status = certificate->methods->SerializeCertificate(certificate,
									    capacity, data,
									    length);
	}
	return status;
}

int SceSslInternal_29AAC768(void *context, int mode, int type,
			    SceSize length, const void *data,
			    void **consumed, void **result)
{
	SslCertificateHeader *object = context;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL && data != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->NewCertificateFromData != NULL)
			status = object->methods->NewCertificateFromData(context, mode,
									 type, length,
									 data,
									 (SceUInt32 *)consumed,
									 result);
	}
	return status;
}

int SceSslInternal_170D595D(void *certificate, int mode, void **result)
{
	SslCertificate *object = certificate;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->keyMethods != NULL &&
		    object->methods->keyMethods->GetPublicKey != NULL)
			status = object->methods->keyMethods->GetPublicKey(object, mode,
									   result);
	}
	return status;
}

int sslDecodeCertificateNameObject(SslCertificateContext *context, int mode,
				   SceUInt32 length, const void *data,
				   SceUInt32 *consumed, void **result)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (context != NULL && result != NULL && data != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (context->methods->nameMethods != NULL &&
		    context->methods->nameMethods->Decode != NULL)
			status = context->methods->nameMethods->Decode(context, mode,
								       length, data,
								       consumed, result);
	}
	return status;
}

int sslEncodeCertificateNameObject(SslCertificateName *name, SceUInt32 capacity,
				   void *data, SceUInt32 *length)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (name != NULL && length != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (name->methods->Encode != NULL)
			status = name->methods->Encode(name, capacity, data, length);
	}
	return status;
}

int SceSslInternal_DABB61EE(void *name, SceUInt32 capacity, char *data)
{
	SslCertificateName *object = name;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && data != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->Format != NULL)
			status = object->methods->Format(object, 1, capacity, data);
	}
	return status;
}

int SceSslInternal_91C2531C(void *name, int selector, void *result)
{
	SslCertificateName *object = name;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (object != NULL && result != NULL) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		if (object->methods->GetInfo != NULL)
			status = object->methods->GetInfo(object, selector, result);
	}
	return status;
}

int sslDecodeCertificateNameProperties(SslPropertyList *properties,
				       const SceUInt8 *data, SceUInt32 length,
				       SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslCertificateNameSchema, properties, data, length,
				       consumed);
}

int sslEncodeCertificateNameProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslCertificateNameEncodingSchema, properties, data,
				       length, capacity);
}
