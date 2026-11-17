/*
 * SceNpCommonPs4: HttpContext (npext/http/http_context.cpp), the HTTP template and the connection
 * pool a library creates its NP requests from.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_httpContextVtable[];	/* 0x8102aa38 */

struct NpCommonPs4HttpContext *SceNpCommonPs4_53D6E2C9(struct NpCommonPs4HttpContext *context,
	struct NpCommonPs4AllocatorWithError *pool)
{
	context->pool = pool;
	context->vtable = NP_COMMON_PS4_VTABLE(g_httpContextVtable);
	context->tmpl = NULL;
	context->connPool = NULL;
	context->libHttpCtxId = -1;
	return context;
}

struct NpCommonPs4HttpContext *SceNpCommonPs4_05B5B96E(struct NpCommonPs4HttpContext *context,
	struct NpCommonPs4AllocatorWithError *pool)
{
	context->pool = pool;
	context->vtable = NP_COMMON_PS4_VTABLE(g_httpContextVtable);
	context->tmpl = NULL;
	context->connPool = NULL;
	context->libHttpCtxId = -1;
	return context;
}

struct NpCommonPs4HttpContext *SceNpCommonPs4_B8A829E7(struct NpCommonPs4HttpContext *context)
{
	struct NpCommonPs4HttpConnPool *connPool;
	struct NpCommonPs4HttpTemplate *tmpl;

	connPool = context->connPool;
	context->vtable = NP_COMMON_PS4_VTABLE(g_httpContextVtable);
	if (connPool != NULL)
		((void (*)(struct NpCommonPs4HttpConnPool *))connPool->vtable[1])(connPool);
	tmpl = context->tmpl;
	if (tmpl != NULL)
		((void (*)(struct NpCommonPs4HttpTemplate *))tmpl->vtable[1])(tmpl);
	context->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return context;
}

int SceNpCommonPs4_03010876(struct NpCommonPs4HttpContext *context)
{
	struct NpCommonPs4HttpConnPool *connPool;
	struct NpCommonPs4HttpTemplate *tmpl;

	connPool = context->connPool;
	context->vtable = NP_COMMON_PS4_VTABLE(g_httpContextVtable);
	if (connPool != NULL)
		((void (*)(struct NpCommonPs4HttpConnPool *))connPool->vtable[1])(connPool);
	tmpl = context->tmpl;
	if (tmpl != NULL)
		((void (*)(struct NpCommonPs4HttpTemplate *))tmpl->vtable[1])(tmpl);
	context->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(context);
}

struct NpCommonPs4HttpContext *SceNpCommonPs4_5502B83B(struct NpCommonPs4HttpContext *context)
{
	struct NpCommonPs4HttpConnPool *connPool;
	struct NpCommonPs4HttpTemplate *tmpl;

	connPool = context->connPool;
	context->vtable = NP_COMMON_PS4_VTABLE(g_httpContextVtable);
	if (connPool != NULL)
		((void (*)(struct NpCommonPs4HttpConnPool *))connPool->vtable[1])(connPool);
	tmpl = context->tmpl;
	if (tmpl != NULL)
		((void (*)(struct NpCommonPs4HttpTemplate *))tmpl->vtable[1])(tmpl);
	context->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return context;
}

int SceNpCommonPs4_100E042E(struct NpCommonPs4HttpContext *context, SceInt32 libHttpCtxId, SceInt32 connNumMax)
{
	struct NpCommonPs4HttpTemplate *tmpl;
	struct NpCommonPs4HttpConnPool *connPool;
	void *memory;
	int result;

	tmpl = NULL;
	memory = SceNpCommonPs4_D154D84F(sizeof(struct NpCommonPs4HttpTemplate), (struct NpCommonPs4Allocator *)context->pool);
	if (memory != NULL)
		tmpl = npCommonPs4HttpTemplateConstruct((struct NpCommonPs4HttpTemplate *)memory);
	context->tmpl = tmpl;
	if (tmpl != NULL) {
		result = npCommonPs4HttpTemplateInit(tmpl, libHttpCtxId, NULL, 2, 0);
		if (result >= 0) {
			if (connNumMax > 0) {
				connPool = NULL;
				memory = SceNpCommonPs4_D154D84F(sizeof(struct NpCommonPs4HttpConnPool),
					(struct NpCommonPs4Allocator *)context->pool);
				if (memory != NULL)
					connPool = npCommonPs4HttpConnPoolConstruct((struct NpCommonPs4HttpConnPool *)memory, context->pool);
				context->connPool = connPool;
				if (connPool == NULL) {
					result = context->pool->error;
					goto fail;
				}
				npCommonPs4HttpConnPoolInit(connPool, connNumMax);
				context->libHttpCtxId = libHttpCtxId;
			}
			return 0;
		}
	} else {
		result = context->pool->error;
	}
fail:
	connPool = context->connPool;
	if (connPool != NULL)
		((void (*)(struct NpCommonPs4HttpConnPool *))connPool->vtable[1])(connPool);
	tmpl = context->tmpl;
	context->connPool = NULL;
	if (tmpl != NULL)
		((void (*)(struct NpCommonPs4HttpTemplate *))tmpl->vtable[1])(tmpl);
	context->tmpl = NULL;
	return result;
}

const void *g_httpContextVtable[] = {		/* 0x8102aa38 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_B8A829E7,
	(const void *)SceNpCommonPs4_03010876,
};

}
