/* Filename and private descriptive names are ours; CRYPTOCME direct-memory interface. */
#ifndef LIBSSL_CRYPTOCME_COMMON_MODULE_DMEM_DMEM_H
#define LIBSSL_CRYPTOCME_COMMON_MODULE_DMEM_DMEM_H

/* Allocate flags: clear the new memory */
#define SSL_DMEM_ALLOCATE_ZERO 0x100

#include <stddef.h>
#include <scetypes.h>

typedef struct SslDirectMemoryContext SslDirectMemoryContext;
typedef struct SslDirectMemoryMethod SslDirectMemoryMethod;

struct SslDirectMemoryMethod {
	const char *name;
	int (*Create)(SslDirectMemoryContext **ctx,
		      const SslDirectMemoryMethod *meth,
		      SslDirectMemoryContext *parent, void *arg);
	int (*Destroy)(SslDirectMemoryContext *ctx);
	int (*Allocate)(void **ptr, SceUInt32 size,
			SslDirectMemoryContext *ctx, SceUInt32 flags);
	int (*Reallocate)(void **ptr, SceUInt32 size,
			  SslDirectMemoryContext *ctx, SceUInt32 oldSize,
			  SceUInt32 flags);
	void (*Free)(void *ptr, SslDirectMemoryContext *ctx);
	int (*Control)(SslDirectMemoryContext *ctx, int cmd, int arg1,
		       void *arg2);
};

struct SslDirectMemoryContext {
	SslDirectMemoryContext *parent;
	const SslDirectMemoryMethod *methods;
	int type;
	void *data;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslDirectMemoryMethod) == 28, "SslDirectMemoryMethod");
_Static_assert(sizeof(SslDirectMemoryContext) == 20, "SslDirectMemoryContext");

SslDirectMemoryContext *sslGetDefaultDirectMemoryContext(void);
void sslResetDirectMemoryState(void);
int sslCleanupDirectMemoryModule(void *library, int flag);
int sslInitDirectMemoryModule(void *library, int flag);

int sslNewDirectMemoryContext(SslDirectMemoryContext **ctx,
			      const SslDirectMemoryMethod *meth,
			      SslDirectMemoryContext *parent, void *arg);
int sslFreeDirectMemoryContext(SslDirectMemoryContext *ctx);
int sslControlDirectMemoryContext(SslDirectMemoryContext *ctx, int cmd,
				  int arg1, void *arg2);
int sslAllocateDirectMemory(void **ptr, SceUInt32 size,
			    SslDirectMemoryContext *ctx, SceUInt32 flags);
void sslFreeDirectMemory(void *ptr, SslDirectMemoryContext *ctx);
void sslWipeAndFreeDirectMemory(void *ptr, SceUInt32 size,
				SslDirectMemoryContext *ctx);
int sslReallocateDirectMemory(void **ptr, SceUInt32 size,
			      SslDirectMemoryContext *ctx, SceUInt32 oldSize,
			      SceUInt32 flags);

const SslDirectMemoryMethod *sslGetDefaultDirectMemoryMethod(void);
extern SslDirectMemoryContext *sslDefaultDirectMemoryContext;

static __inline SslDirectMemoryContext *
sslGetDefaultDirectMemoryContextInline(void)
{
	if (sslDefaultDirectMemoryContext == NULL) {
		sslNewDirectMemoryContext(&sslDefaultDirectMemoryContext,
					  sslGetDefaultDirectMemoryMethod(),
					  NULL, NULL);
	}
	return sslDefaultDirectMemoryContext;
}

#endif
