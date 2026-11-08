/* Identified SSLeay cipher operations, adapted in ssl_ciph_sce.patch. */
#include "ssl_locl.h"
#include "ssl_ext.h"
#include "../err/err.h"
#include "../format.h"
#include "../memory.h"
#include "../CRYPTOCME/common/key/pkey_ctx.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"

#define SSL_F_SSL_CIPHER_GET_BITS 158
#define SSL_F_SSL_CIPHER_DESCRIPTION 216
#define SSL_R_CIPHER_OR_HASH_UNAVAILABLE 138
#define ERR_R_BUF_LIB 16
#define ERR_R_MALLOC_FAILURE 33
#define SSLerr(function, reason) sslPushError(20, function, reason, NULL, 0)

#define Malloc sslAllocate
#define strncpy sslCopyString
#define sprintf sslFormatString
#define SSL_CIPHER_get_name SceSslInternal_2B90012E

#include "ssl_ciph_sce.inc"
