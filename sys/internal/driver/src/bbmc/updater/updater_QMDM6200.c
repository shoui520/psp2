/*
 * The modem updater (SceBbmcUdMod): its context, the image the download sequence feeds
 * it, and update files read from RAM or through iofilemgr.
 */
#include <string.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <kernel/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include "updater.h"

/* name unknown: the hardware an image entry is for, by its HWID string */
typedef struct UdHardware {
	SceUInt32 type;
	char hardwareId[32];
} UdHardware;

/* name unknown: an update file's trailer, its last 104 bytes */
typedef struct UdFileTrailer {
	SceUInt8 reserved[40];
	SceUInt32 buildIdMagic;
	SceUInt32 buildId[4];
	SceUInt32 nameMagic;
	char name[32];
	SceUInt32 magic;
	SceUInt32 padding;
} UdFileTrailer;

_Static_assert(sizeof(UdFileTrailer) == 104, "update file trailer size");

static const UdHardware udHardwares[2] = {
	{ 0, "HWID_QUALCOMM_MDM6200" },
	{ 1, "HWID_QUALCOMM_MDM6600" },
};

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, dbg_udRamIoClose_QMDM6200_L2867, 2867, "udRamIoClose_QMDM6200");
SCE_DBGINFO_DEFINE(static, dbg_udRemoveDeleteList_L2602, 2602, "udRemoveDeleteList");
SCE_DBGINFO_DEFINE(static, dbg_udRemoveDeleteList_L2603, 2603, "udRemoveDeleteList");
SCE_DBGINFO_DEFINE(static, dbg_udRemoveDeleteList_L2601, 2601, "udRemoveDeleteList");
SCE_DBGINFO_DEFINE(static, dbg__udAddUqcnData_L964, 964, "_udAddUqcnData");
SCE_DBGINFO_DEFINE(static, dbg__udMatchSafeBuildId_L732, 732, "_udMatchSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udMatchSafeBuildId_L733, 733, "_udMatchSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udMatchSafeBuildId_L734, 734, "_udMatchSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udCheckUpdateNoNeedFromSafeBuildId_L775, 775, "_udCheckUpdateNoNeedFromSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udGetSubListLenOfFirmList_L396, 396, "_udGetSubListLenOfFirmList");
SCE_DBGINFO_DEFINE(static, dbg__udGetSubListOfFirmList_L417, 417, "_udGetSubListOfFirmList");
SCE_DBGINFO_DEFINE(static, dbg__udMatchUqcnBuildId_L442, 442, "_udMatchUqcnBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udCheckUpdateNoNeedFromSafeBuildId_L774, 774, "_udCheckUpdateNoNeedFromSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udMatchUqcnBuildId_L443, 443, "_udMatchUqcnBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udMatchUqcnBuildId_L444, 444, "_udMatchUqcnBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udCheckUpdateNoNeedFromSafeBuildId_L776, 776, "_udCheckUpdateNoNeedFromSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udCheckUpdateNoNeedFromSafeBuildId_L777, 777, "_udCheckUpdateNoNeedFromSafeBuildId");
SCE_DBGINFO_DEFINE(static, dbg__udGetAmssFirmInfoFromSlotNum_L809, 809, "_udGetAmssFirmInfoFromSlotNum");
SCE_DBGINFO_DEFINE(static, dbg_udCreateDeleteList_L2509, 2509, "udCreateDeleteList");
SCE_DBGINFO_DEFINE(static, dbg_udCreateDeleteList_L2510, 2510, "udCreateDeleteList");
SCE_DBGINFO_DEFINE(static, dbg__udCompareVersionCarrierImage_L835, 835, "_udCompareVersionCarrierImage");
SCE_DBGINFO_DEFINE(static, dbg__udCompareVersionCarrierImage_L836, 836, "_udCompareVersionCarrierImage");

#define UD_ASSERT(_cond, _record)						\
	sceKernelAssert((_cond), &(_record), __builtin_return_address(0))
#else
#define UD_ASSERT(_cond, _record)	do { } while (0)
#endif

/* name unknown: the firmware types by the names udRamIoOpen_QMDM6200 opens them by */
static const struct {
	SceUInt32 type;
	const char *name;
} udFirmNames[9] = {
	{ UD_FIRM_ENPRG_HEX,	"ENPRG_HEX" },
	{ UD_FIRM_PARTITION,	"PARTITION" },
	{ UD_FIRM_DBL,		"DBL" },
	{ UD_FIRM_OSBL,		"OSBL" },
	{ UD_FIRM_SAFE,		"SAFE" },
	{ UD_FIRM_UQCN_GENERIC,	"UQCN_GENERIC" },
	{ UD_FIRM_UQCN_VF,	"UQCN_VF" },
	{ UD_FIRM_UQCN_ATT,	"UQCN_ATT" },
	{ UD_FIRM_UQCN_DOCOMO,	"UQCN_DOCOMO" },
};

UdContext *udContext;
SceUInt32 udRamIoEnabled;
SceUInt8 udSizeBuffer[1024];

/* name unknown: the firmware of the list with the record's id and chunk, added if missing */
static __attribute__((noinline)) UdFirmInfo *udAddFirmInfo(UdFirmList *list, const UdFirmRecord *record)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	UdFirmInfo *last;

	if (record == NULL || list == NULL)
		return NULL;
	for (firm = list->first; firm != NULL; firm = firm->next)
		if (firm->offset == record->offset && firm->chunkIndex == record->chunkIndex)
			return firm;

	firm = sceKernelAllocHeapMemory(context->heapId, sizeof(*firm));
	if (firm == NULL)
		return NULL;
	memset(firm, 0, sizeof(*firm));
	firm->type = record->type;
	firm->chunkIndex = record->chunkIndex;
	firm->offset = record->offset;
	firm->recordCompression = record->compression;
	firm->recordSize = record->size;
	firm->recordStoredSize = record->storedSize;
	__builtin_memcpy(firm->name, record->name, sizeof(firm->name));
	firm->compression = firm->recordCompression;
	firm->storedSize = firm->recordStoredSize;
	firm->size = firm->recordSize;
	firm->position = 0;

	last = list->first;
	if (last == NULL) {
		list->first = firm;
	} else {
		while (last->next != NULL)
			last = last->next;
		last->next = firm;
	}
	list->count++;
	return firm;
}

/* the firmwares of the modem's list _index_ */
static inline SceUInt32 _udGetSubListLenOfFirmList(const UdModemFirmList *list, int index)
{
	UD_ASSERT(list != NULL, dbg__udGetSubListLenOfFirmList_L396);
	return list->subList[index].count;
}

static inline UdModemFirm *_udGetSubListOfFirmList(UdModemFirmList *list, int index)
{
	UD_ASSERT(list != NULL, dbg__udGetSubListOfFirmList_L417);
	return list->subList[index].firm;
}

/* whether exactly one of the modem's UQCN firmwares has the name */
static inline int _udMatchUqcnBuildId(UdModemFirmList *list, const char *name,
				      SceUInt32 length)
{
	UdModemFirm *firm;
	SceUInt32 count;
	SceUInt32 i;
	int matches = 0;

	UD_ASSERT(list != NULL, dbg__udMatchUqcnBuildId_L442);
	UD_ASSERT(name != NULL, dbg__udMatchUqcnBuildId_L443);
	UD_ASSERT(length != 0, dbg__udMatchUqcnBuildId_L444);
	count = _udGetSubListLenOfFirmList(list, 1);
	firm = _udGetSubListOfFirmList(list, 1);
	for (i = 0; i < count; i++)
		if (firm[i].nameLength == length && memcmp(firm[i].name, name, length) == 0)
			matches++;
	return matches == 1;
}

/* the modem's AMSS firmware in the slot */
static inline UdModemFirm *_udGetAmssFirmInfoFromSlotNum(UdModemFirmList *list, SceUInt32 slot)
{
	UdModemFirm *firm;
	SceUInt32 count;
	SceUInt32 i;

	UD_ASSERT(list != NULL, dbg__udGetAmssFirmInfoFromSlotNum_L809);
	count = _udGetSubListLenOfFirmList(list, 0);
	firm = _udGetSubListOfFirmList(list, 0);
	for (i = 0; i < count; i++)
		if (firm[i].slot == slot)
			return &firm[i];
	return NULL;
}

/* the modem's firmware of the list _kind_ with the name kept: taken off the delete list */
static inline int udRemoveDeleteList(const char *name, SceUInt32 length, SceUInt32 kind)
{
	UdContext *context = udGetContext();
	UdDeleteEntry *entry;
	SceUInt32 count;
	SceUInt32 i;
	int result = (int)0x80330002;

	UD_ASSERT(name != NULL, dbg_udRemoveDeleteList_L2601);
	UD_ASSERT(length != 0, dbg_udRemoveDeleteList_L2602);
	UD_ASSERT(context != NULL, dbg_udRemoveDeleteList_L2603);
	count = context->work.deleteCount;
	entry = context->work.deleteList;
	for (i = 0; i < count; i++, entry++) {
		if (entry->kind == kind && entry->nameLength == length &&
		    memcmp(entry->name, name, length) == 0) {
			entry->nameLength = 0;
			context->work.deleteRemaining--;
			result = 0;
		}
	}
	return result;
}

