/* SSLeay names and layout, adapted to the original SSL-C ABI. */
#ifndef LIBSSL_BIO_H
#define LIBSSL_BIO_H

#include <scetypes.h>
#include <stdio.h>
#include <stddef.h>

/* Sony export link names; source API names are SSLeay's. */
#define BIO_write SceSslInternal_33AC7A16
#define BIO_free SceSslInternal_C4D984C2
#include "bio_sce.h"

/* SSL-C turns these upstream macros into separately compiled helpers. */
#undef BIO_set_flags
#undef BIO_get_retry_flags
BIO *BIO_set_flags(BIO *bio, int flags);
int BIO_get_retry_flags(BIO *bio);

/* Descriptive name for SSL-C's extra retry bit is ours. */
#define SCE_BIO_RETRY_EXTENSION 0x40

_Static_assert(sizeof(BIO) == 60, "BIO");
_Static_assert(offsetof(BIO_METHOD, ctrl) == 24, "ctrl");
_Static_assert(offsetof(BIO, references) == 44, "references");
_Static_assert(offsetof(BIO, num_write) == 52, "num_write");

/* These descriptive names are ours, not recovered Sony names. */
STACK *sslGetBioExDataMethods(void);
STACK *sslSetBioExDataMethods(STACK *methods);

#endif
