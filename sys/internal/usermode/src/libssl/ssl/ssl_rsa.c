/* Identified SSLeay certificate/private-key attachment operations with SSL-C adaptations. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../runtime.h"
#include "../err/err.h"
#include "../MES/common/cert/src/ri_cert.h"
#include "../CRYPTOCME/common/key/pkey_ctx.h"
#include <libssl/object.h>

#define SSL_F_SSL_CERT_NEW 163
#define SSL_F_SSL_CTX_USE_CERTIFICATE 171
#define SSL_F_SSL_CTX_USE_PRIVATEKEY 174
#define SSL_F_SSL_SET_CERT 191
#define SSL_F_SSL_SET_PKEY 194
#define SSL_F_SSL_USE_CERTIFICATE 199
#define SSL_F_SSL_USE_PRIVATEKEY 202
#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_PASSED_NULL_PARAMETER 35
#define ERR_R_X509_LIB 1306
#define SSL_R_UNKNOWN_CERTIFICATE_TYPE 247
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)
#define ERR_clear_error sslClearThreadErrors
#define X509_free SceSslInternal_144FA354
#define EVP_PKEY_free SceSslInternal_46358909
#define X509_check_private_key SceSslInternal_D68D8421
#define SSL_CTX_use_certificate SceSslInternal_D419451B
#define SSL_CTX_use_PrivateKey SceSslInternal_26CBEEB6

static __inline CERT *sslCertNewInline(void)
{
	CERT *ret;

	ret = (CERT *)sslAllocate(sizeof(CERT));
	if (ret == NULL) {
		SSLerr(SSL_F_SSL_CERT_NEW, ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	sslFillMemory(ret, 0, sizeof(CERT));

	ret->key = &(ret->pkeys[SSL_PKEY_RSA_ENC]);
	ret->references = 1;

	return ret;
}

static __inline int sslCertTypeInline(void *x, void *pkey)
{
	void *pk;
	int ret = -1, i;

	if (pkey == NULL) {
		if (SceSslInternal_170D595D(x, 0, &pk) != 0)
			goto err;
	} else {
		pk = pkey;
	}
	if (pk == NULL)
		goto err;

	if (sslGetPublicKeyInfo(pk, 2006, &i) == 0 && i == 6)
		ret = SSL_PKEY_RSA_ENC;

err:
	return ret;
}

#define ssl_cert_new sslCertNewInline
#define ssl_cert_type sslCertTypeInline

#include "ssl_rsa_sce.inc"
