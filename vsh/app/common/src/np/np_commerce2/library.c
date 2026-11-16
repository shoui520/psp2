/*
 * SceNpCommerce2: the library's own exports (initialization, the memory it owns, and the
 * accessors made of other exports). A source file of its own: the calls to the exports stay calls.
 */

#include "np_commerce2_internal.h"

int g_libInitialized = 0;
SceUID g_heapMemBlock = -1;
SceClibMspace g_heap = NULL;

int sceNpCommerce2Init(void)
{
	int value = 0;
	void *base;
	char environment[20];
	int ret;
	int a2;
	int result;

	if (g_libInitialized != 0) {
		ret = SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;
	} else {
		ret = sceKernelAllocMemBlock("SceNpCommerce2", NP_COMMERCE2_HEAP_MEMTYPE,
					     NP_COMMERCE2_HEAP_SIZE, NULL);
		if (ret >= 0) {
			g_heapMemBlock = ret;
			ret = sceKernelGetMemBlockBase(ret, &base);
			if (ret >= 0) {
				g_heap = sceClibMspaceCreate(base, NP_COMMERCE2_HEAP_SIZE);
				if (g_heap != NULL) {
					ret = sceNpCommonGetNpEnviroment(environment);
					if (ret >= 0) {
						a2 = SceNpCommon_F585C86C();
						SceNpCommon_187C0A8D(&value);
						ret = npCommerce2_8100009C(environment, a2, value);
						if (ret >= 0) {
							g_libInitialized = 1;
							result = 0;
							goto done;
						}
					}
				}
			}
		}
	}
	if (g_heap != NULL) {
		sceClibMspaceDestroy(g_heap);
		g_heap = NULL;
	}
	if (g_heapMemBlock >= 0) {
		sceKernelFreeMemBlock(g_heapMemBlock);
		g_heapMemBlock = -1;
	}
	result = ret;
done:
	return result;
}

int sceNpCommerce2Term(void)
{
	if (g_libInitialized == 0)
		return 0;
	npCommerce2_81000126();
	if (g_heap != NULL) {
		sceClibMspaceDestroy(g_heap);
		g_heap = NULL;
	}
	if (g_heapMemBlock >= 0) {
		sceKernelFreeMemBlock(g_heapMemBlock);
		g_heapMemBlock = -1;
	}
	g_libInitialized = 0;
	return 0;
}

int sceNpCommerce2GetSessionInfo(SceNpCommerce2SessionInfo *sessionInfo, void *data,
				 SceSize dataSize)
{
	NpCommerce2SessionData session;
	char raw[96];
	int ret;

	if (g_libInitialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (data == NULL || dataSize == 0 || sessionInfo == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = SceNpCommerce2_1BFF9C73((int)raw, data, dataSize);
	if (ret >= 0) {
		ret = SceNpCommerce2_64E27331((int)raw, 0, (int)&session);
		if (ret >= 0) {
			sceClibMemset(sessionInfo, 0, 0x60);
			sceClibMemcpy_safe(sessionInfo->currencyCode, session.currencyCode, 3);
			sessionInfo->currencyCode[3] = '\0';
			sessionInfo->decimals = session.decimals;
			sceClibMemcpy_safe(sessionInfo->currencySymbol, session.currencySymbol, 3);
			sessionInfo->currencySymbol[3] = '\0';
			sessionInfo->symbolPosition = session.symbolPosition;
			sessionInfo->symbolWithSpace = session.symbolWithSpace;
			sceClibMemcpy_safe(sessionInfo->thousandSeparator, session.thousandSeparator, 4);
			sessionInfo->thousandSeparator[4] = '\0';
			sceClibMemcpy_safe(sessionInfo->decimalLetter, session.decimalLetter, 4);
			sessionInfo->decimalLetter[4] = '\0';
		}
	}
	return ret;
}

int sceNpCommerce2GetGameProductInfoFromContentInfo(const SceNpCommerce2ContentInfo *contentInfo,
						    SceNpCommerce2GameProductInfo *gameProductInfo)
{
	char entry[96];
	int ret;

	if (g_libInitialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	ret = SceNpCommerce2_F6A4FC50((int)contentInfo, (int)entry);
	if (ret >= 0)
		ret = SceNpCommerce2_EC09FF89((int)entry, (int)gameProductInfo);
	return ret;
}

int sceNpCommerce2GetGameProductInfo(const SceNpCommerce2GetProductInfoResult *result,
				     SceNpCommerce2GameProductInfo *gameProductInfo)
{
	char entry[96];
	int ret;

	if (g_libInitialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	ret = SceNpCommerce2_9E082DB2((int)result, (int)entry);
	if (ret >= 0)
		ret = SceNpCommerce2_F988A255((const SceNpCommerce2GetProductInfoResult *)entry, gameProductInfo);
	return ret;
}

int sceNpCommerce2DestroyGetCategoryContentsResult(SceNpCommerce2GetCategoryContentsResult *result)
{
	return 0;
}

int sceNpCommerce2DestroyGetProductInfoResult(SceNpCommerce2GetProductInfoResult *result)
{
	return 0;
}

int sceNpCommerce2DestroyGetProductInfoListResult(SceNpCommerce2GetProductInfoListResult *result)
{
	return 0;
}

int sceNpCommerce2GetContentRatingDescriptor(const SceNpCommerce2ContentRatingInfo *contentRatingInfo,
					     unsigned int idx,
					     SceNpCommerce2ContentRatingDescriptor *descriptor)
{
	return SceNpCommerce2_C4CC8E22((int)contentRatingInfo, (int)idx, (int)descriptor);
}
