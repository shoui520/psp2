/*
 * SceCompanionDialogPlugin: RemotePlayDB implementation.
 */
#include "remoteplay_db.h"
#include <fw.h>

extern "C" {
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
void *sce_paf_memset(void *dst, int c, SceSize size);
char *sce_paf_strncpy(char *dst, const char *src, SceSize n);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize n);
int sce_paf_strncasecmp(const char *lhs, const char *rhs, SceSize n);
void *sceClibMemset(void *dst, int c, SceSize size);
SceUID sceIoOpen(const char *file, int flags, SceInt32 mode);
SceSSize sceIoRead(SceUID fd, void *data, SceSize size);
SceInt32 sceIoLseek32(SceUID fd, SceInt32 offset, int whence);
SceInt32 sceIoClose(SceUID fd);

struct SceAppUtilSaveDataSlotParam {
	SceUInt32 status;
	char title[64];
	char subTitle[128];
	char detail[512];
	char iconPath[64];
	SceInt32 userParam;
	SceSize sizeKB;
	SceUInt8 modifiedTime[16];
	SceUInt8 reserved[48];
};

struct SceAppUtilSaveDataFileSlot {
	SceUInt32 id;
	SceAppUtilSaveDataSlotParam *slotParam;
	SceUInt8 reserved[32];
};

struct SceAppUtilSaveDataFile {
	const char *filePath;
	void *buf;
	SceSize bufSize;
	SceOff offset;
	SceUInt32 mode;
	SceUInt32 progDelta;
	SceUInt8 reserved[32];
};

struct SceAppUtilSaveDataRemoveItem {
	const char *dataPath;
	SceUInt32 mode;
	SceUInt8 reserved[36];
};

SceInt32 sceAppUtilSaveDataDataRemove(SceAppUtilSaveDataFileSlot *slot,
	SceAppUtilSaveDataRemoveItem *files, SceUInt32 fileNum, void *mountPoint);
SceInt32 sceAppUtilSaveDataDataSave(SceAppUtilSaveDataFileSlot *slot,
	SceAppUtilSaveDataFile *files, SceUInt32 fileNum, void *mountPoint,
	SceSize *requiredSizeKB);
}

static const SceInt32 kErrRemotePlayDbCorrupt = (SceInt32)0x8010D902;

struct DbChunkHeader {
	SceUInt32 tag;
	SceUInt32 size;
};

SceInt32 RemotePlayDB::UnpackRecord(const RemotePlayDiskRecord *src, RemotePlayRecord *dst)
{
	sce_paf_memcpy(dst->apBssid, src->apBssid, sizeof(dst->apBssid));
	sce_paf_memcpy(dst->apKey, src->apKey, sizeof(dst->apKey));
	sce_paf_memcpy(dst->apName, src->apName, sizeof(dst->apName));
	sce_paf_memcpy(dst->apSsid, src->apSsid, sizeof(dst->apSsid));
	dst->rpKeyType = src->rpKeyType;
	sce_paf_memcpy(dst->rpKey, src->rpKey, sizeof(dst->rpKey));
	sce_paf_memcpy(dst->ps4Mac, src->ps4Mac, sizeof(dst->ps4Mac));
	sce_paf_memcpy(dst->ps4Nickname, src->ps4Nickname, sizeof(dst->ps4Nickname));
	sce_paf_memcpy(dst->ps4RegistKey, src->ps4RegistKey, sizeof(dst->ps4RegistKey));
	return 0;
}

SceInt32 RemotePlayDB::PackRecord(const RemotePlayRecord *src, RemotePlayDiskRecord *dst)
{
	sce_paf_memcpy(dst->apBssid, src->apBssid, sizeof(dst->apBssid));
	sce_paf_memcpy(dst->apKey, src->apKey, sizeof(dst->apKey));
	sce_paf_memcpy(dst->apName, src->apName, sizeof(dst->apName));
	sce_paf_memcpy(dst->apSsid, src->apSsid, sizeof(dst->apSsid));
	dst->rpKeyType = src->rpKeyType;
	sce_paf_memcpy(dst->rpKey, src->rpKey, sizeof(dst->rpKey));
	sce_paf_memcpy(dst->ps4Mac, src->ps4Mac, sizeof(dst->ps4Mac));
	sce_paf_memcpy(dst->ps4Nickname, src->ps4Nickname, sizeof(dst->ps4Nickname));
	sce_paf_memcpy(dst->ps4RegistKey, src->ps4RegistKey, sizeof(dst->ps4RegistKey));
	return 0;
}

RemotePlayDB::RemotePlayDB()
	: m_reserved(0)
	, m_status(0)
	, m_hasPendingUpdate(0)
	, m_isLocked(0)
	, m_isValid(0)
{
	sce_paf_memset(&m_record, 0, sizeof(m_record));
	sce_paf_memset(m_extraData, 0, sizeof(m_extraData));
	Load();
}

RemotePlayDB::~RemotePlayDB()
{
}

