/* Private descriptive names and this filename are ours, not recovered Sony names. */
#ifndef LIBSSL_RUNTIME_H
#define LIBSSL_RUNTIME_H

#include <scetypes.h>

typedef int (*SslLockCallback)(int mode, int index, const char *file, int line);
typedef int (*SslAddLockCallback)(int *value, int amount, int index,
				  const char *file, int line);
typedef SceUInt32 (*SslThreadCallback)(int arg);

extern SslLockCallback sslLockCallback;

enum {
	SSL_LOCK_ERROR_QUEUE = 5,
	SSL_UNLOCK_ERROR_QUEUE = 6,
	SSL_LOCK_REFERENCE = 9,
	SSL_UNLOCK_REFERENCE = 10
};

void *sslResetLockState(void);
int sslNumLocks(void);
int sslGetNewLockId(char *name);
int sslCleanupDynamicLocks(void);
char *sslGetLockName(int type);
int sslLock(int mode, int index, const char *file, int line);
SslLockCallback sslGetLockCallback(void);
SslAddLockCallback sslGetAddLockCallback(void);
int sslSetLockCallback(SslLockCallback callback);
int sslSetAddLockCallback(SslAddLockCallback callback);
int sslAddReference(int *value, int amount, int index, const char *file, int line);
SslThreadCallback sslGetThreadCallback(void);
int sslSetThreadCallback(SslThreadCallback callback);
SceUInt32 sslThreadId(void);
SceUInt32 sslUpdateRuntimeFlags(int command);

#endif
