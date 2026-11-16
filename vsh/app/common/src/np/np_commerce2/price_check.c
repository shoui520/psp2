/*
 * SceNpCommerce2: the library's own test flow (signing up a test account and buying with it, on the
 * networks that are not the production one), and the request it keeps to be able to abort it.
 */

#include "np_commerce2_internal.h"

SceUInt32 g_priceReqId = 0;		/* 0x81026004: the request the flow waits for */
int g_priceAborted = 0;			/* 0x81026008 */

/* aborts the flow: the request it waits for, then the flow itself at its next step */
int SceNpCommerce2_785095D9(void)
{
	if (g_priceReqId != 0)
		sceNpCommerce2AbortReq(g_priceReqId);
	g_priceAborted = 1;
	return 0;
}

/* the flow calls the library's own exports, with the arguments the exports take */
extern int SceNpCommerce2_B75D54D3();
extern int SceNpCommerce2_FE0EB375();
extern int SceNpCommerce2_4A36892A();
extern int SceNpCommerce2_115DF6B1();
extern int SceNpCommerce2_AED6006D();

/* a result the flow reads, and one of its items (the last word of an item is the value the flow wants) */
typedef struct NpCommerce2FlowResult {
	SceUInt32 words[28];
} NpCommerce2FlowResult;

typedef struct NpCommerce2FlowItem {
	SceUInt32 words[28];
} NpCommerce2FlowItem;

/* asks the store for one thing of the account and gives back the value of the first item the answer lists */
int npCommerce2FlowCheck(SceUInt32 ctxId, int a2, int a3, int a4, SceUInt32 *value)
{
	NpCommerce2FlowResult result;
	NpCommerce2FlowItem item;
	char buf[2048];
	int size;
	int ret;

	ret = SceNpCommerce2_B75D54D3(ctxId, &g_priceReqId);
	if (ret >= 0) {
		ret = SceNpCommerce2_FE0EB375(g_priceReqId, a2, a3, a4);
		if (ret >= 0) {
			ret = SceNpCommerce2_4A36892A(g_priceReqId, buf, 2048, &size);
			if (ret >= 0 && value != NULL) {
				ret = SceNpCommerce2_115DF6B1(&result, buf, size);
				if (ret >= 0) {
					ret = SceNpCommerce2_AED6006D(&result, 0, &item);
					if (ret >= 0)
						*value = item.words[27];
				}
			}
		}
	}
	sceNpCommerce2DestroyReq(g_priceReqId);
	g_priceReqId = 0;
	return ret;
}

extern void SceNpCommerce2_FF13E2C3(const char *label, const char **titleId, int enable);
extern int SceNpCommerce2_00F16110();
extern int SceNpCommerce2_6218343E();
extern int SceNpCommerce2_7DE103BB();
extern int SceNpCommerce2_8ECAE666();
extern int SceNpCommerce2_AA090F5E();
extern int SceNpCommerce2_1A9E8047();
extern int SceNpCommerce2_8B4BEA99();
extern int SceNpCommerce2_00DEFA26();
extern int SceNpCommerce2_AD4F85E1();
extern int SceNpCommerce2_9E082DB2();
extern int SceNpCommerce2_F988A255();
extern int SceNpCommerce2_C737A929();
extern int SceNpCommerce2_2DC2A0F9();
extern int SceNpCommerce2_7E3C9196();
extern int SceNpCommerce2_1EC16D19();

/* a result's data with the count of its items behind it */
typedef struct NpCommerce2FlowCount {
	char data[100];
	int count;
} NpCommerce2FlowCount;

