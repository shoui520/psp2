/*
 * SceNpCommon: utilities. The module's heap, NP ID comparison, the platform an NP ID names,
 * UTF-8 checks, the system software version and the NP settings with their registry overrides.
 */

#include "np_common_internal.h"

static int s_heapInitialized;
static void *s_heap;
static NpCommonSettings s_settings;

/* the platform names an NP ID carries in opt[4..7] */
static const char s_platformNone[4] = "";

/* characters an NP string may not hold: controls other than tab, newline and return */
static const SceUInt8 s_invalidCharacters[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	[0x7f] = 1,
	[0xff] = 1,
};

int SceNpCommon_55F7FCF7(SceUInt64 *time)
{
	*time = sceKernelGetSystemTimeWide();
	return 0;
}

int SceNpCommon_A385C8CD(int arg0, int arg1, SceSize size, void *memblock)
{
	int ret = 0;

	if (size != 0 && memblock != NULL) {
		ret = SceNpCommon_619FB198(size, memblock);
		if (ret >= 0)
			s_heapInitialized = 1;
	}
	return ret;
}

void SceNpCommon_BA6310A1(void)
{
	if (s_heapInitialized != 0) {
		SceNpCommon_622F004B();
		s_heapInitialized = 0;
	}
}

__attribute__((noinline))
int SceNpCommon_619FB198(SceSize size, void *memblock)
{
	s_heap = sceClibMspaceCreate(memblock, size);
	return 0;
}

__attribute__((noinline))
void SceNpCommon_622F004B(void)
{
	sceClibMspaceDestroy(s_heap);
}

void *sceNpCommonMallocNpServerName(SceSize size)
{
	return sceClibMspaceMalloc(s_heap, size);
}

void *SceNpCommon_50D71BB5(void *ptr, SceSize size)
{
	return sceClibMspaceRealloc(s_heap, ptr, size);
}

void sceNpCommonFreeNpServerName(void *ptr)
{
	sceClibMspaceFree(s_heap, ptr);
}

SceBool SceNpCommon_A1CF53F3(SceUInt64 left, SceUInt64 right)
{
	return left != right;
}

