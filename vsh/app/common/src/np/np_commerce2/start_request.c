/*
 * SceNpCommerce2: the request starters. Each takes the context and the request of an ID,
 * checks that the request may run, renews the connection when it is a second old, and hands
 * the request to its operation's sender. Only the sender and the arguments after the request ID
 * differ between them.
 */

#include "np_commerce2_internal.h"

/* one request starter */
#define NP_COMMERCE2_ARGS(...)	, __VA_ARGS__
#define NP_COMMERCE2_NO_ARGS
#define NP_COMMERCE2_UNPAREN(...)	__VA_ARGS__
#define NP_COMMERCE2_START_REQUEST_EX(name, send, firstArg, precond, shellCheck, extra, after)			\
{												\
	NpCommerce2Context *ctx = NULL;								\
	NpCommerce2Request *req = NULL;								\
	SceUInt64 now;										\
	int linked;										\
	int ret;										\
												\
	ret = npCommerce2GetContext(&g_tableLock, reqId >> 16, &ctx, 1);			\
	if (ret >= 0) {										\
		ret = npCommerce2GetRequest(&g_tableLock, reqId, &req);				\
		if (ret >= 0) {									\
			ret = (precond) ? npCommerce2CheckRequest(ctx, req) : SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT; \
			if (ret >= 0) {								\
				NpCommerce2Context *c = ctx;					\
												\
				linked = 0;							\
				if ((shellCheck) && c->shellSlot != 0) {					\
					int r = npCommerce2_81017FE8(c->shellSlot, &linked);	\
												\
					if (r >= 0) {						\
						if (linked != c->shellCounter) {		\
							c->text[0] = '\0';				\
							r = NP_COMMERCE2_ERROR_SHELL_CHANGED;	\
						}						\
						ret = r;					\
					} else {						\
						ret = 0;					\
					}							\
				} else {							\
					ret = 0;						\
				}								\
				if (ret >= 0) {							\
					c = ctx;						\
					if (npCommerce2GetSystemTime(&now) >= 0 &&		\
					    now - c->lastRequestTime >= 10000000)		\
						npCommerce2RenewConnection(c->httpTemplate,	\
									   &c->httpConnection,	\
									   10000000);		\
					ret = send(firstArg, ctx->httpTemplate, &ctx->httpConnection,	\
						   req->type, &req->httpRequest NP_COMMERCE2_UNPAREN extra);	\
					if (ret >= 0)						\
						{ npCommerce2GetSystemTime(&ctx->lastRequestTime); after }	\
				}								\
			}									\
		}										\
	}											\
	if (req != NULL) {									\
		if ((req->flags & NP_COMMERCE2_REQUEST_FLAG_ABORTED) != 0)			\
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;					\
		npCommerce2ReleaseRequest(&g_tableLock, req);					\
	}											\
	if (ctx != NULL)									\
		npCommerce2ReleaseContext(&g_tableLock, ctx);					\
	return ret;										\
}

/* the common starter: the sender gets the context's mode */
#define NP_COMMERCE2_START_REQUEST(name, send, extra, after)					\
	NP_COMMERCE2_START_REQUEST_EX(name, send, ctx->mode, 1, 1, (extra), after)

