/*
 * SceNpCommonPs4: NpTrans (npext_npm/np_trans.cpp), an HttpTrans made on behalf of an NP user: the
 * user's and the title's tokens go into its request headers, and a response that is not a success
 * is turned into an NP error.
 */

#include "../np_common_ps4_internal.h"

extern "C" {

extern const void *g_npTransVtable[];		/* 0x8102aa48: the vtables of NpTrans and of its second base */

struct NpCommonPs4NpTrans *SceNpCommonPs4_7ADE1B7A(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Allocator *pool,
	const struct NpCommonPs4OnlineId *onlineId)
{
	SceNpCommonPs4_9256CF02(&npTrans->trans, pool);
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_npTransVtable);
	npTrans->trans.vtableSecondBase = &g_npTransVtable[8];
	SceNpCommonPs4_AEC0DAC5(&npTrans->onlineId, onlineId);
	return npTrans;
}

struct NpCommonPs4NpTrans *SceNpCommonPs4_3FC6688C(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Allocator *pool,
	const struct NpCommonPs4OnlineId *onlineId)
{
	SceNpCommonPs4_9256CF02(&npTrans->trans, pool);
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_npTransVtable);
	npTrans->trans.vtableSecondBase = &g_npTransVtable[8];
	SceNpCommonPs4_AEC0DAC5(&npTrans->onlineId, onlineId);
	return npTrans;
}

__attribute__((noinline)) struct NpCommonPs4NpTrans *SceNpCommonPs4_7FD20D75(struct NpCommonPs4NpTrans *npTrans)
{
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_npTransVtable);
	npTrans->trans.vtableSecondBase = &g_npTransVtable[8];
	SceNpCommonPs4_1543D7EE(&npTrans->onlineId);
	SceNpCommonPs4_47C5936B(&npTrans->trans);
	return npTrans;
}

__attribute__((noinline)) int SceNpCommonPs4_5BB97F78(struct NpCommonPs4NpTrans *npTrans)
{
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_npTransVtable);
	npTrans->trans.vtableSecondBase = &g_npTransVtable[8];
	SceNpCommonPs4_1543D7EE(&npTrans->onlineId);
	SceNpCommonPs4_47C5936B(&npTrans->trans);
	return SceNpCommonPs4_BB5282C8(npTrans);
}

struct NpCommonPs4NpTrans *SceNpCommonPs4_47D94EF0(struct NpCommonPs4NpTrans *npTrans)
{
	npTrans->trans.vtable = NP_COMMON_PS4_VTABLE(g_npTransVtable);
	npTrans->trans.vtableSecondBase = &g_npTransVtable[8];
	SceNpCommonPs4_1543D7EE(&npTrans->onlineId);
	SceNpCommonPs4_47C5936B(&npTrans->trans);
	return npTrans;
}

int SceNpCommonPs4_060EB06B(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle)
{
	struct NpCommonPs4TitleToken titleToken;
	int result;
	int status;

	SceNpCommonPs4_DC4787E9(&titleToken);
	result = SceNpCommonPs4_9BC491E3(handle, npTrans->onlineId.data, &titleToken);
	if (result >= 0) {
		status = SceNpCommonPs4_44FF13E8(&npTrans->trans, "X-NP-TITLE-TOKEN", SceNpCommonPs4_80D16974(&titleToken));
		SceNpCommonPs4_FD80FE40(&titleToken);
		if (status >= 0)
			result = 0;
		else
			result = status;
	} else {
		SceNpCommonPs4_FD80FE40(&titleToken);
	}
	return result;
}

int SceNpCommonPs4_D95658C6(const struct NpCommonPs4UserToken *userToken, char *buffer, SceSize size)
{
	sceClibMemset(buffer, 0, size);
	if (sceClibStrlcpy(buffer, "Bearer ", size) < size && sceClibStrlcat(buffer, userToken->data, size) < size)
		return 0;
	return NP_COMMON_PS4_ERROR_80559E0D;
}

