/*
 * SceSysmem: the memblock type database. A memblock type (as the API takes
 * it) maps to an internal code and attributes; both tables are sorted by
 * lookup key (bits 24..31 of the type or code, see MemBlockKey) and indexed
 * by key.
 */

#include "sysmem_internal.h"


int SceDipswForDriver_A98FC2FD(unsigned int bit);

/* attr: only with the tool DIP switch */
#define MEMBLOCK_TYPE_ATTR_TOOL_ONLY	0x20
#define DIPSW_ENABLE_TOOL_PHYMEMPART	210

/* PLACEHOLDER: attr is not decoded beyond the tool-only and contiguous bits */
/* MemBlockTypeInfo: sysmem_internal.h */

#define MEMBLOCK_DB_KEYS		38


static const MemBlockTypeInfo s_typeInfo[89] = {
	{ 0x00000000, 0x00000000, 0x00000001, 0x00000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_MAIN_RW, 0x0ab0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_MAIN_R, 0x0ab0d040, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_MAIN_RX, 0x0ab0d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_SHARED_R, 0x0a90d040, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_SHARED_RX, 0x0a90d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_SHARED_RW, 0x0a90d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_CDIALOG_R, 0x0aa0d040, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_CDIALOG_RX, 0x0aa0d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_CDIALOG_RW, 0x0aa0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_TOOL_R, 0x0af0d040, 0x02500021, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_TOOL_RX, 0x0af0d050, 0x02500021, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_SHARED_TOOL_RW, 0x0af0d060, 0x02500021, 0x01000000 },
	{ 0x0720d006, 0x07b0d006, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_TOOL_RW, 0x08f0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_TOOL_RX, 0x08f0d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_TOOL_R, 0x08f0d040, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_TOOL_UNK_RW, 0x08f0d006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 0x09c08060, 0x09100081, 0x08000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_R, 0x09c08040, 0x09100081, 0x08000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_L1WBWA_RW, 0x09c04060, 0x09100081, 0x08000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_IO_DEVICE_R, 0x0b100840, 0x23040000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_IO_DEVICE_RW, 0x0b100860, 0x23040000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_IO_SO_R, 0x0b100240, 0x23040000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_IO_SO_RW, 0x0b100260, 0x23040000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_DEVICE_RW, 0x0bb00860, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_NC_RW, 0x0db08060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_R, 0x0cb0d040, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RX, 0x0cb0d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW, 0x0cb0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_R, 0x0c80d040, 0x11080100, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW, 0x0c80d060, 0x11080101, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW, 0x0ca0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW, 0x0da08060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_GAME_RW, 0x0c50d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_RW, 0x0cf0d060, 0x02500021, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_RX, 0x0cf0d050, 0x02500021, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_NC_RW, 0x0df08060, 0x02500021, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, 0x0d808060, 0x11080101, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_R, 0x0d808040, 0x11080100, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_R, 0x0ea0d040, 0x23100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RX, 0x0ea0d050, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW, 0x0ea0d060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_CDIALOG_NC_R, 0x0fa08040, 0x23100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_NC_RW, 0x0fa08060, 0x03100001, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, 0x1060d006, 0x02201000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_R, 0x1060d004, 0x23001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RX, 0x1060d005, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_SO_RW, 0x30600206, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_DEVICE_RW, 0x30600806, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_L1WBWA_RW, 0x30604006, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_R, 0x30608004, 0x23001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, 0x30608006, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_R, 0x1080d004, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_RW, 0x1080d006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_GAME_RW, 0x1050d006, 0x03008000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_UMAIN_RW, 0x10b0d006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_UMAIN_NC_RW, 0x10b08006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_CDIALOG_RW, 0x10a0d006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_CDIALOG_R, 0x10a0d004, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_R, 0x14f0d004, 0x22420020, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RX, 0x14f0d005, 0x22420020, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW, 0x14f0d006, 0x02420020, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_NC_R, 0x15f08004, 0x22420020, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_NC_RW, 0x15f08006, 0x02420020, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x20100806, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_R, 0x20100804, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_R, 0x20100204, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x20100206, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_R, 0x20108004, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW, 0x20108006, 0x25010040, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_NC_R, 0x30808004, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_NC_RW, 0x30808006, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_RW, 0x40c08006, 0x09001080, 0x08000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_L1WBWA_RW, 0x40c04006, 0x09001080, 0x08000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_R, 0x6060d004, 0x22900000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW, 0x6060d006, 0x02900000, 0x01000000 },
	{ 0x60208014, 0x60608004, 0x22801000, 0x01000000 },
	{ 0x60208016, 0x60608006, 0x02801000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_R, 0x50608004, 0x22801000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_RW, 0x50608006, 0x02801000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_GAME_RW, 0x6050d006, 0x03100000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_KMP_GAME_RW, 0xc050d006, 0x03004000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_KMP_TOOL_RW, 0xc0f0d006, 0x03004020, 0x01000000 },
	{ 0x11208006, 0x11608006, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_R, 0x1260d004, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_RW, 0x1260d006, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_NC_R, 0x13608004, 0x03001000, 0x01000000 },
	{ SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_NC_RW, 0x13608006, 0x03001000, 0x01000000 },
};

