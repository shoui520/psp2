/*
 * The shell's LiveArea methods over SceShellSvc's IPMI connection. Sony's build keeps them out of
 * livearea_util.cpp (after its exports, not inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "livearea_util_ipc_client.h"

#define LIVEAREA_UTIL_METHOD_REPLACE_ALL			0x110000
#define LIVEAREA_UTIL_METHOD_REPLACE_ALL_RESULT			0x110001
#define LIVEAREA_UTIL_METHOD_UPDATE_FRAME			0x110002
#define LIVEAREA_UTIL_METHOD_UPDATE_FRAME_RESULT		0x110003
#define LIVEAREA_UTIL_METHOD_UPDATE_FRAME_LIST			0x110004
#define LIVEAREA_UTIL_METHOD_UPDATE_FRAME_LIST_RESULT		0x110005
#define LIVEAREA_UTIL_METHOD_GET_FRAME_REVISION			0x110006
#define LIVEAREA_UTIL_METHOD_GET_FRAME_REVISION_RESULT		0x110007
#define LIVEAREA_UTIL_METHOD_GET_FRAME_USER_DATA		0x110008
#define LIVEAREA_UTIL_METHOD_GET_FRAME_USER_DATA_RESULT		0x110009

/* what each request sends ahead of its strings */
struct ReplaceAllHeader {
	bool fromSystem;
	bool isBgApp;
};

struct UpdateFrameHeader {
	SceInt32 frameXmlLen;
	SceUInt32 flag;
	bool fromSystem;
	bool isBgApp;
};

struct UpdateFrameListHeader {
	SceUInt32 frameNum;
	SceUInt32 frameXmlStrNum;
	SceUInt32 flag;
	bool fromSystem;
};

struct GetFrameRevisionHeader {
	bool isBgApp;
};

struct GetFrameUserDataHeader {
	SceUInt32 userDataSize;
	bool isBgApp;
};

/* the bytes before the terminator, counted inline */
static inline SceSize stringLength(const char *str)
{
	SceSize length = 0;

	while (*str != '\0') {
		str++;
		length++;
	}
	return length;
}