int SceNpCommonPs4_57B14D22(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle)
{
	struct NpCommonPs4UserToken userToken;
	char header[272];
	int result;
	int status;

	SceNpCommonPs4_988DFD87(&userToken);
	result = SceNpCommonPs4_191AEA9E(handle, &npTrans->onlineId, &userToken);
	if (result >= 0) {
		sceClibMemset(header, 0, 271);
		if (sceClibStrlcpy(header, "Bearer ", 271) < 271 && sceClibStrlcat(header, userToken.data, 271) < 271) {
			status = SceNpCommonPs4_44FF13E8(&npTrans->trans, "Authorization", header);
			SceNpCommonPs4_78CC9FCD(&userToken);
			if (status >= 0)
				result = 0;
			else
				result = status;
		} else {
			SceNpCommonPs4_78CC9FCD(&userToken);
			result = NP_COMMON_PS4_ERROR_80559E0D;
		}
	} else {
		SceNpCommonPs4_78CC9FCD(&userToken);
	}
	return result;
}

int SceNpCommonPs4_49776226(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle)
{
	struct NpCommonPs4TitleToken titleToken;
	int result;
	int status;
	int added;

	result = SceNpCommonPs4_57B14D22(npTrans, handle);
	if (result >= 0) {
		SceNpCommonPs4_DC4787E9(&titleToken);
		status = SceNpCommonPs4_9BC491E3(handle, npTrans->onlineId.data, &titleToken);
		if (status >= 0) {
			added = SceNpCommonPs4_44FF13E8(&npTrans->trans, "X-NP-TITLE-TOKEN", SceNpCommonPs4_80D16974(&titleToken));
			SceNpCommonPs4_FD80FE40(&titleToken);
			status = added >= 0 ? 0 : added;
		} else {
			SceNpCommonPs4_FD80FE40(&titleToken);
		}
		result = status;
		if (status >= 0)
			result = 0;
	}
	return result;
}

void SceNpCommonPs4_6DA866D5(int reserved, int error)
{
	int code;

	if ((error & 0xFFF00000) == 0x82200000) {
		code = error & 0xFFF;
		if (code == 513 || code == 514 || code == 525)
			SceNpManager_F66A7AAE();
	}
	if ((error & 0xFFF00000) == 0x82300000) {
		if ((error & 0xFFF) == 0xF12)
			SceNpManager_F66A7AAE();
	}
}

int SceNpCommonPs4_18DDD712(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle)
{
	char *body;
	SceSize length;
	SceInt32 statusCode;
	SceSize received;
	int result;
	int code;

	body = NULL;
	length = 0;
	result = SceNpCommonPs4_6B6E813E(&npTrans->trans, handle, &statusCode);
	if (result >= 0) {
		if (statusCode >= 200 && statusCode <= 299)
			return 0;
		body = (char *)SceNpCommonPs4_299ADFC3(npTrans->trans.pool, 4096);
		if (body != NULL) {
			for (;;) {
				result = SceNpCommonPs4_C80B0275(&npTrans->trans, handle, &body[length], 4095 - length, &received);
				if (result < 0)
					break;
				if (received != 0) {
					length += received;
					if (length < 4095)
						continue;
				}
				if (length != 0)
					body[length] = 0;
				else
					*body = 0;
				result = SceNpCommonPs4_FB63A152(statusCode, body, length);
				if (result >= 0)
					result = NP_COMMON_PS4_ERROR_80559F7F;
				break;
			}
		} else {
			result = ((struct NpCommonPs4AllocatorWithError *)npTrans->trans.pool)->error;
		}
	}
	SceNpCommonPs4_38CC9AF5(npTrans->trans.pool, body);
	if (((result & 0xFFF00000) == 0x82200000 && ((code = result & 0xFFF) == 513 || code == 514 || code == 525)) ||
	    ((result & 0xFFF00000) == 0x82300000 && (result & 0xFFF) == 0xF12))
		SceNpManager_F66A7AAE();
	return result;
}

int SceNpCommonPs4_AA7686C1(char *secondBase)
{
	return SceNpCommonPs4_5BB97F78((struct NpCommonPs4NpTrans *)(secondBase - 4));
}

struct NpCommonPs4NpTrans *SceNpCommonPs4_EF79F43C(char *secondBase)
{
	return SceNpCommonPs4_7FD20D75((struct NpCommonPs4NpTrans *)(secondBase - 4));
}

const void *g_npTransVtable[] = {		/* 0x8102aa48 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_7FD20D75,
	(const void *)SceNpCommonPs4_5BB97F78,
	(const void *)SceNpCommonPs4_47443DCE,
	(const void *)SceNpCommonPs4_F81085EE,
	(const void *)-4,
	NULL,
	(const void *)SceNpCommonPs4_EF79F43C,
	(const void *)SceNpCommonPs4_AA7686C1,
	(const void *)SceNpCommonPs4_8EBA83E1,
};

}
