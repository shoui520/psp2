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

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_session_st SSL_SESSION;

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

#define SSL_in_init(s) ((s)->state & SSL_ST_INIT)
#define SSL_in_before(s) ((s)->state & SSL_ST_BEFORE)

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
int ssl3_new(SSL *ssl);
int ssl3ResizeReadBuffer(SSL *ssl, int length);
int ssl3_setup_buffers(SSL *ssl);

/* Descriptive names for SSL-C's extra states are ours, not Sony spellings. */
#define SSL3_ST_CR_HELLO_REQ_A (0x101 | SSL_ST_CONNECT)
#define SSL3_ST_SR_CLNT_HELLO_RENEGOTIATE (0x113 | SSL_ST_ACCEPT)

_Static_assert(sizeof(SslRecordCrypto) == 28, "SslRecordCrypto");
_Static_assert(sizeof(SslExtensionList) == 8, "SslExtensionList");
_Static_assert(sizeof(SSL) == 352, "SSL");
_Static_assert(sizeof(SSL_METHOD) == 88, "SSL_METHOD");
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