int liveAreaUtilRequestReplaceAll(IPMI::Client *client, int *pRequestId,
				  const IPMI::EventFlagInfo *pEventFlag, const char *dirpath,
				  const char *pOptionStr, const SceUInt64 *pOption,
				  bool fromSystem, bool isBgApp)
{
	ReplaceAllHeader header;
	IPMI::DataInfo in[4];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = dirpath;
	in[1].dataSize = stringLength(dirpath) + 1;
	in[2].pData = pOptionStr;
	in[2].dataSize = stringLength(pOptionStr) + 1;
	in[3].pData = pOption;
	in[3].dataSize = sizeof(*pOption);
	header.fromSystem = fromSystem;
	header.isBgApp = isBgApp;
	ret = client->invokeAsyncMethod(LIVEAREA_UTIL_METHOD_REPLACE_ALL, in, 4, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int liveAreaUtilGetReplaceAllResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(LIVEAREA_UTIL_METHOD_REPLACE_ALL_RESULT, requestId, &result,
				   NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int liveAreaUtilRequestUpdateFrame(IPMI::Client *client, int *pRequestId,
				   const IPMI::EventFlagInfo *pEventFlag, const char *formatVer,
				   const char *frameXmlStr, SceInt32 frameXmlLen,
				   const char *dirpathTop, SceUInt32 flag, const char *pOptionStr,
				   bool fromSystem, bool isBgApp)
{
	UpdateFrameHeader header;
	IPMI::DataInfo in[5];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = formatVer;
	in[1].dataSize = stringLength(formatVer) + 1;
	in[2].pData = frameXmlStr;
	in[2].dataSize = frameXmlLen;
	in[3].pData = dirpathTop;
	in[3].dataSize = stringLength(dirpathTop) + 1;
	in[4].pData = pOptionStr;
	in[4].dataSize = stringLength(pOptionStr) + 1;
	header.frameXmlLen = frameXmlLen;
	header.flag = flag;
	header.fromSystem = fromSystem;
	header.isBgApp = isBgApp;
	ret = client->invokeAsyncMethod(LIVEAREA_UTIL_METHOD_UPDATE_FRAME, in, 5, pRequestId,
					pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int liveAreaUtilGetUpdateFrameResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(LIVEAREA_UTIL_METHOD_UPDATE_FRAME_RESULT, requestId, &result,
				   NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

/* the header, the format version, one string per frame, the top directory and the option */
int liveAreaUtilRequestUpdateFrameList(IPMI::Client *client, int *pRequestId,
				       const IPMI::EventFlagInfo *pEventFlag,
				       const char *formatVer, const char **frameXmlStrList,
				       SceUInt32 frameNum, const char *dirpathTop, SceUInt32 flag,
				       const char *pOptionStr, bool fromSystem)
{
	UpdateFrameListHeader header;
	IPMI::DataInfo in[frameNum + 4];
	SceUInt32 count;
	SceUInt32 i;
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = formatVer;
	in[1].dataSize = stringLength(formatVer) + 1;
	count = 2;
	for (i = 0; i < frameNum; i++) {
		in[count].pData = frameXmlStrList[i];
		in[count].dataSize = stringLength(frameXmlStrList[i]) + 1;
		count++;
	}
	header.frameNum = frameNum;
	header.frameXmlStrNum = frameNum;
	in[count].pData = dirpathTop;
	in[count].dataSize = stringLength(dirpathTop) + 1;
	count++;
	header.flag = flag;
	in[count].pData = pOptionStr;
	in[count].dataSize = stringLength(pOptionStr) + 1;
	header.fromSystem = fromSystem;
	ret = client->invokeAsyncMethod(LIVEAREA_UTIL_METHOD_UPDATE_FRAME_LIST, in, frameNum + 4,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int liveAreaUtilGetUpdateFrameListResult(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(LIVEAREA_UTIL_METHOD_UPDATE_FRAME_LIST_RESULT, requestId,
				   &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int liveAreaUtilRequestGetFrameRevision(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *frameId,
					bool isBgApp)
{
	GetFrameRevisionHeader header;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = frameId;
	in[1].dataSize = stringLength(frameId) + 1;
	header.isBgApp = isBgApp;
	ret = client->invokeAsyncMethod(LIVEAREA_UTIL_METHOD_GET_FRAME_REVISION, in, 2,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int liveAreaUtilGetFrameRevisionResult(IPMI::Client *client, int requestId, SceInt64 *revision)
{
	IPMI::BufferInfo out[1];
	int result = 0;
	int ret;

	out[0].pBuffer = revision;
	out[0].bufferSize = sizeof(*revision);
	ret = client->tryGetResult(LIVEAREA_UTIL_METHOD_GET_FRAME_REVISION_RESULT, requestId,
				   &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int liveAreaUtilRequestGetFrameUserData(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *frameId,
					SceUInt32 userDataSize, bool isBgApp)
{
	GetFrameUserDataHeader header;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = frameId;
	in[1].dataSize = stringLength(frameId) + 1;
	header.userDataSize = userDataSize;
	header.isBgApp = isBgApp;
	ret = client->invokeAsyncMethod(LIVEAREA_UTIL_METHOD_GET_FRAME_USER_DATA, in, 2,
					pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

/* the shell writes the user data and its size */
int liveAreaUtilGetFrameUserDataResult(IPMI::Client *client, int requestId, char *userData,
				       SceUInt32 *pUserDataSize)
{
	IPMI::BufferInfo out[2];
	int result = 0;
	int ret;

	out[0].pBuffer = userData;
	out[0].bufferSize = *pUserDataSize;
	out[1].pBuffer = pUserDataSize;
	out[1].bufferSize = sizeof(*pUserDataSize);
	ret = client->tryGetResult(LIVEAREA_UTIL_METHOD_GET_FRAME_USER_DATA_RESULT, requestId,
				   &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}
