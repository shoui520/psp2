/* Identified SSLeay TLSv1 client method, adapted in t1_clnt_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"

#define memcpy sslCopyMemory
#define TLSv1_client_method SceSslInternal_133ED36D

#include "t1_clnt_sce.inc"
