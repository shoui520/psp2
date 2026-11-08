/* Identified SSLeay SSL library init and temporary RSA key helpers, adapted in ssl_algs_sce.patch. */
#include "ssl_locl.h"
#include "../err/err.h"
#include "../CRYPTOCME/common/key/pkey_ctx.h"
#include "../CRYPTOCME/common/module/crypto/cry_api.h"

#define SSL_CTX_ctrl SceSslInternal_CC7F4211

#include "ssl_algs_sce.inc"
