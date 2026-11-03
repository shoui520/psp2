/* Identified SSLeay connection operations, adapted in ssl_lib_sce.patch. */
#include "ssl_locl.h"

#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302

/* Sony's export link names; the API names are upstream's. */
#define SSL_version SceSslInternal_3EF42454
#define SSL_get_shutdown SceSslInternal_B1745DAD
#define SSL_state SceSslInternal_6F3B9809

#include "ssl_lib_sce.inc"

/* SSL-C's out-of-line form of the upstream rwstate query. */
int SceSslInternal_D6D32A2F(SSL *ssl)
{
	return ssl->rwstate;
}
