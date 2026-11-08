/* Identified SSLeay certificate-container operations with SSL-C adaptations. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../runtime.h"
#include "../err/err.h"
#include "../MES/common/module/crt_stor/src/small_stor.h"
#include "../MES/common/verify/r_verify.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"
#include <libssl/object.h>

#undef Malloc
#undef Free
#undef CRYPTO_add
#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#define SSL3_VERSION_MAJOR 0x03
#define SSL_F_SSL_CERT_NEW 163
#define SSL_F_SSL_VERIFY_CERT_CHAIN 208
#define ERR_R_MALLOC_FAILURE 33
#define CRYPTO_LOCK_SSL_CERT 13
#define CRYPTO_add(value, amount, index) \
	sslAddReference(value, amount, index, \
		"S:\\work\\MES\\sslc\\ssl/ssl_cert.c", 95)
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)
#define X509_free SceSslInternal_144FA354
#define EVP_PKEY_free SceSslInternal_46358909
#define SSL_get_client_CA_list SceSslInternal_F4007DBE

int SceSslInternal_08179EBD(void *cert, int flags, void **subject);
void SceSslInternal_CE37B69E(void *name);

#include "ssl_cert_sce.inc"
