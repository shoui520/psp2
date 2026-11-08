/* Identified SSLeay TLSv1/TLSv1.1 operations, adapted in t1_lib_sce.patch. */
#include "ssl_locl.h"
#include "tls1_sce.h"
#include "../memory.h"

#define memcpy sslCopyMemory
#define TLSv1_1_client_method SceSslInternal_A374CD7B

#include "t1_lib_sce.inc"
