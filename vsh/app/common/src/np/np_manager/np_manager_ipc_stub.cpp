/*
 * The requests SceNpManager sends to the shell's NP service, one function per IPMI method.
 * A blocking request is a pair: the start sends the request with the BlockingIpc event flag,
 * the finish takes its result. The method ids are the service's; the functions are named
 * after the export that sends them.
 */

#include "np_manager_internal.h"

/* the start of a blocking request: the request is sent, its result waits on the event flag */
static inline int npIpcStart(IPMI::Client *client, SceUInt32 methodId, const IPMI::DataInfo *in,
			     SceUInt32 inCount, int *pRequestId,
			     const BlockingIpc::Client::Handle *pHandle)
{
	int ret;

	ret = client->invokeAsyncMethod(methodId, in, inCount, pRequestId,
					(const IPMI::EventFlagInfo *)pHandle);
	if (ret == 0)
		return 0;
	return ret;
}

/* the length of a string sent with a request */
static inline SceSize npIpcStrlen(const SceChar8 *str)
{
	SceSize len;

	len = 0;
	while (str[len] != '\0')
		len++;
	return len;
}

int npManagerIpcStart_877251F3(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId)
{
	IPMI::DataInfo in[1];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	return npIpcStart(client, 0xb0000, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_877251F3(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 36;
	ret = client->tryGetResult(0xb0001, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_2841C20F(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId)
{
	IPMI::DataInfo in[1];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	return npIpcStart(client, 0xb0002, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_2841C20F(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 36;
	ret = client->tryGetResult(0xb0003, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_80895A1E(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize)
{
	NpManagerTicketParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.bufferSize = bufferSize;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0004, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_80895A1E(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket)
{
	int result;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	out[0].pBuffer = pTicketSize;
	out[0].bufferSize = 4;
	out[1].pBuffer = pSize;
	out[1].bufferSize = 4;
	out[2].pBuffer = ticket;
	out[2].bufferSize = NP_MANAGER_TICKET_SIZE;
	ret = client->tryGetResult(0xb0005, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_E2FAC2CA(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize)
{
	NpManagerTicketParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.bufferSize = bufferSize;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0006, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_E2FAC2CA(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket)
{
	int result;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	out[0].pBuffer = pTicketSize;
	out[0].bufferSize = 4;
	out[1].pBuffer = pSize;
	out[1].bufferSize = 4;
	out[2].pBuffer = ticket;
	out[2].bufferSize = NP_MANAGER_TICKET_SIZE;
	ret = client->tryGetResult(0xb0007, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_5EB63590(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize)
{
	NpManagerTicketParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.bufferSize = bufferSize;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0008, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_5EB63590(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket)
{
	int result;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	out[0].pBuffer = pTicketSize;
	out[0].bufferSize = 4;
	out[1].pBuffer = pSize;
	out[1].bufferSize = 4;
	out[2].pBuffer = ticket;
	out[2].bufferSize = NP_MANAGER_TICKET_SIZE;
	ret = client->tryGetResult(0xb0009, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_2FE3625C(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int wantData1,
			       int wantData2)
{
	NpManagerIpcParam_2FE3625C param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.wantData1 = wantData1;
	param.wantData2 = wantData2;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb000a, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_2FE3625C(IPMI::Client *client, int requestId, void *buf1, void *buf2)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = buf1;
	out[0].bufferSize = 464;
	out[1].pBuffer = buf2;
	out[1].bufferSize = 16;
	ret = client->tryGetResult(0xb000b, requestId, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_63EF4853(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data1, const void *data2)
{
	IPMI::DataInfo in[3];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	in[1].pData = data1;
	in[1].dataSize = 68;
	in[2].pData = data2;
	in[2].dataSize = 36;
	return npIpcStart(client, 0xb000c, in, 3, pRequestId, pHandle);
}

int npManagerIpcFinish_63EF4853(IPMI::Client *client, int requestId)
{
	int result;
	int ret;

	result = 0;
	ret = client->tryGetResult(0xb000d, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_0F6AC47E(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, int value3)
{
	NpManagerIpcParam_0F6AC47E param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.value3 = value3;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb000e, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_0F6AC47E(IPMI::Client *client, int requestId, int *value)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = value;
	out[0].bufferSize = 4;
	ret = client->tryGetResult(0xb000f, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_sceNpManagerGetNpId(IPMI::Client *client, SceNpId *npId)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = npId;
	out[0].bufferSize = sizeof(*npId);
	ret = client->invokeSyncMethod(0xb0010, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_1796CD2E(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0011, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_A775A452(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0012, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_455E05C1(IPMI::Client *client, SceUInt64 value, const void *data1,
			  int data1Absent, const void *data2, int data2Absent)
{
	int result;
	NpManagerIpcParam_455E05C1 param;
	IPMI::DataInfo in[3];
	int ret;

	result = 0;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	param.value = value;
	in[1].pData = data1;
	in[1].dataSize = 68;
	param.data1Absent = data1Absent;
	in[2].pData = data2;
	in[2].dataSize = 36;
	param.data2Absent = data2Absent;
	ret = client->invokeSyncMethod(0xb0013, in, 3, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_D83C8CCB(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0014, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_E45E4003(IPMI::Client *client, NpManagerAccountInfo *info)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = info;
	out[0].bufferSize = sizeof(*info);
	ret = client->invokeSyncMethod(0xb0015, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_D4180D28(IPMI::Client *client)
{
	int result;
	int ret;

	result = 0;
	ret = client->invokeSyncMethod(0xb0016, (const IPMI::DataInfo *)NULL, 0, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_2EA038D8(IPMI::Client *client, void *ticket, SceSize size, SceSize *pSize)
{
	int result;
	SceSize param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	param = size;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = ticket;
	out[0].bufferSize = NP_MANAGER_TICKET_SIZE;
	out[1].pBuffer = pSize;
	out[1].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0017, in, 1, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_4341692A(IPMI::Client *client, int *value)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = value;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0018, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_C80D8C3E(IPMI::Client *client, int value, void *ticket, SceSize *pSize)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = ticket;
	out[0].bufferSize = NP_MANAGER_TICKET_SIZE;
	out[1].pBuffer = pSize;
	out[1].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0019, in, 1, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_6CDB2AEC(IPMI::Client *client, int hasData1, const void *data1, int hasData2,
			  const void *data2, int hasData3, const void *data3)
{
	int result;
	NpManagerIpcFlags3 flags;
	IPMI::DataInfo in[4];
	int ret;

	result = 0;
	flags.hasData1 = hasData1;
	in[0].pData = &flags;
	in[0].dataSize = sizeof(flags);
	in[1].pData = data1;
	in[1].dataSize = 68;
	flags.hasData2 = hasData2;
	in[2].pData = data2;
	in[2].dataSize = 36;
	flags.hasData3 = hasData3;
	in[3].pData = data3;
	in[3].dataSize = 8;
	ret = client->invokeSyncMethod(0xb001a, in, 4, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_ED12AF16(IPMI::Client *client, int hasData1, const void *data1, int hasData2,
			  const void *data2, int hasData3, const void *data3, int hasData4,
			  const void *data4)
{
	int result;
	NpManagerIpcFlags4 flags;
	IPMI::DataInfo in[5];
	int ret;

	result = 0;
	flags.hasData1 = hasData1;
	in[0].pData = &flags;
	in[0].dataSize = sizeof(flags);
	in[1].pData = data1;
	in[1].dataSize = 68;
	flags.hasData2 = hasData2;
	in[2].pData = data2;
	in[2].dataSize = 36;
	flags.hasData3 = hasData3;
	in[3].pData = data3;
	in[3].dataSize = 8;
	flags.hasData4 = hasData4;
	in[4].pData = data4;
	in[4].dataSize = 20;
	ret = client->invokeSyncMethod(0xb001b, in, 5, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_DDF30B0E(IPMI::Client *client, int hasData1, void *data1, int hasData2,
			  void *data2, int hasData3, void *data3)
{
	int result;
	NpManagerIpcFlags3 flags;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[3];
	int ret;

	flags.hasData1 = hasData1;
	flags.hasData2 = hasData2;
	result = 0;
	in[0].pData = &flags;
	in[0].dataSize = sizeof(flags);
	out[0].pBuffer = data1;
	out[0].bufferSize = 68;
	out[1].pBuffer = data2;
	out[1].bufferSize = 36;
	flags.hasData3 = hasData3;
	out[2].pBuffer = data3;
	out[2].bufferSize = 8;
	ret = client->invokeSyncMethod(0xb001c, in, 1, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_4E94274C(IPMI::Client *client)
{
	int result;
	int ret;

	result = 0;
	ret = client->invokeSyncMethod(0xb001d, (const IPMI::DataInfo *)NULL, 0, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_sceNpManagerGetAccountRegion(IPMI::Client *client, SceNpCountryCode *countryCode,
			  int *languageCode)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = countryCode;
	out[0].bufferSize = 4;
	out[1].pBuffer = languageCode;
	out[1].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb001e, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_97291707(IPMI::Client *client, void *data1, void *data2)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = data1;
	out[0].bufferSize = 4;
	out[1].pBuffer = data2;
	out[1].bufferSize = 6;
	ret = client->invokeSyncMethod(0xb001f, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_sceNpManagerGetContentRatingFlag(IPMI::Client *client, int *isRestricted, int *age)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = isRestricted;
	out[0].bufferSize = 4;
	out[1].pBuffer = age;
	out[1].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0020, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_sceNpManagerGetChatRestrictionFlag(IPMI::Client *client, int *isRestricted)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = isRestricted;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0021, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_BB384A4D(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0022, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_351A74C2(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0023, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_8C6EACC4(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0024, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_FFBFC540(IPMI::Client *client, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 64;
	ret = client->invokeSyncMethod(0xb0025, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_375A8BC0(IPMI::Client *client, int *pValue, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = pValue;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0026, in, 1, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_F35CC917(IPMI::Client *client, int *pValue, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = pValue;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0027, in, 1, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_E41932D4(IPMI::Client *client, int *pValue, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = pValue;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb0028, in, 1, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_D1D191FB(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb0029, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_EB16514C(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(0xb002a, in, 1, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_96534006(IPMI::Client *client, int value, int *pValue)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = pValue;
	out[0].bufferSize = 4;
	ret = client->invokeSyncMethod(0xb002b, in, 1, &result, out,
				       1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_FF0D6501(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data, int value)
{
	NpManagerIpcParam_FF0D6501 param;
	IPMI::DataInfo in[2];

	param.reqId = reqId;
	param.value = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = 36;
	return npIpcStart(client, 0xb002c, in, 2, pRequestId, pHandle);
}

int npManagerIpcFinish_FF0D6501(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 456;
	ret = client->tryGetResult(0xb002d, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_2BB7D844(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data1,
			       int data1Absent, const void *data2, int data2Absent,
			       const void *data3,
			       int data3Absent, const int *pValue1, int value1Absent,
			       const void *data4,
			       int data4Absent, int value2, const int *pValue3, int value3Absent,
			       const void *data5, int data5Absent, int value4, const void *data6,
			       SceSize data6Size)
{
	NpManagerIpcParam_2BB7D844 param;
	IPMI::DataInfo in[9];

	param.reqId = reqId;
	param.data1Absent = data1Absent;
	param.data2Absent = data2Absent;
	param.data3Absent = data3Absent;
	param.value1Absent = value1Absent;
	param.data4Absent = data4Absent;
	param.value2 = value2;
	param.value3Absent = value3Absent;
	param.data5Absent = data5Absent;
	param.value4 = value4;
	param.data6Size = data6Size;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data1;
	in[1].dataSize = 52;
	in[2].pData = data2;
	in[2].dataSize = 16;
	in[3].pData = data3;
	in[3].dataSize = 64;
	in[4].pData = pValue1;
	in[4].dataSize = 4;
	in[5].pData = data4;
	in[5].dataSize = 8;
	in[6].pData = pValue3;
	in[6].dataSize = 4;
	in[7].pData = data5;
	in[7].dataSize = 13;
	in[8].pData = data6;
	in[8].dataSize = data6Size;
	return npIpcStart(client, 0xb002e, in, 9, pRequestId, pHandle);
}

int npManagerIpcFinish_2BB7D844(IPMI::Client *client, int requestId, void *buf1, int *value1,
				int *value2, void *buf2)
{
	int result;
	IPMI::BufferInfo out[4];
	int ret;

	result = 0;
	out[0].pBuffer = buf1;
	out[0].bufferSize = 456;
	out[1].pBuffer = value1;
	out[1].bufferSize = 4;
	out[2].pBuffer = value2;
	out[2].bufferSize = 4;
	out[3].pBuffer = buf2;
	out[3].bufferSize = 8;
	ret = client->tryGetResult(0xb002f, requestId, &result, out, 4);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_A5F0BD9C(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count)
{
	NpManagerIpcListParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.count = count;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0030, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_A5F0BD9C(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = entries;
	out[1].bufferSize = 412 * count;
	out[2].pBuffer = pValue;
	out[2].bufferSize = 4;
	ret = client->tryGetResult(0xb0031, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_D6C54CC2(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const int *data, int value1, int value2, SceUInt32 count)
{
	NpManagerIpcListParam param;
	IPMI::DataInfo in[2];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.count = count;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = 4;
	return npIpcStart(client, 0xb0032, in, 2, pRequestId, pHandle);
}

int npManagerIpcFinish_D6C54CC2(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, int *pValue)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[4];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = values;
	out[1].bufferSize = 4 * count;
	out[2].pBuffer = entries;
	out[2].bufferSize = count << 7;
	out[3].pBuffer = pValue;
	out[3].bufferSize = 4;
	ret = client->tryGetResult(0xb0033, requestId, &result, out, 4);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_89E693FB(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count)
{
	NpManagerIpcListParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.count = count;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0034, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_89E693FB(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = entries;
	out[1].bufferSize = 412 * count;
	out[2].pBuffer = pValue;
	out[2].bufferSize = 4;
	ret = client->tryGetResult(0xb0035, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_28DED5D1(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const int *data, int value1, int value2, SceUInt32 count)
{
	NpManagerIpcListParam param;
	IPMI::DataInfo in[2];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.count = count;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = 4;
	return npIpcStart(client, 0xb0036, in, 2, pRequestId, pHandle);
}

int npManagerIpcFinish_28DED5D1(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				int *values1, int *values2, void *entries, int *pValue)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[5];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = values1;
	out[1].bufferSize = 4 * count;
	out[2].pBuffer = values2;
	out[2].bufferSize = 4 * count;
	out[3].pBuffer = entries;
	out[3].bufferSize = count << 7;
	out[4].pBuffer = pValue;
	out[4].bufferSize = 4;
	ret = client->tryGetResult(0xb0037, requestId, &result, out, 5);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_6D13F55F(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count)
{
	NpManagerIpcListParam param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.count = count;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0038, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_6D13F55F(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = entries;
	out[1].bufferSize = 412 * count;
	out[2].pBuffer = pValue;
	out[2].bufferSize = 4;
	ret = client->tryGetResult(0xb0039, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_C7F6ADC4(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data,
			       SceSize size, const int *pValue, int value1, int value2, int value3,
			       int value4)
{
	NpManagerIpcParam_C7F6ADC4 param;
	IPMI::DataInfo in[3];

	param.reqId = reqId;
	param.size = size;
	param.value1 = value1;
	param.value2 = value2;
	param.value3 = value3;
	param.value4 = value4;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = size;
	in[2].pData = pValue;
	in[2].dataSize = 4;
	return npIpcStart(client, 0xb003a, in, 3, pRequestId, pHandle);
}

int npManagerIpcFinish_C7F6ADC4(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, SceUInt32 *pCount2,
				int *values2, int *pValue1, int *pValue2)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[7];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = values;
	out[1].bufferSize = 4 * count;
	out[2].pBuffer = entries;
	out[2].bufferSize = count << 7;
	out[3].pBuffer = pCount2;
	out[3].bufferSize = 4;
	out[4].pBuffer = values2;
	out[4].bufferSize = 4 * *pCount2;
	out[5].pBuffer = pValue1;
	out[5].bufferSize = 4;
	out[6].pBuffer = pValue2;
	out[6].bufferSize = 4;
	ret = client->tryGetResult(0xb003b, requestId, &result, out, 7);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_BBCD1B6D(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data, SceSize size, int value1, int value2)
{
	NpManagerIpcParam_BBCD1B6D param;
	IPMI::DataInfo in[2];

	param.reqId = reqId;
	param.size = size;
	param.value1 = value1;
	param.value2 = value2;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = data;
	in[1].dataSize = size;
	return npIpcStart(client, 0xb003c, in, 2, pRequestId, pHandle);
}

int npManagerIpcFinish_BBCD1B6D(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 128;
	ret = client->tryGetResult(0xb003d, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_557D5CC1(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, const int *pValue, int value3, int value4, int value5,
			       int value6)
{
	NpManagerIpcParam_557D5CC1 param;
	IPMI::DataInfo in[2];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.value3 = value3;
	param.value4 = value4;
	param.value5 = value5;
	param.value6 = value6;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = pValue;
	in[1].dataSize = 4;
	return npIpcStart(client, 0xb003e, in, 2, pRequestId, pHandle);
}

int npManagerIpcFinish_557D5CC1(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, SceUInt32 *pCount2,
				int *values2, int *pValue1, int *pValue2)
{
	int result;
	SceUInt32 count;
	IPMI::BufferInfo out[7];
	int ret;

	result = 0;
	count = *pCount;
	out[0].pBuffer = pCount;
	out[0].bufferSize = 4;
	out[1].pBuffer = values;
	out[1].bufferSize = 4 * count;
	out[2].pBuffer = entries;
	out[2].bufferSize = count << 7;
	out[3].pBuffer = pCount2;
	out[3].bufferSize = 4;
	out[4].pBuffer = values2;
	out[4].bufferSize = 4 * *pCount2;
	out[5].pBuffer = pValue1;
	out[5].bufferSize = 4;
	out[6].pBuffer = pValue2;
	out[6].bufferSize = 4;
	ret = client->tryGetResult(0xb003f, requestId, &result, out, 7);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_D891AB9A(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, int value3, int value4)
{
	NpManagerIpcParam_D891AB9A param;
	IPMI::DataInfo in[1];

	param.reqId = reqId;
	param.value1 = value1;
	param.value2 = value2;
	param.value3 = value3;
	param.value4 = value4;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	return npIpcStart(client, 0xb0040, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_D891AB9A(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 128;
	ret = client->tryGetResult(0xb0041, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_FDE78C74(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId)
{
	IPMI::DataInfo in[1];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	return npIpcStart(client, 0xb0042, in, 1, pRequestId, pHandle);
}

int npManagerIpcFinish_FDE78C74(IPMI::Client *client, int requestId, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 264;
	ret = client->tryGetResult(0xb0043, requestId, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_npAuthGetAuthorizationCode(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const SceNpClientId *pClientId, const SceChar8 *pScope)
{
	IPMI::DataInfo in[3];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	in[1].pData = pClientId;
	in[1].dataSize = sizeof(*pClientId);
	in[2].pData = pScope;
	in[2].dataSize = npIpcStrlen(pScope) + 1;
	return npIpcStart(client, 0xb0044, in, 3, pRequestId, pHandle);
}

int npManagerIpcFinish_npAuthGetAuthorizationCode(IPMI::Client *client, int requestId,
				SceNpAuthorizationCode *authCode, int *issuerId)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = authCode;
	out[0].bufferSize = sizeof(*authCode);
	out[1].pBuffer = issuerId;
	out[1].bufferSize = 4;
	ret = client->tryGetResult(0xb0045, requestId, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpcStart_npAuthGetAuthorizationCodeEx(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const SceNpClientId *pClientId, const SceChar8 *pClientSecret,
			       const SceChar8 *pScope)
{
	IPMI::DataInfo in[4];

	in[0].pData = &reqId;
	in[0].dataSize = sizeof(reqId);
	in[1].pData = pClientId;
	in[1].dataSize = sizeof(*pClientId);
	in[2].pData = pClientSecret;
	in[2].dataSize = npIpcStrlen(pClientSecret) + 1;
	in[3].pData = pScope;
	in[3].dataSize = npIpcStrlen(pScope) + 1;
	return npIpcStart(client, 0xb0046, in, 4, pRequestId, pHandle);
}

int npManagerIpcFinish_npAuthGetAuthorizationCodeEx(IPMI::Client *client, int requestId,
				SceNpAuthorizationCode *authCode, int *issuerId)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = authCode;
	out[0].bufferSize = sizeof(*authCode);
	out[1].pBuffer = issuerId;
	out[1].bufferSize = 4;
	ret = client->tryGetResult(0xb0047, requestId, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_F66A7AAE(IPMI::Client *client)
{
	int result;
	int ret;

	result = 0;
	ret = client->invokeSyncMethod(0xb0048, (const IPMI::DataInfo *)NULL, 0, &result, NULL,
				       0);
	if (ret != 0)
		return ret;
	return result;
}

int npManagerIpc_1F0C6314(IPMI::Client *client, void *buf, SceUInt8 *value)
{
	int result;
	IPMI::BufferInfo out[2];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 264;
	out[1].pBuffer = value;
	out[1].bufferSize = 1;
	ret = client->invokeSyncMethod(0xb0049, (const IPMI::DataInfo *)NULL, 0, &result, out,
				       2);
	if (ret != 0)
		return ret;
	return result;
}
