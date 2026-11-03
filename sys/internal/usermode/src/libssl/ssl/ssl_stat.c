/* SSLeay alert diagnostics with measured SSL-C changes in ssl_stat_sce.patch. */
#include "ssl_stat.h"

#define SCE_SSL_STAT_ALERTS_ONLY
#include "ssl_stat_sce.inc"
