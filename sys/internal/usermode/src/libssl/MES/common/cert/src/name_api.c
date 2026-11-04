/* Filename is ours; common/cert/src follows recovered MES source paths. */
#include <stddef.h>
#include "../../../../CRYPTOCME/common/object.h"
#include <libssl/name.h>
#include "ri_cert.h"

int SceSslInternal_CE37B69E(void *name)
{
	SslObjectHeader *object = name;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->Destroy(name);
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