static const MemBlockCodeInfo s_codeInfo[88] = {
	{ 0x00000000, &s_typeInfo[0] },
	{ 0x07b0d006, &s_typeInfo[13] },
	{ 0x08f0d060, &s_typeInfo[14] },
	{ 0x08f0d050, &s_typeInfo[15] },
	{ 0x08f0d040, &s_typeInfo[16] },
	{ 0x08f0d006, &s_typeInfo[17] },
	{ 0x09c08060, &s_typeInfo[18] },
	{ 0x09c08040, &s_typeInfo[19] },
	{ 0x09c04060, &s_typeInfo[20] },
	{ 0x0ab0d060, &s_typeInfo[1] },
	{ 0x0ab0d040, &s_typeInfo[2] },
	{ 0x0ab0d050, &s_typeInfo[3] },
	{ 0x0a90d040, &s_typeInfo[4] },
	{ 0x0a90d050, &s_typeInfo[5] },
	{ 0x0a90d060, &s_typeInfo[6] },
	{ 0x0aa0d040, &s_typeInfo[7] },
	{ 0x0aa0d050, &s_typeInfo[8] },
	{ 0x0aa0d060, &s_typeInfo[9] },
	{ 0x0af0d040, &s_typeInfo[10] },
	{ 0x0af0d050, &s_typeInfo[11] },
	{ 0x0af0d060, &s_typeInfo[12] },
	{ 0x0b100840, &s_typeInfo[21] },
	{ 0x0b100860, &s_typeInfo[22] },
	{ 0x0b100240, &s_typeInfo[23] },
	{ 0x0b100260, &s_typeInfo[24] },
	{ 0x0bb00860, &s_typeInfo[25] },
	{ 0x0cb0d040, &s_typeInfo[27] },
	{ 0x0cb0d050, &s_typeInfo[28] },
	{ 0x0cb0d060, &s_typeInfo[29] },
	{ 0x0c80d040, &s_typeInfo[30] },
	{ 0x0c80d060, &s_typeInfo[31] },
	{ 0x0ca0d060, &s_typeInfo[32] },
	{ 0x0c50d060, &s_typeInfo[34] },
	{ 0x0cf0d060, &s_typeInfo[35] },
	{ 0x0cf0d050, &s_typeInfo[36] },
	{ 0x0db08060, &s_typeInfo[26] },
	{ 0x0df08060, &s_typeInfo[37] },
	{ 0x0d808060, &s_typeInfo[38] },
	{ 0x0da08060, &s_typeInfo[33] },
	{ 0x0ea0d040, &s_typeInfo[40] },
	{ 0x0ea0d050, &s_typeInfo[41] },
	{ 0x0ea0d060, &s_typeInfo[42] },
	{ 0x0fa08040, &s_typeInfo[43] },
	{ 0x0fa08060, &s_typeInfo[44] },
	{ 0x1060d006, &s_typeInfo[45] },
	{ 0x1060d004, &s_typeInfo[46] },
	{ 0x1060d005, &s_typeInfo[47] },
	{ 0x1080d004, &s_typeInfo[53] },
	{ 0x1080d006, &s_typeInfo[54] },
	{ 0x1050d006, &s_typeInfo[55] },
	{ 0x10b0d006, &s_typeInfo[56] },
	{ 0x10b08006, &s_typeInfo[57] },
	{ 0x10a0d004, &s_typeInfo[59] },
	{ 0x10a0d006, &s_typeInfo[58] },
	{ 0x20100804, &s_typeInfo[66] },
	{ 0x20100806, &s_typeInfo[65] },
	{ 0x20100204, &s_typeInfo[67] },
	{ 0x20100206, &s_typeInfo[68] },
	{ 0x20108004, &s_typeInfo[69] },
	{ 0x20108006, &s_typeInfo[70] },
	{ 0x30600206, &s_typeInfo[48] },
	{ 0x30600806, &s_typeInfo[49] },
	{ 0x30604006, &s_typeInfo[50] },
	{ 0x30608004, &s_typeInfo[51] },
	{ 0x30608006, &s_typeInfo[52] },
	{ 0x30808004, &s_typeInfo[71] },
	{ 0x30808006, &s_typeInfo[72] },
	{ 0x40c08006, &s_typeInfo[73] },
	{ 0x40c04006, &s_typeInfo[74] },
	{ 0x50608004, &s_typeInfo[79] },
	{ 0x50608006, &s_typeInfo[80] },
	{ 0x60608004, &s_typeInfo[77] },
	{ 0x60608006, &s_typeInfo[78] },
	{ 0x6060d004, &s_typeInfo[75] },
	{ 0x6060d006, &s_typeInfo[76] },
	{ 0x6050d006, &s_typeInfo[81] },
	{ 0xc050d006, &s_typeInfo[82] },
	{ 0xc0f0d006, &s_typeInfo[83] },
	{ 0x11608006, &s_typeInfo[84] },
	{ 0x1260d004, &s_typeInfo[85] },
	{ 0x1260d006, &s_typeInfo[86] },
	{ 0x13608004, &s_typeInfo[87] },
	{ 0x13608006, &s_typeInfo[88] },
	{ 0x14f0d004, &s_typeInfo[60] },
	{ 0x14f0d005, &s_typeInfo[61] },
	{ 0x14f0d006, &s_typeInfo[62] },
	{ 0x15f08004, &s_typeInfo[63] },
	{ 0x15f08006, &s_typeInfo[64] },
};

