/*
 * SceMusicExport's methods of the media service (the names are ours; the library's files are not
 * named by any record). Each is a synchronous call, or the asynchronous Start with its result
 * taken later, and returns the call's error or the method's result.
 */
#include <stddef.h>
#include <scetypes.h>
#include "music_export_internal.h"

int musicExportIpcStart(IPMI::Client *client, int *pRequestId,
			const IPMI::EventFlagInfo *pEventFlag, SceUInt32 count,
			const MusicExportItem *pItems, SceUInt32 mode)
{
	SceUInt32 head[2];
	IPMI::DataInfo in[2];
	int ret;

	head[0] = count;
	in[0].pData = head;
	in[0].dataSize = sizeof(head);
	in[1].pData = pItems;
	in[1].dataSize = sizeof(MusicExportItem) * count;
	head[1] = mode;
	ret = client->invokeAsyncMethod(MUSIC_EXPORT_METHOD_START, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

int musicExportIpcGetResult(IPMI::Client *client, int requestId, SceUInt32 *pCount,
			    MusicExportResult *pResults)
{
	int result = 0;
	SceUInt32 count = *pCount;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pCount;
	out[0].bufferSize = sizeof(*pCount);
	out[1].pBuffer = pResults;
	out[1].bufferSize = sizeof(MusicExportResult) * count;
	ret = client->tryGetResult(MUSIC_EXPORT_METHOD_GET_RESULT, requestId, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

int musicExportIpcCancel(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(MUSIC_EXPORT_METHOD_CANCEL, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int musicExportIpcGetProgress(IPMI::Client *client, SceInt32 *pProgress)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pProgress;
	out[0].bufferSize = sizeof(*pProgress);
	ret = client->invokeSyncMethod(MUSIC_EXPORT_METHOD_GET_PROGRESS, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int musicExportIpcSendData(IPMI::Client *client, const void *pData, SceUInt32 size,
			   SceUInt32 index, SceUInt8 last, SceOff total, const char *pExtension,
			   SceUInt32 reserved, const char *pPath)
{
	int result = 0;
	MusicExportDataHeader header;
	IPMI::DataInfo in[4];
	SceSize length;
	int ret;

	header.size = size;
	header.index = index;
	header.last = last;
	header.total = total;
	header.reserved = reserved;
	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = pData;
	in[1].dataSize = size;
	in[2].pData = pExtension;
	for (length = 0; pExtension[length] != '\0'; length++)
		;
	in[2].dataSize = length + 1;
	in[3].pData = pPath;
	for (length = 0; pPath[length] != '\0'; length++)
		;
	in[3].dataSize = length + 1;
	ret = client->invokeSyncMethod(MUSIC_EXPORT_METHOD_SEND_DATA, in, 4, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
