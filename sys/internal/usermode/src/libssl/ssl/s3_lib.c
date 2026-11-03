/* Identified SSLeay SSL3 operations, adapted in s3_lib_sce.patch. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../err/err.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"
#include <libssl/name.h>

#define Malloc sslAllocate
#define memset sslFillMemory
#define Free sslFree
#define SSL_F_SSL3_NEW 147
#define ERR_R_MALLOC_FAILURE 33
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

#include "s3_lib_sce.inc"
