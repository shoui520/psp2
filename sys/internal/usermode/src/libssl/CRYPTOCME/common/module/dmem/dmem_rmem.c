/* Filename and private descriptive names are ours; R_malloc-backed direct-memory method. */
#include "dmem.h"
#include "../../../../memory.h"

static int sslControlDefaultDirectMemoryContext(SslDirectMemoryContext *ctx,
						int cmd, int arg1, void *arg2);
static int sslCreateDefaultDirectMemoryContext(
	SslDirectMemoryContext **outCtx, const SslDirectMemoryMethod *meth,
	SslDirectMemoryContext *parent, void *arg);
static int sslDestroyDefaultDirectMemoryContext(SslDirectMemoryContext *ctx);
static int sslAllocateDefaultDirectMemory(void **outPtr, SceUInt32 size,
					  SslDirectMemoryContext *ctx,
					  SceUInt32 flags);
static void sslFreeDefaultDirectMemory(void *ptr, SslDirectMemoryContext *ctx);
static int sslReallocateDefaultDirectMemory(void **ptrRef, SceUInt32 size,
					    SslDirectMemoryContext *ctx,
					    SceUInt32 oldSize, SceUInt32 flags);

static const SslDirectMemoryMethod sslDefaultDirectMemoryMethod = {
	"malloc using R_malloc",
	sslCreateDefaultDirectMemoryContext,
	sslDestroyDefaultDirectMemoryContext,
	sslAllocateDefaultDirectMemory,
	sslReallocateDefaultDirectMemory,
	sslFreeDefaultDirectMemory,
	sslControlDefaultDirectMemoryContext
};

const SslDirectMemoryMethod *sslGetDefaultDirectMemoryMethod(void)
{
	return &sslDefaultDirectMemoryMethod;
}

static int sslControlDefaultDirectMemoryContext(SslDirectMemoryContext *ctx,
						int cmd, int arg1, void *arg2)
{
	(void)ctx;
	(void)cmd;
	(void)arg1;
	(void)arg2;
	return 0;
}

static int sslCreateDefaultDirectMemoryContext(
	SslDirectMemoryContext **outCtx, const SslDirectMemoryMethod *meth,
	SslDirectMemoryContext *parent, void *arg)
{
	SslDirectMemoryContext *ctx;
	int err;

	(void)meth;
	(void)arg;
	if (parent != NULL) {
		err = parent->methods->Allocate((void **)&ctx, sizeof(*ctx),
						parent, 0x100);
		/* Sony's test is inverted: a successful allocation returns at
		 * once and leaves *outCtx unset */
		if (err == 0)
			return err;
		ctx->parent = parent;
	} else {
		ctx = sslAllocate(sizeof(*ctx));
		if (ctx == NULL)
			return 0x2713;
		sslFillMemory(ctx, 0, sizeof(*ctx));
	}
	ctx->methods = &sslDefaultDirectMemoryMethod;
	ctx->type = 0;
	ctx->flags |= 1u;
	*outCtx = ctx;
	return 0;
}

static int sslDestroyDefaultDirectMemoryContext(SslDirectMemoryContext *ctx)
{
	if ((ctx->flags & 1u) != 0) {
		if (ctx->parent != NULL)
			ctx->parent->methods->Free(ctx, ctx->parent);
		else
			sslFree(ctx);
	}
	return 0;
}

static int sslAllocateDefaultDirectMemory(void **outPtr, SceUInt32 size,
					  SslDirectMemoryContext *ctx,
					  SceUInt32 flags)
{
	void *ptr;

	(void)ctx;
	if (size == 0)
		size = 1;
	ptr = sslAllocate(size);
	if (ptr == NULL)
		return 0x2713;
	if ((flags & 0x100u) != 0)
		sslFillMemory(ptr, 0, size);
	*outPtr = ptr;
	return 0;
}

static void sslFreeDefaultDirectMemory(void *ptr, SslDirectMemoryContext *ctx)
{
	(void)ctx;
	if (ptr != NULL)
		sslFree(ptr);
}

static int sslReallocateDefaultDirectMemory(void **ptrRef, SceUInt32 size,
					    SslDirectMemoryContext *ctx,
					    SceUInt32 oldSize, SceUInt32 flags)
{
	void *ptr = *ptrRef;
	void *newPtr;

	(void)ctx;
	if (size == 0)
		size = 1;
	if (ptr == NULL)
		return sslAllocateDefaultDirectMemory(ptrRef, size, ctx, flags);
	newPtr = sslReallocate(ptr, size, oldSize);
	if (newPtr == NULL)
		return 0x2713;
	if ((flags & 0x100u) != 0 && (int)size > (int)oldSize)
		sslFillMemory((unsigned char *)newPtr + oldSize, 0,
			      size - oldSize);
	*ptrRef = newPtr;
	return 0;
}
