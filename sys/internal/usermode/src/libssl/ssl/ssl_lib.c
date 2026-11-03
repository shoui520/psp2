/* Identified SSLeay connection operations, adapted in ssl_lib_sce.patch. */
#include "ssl_locl.h"
#include "../err/err.h"

#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define SSL_NOTHING 1
#define SSL_SENT_SHUTDOWN 1
#define SSL_RECEIVED_SHUTDOWN 2
#define SSL_F_SSL_WRITE 209
#define SSL_R_PROTOCOL_IS_SHUTDOWN 207
#define SSL_F_SSL_DO_HANDSHAKE 180
#define SSL_R_CONNECTION_TYPE_NOT_SET 144
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

/* Sony's export link names; the API names are upstream's. */
#define SSL_version SceSslInternal_3EF42454
#define SSL_get_shutdown SceSslInternal_B1745DAD
#define SSL_state SceSslInternal_6F3B9809
#define SSL_pending SceSslInternal_D776E2F8
#define SSL_read SceSslInternal_4F781BB6
#define SSL_write sceSslInternalWrite
#define SSL_shutdown SceSslInternal_EC9FAF42
#define SSL_do_handshake SceSslInternal_E5EDD91B

#include "ssl_lib_sce.inc"

/* SSL-C's out-of-line form of the upstream rwstate query. */
int SceSslInternal_D6D32A2F(SSL *ssl)
{
	return ssl->rwstate;
}
