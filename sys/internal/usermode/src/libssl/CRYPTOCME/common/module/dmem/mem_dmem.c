/* Source path S:\work\CRYPTOCME\common\module/dmem/mem_dmem.c is recovered from
 * the binary's lock callback strings; private descriptive names are ours.
 */
#include "dmem.h"
#include "../../../../runtime.h"

#define SSL_DMEM_SOURCE_FILE "S:\\work\\CRYPTOCME\\common\\module/dmem/mem_dmem.c"
#define SSL_LOCK_DMEM 24

SslDirectMemoryContext *sslDefaultDirectMemoryContext = NULL;
static int sslDirectMemoryRefCount = 0;

static int sslLockDirectMemory(int mode, int type, const char *file, int line)
{
	if (sslLockCallback != NULL)
		return sslLockCallback(mode, type, file, line);
	return 1;
}

SslDirectMemoryContext *sslGetDefaultDirectMemoryContext(void)
{
	if (sslDefaultDirectMemoryContext == NULL) {
		sslNewDirectMemoryContext(&sslDefaultDirectMemoryContext,
					  sslGetDefaultDirectMemoryMethod(),
					  NULL, NULL);
	}
	return sslDefaultDirectMemoryContext;
}

void sslResetDirectMemoryState(void)
{
	sslDefaultDirectMemoryContext = NULL;
	sslDirectMemoryRefCount = 0;
}

int sslCleanupDirectMemoryModule(void *library, int flag)
{
	(void)library;
	(void)flag;
	sslLockDirectMemory(SSL_LOCK_REFERENCE, SSL_LOCK_DMEM,
			    SSL_DMEM_SOURCE_FILE, 74);
	sslDirectMemoryRefCount -= 1;
	if (sslDirectMemoryRefCount == 0 &&
	    sslDefaultDirectMemoryContext != NULL) {
		sslFreeDirectMemoryContext(sslDefaultDirectMemoryContext);
		sslDefaultDirectMemoryContext = NULL;
	}
	sslLockDirectMemory(SSL_UNLOCK_REFERENCE, SSL_LOCK_DMEM,
			    SSL_DMEM_SOURCE_FILE, 83);
	return 0;
}

int sslInitDirectMemoryModule(void *library, int flag)
{
	(void)library;
	(void)flag;
	sslLockDirectMemory(SSL_LOCK_REFERENCE, SSL_LOCK_DMEM,
			    SSL_DMEM_SOURCE_FILE, 113);
	sslDirectMemoryRefCount += 1;
	sslLockDirectMemory(SSL_UNLOCK_REFERENCE, SSL_LOCK_DMEM,
			    SSL_DMEM_SOURCE_FILE, 115);
	return 0;
}
