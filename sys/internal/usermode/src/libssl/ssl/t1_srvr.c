/* Identified SSLeay TLSv1 server method, adapted in t1_srvr_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"

#define memcpy sslCopyMemory

#include "t1_srvr_sce.inc"