int sceNpGetPlatformType(const SceNpId *npId)
{
	const SceUChar8 *platform;

	if (npId == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	platform = &npId->opt[4];
	if (platform[0] == '\0')
		return SCE_NP_PLATFORM_TYPE_NONE;
	if (sceClibMemcmp(platform, "ps3", 4) == 0)
		return SCE_NP_PLATFORM_TYPE_PS3;
	if (sceClibMemcmp(platform, "psp2ps4", 4) == 0)
		return SCE_NP_PLATFORM_TYPE_VITA;
	if (sceClibMemcmp(platform, "ps4", 4) == 0)
		return SCE_NP_PLATFORM_TYPE_PS4;
	return SCE_NP_ERROR_UNKNOWN_PLATFORM_TYPE;
}

int sceNpSetPlatformType(SceNpId *npId, SceNpPlatformType platformType)
{
	if (npId == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if ((unsigned int)platformType > SCE_NP_PLATFORM_TYPE_PS4)
		return SCE_NP_ERROR_UNKNOWN_PLATFORM_TYPE;
	switch (platformType) {
	case SCE_NP_PLATFORM_TYPE_NONE:
		sceClibMemcpy_safe(&npId->opt[4], s_platformNone, 4);
		break;
	case SCE_NP_PLATFORM_TYPE_PS3:
		sceClibMemcpy_safe(&npId->opt[4], "ps3", 4);
		break;
	case SCE_NP_PLATFORM_TYPE_VITA:
		sceClibMemcpy_safe(&npId->opt[4], "psp2ps4", 4);
		break;
	default:
		sceClibMemcpy_safe(&npId->opt[4], "ps4", 4);
		break;
	}
	return 0;
}

int sceNpCmpNpId(const SceNpId *npid1, const SceNpId *npid2)
{
	if (npid1 == NULL || npid2 == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (npid1->reserved[0] != 1 || npid2->reserved[0] != 1)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	if (sceClibStrncmp(npid1->handle.data, npid2->handle.data, SCE_NP_ONLINEID_MAX_LENGTH) != 0 ||
	    sceClibMemcmp(&npid1->opt[0], &npid2->opt[0], 4) != 0)
		return SCE_NP_UTIL_ERROR_NOT_MATCH;
	if (sceClibMemcmp(&npid1->opt[4], &npid2->opt[4], 4) != 0 &&
	    npid1->opt[4] != '\0' && npid2->opt[4] != '\0')
		return SCE_NP_UTIL_ERROR_NOT_MATCH;
	return 0;
}

int SceNpCommon_8C21F489(const SceNpOnlineId *onlineId1, const SceNpOnlineId *onlineId2)
{
	if (onlineId1 == NULL || onlineId2 == NULL)
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	if (sceClibStrncmp(onlineId1->data, onlineId2->data, SCE_NP_ONLINEID_MAX_LENGTH) != 0)
		return SCE_NP_UTIL_ERROR_NOT_MATCH;
	return 0;
}

int sceNpCmpOnlineId(const SceNpId *npid1, const SceNpId *npid2)
{
	if (npid1 == NULL || npid2 == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (npid1->reserved[0] != 1 || npid2->reserved[0] != 1)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	if (sceClibStrncmp(npid1->handle.data, npid2->handle.data, SCE_NP_ONLINEID_MAX_LENGTH) != 0)
		return SCE_NP_UTIL_ERROR_NOT_MATCH;
	return 0;
}

/* the characters of a UTF-8 string, INVALID_CHARACTER at a bad sequence or control */
int SceNpCommon_73BDFF32(const SceUInt8 *str)
{
	int count = 0;
	SceUInt8 c;

	if (str == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	for (c = *str; c != '\0'; c = *str) {
		if ((c & 0x80) == 0) {
			if (s_invalidCharacters[c] != 0)
				return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
			str++;
		} else if ((c & 0xe0) == 0xc0 && c >= 0xc2 && (str[1] & 0xc0) == 0x80) {
			str += 2;
		} else if ((c & 0xf0) == 0xe0 && (str[1] & 0xc0) == 0x80 && (str[2] & 0xc0) == 0x80) {
			str += 3;
		} else if ((c & 0xf8) == 0xf0 && (str[1] & 0xc0) == 0x80 &&
			   (str[2] & 0xc0) == 0x80 && (str[3] & 0xc0) == 0x80) {
			str += 4;
		} else {
			return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
		}
		count++;
	}
	return count;
}

/* the position count characters into a UTF-8 string, checking them */
int SceNpCommon_A04C77A1(int count, const SceUInt8 *str, const SceUInt8 **end)
{
	SceUInt8 c;

	if (str == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	for (c = *str; c != '\0' && count != 0; c = *str) {
		if ((c & 0x80) == 0) {
			if (s_invalidCharacters[c] != 0)
				return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
			str++;
		} else if ((c & 0xe0) == 0xc0 && c >= 0xc2 && (str[1] & 0xc0) == 0x80) {
			str += 2;
		} else if ((c & 0xf0) == 0xe0 && (str[1] & 0xc0) == 0x80 && (str[2] & 0xc0) == 0x80) {
			str += 3;
		} else if ((c & 0xf8) == 0xf0 && (str[1] & 0xc0) == 0x80 &&
			   (str[2] & 0xc0) == 0x80 && (str[3] & 0xc0) == 0x80) {
			str += 4;
		} else {
			return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
		}
		count--;
	}
	*end = str;
	return 0;
}

int sceNpCmpNpIdInOrder(const SceNpId *npid1, const SceNpId *npid2, int *order)
{
	const void *platform1, *platform2;
	int ret;

	if (npid1 == NULL || npid2 == NULL || order == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (npid1->reserved[0] != 1 || npid2->reserved[0] != 1)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	ret = sceClibStrncmp(npid1->handle.data, npid2->handle.data, SCE_NP_ONLINEID_MAX_LENGTH);
	if (ret < 0) {
		*order = -1;
		return 0;
	}
	if (ret > 0) {
		*order = 1;
		return 0;
	}
	ret = sceClibMemcmp(&npid1->opt[0], &npid2->opt[0], 4);
	if (ret < 0) {
		*order = -1;
		return 0;
	}
	if (ret > 0) {
		*order = 1;
		return 0;
	}
	/* an NP ID without a platform is a Vita's */
	if (npid1->opt[4] == '\0' && npid2->opt[4] == '\0') {
		*order = 0;
		return 0;
	}
	platform1 = npid1->opt[4] != '\0' ? (const void *)&npid1->opt[4] : "psp2ps4";
	platform2 = npid2->opt[4] != '\0' ? (const void *)&npid2->opt[4] : "psp2ps4";
	ret = sceClibMemcmp(platform1, platform2, 4);
	if (ret < 0) {
		*order = -1;
		return 0;
	}
	if (ret > 0) {
		*order = 1;
		return 0;
	}
	*order = 0;
	return 0;
}

/* marks an NP ID as a Vita's when it names no platform, after clearing the platform if asked */
int SceNpCommon_256670D6(SceNpId *npId, int clear)
{
	if (npId == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (clear != 0)
		sceClibMemset(&npId->opt[4], 0, 4);
	if (npId->reserved[0] != 1)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	if (npId->opt[4] == '\0')
		sceClibMemcpy_safe(&npId->opt[4], "psp2ps4", 4);
	return 0;
}

/*
 * The system software version as "major.minor". Sony's major digit is
 * (version >> 28) & 0xf * 10: the multiplication binds first, so the tens digit is masked
 * with 150; kept as Sony built it.
 */
void SceNpCommon_0624AE44(char *version)
{
	SceKernelSystemSwVersion info;

	sceClibMemset(version, 0, 8);
	info.size = sizeof(info);
	sceKernelGetSystemSwVersion(&info);
	sceClibSnprintf(version, 8, "%d.%02d",
			((info.version >> 28) & 0xf * 10) + ((info.version >> 24) & 0xf),
			((info.version >> 20) & 0xf) * 10 + ((info.version >> 16) & 0xf));
}

void sceNpCommonGetSystemSwVersion(char *version)
{
	SceKernelSystemSwVersion info;

	sceClibMemset(version, 0, 8);
	info.size = sizeof(info);
	sceKernelGetSystemSwVersion(&info);
	sceClibSnprintf(version, 8, "%02d.%02d",
			((info.version >> 28) & 0xf * 10) + ((info.version >> 24) & 0xf),
			((info.version >> 20) & 0xf) * 10 + ((info.version >> 16) & 0xf));
}

int SceNpCommon_DA04BA55(const NpCommonSettings *settings)
{
	sceClibMemcpy_safe(&s_settings, settings, sizeof(s_settings));
	return 0;
}

/* the registry overrides the settings on a test or development unit */
static inline SceBool npCommonUseRegistry(void)
{
	return sceSblQafMgrIsAllowNpFullTest() == 1 || vshSblAimgrIsCEX() != 1 ||
	       vshSblUtMgrHasStoreFlag() == 1;
}

SceBool SceNpCommon_BBC13621(void)
{
	int value;

	if ((sceSblQafMgrIsAllowNpFullTest() == 1 || vshSblAimgrIsCEX() != 1 ||
	     vshSblUtMgrHasStoreFlag() == 1) &&
	    sceRegMgrSystemParamGetInt(750541, &value) >= 0)
		return value != 0;
	return s_settings.fullTest;
}

int SceNpCommon_92DF18F6(char *serverName, SceSize size)
{
	if (size < 38)
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	sceClibStrncpy(serverName, s_settings.serverName, size);
	serverName[size - 1] = '\0';
	return 0;
}

SceBool SceNpCommon_F585C86C(void)
{
	int value;

	if ((sceSblQafMgrIsAllowNpTest() == 1 || vshSblAimgrIsCEX() != 1 ||
	     vshSblUtMgrHasStoreFlag() == 1) &&
	    sceRegMgrSystemParamGetInt(656533, &value) >= 0)
		return value != 0;
	return s_settings.test;
}

int SceNpCommon_B88B3F92(int *value)
{
	if (!npCommonUseRegistry() || sceRegMgrSystemParamGetInt(749261, value) < 0)
		*value = s_settings.param749261;
	return 0;
}

int SceNpCommon_43F5DE5F(int *value)
{
	if (!npCommonUseRegistry() || sceRegMgrSystemParamGetInt(858341, value) < 0)
		*value = s_settings.param858341;
	return 0;
}

int SceNpCommon_187C0A8D(int *value)
{
	if (!npCommonUseRegistry() || sceRegMgrSystemParamGetInt(1401993, value) < 0)
		*value = s_settings.param1401993;
	return 0;
}
