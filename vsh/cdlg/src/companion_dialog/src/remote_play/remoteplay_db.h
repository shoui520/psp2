/*
 * SceCompanionDialogPlugin: RemotePlayDB (savedata0:remoteplay_orbis.dat).
 */
#ifndef COMPANION_REMOTEPLAY_DB_H
#define COMPANION_REMOTEPLAY_DB_H

#include <stdint.h>
#include <scetypes.h>

struct RemotePlayRecord {
	char apSsid[33];
	char apBssid[33];
	char apKey[64];
	char apName[61];
	char ps4Mac[12];
	char ps4RegistKey[8];
	char ps4Nickname[125];
	uint8_t rpKeyType;
	uint8_t rpKey[16];
	uint8_t reserved[15];
};

struct RemotePlayDiskRecord {
	char apSsid[33];
	char apBssid[33];
	char apKey[64];
	char apName[61];
	char ps4Mac[12];
	char ps4RegistKey[8];
	char ps4Nickname[125];
	uint8_t rpKeyType;
	uint8_t rpKey[16];
};

class RemotePlayDB {
public:
	RemotePlayDB();
	virtual ~RemotePlayDB();

	SceInt32 Load();
	SceInt32 Save();
	SceInt32 Flush();
	void SetRecord(const RemotePlayRecord *record);
	const RemotePlayRecord *GetRecord() const { return &m_record; }
	uint8_t IsLocked() const;
	uint8_t IsValid() const { return m_isValid; }

private:
	static SceInt32 UnpackRecord(const RemotePlayDiskRecord *src, RemotePlayRecord *dst);
	static SceInt32 PackRecord(const RemotePlayRecord *src, RemotePlayDiskRecord *dst);

	SceInt32 m_reserved;
	SceInt32 m_status;
	RemotePlayRecord m_record;
	uint8_t m_hasPendingUpdate;
	uint8_t m_isLocked;
	uint8_t m_pad[2];
	uint8_t m_extraData[148];
	uint8_t m_isValid;
	uint8_t m_tailPadding[3];
};

#endif /* COMPANION_REMOTEPLAY_DB_H */
