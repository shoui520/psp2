/*
 * SceScreenShot (vs0:sys/external/libSceScreenShot.suprx), built by Sony with SNC 3.57. The client
 * of the screenshot service: it connects to the "SceScreenShot" IPMI service, sends the game's
 * titles, an overlay image and the commands, and reads the game's information out of a
 * screenshot file (a JPEG with an EXIF block whose maker note carries the titles).
 * No debug record names the types or the static functions; their names are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <screenshot.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "../../../../../lowlayer/src/ipmi/ipmi_error.h"
#include "screenshot_internal.h"

extern "C" int sceAppMgrSendNotificationRequest(const void *request, int flags);

/* the maker note's signature (compared up to 18 bytes) */
static const SceUInt8 screenShotMakerNoteSignature[20] = {
	'S', 'O', 'N', 'Y', ' ', 'P', 'S', 0, 0, 0, 0, 0, 'S', 'I', 'E', 1, 0, 0, 0, 0
};

/* the size of an EXIF value of each type */
static const SceInt32 screenShotTypeSizes[13] = { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };

static __attribute__((noinline)) SceInt32 screenShotConnect(ScreenShotSession *pSession);
static __attribute__((noinline)) SceInt32 screenShotDisconnect(ScreenShotSession *pSession);
static __attribute__((noinline)) void screenShotFree(ScreenShotSession *pSession, void *pData);
static __attribute__((noinline)) void *screenShotAlloc(ScreenShotSession *pSession, SceSize size);
static __attribute__((noinline)) SceInt32 screenShotSetParam(ScreenShotSession *pSession, const char *pPhotoTitle,
							     const char *pGameTitle, const char *pGameComment,
							     SceUInt8 flag);
static __attribute__((noinline)) SceInt32 screenShotSendOverlayData(ScreenShotSession *pSession, void *pData,
								    SceSize size, SceInt32 index);
static __attribute__((noinline)) SceInt32 screenShotSetOverlayOffset(ScreenShotSession *pSession, SceInt32 offsetX,
								     SceInt32 offsetY);
static __attribute__((noinline)) SceInt32 screenShotEnable(ScreenShotSession *pSession);
static __attribute__((noinline)) SceInt32 screenShotDisable(ScreenShotSession *pSession);
static __attribute__((noinline)) SceInt32 screenShotEnableNotification(ScreenShotSession *pSession);
static __attribute__((noinline)) SceInt32 screenShotDisableNotification(ScreenShotSession *pSession);
static __attribute__((noinline)) SceInt32 screenShotStartCapture(ScreenShotSession *pSession, SceInt32 mode);
static __attribute__((noinline)) SceInt32 screenShotGetCaptureResult(ScreenShotSession *pSession, void *pInfo);
static __attribute__((noinline)) SceInt32 screenShotCancelCapture(ScreenShotSession *pSession);

