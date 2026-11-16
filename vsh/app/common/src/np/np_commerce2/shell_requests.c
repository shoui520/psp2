/*
 * SceNpCommerce2: the requests that go to the shell's store service. The request of an operation
 * gets the structure its slot's type sends, filled from the arguments, and hands it to the slot.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_SHELL_REQUEST_BEGIN							\
	NpCommerce2Context *ctx = NULL;								\
	NpCommerce2Request *req = NULL;								\
	int ret;										\
												\
	ret = npCommerce2GetContext(&g_tableLock, reqId >> 16, &ctx, 1);			\
	if (ret >= 0)										\
		ret = npCommerce2GetRequest(&g_tableLock, reqId, &req);				\
	if (ret >= 0) {

#define NP_COMMERCE2_SHELL_REQUEST_END								\
	}											\
	if (req != NULL) {									\
		if ((req->flags & NP_COMMERCE2_REQUEST_FLAG_ABORTED) != 0)			\
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;					\
		npCommerce2ReleaseRequest(&g_tableLock, req);					\
	}											\
	if (ctx != NULL)									\
		npCommerce2ReleaseContext(&g_tableLock, ctx);					\
	return ret;

/* two names of 256 characters */
int npCommerce2Start_81007EDA(SceUInt32 reqId, const char *a2, const char *a3)
{
	NpCommerce2ShellRequestNames request;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, sizeof(request));
	request.size = sizeof(request);
	if (a2 != NULL) {
		sceClibStrncpy(request.first, a2, 0x100);
		request.first[255] = '\0';
	}
	if (a3 != NULL) {
		sceClibStrncpy(request.second, a3, 0x100);
		request.second[255] = '\0';
	}
	ret = npCommerce2StartShellSlot_810184BC(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}

/* two codes of three characters and four numbers */
int npCommerce2Start_81007FBE(SceUInt32 reqId, const char *a2, const char *a3, int a4, int a5, int a6, int a7)
{
	NpCommerce2ShellRequestCodes request;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, 0x1c);
	request.size = 0x1c;
	sceClibStrncpy(request.first, a2, 4);
	request.first[3] = '\0';
	sceClibStrncpy(request.second, a3, 4);
	request.second[3] = '\0';
	request.values[0] = a4;
	request.values[1] = a5;
	request.values[2] = a6;
	request.values[3] = a7;
	ret = npCommerce2StartShellSlot_810184B4(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}

/* a number and two names, to the slot type of the context's mode */
int npCommerce2Start_810096BA(SceUInt32 reqId, int a2, const char *a3, const char *a4, int a5)
{
	NpCommerce2ShellRequestPair request;
	int mode;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, sizeof(request));
	request.size = sizeof(request);
	request.value = a2;
	if (a3 != NULL) {
		sceClibStrncpy(request.first, a3, 0x100);
		request.first[255] = '\0';
	}
	if (a4 != NULL) {
		sceClibStrncpy(request.second, a4, 0x100);
		request.second[255] = '\0';
	}
	request.last = a5;
	mode = ctx->mode;
	if (mode == 8)
		ret = npCommerce2StartShellSlot_810184A4(req->shellSlot, &request);
	else if (mode == 7)
		ret = npCommerce2StartShellSlot_810184AC(req->shellSlot, &request);
	else
		ret = npCommerce2StartShellSlot_8101849C(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}

/* a text of 256 characters and three numbers */
int npCommerce2Start_8100A2B4(SceUInt32 reqId, const char *a2, int a3, int a4, int a5)
{
	NpCommerce2ShellRequestText request;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, sizeof(request));
	request.size = sizeof(request);
	if (a2 != NULL) {
		sceClibStrncpy(request.text, a2, 0x100);
		request.text[255] = '\0';
	}
	request.values[0] = a3;
	request.values[1] = a4;
	request.values[2] = a5;
	ret = npCommerce2StartShellSlot_810184C4(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}

/* two codes of three characters and three numbers */
int npCommerce2Start_8100A742(SceUInt32 reqId, const char *a2, const char *a3, int a4, int a5, int a6)
{
	NpCommerce2ShellRequestCodes request;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, 0x18);
	request.size = 0x18;
	sceClibStrncpy(request.first, a2, 4);
	request.first[3] = '\0';
	sceClibStrncpy(request.second, a3, 4);
	request.second[3] = '\0';
	request.values[0] = a4;
	request.values[1] = a5;
	request.values[2] = a6;
	ret = npCommerce2StartShellSlot_81018494(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}

/* a text of 256 characters */
int npCommerce2Start_8100B182(SceUInt32 reqId, const char *a2)
{
	NpCommerce2ShellRequestOne request;

	NP_COMMERCE2_SHELL_REQUEST_BEGIN
	sceClibMemset(&request, 0, sizeof(request));
	request.size = sizeof(request);
	sceClibStrncpy(request.text, a2, 0x100);
	request.text[255] = '\0';
	ret = npCommerce2StartShellSlot_810184CC(req->shellSlot, &request);
	NP_COMMERCE2_SHELL_REQUEST_END
}
