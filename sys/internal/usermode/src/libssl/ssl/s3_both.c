/* Identified SSLeay handshake code, adapted in s3_both_sce.patch. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../err/err.h"
#include <libssl/object.h>

#undef Malloc
#undef Realloc
#define Malloc sslAllocate
#define Realloc sslReallocate
#define memset sslFillMemory
#define memcpy sslCopyMemory
#define memcmp sslCompareMemory
#define X509_free SceSslInternal_144FA354
#define X509_NAME_free SceSslInternal_CE37B69E

#define SSL_F_SSL3_GET_FINISHED 140
#define SSL_F_SSL3_GET_MESSAGE 142
#define SSL_F_SSL3_OUTPUT_CERT_CHAIN 148
#define SSL_F_SSL3_SETUP_BUFFERS 157

#define SSL_R_BAD_DIGEST_LENGTH 111
#define SSL_R_DIGEST_CHECK_FAILED 149
#define SSL_R_EXCESSIVE_MESSAGE_SIZE 152
#define SSL_R_GOT_A_FIN_BEFORE_A_CCS 154
#define SSL_R_UNEXPECTED_MESSAGE 244
#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_BUF_LIB 2055

#define SSL_AD_UNEXPECTED_MESSAGE SSL3_AD_UNEXPECTED_MESSAGE
#define SSL_AD_BAD_CERTIFICATE SSL3_AD_BAD_CERTIFICATE
#define SSL_AD_CERTIFICATE_REVOKED SSL3_AD_CERTIFICATE_REVOKED
#define SSL_AD_CERTIFICATE_EXPIRED SSL3_AD_CERTIFICATE_EXPIRED
#define SSL_AD_CERTIFICATE_UNKNOWN SSL3_AD_CERTIFICATE_UNKNOWN
#define SSL_AD_ILLEGAL_PARAMETER SSL3_AD_ILLEGAL_PARAMETER
#define SSL_AD_UNKNOWN_CA 48
#define SSL_AD_DECODE_ERROR 50
#define SSL_AD_DECRYPT_ERROR 51
#define SSL_AD_INTERNAL_ERROR 80
#define SSL_AD_HANDSHAKE_FAILURE SSL3_AD_HANDSHAKE_FAILURE

#define X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT 1
#define X509_V_ERR_CERT_SIGNATURE_FAILURE 2
#define X509_V_ERR_CERT_NOT_YET_VALID 3
#define X509_V_ERR_CERT_HAS_EXPIRED 4
#define X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT 5
#define X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN 6
#define X509_V_ERR_CERT_CHAIN_TOO_LONG 7
#define X509_V_ERR_CERT_REVOKED 8
#define X509_V_ERR_APPLICATION_VERIFICATION 10
#define X509_V_ERR_OUT_OF_MEM 11
#define X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE 12
#define X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE 13
#define X509_V_ERR_UNABLE_TO_GET_CRL 14
#define X509_V_ERR_CRL_SIGNATURE_FAILURE 15
#define X509_V_ERR_CRL_HAS_EXPIRED 16
#define X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY 100
#define X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD 101
#define X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD 102
#define X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD 103
#define X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD 104
#define X509_V_ERR_CRL_NOT_YET_VALID 105
#define X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY 106
#define X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE 107

#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

int ssl3_do_write(SSL *s, int type);
int ssl3_read_bytes(SSL *s, int type, char *buf, int len);
void ssl3_send_alert(SSL *s, int level, int desc);
void sslRetainCertificateObject(void *cert);
int sslSerializeCertificateObject(void *cert, int max_len, void *out, int *out_len);
int SceSslInternal_81FA5DBE(void *cert, int flags, void **issuer);
int SceSslInternal_08179EBD(void *cert, int flags, void **subject);
int SceSslInternal_568F0E80(void *a, void *b);
int sslCreateCertificateStoreCursor(void *store, void **cursor);
void sslDestroyCertificateStoreCursor(void *cursor);
int SceSslInternal_BF99D664(const void *methods, int unused, void **ext);
int SceSslInternal_64BDF1BB(void *cert, int nid, void *ext);
int SceSslInternal_93EAE08A(void *ext, int idx, void *out);
void SceSslInternal_CE37B69E(void *name);
int sslFindCertificateStoreEntryByKeyId(void *cursor, void *key_id, int mask);
int sslFindCertificateStoreEntryByName(void *cursor, void *name, int mask);
int sslGetCertificateStoreCursorCertificate(void *cursor, void **cert, int flags);
int SceSslInternal_170D595D(void *cert, int flags, void **pkey);
int sslGetPublicKeyInfo(void *key, int selector, void *result);

#include "s3_both_sce.inc"