/*
 * The carriers whose AMSS and UQCN firmwares the modem already has are taken off the
 * delete list; the others are marked to be written.
 */
int _udCompareVersionCarrierImage(UdQmdm6200Work *work, UdModemFirmList *list)
{
	UdModemFirm *info;
	UdCarrier *carrier;
	SceUInt32 length;
	SceUInt32 uqcnLength;
	SceUInt32 i;
	int match;
	int result;

	UD_ASSERT(work != NULL, dbg__udCompareVersionCarrierImage_L835);
	UD_ASSERT(list != NULL, dbg__udCompareVersionCarrierImage_L836);
	info = _udGetAmssFirmInfoFromSlotNum(list, 0);
	for (i = 0; i < work->carrierCount; i++) {
		carrier = &work->carriers[i];
		if (info != NULL && info->status == 1) {
			carrier->update = 1;
			continue;
		}
		length = strnlen(carrier->firm[0]->name, sizeof(carrier->firm[0]->name));
		info = _udGetAmssFirmInfoFromSlotNum(list, carrier->id);
		if (info == NULL || info->nameLength != length ||
		    memcmp(info->name, carrier->firm[0]->name, length) != 0) {
			carrier->update = 1;
			continue;
		}
		uqcnLength = strnlen(carrier->firm[1]->name, sizeof(carrier->firm[1]->name));
		match = _udMatchUqcnBuildId(list, carrier->firm[1]->name, uqcnLength);
		if (!match || _udGetSubListLenOfFirmList(list, 1) > 1)
			carrier->update = 1;
		udRemoveDeleteList(carrier->firm[0]->name, info->nameLength, 0);
		if (match) {
			result = udRemoveDeleteList(carrier->firm[1]->name, uqcnLength, 1);
			if (result != 0)
				return result;
		}
	}
	return 0;
}

__attribute__((noinline)) void udReleaseImage_QMDM6200(UdContext *context, UdQmdm6200Work *work)
{
	UdFirmInfo *firm;
	UdFirmInfo *next;

	if (work == NULL || context == NULL)
		return;
	if (work->deleteList != NULL) {
		sceKernelFreeHeapMemory(context->heapId, work->deleteList);
		work->deleteRemaining = 0;
		work->deleteList = NULL;
	}
	if (work->carriers != NULL) {
		sceKernelFreeHeapMemory(context->heapId, work->carriers);
		work->carrierCount = 0;
		work->carriers = NULL;
	}
	firm = work->firmList.first;
	work->firmList.count = 0;
	while (firm != NULL) {
		next = firm->next;
		if (firm->recordCompression == UD_COMPRESSION_BZIP2)
			udRamFileClose(context, &firm->ramFile);
		if (firm->ownsData == 1 && firm->loadedData != NULL) {
			sceKernelFreeHeapMemory(context->heapId, firm->loadedData);
			firm->loadedData = NULL;
		}
		sceKernelFreeHeapMemory(context->heapId, firm);
		firm = next;
	}
}

/* name unknown: a carrier record's firmware as a firmware record of type 0 */
static void udGetCarrierFirmRecord(UdFirmRecord *record, const UdCarrierFirmRecord *firm)
{
	record->type = 0;
	__builtin_memcpy(record->name, firm->name, sizeof(record->name));
	record->chunkIndex = firm->chunkIndex;
	record->offset = firm->offset;
	record->compression = firm->compression;
	record->size = firm->size;
	record->storedSize = firm->storedSize;
}

int udParseImage_QMDM6200(const SceUInt8 **cursor, SceUInt32 *length)
{
	UdContext *context;
	const UdFirmRecord *record;
	const UdCarrierRecord *carrierRecord;
	const SceUInt8 *data = NULL;
	const SceUInt8 *carrierData;
	UdCarrierGroup group;
	UdFirmRecord firmRecord;
	UdCarrier *carrier;
	UdFirmInfo *firm;
	SceUInt32 configurationMode;
	SceUInt32 remaining = 0;
	SceUInt32 total;
	SceUInt16 firmCount;
	SceUInt16 groupCount;
	SceUInt32 i;
	int result;

	if (cursor == NULL) {
		result = (int)0x80330002;
		goto out_length;
	}
	data = *cursor;
	if (data == NULL || length == NULL || (total = *length) == 0) {
		data = NULL;
		goto invalid;
	}

	/* the firmware records */
	remaining = total;
	if (remaining < 2)
		goto invalid;
	remaining -= 2;
	udCopyBytes(&firmCount, &data, sizeof(firmCount));
	context = udGetContext();
	if (data == NULL || remaining == 0) {
		data = NULL;
		remaining = 0;
		goto invalid;
	}
	sceKernelSpinlockLowLock(&context->lock);
	record = (const UdFirmRecord *)data;
	for (i = 0; i < firmCount; i++) {
		if (remaining < sizeof(*record)) {
			result = (int)0x80330002;
			goto firm_unlock;
		}
		firm = udAddFirmInfo(&context->work.firmList, record);
		if (firm == NULL) {
			result = (int)0x80330005;
			goto firm_unlock;
		}
		switch (record->type) {
		case UD_FIRM_ENPRG_HEX:
			context->work.enprgHex.firm = firm;
			break;
		case UD_FIRM_PARTITION:
			context->work.partition.firm = firm;
			break;
		case UD_FIRM_DBL:
			context->work.dbl.firm = firm;
			break;
		case UD_FIRM_OSBL:
			context->work.osbl.firm = firm;
			break;
		case UD_FIRM_SAFE:
			context->work.safe.firm = firm;
			break;
		}
		remaining -= sizeof(*record);
		record++;
	}
	sceKernelSpinlockLowUnlock(&context->lock);
	data = (const SceUInt8 *)record;

	/* the carrier records of the configuration mode */
	if (remaining < 2)
		goto invalid;
	udCopyBytes(&groupCount, &data, sizeof(groupCount));
	remaining -= 2;
	context = udGetContext();
	carrierData = NULL;
	configurationMode = context->configurationMode;
	if (groupCount == 0 || data == NULL || remaining == 0) {
		data = NULL;
		remaining = 0;
		goto invalid;
	}
	carrierData = data;
	sceKernelSpinlockLowLock(&context->lock);
	for (i = 0; ; ) {
		if (remaining < sizeof(group)) {
			result = (int)0x80330002;
			goto carrier_unlock;
		}
		remaining -= sizeof(group);
		udCopyBytes(&group, &carrierData, sizeof(group));
		if (configurationMode == group.configurationMode)
			break;
		i++;
		remaining -= group.count * sizeof(UdCarrierRecord);
		carrierData += group.count * sizeof(UdCarrierRecord);
		if (groupCount <= i) {
			result = 0;
			goto carrier_unlock;
		}
	}

	context->work.carriers = sceKernelAllocHeapMemory(context->heapId,
							   group.count * sizeof(UdCarrier));
	if (context->work.carriers == NULL) {
		result = (int)0x80330005;
		goto carrier_unlock;
	}
	memset(context->work.carriers, 0, group.count * sizeof(UdCarrier));
	context->work.carrierCount = group.count;
	carrier = context->work.carriers;
	for (i = 0; i < group.count; i++) {
		if (remaining < sizeof(UdCarrierRecord)) {
			result = (int)0x80330002;
			goto carrier_unlock;
		}
		carrierRecord = (const UdCarrierRecord *)carrierData;
		udGetCarrierFirmRecord(&firmRecord, &carrierRecord->firm[0]);
		firm = udAddFirmInfo(&context->work.firmList, &firmRecord);
		if (firm == NULL) {
			result = (int)0x80330005;
			goto carrier_unlock;
		}
		carrier->id = carrierRecord->id;
		carrier->firm[0] = firm;
		udGetCarrierFirmRecord(&firmRecord, &carrierRecord->firm[1]);
		firm = udAddFirmInfo(&context->work.firmList, &firmRecord);
		if (firm == NULL) {
			result = (int)0x80330005;
			goto carrier_unlock;
		}
		firm->secondary = 1;
		remaining -= sizeof(UdCarrierRecord);
		carrier->firm[1] = firm;
		carrierData += sizeof(UdCarrierRecord);
		carrier = &context->work.carriers[i + 1];
	}
	result = 0;

carrier_unlock:
	sceKernelSpinlockLowUnlock(&context->lock);
	data = carrierData;
	goto out;

firm_unlock:
	sceKernelSpinlockLowUnlock(&context->lock);
	data = (const SceUInt8 *)record;
	goto out;

invalid:
	result = (int)0x80330002;
out:
	*cursor = data;
out_length:
	if (length != NULL)
		*length = remaining;
	return result;
}

