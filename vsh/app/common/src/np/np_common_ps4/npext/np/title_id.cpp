/*
 * SceNpCommonPs4: a title id (NPXX12345_00) behind a vtable.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_titleIdVtable[];		/* 0x8102a920 */

/* the nine characters that name the title: four letters and five digits */
#define IS_TITLE_NAME(string) \
	((sceClibLookCtypeTable((string)[0]) & CTYPE_ALPHA) != 0 \
		&& (sceClibLookCtypeTable((string)[1]) & CTYPE_ALPHA) != 0 \
		&& (sceClibLookCtypeTable((string)[2]) & CTYPE_ALPHA) != 0 \
		&& (sceClibLookCtypeTable((string)[3]) & CTYPE_ALPHA) != 0 \
		&& (sceClibLookCtypeTable((string)[4]) & CTYPE_DIGIT) != 0 \
		&& (sceClibLookCtypeTable((string)[5]) & CTYPE_DIGIT) != 0 \
		&& (sceClibLookCtypeTable((string)[6]) & CTYPE_DIGIT) != 0 \
		&& (sceClibLookCtypeTable((string)[7]) & CTYPE_DIGIT) != 0 \
		&& (sceClibLookCtypeTable((string)[8]) & CTYPE_DIGIT) != 0)

int SceNpCommonPs4_D35E42F8(struct NpCommonPs4TitleId *titleId, const char *string, int length)
{
	if (length != 12 || !IS_TITLE_NAME(string) || string[9] != '_'
		|| (sceClibLookCtypeTable(string[10]) & CTYPE_DIGIT) == 0 || (sceClibLookCtypeTable(string[11]) & CTYPE_DIGIT) == 0)
		return NP_COMMON_PS4_ERROR_80559E11;
	sceClibMemset(titleId->id, 0, 16);
	sceClibMemset(titleId->id, 0, 16);
	sceClibStrncpy(titleId->id, string, 9);
	titleId->id[12] = 0;
	titleId->id[9] = '_';
	titleId->id[11] = '0';
	titleId->id[10] = '0';
	return 0;
}

int SceNpCommonPs4_295B8FDF(struct NpCommonPs4TitleId *titleId, const char *string)
{
	SceSize length;

	length = sceClibStrnlen(string, 0xFFFFFFFF);
	return SceNpCommonPs4_D35E42F8(titleId, string, length);
}

int SceNpCommonPs4_D80EDCFC(const struct NpCommonPs4TitleId *titleId, void *buffer, SceSize size)
{
	if (size < 13)
		return NP_COMMON_PS4_ERROR_80559E0D;
	sceClibMemset(buffer, 0, size);
	sceClibMemcpy_safe(buffer, titleId->id, 13);
	return 0;
}

int SceNpCommonPs4_EBCC0F58(struct NpCommonPs4TitleId *titleId, const char *string, int length)
{
	if (length != 9 || !IS_TITLE_NAME(string))
		return NP_COMMON_PS4_ERROR_80559E11;
	sceClibMemset(titleId->id, 0, 16);
	sceClibMemset(titleId->id, 0, 16);
	sceClibStrncpy(titleId->id, string, 9);
	titleId->id[12] = 0;
	titleId->id[9] = '_';
	titleId->id[11] = '0';
	titleId->id[10] = '0';
	return 0;
}

int SceNpCommonPs4_EF49526B(struct NpCommonPs4TitleId *titleId, const char *string)
{
	SceSize length;

	length = sceClibStrnlen(string, 0xFFFFFFFF);
	return SceNpCommonPs4_EBCC0F58(titleId, string, length);
}

struct NpCommonPs4TitleId *SceNpCommonPs4_93790593(struct NpCommonPs4TitleId *titleId)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemset(titleId->id, 0, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_03186D84(struct NpCommonPs4TitleId *titleId)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemset(titleId->id, 0, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_266E3DA9(struct NpCommonPs4TitleId *titleId, const struct NpCommonPs4TitleId *source)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemcpy_safe(titleId->id, source->id, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_1668A3AC(struct NpCommonPs4TitleId *titleId, const struct NpCommonPs4TitleId *source)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemcpy_safe(titleId->id, source->id, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_DAA742B0(struct NpCommonPs4TitleId *titleId, const void *data)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemcpy_safe(titleId->id, data, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_9B806F45(struct NpCommonPs4TitleId *titleId, const void *data)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_titleIdVtable);
	sceClibMemcpy_safe(titleId->id, data, 16);
	return titleId;
}

struct NpCommonPs4TitleId *SceNpCommonPs4_57005284(struct NpCommonPs4TitleId *titleId)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return titleId;
}

int SceNpCommonPs4_B9600F4B(struct NpCommonPs4TitleId *titleId)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(titleId);
}

struct NpCommonPs4TitleId *SceNpCommonPs4_E4853E32(struct NpCommonPs4TitleId *titleId)
{
	titleId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return titleId;
}

int SceNpCommonPs4_AFDC8D8E(const struct NpCommonPs4TitleId *left, const struct NpCommonPs4TitleId *right)
{
	return sceClibStrncmp(left->id, right->id, 12) == 0;
}

int SceNpCommonPs4_FE3FCDF0(const struct NpCommonPs4TitleId *titleId)
{
	char zero[16];
	int result;

	sceClibMemset(zero, 0, 16);
	result = sceClibStrncmp(titleId->id, zero, 12) == 0;
	return result;
}

int SceNpCommonPs4_61B1483C(const struct NpCommonPs4TitleIdData *left, const struct NpCommonPs4TitleId *right)
{
	return sceClibStrncmp(left->id, right->id, 12) == 0;
}

int SceNpCommonPs4_213E50AB(const struct NpCommonPs4TitleId *left, const struct NpCommonPs4TitleIdData *right)
{
	return sceClibStrncmp(left->id, right->id, 12) == 0;
}

int SceNpCommonPs4_0B9FCE06(const struct NpCommonPs4TitleId *left, const struct NpCommonPs4TitleId *right)
{
	return sceClibStrncmp(left->id, right->id, 12) != 0;
}

int SceNpCommonPs4_4022699D(const struct NpCommonPs4TitleIdData *left, const struct NpCommonPs4TitleId *right)
{
	return sceClibStrncmp(left->id, right->id, 12) != 0;
}

int SceNpCommonPs4_DE25D3DC(const struct NpCommonPs4TitleId *left, const struct NpCommonPs4TitleIdData *right)
{
	return sceClibStrncmp(left->id, right->id, 12) != 0;
}

const void *g_titleIdVtable[] = {		/* 0x8102a920 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_57005284,
	(const void *)SceNpCommonPs4_B9600F4B,
};

}
