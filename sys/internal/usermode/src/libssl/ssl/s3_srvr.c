/* Identified SSLeay SSLv3 server handshake, adapted in s3_srvr_sce.patch. */
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
#define sk_new_null() sk_new(NULL)
#define X509_free SceSslInternal_144FA354
#define EVP_PKEY_free SceSslInternal_46358909
#define SSL_get_client_CA_list SceSslInternal_F4007DBE
#define SSL_get_value SceSslInternal_24C6257F

#define CRYPTO_LOCK_RSA 12
#define CRYPTO_LOCK_SSL_CERT 13
#define CRYPTO_LOCK_READ 1
#define CRYPTO_LOCK_WRITE 2
#define CRYPTO_LOCK 4
#define CRYPTO_UNLOCK 8

#define MD5_DIGEST_LENGTH 16
#define SHA_DIGEST_LENGTH 20

#define SSL_VERIFY_NONE 0x00
#define SSL_VERIFY_PEER 0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02
#define SSL_VERIFY_CLIENT_ONCE 0x04

#define SSL_CB_HANDSHAKE_START 0x10
#define SSL_CB_HANDSHAKE_DONE 0x20

#define SSL_SESS_CACHE_CLIENT 0x0001
#define SSL_SESS_CACHE_SERVER 0x0002
#define SSL_SESS_CACHE_NO_INTERNAL_STORE 0x0200

#define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG 0x00000008L
#define SSL_OP_TLS_D5_BUG 0x00000100L
#define SSL_OP_TLS_ROLLBACK_BUG 0x00000400L
#define SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS 0x00000800L
#define SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION 0x00008000L
#define SSL_OP_EPHEMERAL_RSA 0x00200000L
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x01000000L
#define SSL_OP_NO_SSLv3 0x02000000L
#define SSL_OP_NO_TLSv1 0x04000000L
#define SSL_OP_NETSCAPE_CA_DN_BUG 0x20000000L
#define SSL_OP_NO_TLSv1_1 0x40000000L
#define SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG 0x80000000UL

#define SSL3_FLAGS_EXPECT_SERVER_FINISHED 0x0020L
#define SSL3_MT_CERTIFICATE_STATUS 21
#define SSL3_ST_SR_CERT_STATUS_A (0x182 | SSL_ST_ACCEPT)
#define SSL3_ST_SR_CERT_STATUS_B (0x183 | SSL_ST_ACCEPT)

#define SSL_AD_UNEXPECTED_MESSAGE SSL3_AD_UNEXPECTED_MESSAGE
#define SSL_AD_HANDSHAKE_FAILURE SSL3_AD_HANDSHAKE_FAILURE
#define SSL_AD_UNSUPPORTED_CERTIFICATE SSL3_AD_UNSUPPORTED_CERTIFICATE
#define SSL_AD_ILLEGAL_PARAMETER SSL3_AD_ILLEGAL_PARAMETER
#define SSL_AD_DECODE_ERROR 50
#define SSL_AD_DECRYPT_ERROR 51
#define SSL_AD_INTERNAL_ERROR 80
#define SSL_AD_NO_RENEGOTIATION 100

#define SSL_F_SSL3_ACCEPT 128
#define SSL_F_SSL3_SEND_SERVER_HELLO 131
#define SSL_F_SSL3_GET_CERT_VERIFY 136
#define SSL_F_SSL3_GET_CLIENT_CERTIFICATE 137
#define SSL_F_SSL3_GET_CLIENT_HELLO 138
#define SSL_F_SSL3_GET_CLIENT_KEY_EXCHANGE 139
#define SSL_F_SSL3_KEY_INFO 144
#define SSL_F_SSL3_EXTENSION_CHECK 146
#define SSL_F_SSL3_SEND_CERTIFICATE_REQUEST 151
#define SSL_F_SSL3_SEND_SERVER_CERTIFICATE 155
#define SSL_F_SSL3_SEND_SERVER_KEY_EXCHANGE 156
#define SSL_F_SSL3_SRVR_RANDOM 214