int udPrepareImage_QMDM6200(SceUInt32 configurationMode, UdQmdm6200Work *work)
{
	UdFirmInfo *firm;
	UdCarrier *carrier;
	SceUInt32 count;
	SceUInt32 i;
	int hasEnprgHex = 0;
	int hasPartition = 0;
	int hasDbl = 0;
	int hasOsbl = 0;
	int hasSafe = 0;

	if (work == NULL)
		return (int)0x80330002;
	if (work->firmList.count == 0 && work->carrierCount == 0)
		return (int)0x80330705;

	firm = work->firmList.first;
	carrier = work->carriers;
	if (firm != NULL) {
		for (; firm != NULL; firm = firm->next) {
			if (firm->recordStoredSize == 0 || firm->recordSize == 0)
				return (int)0x80330002;
			switch (firm->type) {
			case UD_FIRM_ENPRG_HEX:
				hasEnprgHex = 1;
				break;
			case UD_FIRM_PARTITION:
				hasPartition = 1;
				break;
			case UD_FIRM_DBL:
				hasDbl = 1;
				break;
			case UD_FIRM_OSBL:
				hasOsbl = 1;
				break;
			case UD_FIRM_SAFE:
				hasSafe = 1;
				break;
			}
		}
		if ((hasDbl || hasOsbl || hasSafe) && (hasEnprgHex & hasPartition))
			work->hasFirmImages = 1;
	}

	count = work->carrierCount;
	if (count == 0)
		return 0;
	for (i = 0; i < count; i++, carrier++)
		if (carrier->firm[0] == NULL || carrier->firm[1] == NULL ||
		    carrier->id - 1 > 4)
			return (int)0x80330002;
	work->hasCarrierImages = 1;
	return 0;
}

int udResetDeleteList_QMDM6200(void)
{
	UdContext *context = udGetContext();
	SceUInt32 i;

	context->work.dbl.update = 0;
	context->work.osbl.update = 0;
	context->work.safe.update = 0;
	for (i = 0; i < context->work.carrierCount; i++)
		context->work.carriers[i].update = 0;
	return 0;
}

/* name unknown: whether the modem's AMSS in slot 0 has the image's SAFE name */
static inline int _udMatchSafeBuildId(UdModemFirmList *list, const char *name, SceUInt32 length)
{
	UdModemFirm *firm;
	SceUInt32 count;
	SceUInt32 i;

	UD_ASSERT(list != NULL, dbg__udMatchSafeBuildId_L732);
	UD_ASSERT(name != NULL, dbg__udMatchSafeBuildId_L733);
	UD_ASSERT(length != 0, dbg__udMatchSafeBuildId_L734);
	count = _udGetSubListLenOfFirmList(list, 0);
	firm = _udGetSubListOfFirmList(list, 0);
	for (i = 0; i < count; i++)
		if (firm[i].slot == 0)
			return firm[i].nameLength == length &&
			       memcmp(firm[i].name, name, length) == 0;
	return 0;
}

/* name unknown: the index of the first _c_ in the string */
static inline int udFindCharacter(const char *string, int length, char c, int *index)
{
	int i;

	if (string == NULL || length <= 0)
		return (int)0x80330002;
	for (i = 0; i < length; i++) {
		if (string[i] == c) {
			*index = i;
			return 0;
		}
	}
	return (int)0x8033070C;
}

/* name unknown: the decimal number the string starts with */
static inline int udParseDecimal(const char *string, int length, SceUInt32 *value)
{
	SceUInt32 number = 0;

	if (string == NULL || length == 0)
		return (int)0x80330002;
	if ((SceUInt8)(*string - '0') > 9)
		return (int)0x80330002;
	do {
		number = number * 10 + (*string - '0');
		string++;
		length--;
	} while (length != 0 && (SceUInt8)(*string - '0') <= 9);
	*value = number;
	return 0;
}

/*
 * Whether the modem's SAFE (its AMSS in slot 0) is not older than the image's: the number
 * after the second '-' of the names, then up to three '.'-separated numbers after the
 * next '-'. Sony's code looks for the modem's separators after the first number in the
 * image's name.
 */
static inline int _udCheckUpdateNoNeedFromSafeBuildId(UdModemFirmList *list, const char *name,
						      SceUInt32 length, int *noNeed)
{
	UdModemFirm *firm;
	const char *modemName;
	SceUInt32 modemLength;
	SceUInt32 number;
	SceUInt32 modemNumber;
	SceUInt32 count;
	SceUInt32 i;
	int first;
	int second;
	int start;
	int modemStart;
	int position;
	int modemPosition;
	int separator;
	int fields;
	int result;

	UD_ASSERT(list != NULL, dbg__udCheckUpdateNoNeedFromSafeBuildId_L774);
	UD_ASSERT(name != NULL, dbg__udCheckUpdateNoNeedFromSafeBuildId_L775);
	UD_ASSERT(length != 0, dbg__udCheckUpdateNoNeedFromSafeBuildId_L776);
	UD_ASSERT(noNeed != NULL, dbg__udCheckUpdateNoNeedFromSafeBuildId_L777);
	count = _udGetSubListLenOfFirmList(list, 0);
	firm = _udGetSubListOfFirmList(list, 0);
	for (i = 0; i < count; i++)
		if (firm[i].slot == 0)
			break;
	if (i == count)
		return (int)0x80330002;
	modemName = firm[i].name;
	modemLength = firm[i].nameLength;
	if (name == NULL || modemLength == 0 || length == 0)
		return (int)0x80330002;

	/* the number after the second '-' */
	result = udFindCharacter(name, length, '-', &first);
	if (result < 0)
		return result;
	if ((SceUInt32)first >= length)
		return (int)0x8033070C;
	result = udFindCharacter(name + first + 1, length - (first + 1), '-', &second);
	if (result < 0)
		return result;
	if (second >= (int)length - (first + 1))
		return (int)0x8033070C;
	start = first + 2 + second;
	result = udParseDecimal(name + start, length - start, &number);
	if (result < 0)
		return result;

	result = udFindCharacter(modemName, modemLength, '-', &first);
	if (result < 0)
		return result;
	if ((SceUInt32)first >= modemLength)
		return (int)0x8033070C;
	result = udFindCharacter(modemName + first + 1, modemLength - (first + 1), '-', &second);
	if (result < 0)
		return result;
	if (second >= (int)modemLength - (first + 1))
		return (int)0x8033070C;
	modemStart = first + 2 + second;
	result = udParseDecimal(modemName + modemStart, modemLength - modemStart, &modemNumber);
	if (result < 0)
		return result;

	if (number < modemNumber) {
		*noNeed = 0;
		return 0;
	}
	if (modemNumber != number) {
		*noNeed = 1;
		return 0;
	}

	/* the same number: the '.'-separated numbers after the next '-' */
	result = udFindCharacter(name + start, length - start, '-', &separator);
	if (result < 0)
		return result;
	if (separator >= (int)length - start)
		return (int)0x8033070C;
	position = start + 1 + separator;
	result = udFindCharacter(name + modemStart, length - modemStart, '-', &separator);
	if (result < 0)
		return result;
	if (separator >= (int)length - modemStart)
		return (int)0x8033070C;
	modemPosition = modemStart + 1 + separator;

	for (fields = 3; ; ) {
		if ((SceUInt32)modemPosition >= modemLength || (SceUInt32)position >= length)
			break;
		result = udParseDecimal(modemName + modemPosition, modemLength - modemPosition,
					&modemNumber);
		if (result < 0)
			return result;
		result = udParseDecimal(name + position, length - position, &number);
		if (result < 0)
			return result;
		if (modemNumber > number) {
			*noNeed = 0;
			return 0;
		}
		if (modemNumber < number || --fields == 0)
			break;
		result = udFindCharacter(name + position, length - position, '.', &separator);
		if (result < 0)
			return result;
		if (separator >= (int)length - position)
			return (int)0x8033070C;
		position += separator + 1;
		result = udFindCharacter(name + modemPosition, length - modemPosition, '.',
					 &separator);
		if (result < 0)
			return result;
		if (separator >= (int)length - modemPosition)
			return (int)0x8033070C;
		modemPosition += separator + 1;
	}
	*noNeed = 1;
	return 0;
}

/* name unknown: a boot loader version name, digits only */
static inline int udGetDecimalName(const char *name, SceUInt32 *value)
{
	SceUInt32 number = 0;
	int i;

	for (i = 0; i < 32; i++) {
		if ((SceUInt8)(name[i] - '0') > 9)
			break;
		number = number * 10 + (name[i] - '0');
	}
	if (i == 0 || i == 32 || name[i] != '\0')
		return (int)0x80330002;
	*value = number;
	return 0;
}

