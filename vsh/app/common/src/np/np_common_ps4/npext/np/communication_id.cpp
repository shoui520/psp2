/*
 * SceNpCommonPs4: a communication id (a title's NPWR12345_00 name and its number) behind a vtable.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_communicationIdVtable[];	/* 0x8102a900 */

#define IS_COMMUNICATION_ID_STRING(string) \
	((sceClibLookCtypeTable((string)[0]) & CTYPE_ALPHA) != 0\
		&& (sceClibLookCtypeTable((string)[1]) & CTYPE_ALPHA) != 0\
		&& (sceClibLookCtypeTable((string)[2]) & CTYPE_ALPHA) != 0\
		&& (sceClibLookCtypeTable((string)[3]) & CTYPE_ALPHA) != 0\
		&& (sceClibLookCtypeTable((string)[4]) & CTYPE_DIGIT) != 0\
		&& (sceClibLookCtypeTable((string)[5]) & CTYPE_DIGIT) != 0\
		&& (sceClibLookCtypeTable((string)[6]) & CTYPE_DIGIT) != 0\
		&& (sceClibLookCtypeTable((string)[7]) & CTYPE_DIGIT) != 0\
		&& (sceClibLookCtypeTable((string)[8]) & CTYPE_DIGIT) != 0\
		&& (string)[9] == '_'\
		&& (sceClibLookCtypeTable((string)[10]) & CTYPE_DIGIT) != 0\
		&& (sceClibLookCtypeTable((string)[11]) & CTYPE_DIGIT) != 0)

int SceNpCommonPs4_54F72557(struct NpCommonPs4CommunicationId *communicationId, const char *string)
{
	int result;

	if (sceClibStrnlen(string, 0xFFFFFFFF) != 12 || !IS_COMMUNICATION_ID_STRING(string))
		return NP_COMMON_PS4_ERROR_80559E11;
	result = SceNpCommon_5BC54899(&communicationId->data, string);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_5A8057AC(struct NpCommonPs4CommunicationId *communicationId, const char *string, int length)
{
	char buffer[16];
	int result;

	if (length == 12 && IS_COMMUNICATION_ID_STRING(string)) {
		sceClibMemset(buffer, 0, 13);
		sceClibMemcpy_safe(buffer, string, 12);
		result = SceNpCommon_5BC54899(&communicationId->data, buffer);
		if (result >= 0)
			result = 0;
	} else {
		result = NP_COMMON_PS4_ERROR_80559E11;
	}
	return result;
}

int SceNpCommonPs4_7BC99BF3(struct NpCommonPs4CommunicationId *communicationId, void *buffer, SceSize size)
{
	int result;

	sceClibMemset(buffer, 0, size);
	result = SceNpCommon_E426FEE8(&communicationId->data, (char *)buffer, size);
	if (result >= 0)
		return 0;
	return result;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_79576C83(struct NpCommonPs4CommunicationId *communicationId)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemset(communicationId->data, 0, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_68A64BBC(struct NpCommonPs4CommunicationId *communicationId)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemset(communicationId->data, 0, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_4BB7A7F9(struct NpCommonPs4CommunicationId *communicationId,
	const struct NpCommonPs4CommunicationId *source)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemcpy_safe(communicationId->data, source->data, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_C8E7B7DB(struct NpCommonPs4CommunicationId *communicationId,
	const struct NpCommonPs4CommunicationId *source)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemcpy_safe(communicationId->data, source->data, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_A0847F85(struct NpCommonPs4CommunicationId *communicationId, const void *data)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemcpy_safe(communicationId->data, data, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_5D4D9D58(struct NpCommonPs4CommunicationId *communicationId, const void *data)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_communicationIdVtable);
	sceClibMemcpy_safe(communicationId->data, data, 12);
	return communicationId;
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_511C5E91(struct NpCommonPs4CommunicationId *communicationId)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return communicationId;
}

int SceNpCommonPs4_44A37ED5(struct NpCommonPs4CommunicationId *communicationId)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(communicationId);
}

struct NpCommonPs4CommunicationId *SceNpCommonPs4_CF9D698A(struct NpCommonPs4CommunicationId *communicationId)
{
	communicationId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return communicationId;
}

void *SceNpCommonPs4_EBC4EF55(struct NpCommonPs4CommunicationId *communicationId)
{
	return sceClibMemset(communicationId->data, 0, 12);
}

int SceNpCommonPs4_0F8CD52B(const struct NpCommonPs4CommunicationId *left, const struct NpCommonPs4CommunicationId *right)
{
	int result;

	result = 0;
	if (sceClibStrncmp(left->data, right->data, 9) == 0)
		return left->num == right->num;
	return result;
}

const void *g_communicationIdVtable[] = {	/* 0x8102a900 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_511C5E91,
	(const void *)SceNpCommonPs4_44A37ED5,
};

}