int npCommerce2Start_8100673A(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100673A, npCommerce2Send_8101940E, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_81006880(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81006880, npCommerce2Send_8101950A, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_810069C0(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810069C0, npCommerce2Send_81019592, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81006B0E(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81006B0E, npCommerce2Send_8101961A, NP_COMMERCE2_ARGS(a2, a3, a4), )

int npCommerce2Start_81006C64(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81006C64, npCommerce2Send_810197A6, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81006DA4(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81006DA4, npCommerce2Send_8101982E, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81006F00(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81006F00, npCommerce2Send_810198B6, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_810071EA(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810071EA, npCommerce2Send_81019BEC, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_8100732A(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100732A, npCommerce2Send_8101A378, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_81007470(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81007470, npCommerce2Send_8101A43E, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_810075B6(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810075B6, npCommerce2Send_8101A53A, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81007704(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81007704, npCommerce2Send_8101A5C2, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100784A(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100784A, npCommerce2Send_8101A688, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_81007B04(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81007B04, npCommerce2Send_8101A9D6, NP_COMMERCE2_ARGS(a2, a3, a4), )

int npCommerce2Start_810080AE(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810080AE, npCommerce2Send_8101AFE4, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81008358(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008358, npCommerce2Send_8101B2A8, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100849E(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100849E, npCommerce2Send_8101B3CC, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_81008606(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008606, npCommerce2Send_8101B560, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100874C(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100874C, npCommerce2Send_8101B684, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81008A0A(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008A0A, npCommerce2Send_8101B9B2, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_81008B50(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008B50, npCommerce2Send_8101BA78, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_81008C9C(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008C9C, npCommerce2Send_8101BBB8, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_81008DDC(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008DDC, npCommerce2Send_8101BC40, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_81008F28(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81008F28, npCommerce2Send_8101BD76, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_810092EE(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810092EE, npCommerce2Send_8101C224, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_8100942E(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100942E, npCommerce2Send_8101C2AC, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_8100957A(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100957A, npCommerce2Send_8101C3E2, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_8100999A(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100999A, npCommerce2Send_8101C818, NP_COMMERCE2_ARGS(a2, a3, a4), )

int npCommerce2Start_81009AE2(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81009AE2, npCommerce2Send_8101C8F4, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_81009C2E(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81009C2E, npCommerce2Send_8101C9F2, NP_COMMERCE2_ARGS(a2, a3, a4), )

int npCommerce2Start_81009D84(SceUInt32 reqId, int a2, int a3, int a4)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81009D84, npCommerce2Send_8101CB3C, NP_COMMERCE2_ARGS(a2, a3, a4), )

int npCommerce2Start_81009EDA(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81009EDA, npCommerce2Send_8101940E, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100A020(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A020, npCommerce2Send_8101940E, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100A166(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A166, npCommerce2Send_8101982E, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_8100A4B6(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A4B6, npCommerce2Send_8101CE02, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_8100A602(SceUInt32 reqId)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A602, npCommerce2Send_8101CECC, NP_COMMERCE2_NO_ARGS, )

int npCommerce2Start_8100A83A(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A83A, npCommerce2Send_8101CF56, NP_COMMERCE2_NO_ARGS,
			   ctx->responseSize = 0;)

int npCommerce2Start_8100A982(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100A982, npCommerce2Send_8101CFDE, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_8100AC38(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100AC38, npCommerce2Send_8101D2B0, NP_COMMERCE2_ARGS(a2), )

int npCommerce2Start_8100AD7E(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100AD7E, npCommerce2Send_8101D376, NP_COMMERCE2_ARGS(a2, a3), )

int npCommerce2Start_8100AECA(SceUInt32 reqId, int a2)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100AECA, npCommerce2Send_8101D4B6, NP_COMMERCE2_ARGS(a2), )

/* starters of operations that carry their arguments on to the sender */
int npCommerce2Start_810062FA(SceUInt32 reqId, int a2, int a3, int a4, int a5, SceUInt32 a6)
NP_COMMERCE2_START_REQUEST_EX(npCommerce2Start_810062FA, npCommerce2Send_81018D02, ctx->mode,
			      ctx->kind != 0 || a6 <= 0x3c, 1, (NP_COMMERCE2_ARGS(a2, a3, a4, a5, a6)), )

int npCommerce2Start_8100647A(SceUInt32 reqId, int a2, int a3, int a4, int a5)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100647A, npCommerce2Send_81018EF8, NP_COMMERCE2_ARGS(a2, a3, a4, a5), )

int npCommerce2Start_810065DA(SceUInt32 reqId, int a2, int a3, int a4, int a5)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810065DA, npCommerce2Send_81019254, NP_COMMERCE2_ARGS(a2, a3, a4, a5), )

int npCommerce2Start_81007046(SceUInt32 reqId, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81007046, npCommerce2Send_81019998,
			   NP_COMMERCE2_ARGS(a2, a3, a4, a5, a6, a7, a8, a9), )

int npCommerce2Start_810079A4(SceUInt32 reqId, int a2, int a3, int a4, int a5)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810079A4, npCommerce2Send_8101A7BE, NP_COMMERCE2_ARGS(a2, a3, a4, a5), )

int npCommerce2Start_81007C5A(SceUInt32 reqId, SceUInt32 a2, int a3, int a4, int a5, int a6, int a7)
NP_COMMERCE2_START_REQUEST_EX(npCommerce2Start_81007C5A, npCommerce2Send_8101AE3E, a2, 1, 1,
			      (NP_COMMERCE2_ARGS(a3, a4, a5, a6, a7)), )

int npCommerce2Start_810081EE(SceUInt32 reqId, int a2, int a3, int a4, int a5, int a6)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_810081EE, npCommerce2Send_8101B06C, NP_COMMERCE2_ARGS(a2, a3, a4, a5, a6), )

int npCommerce2Start_8100888C(SceUInt32 reqId, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_8100888C, npCommerce2Send_8101B70C,
			   NP_COMMERCE2_ARGS(a2, a3, a4, a5, a6, a7, a8), )

int npCommerce2Start_81009074(SceUInt32 reqId, int a2, int a3, int a4, int a5, int a6)
NP_COMMERCE2_START_REQUEST(npCommerce2Start_81009074, npCommerce2Send_8101BEBC, NP_COMMERCE2_ARGS(a2, a3, a4, a5, a6), )

int npCommerce2Start_810091DE(SceUInt32 reqId, int a2, int a3)
NP_COMMERCE2_START_REQUEST_EX(npCommerce2Start_810091DE, npCommerce2Send_8101C0B4, ctx->mode, 1, 0,
			      (NP_COMMERCE2_ARGS(a2, a3)), )
