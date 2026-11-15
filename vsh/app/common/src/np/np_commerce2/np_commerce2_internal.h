/*
 * SceNpCommerce2: the NP commerce client, a library of the PlayStation Store's web API.
 * Sony built it with SNC 3.57. No debug record names its files or functions: the exports'
 * names are the SDK's (np/np_commerce2.h) where it has one, the rest are the NID's; the
 * internal names are ours, taken from what the code does.
 */
#ifndef NP_COMMERCE2_INTERNAL_H
#define NP_COMMERCE2_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np/np_commerce2.h>

/* the imports SceNpCommerce2 takes from libraries the tree has no header of yet */
extern void SceNpManager_9554802B(const void *data1, const void *data2, const void *data3);

/* the request kinds the Start and GetResult exports of one operation come in */
#define NP_COMMERCE2_VERSION_1		0
#define NP_COMMERCE2_VERSION_2		1
#define NP_COMMERCE2_VERSION_3		2

/* a request kind the operation does not have */
#define NP_COMMERCE2_ERROR_INVALID_VERSION	-2141909100	/* 0x80550f94 */

/* set by Init, cleared by Term */
extern int g_initialized;

/* the context and request tables (commerce2.c) */
extern int npCommerce2CreateContext(SceUInt32 version, SceUInt32 *ctxId, int shell);
extern int npCommerce2DestroyContext(SceUInt32 ctxId);
extern int npCommerce2DestroyRequest(SceUInt32 reqId);
extern int npCommerce2AbortRequest(SceUInt32 reqId);
extern int npCommerce2UnlockTable(void);

/* request builders and result readers not understood well enough to name yet; named by address */
extern int npCommerce2_8100D4EC(int a1, void *a2, int a3);
extern int npCommerce2_8100D61A(int a1, int a2, int a3);
extern int npCommerce2_81005E40(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);
extern int npCommerce2_810062FA(SceUInt32 reqId, const char *id, int a3, int a4, int a5, int a6);
extern int npCommerce2GetResultData(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize);
extern int npCommerce2_8100DAB0(SceNpCommerce2GetCategoryContentsResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_8100DD78(const SceNpCommerce2GetCategoryContentsResult *result,
				SceNpCommerce2CategoryInfo *categoryInfo);
extern int npCommerce2GetEnvironment(void);
extern int npCommerce2_8100DDCA(const SceNpCommerce2GetCategoryContentsResult *result,
				SceUInt32 index, SceNpCommerce2ContentInfo *contentInfo);
extern int npCommerce2_8100DE5C(const SceNpCommerce2ContentInfo *contentInfo,
				SceNpCommerce2CategoryInfo *categoryInfo);
extern int npCommerce2_8100DEB4(int a1, int a2);
extern int npCommerce2_8100E272(int a1, void *a2, int a3);
extern int npCommerce2_8100E5FA(int a1, int a2);
extern int npCommerce2_8100E6F4(const void *info, SceNpCommerce2ContentRatingInfo *contentRatingInfo);
extern int npCommerce2_8100E746(int a1, int a2, int a3);
extern int npCommerce2_8100E868(int a1, int a2, int a3);
extern int npCommerce2_8100ED8C(int a1, void *a2, int a3);
extern int npCommerce2_8101059C(int a1, int a2, int a3);
extern int npCommerce2_810102FC(int a1, int a2, int a3);
extern int npCommerce2_81010620(int a1, int a2, int a3);
extern int npCommerce2_810100C2(int a1, int a2, int a3);
extern int npCommerce2_8100F1D6(int a1, int a2);
extern char g_shortText[0x39];
extern char g_longText[0xFA1];
extern int npCommerce2_8100646C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100647A(SceUInt32 reqId, const char *categoryId, const char *productId,
				int a4, int a5);
extern int npCommerce2_8100F316(SceNpCommerce2GetProductInfoResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_8100F38C(int a1, int a2);
extern int npCommerce2_8100F416(int a1, void *a2, int a3);
extern int npCommerce2_8100F740(const SceNpCommerce2GetProductInfoResult *result,
				SceNpCommerce2GameProductInfo *gameProductInfo);
extern int npCommerce2_8100F798(int a1, int a2, int a3);
extern int npCommerce2_8100F8D8(int a1, int a2, void *a3, int a4);
extern int npCommerce2_8101144E(int a1, int a2, int a3, int a4);
extern int npCommerce2_810114C6(int a1, int a2, int a3);
extern int npCommerce2_8100D6BC(int a1, void *a2, int a3);
extern int npCommerce2_8100D8E4(int a1, int a2, int a3);
extern int npCommerce2_8100D8EC(int a1, int a2);
extern int npCommerce2_81013FEE(int a1, int a2);
extern int npCommerce2_810142C6(int a1, int a2);
extern int npCommerce2_8100D980(int a1, int a2, int a3);
extern int npCommerce2_810060C0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810060CE(SceUInt32 reqId);
extern int npCommerce2_810062EC(SceUInt32 ctxId, SceUInt32 *reqId);

/* the session requests (CreateSession) */
extern int npCommerce2CreateSessionRequest(SceUInt32 ctxId, SceUInt32 *reqId, int version);
extern int npCommerce2StartSessionRequest(SceUInt32 reqId);
extern int npCommerce2GetResult(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);

#endif
