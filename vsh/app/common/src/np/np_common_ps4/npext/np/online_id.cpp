/*
 * SceNpCommonPs4: an online id (a user's 16 character name) behind a vtable.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_onlineIdVtable[];		/* 0x8102a910 */

struct NpCommonPs4OnlineId *SceNpCommonPs4_B0BAB9BE(struct NpCommonPs4OnlineId *onlineId)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemset(onlineId->data, 0, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_AB1BD6BB(struct NpCommonPs4OnlineId *onlineId)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemset(onlineId->data, 0, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_AEC0DAC5(struct NpCommonPs4OnlineId *onlineId, const struct NpCommonPs4OnlineId *source)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemcpy_safe(onlineId->data, source->data, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_0A4F7665(struct NpCommonPs4OnlineId *onlineId, const struct NpCommonPs4OnlineId *source)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemcpy_safe(onlineId->data, source->data, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_D0787194(struct NpCommonPs4OnlineId *onlineId, const void *data)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemcpy_safe(onlineId->data, data, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_7F05712E(struct NpCommonPs4OnlineId *onlineId, const void *data)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_onlineIdVtable);
	sceClibMemcpy_safe(onlineId->data, data, 20);
	return onlineId;
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_1543D7EE(struct NpCommonPs4OnlineId *onlineId)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return onlineId;
}

int SceNpCommonPs4_E8233EC1(struct NpCommonPs4OnlineId *onlineId)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(onlineId);
}

struct NpCommonPs4OnlineId *SceNpCommonPs4_CC8941A2(struct NpCommonPs4OnlineId *onlineId)
{
	onlineId->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return onlineId;
}

int SceNpCommonPs4_21B2C0B7(const struct NpCommonPs4OnlineId *left, const struct NpCommonPs4OnlineId *right)
{
	return SceNpCommon_8C21F489(left->data, right->data) == 0;
}

int SceNpCommonPs4_3F995BC3(struct NpCommonPs4OnlineId *onlineId, const char *string)
{
	char *data;

	if (sceClibStrnlen(string, 0xFFFFFFFF) > 16)
		return NP_COMMON_PS4_ERROR_80559E11;
	data = onlineId->data;
	sceClibMemset(data, 0, 20);
	sceClibStrncpy(data, string, 16);
	return 0;
}

int SceNpCommonPs4_FB7AE916(const struct NpCommonPs4OnlineIdData *left, const struct NpCommonPs4OnlineId *right)
{
	return SceNpCommon_8C21F489(left, right->data) == 0;
}

int SceNpCommonPs4_21F316D6(const struct NpCommonPs4OnlineId *left, const struct NpCommonPs4OnlineIdData *right)
{
	return SceNpCommon_8C21F489(left->data, right) == 0;
}

int SceNpCommonPs4_03B4D52E(const struct NpCommonPs4OnlineId *left, const struct NpCommonPs4OnlineId *right)
{
	return SceNpCommon_8C21F489(left->data, right->data) != 0;
}

int SceNpCommonPs4_3FC41D5B(const struct NpCommonPs4OnlineIdData *left, const struct NpCommonPs4OnlineId *right)
{
	return SceNpCommon_8C21F489(left, right->data) != 0;
}

int SceNpCommonPs4_E83466DA(const struct NpCommonPs4OnlineId *left, const struct NpCommonPs4OnlineIdData *right)
{
	return SceNpCommon_8C21F489(left->data, right) != 0;
}

void *SceNpCommonPs4_DA48D36D(struct NpCommonPs4OnlineId *onlineId)
{
	return sceClibMemset(onlineId->data, 0, 16);
}

const void *g_onlineIdVtable[] = {		/* 0x8102a910 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_1543D7EE,
	(const void *)SceNpCommonPs4_E8233EC1,
};

}
