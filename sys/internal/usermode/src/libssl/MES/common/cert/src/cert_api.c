/* Filename and descriptive interface names are ours. This component path is
 * recovered from MES/common/cert/src/ri_cert.c.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "ri_cert.h"

int SceSslInternal_144FA354(void *certificate)
{
	SslCertificateHeader *object = certificate;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->DestroyCertificate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->DestroyCertificate(certificate);
}
