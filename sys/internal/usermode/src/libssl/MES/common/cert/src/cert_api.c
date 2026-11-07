/* Filename and descriptive interface names are ours. This component path is
 * recovered from MES/common/cert/src/ri_cert.c.
 */
#include <stddef.h>
#include <libssl/name.h>
#include <libssl/object.h>
#include "ri_cert.h"
#include "../../../../memory.h"
#include "../../../../objects/objects.h"
#include "../../../../CRYPTOCME/common/key/pkey_ctx.h"
#include "../../../../CRYPTOCME/common/library/lib_ctx.h"
#include "../../../../CRYPTOCME/common/module/crypto/cry_api.h"

static const SslLibraryValueMap sslCertificateAttributeOids[] = {
	{ 13, (int)(SceUIntPtr)"\x55\x04\x03", 0 },
	{ 14, (int)(SceUIntPtr)"\x55\x04\x06", 0 },
	{ 15, (int)(SceUIntPtr)"\x55\x04\x07", 0 },
	{ 166, (int)(SceUIntPtr)"\x55\x04\x09", 0 },
	{ 16, (int)(SceUIntPtr)"\x55\x04\x08", 0 },
	{ 17, (int)(SceUIntPtr)"\x55\x04\x0a", 0 },
	{ 18, (int)(SceUIntPtr)"\x55\x04\x0b", 0 },
	{ 99, (int)(SceUIntPtr)"\x55\x04\x2a", 0 },
	{ 100, (int)(SceUIntPtr)"\x55\x04\x04", 0 },
	{ 101, (int)(SceUIntPtr)"\x55\x04\x2b", 0 },
	{ 102, (int)(SceUIntPtr)"\x55\x04\x2d", 0 },
	{ 105, (int)(SceUIntPtr)"\x55\x04\x05", 0 },
	{ 106, (int)(SceUIntPtr)"\x55\x04\x0c", 0 },
	{ 107, (int)(SceUIntPtr)"\x55\x04\x0d", 0 },
	{ 48, (int)(SceUIntPtr)"\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01", 0 },
	{ 170, (int)(SceUIntPtr)"\x09\x92\x26\x89\x93\xf2\x2c\x64\x01\x19", 0 },
	{ 167, (int)(SceUIntPtr)"\x55\x04\x2c", 0 },
	{ 168, (int)(SceUIntPtr)"\x55\x04\x2e", 0 },
	{ 169, (int)(SceUIntPtr)"\x09\x92\x26\x89\x93\xf2\x2c\x64\x01\x01", 0 },
	{ 237, (int)(SceUIntPtr)"\x55\x04\x0f", 0 },
	{ 238, (int)(SceUIntPtr)"\x2b\x06\x01\x04\x01\x82\x37\x3c\x02\x01\x01", 0 },
	{ 239, (int)(SceUIntPtr)"\x2b\x06\x01\x04\x01\x82\x37\x3c\x02\x01\x02", 0 },
	{ 240, (int)(SceUIntPtr)"\x2b\x06\x01\x04\x01\x82\x37\x3c\x02\x01\x03", 0 },
	{ 0, 0, 0 }
};

int sslGetCertificateMethods(SslCertificate *certificate, void *result)
{
	SslCertificateContext *context;
	int status;

	status = certificate->methods->GetCertificateInfo(certificate, 0x8005, &context);
	if (status == 0)
		status = context->methods->GetContextInfo(context, 0, result);
	return status;
}

int sslAllocateAndSerializeCertificate(SslCertificate *certificate, void **data,
				       SceUInt32 *length)
{
	int status;

	status = certificate->methods->SerializeCertificate(certificate, 0, NULL,
							    length);
	if (status == 0) {
		*data = sslAllocate(*length);
		if (*data == NULL) {
			status = 10005;
		} else {
			status = certificate->methods->SerializeCertificate(certificate,
									    *length, *data,
									    length);
			if (status != 0) {
				sslFree(*data);
				*data = NULL;
			}
		}
	}
	return status;
}

int sslGetCertificateName(SslCertificate *certificate, int selector, int mode,
			  void **result)
{
	SslCertificatePropertyValue encoded;
	SslCertificateContext *context;
	void *name = NULL;
	SceUInt32 consumed = 0;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate == NULL || result == NULL)
		return status;
	status = certificate->methods->GetCertificateInfo(certificate, selector, &encoded);
	if (status == 0)
		status = certificate->methods->GetCertificateInfo(certificate, 0x8005, &context);
	if (status == 0)
		status = context->methods->nameMethods->Decode(context, mode, encoded.length,
							     encoded.data, &consumed, &name);
	if (status == 0)
		*result = name;
	else if (name != NULL)
		((SslObjectHeader *)name)->methods->Destroy(name);
	return status;
}

int SceSslInternal_81FA5DBE(void *certificate, int mode, void **result)
{
	return sslGetCertificateName(certificate, 4, mode, result);
}

