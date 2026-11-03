/* Identified SSLeay certificate-container operations with SSL-C adaptations. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../runtime.h"
#include "../err/err.h"
#include <libssl/object.h>

#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#define SSL_F_SSL_CERT_NEW 163
#define ERR_R_MALLOC_FAILURE 33
#define CRYPTO_LOCK_SSL_CERT 13
#define CRYPTO_add(value, amount, index) \
	sslAddReference(value, amount, index, \
		"S:\\work\\MES\\sslc\\ssl/ssl_cert.c", 95)
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)
#define X509_free SceSslInternal_144FA354
#define EVP_PKEY_free SceSslInternal_46358909

#include "ssl_cert_sce.inc"
