/* Filename and descriptive interface names are ours. This component path is
 * recovered from MES/common/cert/src/ri_cert.c.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "ri_cert.h"

int sslGetCertificateMethods(SslCertificate *certificate, void *result)
{
	SslCertificateContext *context;
	int status;

	status = certificate->methods->GetCertificateInfo(certificate, 0x8005, &context);
	if (status == 0)
		status = context->methods->GetContextInfo(context, 0, result);
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