int SceSslInternal_08179EBD(void *certificate, int mode, void **result)
{
	return sslGetCertificateName(certificate, 7, mode, result);
}

int SceSslInternal_D68D8421(void *certificate, void *publicKey)
{
	SslCertificate *object = certificate;
	void *certificateKey;
	int matched = 0;

	if (object != NULL && publicKey != NULL) {
		certificateKey = NULL;
		if (object->methods->keyMethods->GetPublicKey(object, 1,
							      &certificateKey) == 0) {
			if (sslComparePublicKeys(publicKey, certificateKey) == 0)
				matched = 1;
			SceSslInternal_46358909(certificateKey);
		}
	}
	return matched;
}

int sslCompareCertificates(SslCertificate *left, SslCertificate *right)
{
	SceUInt32 leftLength, rightLength;
	void *leftData, *rightData = NULL;
	int result;

	if (left == NULL && right == NULL) {
		result = 0;
	} else if (left == NULL) {
		result = -1;
	} else if (right == NULL) {
		result = 1;
	} else if (left->methods->SerializeCertificate(left, 0, NULL,
						       &leftLength) != 0) {
		result = -1;
	} else if (right->methods->SerializeCertificate(right, 0, NULL,
							&rightLength) != 0) {
		result = 1;
	} else {
		result = (int)(leftLength - rightLength);
		if (result == 0) {
			leftData = sslAllocate(leftLength);
			if (leftData == NULL) {
				result = -1;
			} else if (left->methods->SerializeCertificate(left, leftLength,
								       leftData,
								       &leftLength) != 0) {
				result = -1;
			} else {
				rightData = sslAllocate(rightLength);
				if (rightData == NULL) {
					result = 1;
				} else if (right->methods->SerializeCertificate(right,
										rightLength,
										rightData,
										&rightLength) != 0) {
					result = 1;
				} else {
					result = sslCompareMemory(leftData, rightData,
								  leftLength);
				}
			}
			if (leftData != NULL)
				sslFree(leftData);
			if (rightData != NULL)
				sslFree(rightData);
		}
	}
	return result;
}

int sslAreCertificatesEqual(SslCertificate *left, SslCertificate *right)
{
	int equal = 0;

	if (sslCompareCertificates(left, right) == 0)
		equal = 1;
	return equal;
}

int sslCompareCertificateNames(SslCertificateName *left,
			       SslCertificateName *right)
{
	SceUInt32 leftLength, rightLength;
	void *leftData, *rightData = NULL;
	int result;

	if (left == NULL && right == NULL) {
		result = 0;
	} else if (left == NULL) {
		result = -1;
	} else if (right == NULL) {
		result = 1;
	} else {
		leftLength = 0;
		rightLength = 0;
		if (left->methods->Encode(left, 0, NULL, &leftLength) != 0) {
			result = -1;
		} else if (right->methods->Encode(right, 0, NULL, &rightLength) != 0) {
			result = 1;
		} else {
			result = (int)(leftLength - rightLength);
			if (result == 0) {
				leftData = sslAllocate(leftLength);
				if (leftData == NULL) {
					result = -1;
				} else if (left->methods->Encode(left, leftLength,
								 leftData,
								 &leftLength) != 0) {
					result = -1;
				} else {
					rightData = sslAllocate(rightLength);
					if (rightData == NULL) {
						result = 1;
					} else if (right->methods->Encode(right,
									  rightLength,
									  rightData,
									  &rightLength) != 0) {
						result = 1;
					} else {
						result = sslCompareMemory(leftData,
									  rightData,
									  leftLength);
					}
				}
				if (leftData != NULL)
					sslFree(leftData);
				if (rightData != NULL)
					sslFree(rightData);
			}
		}
	}
	return result;
}

int sslCompareCertificateSubjects(SslCertificate *left, SslCertificate *right)
{
	int leftType, rightType, result;
	void *leftName, *rightName;

	if (left == NULL && right == NULL) {
		result = 0;
	} else if (left == NULL) {
		result = -1;
	} else if (right == NULL) {
		result = 1;
	} else {
		leftName = NULL;
		rightName = NULL;
		if (left->methods->GetCertificateInfo(left, 0x8004, &leftType) != 0) {
			result = -1;
		} else if (right->methods->GetCertificateInfo(right, 0x8004,
							      &rightType) != 0) {
			result = 1;
		} else if (leftType != rightType) {
			result = 1;
		} else if (sslGetCertificateName(left, 7, 1, &leftName) != 0) {
			result = -1;
		} else if (sslGetCertificateName(right, 7, 1, &rightName) != 0) {
			result = -1;
		} else {
			result = sslCompareCertificateNames(leftName, rightName);
		}
		if (leftName != NULL)
			((SslObjectHeader *)leftName)->methods->Destroy(leftName);
		if (rightName != NULL)
			((SslObjectHeader *)rightName)->methods->Destroy(rightName);
	}
	return result;
}

