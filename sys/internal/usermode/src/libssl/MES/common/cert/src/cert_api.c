/* Filename and descriptive interface names are ours. This component path is
 * recovered from MES/common/cert/src/ri_cert.c.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "../../../../CRYPTOCME/common/object.h"

/* Shared certificate-context interface prefix, not concrete object storage. */
typedef struct SslCertificateMethods {
	int (*CreateContext)(void *library, int flags, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*CreateCertificate)(void *context, int flags, void **result);
	int (*DestroyCertificate)(void *certificate);
} SslCertificateMethods;

typedef struct SslCertificateHeader {
	const SslCertificateMethods *methods;
} SslCertificateHeader;

_Static_assert(offsetof(SslCertificateMethods, DestroyCertificate) == 20,
	       "DestroyCertificate");

int SceSslInternal_144FA354(void *certificate)
{
	SslCertificateHeader *object = certificate;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->DestroyCertificate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->DestroyCertificate(certificate);
}
