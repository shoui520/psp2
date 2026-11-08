/* Identified SSLeay session operations, adapted in ssl_sess_sce.patch. */
#include "ssl_locl.h"
#include "ssl_context.h"
#include "ssl_ext.h"
#include "../memory.h"
#include "../runtime.h"
#include "../time.h"
#include "../err/err.h"

#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#define memcpy sslCopyMemory
#define SSL3_VERSION 0x0300
#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define SSL_F_SSL_GET_NEW_SESSION 181
#define SSL_F_SSL_SESSION_NEW 189
#define SSL_F_SSL_SET_SESSION 196
#define ERR_R_MALLOC_FAILURE 33
#define SSL_R_UNABLE_TO_FIND_SSL_METHOD 240
#define SSL_R_UNSUPPORTED_SSL_VERSION 259
#define SSL_R_RANDOM_GENERATION_FAILED 1116
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

#include "ssl_sess_sce.inc"