int udCheckUpdateNoNeed_QMDM6200(const UdModemInformation *information)
{
	UdContext *context = udGetContext();
	UdQmdm6200Work *work = &context->work;
	UdModemFirmList *list;
	const char *name;
	SceUInt32 length;
	SceUInt32 version;
	SceUInt32 i;
	int noNeed;
	int result;

	if (information == NULL)
		return (int)0x80330002;
	list = information->firmList;
	if (work->partition.firm == NULL)
		return _udCompareVersionCarrierImage(work, list);

	if (work->updateAllRequested == 1 && work->updateAllMode == 0) {
		if (work->enprgHex.firm == NULL || work->dbl.firm == NULL ||
		    work->osbl.firm == NULL || work->safe.firm == NULL ||
		    work->carrierCount == 0)
			return (int)0x80330002;
		work->dbl.update = 1;
		work->osbl.update = 1;
		work->safe.update = 1;
		for (i = 0; i < work->carrierCount; i++)
			work->carriers[i].update = 1;
		return 0;
	}

	if (work->dbl.firm != NULL &&
	    (udGetDecimalName(work->dbl.firm->name, &version) < 0 ||
	     information->dblVersion != version))
		work->dbl.update = 1;
	if (work->osbl.firm != NULL &&
	    (udGetDecimalName(work->osbl.firm->name, &version) < 0 ||
	     information->osblVersion != version))
		work->osbl.update = 1;
	if (work->safe.firm != NULL) {
		name = work->safe.firm->name;
		length = strnlen(name, sizeof(work->safe.firm->name));
		if (!_udMatchSafeBuildId(list, name, length)) {
			result = _udCheckUpdateNoNeedFromSafeBuildId(list, work->safe.firm->name,
								     length, &noNeed);
			if (result < 0)
				return result;
			if (!noNeed || work->dbl.update == 1 || work->osbl.update == 1) {
				work->safe.update = 1;
				return udSetUpdateCarrierImages_QMDM6200();
			}
		}
	}
	if (work->safe.update == 1)
		return udSetUpdateCarrierImages_QMDM6200();
	return _udCompareVersionCarrierImage(work, list);
}

int udSetUpdateFirmImages_QMDM6200(void)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL || context->state <= 2)
		return (int)0x80330003;
	if (context->work.dbl.firm != NULL)
		context->work.dbl.update = 1;
	if (context->work.osbl.firm != NULL)
		context->work.osbl.update = 1;
	if (context->work.safe.firm != NULL)
		context->work.safe.update = 1;
	if (context->work.enprgHex.firm != NULL && context->work.partition.firm != NULL &&
	    (context->work.dbl.update == 1 || context->work.osbl.update == 1 ||
	     context->work.safe.update == 1)) {
		result = 0;
	} else {
		result = (int)0x8033070E;
		context->work.dbl.update = 0;
		context->work.osbl.update = 0;
		context->work.safe.update = 0;
	}
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 4;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udSetUpdateCarrierImages_QMDM6200(void)
{
	UdContext *context = udGetContext();
	SceUInt32 i;

	if (context == NULL || context->state <= 2)
		return (int)0x80330003;
	for (i = 0; i < context->work.carrierCount; i++)
		context->work.carriers[i].update = 1;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 4;
	sceKernelSpinlockLowUnlock(&context->lock);
	return 0;
}

int udIsFirmUpdateNeeded_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context == NULL || context->state <= 3)
		return 0;
	if (context->work.dbl.update + context->work.osbl.update + context->work.safe.update == 0)
		return 0;
	context->work.dbl.update = 1;
	return 1;
}

int udIsCarrierUpdatePending_QMDM6200(void)
{
	UdContext *context = udGetContext();
	UdCarrier *carrier;
	SceUInt32 i;

	if (context == NULL || context->state <= 3)
		return 0;
	carrier = context->work.carriers;
	for (i = 0; i < context->work.carrierCount; i++, carrier++)
		if (carrier->update == 1 && carrier->written == 0)
			return 1;
	return 0;
}

int udIsUpdateAllRequested_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	return context->state > 2 && context->work.updateAllRequested == 1 &&
	       context->work.updateAllMode == 0 && context->work.backupSaved == 0;
}

int udIsBackupRestoreNeeded_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	return context->state > 3 && context->work.backupSaved == 1 &&
	       context->work.backupRestored == 0;
}

int udIsImageVersionChanged_QMDM6200(SceUInt32 version)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	return context->state > 3 && context->imageVersion != version;
}

int udCreateBackupImage_QMDM6200(void)
{
	UdContext *context = udGetContext();
	UdFirmRecord record;
	UdFirmInfo *firm;
	SceUInt8 *data;
	int result;

	if (context == NULL || context->state <= 2)
		return (int)0x80330003;
	data = sceKernelAllocHeapMemory(context->heapId, UD_BACKUP_SIZE);
	if (data == NULL)
		return (int)0x80330005;
	sceKernelSpinlockLowLock(&context->lock);
	record.chunkIndex = 0xFFFF;
	record.offset = 0;
	record.compression = UD_COMPRESSION_NONE;
	record.size = UD_BACKUP_SIZE;
	record.storedSize = UD_BACKUP_SIZE;
	record.type = 0;
	memset(record.name, 0, sizeof(record.name));
	firm = udAddFirmInfo(&context->work.firmList, &record);
	if (firm != NULL) {
		result = 0;
		firm->loadedData = data;
		firm->position = 0;
		firm->storedSize = firm->recordStoredSize;
		firm->size = firm->recordSize;
		firm->compression = firm->recordCompression;
		firm->ownsData = 1;
		context->work.backupSaved = 0;
		context->work.backupRestored = 0;
		context->work.backupFirm = firm;
	} else {
		result = (int)0x80330005;
	}
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udSetBackupSaved_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context == NULL || context->state <= 2)
		return (int)0x80330003;
	context->work.backupSaved = 1;
	return 0;
}

int udSetBackupRestored_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context != NULL && context->state > 3)
		context->work.backupRestored = 1;
	return 0;
}

/* a carrier's UQCN data, copied out of the chunk the next one replaces */
static inline int _udAddUqcnData(UdFirmInfo *firm)
{
	UdContext *context = udGetContext();
	const SceUInt8 *image = context->imageData;
	SceUInt8 *data;

	UD_ASSERT(firm != NULL, dbg__udAddUqcnData_L964);
	data = sceKernelAllocHeapMemory(context->heapId, firm->recordStoredSize);
	if (data == NULL)
		return (int)0x80330005;
	firm->ownsData = 1;
	firm->loadedData = data;
	firm->position = 0;
	firm->storedSize = firm->recordStoredSize;
	firm->compression = firm->recordCompression;
	firm->size = firm->recordSize;
	memcpy(data, image + firm->offset, firm->recordStoredSize);
	return 0;
}

/* the firmwares of the chunk just received pointed at it */
int udReceiveImageChunk_QMDM6200(void)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	SceUInt32 base;
	int result = 0;

	sceKernelSpinlockLowLock(&context->lock);
	for (firm = context->work.firmList.first; firm != NULL; firm = firm->next) {
		if (firm->ownsData == 1)
			continue;
		if (firm->recordCompression == UD_COMPRESSION_BZIP2) {
			result = udRamFileClose(context, &firm->ramFile);
			if (result < 0)
				goto out;
		}
		firm->data = NULL;
		firm->position = 0;
		memset(&firm->ramFile, 0, sizeof(firm->ramFile));
		if (context->chunkIndex != firm->chunkIndex)
			continue;

		base = (SceUInt32)context->imageData;
		if (~base < firm->offset || ~base - firm->offset < firm->recordStoredSize ||
		    base + context->imageSize < base + firm->offset + firm->recordStoredSize) {
			sceKernelSpinlockLowUnlock(&context->lock);
			return (int)0x8033070E;
		}
		if (firm->secondary == 1) {
			result = _udAddUqcnData(firm);
			if (result < 0)
				goto out;
		} else {
			firm->data = context->imageData + firm->offset;
			firm->ownsData = 0;
			firm->position = 0;
			firm->storedSize = firm->recordStoredSize;
			firm->compression = firm->recordCompression;
			firm->size = firm->recordSize;
		}
		if (firm->recordCompression == UD_COMPRESSION_BZIP2) {
			result = udRamFileOpen(context, &firm->ramFile,
					       (char *)(firm->ownsData == 1 ? firm->loadedData :
							firm->data),
					       firm->storedSize);
			if (result < 0)
				goto out;
		}
	}
out:
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