/* the first entry of each key */
static const MemBlockTypeInfo *const s_typeIndex[MEMBLOCK_DB_KEYS] = {
	&s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[1],
	&s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[13],
	&s_typeInfo[14], &s_typeInfo[18], &s_typeInfo[0], &s_typeInfo[21],
	&s_typeInfo[25], &s_typeInfo[38], &s_typeInfo[40], &s_typeInfo[0],
	&s_typeInfo[0], &s_typeInfo[45], &s_typeInfo[65], &s_typeInfo[71],
	&s_typeInfo[73], &s_typeInfo[0], &s_typeInfo[75], &s_typeInfo[0],
	&s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[0],
	&s_typeInfo[82], &s_typeInfo[0], &s_typeInfo[0], &s_typeInfo[0],
	&s_typeInfo[0], &s_typeInfo[84], &s_typeInfo[85], &s_typeInfo[87],
	&s_typeInfo[0], &s_typeInfo[0],
};

static const MemBlockCodeInfo *const s_codeIndex[MEMBLOCK_DB_KEYS] = {
	&s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0],
	&s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[1],
	&s_codeInfo[2], &s_codeInfo[6], &s_codeInfo[9], &s_codeInfo[21],
	&s_codeInfo[26], &s_codeInfo[35], &s_codeInfo[39], &s_codeInfo[42],
	&s_codeInfo[0], &s_codeInfo[44], &s_codeInfo[54], &s_codeInfo[60],
	&s_codeInfo[67], &s_codeInfo[69], &s_codeInfo[71], &s_codeInfo[0],
	&s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0],
	&s_codeInfo[76], &s_codeInfo[0], &s_codeInfo[0], &s_codeInfo[0],
	&s_codeInfo[0], &s_codeInfo[78], &s_codeInfo[79], &s_codeInfo[81],
	&s_codeInfo[83], &s_codeInfo[86],
};