int sslAreCertificateSubjectsEqual(SslCertificate *left, SslCertificate *right)
{
	int equal = 0;

	if (sslCompareCertificateSubjects(left, right) == 0)
		equal = 1;
	return equal;
}

int SceSslInternal_568F0E80(void *left, void *right)
{
	int equal = 0;

	if (sslCompareCertificateNames(left, right) == 0)
		equal = 1;
	return equal;
}

int sslComputeCertificateDigest(SslCertificate *certificate, int selector,
				int algorithm, SceUInt32 capacity, void *digest,
				SceUInt32 *digestLength)
{
	SslCertificateContext *certContext;
	SslCryptoContextHeader *cryptoContext = NULL;
	SslCryptoAlgorithm *digestAlg = NULL;
	void *encoded;
	SceUInt32 encodedLength;
	SceUInt32 outLength = capacity;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL && digestLength != NULL) {
		switch (algorithm) {
		case 3:
			*digestLength = 16;
			break;
		case 4:
			*digestLength = 16;
			break;
		case 64:
			*digestLength = 20;
			break;
		default:
			return 10018;
		}
		if (digest == NULL)
			return 0;
		if (selector == 0x8003) {
			encoded = NULL;
			status = sslAllocateAndSerializeCertificate(certificate, &encoded,
								    &encodedLength);
			if (status == 0) {
				status = certificate->methods->GetCertificateInfo(certificate,
										  0x8005,
										  &certContext);
				if (status == 0) {
					status = certContext->methods->GetContextInfo(certContext,
										      2,
										      &cryptoContext);
					if (status == 0) {
						status = sslCreateAlgorithm(cryptoContext, 3,
									    algorithm, 0,
									    &digestAlg);
						if (status == 0) {
							status = sslDigestInit(digestAlg);
							if (status == 0)
								status = sslDigest(digestAlg,
										   encoded,
										   encodedLength,
										   digest,
										   &outLength);
							sslDestroyAlgorithm(digestAlg);
						}
					}
				}
			}
			if (encoded != NULL)
				sslFree(encoded);
		} else {
			status = 10018;
		}
	}
	return status;
}

int SceSslInternal_08ED1B25(void *certificate, int algorithm,
			    SceUInt32 capacity, void *digest,
			    SceUInt32 *digestLength)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL && digestLength != NULL)
		status = sslComputeCertificateDigest(certificate, 0x8003, algorithm,
						     capacity, digest, digestLength);
	return status;
}

int sslGetCertificateDate(SslCertificate *certificate, int selector, void *result)
{
	SslCertificatePropertyValue encoded;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate != NULL && result != NULL) {
		status = certificate->methods->GetCertificateInfo(certificate, selector,
								  &encoded);
		if (status == 0)
			status = certificate->methods->DecodeDate(certificate, &encoded,
								  result);
	}
	return status;
}

int SceSslInternal_2074A6EB(void *certificate, void *result)
{
	return sslGetCertificateDate(certificate, 6, result);
}

int SceSslInternal_2464F9D0(void *certificate, void *result)
{
	return sslGetCertificateDate(certificate, 5, result);
}

int sslSetObjectIdentifierByType(SslObjectIdentifier *object, int type)
{
	const char *oid;
	int status = 10018;

	oid = (const char *)sslLookupLibraryValue(sslCertificateAttributeOids,
						  type, 1, 0);
	if (oid != NULL)
		status = SceSslInternal_4D80D3F7(object, 1, oid,
						 sslStringLength(oid));
	return status;
}

int sslNewObjectIdentifierByType(void *unused, int type,
				 SslObjectIdentifier **result)
{
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (result != NULL) {
		status = sslNewObjectIdentifier(unused, result);
		if (status == 0)
			status = sslSetObjectIdentifierByType(*result, type);
	}
	return status;
}

int sslValidateCertificateVersionFields(SslCertificate *certificate)
{
	int version, extensionCount = 0;
	SslCertificatePropertyValue uniqueId;
	int status;

	status = certificate->methods->GetCertificateInfo(certificate, 1, &version);
	if (status == 0) {
		status = certificate->methods->GetCertificateInfo(certificate, 0x8000,
								  &extensionCount);
		if (status == 0) {
			if (extensionCount > 0 && version < 3)
				return 10022;
		} else if (status == 10008) {
			status = 0;
		}
	}
	if (status == 0 && version < 2) {
		status = certificate->methods->GetCertificateInfo(certificate, 11,
								  &uniqueId);
		if (status == 0) {
			if ((uniqueId.value | uniqueId.length |
			     (SceUInt32)(SceUIntPtr)uniqueId.data) != 0) {
				status = 10022;
			} else {
				status = certificate->methods->GetCertificateInfo(certificate,
										  10,
										  &uniqueId);
				if (status == 0 &&
				    (uniqueId.value | uniqueId.length |
				     (SceUInt32)(SceUIntPtr)uniqueId.data) != 0)
					status = 10022;
			}
		}
	}
	return status;
}
