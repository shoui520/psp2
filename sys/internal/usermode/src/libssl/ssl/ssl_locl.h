/* SSL-C connection layout, retaining identified SSLeay names.
 * SslRecordCrypto, SslExtensionList and the added descriptive snake-case fields
 * are ours, not recovered Sony spellings. The reserved word has no independent
 * access in the original; it is initialized with the whole connection.
 */
#ifndef LIBSSL_SSL_LOCL_H
#define LIBSSL_SSL_LOCL_H

#include <stddef.h>
#include <scetypes.h>
#include "../bio/bio.h"
#include "../buffer/buffer.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"

typedef struct SslRecordCrypto {
	int mac_size;
	void *mac_algorithm;
	void *cipher_algorithm;
	int block_size;
	void *cipher_key;
	int mac_secret_size;
	void *mac_key;
} SslRecordCrypto;

typedef struct SslExtensionList {
	STACK *entries;
	int pending_index;
} SslExtensionList;

#include "ssl_sce.h"
#include "ssl23_upstream.h"
#include "ssl_cert_layout_sce.h"

/* Descriptive callback typedef is ours; the upstream member name is retained. */
typedef void (*SslInfoCallback)(SSL *ssl, int where, int result);

#define SCE_SSL3_CONSTANTS_ONLY
#define SCE_SSL3_OBJECTS
/* Descriptive bound name is ours, measured from the native secret arrays. */
#define SCE_SSL3_MAX_MD_SIZE 64
#include "ssl3_sce.h"
#undef SCE_SSL3_CONSTANTS_ONLY
#undef SCE_SSL3_OBJECTS

/* SSL-C option name is ours; the native shutdown dispatcher tests this bit. */
#define SSL_OP_SHUTDOWN_IN_INIT 0x00040000UL

/* Retained upstream lock index, shared by the session translation units. */
#define CRYPTO_LOCK_SSL_SESSION 14

#define SSL_NOTHING 1
#define SSL_WRITING 2
#define SSL_READING 3
#define SSL_X509_LOOKUP 4
#define SSL_want_read(s) ((s)->rwstate == SSL_READING)
#define SSL_want_write(s) ((s)->rwstate == SSL_WRITING)
#define SSL_want_x509_lookup(s) ((s)->rwstate == SSL_X509_LOOKUP)

#define SSL_ERROR_NONE 0
#define SSL_ERROR_SSL 1
#define SSL_ERROR_WANT_READ 2
#define SSL_ERROR_WANT_WRITE 3
#define SSL_ERROR_WANT_X509_LOOKUP 4
#define SSL_ERROR_SYSCALL 5
#define SSL_ERROR_ZERO_RETURN 6
#define SSL_ERROR_WANT_CONNECT 7

/* Descriptive names for SSL-C's additional retry classifications are ours. */
#define SCE_SSL_ERROR_WANT_EXTENSION 8
#define SCE_SSL_ERROR_WANT_READ_SPECIAL 9
#define SCE_BIO_RR_READ_SPECIAL 3
/* Descriptive names for the native buffer setup constants are ours. */
#define SCE_SSL3_INITIAL_SEND_FRAGMENT 2048
#define SCE_SSL3_WRITE_OVERHEAD 101

int ssl3_pending(SSL *ssl);
int SSL_accept(SSL *ssl);
int SceSslInternal_92560B8D(SSL *ssl);
int SSL_clear(SSL *ssl);
int sslClearConnection(SSL *ssl, int keep_method); /* Descriptive name ours. */
int SSL_set_accept_state(SSL *ssl);
int SceSslInternal_CEE46B33(SSL *ssl);
int ssl_clear_bad_session(SSL *ssl);
SSL_METHOD *sslv3_base_method(void);
int ssl3_num_ciphers(void);
SSL_CIPHER *ssl3GetScsvCipher(void); /* Descriptive name is ours. */
int ssl3RemoveScsvCipher(STACK *sk); /* Descriptive name is ours. */
SSL_CIPHER *ssl3_get_cipher(unsigned int u);
int ssl3_new(SSL *ssl);
void ssl3CleanupHandshake(SSL *ssl); /* Descriptive name is ours. */
void ssl3_free(SSL *ssl);
void ssl3_clear(SSL *ssl);
long ssl3_ctrl(SSL *ssl, int cmd, long larg, char *parg);
long ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, char *parg);
SSL_CIPHER *ssl3_get_cipher_by_char(unsigned char *p);
SSL_CIPHER *ssl3_choose_cipher(SSL *ssl, STACK *have, STACK *pref);
int ssl3_get_req_cert_type(SSL *ssl, unsigned char *p);
int ssl3_shutdown(SSL *ssl);
int ssl3_renegotiate_check(SSL *ssl);
int ssl3_write(SSL *ssl, const void *buf, int len);
int ssl3_read(SSL *ssl, void *buf, int len);
int ssl3_peek(SSL *ssl, void *buf, int len);
int ssl3_renegotiate(SSL *ssl);
int ssl3ResizeReadBuffer(SSL *ssl, int length);
int ssl3_setup_buffers(SSL *ssl);
void ssl_clear_cipher_ctx(SSL *ssl);
SSL_SESSION *SSL_get_session(SSL *ssl);
SSL_SESSION *SSL_SESSION_new(void);
void SSL_SESSION_free(SSL_SESSION *session);
CERT *ssl_cert_new(void);
void ssl_cert_free(CERT *certificate);
long SSL_SESSION_get_timeout(SSL_SESSION *session);
long SSL_CTX_set_timeout(SSL_CTX *ctx, long timeout);
int SSL_set_session(SSL *ssl, SSL_SESSION *session);
int ssl_get_new_session(SSL *ssl, int session);
int ssl_get_prev_session(SSL *ssl, unsigned char *session_id, int len);
void ssl_update_cache(SSL *ssl, int mode);
/* Descriptive cipher/digest table entry types are ours; array names are upstream's. */
typedef struct SslCipherInfo {
	unsigned long algorithms;
	unsigned long algorithm2;
	int crypto_id;
	int key_len;
	int iv_len;
} SslCipherInfo;

