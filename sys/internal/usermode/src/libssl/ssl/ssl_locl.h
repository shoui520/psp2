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

typedef struct ssl_method_st SSL_METHOD;
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
#include "ssl3_sce.h"
#undef SCE_SSL3_CONSTANTS_ONLY

/* Descriptive names for SSL-C's extra states are ours, not Sony spellings. */
#define SSL3_ST_CR_HELLO_REQ_A (0x101 | SSL_ST_CONNECT)
#define SSL3_ST_SR_CLNT_HELLO_RENEGOTIATE (0x113 | SSL_ST_ACCEPT)

_Static_assert(sizeof(SslRecordCrypto) == 28, "SslRecordCrypto");
_Static_assert(sizeof(SslExtensionList) == 8, "SslExtensionList");
_Static_assert(sizeof(SSL) == 352, "SSL");
_Static_assert(offsetof(SSL, state) == 48, "state");
_Static_assert(offsetof(SSL, rstate) == 56, "rstate");
_Static_assert(offsetof(SSL, read_crypto) == 104, "read_crypto");
_Static_assert(offsetof(SSL, write_crypto) == 132, "write_crypto");
_Static_assert(offsetof(SSL, session) == 208, "session");
_Static_assert(offsetof(SSL, ctx) == 256, "ctx");
_Static_assert(offsetof(SSL, mode) == 348, "mode");

#endif
