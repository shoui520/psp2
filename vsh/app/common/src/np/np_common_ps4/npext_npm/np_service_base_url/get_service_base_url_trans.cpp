/*
 * SceNpCommonPs4: GetServiceBaseUrl (npext_npm/np_service_base_url/get_service_base_url_trans.cpp), the request that
 * asks the account service manager (asm) for the base URL of an NP service. The response is a JSON
 * object {"url": "..."} that a parser (this file's observer) turns into a string.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

/* the JSON parser with the observer that fetches the "url" member (the observer is a second base, at the
   end of the parser) */
struct NpCommonPs4ServiceBaseUrlParser {
	struct NpCommonPs4JsonParser parser;
	const void *const *observerVtable;
	struct NpCommonPs4StringHolder *url;
};

extern const void *g_serviceBaseUrlTransVtable[];	/* 0x8102aaac: the vtables of the transaction and of its second base */
extern const void *g_serviceBaseUrlParserVtable[];	/* 0x8102aa74: the vtables of the parser and of its observer base */
extern const void *g_stringHolderVtable[];		/* 0x8102aad8 */
extern unsigned char g_serviceBaseUrlParseConfig[];	/* 0x81028880: the members the parser looks for */

/* the observer base's callbacks that the observer leaves alone (they return 0) */
int npCommonPs4JsonObserverDefault1(void)
{
	return 0;
}

int npCommonPs4JsonObserverDefault2(void)
{
	return 0;
}

int npCommonPs4JsonObserverDefault3(void)
{
	return 0;
}

int npCommonPs4JsonObserverDefault4(void)
{
	return 0;
}

int npCommonPs4JsonObserverDefault5(void)
{
	return 0;
}

int npCommonPs4JsonObserverDefault6(void)
{
	return 0;
}

__attribute__((noinline)) struct NpCommonPs4NpTrans *npCommonPs4ServiceBaseUrlTransConstruct(struct NpCommonPs4NpTrans *npTrans,
	struct NpCommonPs4Allocator *pool, const struct NpCommonPs4OnlineId *onlineId)
{
	SceNpCommonPs4_3FC6688C(npTrans, pool, onlineId);
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlTransVtable);
	npTrans->trans.vtableSecondBase = &g_serviceBaseUrlTransVtable[8];
	return npTrans;
}

__attribute__((noinline)) struct NpCommonPs4NpTrans *npCommonPs4ServiceBaseUrlTransDestruct(struct NpCommonPs4NpTrans *npTrans)
{
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlTransVtable);
	npTrans->trans.vtableSecondBase = &g_serviceBaseUrlTransVtable[8];
	SceNpCommonPs4_47D94EF0(npTrans);
	return npTrans;
}

__attribute__((noinline)) int npCommonPs4ServiceBaseUrlTransDelete(struct NpCommonPs4NpTrans *npTrans)
{
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlTransVtable);
	npTrans->trans.vtableSecondBase = &g_serviceBaseUrlTransVtable[8];
	SceNpCommonPs4_47D94EF0(npTrans);
	return SceNpCommonPs4_BB5282C8(npTrans);
}

int SceNpCommonPs4_80976815(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle,
	struct NpCommonPs4HttpContext *context, const char *titleId, const char *serviceName)
{
	char *url;
	char environment[20];
	int result;

	url = (char *)SceNpCommonPs4_299ADFC3(npTrans->trans.pool, 128);
	if (url != NULL) {
		result = npCommonPs4NpEnvironmentGet(environment);
		if (result >= 0) {
			sceClibSnprintf(url, 128, "https://asm.%s.community.playstation.net/asm/v1/apps/%s/baseUrls/%s",
				environment, titleId, serviceName);
			result = SceNpCommonPs4_D30A8E84(&npTrans->trans, context->tmpl, context->connPool, 0, url, 0);
			if (result >= 0) {
				result = SceNpCommonPs4_49776226(npTrans, handle);
				if (result >= 0) {
					result = SceNpCommonPs4_3FC1A88F(&npTrans->trans, handle, NULL, 0);
					if (result >= 0)
						result = 0;
				}
			}
		}
	} else {
		result = ((struct NpCommonPs4AllocatorWithError *)npTrans->trans.pool)->error;
	}
	SceNpCommonPs4_38CC9AF5(npTrans->trans.pool, url);
	return result;
}

