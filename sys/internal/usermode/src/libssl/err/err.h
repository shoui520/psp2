/* Private descriptive names and this header's filename are ours. */
#ifndef LIBSSL_ERR_H
#define LIBSSL_ERR_H

#include <scetypes.h>

void *sslClearErrors(void);
int sslPushError(int library, int function, int reason, const char *file, int line);
int sslRemoveThreadErrors(SceUInt32 threadId);
int sslClearThreadErrors(void);
SceUInt32 sslReadError(int remove);
SceUInt32 sslGetError(void);
SceUInt32 sslPeekError(void);

#endif
