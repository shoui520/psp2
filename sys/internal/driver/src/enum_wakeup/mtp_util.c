/* SceEnumWakeUp: MTP dataset packing and responses. */
#include "ew_internal.h"

char *mtpStrcpy(char *dst, const char *src)
{
	char *d = dst;

	if (src == NULL || dst == NULL)
		return NULL;
	while ((*d++ = *src++) != '\0')
		;
	return dst;
}

int mtpSetResponse(SceUInt16 code, const SceUInt32 *params, SceUInt32 nparams,
			  const MtpContainer *request, MtpContainer *response)
{
	response->code = code;
	response->nparams = nparams;
	response->sessionId = request->sessionId;
	response->transactionId = request->transactionId;
	if (params && nparams) {
		response->params[0] = params[0];
		if (nparams > 1) {
			response->params[1] = params[1];
			if (nparams > 2) {
				response->params[2] = params[2];
				if (nparams > 3) {
					response->params[3] = params[3];
					if (nparams > 4)
						response->params[4] = params[4];
				}
			}
		}
	}
	return 0;
}

/* pack a string as UTF-16; returns the bytes written with the NUL */
int mtpPackString(MtpString *out, const char *str)
{
	SceUInt16 *tmp, *end;
	SceSize room = out->length;
	int ret;

	if (str == NULL || *str == '\0') {
		out->length = 0;
		return 0;
	}
	tmp = SceSysmemForDriver_49D4DD9B(ewg.heap, room * 2, &ewg.allocOpt);
	if (tmp == NULL)
		return MTP_ERROR_NO_MEMORY;
	ret = mtpUtf8ToUtf16(tmp, room * 2, (const SceUInt8 *)str);
	if (ret >= 0) {
		out->length = ret + 1;
		for (end = tmp; end < tmp + room && *end != 0; end++)
			;
		ret = (char *)end - (char *)tmp + 2;
		memcpy(out->buf, tmp, ret);
	}
	sceKernelFreeHeapMemory(ewg.heap, tmp);
	return ret;
}

/* pack a counted array of 1, 2, 4 or 8 byte elements */
int mtpPackArray8(void *out, const MtpArray *in)
{
	MtpWireArray *array = out;
	SceUInt32 count = in->count;
	SceSize size = count;

	__builtin_memcpy(&array->count, &count, sizeof(count));
	memcpy(array->elements, in->data, size);
	return sizeof(*array) + size;
}

int mtpPackArray16(void *out, const MtpArray *in)
{
	MtpWireArray *array = out;
	SceUInt32 count = in->count;
	SceSize size = count * 2;

	__builtin_memcpy(&array->count, &count, sizeof(count));
	memcpy(array->elements, in->data, size);
	return sizeof(*array) + size;
}

int mtpPackArray32(void *out, const MtpArray *in)
{
	MtpWireArray *array = out;
	SceUInt32 count = in->count;
	SceSize size = count * 4;

	__builtin_memcpy(&array->count, &count, sizeof(count));
	memcpy(array->elements, in->data, size);
	return sizeof(*array) + size;
}

int mtpPackArray64(void *out, const MtpArray *in)
{
	MtpWireArray *array = out;
	SceUInt32 count = in->count;
	SceSize size = count * 8;

	__builtin_memcpy(&array->count, &count, sizeof(count));
	memcpy(array->elements, in->data, size);
	return sizeof(*array) + size;
}