/* name unknown: a firmware opened for reading from its start */
static inline int udOpenFirmInfo(UdFirmInfo *firm)
{
	UdRamFile *file;

	if (firm == NULL || firm->opened == 1)
		return (int)0x80330002;
	firm->opened = 1;
	firm->position = 0;
	if (firm->compression == UD_COMPRESSION_BZIP2) {
		file = &firm->ramFile;
		file->outLength = 0;
		file->outBase = 0;
		file->outPosition = 0;
		file->stream.next_in = file->compressedData;
		file->stream.avail_in = file->compressedSize;
		file->stream.next_out = file->outBuffer;
		file->stream.avail_out = file->outBufferSize;
		if (file->compressed == 1) {
			if (BZ2_bzDecompressEnd(&file->stream) != BZ_OK)
				return (int)0x80330002;
			file->streamEnd = 0;
			if (BZ2_bzDecompressInit(&file->stream, 0, 1) != BZ_OK)
				return (int)0x80330002;
		}
	}
	return 0;
}

int udGetFirmImages_QMDM6200(UdFirmImages *images)
{
	UdContext *context = udGetContext();

	if (images == NULL || context == NULL)
		return (int)0x80330003;
	if (context->state <= 2)
		return (int)0x80330002;
	memset(images, 0, sizeof(*images));
	if (context->work.enprgHex.firm != NULL) {
		if (udOpenFirmInfo(context->work.enprgHex.firm) != 0)
			return (int)0x80330002;
		images->enprgHex = context->work.enprgHex.firm;
	}
	if (context->work.partition.firm != NULL) {
		if (udOpenFirmInfo(context->work.partition.firm) != 0)
			return (int)0x80330002;
		images->partition = context->work.partition.firm;
	}
	if (context->work.dbl.update == 1) {
		if (udOpenFirmInfo(context->work.dbl.firm) != 0)
			return (int)0x80330002;
		images->dbl = context->work.dbl.firm;
	}
	if (context->work.osbl.update == 1) {
		if (udOpenFirmInfo(context->work.osbl.firm) != 0)
			return (int)0x80330002;
		images->osbl = context->work.osbl.firm;
	}
	if (context->work.safe.update == 1) {
		if (udOpenFirmInfo(context->work.safe.firm) != 0)
			return (int)0x80330002;
		images->safe = context->work.safe.firm;
	}
	return 0;
}

int udGetCarrierImages_QMDM6200(UdCarrierImages *images)
{
	UdContext *context = udGetContext();
	UdCarrier *carrier;
	SceUInt32 count;
	SceUInt32 i;

	if (images == NULL || context == NULL)
		return (int)0x80330003;
	if (context->state <= 3)
		return (int)0x80330002;
	carrier = context->work.carriers;
	__builtin_memset(images, 0, sizeof(*images));
	count = context->work.carrierCount;
	for (i = 0; i < count; i++, carrier++)
		if (carrier->update == 1 && carrier->written == 0)
			break;
	if (i == count)
		return 0;
	if (udOpenFirmInfo(carrier->firm[0]) != 0)
		return (int)0x80330002;
	images->firm[0] = carrier->firm[0];
	if (udOpenFirmInfo(carrier->firm[1]) != 0)
		return (int)0x80330002;
	images->firm[1] = carrier->firm[1];
	images->id = carrier->id;
	return 0;
}

int udGetBackupImage_QMDM6200(UdFirmInfo **image)
{
	UdContext *context = udGetContext();

	if (image == NULL || context == NULL)
		return (int)0x80330003;
	if (context->state <= 2)
		return (int)0x80330002;
	if (context->work.backupFirm == NULL)
		return 0;
	if (udOpenFirmInfo(context->work.backupFirm) != 0)
		return (int)0x80330002;
	*image = context->work.backupFirm;
	return 0;
}

int udGetWriteSize_QMDM6200(void)
{
	UdContext *context = udGetContext();
	int size = 0;

	if (context->work.enprgHex.firm != NULL)
		size = context->work.enprgHex.firm->recordSize;
	if (context->work.partition.firm != NULL)
		size += context->work.partition.firm->recordSize;
	if (context->work.dbl.update == 1)
		size += context->work.dbl.firm->recordSize;
	if (context->work.osbl.update == 1)
		size += context->work.osbl.firm->recordSize;
	if (context->work.safe.update == 1)
		size += context->work.safe.firm->recordSize;
	return size;
}

int udGetDeleteSize_QMDM6200(void)
{
	UdContext *context = udGetContext();
	UdCarrier *carrier = context->work.carriers;
	SceUInt32 count = context->work.carrierCount;
	SceUInt32 i;
	int size = 0;

	for (i = 0; i < count; i++, carrier++)
		if (carrier->update == 1 && carrier->written == 0)
			size += carrier->firm[0]->recordSize + carrier->firm[1]->recordSize;
	return size;
}

int udSetCarrierWritten_QMDM6200(SceUInt32 id)
{
	UdContext *context = udGetContext();
	UdCarrier *carrier;
	SceUInt32 count;
	SceUInt32 i;

	if (context == NULL)
		return (int)0x80330003;
	if (context->state <= 3)
		return (int)0x80330002;
	carrier = context->work.carriers;
	count = context->work.carrierCount;
	for (i = 0; i < count; i++, carrier++) {
		if (carrier->id == id && carrier->update != 0 && carrier->written != 1) {
			carrier->written = 1;
			break;
		}
	}
	if (i == count)
		return (int)0x80330002;
	return 0;
}

/* the modem's firmwares the update deletes unless the image has them (udRemoveDeleteList) */
int udCreateDeleteList(UdModemFirmList *list)
{
	UdContext *context = udGetContext();
	UdModemFirm *firm;
	UdDeleteEntry *entry;
	SceUInt32 amssCount;
	SceUInt32 uqcnCount;
	SceUInt32 count;
	SceUInt32 i;

	UD_ASSERT(list != NULL, dbg_udCreateDeleteList_L2509);
	UD_ASSERT(context != NULL, dbg_udCreateDeleteList_L2510);
	amssCount = _udGetSubListLenOfFirmList(list, 0);
	uqcnCount = _udGetSubListLenOfFirmList(list, 1);
	if (amssCount == 0 && uqcnCount == 0)
		return (int)0x80330002;
	count = amssCount + uqcnCount - 1;
	context->work.deleteCount = 0;
	context->work.deleteRemaining = 0;
	context->work.deleteList = NULL;
	if (amssCount + uqcnCount == 1)
		return 0;
	context->work.deleteList = sceKernelAllocHeapMemory(context->heapId,
							     count * sizeof(UdDeleteEntry));
	if (context->work.deleteList == NULL)
		return (int)0x80330005;
	context->work.deleteCount = count;

	firm = _udGetSubListOfFirmList(list, 0);
	for (i = 0; i < amssCount; i++, firm++) {
		if (firm->slot == 0 || firm->nameLength == 0)
			continue;
		if (firm->nameLength > sizeof(entry->name) - 1)
			firm->nameLength = sizeof(entry->name) - 1;
		entry = &context->work.deleteList[context->work.deleteRemaining];
		entry->kind = 0;
		entry->nameLength = firm->nameLength;
		memcpy(entry->name, firm->name, firm->nameLength);
		context->work.deleteList[context->work.deleteRemaining].name[firm->nameLength] = '\0';
		entry = &context->work.deleteList[context->work.deleteRemaining];
		entry->buildId[0] = firm->buildId[0];
		entry->buildId[1] = firm->buildId[1];
		entry->buildId[2] = firm->buildId[2];
		entry->buildId[3] = firm->buildId[3];
		context->work.deleteRemaining++;
	}
	firm = _udGetSubListOfFirmList(list, 1);
	for (i = 0; i < uqcnCount; i++, firm++) {
		if (firm->nameLength > sizeof(entry->name) - 1)
			firm->nameLength = sizeof(entry->name) - 1;
		entry = &context->work.deleteList[context->work.deleteRemaining];
		entry->kind = 1;
		entry->nameLength = firm->nameLength;
		memcpy(entry->name, firm->name, firm->nameLength);
		context->work.deleteList[context->work.deleteRemaining].name[firm->nameLength] = '\0';
		entry = &context->work.deleteList[context->work.deleteRemaining];
		entry->buildId[0] = firm->buildId[0];
		entry->buildId[1] = firm->buildId[1];
		entry->buildId[2] = firm->buildId[2];
		entry->buildId[3] = firm->buildId[3];
		context->work.deleteRemaining++;
	}
	context->work.deleteCount = context->work.deleteRemaining;
	return 0;
}