/* name unknown: the key of a type or code whose top nibble is set */
__attribute__((noinline))
SceUInt32 MemBlockKeyHigh(SceUInt32 code)
{
	if (code & 0x0f000000)
		return ((code & 0x0f000000) >> 24) + 0x20;
	return (code >> 28) + 0x10;
}

static inline SceUInt32 MemBlockKey(SceUInt32 code)
{
	if ((code & 0xf0000000) == 0)
		return (code & 0x0f000000) >> 24;
	return MemBlockKeyHigh(code);
}

SceUInt32 MemBlockCode2PartitionIndex(SceUInt32 code)
{
	SCE_DBG_ASSERT_CHECK((code & 0xff000000) != 0, 1103);
	if (code >> 28 == 0)
		return (code & 0x0f000000) >> 24;
	return (code & 0x0f000000) ? code >> 24 : code >> 28;
}

/* name unknown: a code's entry */
const MemBlockCodeInfo *MemBlockCode2CodeInfo(SceUInt32 code)
{
	SceUInt32 key = MemBlockKey(code);
	const MemBlockCodeInfo *entry = s_codeIndex[key], *end = &s_codeInfo[sizeof(s_codeInfo) /
							       sizeof(s_codeInfo[0])];

	for (; entry < end; entry++) {
		if (entry->code == code)
			return entry;
		if (MemBlockKey(entry->code) != key)
			return NULL;
	}
	return NULL;
}

int MemBlockType2TypeInfo(SceUInt32 type, const MemBlockTypeInfo **info)
{
	SceUInt32 key = MemBlockKey(type);
	const MemBlockTypeInfo *entry = s_typeIndex[key], *end = &s_typeInfo[sizeof(s_typeInfo) /
							       sizeof(s_typeInfo[0])];

	for (; entry < end; entry++) {
		if (entry->type == type)
			break;
		if (MemBlockKey(entry->type) != key)
			goto unknown;
	}
	if (entry >= end)
		goto unknown;
	if (!(entry->attr & MEMBLOCK_TYPE_ATTR_TOOL_ONLY) ||
	    SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART)) {
		*info = entry;
		return 0;
	}
	SCE_DBG_PRINTF(0xb, 2039, "Only SCE_DIPSW_ENABLE_TOOL_PHYMEMPART\n");
	SCE_DBG_ASSERT_FAIL(2040);
unknown:
	*info = NULL;
	return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
}

/* name unknown: a type's code, 0 for an unknown type */
int MemBlockType2Code(SceUInt32 type, SceUInt32 *code)
{
	const MemBlockTypeInfo *info;
	int ret;

	ret = MemBlockType2TypeInfo(type, &info);
	*code = ret < 0 ? 0 : info->code;
	return ret;
}

int MemBlockCode2TypeInfo(SceUInt32 code, const MemBlockTypeInfo **info)
{
	const MemBlockCodeInfo *entry = MemBlockCode2CodeInfo(code);

	if (entry != NULL) {
		if (!(entry->typeInfo->attr & MEMBLOCK_TYPE_ATTR_TOOL_ONLY) ||
		    SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART)) {
			*info = entry->typeInfo;
			return 0;
		}
		SCE_DBG_PRINTF(0xb, 2162, "Only SCE_DIPSW_ENABLE_TOOL_PHYMEMPART\n");
		SCE_DBG_ASSERT_FAIL(2163);
	}
	*info = NULL;
	return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
}

/* name unknown: a code's type */
int MemBlockCode2Type(SceUInt32 code, SceUInt32 *type)
{
	const MemBlockTypeInfo *info;
	int ret;

	ret = MemBlockCode2TypeInfo(code, &info);
	if (ret >= 0)
		*type = info->type;
	return ret;
}