typedef struct SslDigestInfo {
	unsigned long algorithms;
	int crypto_id;
	int md_size;
} SslDigestInfo;

extern SslCipherInfo ssl_cipher_methods[];
extern SslDigestInfo ssl_digest_methods[];
extern int ssl_weak_cipher_flags; /* Descriptive name is ours. */

/* Descriptive helper names are ours, not recovered Sony names. */
int sslSessionAddReference(SSL_SESSION *session);
struct SslTime *sslSessionCopyTime(SSL_SESSION *session, struct SslTime *result);
unsigned int sslSessionGetIdLength(SSL_SESSION *session);
int sslSessionMarkNotResumable(SSL_SESSION *session);
int sslSessionSetExtensions(SSL_SESSION *session, SslExtensionList *extensions);
int sslSessionGetExtensions(SSL_SESSION *session, SslExtensionList **result);
int sslSessionIsValid(SSL_SESSION *session);
int sslExtensionListCopy(SslExtensionList *source, SslExtensionList **result);
int ssl_cipher_get_evp(SSL_CTX *ctx, SSL_CIPHER *c, SslCryptoAlgorithm **enc,
		       SslCryptoAlgorithm **md);
int sslCipherQuerySupport(SSL_CTX *ctx, SSL_CIPHER *c, int *enc_supported,
			  int *mac_supported);
char *SSL_CIPHER_description(SSL_CIPHER *cipher, char *buf, int len);
unsigned long SSL_CIPHER_get_id(SSL_CIPHER *c);
char *SSL_CIPHER_get_version(SSL_CIPHER *c);
char *SceSslInternal_2B90012E(SSL_CIPHER *c);
int sslCipherIsValid(SSL_CIPHER *c);
int SSL_CIPHER_get_bits(SSL_CIPHER *c, int *alg_bits);
int sslCipherGetBits(SSL *ssl, SSL_CIPHER *c, int *alg_bits);
int sslGetCertificateType(int sig_alg_id, void *pkey);
int sslCheckRsaKeyLimits(SSL *ssl, void *pkey);
int ssl3CopyDigestAlgorithm(SslCryptoAlgorithm **dst, SslCryptoAlgorithm *src);
int sslCipherGetSizes(SSL_CIPHER *c, int *key_len, int *iv_len, int *mac_len);
int ssl3GetCipherAndDigest(SSL_CTX *ctx, SSL_CIPHER *c, SslCryptoAlgorithm **enc,
			   SslCryptoAlgorithm **md);
int ssl3_change_cipher_state(SSL *s, int which);
int ssl3_setup_key_block(SSL *s);
void ssl3_cleanup_key_block(SSL *s);
int ssl3_enc(SSL *s, int send_data);
int ssl3_init_finished_mac(SSL *s);
int ssl3_finish_mac(SSL *s, unsigned char *buf, int len);
int ssl3_cert_verify_mac(SSL *s, SslCryptoAlgorithm *ctx, unsigned char *p);
int ssl3_final_finish_mac(SSL *s, SslCryptoAlgorithm *ctx1,
			  SslCryptoAlgorithm *ctx2, unsigned char *sender,
			  int len, unsigned char *p);
int ssl3_mac(SSL *ssl, unsigned char *md, int send_data);
int ssl3_generate_master_secret(SSL *s, unsigned char *out,
				unsigned char *p, int len);