int SceNpCommonPs4_D26B641F(struct NpCommonPs4ServiceBaseUrlParser *parser, struct NpCommonPs4StringHolder *holder)
{
	int result;

	if (parser->url == NULL)
		return NP_COMMON_PS4_ERROR_80559E2E;
	result = SceNpCommonPs4_B0F7DE6E(holder, SceNpCommonPs4_2CE76BE5(parser->url));
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_C2B11396(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle,
	struct NpCommonPs4StringHolder *holder)
{
	struct NpCommonPs4ServiceBaseUrlParser *parser;
	struct NpCommonPs4ServiceBaseUrlParser *memory;
	int result;
	int status;

	result = SceNpCommonPs4_18DDD712(npTrans, handle);
	if (result >= 0) {
		parser = NULL;
		memory = (struct NpCommonPs4ServiceBaseUrlParser *)SceNpCommonPs4_D154D84F(sizeof(struct NpCommonPs4ServiceBaseUrlParser),
			npTrans->trans.pool);
		if (memory != NULL) {
			SceNpCommonPs4_4F720AFD(&memory->parser, npTrans->trans.pool);
			memory->url = NULL;
			memory->parser.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlParserVtable);
			memory->observerVtable = &g_serviceBaseUrlParserVtable[7];
			parser = memory;
			result = SceNpCommonPs4_89322005(&memory->parser, g_serviceBaseUrlParseConfig, &memory->observerVtable);
			if (result >= 0) {
				result = SceNpCommonPs4_7CC0800E(&parser->parser, (int)handle, npTrans);
				if (result >= 0) {
					if (parser->url != NULL) {
						status = SceNpCommonPs4_B0F7DE6E(holder, SceNpCommonPs4_2CE76BE5(parser->url));
						result = 0;
						if (status < 0)
							result = status;
					} else {
						result = NP_COMMON_PS4_ERROR_80559E2E;
					}
					if (result >= 0)
						result = 0;
				}
			}
		} else {
			result = ((struct NpCommonPs4AllocatorWithError *)npTrans->trans.pool)->error;
		}
		if (parser != NULL)
			((void (*)(struct NpCommonPs4ServiceBaseUrlParser *))parser->parser.vtable[1])(parser);
	}
	return result;
}

__attribute__((noinline)) struct NpCommonPs4ServiceBaseUrlParser *npCommonPs4ServiceBaseUrlParserDestruct(
	struct NpCommonPs4ServiceBaseUrlParser *parser)
{
	struct NpCommonPs4StringHolder *url;

	url = parser->url;
	parser->parser.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlParserVtable);
	parser->observerVtable = &g_serviceBaseUrlParserVtable[7];
	if (url != NULL)
		((void (*)(struct NpCommonPs4StringHolder *))url->vtable[1])(url);
	SceNpCommonPs4_0A3AA2EF(&parser->parser);
	return parser;
}

int npCommonPs4ServiceBaseUrlParserDelete(struct NpCommonPs4ServiceBaseUrlParser *parser)
{
	struct NpCommonPs4StringHolder *url;

	url = parser->url;
	parser->parser.vtable = NP_COMMON_PS4_VTABLE(g_serviceBaseUrlParserVtable);
	parser->observerVtable = &g_serviceBaseUrlParserVtable[7];
	if (url != NULL)
		((void (*)(struct NpCommonPs4StringHolder *))url->vtable[1])(url);
	SceNpCommonPs4_0A3AA2EF(&parser->parser);
	return SceNpCommonPs4_BB5282C8(parser);
}

