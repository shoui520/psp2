/*
 * SceNpWebApi: the push service's IPC packets (Sony's np_push/ipc/np_push_ipc_tlv.cpp). A packet is
 * made of {type, length} headers in network order, each followed by its data. The names are ours.
 */

#include "np_webapi_internal.h"

#define NP_PUSH_IPC_HEADER_SIZE		4

/* the header a packet begins with: it must be of the type expected, and the packet must hold the length it names */
int npPushIpcReadHeader(unsigned short expected, unsigned short *length, const char *src, unsigned int size,
	unsigned int *consumed)
{
	unsigned short type;
	unsigned short net16;
	unsigned short host16;
	int result;

	if (size >= NP_PUSH_IPC_HEADER_SIZE) {
		sceClibMemcpy_safe(&type, src, 2);
		host16 = sceNetNtohs(type);
		sceClibMemcpy_safe(&net16, src + 2, 2);
		result = sceNetNtohs(net16);
		*length = result;
		if (consumed != NULL)
			*consumed = NP_PUSH_IPC_HEADER_SIZE;
		if (host16 == expected) {
			if (size >= (unsigned int)(result + NP_PUSH_IPC_HEADER_SIZE))
				result = 0;
			else
				result = NP_PUSH_IPC_ERROR_INVALID_DATA;
		} else {
			result = NP_PUSH_IPC_ERROR_INVALID_DATA;
		}
	} else {
		result = NP_PUSH_IPC_ERROR_INVALID_DATA;
	}
	return result;
}

int npPushIpcReadTypeLength(unsigned short *type, unsigned short *length, const char *src, unsigned int size,
	unsigned int *consumed)
{
	unsigned short net16;
	unsigned short host16;
	int result;

	if (size >= NP_PUSH_IPC_HEADER_SIZE) {
		sceClibMemcpy_safe(&net16, src, 2);
		*type = sceNetNtohs(net16);
		sceClibMemcpy_safe(&host16, src + 2, 2);
		*length = sceNetNtohs(host16);
		if (consumed != NULL)
			*consumed = NP_PUSH_IPC_HEADER_SIZE;
		result = 0;
	} else {
		result = NP_PUSH_IPC_ERROR_INVALID_DATA;
	}
	return result;
}

/* how much a header and its data take */
int npPushIpcSkip(const char *src, unsigned int size, unsigned int *consumed)
{
	unsigned short net16;
	unsigned short host16;
	unsigned int total;
	int result;

	if (size >= NP_PUSH_IPC_HEADER_SIZE) {
		sceClibMemcpy_safe(&net16, src, 2);
		sceNetNtohs(net16);
		sceClibMemcpy_safe(&host16, src + 2, 2);
		result = sceNetNtohs(host16);
		if (consumed != NULL)
			*consumed = NP_PUSH_IPC_HEADER_SIZE;
		total = result + NP_PUSH_IPC_HEADER_SIZE;
		if (size >= total) {
			*consumed = total;
			result = 0;
		} else {
			result = NP_PUSH_IPC_ERROR_INVALID_DATA;
		}
	} else {
		result = NP_PUSH_IPC_ERROR_INVALID_DATA;
	}
	return result;
}

const void *const g_ipcTlvVtable[] = {		/* 0x8100f098 */
	(const void *)npPushIpcTlvDestruct,
	(const void *)npPushIpcTlvDelete,
	(const void *)npPushIpcTlvPure,
	(const void *)npPushIpcTlvPure,
	(const void *)npPushIpcTlvPure,
	NULL,
	NULL,
};

const void *const g_ipcTlvStringVtable[] = {	/* 0x8100f0b4 */
	(const void *)npPushIpcTlvStringDestruct,
	(const void *)npPushIpcTlvStringDelete,
	(const void *)npPushIpcTlvStringGetLength,
	(const void *)npPushIpcTlvStringSerialize,
	(const void *)npPushIpcTlvStringUnserialize,
	NULL,
	NULL,
};