int udGetDeleteEntry_QMDM6200(UdDeleteEntry *entry)
{
	UdContext *context = udGetContext();
	UdDeleteEntry *list;
	SceUInt32 count;
	SceUInt32 i;

	if (entry == NULL || context == NULL)
		return (int)0x80330003;
	if (context->state <= 3)
		return (int)0x80330002;
	if (context->work.deleteRemaining == 0) {
		entry->nameLength = 0;
		return 0;
	}
	list = context->work.deleteList;
	if (list == NULL)
		return (int)0x80330002;
	count = context->work.deleteCount;
	for (i = 0; i < count; i++) {
		if (list[i].nameLength != 0) {
			*entry = context->work.deleteList[i];
			context->work.deleteList[i].nameLength = 0;
			if (context->work.deleteRemaining-- == 1) {
				sceKernelFreeHeapMemory(context->heapId, context->work.deleteList);
				context->work.deleteList = NULL;
				context->work.deleteCount = 0;
			}
			return 0;
		}
	}
	return 0;
}

int udReleaseDeleteList_QMDM6200(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return (int)0x80330003;
	if (context->state <= 3)
		return (int)0x80330002;
	if (context->work.deleteList != NULL) {
		sceKernelFreeHeapMemory(context->heapId, context->work.deleteList);
		context->work.deleteList = NULL;
		context->work.deleteCount = 0;
		context->work.deleteRemaining = 0;
	}
	return 0;
}

int udRequestUpdateAll_QMDM6200(SceUInt32 mode)
{
	UdContext *context = udGetContext();

	if (context == NULL || context->state <= 2)
		return (int)0x80330002;
	if (context->work.updateAllRequested != 0)
		return 0;
	context->work.updateAllMode = mode;
	context->work.updateAllRequested = 1;
	return 0;
}

int udIsUpdateNeeded_QMDM6200(SceUInt32 *needed)
{
	UdContext *context = udGetContext();

	if (needed == NULL || context == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330002;
	if (context->work.carriers != NULL) {
		*needed = 1;
		return 0;
	}
	*needed = 0;
	return 0;
}

/* name unknown: the firmware an fd of udRamIoOpen_QMDM6200 is */
static inline UdFirmInfo *udFindFirmInfo(SceUID fd)
{
	UdFirmInfo *firm;

	for (firm = udGetContext()->work.firmList.first; firm != NULL; firm = firm->next)
		if (firm == (UdFirmInfo *)fd)
			break;
	return firm;
}

/* the firmware of the image with the name (udFirmNames), opened; the fd is its address */
SceUID udRamIoOpen_QMDM6200(const char *path, int flags, SceMode mode)
{
	UdContext *context = udGetContext();
	UdContext *current;
	UdFirmInfo *firm;
	SceUID result;
	int i;

	sceKernelSpinlockLowLock(&context->lock);
	current = udGetContext();
	for (i = 0; i < 9; i++)
		if (strncmp(path, udFirmNames[i].name, 64) == 0)
			break;
	if (i == 9)
		goto invalid;
	for (firm = current->work.firmList.first; firm != NULL; firm = firm->next)
		if (firm->type == udFirmNames[i].type)
			break;
	if (udOpenFirmInfo(firm) != 0)
		goto invalid;
	result = (SceUID)firm;
	goto out;

invalid:
	result = (int)0x80330002;
out:
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udRamIoClose_QMDM6200(SceUID fd)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	int result;

	sceKernelSpinlockLowLock(&context->lock);
	firm = udFindFirmInfo(fd);
	if (firm == NULL) {
		result = (int)0x80330002;
#if FW_DEBUG
		sceKernelAssert(0, &dbg_udRamIoClose_QMDM6200_L2867, __builtin_return_address(0));
#endif
	} else if (firm->opened != 0) {
		result = 0;
		firm->opened = 0;
	} else {
		result = (int)0x80330002;
	}
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

SceOff udRamIoLseek_QMDM6200(SceUID fd, SceOff offset, int whence)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	SceOff position;
	int result;

	sceKernelSpinlockLowLock(&context->lock);
	firm = udFindFirmInfo(fd);
	if (firm == NULL || firm->opened == 0)
		goto invalid;
	switch (whence) {
	case SCE_SEEK_SET:
		break;
	case SCE_SEEK_CUR:
		offset += firm->position;
		break;
	case SCE_SEEK_END:
		offset += firm->size;
		break;
	default:
		goto invalid;
	}
	if (firm->data == NULL && firm->loadedData == NULL) {
		sceKernelSpinlockLowUnlock(&context->lock);
		result = udWaitImageChunk(firm->chunkIndex);
		sceKernelSpinlockLowLock(&context->lock);
		if (result < 0)
			goto out;
		if (firm->data == NULL && firm->loadedData == NULL)
			goto invalid;
	}
	if (firm->compression == UD_COMPRESSION_NONE) {
		firm->position = offset;
		sceKernelSpinlockLowUnlock(&context->lock);
		return 0;
	}
	if (firm->compression != UD_COMPRESSION_BZIP2)
		goto invalid;
	position = udRamFileSeek(&firm->ramFile, offset);
	if (position >= 0)
		firm->position = offset;
	sceKernelSpinlockLowUnlock(&context->lock);
	return position;

invalid:
	result = (int)0x80330002;
out:
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udRamIoRead_QMDM6200(SceUID fd, void *data, SceSize size)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	const SceUInt8 *source;
	int result;

	if (data == NULL)
		return (int)0x80330002;
	if (size == 0)
		return 0;
	sceKernelSpinlockLowLock(&context->lock);
	firm = udFindFirmInfo(fd);
	if (firm == NULL || firm->opened == 0)
		goto invalid;
	if (firm->data == NULL && firm->loadedData == NULL) {
		sceKernelSpinlockLowUnlock(&context->lock);
		result = udWaitImageChunk(firm->chunkIndex);
		sceKernelSpinlockLowLock(&context->lock);
		if (result < 0)
			goto out;
		if (firm->data == NULL && firm->loadedData == NULL)
			goto invalid;
	}
	if (firm->compression == UD_COMPRESSION_NONE) {
		if (size >= firm->storedSize - firm->position)
			size = firm->storedSize - firm->position;
		if (firm->ownsData == 1)
			source = firm->loadedData + firm->position;
		else
			source = firm->data + firm->position;
		result = size;
		memcpy(data, source, size);
		firm->position += size;
		goto out;
	}
	if (firm->compression == UD_COMPRESSION_BZIP2) {
		result = udRamFileRead(&firm->ramFile, data, size);
		if (result >= 0)
			firm->position += result;
		goto out;
	}

invalid:
	result = (int)0x80330002;
out:
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udRamIoWrite_QMDM6200(SceUID fd, const void *data, SceSize size)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	SceSize length;
	int result;

	if (data == NULL)
		return (int)0x80330002;
	if (size == 0)
		return 0;
	sceKernelSpinlockLowLock(&context->lock);
	firm = udFindFirmInfo(fd);
	if (firm != NULL && firm->opened != 0 && firm->loadedData != NULL &&
	    firm->compression == UD_COMPRESSION_NONE) {
		length = firm->storedSize - firm->position;
		if (size < length)
			length = size;
		result = length;
		memcpy(firm->loadedData + firm->position, data, length);
		firm->position += length;
	} else {
		result = (int)0x80330002;
	}
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

SceOff udRamIoGetSize_QMDM6200(SceUID fd)
{
	UdContext *context = udGetContext();
	UdFirmInfo *firm;
	SceUInt32 size;

	sceKernelSpinlockLowLock(&context->lock);
	firm = udFindFirmInfo(fd);
	if (firm == NULL) {
		sceKernelSpinlockLowUnlock(&context->lock);
		return (int)0x80330002;
	}
	size = firm->size;
	sceKernelSpinlockLowUnlock(&context->lock);
	return size;
}

/* bzip2's allocator: the updater's heap */
static void udBzFree(void *opaque, void *address)
{
	UdContext *context = udGetContext();

	if (address != NULL)
		sceKernelFreeHeapMemory(context->heapId, address);
}

static void *udBzAlloc(void *opaque, int items, int size)
{
	return sceKernelAllocHeapMemory(udGetContext()->heapId, size * items);
}

void bz_internal_error(int errcode)
{
	for (;;)
		;
}

/* name unknown: a compressed firmware read through bzip2, a 16 KiB buffer at a time */
int udRamFileOpen(UdContext *context, UdRamFile *file, char *data, SceUInt32 size)
{
	int result;

	if (data == NULL || file == NULL || size == 0)
		return (int)0x80330002;
	if (file->compressed == 1)
		return 0;
	file->outBuffer = sceKernelAllocHeapMemory(context->heapId, UD_RAM_FILE_BUFFER_SIZE);
	if (file->outBuffer == NULL)
		return (int)0x80330005;
	file->compressedData = data;
	file->compressedSize = size;
	file->outBase = 0;
	file->outPosition = 0;
	file->streamEnd = 0;
	file->stream.opaque = NULL;
	file->stream.bzalloc = udBzAlloc;
	file->stream.bzfree = udBzFree;
	file->outBufferSize = UD_RAM_FILE_BUFFER_SIZE;
	result = BZ2_bzDecompressInit(&file->stream, 0, 1);
	if (result != BZ_OK) {
		sceKernelFreeHeapMemory(context->heapId, file->outBuffer);
		return (int)0x80330002;
	}
	file->compressed = 1;
	file->stream.next_in = file->compressedData;
	file->stream.avail_in = file->compressedSize;
	file->stream.next_out = file->outBuffer;
	file->stream.avail_out = file->outBufferSize;
	return result;
}

/* name unknown: a compressed firmware's buffer freed */
__attribute__((noinline)) int udRamFileClose(UdContext *context, UdRamFile *file)
{
	if (file == NULL)
		return (int)0x80330002;
	if (file->compressed != 0) {
		sceKernelFreeHeapMemory(context->heapId, file->outBuffer);
		memset(file, 0, sizeof(*file));
	}
	return 0;
}

/* name unknown: decompress up to the offset; seeking back starts the stream again */
SceOff udRamFileSeek(UdRamFile *file, SceOff offset)
{
	SceOff current;
	SceOff end;
	SceUInt32 skipped;
	int result;

	if (file == NULL)
		return (int)0x80330002;
	if (file->compressed == 0)
		return (int)0x80330003;
	current = file->outBase + file->outPosition;
	if (offset == current)
		return offset;
	if (offset < current) {
		file->outBase = 0;
		file->outLength = 0;
		file->outPosition = 0;
		if (BZ2_bzDecompressEnd(&file->stream) != BZ_OK)
			return (int)0x80330002;
		file->streamEnd = 0;
		file->stream.avail_out = file->outBufferSize;
		file->stream.next_in = file->compressedData;
		file->stream.avail_in = file->compressedSize;
		file->stream.next_out = file->outBuffer;
		if (BZ2_bzDecompressInit(&file->stream, 0, 1) != BZ_OK)
			return (int)0x80330002;
	} else if (offset > current && file->streamEnd == 1) {
		end = file->outBase + file->outLength;
		if (offset < end)
			end = offset;
		file->outPosition = end - file->outBase;
		return end;
	}
	skipped = 0;
	while (offset >= file->outBase + file->outLength) {
		if (file->stream.avail_out == 0) {
			skipped += file->outLength;
			file->outBase += file->outLength;
			file->outLength = 0;
			file->stream.next_out = file->outBuffer;
			file->stream.avail_out = file->outBufferSize;
		}
		result = BZ2_bzDecompress(&file->stream);
		if (result != BZ_OK && result != BZ_STREAM_END)
			return result;
		file->outLength = file->outBufferSize - file->stream.avail_out;
		if (result == BZ_STREAM_END) {
			file->streamEnd = 1;
			end = file->outBase + file->outLength;
			if (end < offset)
				offset = end;
			break;
		}
		if (skipped >= 0x2800) {
			skipped -= 10240;
			sceKernelDelayThread(1000);
		}
	}
	file->outPosition = offset - file->outBase;
	return offset;
}

/* name unknown: the next bytes of a compressed firmware */
int udRamFileRead(UdRamFile *file, void *data, SceSize size)
{
	SceUInt8 *out = data;
	SceSize length;
	int done = 0;
	int result;

	if (data == NULL || file == NULL || size == 0)
		return (int)0x80330002;
	if (file->compressed == 0)
		return (int)0x80330003;
	for (;;) {
		length = file->outLength - file->outPosition;
		if (size < length)
			length = size;
		memcpy(out + done, file->outBuffer + file->outPosition, length);
		done += length;
		size -= length;
		file->outPosition += length;
		if (size == 0 || file->streamEnd == 1)
			break;
		if (file->stream.avail_out == 0) {
			file->outBase += file->outLength;
			file->outLength = 0;
			file->outPosition = 0;
			file->stream.next_out = file->outBuffer;
			file->stream.avail_out = file->outBufferSize;
		}
		result = BZ2_bzDecompress(&file->stream);
		if (result != BZ_OK && result != BZ_STREAM_END)
			return result;
		if (result == BZ_STREAM_END)
			file->streamEnd = 1;
		file->outLength = file->outBufferSize - file->stream.avail_out;
	}
	return done;
}

__attribute__((noinline)) void udSetContext(UdContext *context)
{
	udContext = context;
}

__attribute__((noinline)) UdContext *udGetContext(void)
{
	return udContext;
}

int udGetHardwareType(const char *hardwareId)
{
	int i;

	for (i = 0; i < 2; i++)
		if (strncmp(udHardwares[i].hardwareId, hardwareId, 31) == 0)
			return udHardwares[i].type;
	return -1;
}

int udPrepareImage(SceUInt32 hardwareType, SceUInt32 configurationMode,
			  UdQmdm6200Work *work)
{
	if (hardwareType <= 1)
		return udPrepareImage_QMDM6200(configurationMode, work);
	return 0;
}

int udWaitImageChunk(SceUInt32 chunkIndex)
{
	UdContext *context = udContext;
	SceUInt32 timeout = 60000000;
	unsigned int pattern = 0;
	int result;

	result = sceKernelClearEventFlag(context->eventFlagId, ~1U);
	if (result < 0)
		return result;
	while (context->chunkIndex < chunkIndex) {
		if (context->requestChunk == NULL)
			return (int)0x80330002;
		result = context->requestChunk(chunkIndex);
		if (result < 0)
			return result;
		result = sceKernelWaitEventFlag(context->eventFlagId, 1, 1, &pattern,
						&timeout);
		if (result < 0)
			return result;
		if ((pattern & 1) == 0)
			return (int)0x80330002;
		result = sceKernelClearEventFlag(context->eventFlagId, ~pattern);
		if (result < 0)
			return result;
	}
	if (chunkIndex < context->chunkIndex)
		result = (int)0x80330002;
	return result;
}

SceUID udRamIoOpen(const char *path, int flags, SceMode mode)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoOpen_QMDM6200(path, flags, mode);
}

int udRamIoClose(SceUID fd)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoClose_QMDM6200(fd);
}