SceInt32 RemotePlayDB::Load()
{
	RemotePlayDiskRecord diskRec;
	SceUID fileHandle = sceIoOpen("savedata0:remoteplay_orbis.dat", 1, 4);
	SceInt32 ret = fileHandle;

	if (fileHandle >= 0) {
		for (;;) {
			DbChunkHeader hdr;
			SceSSize nread = sceIoRead(fileHandle, &hdr, sizeof(hdr));
			if (nread <= 0) {
				ret = nread;
				break;
			}
			if (nread != (SceSSize)sizeof(hdr)) {
				ret = kErrRemotePlayDbCorrupt;
				break;
			}

			void *dst = 0;
			SceSize expected = 0;
			if (hdr.tag == 1) {
				dst = &diskRec;
				expected = sizeof(diskRec);
			} else if (hdr.tag == 2) {
				dst = &m_isLocked;
				expected = 1;
			} else {
				SceInt32 pos = sceIoLseek32(fileHandle, (SceInt32)hdr.size, 1);
				if (pos >= 0 && pos == (SceInt32)hdr.size)
					continue;
				ret = kErrRemotePlayDbCorrupt;
				break;
			}

			if (expected != hdr.size) {
				ret = kErrRemotePlayDbCorrupt;
				break;
			}
			SceSSize bodyRead = sceIoRead(fileHandle, dst, expected);
			if (bodyRead < 0) {
				ret = bodyRead;
				break;
			}
			if (bodyRead != (SceSSize)expected) {
				ret = kErrRemotePlayDbCorrupt;
				break;
			}
		}
		sceIoClose(fileHandle);
	}

	if (ret >= 0) {
		UnpackRecord(&diskRec, &m_record);
		m_isValid = (sce_paf_strncasecmp(diskRec.apKey, "AP-TESTMODE", 11) != 0) ? 1 : 0;
	}
	return ret;
}

SceInt32 RemotePlayDB::Save()
{
#if FW_TYPE != FW_TYPE_CEX
	if (sce_paf_strncmp(m_record.ps4RegistKey, "75BCD15", 8) == 0)
		return 0;
#endif
	RemotePlayDiskRecord diskRec;
	PackRecord(&m_record, &diskRec);

	uint8_t payload[sizeof(DbChunkHeader) + sizeof(RemotePlayDiskRecord) + sizeof(DbChunkHeader) + 1];
	DbChunkHeader hdr;

	hdr.tag = 1;
	hdr.size = sizeof(RemotePlayDiskRecord);
	sce_paf_memcpy(payload, &hdr, sizeof(hdr));
	sce_paf_memcpy(payload + sizeof(hdr), &diskRec, sizeof(diskRec));

	hdr.tag = 2;
	hdr.size = 1;
	SceSize secondOff = sizeof(hdr) + sizeof(diskRec);
	sce_paf_memcpy(payload + secondOff, &hdr, sizeof(hdr));
	sce_paf_memcpy(payload + secondOff + sizeof(hdr), &m_isLocked, 1);

	char filePath[272];
	sceClibMemset(filePath, 0, sizeof(filePath));
	sce_paf_strncpy(filePath, "remoteplay_orbis.dat", sizeof(filePath));

	SceAppUtilSaveDataRemoveItem removeItem;
	sceClibMemset(&removeItem, 0, sizeof(removeItem));
	removeItem.dataPath = filePath;
	removeItem.mode = 0;
	sceAppUtilSaveDataDataRemove(0, &removeItem, 1, 0);

	SceAppUtilSaveDataSlotParam slotParam;
	sceClibMemset(&slotParam, 0, sizeof(slotParam));
	sce_paf_strncpy(slotParam.title, "RemotePlay[Orbis]", sizeof(slotParam.title) - 1);
	sce_paf_strncpy(slotParam.subTitle, "RemotePlay", sizeof(slotParam.subTitle) - 1);
	sce_paf_strncpy(slotParam.detail, "RemotePlay Data File", sizeof(slotParam.detail) - 1);
	sce_paf_strncpy(slotParam.iconPath, "app0:/icon.png", sizeof(slotParam.iconPath) - 1);

	SceAppUtilSaveDataFileSlot slot;
	sceClibMemset(&slot, 0, sizeof(slot));
	slot.id = 0;
	slot.slotParam = &slotParam;

	SceAppUtilSaveDataFile saveFile;
	sceClibMemset(&saveFile, 0, sizeof(saveFile));
	saveFile.filePath = filePath;
	saveFile.buf = payload;
	saveFile.bufSize = sizeof(payload);
	saveFile.offset = 0;
	saveFile.mode = 0;

	SceSize requiredSizeKB = 0;
	SceInt32 ret = sceAppUtilSaveDataDataSave(&slot, &saveFile, 1, 0, &requiredSizeKB);
	if (ret == 0)
		m_isValid = 1;
	return ret;
}

SceInt32 RemotePlayDB::Flush()
{
	return Save();
}

void RemotePlayDB::SetRecord(const RemotePlayRecord *record)
{
	sce_paf_memcpy(&m_record, record, sizeof(m_record));
}

uint8_t RemotePlayDB::IsLocked() const
{
	return m_isLocked;
}