int ssl3_alert_code(int code);
int SSL_use_certificate(SSL *ssl, void *x);
int SSL_use_PrivateKey(SSL *ssl, void *pkey);
int SceSslInternal_D419451B(SSL_CTX *ctx, void *x);
int SceSslInternal_26CBEEB6(SSL_CTX *ctx, void *pkey);

/* Descriptive names for SSL-C's extra states are ours, not Sony spellings. */
#define SSL3_ST_CR_HELLO_REQ_A (0x101 | SSL_ST_CONNECT)
#define SSL3_ST_SR_CLNT_HELLO_RENEGOTIATE (0x113 | SSL_ST_ACCEPT)

_Static_assert(sizeof(SslRecordCrypto) == 28, "SslRecordCrypto");
_Static_assert(sizeof(SslExtensionList) == 8, "SslExtensionList");
_Static_assert(sizeof(SSL) == 352, "SSL");
_Static_assert(sizeof(SSL_CTX) == 220, "SSL_CTX");
_Static_assert(sizeof(SSL_CIPHER) == 24, "SSL_CIPHER");
_Static_assert(sizeof(SSL3_ENC_METHOD) == 84, "SSL3_ENC_METHOD");
_Static_assert(sizeof(SSL_METHOD) == 88, "SSL_METHOD");
_Static_assert(sizeof(SSL_SESSION) == 196, "SSL_SESSION");
_Static_assert(sizeof(CERT) == 104, "CERT");
_Static_assert(sizeof(CERT_PKEY) == 12, "CERT_PKEY");
_Static_assert(offsetof(CERT, pkeys) == 36, "pkeys");
_Static_assert(offsetof(CERT, rsa_tmp_cb) == 32, "rsa_tmp_cb");
_Static_assert(offsetof(CERT, references) == 100, "references");
_Static_assert(offsetof(SSL_SESSION, master_key_length) == 16, "master_key_length");
_Static_assert(offsetof(SSL_SESSION, master_key) == 20, "master_key");
_Static_assert(offsetof(SSL_SESSION, session_id_length) == 68, "session_id_length");
_Static_assert(offsetof(SSL_SESSION, sid_ctx_length) == 104, "sid_ctx_length");
_Static_assert(offsetof(SSL_SESSION, not_resumable) == 140, "not_resumable");
_Static_assert(offsetof(SSL_SESSION, references) == 152, "references");
_Static_assert(offsetof(SSL_SESSION, time) == 160, "time");
_Static_assert(offsetof(SSL_SESSION, cipher) == 164, "cipher");
_Static_assert(offsetof(SSL_SESSION, extensions) == 184, "extensions");
_Static_assert(offsetof(SSL_SESSION, verify_result) == 188, "verify_result");
_Static_assert(offsetof(SSL_SESSION, verification_state) == 192,
	       "verification_state");
_Static_assert(sizeof(SSL3_CTX) == 580, "SSL3_CTX");
_Static_assert(sizeof(SSL3_BUFFER) == 24, "SSL3_BUFFER");
_Static_assert(sizeof(SSL3_RECORD) == 24, "SSL3_RECORD");
_Static_assert(offsetof(SSL3_CTX, rbuf) == 228, "rbuf");
_Static_assert(offsetof(SSL3_CTX, rrec) == 276, "rrec");
_Static_assert(offsetof(SSL3_CTX, finish_dgst1) == 344, "finish_dgst1");
_Static_assert(offsetof(SSL3_CTX, warn_alert) == 356, "warn_alert");
_Static_assert(offsetof(SSL3_CTX, tmp) == 388, "tmp");
_Static_assert(offsetof(SSL3_CTX, tmp.ca_names) == 552, "ca_names");
_Static_assert(offsetof(SSL3_CTX, tmp.cert_request) == 576, "cert_request");
_Static_assert(offsetof(SSL_METHOD, ssl_read) == 24, "ssl_read");
_Static_assert(offsetof(SSL_METHOD, ssl_shutdown) == 36, "ssl_shutdown");
_Static_assert(offsetof(SSL_METHOD, ssl_pending) == 64, "ssl_pending");
_Static_assert(offsetof(SSL, state) == 48, "state");
_Static_assert(offsetof(SSL, rstate) == 56, "rstate");
_Static_assert(offsetof(SSL, read_crypto) == 104, "read_crypto");
_Static_assert(offsetof(SSL, write_crypto) == 132, "write_crypto");
_Static_assert(offsetof(SSL, session) == 208, "session");
_Static_assert(offsetof(SSL, ctx) == 256, "ctx");
_Static_assert(offsetof(SSL, mode) == 348, "mode");

#endif