int SceNpCommonPs4_F889B2B1(struct NpCommonPs4ServiceBaseUrlParser *parser, int depth, int index, const char *name,
	int valueType, const char *value)
{
	struct NpCommonPs4StringHolder *holder;
	struct NpCommonPs4StringHolder *memory;
	struct NpCommonPs4StringHolder *old;
	int result;

	if (depth != 1 || index != 0)
		return 0;
	if (sceClibStrncmp(name, "url", sizeof("url")) != 0)
		NP_COMMON_PS4_ASSERT(0, "onObjectValue", "npext_npm\\np_service_base_url\\get_service_base_url_trans.cpp", 67,
			"sceClibStrncmp((name), (\"url\"), (sizeof(\"url\")))==0");
	if (value == NULL)
		return NP_COMMON_PS4_ERROR_80559F7F;
	if (valueType != 4)
		return NP_COMMON_PS4_ERROR_80559E2D;
	holder = NULL;
	memory = (struct NpCommonPs4StringHolder *)SceNpCommonPs4_D154D84F(sizeof(struct NpCommonPs4StringHolder),
		parser->parser.pool);
	if (memory != NULL) {
		holder = SceNpCommonPs4_FDA863C0(memory, (struct NpCommonPs4AllocatorWithError *)parser->parser.pool);
		if (holder != NULL) {
			result = SceNpCommonPs4_B0F7DE6E(holder, value);
			if (result < 0)
				goto fail;
			old = parser->url;
			if (old != NULL)
				((void (*)(struct NpCommonPs4StringHolder *))old->vtable[1])(old);
			parser->url = holder;
			return 0;
		}
	}
	result = ((struct NpCommonPs4AllocatorWithError *)parser->parser.pool)->error;
fail:
	if (holder != NULL)
		((void (*)(struct NpCommonPs4StringHolder *))holder->vtable[1])(holder);
	return result;
}

int npCommonPs4ServiceBaseUrlTransDeleteThunk(char *secondBase)
{
	return npCommonPs4ServiceBaseUrlTransDelete((struct NpCommonPs4NpTrans *)(secondBase - 4));
}

struct NpCommonPs4NpTrans *npCommonPs4ServiceBaseUrlTransDestructThunk(char *secondBase)
{
	return npCommonPs4ServiceBaseUrlTransDestruct((struct NpCommonPs4NpTrans *)(secondBase - 4));
}

int SceNpCommonPs4_FFB0E497(char *observer, int depth, int index, const char *name, int valueType, const char *value)
{
	return SceNpCommonPs4_F889B2B1((struct NpCommonPs4ServiceBaseUrlParser *)(observer - 1876), depth, index, name,
		valueType, value);
}

__attribute__((noinline)) struct NpCommonPs4StringHolder *SceNpCommonPs4_FDA863C0(struct NpCommonPs4StringHolder *holder,
	struct NpCommonPs4AllocatorWithError *pool)
{
	holder->pool = pool;
	holder->vtable = NP_COMMON_PS4_VTABLE(g_stringHolderVtable);
	holder->string = NULL;
	return holder;
}

struct NpCommonPs4StringHolder *SceNpCommonPs4_BDD7DD38(struct NpCommonPs4StringHolder *holder,
	struct NpCommonPs4AllocatorWithError *pool)
{
	holder->pool = pool;
	holder->vtable = NP_COMMON_PS4_VTABLE(g_stringHolderVtable);
	holder->string = NULL;
	return holder;
}

struct NpCommonPs4StringHolder *SceNpCommonPs4_483168C0(struct NpCommonPs4StringHolder *holder)
{
	char *string;
	struct NpCommonPs4AllocatorWithError *pool;

	string = holder->string;
	pool = holder->pool;
	holder->vtable = NP_COMMON_PS4_VTABLE(g_stringHolderVtable);
	SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)pool, string);
	holder->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return holder;
}

int SceNpCommonPs4_5F96ACFB(struct NpCommonPs4StringHolder *holder)
{
	char *string;
	struct NpCommonPs4AllocatorWithError *pool;

	string = holder->string;
	pool = holder->pool;
	holder->vtable = NP_COMMON_PS4_VTABLE(g_stringHolderVtable);
	SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)pool, string);
	holder->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(holder);
}

