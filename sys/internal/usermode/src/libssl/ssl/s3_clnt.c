/* Identified SSLeay SSLv3 client handshake, adapted in s3_clnt_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"
#include "../runtime.h"
#include "../err/err.h"
#include "../time.h"
#include "../MES/common/cert/src/ri_cert.h"
#include "../CRYPTOCME/common/key/pkey_ctx.h"
#include <libssl/name.h>
#include <libssl/object.h>
#include <net/errno.h>

#undef Malloc
#undef Free
#undef CRYPTO_add
#undef CRYPTO_w_lock
#undef CRYPTO_w_unlock
#define memcpy sslCopyMemory
#define memset sslFillMemory
#define memcmp sslCompareMemory
#define ERR_clear_error sslClearThreadErrors
#define clear_sys_error() (*sceNetErrnoLoc() = 0)
#define BIO_flush(b) (int)BIO_ctrl(b, BIO_CTRL_FLUSH, 0, NULL)
#undef BIO_should_write
#define ssl_put_cipher_by_char(ssl, ciph, ptr) \
	((ssl)->method->put_cipher_by_char((ciph), (ptr)))
#define sk_new_null() sk_new(NULL)
#define X509_free SceSslInternal_144FA354
#define EVP_PKEY_free SceSslInternal_46358909
#define X509_NAME_cmp sslCompareCertificateNames
#define SSL_connect SceSslInternal_92560B8D
#define SSL_get_value SceSslInternal_24C6257F

#define MD5_DIGEST_LENGTH 16
#define SHA_DIGEST_LENGTH 20

#define SSL_VERIFY_NONE 0x00

#define EVP_PK_RSA 0x0001
#define EVP_PKT_SIGN 0x0010
#define EVP_PKT_ENC 0x0020
#define EVP_PKT_EXP 0x1000

#define SSL_CB_HANDSHAKE_START 0x10
#define SSL_CB_HANDSHAKE_DONE 0x20

#define SSL_SESS_CACHE_CLIENT 0x0001
#define SSL_SESS_CACHE_SERVER 0x0002

#define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER 0x00800000L
#define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG 0x00000008L
#define SSL_OP_TLS_D5_BUG 0x00001000L
#define SSL_OP_TLS_BLOCK_PADDING_BUG 0x00400000L
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x01000000L
#define SSL_OP_NO_SSLv3 0x02000000L
#define SSL_OP_NO_TLSv1 0x04000000L
#define SSL_OP_PKCS1_CHECK_1 0x08000000L
#define SSL_OP_PKCS1_CHECK_2 0x10000000L
#define SSL_OP_NETSCAPE_CA_DN_BUG 0x20000000L
#define SSL_OP_NO_TLSv1_1 0x40000000L
#define SSL_OP_LEGACY_SERVER_CONNECT 0x00000004L

#define SSL3_FLAGS_EXPECT_SERVER_FINISHED 0x0020L

#define SSL_AD_UNEXPECTED_MESSAGE SSL3_AD_UNEXPECTED_MESSAGE
#define SSL_AD_HANDSHAKE_FAILURE SSL3_AD_HANDSHAKE_FAILURE
#define SSL_AD_NO_CERTIFICATE SSL3_AD_NO_CERTIFICATE
#define SSL_AD_BAD_CERTIFICATE SSL3_AD_BAD_CERTIFICATE
#define SSL_AD_CERTIFICATE_UNKNOWN SSL3_AD_CERTIFICATE_UNKNOWN
#define SSL_AD_ILLEGAL_PARAMETER SSL3_AD_ILLEGAL_PARAMETER
#define SSL_AD_DECODE_ERROR 50
#define SSL_AD_DECRYPT_ERROR 51
#define SSL_AD_PROTOCOL_VERSION 70
#define SSL_AD_INTERNAL_ERROR 80
#define SSL_AD_NO_RENEGOTIATION 100

#define SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM 130
#define SSL_F_SSL3_CLIENT_HELLO 131
#define SSL_F_SSL3_CONNECT 132
#define SSL_F_SSL3_GET_CERTIFICATE_REQUEST 135
#define SSL_F_SSL3_GET_CLIENT_HELLO 138
#define SSL_F_SSL3_GET_KEY_EXCHANGE 141
#define SSL_F_SSL3_GET_SERVER_CERTIFICATE 144
#define SSL_F_SSL3_GET_SERVER_DONE 145
#define SSL_F_SSL3_GET_SERVER_HELLO 146
#define SSL_F_SSL3_SEND_CLIENT_CERTIFICATE 152
#define SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE 153
#define SSL_F_SSL3_SEND_CLIENT_VERIFY 154
#define SSL_F_SSL3_KEY_INFO 144
#define SSL_F_SSL3_CERT_TYPE 191

#define ERR_R_BN_LIB 2051
#define ERR_R_RSA_LIB 2052
#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_BUF_LIB 2055
#define ERR_R_ASN1_LIB 2061

#define SSL_R_BAD_DATA_RETURNED_BY_CALLBACK 106
#define SSL_R_BAD_MESSAGE_TYPE 114
#define SSL_R_BAD_PACKET_LENGTH 115
#define SSL_R_BAD_RSA_DECRYPT 118
#define SSL_R_BAD_RSA_ENCRYPT 119
#define SSL_R_BAD_RSA_E_LENGTH 120
#define SSL_R_BAD_RSA_MODULUS_LENGTH 121
#define SSL_R_BAD_SIGNATURE 123
#define SSL_R_CA_DN_LENGTH_MISMATCH 131
#define SSL_R_CA_DN_TOO_LONG 132
#define SSL_R_CERTIFICATE_VERIFY_FAILED 134
#define SSL_R_CERT_LENGTH_MISMATCH 135
#define SSL_R_EXTRA_DATA_IN_MESSAGE 153
#define SSL_R_INTERNAL_ERROR 157
#define SSL_R_LENGTH_MISMATCH 159
#define SSL_R_MISSING_EXPORT_TMP_RSA_KEY 167
#define SSL_R_MISSING_RSA_ENCRYPTING_CERT 169
#define SSL_R_MISSING_RSA_SIGNING_CERT 170
#define SSL_R_NO_CIPHERS_AVAILABLE 181
#define SSL_R_NO_PROTOCOLS_AVAILABLE 191
#define SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED 197
#define SSL_R_SSL3_SESSION_ID_TOO_LONG 1105
#define SSL_R_TLS_CLIENT_CERT_REQ_WITH_ANON_CIPHER 232
#define SSL_R_TRIED_TO_USE_UNSUPPORTED_CIPHER 235
#define SSL_R_UNABLE_TO_FIND_PUBLIC_KEY_PARAMETERS 239
#define SSL_R_UNKNOWN_CERTIFICATE_TYPE 247
#define SSL_R_UNKNOWN_CIPHER_RETURNED 248
#define SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE 250
#define SSL_R_UNKNOWN_STATE 255
#define SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM 257
#define SSL_R_WRONG_CIPHER_RETURNED 261
#define SSL_R_WRONG_MESSAGE_TYPE 262
#define SSL_R_WRONG_SIGNATURE_LENGTH 264
#define SSL_R_WRONG_SSL_VERSION 266
#define SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED 1100
#define SSL_R_RANDOM_GENERATION_FAILED 1116
#define SSL_R_EXTENSION_FAILURE 1121
#define SSL_R_MANDATORY_EXTENSION_MISSING 1122
#define SSL_R_DECODE_CERTIFICATE_ERROR 1302
#define SSL_R_GET_PUBLIC_KEY_ERROR_1303 1303
#define SSL_R_PUBLIC_KEY_SIZE_ERROR 1304
#define SSL_R_PUBLIC_KEY_TYPE_ERROR 1305
#define SSL_R_GET_PUBLIC_KEY_ERROR 1306
#define SSL_R_GET_SIGNATURE_ALG_ERROR 1307

#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

extern SSL_METHOD TLSv1_client_data;
extern SSL_METHOD SSLv3_client_data;

SSL_METHOD *SceSslInternal_AC1DBE40(void);
SSL_METHOD *ssl3_get_client_method(int ver);
int SceSslInternal_24C6257F(SSL *ssl, int type, void *val);
int SceSslInternal_64BDF1BB(void *cert, int type, void *out);
int ssl_cert_type(void *x, void *pkey);
long ssl3_get_message(SSL *s, int st1, int stn, int mt, long max, int *ok);
int ssl3_get_finished(SSL *s, int a, int b);
int ssl3_send_change_cipher_spec(SSL *s, int a, int b);
int ssl3_send_finished(SSL *s, int a, int b, const unsigned char *sender, int slen);
unsigned long ssl3_output_cert_chain(SSL *s, void *x);
int ssl_verify_alarm_type(long type);
int ssl3_write_bytes(SSL *s, int type, char *buf, int len);
int ssl3_dispatch_alert(SSL *s);

#include "s3_clnt_sce.inc"