SceOff udRamIoLseek(SceUID fd, SceOff offset, int whence)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoLseek_QMDM6200(fd, offset, whence);
}

int udRamIoRead(SceUID fd, void *data, SceSize size)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoRead_QMDM6200(fd, data, size);
}

int udRamIoWrite(SceUID fd, const void *data, SceSize size)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoWrite_QMDM6200(fd, data, size);
}

SceOff udRamIoGetSize(SceUID fd)
{
	if (udContext == NULL)
		return (int)0x80330003;
	if (udContext->state <= 2 || udContext->hardwareType > 1)
		return (int)0x80330002;
	return udRamIoGetSize_QMDM6200(fd);
}

int udInitialize(SceUInt32 version, const UdClient *client)
{
	UdContext *context;
	SceUID heapId;
	SceUID eventFlagId;

	if (udGetContext() != NULL)
		return (int)0x80330004;
	if (version != 4 || client == NULL)
		return (int)0x80330002;
	heapId = sceKernelCreateHeap("SceUpdaterHeap", 492544, NULL);
	if (heapId < 0)
		return heapId;
	context = sceKernelAllocHeapMemory(heapId, sizeof(*context));
	if (context == NULL) {
		eventFlagId = (int)0x80330005;
	} else {
		memset(context, 0, sizeof(*context));
		context->lock = 0;
		eventFlagId = sceKernelCreateEventFlag("SceBbmcUdMod", 4096, 0, NULL);
		if (eventFlagId >= 0) {
			context->eventFlagId = eventFlagId;
			context->heapId = heapId;
			context->requestChunk = client->requestChunk;
			udSetContext(context);
			return eventFlagId;
		}
		sceKernelFreeHeapMemory(heapId, context);
	}
	sceKernelDeleteHeap(heapId);
	return eventFlagId;
}

int udTerminate(void)
{
	UdContext *context = udGetContext();
	SceUID heapId;

	if (context == NULL)
		return (int)0x80330003;
	heapId = context->heapId;
	udSetContext(NULL);
	if (context->hardwareType <= 1) {
		udReleaseImage_QMDM6200(context, &context->work);
		memset(&context->work, 0, sizeof(context->work));
	}
	sceKernelDeleteEventFlag(context->eventFlagId);
	sceKernelFreeHeapMemory(heapId, context);
	sceKernelDeleteHeap(heapId);
	return 0;
}

int udSetImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		     SceUInt32 size)
{
	UdContext *context = udGetContext();
	SceUInt32 state;

	if (context == NULL)
		return (int)0x80330003;
	if (size == 0 || data == NULL)
		return (int)0x80330002;
	state = context->state;
	if (state != 0)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	context->chunkIndex = chunkIndex;
	context->chunkCount = chunkCount;
	context->imageData = data;
	context->imageSize = size;
	sceKernelSpinlockLowUnlock(&context->lock);
	return state;
}

