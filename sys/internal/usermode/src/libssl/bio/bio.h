/* SSLeay names and layout, adapted to the original SSL-C ABI. */
#ifndef LIBSSL_BIO_H
#define LIBSSL_BIO_H

#include <scetypes.h>
#include <stdio.h>
#include <stddef.h>

/* Sony export link names; source API names are SSLeay's. */
#define BIO_write SceSslInternal_33AC7A16
#define BIO_free SceSslInternal_C4D984C2
#define BIO_new_socket SceSslInternal_FFE2ED51
#define BIO_printf SceSslInternal_CB83EA07
#include "bio_sce.h"

/* SSL-C turns these upstream macros into separately compiled helpers. */
#undef BIO_set_flags
#undef BIO_get_retry_flags
BIO *BIO_set_flags(BIO *bio, int flags);
int BIO_get_retry_flags(BIO *bio);
#undef BIO_clear_retry_flags
#undef BIO_set_retry_read
#undef BIO_set_retry_write
BIO *BIO_clear_retry_flags(BIO *bio);
BIO *BIO_set_retry_read(BIO *bio);
BIO *BIO_set_retry_write(BIO *bio);
/* Parentheses preserve the upstream macros for native inline uses. */
int (BIO_should_read)(BIO *bio);
int (BIO_should_write)(BIO *bio);
int (BIO_should_io_special)(BIO *bio);
#undef BIO_set_fd
#undef BIO_get_fd
long BIO_set_fd(BIO *bio, int fd, long closeFlag);
int BIO_get_fd(BIO *bio, int *fd);
BIO *SceSslInternal_B8E6E249(void);

/* Descriptive name for SSL-C's extra retry bit is ours. */
#define SCE_BIO_RETRY_EXTENSION 0x40

_Static_assert(sizeof(BIO) == 64, "BIO");
_Static_assert(offsetof(BIO, socketDescriptor) == 60, "socketDescriptor");
_Static_assert(offsetof(BIO_METHOD, ctrl) == 24, "ctrl");
_Static_assert(offsetof(BIO, references) == 44, "references");
_Static_assert(offsetof(BIO, num_write) == 52, "num_write");

/* These descriptive names are ours, not recovered Sony names. */
STACK *sslGetBioExDataMethods(void);
STACK *sslSetBioExDataMethods(STACK *methods);

#endif
