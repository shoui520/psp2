/* Identified SSLeay connection operations, adapted in ssl_lib_sce.patch. */
#include "ssl_locl.h"
#include "ssl_context.h"
#include "../err/err.h"
#include "../CRYPTOCME/common/key/key_api.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"

#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define SSL_SENT_SHUTDOWN 1
#define SSL_RECEIVED_SHUTDOWN 2
#define SSL_F_SSL_WRITE 209
#define SSL_R_PROTOCOL_IS_SHUTDOWN 207
#define SSL_F_SSL_DO_HANDSHAKE 180
#define SSL_R_CONNECTION_TYPE_NOT_SET 144
/* Retained upstream error identifiers; SSL-C's clear function index is165. */
#define SSL_F_SSL_CLEAR 165
#define SSL_R_NO_METHOD_SPECIFIED 188
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)
#define ERR_peek_error sslPeekError

/* Retain SSL-C's out-of-line BIO queries at these call sites. */
#undef BIO_should_read
#undef BIO_should_write
#undef BIO_should_io_special

/* Sony's export link names; the API names are upstream's. */
#define SSL_version SceSslInternal_3EF42454
#define SSL_get_shutdown SceSslInternal_B1745DAD
#define SSL_state SceSslInternal_6F3B9809
#define SSL_pending SceSslInternal_D776E2F8
#define SSL_read SceSslInternal_4F781BB6
#define SSL_write sceSslInternalWrite
#define SSL_shutdown SceSslInternal_EC9FAF42
#define SSL_do_handshake SceSslInternal_E5EDD91B
#define SSL_get_error SceSslInternal_2F0FC3CD
#define SSL_connect SceSslInternal_92560B8D
#define SSL_set_connect_state SceSslInternal_CEE46B33

#include "ssl_lib_sce.inc"

/* SSL-C's out-of-line form of the upstream rwstate query. */
int SceSslInternal_D6D32A2F(SSL *ssl)
{
	return ssl->rwstate;
}
