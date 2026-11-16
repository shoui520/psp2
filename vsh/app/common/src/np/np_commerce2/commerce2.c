/*
 * SceNpCommerce2: the export layer. Every export checks that the library is initialized and
 * its arguments, then hands over to the request builders.
 */

#include "np_commerce2_internal.h"

/* set by Init and cleared by Term */
int g_initialized = 0;

/* the placeholder texts of the products the test environment answers with: filled with "Z",
 * the long one ends with "----END" */
char g_shortText[0x39];
char g_longText[0xFA1];

/* tells NpManager which store label and title the following requests are made for */
void SceNpCommerce2_FF13E2C3(const char *label, const char **titleId, int enable)
{
	const char *start;
	const char *end;
	SceSize length;
	const char *cur;
	char name[32];
	char commId[32];
	char c;

	if (label == NULL || titleId == NULL || enable == 0)
		return;

	start = *titleId;
	end = start;
	c = *end;
	for (;;) {
		cur = end;
		if (c == 0)
			break;
		if (c == '-') {
			c = cur[1];
			end++;
			if (c != 0) {
				do {
					if (c == '-')
						break;
					c = *++end;
				} while (c != 0);
			}
			break;
		}
		c = *++end;
	}
	length = end - start;
	if (length > 31)
		length = 31;
	sceClibMemcpy_safe(commId, start, length);
	commId[length] = '\0';

	sceClibMemset(name, 0, sizeof(name));
	sceClibStrlcpy(name, "NpCommerce2", sizeof(name));
	SceNpManager_9554802B(label, *titleId, name);
}

int SceNpCommerce2_25C58D33(void)
{
	return g_initialized;
}

int sceNpCommerce2CreateCtx(SceUInt32 version, SceUInt32 *ctxId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2CreateContext(version, ctxId, 0);
}

int SceNpCommerce2_AA090F5E(SceUInt32 version, SceUInt32 *ctxId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2CreateContext(version, ctxId, 1);
}

int SceNpCommerce2_B45A7935(void)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2DestroyAllContextsLocked();
}

int sceNpCommerce2DestroyCtx(SceUInt32 ctxId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2DestroyContext(ctxId);
}

int SceNpCommerce2_1EC16D19(SceUInt32 ctxId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2DestroyContext(ctxId);
}

int sceNpCommerce2DestroyReq(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2DestroyRequest(reqId);
}

int sceNpCommerce2AbortReq(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2AbortRequest(reqId);
}

