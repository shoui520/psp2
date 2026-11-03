/* SSLeay diagnostics with measured SSL-C changes in ssl_stat_sce.patch. */
#include "ssl_locl.h"
#include "ssl_stat.h"

#define NO_SSL2
#include "ssl_stat_sce.inc"
