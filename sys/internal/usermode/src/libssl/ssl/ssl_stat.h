/* Identified SSLeay interfaces and SSL-C's additional TLS alert. */
#ifndef LIBSSL_SSL_STAT_H
#define LIBSSL_SSL_STAT_H

#include "ssl_locl.h"

#define TLS1_AD_NO_RENEGOTIATION 100

char *SSL_alert_type_string_long(int value);
char *SSL_alert_type_string(int value);
char *SSL_alert_desc_string(int value);
char *SSL_alert_desc_string_long(int value);
char *SSL_state_string_long(SSL *ssl);
char *SSL_state_string(SSL *ssl);
char *SSL_rstate_string_long(SSL *ssl);
char *SSL_rstate_string(SSL *ssl);

#endif
