/* Identified SSLeay interfaces and SSL-C's additional TLS alert. */
#ifndef LIBSSL_SSL_STAT_H
#define LIBSSL_SSL_STAT_H

#define SCE_SSL3_ALERTS_ONLY
#include "ssl3_sce.h"
#undef SCE_SSL3_ALERTS_ONLY

#define TLS1_AD_NO_RENEGOTIATION 100

char *SSL_alert_type_string_long(int value);
char *SSL_alert_type_string(int value);
char *SSL_alert_desc_string(int value);
char *SSL_alert_desc_string_long(int value);

#endif