#define ERR_R_RSA_LIB 4
#define ERR_R_MALLOC_FAILURE 33
#define ERR_R_BUF_LIB 2055
#define ERR_R_ASN1_LIB 2061
#define SSL_R_BAD_EXTENSION 115
#define SSL_R_BAD_RSA_DECRYPT 118
#define SSL_R_BAD_RSA_SIGNATURE 122
#define SSL_R_CCS_RECEIVED_EARLY 133
#define SSL_R_CERT_LENGTH_MISMATCH 135
#define SSL_R_INTERNAL_ERROR 157
#define SSL_R_LENGTH_MISMATCH 159
#define SSL_R_MISSING_RSA_CERTIFICATE 168
#define SSL_R_MISSING_TMP_RSA_KEY 172
#define SSL_R_MISSING_TMP_RSA_PKEY 173
#define SSL_R_MISSING_VERIFY_MESSAGE 174
#define SSL_R_NO_CERTIFICATES_RETURNED 176
#define SSL_R_NO_CERTIFICATE_RETURNED 178
#define SSL_R_NO_CIPHERS_PASSED 182
#define SSL_R_NO_CIPHERS_SPECIFIED 183
#define SSL_R_NO_CLIENT_CERT_RECEIVED 186
#define SSL_R_NO_COMPRESSION_SPECIFIED 187
#define SSL_R_NO_PROTOCOLS_AVAILABLE 191
#define SSL_R_NO_SHARED_CIPHER 193
#define SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE 199
#define SSL_R_REQUIRED_CIPHER_MISSING 215
#define SSL_R_SIGNATURE_FOR_NON_SIGNING_CERTIFICATE 220
#define SSL_R_TLS_PEER_DID_NOT_RESPOND_WITH_CERTIFICATE_LIST 233
#define SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG 234
#define SSL_R_UNKNOWN_CIPHER_TYPE 249
#define SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE 250
#define SSL_R_UNKNOWN_PKEY_TYPE 251
#define SSL_R_UNKNOWN_STATE 255
#define SSL_R_WRONG_MESSAGE_TYPE 262
#define SSL_R_WRONG_SIGNATURE_SIZE 265
#define SSL_R_WEAK_PEER_KEY 270
#define SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED 1100
#define SSL_R_RANDOM_GENERATION_FAILED 1116
#define SSL_R_EXTENSION_FAILURE 1121
#define SSL_R_MANDATORY_EXTENSION_MISSING 1122
#define SSL_R_PUBLIC_KEY_SIZE_ERROR 1304
#define SSL_R_PUBLIC_KEY_TYPE_ERROR 1305
#define SSL_R_GET_PUBLIC_KEY_ERROR 1306
#define SSL_R_GET_SIGNATURE_ALG_ERROR 1307
#define SSL_R_RETAIN_PUBLIC_KEY_ERROR 1308
#define SSL_R_GET_RSA_MODULUS_ERROR 1309
#define SSL_R_GET_RSA_EXPONENT_ERROR 1310
#define SSL_R_CREATE_PUBLIC_KEY_ERROR 1311
#define SSL_R_COPY_PUBLIC_KEY_ERROR 1312

#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

extern SSL_METHOD TLSv1_server_data;
extern SSL_METHOD SSLv3_server_data;

SSL_METHOD *SSLv3_server_method(void);
int ssl3_send_server_certificate(SSL *s);
int SceSslInternal_24C6257F(SSL *ssl, int type, void *val);
long ssl3_get_message(SSL *s, int st1, int stn, int mt, long max, int *ok);
int ssl3_get_finished(SSL *s, int a, int b);
int ssl3_send_change_cipher_spec(SSL *s, int a, int b);
int ssl3_send_finished(SSL *s, int a, int b, const unsigned char *sender, int slen);
unsigned long ssl3_output_cert_chain(SSL *s, void *x);
int ssl_verify_alarm_type(long type);
int ssl3_write_bytes(SSL *s, int type, char *buf, int len);
int ssl3_dispatch_alert(SSL *s);

#include "s3_srvr_sce.inc"