struct NpCommonPs4StringHolder *SceNpCommonPs4_658C86FE(struct NpCommonPs4StringHolder *holder)
{
	char *string;
	struct NpCommonPs4AllocatorWithError *pool;

	string = holder->string;
	pool = holder->pool;
	holder->vtable = NP_COMMON_PS4_VTABLE(g_stringHolderVtable);
	SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)pool, string);
	holder->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return holder;
}

int SceNpCommonPs4_5E28AFF0(struct NpCommonPs4StringHolder *holder)
{
	SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)holder->pool, holder->string);
	holder->string = NULL;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_B0F7DE6E(struct NpCommonPs4StringHolder *holder, const char *string)
{
	char *copy;
	struct NpCommonPs4AllocatorWithError *pool;

	copy = npCommonPs4AllocatorDuplicateString((struct NpCommonPs4Allocator *)holder->pool, string);
	pool = holder->pool;
	if (copy == NULL)
		return pool->error;
	SceNpCommonPs4_38CC9AF5((struct NpCommonPs4Allocator *)pool, holder->string);
	holder->string = copy;
	return 0;
}

__attribute__((noinline)) const char *SceNpCommonPs4_2CE76BE5(const struct NpCommonPs4StringHolder *holder)
{
	return holder->string;
}

int SceNpCommonPs4_0B2A3748(struct NpCommonPs4StringHolder *holder, struct NpCommonPs4Handle *handle,
	struct NpCommonPs4HttpContext *context, const struct NpCommonPs4OnlineId *onlineId, const char *titleId,
	const char *serviceName)
{
	struct NpCommonPs4NpTrans trans;
	int result;
	int status;

	npCommonPs4ServiceBaseUrlTransConstruct(&trans, (struct NpCommonPs4Allocator *)holder->pool, onlineId);
	result = SceNpCommonPs4_80976815(&trans, handle, context, titleId, serviceName);
	if (result >= 0) {
		status = SceNpCommonPs4_C2B11396(&trans, handle, holder);
		npCommonPs4ServiceBaseUrlTransDestruct(&trans);
		if (status >= 0)
			result = 0;
		else
			result = status;
	} else {
		npCommonPs4ServiceBaseUrlTransDestruct(&trans);
	}
	return result;
}

const void *g_serviceBaseUrlParserVtable[] = {		/* 0x8102aa74 */
	NULL,
	NULL,
	(const void *)npCommonPs4ServiceBaseUrlParserDestruct,
	(const void *)npCommonPs4ServiceBaseUrlParserDelete,
	(const void *)SceNpCommonPs4_F889B2B1,
	(const void *)-1876,
	NULL,
	(const void *)npCommonPs4JsonObserverDefault1,	/* the observer base: callbacks that do nothing but 3 */
	(const void *)npCommonPs4JsonObserverDefault2,
	(const void *)npCommonPs4JsonObserverDefault3,
	(const void *)SceNpCommonPs4_FFB0E497,
	(const void *)npCommonPs4JsonObserverDefault4,
	(const void *)npCommonPs4JsonObserverDefault5,
	(const void *)npCommonPs4JsonObserverDefault6,
};

const void *g_serviceBaseUrlTransVtable[] = {		/* 0x8102aaac */
	NULL,
	NULL,
	(const void *)npCommonPs4ServiceBaseUrlTransDestruct,
	(const void *)npCommonPs4ServiceBaseUrlTransDelete,
	(const void *)SceNpCommonPs4_47443DCE,
	(const void *)SceNpCommonPs4_F81085EE,
	(const void *)-4,
	NULL,
	(const void *)npCommonPs4ServiceBaseUrlTransDestructThunk,
	(const void *)npCommonPs4ServiceBaseUrlTransDeleteThunk,
	(const void *)SceNpCommonPs4_8EBA83E1,
};

const void *g_stringHolderVtable[] = {			/* 0x8102aad8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_483168C0,
	(const void *)SceNpCommonPs4_5F96ACFB,
};

}
