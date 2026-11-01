/*
 * SceSslInternal: libssl's library for the system's own users (libhttp), the exports this
 * tree calls so far.
 */

#ifndef _LIBSSL_SSLINTERNAL_H
#define _LIBSSL_SSLINTERNAL_H

#include <scetypes.h>

/* the callback libhttp installs on an SSL context */
typedef int (*SceSslInternalCallback)(int ctx, void *data, int arg2, int arg3, int *result);

int SceSslInternal_260F5A85(int ctx, int value);
int SceSslInternal_AF50B945(int ctx, SceSslInternalCallback callback, void *arg);

#endif /* _LIBSSL_SSLINTERNAL_H */
