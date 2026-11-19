/*
 * SceVideoExport's methods of the media service (the names are ours; the library's files are not
 * named by any record). Each is a synchronous call, or the asynchronous Start with its result
 * taken later, and returns the call's error or the method's result.
 */
#include <stddef.h>
#include <scetypes.h>
#include "video_export_internal.h"

int videoExportIpcStart(IPMI::Client *client, int *pRequestId,
			const IPMI::EventFlagInfo *pEventFlag, SceUInt32 count,
			const VideoExportInputParam *pItems, SceUInt32 mode)
{
	SceUInt32 head[2];
	IPMI::DataInfo in[2];
	int ret;

	head[0] = count;
	in[0].pData = head;
	in[0].dataSize = sizeof(head);
	in[1].pData = pItems;
	in[1].dataSize = sizeof(VideoExportInputParam) * count;
	head[1] = mode;
	ret = client->invokeAsyncMethod(VIDEO_EXPORT_METHOD_START, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int videoExportIpcGetResult(IPMI::Client *client, int requestId, SceUInt32 *pCount,
			    VideoExportOutputParam *pResults)
{
	int result = 0;
	SceUInt32 count = *pCount;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pCount;
	out[0].bufferSize = sizeof(*pCount);
	out[1].pBuffer = pResults;
	out[1].bufferSize = sizeof(VideoExportOutputParam) * count;
	ret = client->tryGetResult(VIDEO_EXPORT_METHOD_GET_RESULT, requestId, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int videoExportIpcCancel(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(VIDEO_EXPORT_METHOD_CANCEL, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int videoExportIpcGetProgress(IPMI::Client *client, SceInt32 *pProgress)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pProgress;
	out[0].bufferSize = sizeof(*pProgress);
	ret = client->invokeSyncMethod(VIDEO_EXPORT_METHOD_GET_PROGRESS, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}
