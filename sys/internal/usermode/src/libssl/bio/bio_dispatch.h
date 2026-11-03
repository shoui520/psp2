/* Inline the adapted upstream body used by BIO's out-of-line dispatcher too. */
#ifndef LIBSSL_BIO_DISPATCH_H
#define LIBSSL_BIO_DISPATCH_H

#include "bio.h"
#include "../err/err.h"

#define BIOerr(function, reason) sslPushError(32, function, reason, 0, 0)
/* This inline helper's descriptive name is ours. */
#define BIO_ctrl sslBioCtrlInline
#define SCE_BIO_CTRL_ONLY
#define SCE_BIO_CTRL_DECL static inline
#include "bio_lib_sce.inc"
#undef SCE_BIO_CTRL_DECL
#undef SCE_BIO_CTRL_ONLY
#undef BIO_ctrl

#endif
