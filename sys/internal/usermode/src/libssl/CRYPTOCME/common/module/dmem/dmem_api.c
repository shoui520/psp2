/* Filename and private descriptive names are ours; CRYPTOCME direct-memory dispatch. */
#include "dmem.h"
#include "../../../../memory.h"

int sslNewDirectMemoryContext(SslDirectMemoryContext **ctx,
			      const SslDirectMemoryMethod *meth,
			      SslDirectMemoryContext *parent, void *arg)
{
	if (meth == NULL) {
		if (parent == NULL)
			return 0x271c;
		meth = parent->methods;
	}
	return meth->Create(ctx, meth, parent, arg);
}

int sslFreeDirectMemoryContext(SslDirectMemoryContext *ctx)
{
	if (ctx == NULL)
		return 0x271c;
	return ctx->methods->Destroy(ctx);
}

int sslControlDirectMemoryContext(SslDirectMemoryContext *ctx, int cmd,
				  int arg1, void *arg2)
{
	if (ctx == NULL)
		return 0x271c;
	return ctx->methods->Control(ctx, cmd, arg1, arg2);
}

int sslAllocateDirectMemory(void **ptr, SceUInt32 size,
			    SslDirectMemoryContext *ctx, SceUInt32 flags)
{
	return ctx->methods->Allocate(ptr, size, ctx, flags);
}

void sslFreeDirectMemory(void *ptr, SslDirectMemoryContext *ctx)
{
	if (ptr != NULL)
		ctx->methods->Free(ptr, ctx);
}

void sslWipeAndFreeDirectMemory(void *ptr, SceUInt32 size,
				SslDirectMemoryContext *ctx)
{
	if (ptr != NULL) {
		sslFillMemory(ptr, 0, size);
		ctx->methods->Free(ptr, ctx);
	}
}

int sslReallocateDirectMemory(void **ptr, SceUInt32 size,
			      SslDirectMemoryContext *ctx, SceUInt32 oldSize,
			      SceUInt32 flags)
{
	return ctx->methods->Reallocate(ptr, size, ctx, oldSize, flags);
}
