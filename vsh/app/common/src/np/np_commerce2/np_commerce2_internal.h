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

/* the request does not name what it is for */
#define NP_COMMERCE2_ERROR_NO_TARGET	-2141908950	/* 0x805510aa */

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
extern int npCommerce2_8100FE34(int a1, int a2, int a3);
extern int npCommerce2_8100FE7C(int a1, int a2, int a3);
extern int npCommerce2_8100FF6E(int a1, int a2, int a3);
extern int npCommerce2_81010B14(int a1, int a2);
extern int npCommerce2_81010BEC(int a1, int a2);
extern int npCommerce2_81010DD8(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100FDBC(const SceNpCommerce2GameProductInfo *gameProductInfo, int dataType,
				SceUInt32 idx, SceNpCommerce2GameSkuInfo *gameSkuInfo);
extern int npCommerce2_810065CC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810065DA(int a1, int a2, int a3, int a4);
extern int npCommerce2_81010938(int a1, void *a2, int a3);
extern int npCommerce2_81010C98(int a1, int a2, int a3);
extern int npCommerce2_81010FFA(int a1, int a2);
extern int npCommerce2_8101117E(int a1, void *a2, int a3);
extern int npCommerce2_81010710(int a1, int a2, void *a3, int a4);
extern int npCommerce2_8100672C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100673A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101150E(int a1, void *a2, int a3);
extern int npCommerce2_810115FC(int a1, int a2);
extern int npCommerce2_81011604(int a1, int a2);
extern int npCommerce2_81006872(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006880(int a1, int a2);
extern int npCommerce2_810069B2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810069C0(SceUInt32 reqId);
extern int npCommerce2_8101160C(int a1, void *a2, int a3);
extern int npCommerce2_810116FA(int a1, int a2);
extern int npCommerce2_81011702(int a1, int a2);
extern int npCommerce2_81006AF2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006B0E(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4);
extern int npCommerce2_8101170A(int a1, void *a2, int a3);
extern int npCommerce2_810117FC(int a1, int a2);
extern int npCommerce2_810118B0(int a1, int a2);
extern int npCommerce2_810118A2(int a1, int a2);
extern int npCommerce2_81006B00(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810118B8(int a1, void *a2, int a3);
extern int npCommerce2_810119A0(int a1, int a2);
extern int npCommerce2_810119A8(int a1, int a2);
extern int npCommerce2_810119B0(int a1, int a2);
extern int npCommerce2_81006C56(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006C64(SceUInt32 reqId);
extern int npCommerce2_810119BE(int a1, void *a2, int a3);
extern int npCommerce2_81011C24(int a1, int a2);
extern int npCommerce2_81006D96(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006DA4(SceUInt32 reqId);
extern int npCommerce2_81011C2C(int a1, void *a2, int a3);
extern int npCommerce2_81011D84(int a1, int a2, int a3);
extern int npCommerce2_81011D8C(int a1, int a2, int a3);
extern int npCommerce2_81006ED6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006F00(int a1, int a2, int a3);
extern int npCommerce2_81011E42(int a1, void *a2, int a3);
extern int npCommerce2_81011FC8(int a1, int a2, int a3);
extern int npCommerce2_81007038(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007046(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_81011FD0(int a1, void *a2, int a3);
extern int npCommerce2_8101209C(int a1, int a2, int a3);
extern int npCommerce2_8101284E(int a1, int a2, int a3);
extern int npCommerce2_810071C0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810071EA(SceUInt32 reqId);
extern int npCommerce2_810128C8(int a1, void *a2, int a3);
extern int npCommerce2_8100731C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007462(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007470(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012A5C(int a1, void *a2, int a3);
extern int npCommerce2_810075A8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810075B6(SceUInt32 reqId);
extern int npCommerce2_81012B60(int a1, void *a2, int a3);
extern int npCommerce2_81012BD6(int a1, int a2);
extern int npCommerce2_8100732A(int a1, const int *a2);
extern int npCommerce2_810076E8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007704(int a1, int a2);
extern int npCommerce2_81012BDE(int a1, void *a2, int a3);
extern int npCommerce2_81012CE8(int a1, int a2);
extern int npCommerce2_810076F6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012CF0(int a1, void *a2, int a3);
extern int npCommerce2_81012D66(int a1, int a2);
extern int npCommerce2_8100783C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007988(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100784A(int a1, int a2, int a3);
extern int npCommerce2_810079A4(int a1, int a2, int a3, int a4);
extern int npCommerce2_81012E3C(int a1, void *a2, int a3);
extern int npCommerce2_81012F80(int a1, int a2);
extern int npCommerce2_81013012(int a1, int a2);
extern int npCommerce2_8101301A(int a1, int a2, int a3);
extern int npCommerce2_81013362(int a1, void *a2, int a3);
extern int npCommerce2_81013432(int a1, int a2, int a3);
extern int npCommerce2_81007996(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013952(int a1, void *a2, int a3);
extern int npCommerce2_81013A22(int a1, int a2);
extern int npCommerce2_81013A2A(int a1, int a2);
extern int npCommerce2_810071CE(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013A32(int a1, void *a2, int a3);
extern int npCommerce2_81007AF6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007B04(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4);
extern int npCommerce2_81007C4C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007C5A(int a1, int a2, int a3, int a4);
extern int npCommerce2_81013BC6(int a1, void *a2, int a3);
extern int npCommerce2_81013C92(int a1, int a2, int a3);
extern int npCommerce2_81007DB6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007DC4(SceUInt32 reqId);
extern int npCommerce2_81013D24(int a1, void *a2, int a3);
extern int npCommerce2_81013DAE(int a1, int a2, int a3);
extern int npCommerce2_81013E86(int a1, int a2, int a3);
extern int npCommerce2_81013ECE(int a1, int a2, int a3);
extern int npCommerce2_8101413A(int a1, int a2);
extern int npCommerce2_81007ECC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007EDA(int a1, int a2, int a3);
extern int npCommerce2_81013F58(int a1, void *a2, int a3);
extern int npCommerce2_810141C2(int a1, int a2, int a3);
extern int npCommerce2_81007FB0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014250(int a1, void *a2, int a3);
extern int npCommerce2_810080A0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810080AE(SceUInt32 reqId);
extern int npCommerce2_810144A0(int a1, void *a2, int a3);
extern int npCommerce2_810145CE(int a1, int a2, int a3);
extern int npCommerce2_810081E0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100834A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007FBE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_810081EE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_81008358(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008490(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085DC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085EA(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008606(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085F8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100873E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100849E(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100874C(SceUInt32 reqId);
extern int npCommerce2_8101461E(int a1, void *a2, int a3);
extern int npCommerce2_81014694(int a1, int a2);
extern int npCommerce2_8100887E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810089FC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008B42(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008B50(int a1, int a2, int a3);
extern int npCommerce2_81008C8E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008C9C(SceUInt32 reqId);
extern int npCommerce2_8100888C(int a1, int a2, int a3, int a4);
extern int npCommerce2_81008A0A(int a1, const int *a2);
extern int npCommerce2_8101478E(int a1, void *a2, int a3);
extern int npCommerce2_81014804(int a1, int a2);
extern int npCommerce2_81008DCE(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008DDC(int a1, int a2, int a3);
extern int npCommerce2_81008F1A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008F28(int a1, int a2, int a3);
extern int npCommerce2_81006EE4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810148A2(int a1, void *a2, int a3);
extern int npCommerce2_81014A92(int a1, int a2, int a3);
extern int npCommerce2_81014A9A(int a1, int a2, int a3);
extern int npCommerce2_81014AA2(int a1, int a2);
extern int npCommerce2_81009066(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009074(int a1, int a2, int a3, int a4);
extern int npCommerce2_810091D0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810091DE(int a1, int a2, int a3);
extern int npCommerce2_810092E0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810092EE(SceUInt32 reqId);
extern int npCommerce2_81014AAA(int a1, void *a2, int a3);
extern int npCommerce2_81014B20(int a1, int a2);
extern int npCommerce2_81009420(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100942E(int a1, int a2, int a3);
extern int npCommerce2_8100956C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100957A(SceUInt32 reqId);
extern int npCommerce2_81014BCE(int a1, void *a2, int a3);
extern int npCommerce2_810096AC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014C44(int a1, void *a2, int a3);
extern int npCommerce2_81014D62(int a1, int a2, int a3);
extern int npCommerce2_81014D6A(int a1, int a2, int a3);
extern int npCommerce2_81014EB4(int a1, int a2, int a3);
extern int npCommerce2_810096BA(int a1, int a2, int a3, int a4);
extern int npCommerce2_810097C4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810097D2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014F08(int a1, void *a2, int a3);
extern int npCommerce2_81014F92(int a1, int a2, int a3);
extern int npCommerce2_8100998C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100999A(SceUInt32 reqId);
extern int npCommerce2_810151B8(int a1, void *a2, int a3);
extern int npCommerce2_8101522E(int a1, int a2);
extern int npCommerce2_8101545C(int a1, int a2, int a3);
extern int npCommerce2_81015606(int a1, int a2, int a3);
extern int npCommerce2_81015818(int a1, int a2, int a3);
extern int npCommerce2_810097E0(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_8100F2F6(int a1, int a2);
extern int npCommerce2_81009AD4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009AE2(int a1, int a2, int a3);
extern int npCommerce2_8101591C(int a1, void *a2, int a3);
extern int npCommerce2_810159E8(int a1, int a2, int a3);
extern int npCommerce2_81009C20(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009C2E(int a1, int a2, int a3, int a4);
extern int npCommerce2_81015BCE(int a1, void *a2, int a3);
extern int npCommerce2_81015C9A(int a1, int a2, int a3);
extern int npCommerce2_81009D76(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009D84(int a1, int a2, int a3, int a4);
extern int npCommerce2_81015EB4(int a1, void *a2, int a3);
extern int npCommerce2_81015F80(int a1, int a2, int a3);
extern int npCommerce2_810162BA(int a1, int a2, int a3);
extern int npCommerce2_81009ECC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009EDA(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A012(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A020(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A158(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A166(SceUInt32 reqId);
extern int npCommerce2_810163AC(int a1, void *a2, int a3);
extern int npCommerce2_81016436(int a1, int a2, int a3);
extern int npCommerce2_8100A298(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A2A6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101652E(int a1, void *a2, int a3);
extern int npCommerce2_810165A4(int a1, int a2);
extern int npCommerce2_8100A380(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810165F0(int a1, void *a2, int a3);
extern int npCommerce2_8100AECA(int a1, int a2);
extern int npCommerce2_8100A2B4(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_8100A38E(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100A49A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A83A(SceUInt32 reqId);
extern int npCommerce2_8100A4A8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A5F4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A602(SceUInt32 reqId);
extern int npCommerce2_81016666(int a1, void *a2, int a3);
extern int npCommerce2_8100B174(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B182(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A734(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A4B6(int a1, const int *a2, int a3);
extern int npCommerce2_8100A81E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006EF2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101670C(int a1, void *a2, int a3);
extern int npCommerce2_810168FC(int a1, int a2, int a3);
extern int npCommerce2_81016904(int a1, int a2, int a3);
extern int npCommerce2_8101690C(int a1, int a2);
extern int npCommerce2_8100A82C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A742(int a1, int a2, int a3, int a4, int a5, int a6);
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