/* the empty store check: asks the store whether a category (kind 1) or a product (kind 2) has content */
int sceNpCommerce2StartEmptyStoreCheck(int kind, const char *id, void *buf, SceSize bufSize, int *empty)
{
	const char *idList = id;
	char productResult[124];
	char productItem[96];
	NpCommerce2FlowCount categoryResult;
	NpCommerce2FlowCount product;
	char dummy[4];
	char unused[36];
	SceSize fill;
	int ret;
	int r;
	int mode;
	int started = 0;
	int zero1 = 0;
	int zero2 = 0;
	SceUInt32 ctxId;

	g_checkAborted = 0;
	if (kind <= 0 || kind > 2) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto cleanup;
	}
	if (id == NULL || buf == NULL || bufSize == 0 || empty == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto cleanup;
	}
	sceClibMemset(unused, 0, 0x24);
	SceNpCommerce2_FF13E2C3("NpCommerce2 EmptyStoreCheck", &idList, 1);
	ret = SceNpCommerce2_AA090F5E(2, &g_checkCtxId);
	if (ret < 0)
		goto cleanup;
	ret = SceNpCommerce2_8ECAE666(g_checkCtxId, 3);
	if (ret < 0)
		goto cleanup;
	r = SceNpCommerce2_7DE103BB(g_checkCtxId, &g_checkReqId);
	if (r >= 0) {
		ret = SceNpCommerce2_00F16110(g_checkReqId);
		if (ret >= 0)
			ret = SceNpCommerce2_6218343E(g_checkReqId, buf, bufSize, &fill);
		sceNpCommerce2DestroyReq(g_checkReqId);
		g_checkReqId = 0;
	} else {
		ret = r;
	}
	if (ret < 0)
		goto cleanup;
	if (g_checkAborted == 1) {
		ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		started = 1;
		goto cleanup;
	}
	if (kind > 1) {
		r = sceNpCommerce2GetProductInfoCreateReq(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			ret = SceNpCommerce2_00DEFA26(g_checkReqId, 0, idList, 0, 0);
			if (ret >= 0)
				ret = SceNpCommerce2_AD4F85E1(g_checkReqId, buf, bufSize, &fill);
			sceNpCommerce2DestroyReq(g_checkReqId);
			g_checkReqId = 0;
		} else {
			ret = r;
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		ret = sceNpCommerce2InitGetProductInfoResult((SceNpCommerce2GetProductInfoResult *)productResult, buf, fill);
		if (ret >= 0) {
			ret = SceNpCommerce2_9E082DB2((int)productResult, (int)productItem);
			if (ret >= 0)
				ret = SceNpCommerce2_F988A255((const SceNpCommerce2GetProductInfoResult *)productItem, &product);
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		mode = npCommerce2GetEnvironment();
		if (mode > 1) {
			if (mode <= 2)
				*empty = 1;
		} else if (mode >= 0) {
			if (product.count != 0)
				*empty = zero1;
			else
				*empty = 1;
		}
	} else {
		r = sceNpCommerce2GetCategoryContentsCreateReq(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			ret = SceNpCommerce2_1A9E8047(g_checkReqId, idList, 0, 0, 0, 1);
			if (ret >= 0)
				ret = SceNpCommerce2_8B4BEA99(g_checkReqId, buf, bufSize, &fill);
			sceNpCommerce2DestroyReq(g_checkReqId);
			g_checkReqId = 0;
		} else {
			ret = r;
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		ret = sceNpCommerce2InitGetCategoryContentsResult((SceNpCommerce2GetCategoryContentsResult *)&categoryResult, buf, fill);
		started = 1;
		if (ret < 0)
			goto cleanup;
		mode = npCommerce2GetEnvironment();
		if (mode > 1) {
			if (mode <= 2)
				*empty = 1;
		} else if (mode >= 0) {
			if (categoryResult.count != 0)
				*empty = zero1;
			else
				*empty = 1;
		}
	}
	ret = zero2;
	started = 1;
cleanup:
	ctxId = g_checkCtxId;
	if (started == 1 && g_checkAborted == 0) {
		r = SceNpCommerce2_C737A929(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			if (SceNpCommerce2_2DC2A0F9(g_checkReqId) >= 0)
				SceNpCommerce2_7E3C9196(g_checkReqId, buf, bufSize, dummy);
		}
		sceNpCommerce2DestroyReq(g_checkReqId);
		g_checkReqId = zero2;
		ctxId = g_checkCtxId;
	}
	if (ctxId != 0)
		SceNpCommerce2_1EC16D19(ctxId);
	return ret;
}