NpPushIpcTlv *npPushIpcTlvDestruct(NpPushIpcTlv *tlv)
{
	tlv->vtable = g_ipcTlvVtable;
	SceNpCommonPs4_BA23AD76(tlv);
	return tlv;
}

int npPushIpcTlvDelete(NpPushIpcTlv *tlv)
{
	tlv->vtable = g_ipcTlvVtable;
	SceNpCommonPs4_BA23AD76(tlv);
	return SceNpCommonPs4_BB5282C8(tlv);
}

void npPushIpcTlvPure(void)
{
	__builtin_trap();
}

NpPushIpcTlv *npPushIpcTlvStringConstruct(NpPushIpcTlv *tlv, unsigned short type)
{
	SceNpCommonPs4_108738F2(tlv);
	tlv->type = type;
	tlv->vtable = g_ipcTlvStringVtable;
	tlv->data = NULL;
	return tlv;
}

NpPushIpcTlv *npPushIpcTlvStringDestruct(NpPushIpcTlv *tlv)
{
	tlv->vtable = g_ipcTlvVtable;
	SceNpCommonPs4_BA23AD76(tlv);
	return tlv;
}

int npPushIpcTlvStringDelete(NpPushIpcTlv *tlv)
{
	tlv->vtable = g_ipcTlvVtable;
	SceNpCommonPs4_BA23AD76(tlv);
	return SceNpCommonPs4_BB5282C8(tlv);
}

/* the text and its terminator */
SceSize npPushIpcTlvStringGetLength(NpPushIpcTlv *tlv)
{
	const char *data;

	data = tlv->data;
	if (data != NULL)
		return sceClibStrnlen(data, 0xFFFFFFFF) + 1;
	return 0;
}

int npPushIpcTlvStringSerialize(NpPushIpcTlv *tlv, char *dst, unsigned int size, SceSize *written)
{
	unsigned short src;
	unsigned short host16;
	unsigned short type;
	int headerSize;
	const char *data;
	SceSize textSize;
	int result;

	type = tlv->type;
	host16 = ((unsigned short (*)(NpPushIpcTlv *))tlv->vtable[2])(tlv);
	src = type;
	if (dst != NULL && size < NP_PUSH_IPC_HEADER_SIZE) {
		result = NP_PUSH_IPC_ERROR_BUFFER_TOO_SMALL;
	} else {
		src = sceNetHtons(type);
		if (dst != NULL)
			sceClibMemcpy_safe(dst, &src, 2);
		host16 = sceNetHtons(host16);
		if (dst != NULL)
			sceClibMemcpy_safe(dst + 2, &host16, 2);
		headerSize = NP_PUSH_IPC_HEADER_SIZE;
		data = tlv->data;
		textSize = 0;
		if (data != NULL) {
			textSize = sceClibStrnlen(data, 0xFFFFFFFF) + 1;
			data = tlv->data;
		}
		if (dst != NULL && size - headerSize < textSize) {
			result = NP_PUSH_IPC_ERROR_BUFFER_TOO_SMALL;
		} else {
			if (dst != NULL) {
				sceClibMemcpy_safe(dst + headerSize, data, textSize);
				dst[textSize + headerSize] = 0;
			}
			if (written != NULL)
				*written = textSize + headerSize;
			result = 0;
		}
	}
	return result;
}

int npPushIpcTlvStringUnserialize(NpPushIpcTlv *tlv, char *src, unsigned int size, unsigned int *consumed)
{
	unsigned short length;
	unsigned int headerSize;
	const char *text;
	int textLength;
	int result;

	result = npPushIpcReadHeader(tlv->type, &length, src, size, &headerSize);
	if (result >= 0) {
		if (length != 0) {
			if (size - headerSize < length)
				SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "Unserialize",
					"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\np_push\\ipc\\np_push_ipc_tlv.cpp", 219,
					"ASSERT", "dataSize-total >= len"));
			text = src + 4;
			src[length + 3] = 0;
			textLength = length - 1;
		} else {
			text = NULL;
			textLength = 0;
		}
		tlv->data = text;
		tlv->length = textLength;
		if (consumed != NULL)
			*consumed = length + 4;
		result = 0;
	}
	return result;
}
