#include "shell_util.h"

/*
 * ShellUtil's IPMI stubs, methods 0xC0000-0xC001E in order: an asynchronous method returns
 * its request ID, a synchronous one its result. The file name is ours.
 */

#define SHELL_UTIL_METHOD(n)	(0xC0000 + (n))
int shellUtilIpcLock(IPMI::Client *pClient, int *pRequestId,
                 const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x0), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpcUnlock(IPMI::Client *pClient, int *pRequestId,
                   const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x1), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_156FEACF(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x2), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_040997D6(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x3), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_40DFAC6B(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const SceWChar16 *pString1,
			  const SceWChar16 *pString2)
{
	IPMI::DataInfo inList[2];
	int ret;

	inList[0].pData = pString1;
	inList[0].dataSize = 9 * sizeof(SceWChar16);
	inList[1].pData = pString2;
	inList[1].dataSize = 9 * sizeof(SceWChar16);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x4), inList, 2, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_03201F4B(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, const char *pString)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = pString;
	inList[0].dataSize = 32;
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x5), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_96BBF91B(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x6), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_8B69AD27(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x7), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_B65B60CA(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x8), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_3219A1C0(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x9), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_9B0EE918(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0xa), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_8F2F143D(IPMI::Client *pClient, int value)
{
	int result = 0;
	int in = value;
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &in;
	inList[0].dataSize = sizeof(in);
	ret = pClient->invokeSyncMethod(SHELL_UTIL_METHOD(0xb), inList, 1, &result,
					(IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int shellUtilIpc_636544FB(IPMI::Client *pClient, int value)
{
	int result = 0;
	int in = value;
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &in;
	inList[0].dataSize = sizeof(in);
	ret = pClient->invokeSyncMethod(SHELL_UTIL_METHOD(0xc), inList, 1, &result,
					(IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int shellUtilIpc_BB54D049(IPMI::Client *pClient, int value1, int value2, int value3)
{
	struct {
		int value1;
		int value2;
		int value3;
	} in;
	IPMI::DataInfo inList[1];
	int result = 0;
	int ret;

	in.value1 = value1;
	in.value2 = value2;
	in.value3 = value3;
	inList[0].pData = &in;
	inList[0].dataSize = sizeof(in);
	ret = pClient->invokeSyncMethod(SHELL_UTIL_METHOD(0xd), inList, 1, &result,
					(IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int shellUtilIpc_6430C45E(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0xe), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_29DB95D1(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0xf), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_0B066FAD(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x10), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_01141995(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x11), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_E701BA71(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, const char *pString)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = pString;
	inList[0].dataSize = 32;
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x12), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_0E7EAD89(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x13), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_F75C684D(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, int value)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = &value;
	inList[0].dataSize = sizeof(value);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x14), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_4A234ABB(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x15), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpcLaunchApp(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, const char *pCmd)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = pCmd;
	inList[0].dataSize = 960;
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x16), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_6F730976(IPMI::Client *pClient)
{
	int result = 0;
	int ret;

	ret = pClient->invokeSyncMethod(SHELL_UTIL_METHOD(0x17), (const IPMI::DataInfo *)NULL, 0,
					&result, (IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int shellUtilIpc_8CAAB23B(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x18), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_48C7E8F8(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, const char *pString)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = pString;
	inList[0].dataSize = 256;
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x19), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_B67A0C68(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag, const char *pString)
{
	IPMI::DataInfo inList[1];
	int ret;

	inList[0].pData = pString;
	inList[0].dataSize = 256;
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x1a), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_71C21A8C(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x1b), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_FB612CA8(IPMI::Client *pClient, int *pRequestId,
                      const IPMI::EventFlagInfo *pEventFlag)
{
	int ret;

	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x1c), (const IPMI::DataInfo *)NULL, 0,
					 pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpc_9A83AC60(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value1, int value2)
{
	struct {
		int value1;
		int value2;
	} in;
	IPMI::DataInfo inList[1];
	int ret;

	in.value1 = value1;
	in.value2 = value2;
	inList[0].pData = &in;
	inList[0].dataSize = sizeof(in);
	ret = pClient->invokeAsyncMethod(SHELL_UTIL_METHOD(0x1d), inList, 1, pRequestId,
					 pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int shellUtilIpcGetResult(IPMI::Client *pClient, int requestId)
{
	int result = 0;
	int ret;

	ret = pClient->tryGetResult(SHELL_UTIL_METHOD(0x1e), requestId, &result,
				    (IPMI::BufferInfo *)NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
