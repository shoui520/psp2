/*
 * SceNpCommerce2: forwarding functions of Sony's source files: each calls the reader its file uses
 * and returns its result.
 */

#include "np_commerce2_internal.h"

int npCommerce2ReadDateItem0x2c_81010B14(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadDateItem0x2c_810115FC(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadDateItem0x2c_810116FA(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadDateItem0x2c_810117FC(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadDateItem0x2c_810119A0(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadDateItem0x2c_81012BD6(int a1, int a2)
{
	return npCommerce2ReadDateItem0x2c((const NpCommerce2Item *)a1, (NpCommerce2DateItem *)a2);
}

int npCommerce2ReadCountItem0x2f_81010BEC(int a1, int a2)
{
	return npCommerce2ReadCountItem0x2f((const NpCommerce2Item *)a1, (NpCommerce2CountItem *)a2);
}

int npCommerce2ReadCountItem0x2f_81011604(int a1, int a2)
{
	return npCommerce2ReadCountItem0x2f((const NpCommerce2Item *)a1, (NpCommerce2CountItem *)a2);
}

int npCommerce2ReadCountItem0x2f_81011702(int a1, int a2)
{
	return npCommerce2ReadCountItem0x2f((const NpCommerce2Item *)a1, (NpCommerce2CountItem *)a2);
}

int npCommerce2ReadCountItem0x2f_810118B0(int a1, int a2)
{
	return npCommerce2ReadCountItem0x2f((const NpCommerce2Item *)a1, (NpCommerce2CountItem *)a2);
}

int npCommerce2ReadCountItem0x2f_810119A8(int a1, int a2)
{
	return npCommerce2ReadCountItem0x2f((const NpCommerce2Item *)a1, (NpCommerce2CountItem *)a2);
}

int npCommerce2ReadEntry0x16_81014D62(int a1, int a2, int a3)
{
	return npCommerce2ReadEntry0x16((const NpCommerce2Item *)a1, a2, (NpCommerce2Entry *)a3);
}

int npCommerce2ReadRecord0x30_81011C24(int a1, int a2)
{
	return npCommerce2ReadRecord0x30((const NpCommerce2Item *)a1, (NpCommerce2Record0x30 *)a2);
}

int npCommerce2ReadRecord0x30_81012CE8(int a1, int a2)
{
	return npCommerce2ReadRecord0x30((const NpCommerce2Item *)a1, (NpCommerce2Record0x30 *)a2);
}

int npCommerce2ReadRecord0x30_81014AA2(int a1, int a2)
{
	return npCommerce2ReadRecord0x30((const NpCommerce2Item *)a1, (NpCommerce2Record0x30 *)a2);
}

int npCommerce2ReadRecord0x30_8101690C(int a1, int a2)
{
	return npCommerce2ReadRecord0x30((const NpCommerce2Item *)a1, (NpCommerce2Record0x30 *)a2);
}

int npCommerce2ReadCountedText0x37_81011D84(int a1, int a2, int a3)
{
	return npCommerce2ReadCountedText0x37((const NpCommerce2Item *)a1, a2, (NpCommerce2CountedText *)a3);
}

int npCommerce2ReadCountedText0x37_81014A92(int a1, int a2, int a3)
{
	return npCommerce2ReadCountedText0x37((const NpCommerce2Item *)a1, a2, (NpCommerce2CountedText *)a3);
}

int npCommerce2ReadCountedText0x37_810168FC(int a1, int a2, int a3)
{
	return npCommerce2ReadCountedText0x37((const NpCommerce2Item *)a1, a2, (NpCommerce2CountedText *)a3);
}

int npCommerce2ReadTextItem0x39_81011FC8(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextValueItem *item)
{
	return npCommerce2ReadTextItem0x39(result, index, item);
}

int npCommerce2ReadItem0x32_81012F80(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadItem0x32(result, item);
}

int npCommerce2ReadCount0x33_81013012(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadCount0x33(result, item);
}

int npCommerce2ReadItem0x32_81013A22(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadItem0x32(result, item);
}

int npCommerce2ReadCount0x33_81013A2A(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadCount0x33(result, item);
}

int npCommerce2ReadEntry0x16_8100D8E4(int a1, int a2, int a3)
{
	return npCommerce2ReadEntry0x16((const NpCommerce2Item *)a1, a2, (NpCommerce2Entry *)a3);
}

int npCommerce2ReadTextItem0x39_81014A9A(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextValueItem *item)
{
	return npCommerce2ReadTextItem0x39(result, index, item);
}

int npCommerce2ReadTextItem0x39_81016904(const NpCommerce2Item *result, SceUInt32 index, NpCommerce2TextValueItem *item)
{
	return npCommerce2ReadTextItem0x39(result, index, item);
}

int npCommerce2ReadItem0x58Of_810118A2(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadItem0x58(&result->commonData, (const char *)result->commonData.data, result->commonData.data_size, item);
}

int npCommerce2ReadItem0x58Of_810119B0(const NpCommerce2Item *result, NpCommerce2Item *item)
{
	return npCommerce2ReadItem0x58(&result->commonData, (const char *)result->commonData.data, result->commonData.data_size, item);
}

int npCommerce2StartShellSlot_8101848C(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_81018494(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_8101849C(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184A4(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184AC(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184B4(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184BC(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184C4(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_810184CC(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

int npCommerce2StartShellSlot_8101884A(int slotId, void *request)
{
	return npCommerce2StartShellSlot(slotId, request);
}

/* the calls of a function the library may not be linked with: 9 when it is missing */
extern int npCommerce2WeakCall(int a, int b, int c, int flag) __attribute__((weak));

int npCommerce2WeakCall_8101EC24(int a, int b, int c)
{
	if (npCommerce2WeakCall != 0)
		return npCommerce2WeakCall(a, b, c, 0);
	return 9;
}

int npCommerce2WeakCall_8101EC42(int a, int b, int c)
{
	if (npCommerce2WeakCall != 0)
		return npCommerce2WeakCall(a, b, c, 1);
	return 9;
}