extern "C" SceInt32 sceScreenShotSetParam(const SceScreenShotParam *param)
{
	ScreenShotSession session;
	const char *pPhotoTitle;
	const char *pGameTitle;
	const char *pGameComment;
	SceInt32 result;

	if (param == NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (param->reserved != NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	pPhotoTitle = (const char *)param->photoTitle;
	pGameTitle = (const char *)param->gameTitle;
	pGameComment = (const char *)param->gameComment;
	if (param->photoTitle != NULL) {
		if (sceClibStrnlen(pPhotoTitle, 0x100) >= 0x100)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pPhotoTitle = "";
	}
	if (pGameTitle != NULL) {
		if (sceClibStrnlen(pGameTitle, 0x100) >= 0x100)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pGameTitle = "";
	}
	if (pGameComment != NULL) {
		if (sceClibStrnlen(pGameComment, 0x200) >= 0x200)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pGameComment = "";
	}
	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error = screenShotSetParam(&session, pPhotoTitle, pGameTitle, pGameComment, 0);

		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

extern "C" SceInt32 sceScreenShotSetOverlayImage(const SceChar8 *filePath, SceInt32 offsetX, SceInt32 offsetY)
{
	ScreenShotSession session;
	SceIoStat stat;
	SceSSize result;
	SceInt32 index, quotient, remainder, pieces;
	SceUID fd;
	SceSize rest;
	void *pBuffer;

	if (filePath == NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (offsetX < 0 || offsetX > 959)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (offsetY < 0 || offsetY > 543)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result != 0)
		return result;
	if (sceClibStrnlen((const char *)filePath, 0x400) >= 0x400)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (sceIoGetstat((const char *)filePath, &stat) != 0) {
		result = SCE_SCREENSHOT_ERROR_FILE_NOT_FOUND;
	} else {
		fd = sceIoOpen((const char *)filePath, SCE_O_RDONLY, 0400);
		if (fd < 0) {
			result = SCE_SCREENSHOT_ERROR_FILE_NOT_FOUND;
		} else {
			pBuffer = screenShotAlloc(&session, 0x4000);
			if (pBuffer != NULL) {
				quotient = stat.st_size / 0x4000;
				remainder = stat.st_size % 0x4000 != 0;
				pieces = quotient + remainder - 1;
				index = 0;
				if (pieces <= 0)
					goto last;
				index = 0;
				do {
					result = sceIoRead(fd, pBuffer, 0x4000);
					if (result < 0)
						break;
					result = screenShotSendOverlayData(&session, pBuffer, 0x4000, index);
					if (result != 0)
						break;
					index++;
				} while (index < pieces);
				if (result == 0) {
last:
					rest = (SceSize)stat.st_size - (index << 14);
					result = sceIoRead(fd, pBuffer, rest);
					if (result >= 0)
						result = screenShotSendOverlayData(&session, pBuffer, rest, index);
				}
				sceIoClose(fd);
				screenShotFree(&session, pBuffer);
			} else {
				sceIoClose(fd);
				result = SCE_SCREENSHOT_ERROR_NO_MEMORY;
			}
		}
	}
	if (result == 0)
		result = screenShotSetOverlayOffset(&session, offsetX, offsetY);
	screenShotDisconnect(&session);
	return result;
}

extern "C" SceInt32 sceScreenShotDisable(void)
{
	ScreenShotSession session;
	ScreenShotNotification notification;
	SceInt32 result;

	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error;

		notification.type = 0;
		sceClibStrncpy(notification.text, "ScreenShot: Disable", 0x3C);
		sceAppMgrSendNotificationRequest(&notification, 1);
		error = screenShotDisable(&session);
		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

extern "C" SceInt32 sceScreenShotEnable(void)
{
	ScreenShotSession session;
	ScreenShotNotification notification;
	SceInt32 result;

	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error;

		notification.type = 0;
		sceClibStrncpy(notification.text, "ScreenShot: Enable", 0x3C);
		sceAppMgrSendNotificationRequest(&notification, 1);
		error = screenShotEnable(&session);
		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

extern "C" SceInt32 sceScreenShotDisableNotification(void)
{
	ScreenShotSession session;
	SceInt32 result;

	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error = screenShotDisableNotification(&session);

		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

extern "C" SceInt32 sceScreenShotEnableNotification(void)
{
	ScreenShotSession session;
	SceInt32 result;

	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error = screenShotEnableNotification(&session);

		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

extern "C" SceInt32 sceScreenShotCapture(SceScreenShotCaptureMode mode, SceScreenShotCaptureFileInfo *captureFileInfo,
					 SceScreenShotCaptureCancelFunc cancelFunc, void *userdata)
{
	ScreenShotSession session;
	ScreenShotNotification notification;
	SceInt32 result;
	SceInt32 error;

	if (mode != SCE_SCREENSHOT_CAPTURE_MODE_NORMAL && mode != SCE_SCREENSHOT_CAPTURE_MODE_FORCE_CAPTURE)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (captureFileInfo == NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result != 0)
		return result;
	sceClibMemset(captureFileInfo, 0, 0x400);
	screenShotStartCapture(&session, mode);
	for (;;) {
		if (cancelFunc != NULL && cancelFunc(userdata) != 0) {
			screenShotCancelCapture(&session);
			error = SCE_SCREENSHOT_ERROR_CAPTURE_CANCELED;
			break;
		}
		sceKernelDelayThread(16000);
		error = screenShotGetCaptureResult(&session, captureFileInfo);
		if (error != IPMI_ERROR_WOULD_BLOCK)
			break;
	}
	screenShotDisconnect(&session);
	if (error != 0)
		sceClibMemset(captureFileInfo, 0, 0x400);
	if (error == SCE_SCREENSHOT_ERROR_SCREENSHOT_DISABLED) {
		notification.type = 0;
		sceClibStrncpy(notification.text, "ScreenShot:\nCapture() is failed. ERROR_SCREENSHOT_DISABLED", 0x3C);
		sceAppMgrSendNotificationRequest(&notification, 1);
	}
	return error;
}

extern "C" SceInt32 SceScreenShot_0757A17F(const SceScreenShotParam *param)
{
	ScreenShotSession session;
	const char *pPhotoTitle;
	const char *pGameTitle;
	const char *pGameComment;
	SceInt32 result;

	if (param == NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	if (param->reserved != NULL)
		return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	pPhotoTitle = (const char *)param->photoTitle;
	pGameTitle = (const char *)param->gameTitle;
	pGameComment = (const char *)param->gameComment;
	if (pPhotoTitle != NULL) {
		if (sceClibStrnlen(pPhotoTitle, 0x100) >= 0x100)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pPhotoTitle = "";
	}
	if (pGameTitle != NULL) {
		if (sceClibStrnlen(pGameTitle, 0x100) >= 0x100)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pGameTitle = "";
	}
	if (pGameComment != NULL) {
		if (sceClibStrnlen(pGameComment, 0x200) >= 0x200)
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		pGameComment = "";
	}
	memset(&session, 0, sizeof(session));
	result = screenShotConnect(&session);
	if (result == 0) {
		SceInt32 error = screenShotSetParam(&session, pPhotoTitle, pGameTitle, pGameComment, 1);

		screenShotDisconnect(&session);
		result = error;
	}
	return result;
}

/* the TIFF header of the EXIF block: the byte order and the first IFD's offset */
static __attribute__((noinline)) SceInt32 screenShotTiffHeader(SceUID fd, SceInt32 *pByteOrder, SceInt64 *pOffset)
{
	char header[8];
	SceUInt32 first, second, magic;
	SceInt32 order;

	if (sceIoRead(fd, header, 8) <= 0)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	if (header[0] == 'I' && header[1] == 'I') {
		order = SCREENSHOT_TIFF_LITTLE_ENDIAN;
		first = header[2];
		second = header[3] << 8;
		magic = header[2];
	} else {
		if (header[0] != 'M' || header[1] != 'M')
			return SCE_SCREENSHOT_ERROR_INVALID_DATA;
		order = SCREENSHOT_TIFF_BIG_ENDIAN;
		first = header[2] << 8;
		second = header[3];
		magic = header[3];
	}
	*pByteOrder = order;
	if ((SceUInt16)(first + second) == 42 || (SceUInt16)magic == 42) {
		SceUInt32 offset = *(const SceUInt32 *)&header[4];

		if (order == SCREENSHOT_TIFF_BIG_ENDIAN)
			offset = screenShotSwap32(offset);
		pOffset[0] = offset;
		return 0;
	}
	return SCE_SCREENSHOT_ERROR_INVALID_DATA;
}

/* the maker note: an ID3-like list of frames, of which two carry the game's title and comment */
static __attribute__((noinline)) SceInt32 screenShotMakerNote(SceUID fd, const ScreenShotIfdEntry *pEntry,
							      SceInt64 base, SceScreenShotGameInfo *pInfo)
{
	SceUInt8 header[0x12];
	SceUInt8 *data;
	SceInt32 result;
	SceUInt32 size, count, area, i;
	SceUInt32 position;
	const char *pText;
	SceUInt32 textTag;

	if ((SceInt32)sceIoLseek(fd, base + pEntry->value, SCE_SEEK_SET) < 0)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	if (pEntry->size >= 0x16) {
		if (sceIoRead(fd, header, 0x12) >= 0) {
			if (sceClibMemcmp(header, screenShotMakerNoteSignature, 0x12) != 0)
				result = SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_DATA;
			else
				result = 0;
		} else {
			result = SCE_SCREENSHOT_ERROR_INTERNAL;
		}
	} else {
		result = SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_DATA;
	}
	if (result != 0)
		return result;
	size = pEntry->size;
	if (size > 0x2000)
		size = 0x2000;
	SceUInt8 dataArea[size];
	data = dataArea;
	if (sceIoRead(fd, data, size) < 0)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	count = screenShotSwap16(*(const SceUInt16 *)&data[4]);
	area = screenShotSwap32(*(const SceUInt32 *)&data[0]);
	position = 6;
	if (count >= 100 || area >= (count << 13))
		return SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_DATA;
	pText = NULL;
	textTag = 0;
	for (i = 0; i < count; i++) {
		const SceUInt8 *pFrame;
		SceInt32 length, next, step;
		SceUInt32 tag, rest, frameSize;
		const SceUInt8 *pPayload;

		if (position + 10 > pEntry->size) {
			result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
			break;
		}
		rest = pEntry->size - position;
		pFrame = &data[position];
		frameSize = 0;
		tag = 0;
		if (rest < 10) {
			result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
			break;
		}
		if ((pFrame[7] & 0x80) != 0)
			length = (pFrame[4] << 24) | (pFrame[5] << 16) | (pFrame[6] << 8) | pFrame[7];
		else
			length = ((pFrame[4] & 0x7F) << 21) | ((pFrame[5] & 0x7F) << 14) | ((pFrame[6] & 0x7F) << 7) |
				 (pFrame[7] & 0x7F);
		next = length + 10;
		if ((SceUInt32)next > rest) {
			result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
			break;
		}
		sceClibMemcpy_safe(&tag, pFrame, 4);
		if ((SceUInt8)tag == 0) {
			result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
			break;
		}
		frameSize = length;
		pPayload = pFrame + 10;
		if (next >= -1) {
			if (length == -10 || length == -11) {
				result = SCE_SCREENSHOT_ERROR_INTERNAL;
				break;
			}
			step = length + 10;
		} else {
			step = -next;
		}
		position += step;
		if (screenShotSwap32(tag) == SCREENSHOT_FRAME_TXXX) {
			SceUInt32 kind = screenShotSwap32(*(const SceUInt32 *)(pPayload + 4));

			if (frameSize < 10) {
				result = SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_DATA;
				break;
			}
			if (kind == SCREENSHOT_TEXT_ALBM || kind == SCREENSHOT_TEXT_GDSC) {
				pText = (const char *)(pPayload + 9);
				textTag = kind;
			}
			if (textTag == SCREENSHOT_TEXT_GDSC)
				sceClibMemcpy_safe(pInfo->gameComment, pText, sceClibStrnlen(pText, 0x200));
			else if (textTag == SCREENSHOT_TEXT_ALBM)
				sceClibMemcpy_safe(pInfo->gameTitle, pText, sceClibStrnlen(pText, 0x100));
		}
	}
	return result;
}

/* an IFD: the maker note, the capture date and the Exif sub-IFD's offset */
static __attribute__((noinline)) SceInt32 screenShotIfd(SceUID fd, SceInt32 reserved, SceInt64 base, SceInt64 offset,
							SceInt32 order, ScreenShotIfdEntry *pEntry,
							SceScreenShotGameInfo *pInfo)
{
	struct {
		SceUInt16 tag;
		SceUInt16 type;
		SceUInt32 count;
		SceUInt32 value;
	} __attribute__((packed)) raw;
	SceUInt16 count;
	SceInt64 position, entryEnd;
	SceInt32 result = 0;
	SceInt32 i;
	SceSSize n;

	position = (SceInt32)sceIoLseek(fd, base + offset, SCE_SEEK_SET);
	if ((SceInt32)position < 0)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	count = 0;
	n = sceIoRead(fd, &count, 2);
	if (n <= 0)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entryEnd = position + n;
	if (order == SCREENSHOT_TIFF_BIG_ENDIAN)
		count = screenShotSwap16(count);
	for (i = 0; i < count; i++) {
		SceUInt32 value, number;
		SceUInt32 tag;
		SceInt32 type;

		if (sceIoRead(fd, &raw, 12) != 12)
			return SCE_SCREENSHOT_ERROR_INTERNAL;
		entryEnd += 12;
		sceClibMemset(pEntry, 0, 16);
		tag = raw.tag;
		type = raw.type;
		number = raw.count;
		value = raw.value;
		if (order == SCREENSHOT_TIFF_BIG_ENDIAN) {
			tag = screenShotSwap16(tag);
			type = screenShotSwap16(type);
		}
		pEntry->tag = tag;
		pEntry->type = type;
		if (type != 0 && type <= 12) {
			if (tag == 34665 && type == 4) {
				if (order == SCREENSHOT_TIFF_BIG_ENDIAN)
					value = screenShotSwap32(value);
				pEntry->value = value;
				break;
			}
			if (tag == 37500) {
				if (order == SCREENSHOT_TIFF_BIG_ENDIAN) {
					number = screenShotSwap32(number);
					value = screenShotSwap32(value);
				}
				pEntry->count = number;
				pEntry->value = value;
				pEntry->size = screenShotTypeSizes[type] * number;
				result = screenShotMakerNote(fd, pEntry, base, pInfo);
				if (result != 0)
					break;
				sceIoLseek(fd, entryEnd, SCE_SEEK_SET);
			} else if (tag == 36867) {
				SceUInt8 text[21];
				SceUInt16 date[6];
				const char *pNumber;
				SceInt32 j;

				if (order == SCREENSHOT_TIFF_BIG_ENDIAN) {
					number = screenShotSwap32(number);
					value = screenShotSwap32(value);
				}
				pEntry->count = number;
				pEntry->value = value;
				pEntry->size = screenShotTypeSizes[type] * number;
				sceClibMemset(text, 0, 0x15);
				sceIoLseek(fd, base + pEntry->value, SCE_SEEK_SET);
				if (sceIoRead(fd, text, 0x14) == 20) {
					char *pEnd;

					pNumber = (const char *)text;
					for (j = 0; j < 6; j++) {
						if (pNumber == NULL) {
							result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
							goto failed;
						}
						date[j] = sceClibStrtoll(pNumber, &pEnd, 0);
						pNumber = pEnd + 1;
					}
					pInfo->createdDate.year = date[0];
					pInfo->createdDate.month = date[1];
					pInfo->createdDate.day = date[2];
					pInfo->createdDate.hour = date[3];
					pInfo->createdDate.minute = date[4];
					pInfo->createdDate.second = date[5];
					pInfo->createdDate.microsecond = 0;
					result = 0;
				} else {
					result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
				}
failed:
				if (result != 0)
					break;
				sceIoLseek(fd, entryEnd, SCE_SEEK_SET);
			}
		}
	}
	return result;
}

extern "C" SceInt32 sceScreenShotGetParam(const SceChar8 *filePath, SceScreenShotGameInfo *param)
{
	SceUID fd;
	SceInt64 size, position, payload, next;
	SceUInt16 soi;
	SceUInt8 marker[2];
	SceInt32 result;
	SceUInt32 length;
	SceUInt8 foundEoi, foundScan;
	ScreenShotMarkerState state;

	fd = sceIoOpen((const char *)filePath, SCE_O_RDONLY, 0400);
	if (fd < 0)
		return SCE_SCREENSHOT_ERROR_FILE_NOT_FOUND;
	sceClibMemset(param->photoTitle, 0, 0x100);
	sceClibMemset(param->gameTitle, 0, 0x100);
	sceClibMemset(param->gameComment, 0, 0x200);
	state.ifdOffset = 0;
	size = sceIoLseek(fd, 0, SCE_SEEK_END);
	position = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (size < 0 || position < 0) {
		result = SCE_SCREENSHOT_ERROR_INTERNAL;
		goto out;
	}
	soi = 0;
	{
		SceSSize n = sceIoRead(fd, &soi, 2);

		if (n <= 0) {
			result = SCE_SCREENSHOT_ERROR_INTERNAL;
			goto out;
		}
		position += n;
	}
	soi = screenShotSwap16(soi);
	if (soi != 0xFFD8) {
		result = SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_FORMAT;
		goto out;
	}
	state.start = 2;
	result = 0;
	state.length = 0;
	foundScan = 0;
	foundEoi = 0;
	for (;;) {
		SceUInt32 kind;

		sceClibMemset(&state, 0, 0x10);
		if (sceIoRead(fd, marker, 2) != 2) {
			result = SCE_SCREENSHOT_ERROR_INTERNAL;
			break;
		}
		payload = position + 2;
		kind = marker[1] | (marker[0] << 8);
		if (payload + 2 > size) {
			result = SCE_SCREENSHOT_ERROR_INTERNAL;
			break;
		}
		if (kind != 0xFFD8 && kind != 0xFFD9) {
			state.start = payload;
			if (sceIoRead(fd, marker, 2) != 2) {
				result = SCE_SCREENSHOT_ERROR_INTERNAL;
				break;
			}
			length = marker[1] | (marker[0] << 8);
			state.length = length;
			position = payload + 2;
			if (length < 2) {
				result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
				break;
			}
			if (state.start + 2 != position) {
				SceInt32 seek = sceIoLseek(fd, state.start + 2, SCE_SEEK_SET);

				if (seek < 0) {
					result = SCE_SCREENSHOT_ERROR_INTERNAL;
					break;
				}
				position = seek;
			}
		}
		if (kind > 0xFFDA) {
			if (kind != 0xFFE1) {
				next = payload + state.length;
			} else if (state.length <= 0xFFFF) {
				char header[6];
				SceInt64 base = position + sceIoRead(fd, header, 6);

				result = SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_DATA;
				if (sceClibStrncmp(header, "Exif", 4) == 0) {
					SceInt32 order;
					SceInt64 ifdOffset[1];
					ScreenShotIfdEntry entry;

					result = screenShotTiffHeader(fd, &order, ifdOffset);
					if (result == 0) {
						entry.value = ifdOffset[0];
						result = screenShotIfd(fd, (SceInt32)(SceIntPtr)&entry, base, ifdOffset[0], order,
								       &entry, param);
						state.ifdOffset = entry.value;
						if (entry.value + base < state.start + state.length)
							result = screenShotIfd(fd, order, base, entry.value, order, &entry,
									       param);
					}
				}
				next = payload + state.length;
			} else {
				result = SCE_SCREENSHOT_ERROR_INVALID_DATA;
				next = payload + state.length;
			}
		} else if (kind <= 0xFFD8) {
			next = payload + state.length;
		} else {
			next = payload + state.length;
			if (kind <= 0xFFD9)
				foundEoi = 1;
			else
				foundScan = 1;
		}
		position = sceIoLseek(fd, next, SCE_SEEK_SET);
		if (position < 0) {
			result = SCE_SCREENSHOT_ERROR_INTERNAL;
			break;
		}
		if ((result | foundEoi | foundScan) != 0)
			break;
	}
	if (result == 0) {
		char *pName = sceClibStrrchr((const char *)filePath, '/') + 1;
		SceSize nameLength = sceClibStrnlen(pName, 0x105);
		char *pExtension = sceClibStrrchr(pName, '.');
		SceSize extensionLength = sceClibStrnlen(pExtension, 5);

		sceClibStrncpy((char *)param->photoTitle, pName, nameLength - extensionLength);
		param->photoTitle[255] = 0;
	}
out:
	sceIoClose(fd);
	return result;
}

static __attribute__((noinline)) void screenShotFree(ScreenShotSession *pSession, void *pData)
{
	sceClibMspaceFree(pSession->mspace, pData);
}

static __attribute__((noinline)) void *screenShotAlloc(ScreenShotSession *pSession, SceSize size)
{
	return sceClibMspaceMalloc(pSession->mspace, size);
}

static __attribute__((noinline)) SceInt32 screenShotConnect(ScreenShotSession *pSession)
{
	SceUID memblockId;
	void *pBase;
	void *mspace;
	void *pBuffer;
	SceInt32 result, connectResult, retry;
	SceSize bufferSize;

	if (pSession->pClient != NULL)
		return 0;
	memblockId = sceKernelAllocMemBlock("sceScreenshotHeap", SCREENSHOT_HEAP_MEMBLOCK_TYPE, 0x9000, NULL);
	if (memblockId < 0)
		return SCE_SCREENSHOT_ERROR_NO_MEMORY;
	sceKernelGetMemBlockBase(memblockId, &pBase);
	mspace = sceClibMspaceCreate(pBase, 0x9000);
	if (mspace == NULL) {
		sceKernelFreeMemBlock(memblockId);
		return SCE_SCREENSHOT_ERROR_NO_MEMORY;
	}
	pSession->memblockId = memblockId;
	pSession->mspace = mspace;
	IPMI::Client::Config config;

	sceClibStrncpy(config.serviceName, "SceScreenShot", sizeof(config.serviceName));
	config.maxSyncCallInSize = 0x4400;
	config.msgQueueCount = 1;
	config.msgQueueBufferSize[0] = 0x4010;
	config.budget = IPMI::USER_BUDGET;
	bufferSize = IPMI::Client::getBufferSize(&config);
	pBuffer = sceClibMspaceMalloc(pSession->mspace, bufferSize);
	if (pBuffer == NULL) {
		if (pSession->mspace != NULL) {
			sceClibMspaceDestroy(pSession->mspace);
			pSession->mspace = NULL;
		}
		if (pSession->memblockId != 0) {
			sceKernelFreeMemBlock(pSession->memblockId);
			pSession->memblockId = 0;
		}
		return SCE_SCREENSHOT_ERROR_NO_MEMORY;
	}
	pSession->pClientBuffer = sceClibMspaceMalloc(pSession->mspace, 0x4400);
	if (pSession->pClientBuffer == NULL) {
		sceClibMspaceFree(pSession->mspace, pBuffer);
		if (pSession->mspace != NULL) {
			sceClibMspaceDestroy(pSession->mspace);
			pSession->mspace = NULL;
		}
		if (pSession->memblockId != 0) {
			sceKernelFreeMemBlock(pSession->memblockId);
			pSession->memblockId = 0;
		}
		return SCE_SCREENSHOT_ERROR_NO_MEMORY;
	}
	result = IPMI::Client::create(&pSession->pClient, &config, pSession, pBuffer);
	if (result != 0) {
		sceClibMspaceFree(pSession->mspace, pBuffer);
		sceClibMspaceFree(pSession->mspace, pSession->pClientBuffer);
		pSession->pClientBuffer = NULL;
		pSession->pClient = NULL;
		if (pSession->mspace != NULL) {
			sceClibMspaceDestroy(pSession->mspace);
			pSession->mspace = NULL;
		}
		if (pSession->memblockId != 0) {
			sceKernelFreeMemBlock(pSession->memblockId);
			pSession->memblockId = 0;
		}
		return result;
	}
	for (retry = 0;; retry++) {
		result = pSession->pClient->connect(NULL, 0, &connectResult);
		if (result != IPMI_ERROR_SERVER_NOT_FOUND || retry >= 10)
			break;
		sceKernelDelayThread(1000000);
	}
	if (result == 0)
		return result;
	if (result <= IPMI_ERROR_CONNECTION_REFUSED) {
		if (result > IPMI_ERROR_CONNECTION_REFUSED - 1)
			result = connectResult;
	} else if (result == IPMI_ERROR_SERVER_NOT_FOUND) {
		result = SCE_SCREENSHOT_ERROR_INTERNAL;
	}
	pSession->pClient->destroy();
	sceClibMspaceFree(pSession->mspace, pBuffer);
	sceClibMspaceFree(pSession->mspace, pSession->pClientBuffer);
	pSession->pClientBuffer = NULL;
	pSession->pClient = NULL;
	if (pSession->mspace != NULL) {
		sceClibMspaceDestroy(pSession->mspace);
		pSession->mspace = NULL;
	}
	if (pSession->memblockId != 0) {
		sceKernelFreeMemBlock(pSession->memblockId);
		pSession->memblockId = 0;
	}
	return result;
}

static __attribute__((noinline)) SceInt32 screenShotDisconnect(ScreenShotSession *pSession)
{
	SceInt32 result;
	void *pBuffer;
	void *mspace;

	if (pSession->pClient == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	result = pSession->pClient->disconnect();
	if (result == 0) {
		result = pSession->pClient->destroy();
		if (result == 0) {
			sceClibMspaceFree(pSession->mspace, pSession->pClient);
			pBuffer = pSession->pClientBuffer;
			mspace = pSession->mspace;
			pSession->pClient = NULL;
			if (pBuffer != NULL) {
				sceClibMspaceFree(mspace, pBuffer);
				mspace = pSession->mspace;
				pSession->pClientBuffer = NULL;
			}
			if (mspace != NULL) {
				sceClibMspaceDestroy(mspace);
				pSession->mspace = NULL;
			}
			if (pSession->memblockId != 0) {
				sceKernelFreeMemBlock(pSession->memblockId);
				pSession->memblockId = 0;
			}
			return 0;
		}
	}
	return result;
}

static __attribute__((noinline)) SceInt32 screenShotIpcSetParam(IPMI::Client *pClient, const char *pPhotoTitle,
								const char *pGameTitle, const char *pGameComment,
								SceUInt8 flag);
static __attribute__((noinline)) SceInt32 screenShotIpcSendOverlayData(IPMI::Client *pClient, void *pData,
								       SceSize size, SceInt32 index);
static __attribute__((noinline)) SceInt32 screenShotIpcSetOverlayOffset(IPMI::Client *pClient, SceInt32 offsetX,
									SceInt32 offsetY);
static __attribute__((noinline)) SceInt32 screenShotIpcEnable(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 screenShotIpcDisable(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 screenShotIpcEnableNotification(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 screenShotIpcDisableNotification(IPMI::Client *pClient);
static __attribute__((noinline)) SceInt32 screenShotIpcStartCapture(IPMI::Client *pClient, SceInt32 *pRequestId,
								    const IPMI::EventFlagInfo *pEventFlag,
								    SceInt32 mode);
static __attribute__((noinline)) SceInt32 screenShotIpcGetCaptureResult(IPMI::Client *pClient, SceInt32 requestId,
									void *pInfo);
static __attribute__((noinline)) SceInt32 screenShotIpcCancelCapture(IPMI::Client *pClient);

static __attribute__((noinline)) SceInt32 screenShotSetParam(ScreenShotSession *pSession, const char *pPhotoTitle,
							     const char *pGameTitle, const char *pGameComment,
							     SceUInt8 flag)
{
	return screenShotIpcSetParam(pSession->pClient, pPhotoTitle, pGameTitle, pGameComment, flag);
}

static __attribute__((noinline)) SceInt32 screenShotSendOverlayData(ScreenShotSession *pSession, void *pData,
								    SceSize size, SceInt32 index)
{
	return screenShotIpcSendOverlayData(pSession->pClient, pData, size, index);
}

static __attribute__((noinline)) SceInt32 screenShotSetOverlayOffset(ScreenShotSession *pSession, SceInt32 offsetX,
								     SceInt32 offsetY)
{
	return screenShotIpcSetOverlayOffset(pSession->pClient, offsetX, offsetY);
}

static __attribute__((noinline)) SceInt32 screenShotEnable(ScreenShotSession *pSession)
{
	return screenShotIpcEnable(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 screenShotDisable(ScreenShotSession *pSession)
{
	return screenShotIpcDisable(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 screenShotEnableNotification(ScreenShotSession *pSession)
{
	return screenShotIpcEnableNotification(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 screenShotDisableNotification(ScreenShotSession *pSession)
{
	return screenShotIpcDisableNotification(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 screenShotStartCapture(ScreenShotSession *pSession, SceInt32 mode)
{
	return screenShotIpcStartCapture(pSession->pClient, &pSession->requestId, NULL, mode);
}

static __attribute__((noinline)) SceInt32 screenShotGetCaptureResult(ScreenShotSession *pSession, void *pInfo)
{
	return screenShotIpcGetCaptureResult(pSession->pClient, pSession->requestId, pInfo);
}

static __attribute__((noinline)) SceInt32 screenShotCancelCapture(ScreenShotSession *pSession)
{
	return screenShotIpcCancelCapture(pSession->pClient);
}

static __attribute__((noinline)) SceInt32 screenShotIpcSetParam(IPMI::Client *pClient, const char *pPhotoTitle,
								const char *pGameTitle, const char *pGameComment,
								SceUInt8 flag)
{
	SceInt32 result = 0;
	SceUInt8 value;
	IPMI::DataInfo buffers[4];
	SceSize length;

	buffers[0].pData = &value;
	buffers[0].dataSize = 1;
	buffers[1].pData = pPhotoTitle;
	for (length = 0; pPhotoTitle[length] != 0; length++)
		;
	buffers[1].dataSize = length + 1;
	buffers[2].pData = pGameTitle;
	for (length = 0; pGameTitle[length] != 0; length++)
		;
	buffers[2].dataSize = length + 1;
	buffers[3].pData = pGameComment;
	for (length = 0; pGameComment[length] != 0; length++)
		;
	value = flag;
	buffers[3].dataSize = length + 1;
	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_SET_PARAM, buffers, 4, &result,
						   (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcSendOverlayData(IPMI::Client *pClient, void *pData,
								       SceSize size, SceInt32 index)
{
	SceInt32 result = 0;
	SceInt32 header[2];
	IPMI::DataInfo buffers[2];

	header[0] = size;
	header[1] = index;
	buffers[0].pData = header;
	buffers[0].dataSize = 8;
	buffers[1].pData = pData;
	buffers[1].dataSize = 0x4000;
	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_SEND_OVERLAY_DATA, buffers, 2, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcSetOverlayOffset(IPMI::Client *pClient, SceInt32 offsetX,
									SceInt32 offsetY)
{
	SceInt32 result = 0;
	SceInt32 offset[2];
	IPMI::DataInfo buffers[1];

	offset[0] = offsetX;
	offset[1] = offsetY;
	buffers[0].pData = offset;
	buffers[0].dataSize = 8;
	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_SET_OVERLAY_OFFSET, buffers, 1, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcEnable(IPMI::Client *pClient)
{
	SceInt32 result = 0;

	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_ENABLE, (const IPMI::DataInfo *)NULL, 0, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcDisable(IPMI::Client *pClient)
{
	SceInt32 result = 0;

	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_DISABLE, (const IPMI::DataInfo *)NULL, 0, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcEnableNotification(IPMI::Client *pClient)
{
	SceInt32 result = 0;

	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_ENABLE_NOTIFICATION, (const IPMI::DataInfo *)NULL, 0, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcDisableNotification(IPMI::Client *pClient)
{
	SceInt32 result = 0;

	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_DISABLE_NOTIFICATION, (const IPMI::DataInfo *)NULL, 0, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcStartCapture(IPMI::Client *pClient, SceInt32 *pRequestId,
								    const IPMI::EventFlagInfo *pEventFlag,
								    SceInt32 mode)
{
	SceInt32 value = mode;
	IPMI::DataInfo buffers[1];
	SceInt32 result;

	buffers[0].pData = &value;
	buffers[0].dataSize = 4;
	result = pClient->invokeAsyncMethod(SCREENSHOT_METHOD_START_CAPTURE, buffers, 1, pRequestId, pEventFlag);
	if (result == 0)
		result = 0;
	return result;
}

static __attribute__((noinline)) SceInt32 screenShotIpcGetCaptureResult(IPMI::Client *pClient, SceInt32 requestId,
									void *pInfo)
{
	SceInt32 result = 0;
	IPMI::BufferInfo buffers[1];

	buffers[0].pBuffer = pInfo;
	buffers[0].bufferSize = 1024;
	SceInt32 error = pClient->tryGetResult(SCREENSHOT_METHOD_GET_CAPTURE_RESULT, requestId, &result, buffers, 1);

	if (error == 0)
		error = result;
	return error;
}

static __attribute__((noinline)) SceInt32 screenShotIpcCancelCapture(IPMI::Client *pClient)
{
	SceInt32 result = 0;

	SceInt32 error = pClient->invokeSyncMethod(SCREENSHOT_METHOD_CANCEL_CAPTURE, (const IPMI::DataInfo *)NULL, 0, &result, (IPMI::BufferInfo *)NULL, 0);

	if (error == 0)
		error = result;
	return error;
}
