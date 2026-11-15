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
	return npCommerce2UnlockTable();
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

	ret = npCommerce2_8100DD78(result, categoryInfo);
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
		return npCommerce2_8100DE5C(contentInfo, categoryInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F6A4FC50(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100DEB4(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A66EF528(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2_8100E272(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_EC09FF89(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100E5FA(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetContentRatingInfoFromCategoryInfo(const SceNpCommerce2CategoryInfo *categoryInfo,
						       SceNpCommerce2ContentRatingInfo *contentRatingInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (categoryInfo != NULL && contentRatingInfo != NULL)
		return npCommerce2_8100E6F4(categoryInfo, contentRatingInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C4CC8E22(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100E746(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9B1AE84E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100E868(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_85C62297(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2_8100ED8C(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2EAFF40C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101059C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7F6077C9(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810102FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_006200AD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81010620(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_F708CC37(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810102FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1CCF4344(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810102FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B1BB4660(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810102FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A3A7C1FC(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810102FC(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D0831EFE(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810100C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_2447BED0(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100F1D6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetProductInfoCreateReq(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_8100646C(ctxId, reqId);
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
		return npCommerce2_8100F316(result, buf, bufLen);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_9E082DB2(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100F38C(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7471D20F(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2_8100F416(a1, a2, 2);
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

	ret = npCommerce2_8100F740(result, gameProductInfo);
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
		return npCommerce2_8100F798(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int sceNpCommerce2GetContentRatingInfoFromGameProductInfo(
	const SceNpCommerce2GameProductInfo *gameProductInfo,
	SceNpCommerce2ContentRatingInfo *contentRatingInfo)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (gameProductInfo != NULL && contentRatingInfo != NULL)
		return npCommerce2_8100E6F4(gameProductInfo, contentRatingInfo);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_71B86720(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2_8100F8D8(a1, a2, a3, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5730F4A4(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101144E(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D3BCDADD(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810114C6(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_580C74F2(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810100C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_BB8069E3(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810100C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_61634358(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100FE34(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A200696D(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100FE7C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_E793945B(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8100FF6E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_677FE58A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100F1D6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7377AEC9(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81010B14(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DBC8B377(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81010BEC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_8672B0EB(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81010DD8(a1, 0, 0, a2);
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

	ret = npCommerce2_8100FDBC(gameProductInfo, gameProductInfo->dataType, idx, gameSkuInfo);
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
		return npCommerce2_810065CC(ctxId, reqId);
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
		return npCommerce2_81010938(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_FFED270E(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81010C98(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_54BCEFA6(const void *a1, SceNpCommerce2ContentRatingInfo *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100E6F4(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_98E244BA(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81010FFA(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A3A80BFA(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810100C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_57C93D02(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_81010620(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_093460F3(int a1, void *a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != NULL)
		return npCommerce2_8101117E(a1, a2, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5541AF3C(int a1, int a2, int a3, int a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_8101144E(a1, a2, a3, a4);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_3449647C(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810114C6(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_5EA4E81E(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_8100F1D6(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_94C1EA04(int a1, int a2, int a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != 0)
		return npCommerce2_810100C2(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_D42FB74B(int a1, int a2, void *a3)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a3 != NULL)
		return npCommerce2_81010710(a1, a2, a3, 2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_ACABE7E1(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_8100672C(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7FC86589(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_8100673A(ctxId, reqId);
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
		return npCommerce2_8101150E(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_85C461FE(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810115FC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_CCF71DBE(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81011604(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_DF504EEF(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_81006872(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_71CCD6E9(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_81006880(reqId);
}

int SceNpCommerce2_4C30AF39(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_810069B2(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_974B42AE(SceUInt32 reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_810069C0(reqId);
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
		return npCommerce2_8101160C(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_7C13533C(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810116FA(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_B523FE1A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_81011702(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_1AF7FC73(SceUInt32 ctxId, SceUInt32 *reqId)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (reqId != NULL)
		return npCommerce2_81006AF2(ctxId, reqId);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_4BA3CA8C(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2_81006B0E(a1, a2, a3, a4);
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
		return npCommerce2_8101170A(a1, a2, a3);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_C995829A(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810117FC(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}

int SceNpCommerce2_A1E8C739(int a1, int a2)
{
	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (a1 != 0 && a2 != 0)
		return npCommerce2_810118B0(a1, a2);
	return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
}
