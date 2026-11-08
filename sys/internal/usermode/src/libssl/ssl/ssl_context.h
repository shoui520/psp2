/* SSL-C context interface prefix used by connection reset and session removal.
 * SslContextHeader, extended_options and the cache callback names are ours.
 * The complete context allocation is larger; no allocation uses this prefix.
 */
#ifndef LIBSSL_SSL_CONTEXT_H
#define LIBSSL_SSL_CONTEXT_H

typedef struct SslContextHeader {
	SSL_METHOD *method;
	unsigned long options;
	unsigned long extended_options;
	STACK *cipher_list;
	STACK *cipher_list_by_id;
	void *cert_store;
	int (*session_cache_callback)(SSL_CTX *ctx, int operation, void *data,
				      void *arg);
	void *session_cache_arg;
	int session_cache_mode;
} SslContextHeader;

/* Upstream cache-mode bits, retained by SSL-C. */
#define SSL_SESS_CACHE_CLIENT 0x0001
#define SSL_SESS_CACHE_SERVER 0x0002
#define SSL_SESS_CACHE_BOTH (SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_SERVER)

/* Descriptive names ours: native callback operations for session cache. */
#define SCE_SSL_SESSION_CACHE_ADD 1
#define SCE_SSL_SESSION_CACHE_REMOVE 2
#define SCE_SSL_SESSION_CACHE_GET 3

_Static_assert(offsetof(SslContextHeader, session_cache_callback) == 24,
	       "session_cache_callback");
_Static_assert(offsetof(SslContextHeader, session_cache_arg) == 28,
	       "session_cache_arg");
_Static_assert(offsetof(SslContextHeader, session_cache_mode) == 32,
	       "session_cache_mode");

#endif