int udSetTarget(SceUInt32 hardwareType, SceUInt32 configurationMode)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&context->lock);
	context->hardwareType = hardwareType;
	context->configurationMode = configurationMode;
	context->state = 2;
	sceKernelSpinlockLowUnlock(&context->lock);
	return 0;
}

int udLoadImage(void)
{
	UdContext *context = udGetContext();
	UdContext *current;
	UdImageHeader header;
	UdImageEntryHeader entry;
	const SceUInt8 *image;
	const SceUInt8 *cursor;
	const SceUInt8 *data;
	SceUInt32 imageSize;
	SceUInt32 remaining;
	SceUInt32 hardwareType;
	SceUInt32 length;
	int entryType;
	int count;
	int i;
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (context->state != 2 || context->chunkIndex != 0)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	memset(&context->work, 0, sizeof(context->work));
	sceKernelSpinlockLowUnlock(&context->lock);

	image = context->imageData;
	imageSize = context->imageSize;
	current = udGetContext();
	if (image == NULL || imageSize <= 37)
		goto invalid;
	memcpy(&header, image, UD_IMAGE_HEADER_SIZE);
	cursor = image + UD_IMAGE_HEADER_SIZE;
	remaining = imageSize - UD_IMAGE_HEADER_SIZE;
	if (imageSize != header.imageSize)
		goto invalid;
	count = header.entryCount;
	if (count == 0)
		goto invalid;
	current->imageVersion = header.version;
	current = udGetContext();
	hardwareType = current->hardwareType;
	if (remaining == 0 || remaining <= 35)
		goto invalid;

	for (i = 0; ; ) {
		udCopyBytes(&entry, &cursor, sizeof(entry));
		entryType = udGetHardwareType(entry.hardwareId);
		length = entry.size - sizeof(entry);
		if (hardwareType == entryType) {
			data = NULL;
			if (cursor == NULL || entry.size == sizeof(entry))
				goto invalid;
			data = cursor;
			if (hardwareType > 1)
				goto invalid;
			result = udParseImage_QMDM6200(&data, &length);
			if (result < 0)
				goto failed;
			break;
		}
		if (current->imageVersion <= 7 &&
		    ((entryType == 1 && hardwareType == 0) ||
		     (entryType == 0 && hardwareType == 1))) {
			data = NULL;
			if (cursor == NULL || entry.size == sizeof(entry))
				goto invalid;
			data = cursor;
			result = udParseImage_QMDM6200(&data, &length);
			if (result < 0)
				goto failed;
			break;
		}
		i++;
		remaining -= entry.size;
		cursor += length;
		if (count <= i)
			break;
		if (remaining <= 35)
			goto invalid;
	}

	result = udPrepareImage(context->hardwareType, context->configurationMode,
				&context->work);
	if (result < 0)
		goto failed;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 3;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;

invalid:
	result = (int)0x80330002;
failed:
	if (context->hardwareType <= 1) {
		udReleaseImage_QMDM6200(context, &context->work);
		memset(&context->work, 0, sizeof(context->work));
	}
	return result;
}

int udResetDeleteList(void)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL || context->state <= 3)
		return (int)0x80330003;
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = udResetDeleteList_QMDM6200();
	if (result < 0)
		return result;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 3;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udCheckUpdateNoNeed(const SceUInt8 *information)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (information == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330003;
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = udCheckUpdateNoNeed_QMDM6200((const UdModemInformation *)(information + 16));
	if (result < 0)
		return result;
	sceKernelSpinlockLowLock(&context->lock);
	context->state = 4;
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

int udReceiveImageChunk(SceUInt32 chunkCount, SceUInt32 chunkIndex, const void *data,
		       SceUInt32 size)
{
	UdContext *context = udGetContext();
	int result;

	if (context == NULL)
		return (int)0x80330003;
	if (context->state <= 2 || chunkCount <= chunkIndex ||
	    context->chunkCount != chunkCount)
		return (int)0x80330002;
	sceKernelSpinlockLowLock(&context->lock);
	context->chunkIndex = chunkIndex;
	context->imageData = data;
	context->imageSize = size;
	sceKernelSpinlockLowUnlock(&context->lock);
	if (context->hardwareType > 1)
		return (int)0x80330002;
	result = udReceiveImageChunk_QMDM6200();
	if (result < 0)
		return result;
	return sceKernelSetEventFlag(context->eventFlagId, 1);
}

int udGetWriteSize(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	if (context->state > 3 && context->hardwareType <= 1)
		return udGetWriteSize_QMDM6200();
	return 0;
}

int udGetDeleteSize(void)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return 0;
	if (context->state > 3 && context->hardwareType <= 1)
		return udGetDeleteSize_QMDM6200();
	return 0;
}

int udGetImageVersion(SceUInt32 *version)
{
	UdContext *context = udGetContext();

	if (context == NULL)
		return (int)0x80330003;
	if (version == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330003;
	*version = context->imageVersion;
	return 0;
}

int udIsInitialized(void)
{
	return udGetContext() != NULL;
}

int udEnableRamIo(void)
{
	if (udGetContext() == NULL)
		return (int)0x80330003;
	udRamIoEnabled = 1;
	return 0;
}

int udDisableRamIo(void)
{
	udRamIoEnabled = 0;
	return 0;
}

SceUID udIoOpen(const char *path, int flags, SceMode mode)
{
	if (udRamIoEnabled == 1)
		return udRamIoOpen(path, flags, mode);
	return sceIoOpen(path, flags, mode);
}

int udIoClose(SceUID fd)
{
	if (udRamIoEnabled == 1)
		return udRamIoClose(fd);
	return sceIoClose(fd);
}

SceOff udIoLseek(SceUID fd, SceOff offset, int whence)
{
	if (udRamIoEnabled == 1)
		return udRamIoLseek(fd, offset, whence);
	return sceIoLseek(fd, offset, whence);
}

int udIoRead(SceUID fd, void *data, SceSize size)
{
	if (udRamIoEnabled == 1)
		return udRamIoRead(fd, data, size);
	return sceIoRead(fd, data, size);
}

int udIoWrite(SceUID fd, const void *data, SceSize size)
{
	if (udRamIoEnabled == 1)
		return udRamIoWrite(fd, data, size);
	return sceIoWrite(fd, data, size);
}

SceOff udIoGetSize(SceUID fd)
{
	SceOff result;
	SceOff size;
	int read;

	if (udRamIoEnabled == 1)
		return udRamIoGetSize(fd);
	size = 0;
	for (;;) {
		read = sceIoRead(fd, udSizeBuffer, sizeof(udSizeBuffer));
		if (read < 0)
			return read;
		size += read;
		if (read == 0)
			break;
	}
	result = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (result < 0)
		return result;
	return size;
}

/* name unknown: copy size bytes from a cursor and advance it, or only advance it */
__attribute__((noinline)) void udCopyBytes(void *dst, const SceUInt8 **cursor, SceUInt32 size)
{
	SceUInt8 *out = dst;

	if (out == NULL) {
		*cursor += size;
		return;
	}
	while (size-- != 0)
		*out++ = *(*cursor)++;
}

int udReadFileInformation(SceUID fd, UdFileInformation *information)
{
	UdFileTrailer trailer;
	SceOff offset;
	int result;

	if (information == NULL)
		return (int)0x80330003;
	if (udRamIoEnabled == 1)
		offset = udRamIoLseek(fd, -(SceOff)sizeof(trailer), SCE_SEEK_END);
	else
		offset = sceIoLseek(fd, -(SceOff)sizeof(trailer), SCE_SEEK_END);
	if (offset < 0)
		return offset;
	if (udRamIoEnabled == 1)
		result = udRamIoRead(fd, &trailer, sizeof(trailer));
	else
		result = sceIoRead(fd, &trailer, sizeof(trailer));
	if (result < 0)
		return result;
	if (result != sizeof(trailer))
		return (int)0x8033070C;
	if (udRamIoEnabled == 1)
		offset = udRamIoLseek(fd, 0, SCE_SEEK_SET);
	else
		offset = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (offset < 0)
		return offset;
	memset(information, 0, sizeof(*information));
	if (trailer.magic != 0xFEDC1234 || trailer.nameMagic != 0xFEDC1235)
		return (int)0x8033070B;
	memcpy(information->name, trailer.name, sizeof(information->name));
	information->nameLength = strnlen(information->name, 32);
	if (trailer.buildIdMagic != 0xFEDC1237)
		return (int)0x8033070B;
	memcpy(information->buildId, trailer.buildId, sizeof(information->buildId));
	return 0;
}

int udIsUpdateNeeded(SceUInt32 *needed)
{
	UdContext *context = udGetContext();

	if (needed == NULL || context == NULL)
		return (int)0x80330002;
	if (context->state <= 2)
		return (int)0x80330002;
	if (context->hardwareType <= 1)
		return udIsUpdateNeeded_QMDM6200(needed);
	*needed = 0;
	return 0;
}
