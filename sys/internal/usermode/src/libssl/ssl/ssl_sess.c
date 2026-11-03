/* Identified SSLeay session operations, adapted in ssl_sess_sce.patch. */
#include "ssl_locl.h"
#include "../memory.h"
#include "../runtime.h"
#include "../time.h"
#include "../err/err.h"

#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#define SSL_F_SSL_SESSION_NEW 189
#define ERR_R_MALLOC_FAILURE 33
#define CRYPTO_LOCK_SSL_SESSION 14
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

#include "ssl_sess_sce.inc"