int SceNpCommerce2_8F24F115(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != NULL && a3 != 0)
		return npCommerce2_8100D4EC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1B0B5798(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100D61A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2CreateSessionCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateSessionRequest(ctxId, reqId, NP_COMMERCE2_VERSION_1);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7DE103BB(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateSessionRequest(ctxId, reqId, NP_COMMERCE2_VERSION_2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DA5B6779(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateSessionRequest(ctxId, reqId, NP_COMMERCE2_VERSION_3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2CreateSessionStart(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2StartSessionRequest(reqId);
}

int SceNpCommerce2_00F16110(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2StartSessionRequest(reqId);
}

int SceNpCommerce2_6B743513(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2StartSessionRequest(reqId);
}

/* the result of a session request: the request kind decides the layout */
__attribute__((noinline)) static int getSessionResult(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize,
			    int version)
{
	int type;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (version > 0) {
		if (version > 1) {
			if (version > 2)
				return NP_COMMERCE2_ERROR_INVALID_VERSION;
			type = 11;
		} else {
			type = 2;
		}
	} else {
		if (version < 0)
			return NP_COMMERCE2_ERROR_INVALID_VERSION;
		type = 1;
	}
	return npCommerce2GetResult(type, reqId, buf, bufLen, fillSize);
}

int sceNpCommerce2CreateSessionGetResult(SceUInt32 reqId, void *buf, SceSize bufLen,
					 SceSize *fillSize)
{
	return getSessionResult(reqId, buf, bufLen, fillSize, NP_COMMERCE2_VERSION_1);
}

int SceNpCommerce2_6218343E(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	return getSessionResult(reqId, buf, bufLen, fillSize, NP_COMMERCE2_VERSION_2);
}

int SceNpCommerce2_3CA01B22(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	return getSessionResult(reqId, buf, bufLen, fillSize, NP_COMMERCE2_VERSION_3);
}

int SceNpCommerce2_1BFF9C73(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8100D6BC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_64E27331(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100D8E4(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_499732CA(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100D8EC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1A53A505(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C1542070(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810142C6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E2415631(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100D980(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C737A929(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_810060C0(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2DC2A0F9(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_810060CE(reqId);
}

int SceNpCommerce2_7E3C9196(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_81005E40(8, reqId, buf, bufLen, fillSize);
}

int sceNpCommerce2GetCategoryContentsCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_810062EC(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetCategoryContentsStart(SceUInt32 reqId, const char *categoryId,
					   SceUInt32 startPosition, SceUInt32 maxCountOfResults)
{
	const char *id = categoryId;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (id == NULL || maxCountOfResults > 60)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	SceNpCommerce2_FF13E2C3("NpCommerce2 GetCategoryContents", &id, 1);
	return npCommerce2_810062FA(reqId, id, 0, 0, startPosition, maxCountOfResults);
}

int SceNpCommerce2_1A9E8047(int a1, const char *a2, int a3, int a4, int a5, int a6)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != NULL)
		return npCommerce2_810062FA(a1, a2, a3, a4, a5, a6);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetCategoryContentsGetResult(SceUInt32 reqId, void *buf, SceSize bufLen,
					       SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (bufLen >= SCE_NP_COMMERCE2_RECV_BUF_SIZE)
		return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
	return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
}

int SceNpCommerce2_8B4BEA99(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int sceNpCommerce2InitGetCategoryContentsResult(SceNpCommerce2GetCategoryContentsResult *result,
						void *buf, SceSize bufLen)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result != NULL && buf != NULL && bufLen != 0)
		return npCommerce2_8100DAB0(result, buf, bufLen);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetCategoryInfo(const SceNpCommerce2GetCategoryContentsResult *result,
				  SceNpCommerce2CategoryInfo *categoryInfo)
{
	int ret;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result == NULL || categoryInfo == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = npCommerce2GetCategoryOfResult(result, categoryInfo);
	if (ret >= 0 && npCommerce2GetEnvironment() == 2) {
		categoryInfo->countOfSubCategory = 0;
		categoryInfo->countOfProduct = 0;
	}
	return ret;
}

int sceNpCommerce2GetContentInfo(const SceNpCommerce2GetCategoryContentsResult *result,
				 SceUInt32 index, SceNpCommerce2ContentInfo *contentInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result != NULL && contentInfo != NULL)
		return npCommerce2_8100DDCA(result, index, contentInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetCategoryInfoFromContentInfo(const SceNpCommerce2ContentInfo *contentInfo,
						 SceNpCommerce2CategoryInfo *categoryInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (contentInfo != NULL && categoryInfo != NULL)
		return npCommerce2GetCategoryOfContent(contentInfo, categoryInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F6A4FC50(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadItem0x28((const NpCommerce2Item *)a1, (NpCommerce2Item *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A66EF528(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadProductItem((const NpCommerce2Item *)a1, (NpCommerce2ProductItem *)a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EC09FF89(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2GetGameProductInfoOfItem((const NpCommerce2Item *)a1, (SceNpCommerce2GameProductInfo *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetContentRatingInfoFromCategoryInfo(const SceNpCommerce2CategoryInfo *categoryInfo,
						       SceNpCommerce2ContentRatingInfo *contentRatingInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (categoryInfo != NULL && contentRatingInfo != NULL)
		return npCommerce2GetContentRatingInfoOfItem((const NpCommerce2Item *)categoryInfo, contentRatingInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C4CC8E22(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadContentRatingDescriptor((const NpCommerce2Item *)a1, a2, (SceNpCommerce2ContentRatingDescriptor *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9B1AE84E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2GetContentRatingInfoAt((const NpCommerce2Item *)a1, a2, (SceNpCommerce2ContentRatingInfo *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_85C62297(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadSkuItem((const NpCommerce2Item *)a1, (NpCommerce2SkuItem *)a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2EAFF40C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x96((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem2 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7F6077C9(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadGrantItem0x95((const NpCommerce2Item *)a1, a2, (NpCommerce2GrantItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_006200AD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadValueItem0x9c((const NpCommerce2Item *)a1, a2, (NpCommerce2ValueItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F708CC37(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadGrantItem0x95((const NpCommerce2Item *)a1, a2, (NpCommerce2GrantItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1CCF4344(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadGrantItem0x95((const NpCommerce2Item *)a1, a2, (NpCommerce2GrantItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B1BB4660(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadGrantItem0x95((const NpCommerce2Item *)a1, a2, (NpCommerce2GrantItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A3A7C1FC(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadGrantItem0x95((const NpCommerce2Item *)a1, a2, (NpCommerce2GrantItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D0831EFE(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2447BED0(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x4b((const NpCommerce2Item *)a1, (NpCommerce2Detail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetProd(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoStart(SceUInt32 reqId, const char *categoryId,
				      const char *productId)
{
	const char *id = productId;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (id == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	SceNpCommerce2_FF13E2C3("NpCommerce2 GetProductInfo", &id, 1);
	return npCommerce2_8100647A(reqId, categoryId, id, 0, 0);
}

int SceNpCommerce2_00DEFA26(int a1, const char *a2, const char *a3, int a4, int a5)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a3 != NULL)
		return npCommerce2_8100647A(a1, a2, a3, a4, a5);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoGetResult(SceUInt32 reqId, void *buf, SceSize bufLen,
					  SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (bufLen >= SCE_NP_COMMERCE2_RECV_BUF_SIZE)
		return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
	return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
}

int SceNpCommerce2_AD4F85E1(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int sceNpCommerce2InitGetProductInfoResult(SceNpCommerce2GetProductInfoResult *result,
					   void *buf, SceSize bufLen)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result != NULL && buf != NULL && bufLen != 0)
		return npCommerce2InitPlainResult((NpCommerce2Item *)result, buf, bufLen);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9E082DB2(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadItem0x29((const NpCommerce2Item *)a1, (NpCommerce2Item *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7471D20F(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadProductItemLong((const NpCommerce2Item *)a1, (NpCommerce2ProductItemLong *)a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F988A255(const SceNpCommerce2GetProductInfoResult *result,
			    SceNpCommerce2GameProductInfo *gameProductInfo)
{
	int ret;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result == NULL || gameProductInfo == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = npCommerce2GetGameProductInfoOfProductResult(result, gameProductInfo);
	if (ret >= 0 && npCommerce2GetEnvironment() == 1) {
		gameProductInfo->productLongDescription = g_longText;
		gameProductInfo->legalDescription = g_longText;
	}
	return ret;
}

int SceNpCommerce2_A88EA64B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2GetContentRatingInfoAtOfProduct((const NpCommerce2Item *)a1, a2, (SceNpCommerce2ContentRatingInfo *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetContentRatingInfoFromGameProductInfo(
	const SceNpCommerce2GameProductInfo *gameProductInfo,
	SceNpCommerce2ContentRatingInfo *contentRatingInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (gameProductInfo != NULL && contentRatingInfo != NULL)
		return npCommerce2GetContentRatingInfoOfItem((const NpCommerce2Item *)gameProductInfo, contentRatingInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_71B86720(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2ReadSkuListItem((const NpCommerce2Item *)a1, a2, (NpCommerce2SkuListItem *)a3, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5730F4A4(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3d((const NpCommerce2Item *)a1, a2, (const char **)a3, (SceUInt32 *)a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D3BCDADD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_580C74F2(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BB8069E3(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_61634358(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x23((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A200696D(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x24((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem2 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E793945B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x25((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem3 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_677FE58A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x4b((const NpCommerce2Item *)a1, (NpCommerce2Detail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7377AEC9(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_81010B14(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DBC8B377(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadCountItem0x2f_81010BEC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8672B0EB(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadItem0x4e((const NpCommerce2Item *)a1, 0, 0, (NpCommerce2Item *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetGameSkuInfoFromGameProductInfo(const SceNpCommerce2GameProductInfo *gameProductInfo,
						    SceUInt32 idx,
						    SceNpCommerce2GameSkuInfo *gameSkuInfo)
{
	int ret;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (gameProductInfo == NULL || gameSkuInfo == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = npCommerce2GetGameSkuInfoOf(gameProductInfo, gameProductInfo->dataType, idx, gameSkuInfo);
	if (ret >= 0 && npCommerce2GetEnvironment() == 1) {
		gameSkuInfo->skuId = g_shortText;
		gameSkuInfo->price = 99999;
	}
	return ret;
}

int SceNpCommerce2_0985546A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqAddCart(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_868D6E88(int a1, int a2, int a3, int a4, int a5)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && (a5 == 0 || a5 == a3))
		return npCommerce2_810065DA(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B15F1702(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_30ABA9CA(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2InitResult0x2d((NpCommerce2Item *)a1, (const char *)a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FFED270E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadListItem0x2e((const NpCommerce2Item *)a1, a2, (NpCommerce2ListItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_54BCEFA6(const void *a1, SceNpCommerce2ContentRatingInfo *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2GetContentRatingInfoOfItem((const NpCommerce2Item *)a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_98E244BA(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadSkuDetail0x4c((const NpCommerce2Item *)a1, (NpCommerce2SkuDetail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A3A80BFA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_57C93D02(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadValueItem0x9c((const NpCommerce2Item *)a1, a2, (NpCommerce2ValueItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_093460F3(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadSkuDetailLong((const NpCommerce2Item *)a1, (NpCommerce2SkuDetailLong *)a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5541AF3C(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3d((const NpCommerce2Item *)a1, a2, (const char **)a3, (SceUInt32 *)a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3449647C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5EA4E81E(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x4b((const NpCommerce2Item *)a1, (NpCommerce2Detail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_94C1EA04(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D42FB74B(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2ReadOfferItem((const NpCommerce2Item *)a1, a2, (NpCommerce2OfferItem *)a3, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_ACABE7E1(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqRemCart(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7FC86589(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_8100673A(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9833E1F1(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_869EA46B(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2InitResult0x2d_8101150E((NpCommerce2Item *)a1, (const char *)a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_85C461FE(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_810115FC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_CCF71DBE(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadCountItem0x2f_81011604(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DF504EEF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqClearCart(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_71CCD6E9(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006880(reqId, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_4C30AF39(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetCart(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_974B42AE(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810069C0(reqId);
}

int SceNpCommerce2_84B9CB66(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_777ADB4A(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_2BCB23B2(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2InitResult0x2d_8101160C((NpCommerce2Item *)a1, (const char *)a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7C13533C(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_810116FA(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B523FE1A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadCountItem0x2f_81011702(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1AF7FC73(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqPurPrev(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4BA3CA8C(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006B0E(a1, a2, a3, a4);
}

int SceNpCommerce2_4322E924(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_247E42C6(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2InitResult0x2dNoStatus_8101170A((NpCommerce2Item *)a1, (const char *)a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C995829A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_810117FC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A1E8C739(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadCountItem0x2f_810118B0(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2B7C71D0(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810118A2(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C8844157(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqPurch(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3A050F80(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006B0E(a1, a2, a3, a4);
}

int SceNpCommerce2_5786F157(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_2ED2CB51(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810118B8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5F4626FA(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_810119A0(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FC459CE8(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadCountItem0x2f_810119A8(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_307290E8(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810119B0(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2FEC7ABC(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetCC(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2DE33C94(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006C64(reqId);
}

int SceNpCommerce2_DDB91745(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_076E2600(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810119BE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0FFC4396(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81011C24(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F8D3F362(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetProv(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C9C5EBF7(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006DA4(reqId);
}

int SceNpCommerce2_22640144(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_B70BF176(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81011C2C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C3B1179E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81011D84(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F682B145(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81011D8C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_94AB4446(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetPay(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_31782CA7(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006F00(a1, a2, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_8D4FDF78(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_62BBC725(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81011E42(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A18757A4(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81011FC8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetDrmContentInfoCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetDrm(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0D56D112(int a1, int a2, int a3, int a4, int a5)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_81007046(a1, a2, a3, a4, a5);
}

int SceNpCommerce2_838772EE(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_D44AAECA(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81011FD0(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D5E21C19(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101209C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1E26E2D1(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8101284E(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_29154ADF(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4A6B0527(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetBill(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FC8B048C(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810071EA(reqId);
}

int SceNpCommerce2_188B27D3(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_8B8465E2(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810128C8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E4DEA05E(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqRegCC(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_89171F84(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_984036CB(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetAddr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F9730784(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_81007470(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_97BE12D0(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_9FB0D052(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81012A5C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_95F62CC5(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetWal(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_ECCF1C47(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810075B6(reqId);
}

int SceNpCommerce2_05D6DBDB(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_3108F734(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81012B60(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2A9DB98C(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDateItem0x2c_81012BD6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FA3E492B(int a1, const int *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (a2[14] != 0)
		return npCommerce2Start_8100732A(a1, a2);
	return NP_COMMERCE2_ERROR_NO_TARGET;
}

int SceNpCommerce2_C0ECF440(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqTopupCC(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7D2A014E(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81007704(a1, a2);
}

int SceNpCommerce2_07E38F93(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_CF1FA4E5(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81012BDE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_37610D66(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81012CE8(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_72ABB589(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqTopEdy(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9CF6BDA3(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81007704(a1, a2);
}

int SceNpCommerce2_592C3D4A(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_B8F96BD2(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81012CF0(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_303DE914(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81012D66(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B5D6F4AF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqTopEdyRes(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3D49ABFA(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_142EAC1B(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVchrRedPrev(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_81DE35CA(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_9AF7E167(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_8100784A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A73CBF33(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0)
		return npCommerce2_810079A4(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_37D724FA(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81012E3C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2463E793(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81012F80(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0666D252(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013012(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_95C0850D(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101301A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2EBDD562(const void *a1, SceNpCommerce2ContentRatingInfo *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2GetContentRatingInfoOfItem((const NpCommerce2Item *)a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A793AF75(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2_81013362(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5354E2C3(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadValueItem0x9c((const NpCommerce2Item *)a1, a2, (NpCommerce2ValueItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_165CA716(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013432(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_17690AAD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_87D67D0B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x23((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_6308701B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x24((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem2 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9F845F18(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x25((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem3 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0A121766(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x4b((const NpCommerce2Item *)a1, (NpCommerce2Detail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0DDBF063(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3d((const NpCommerce2Item *)a1, a2, (const char **)a3, (SceUInt32 *)a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3C62D93E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_523F9F84(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2ReadOfferItem((const NpCommerce2Item *)a1, a2, (NpCommerce2OfferItem *)a3, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BDA95C6D(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVchrRed(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E2D8070E(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0)
		return npCommerce2_810079A4(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3FEEFAB4(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_BA03D443(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013952(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_6C65E2E2(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013A22(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4D3698A1(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013A2A(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_CAC11D21(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetResAddr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D7B43E47(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810071EA(reqId);
}

int SceNpCommerce2_9FA3DB36(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_027A054E(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013A32(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9B4811DF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqEventLog(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_96C6955D(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81007B04(a1, a2, a3, a4);
}

int SceNpCommerce2_D039082A(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_B98EE069(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSrchProd(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_98345A30(int a1, SceUInt32 a2, int a3, int a4, const SceUInt32 *a5, int a6,
			    SceUInt32 a7)
{
	int i;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == 0 || a2 >= 9)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (a6 != 1 || a7 > 7)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < 1; i++) {
		if (a5[0] >= 7 || a5[1] == 0 || a5[2] == 0)
			return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		a5 += 3;
	}
	return npCommerce2_81007C5A(a1, a2, a3, a4);
}

int SceNpCommerce2_771C582F(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_204066EA(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013BC6(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0390787C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81013C92(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_749C8571(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetCountries(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D09A38DA(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_81007DC4(reqId);
}

int SceNpCommerce2_45ACE466(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_86405E79(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013D24(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4BE534B6(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81013DAE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BDECB406(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81013E86(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_AE7334BA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81013ECE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9DB36783(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8101413A(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4C406B2A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqBindAccount(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C7B04A45(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0)
		return npCommerce2_81007EDA(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5D8B5B59(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013F58(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5AB115CE(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BFDC7B9F(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(6, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_A94C2A99(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810141C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5A1ACD90(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqStartReg(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2837D9E2(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81014250(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0DE0C8B1(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810142C6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_95C185B5(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetSecQ(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D3509F1B(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810080AE(reqId);
}

int SceNpCommerce2_8D8509B1(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_C28139BD(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810144A0(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1A37B2BA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810145CE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B7D151D1(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetCred(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C81DF030(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_F764DBA5(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetLid(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E8F39860(int a1, int a2, int a3, int a4, int a5, int a6)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0 && a6 != 0)
		return npCommerce2_81007FBE(a1, a2, a3, a4, a5, a6);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A6CC5E4A(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(5, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_92DCC788(int a1, int a2, int a3, int a4, int a5, int a6)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a5 != 0 && a6 != 0)
		return npCommerce2_810081EE(a1, a2, a3, a4, a5, a6);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7DB5D84A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_81008358(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0CA0B23F(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_F4DD3FD8(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqUpdPass(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2FC09E2B(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_9C1C168D(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifyAccountPass(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8E4CB649(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_8AA18E16(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifyOid(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_528BF506(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_81008606(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7B93B7A6(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_6E0F1699(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetOid(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BAB8A533(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_81008606(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E89CF937(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_27E08EFA(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetName(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A4C52416(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_8100849E(a1, a2, a3, SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_513940B4(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_8100849E(a1, a2, 0, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_68CF4B6B(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100874C(reqId);
}

int SceNpCommerce2_DDB75C18(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_53235A9F(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8101461E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3DF0BC8F(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81014694(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_693EAE89(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetName(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E6BB6285(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_4E4A2898(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetResAddr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E16A3E4F(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_65743DF2(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetRrn(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E13EBC17(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0)
		return npCommerce2Start_81008B50(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_39E7EB72(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_AC9457D4(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetOptIn(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8EDA59C2(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81008C9C(reqId);
}

int SceNpCommerce2_BD0555F8(int a1, int a2, int a3, int a4, int a5)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0)
		return npCommerce2_8100888C(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D4D3D6D2(int a1, const int *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (a2[14] != 0)
		return npCommerce2Start_81008A0A(a1, a2);
	return NP_COMMERCE2_ERROR_NO_TARGET;
}

int SceNpCommerce2_9E1F6D58(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_F762E758(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8101478E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_447274CD(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81014804(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1729CBC6(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetOptIn(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_81C509DA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81008DDC(a1, a2, a3);
}

int SceNpCommerce2_BC05DE41(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_DACD1180(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqAuthMaster(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EB8775F5(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0)
		return npCommerce2Start_81008F28(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2D3073AF(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_EA4A4CAE(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetBillInfo(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_486D6DE3(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810148A2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_17DF6D62(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81014A92(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5E7B03D8(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81014A9A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_85587466(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_1CC9B661(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006F00(a1, 0, a3);
}

int SceNpCommerce2_AEB340A6(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81014AA2(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5402711A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetSubCtrl(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_10B76A6F(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2_81009074(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_6632CE20(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_9A24EB17(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqResetSubPass(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4FB61A81(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0)
		return npCommerce2_810091DE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_92DF40CC(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_2EBD25FF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetWalOpt(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1BA1F399(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810092EE(reqId);
}

int SceNpCommerce2_6A717339(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_42A33732(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81014AAA(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C9183FC5(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81014B20(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_48171860(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetWalOpt(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_CD382E48(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100942E(a1, a2, a3);
}

int SceNpCommerce2_CD123380(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_E0B222F3(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSubmitReg(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_527C3D90(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100957A(reqId);
}

int SceNpCommerce2_2AA6C91E(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_05002E87(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81014BCE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E829D968(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B75D54D3(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqAuthenticate(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_115DF6B1(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81014C44(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B37493FA(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C46EB548(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810142C6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_AED6006D(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadEntry0x16_81014D62(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5EE39E3F(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81014D6A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_26602DA1(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81014EB4(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FE0EB375(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 1 || (a3 != 0 && a4 != 0))
		return npCommerce2_810096BA(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4A36892A(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(4, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_2AF9CC74(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqRemPay(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B38AB217(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006880(reqId, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_C07A3248(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_A9DFEF68(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetTranHist(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1DE3D522(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_A5ADC2A6(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81014F08(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B882FC31(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81014F92(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F1E443C9(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetTranDetail(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8BB68957(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100999A(reqId, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_3EFF3363(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_E9C58817(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810151B8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_43B04705(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8101522E(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E3E40B5E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101545C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4ABFA125(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81015606(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_6ABA5313(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81015818(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C3F5383C(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if ((a2 != 1 || a3 != 0) && a4 != 0 && a5 != 0 && a6 != 0 && a7 != 0 && a8 != 0 && a9 != 0)
		return npCommerce2_810097E0(a1, a2, a3, a4, a5, a6);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_03B6777B(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81015818(a1, a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4A315A31(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x4b((const NpCommerce2Item *)a1, (NpCommerce2Detail *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_398B0940(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x91((const NpCommerce2Item *)a1, (NpCommerce2Detail0x91 *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_ED1A7FD1(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetSrvs(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_25A32361(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81009AE2(a1, a2, a3);
}

int SceNpCommerce2_6C0B2508(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_91979E62(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8101591C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_50F7E2CD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810159E8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_42DB888A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetSrvDetail(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5DEC2B36(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_81009C2E(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F9DE9564(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_A8459628(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81015BCE(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_57A54D3C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81015C9A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_12A3F4BA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_6F3A7FEF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetSubscr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D9A80442(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_81009D84(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_03A27E16(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_4DAE27BE(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81015EB4(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D8E5E2DA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81015F80(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E46F89EA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810162BA(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_429EA2AF(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0BF3C1C4(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqChangeSubscr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B7434181(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_81009EDA(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F9C7F5F5(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_907BC19E(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqCancelSubscr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0E7B2BCD(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_8100A020(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A609C808(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_FB35C43D(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetCnslAct(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7B4888F0(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100A166(reqId);
}

int SceNpCommerce2_86E0D693(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_1599647E(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810163AC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E77A9E8B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81016436(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_76DA0D80(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqAcptEula(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F40D2B1E(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_E1C27906(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifyDob(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F0E90E02(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8101652E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DFC84F4F(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810165A4(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DB8BA72E(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifySecAns(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4121F2A1(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_E53BD365(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_810165F0(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_791774B7(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F50D7BE4(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_8100AECA(a1, a2);
	return npCommerce2Start_81006880(a1, 0);
}

int SceNpCommerce2_1B229032(int a1, int a2, int a3, int a4, int a5)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0)
		return npCommerce2_8100A2B4(a1, a2, a3, a4, a5);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2A22BEAF(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(7, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_922382C1(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a3 != 0 && a4 != 0)
		return npCommerce2_8100A38E(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9FEE1E0E(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810142C6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4615377F(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSendPassResetMail(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_536DA1DA(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100A83A(reqId, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_2AD3766D(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_4C5EA07A(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifyMasterBill(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4719EB55(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_07C16199(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetEpw(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D35F548B(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100A602(reqId);
}

int SceNpCommerce2_B7387C96(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_5953F48D(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81016666(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9FD5705B(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSendEmailVer(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B7552026(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_8100B182(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4D00AA32(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqCrGuestSess(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8B99ED11(int a1, const int *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (a2[14] != 0)
		return npCommerce2Start_8100A4B6(a1, a2, a3);
	return NP_COMMERCE2_ERROR_NO_TARGET;
}

int SceNpCommerce2_FFFC0101(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(12, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_901E6455(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqResetPass(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D494B4CE(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_577F8BF3(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetMasterBill(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_94AA1BFB(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_23C047F1(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_8101670C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0FA01B8E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810168FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_16FE6A6E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81016904(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4B49C464(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8101690C(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4D7B5ACE(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSendConsentMail(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_917646A1(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_8100A83A(reqId, SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED);
}

int SceNpCommerce2_C3765C32(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_34194245(int a1, int a2, int a3, int a4, int a5, int a6)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0 && a6 != 0)
		return npCommerce2_8100A742(a1, a2, a3, a4, a5, a6);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4329A980(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResult(3, reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_08E8D89D(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0)
		return npCommerce2Start_8100849E(a1, a2, 0, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D160F700(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_81006F00(a1, 0, a3);
}

int SceNpCommerce2_0C33B954(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetMasterResAddr(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B9BC6885(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2Start_810071EA(reqId);
}

int SceNpCommerce2_A222BBDC(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_BFC26DA2(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81013A32(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoListCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetProdList(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_797BC431(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_DAE82F31(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810169A8(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3625C8E7(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0507F960(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoListGetResult(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (bufLen >= SCE_NP_COMMERCE2_RECV_BUF_SIZE)
		return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
	return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
}

int sceNpCommerce2GetProductInfoListStart(SceUInt32 reqId, const char *productIds[],
					  SceUInt32 productNum)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (productIds == NULL || productNum == 0 || productNum > SCE_NP_COMMERCE2_GETPRODLIST_MAX_COUNT)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	SceNpCommerce2_FF13E2C3("NpCommerce2 GetProductInfoList", productIds, productNum);
	return npCommerce2Start_8100A982(reqId, productIds, productNum);
}

int SceNpCommerce2_B27F7BA1(int a1, const char **a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != NULL && a3 != 0)
		return npCommerce2Start_8100A982(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2InitGetProductInfoListResult(SceNpCommerce2GetProductInfoListResult *result,
					       void *buf, SceSize bufLen)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (result != NULL && buf != NULL && bufLen != 0)
		return npCommerce2_81016914(result, buf, bufLen);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetGameProductInfoFromGetProductInfoListResult(
	const SceNpCommerce2GetProductInfoListResult *result, SceUInt32 idx,
	SceNpCommerce2GameProductInfo *gameProductInfo)
{
	char entry[96];
	int ret;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;

	ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (result != NULL)
		ret = npCommerce2_810169A8((int)result, idx, entry);
	if (ret >= 0) {
		if (g_initialized != 0) {
			ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
			if (gameProductInfo != NULL)
				ret = npCommerce2GetGameProductInfoOfItem((const NpCommerce2Item *)entry, gameProductInfo);
		} else {
			ret = SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
		}
	}
	return ret;
}

int SceNpCommerce2_7FECE927(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8A804C00(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRewardItem0x80((const NpCommerce2Item *)a1, a2, (NpCommerce2RewardItem *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_935C25D1(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x91((const NpCommerce2Item *)a1, (NpCommerce2Detail0x91 *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D7ED457F(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9A095860(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadSkuDetailLong((const NpCommerce2Item *)a1, (NpCommerce2SkuDetailLong *)a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_AC59EBB3(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013432(a1, a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_838F1DAB(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x23((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2C3EF0F5(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x24((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem2 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_938D7819(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x25((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem3 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3E8B077C(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x91((const NpCommerce2Item *)a1, (NpCommerce2Detail0x91 *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E5E79E63(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x3e((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EA3FF0FC(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x11((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C51EA536(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2ReadOfferItem((const NpCommerce2Item *)a1, a2, (NpCommerce2OfferItem *)a3, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EFEA25A3(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadSkuItem((const NpCommerce2Item *)a1, (NpCommerce2SkuItem *)a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F520899E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2GetContentRatingInfoAt((const NpCommerce2Item *)a1, a2, (SceNpCommerce2ContentRatingInfo *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A62BBCE3(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x91((const NpCommerce2Item *)a1, (NpCommerce2Detail0x91 *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_58BFD97B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x11((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2EB4B433(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2GetContentRatingInfoAtOfProduct((const NpCommerce2Item *)a1, a2, (SceNpCommerce2ContentRatingInfo *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1D9934A3(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2ReadSkuListItem((const NpCommerce2Item *)a1, a2, (NpCommerce2SkuListItem *)a3, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_0D9666D8(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x23((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_42799B8B(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2ReadDetailOf0x91((const NpCommerce2Item *)a1, (NpCommerce2Detail0x91 *)a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7575795A(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x24((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem2 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_60137ABB(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadTextItem0x25((const NpCommerce2Item *)a1, a2, (NpCommerce2TextItem3 *)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D0B44018(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2ReadRecord0x11((const NpCommerce2Item *)a1, a2, (const char **)a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_22490B90(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadProductItem((const NpCommerce2Item *)a1, (NpCommerce2ProductItem *)a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_CAA91CCE(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2ReadProductItemLong((const NpCommerce2Item *)a1, (NpCommerce2ProductItemLong *)a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_947A58B6(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8101284E(a1, a2, 3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2CCC62DD(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqRecomProds(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4ABA502B(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_4075B119(int a1, int a2, int a3, int a4, SceUInt32 a5, SceUInt32 a6)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a6 > 1) {
		if (a6 <= 2) {
			if (a5 > 60)
				return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
			return npCommerce2_8100AACE(a1, a2, a3, a4, a5, a6);
		}
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	}
	if (a6 == 0)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	if (a2 == 0 || a3 == 0 || a5 > 60)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	return npCommerce2_8100AACE(a1, a2, a3, a4, a5, a6);
}

int SceNpCommerce2_39AE7F83(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81016A3A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_28B9A03A(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81013C92(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F88D6E68(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqVerifyBill(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9F34247B(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_8100AC38(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_94B068D1(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_50E50CFB(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81016B50(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EC0BD12F(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81013FEE(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FDB807AC(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqGetProdIds(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2E9BE5D6(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_AA0D88AE(int a1, void *a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL && a3 != 0)
		return npCommerce2_81016BC6(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_58587046(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81016C50(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E324B61B(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqSetAccountLang(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_AFD1BEE5(int ctxId, int reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_8100AECA(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_12467D66(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_397F839E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if ((a2 | a3) != 0)
		return npCommerce2Start_8100AD7E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2511F962(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqCancelOrder(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7A8CA2C5(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_8100B010(reqId);
}

int SceNpCommerce2_4C69865E(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_24257016(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2CreateReqCheckAccountEula(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_518197E0(int ctxId, int reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2Start_8100AECA(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4C1B2B7B(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
}

int SceNpCommerce2_2C6028A0(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_8100B238(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_71ADF5A6(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_8100B3C6(a1, a2);
}

int SceNpCommerce2_2F0645A6(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a3 != 0 && a4 != 0)
		return npCommerce2_8100B292(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8ECAE666(int a1, SceUInt32 a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 != 0 && a2 < 9)
		return npCommerce2_8100B372(a1, a2);
	return NP_COMMERCE2_ERROR_INVALID_TARGET_TYPE;
}

int SceNpCommerce2_60187EF1(int a1, const char *a2, int *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == NULL || a3 == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	*a3 = 0;
	return npCommerce2_8100B41A(a1, a2);
}

/* the SKU ID's text ends at its third part: cuts off what follows the second '-' */
int SceNpCommerce2_884BF45A(char *skuId)
{
	char *p;
	int ret = 0;
	SceUInt32 i;

	if (skuId == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_SKUID;
	for (i = 0; i < 2; i++) {
		p = sceClibStrchr(skuId, '-');
		if (p == NULL)
			return SCE_NP_COMMERCE2_ERROR_INVALID_SKUID;
		skuId = p + 1;
	}
	p = sceClibStrchr(skuId, '-');
	if (p != NULL)
		*p = '\0';
	return ret;
}

/* copies the SKU ID's first two parts, up to the next '-' or the end, to buf */
int SceNpCommerce2_9FBF9536(char *skuId, void *buf, SceSize bufLen)
{
	const char *start = skuId;
	char *p;
	int ret = 0;
	SceUInt32 i;
	SceSize length;

	if (skuId == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_SKUID;
	for (i = 0; i < 2; i++) {
		p = sceClibStrchr(skuId, '-');
		if (p == NULL)
			return SCE_NP_COMMERCE2_ERROR_INVALID_SKUID;
		skuId = p + 1;
	}
	p = sceClibStrchr(skuId, '-');
	if (p == NULL)
		p = (char *)start + sceClibStrnlen(start, SCE_NP_COMMERCE2_SKU_ID_LEN);
	length = p - start;
	if (length >= bufLen)
		return NP_COMMERCE2_ERROR_BUF_TOO_SMALL;
	sceClibMemcpy_safe(buf, start, length);
	((char *)buf)[length] = '\0';
	return ret;
}

/* the length of the SKU ID's first two parts, with the '-' between */
int SceNpCommerce2_87A71049(const char *skuId)
{
	const char *start = skuId;
	const char *p;
	SceUInt32 i;

	if (skuId == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < 2; i++) {
		p = sceClibStrchr(skuId, '-');
		if (p == NULL)
			return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		skuId = p + 1;
	}
	return skuId - start - 1;
}

/* the UTF-16 text cut down to ASCII; *flags gets 0x80 when a character did not fit 7 bits */
int SceNpCommerce2_34CDA223(int a1, const SceUInt16 *a2, int *flags)
{
	char text[256];
	char *dst;
	const SceUInt16 *src;
	int bits;
	SceUInt32 c;
	SceUInt32 n;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a2 == NULL || flags == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	bits = 0;
	c = *a2;
	dst = text;
	*flags = 0;
	n = 0;
	src = a2;
	for (;;) {
		if (c == 0)
			break;
		if (n >= 0x100)
			break;
		if (c > 0x7F) {
			bits |= 0x80;
			*flags = bits;
		}
		*dst++ = c;
		c = src[1];
		src++;
		n++;
	}
	if (n == 0x100)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	text[n] = '\0';
	return npCommerce2_8100B41A(a1, text);
}

/* the title ID's store label of the content's category (1 to 5), its SKU ID's prefix */
int SceNpCommerce2_9B444B7C(int a1, int a2, const int *a3, char *dst, SceSize dstSize)
{
	const char *label = NULL;
	int ret;
	SceSize length;

	if (a1 == 0 || a2 == 0 || a3 == 0 || dst == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = 0;
	switch (a3[34]) {
	case 1:
		label = "IP9101-NPIA90005";
		break;
	case 2:
		label = "IP9102-NPIA90006";
		break;
	case 3:
		label = "IP9103-NPIA90007";
		break;
	case 4:
		label = "IP9104-NPIA90008";
		break;
	case 5:
		label = "IP9105-NPIA90009";
		break;
	default:
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		break;
	}
	if (ret >= 0) {
		length = sceClibStrnlen(label, SCE_NP_COMMERCE2_SKU_ID_LEN);
		if (dstSize > sceClibStrnlen("_01--STORE", SCE_NP_COMMERCE2_SKU_ID_LEN) + length)
			sceClibSnprintf(dst, dstSize, "%s%s", label, "_01--STORE");
		else
			return NP_COMMERCE2_ERROR_BUF_TOO_SMALL;
	}
	return ret;
}

/* tells whether the ID is a content of the "IP910n-NPIA9000x_" form (n 1-5, x 5-9) */
int SceNpCommerce2_BC44F7F9(const char *id, int *isStoreContent)
{
	const char *p;
	const char *q;
	SceSize length;
	int c;
	int ret = 0;

	if (id == NULL || isStoreContent == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	length = sceClibStrnlen(id, SCE_NP_COMMERCE2_SKU_ID_LEN);
	if (length >= sceClibStrnlen("-NPIA9000", SCE_NP_COMMERCE2_SKU_ID_LEN) +
	    sceClibStrnlen("IP910", SCE_NP_COMMERCE2_SKU_ID_LEN) + 4) {
		if (sceClibStrncmp("IP910", id, sceClibStrnlen("IP910", SCE_NP_COMMERCE2_SKU_ID_LEN)) != 0) {
			*isStoreContent = 0;
		} else {
			p = id + sceClibStrnlen("IP910", SCE_NP_COMMERCE2_SKU_ID_LEN);
			c = *p;
			if (c >= '1' && c <= '5') {
				q = p + 1;
				if (sceClibStrncmp("-NPIA9000", q,
						   sceClibStrnlen("-NPIA9000", SCE_NP_COMMERCE2_SKU_ID_LEN)) != 0) {
					*isStoreContent = 0;
				} else {
					length = sceClibStrnlen("-NPIA9000", SCE_NP_COMMERCE2_SKU_ID_LEN);
					c = p[length + 1];
					q += length;
					*isStoreContent = c >= '5' && c <= '9' && q[1] == '_';
				}
			} else {
				*isStoreContent = 0;
			}
		}
	} else {
		*isStoreContent = 0;
	}
	return ret;
}

/* tells whether the ID is a content of the "IP9100-NPIA0900n_00-CC" form (n 2-6) */
int SceNpCommerce2_043F86FF(const char *id, int *isStoreContent)
{
	const char *p;
	SceSize length;
	int c;
	int ret = 0;

	if (id == NULL || isStoreContent == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	length = sceClibStrnlen(id, SCE_NP_COMMERCE2_SKU_ID_LEN);
	if (length >= sceClibStrnlen("_00-CC", SCE_NP_COMMERCE2_SKU_ID_LEN) +
	    sceClibStrnlen("IP9100-NPIA0900", SCE_NP_COMMERCE2_SKU_ID_LEN) + 1) {
		if (sceClibStrncmp("IP9100-NPIA0900", id,
				   sceClibStrnlen("IP9100-NPIA0900", SCE_NP_COMMERCE2_SKU_ID_LEN)) != 0) {
			*isStoreContent = 0;
		} else {
			p = id + sceClibStrnlen("IP9100-NPIA0900", SCE_NP_COMMERCE2_SKU_ID_LEN);
			c = *p;
			if (c >= '2' && c <= '6') {
				p++;
				if (sceClibStrncmp("_00-CC", p,
						   sceClibStrnlen("_00-CC", SCE_NP_COMMERCE2_SKU_ID_LEN)) != 0) {
					*isStoreContent = 0;
				} else {
					sceClibStrnlen("_00-CC", SCE_NP_COMMERCE2_SKU_ID_LEN);
					*isStoreContent = 1;
				}
			} else {
				*isStoreContent = 0;
			}
		}
	} else {
		*isStoreContent = 0;
	}
	return ret;
}

int sceNpCommerce2GetShortfallOfLibsslPool(SceUInt32 ctxId, SceInt32 *shortfall)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (shortfall != NULL)
		return npCommerce2_8100B7A0(ctxId, shortfall);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetShortfallOfLibhttpPool(SceUInt32 ctxId, SceInt32 *shortfall)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (shortfall != NULL)
		return npCommerce2_8100B7F4(ctxId, shortfall);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

/* the library's start: the shell events and the object table, and in the test
 * environment the placeholder texts the products carry */
int npCommerce2Initialize(const char *environment, int npCommonValue, int environmentType)
{
	int ret;

	if (g_initialized != 0)
		return SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;

	ret = npCommerce2_81017DCC();
	if (ret >= 0) {
		ret = npCommerce2InitTable(environment, npCommonValue, environmentType);
		if (ret >= 0) {
			if (npCommerce2GetEnvironment() == 1) {
				sceClibMemset(g_shortText, 'Z', 0x39);
				g_shortText[0x38] = '\0';
				sceClibMemset(g_longText, 'Z', 0xFA1);
				sceClibMemcpy_safe(&g_longText[0xF99], "----END", 8);
			}
			g_initialized = 1;
			return 0;
		}
	}
	return ret;
}

int npCommerce2Finalize(void)
{
	int ret;

	if (g_initialized != 0) {
		ret = npCommerce2TermTable();
		npCommerce2_81017EF8(ret);
		g_initialized = 0;
	}
	return 0;
}
