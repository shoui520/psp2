/*
 * ScePhotoImportDialogPlugin: AVContent.db manager, query cell source,
 * and asynchronous thumbnail loader (ScePhotoThumbJob).
 */
#ifndef PHOTOIMPORT_PHOTO_DB_H
#define PHOTOIMPORT_PHOTO_DB_H

#include <scetypes.h>
#include <std/string.h>
#include <std/vector>
#include <thread/thread.h>
#include <common/job_slots.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include <dbutil/value.h>

extern "C" int SceDbrecoveryUtility_4AB1DAA9(SceInt32 kind, const char *path, SceUInt32 flags);

enum PhotoSortOrder {
	PHOTO_SORT_CREATED_TIME_ASC = 0,
	PHOTO_SORT_CREATED_TIME_DESC,
	PHOTO_SORT_IMPORTED_TIME_ASC,
	PHOTO_SORT_IMPORTED_TIME_DESC,
	PHOTO_SORT_APPLI_TITLE_ASC,
	PHOTO_SORT_APPLI_TITLE_DESC,
	PHOTO_SORT_TITLE_ASC,
	PHOTO_SORT_TITLE_DESC,
	PHOTO_SORT_TITLE_SORT_ASC,
	PHOTO_SORT_TITLE_SORT_DESC,
	PHOTO_SORT_CREATED_IMPORTED_ASC,
	PHOTO_SORT_CREATED_IMPORTED_DESC,
	PHOTO_SORT_LOCAL_MONTH_ASC,
	PHOTO_SORT_LOCAL_MONTH_DESC
};

struct PhotoRecordInfo {
	SceInt64 mrid;
	SceInt32 contentType;
	SceInt32 genreType;
	SceInt32 duration;
	SceInt32 width;
	SceInt32 height;
	SceInt32 orientationType;
	SceInt32 codecType;
	paf::string contentPath;
	paf::string title;
	paf::string appliTitle;
	paf::string createdTime;
	paf::string createdLocalMonth;
};

class PhotoDBManager {
public:
	PhotoDBManager();
	~PhotoDBManager();

	SceInt32 Open(const paf::string &mountDbPath, const paf::string &fallbackDbPath,
		      SceUInt32 busyTimeoutMs);
	void Close();
	bool IsOpen() const;

	SceInt32 QueryPhotoList(const paf::string &whereClause, PhotoSortOrder sortOrder,
				SceInt32 limit, SceInt32 offset,
				paf::vector<PhotoRecordInfo> &outItems);
	SceInt32 QueryGroupList(const paf::string &groupColumn, const paf::string &whereClause,
				PhotoSortOrder sortOrder,
				paf::vector<paf::string> &outGroups,
				paf::vector<SceInt32> &outCounts);
	SceInt32 QueryPlaylistItems(SceInt64 listId, SceInt32 itemType,
				    paf::vector<SceInt64> &outItemIds);
	SceInt32 QueryPhotoDetail(SceInt64 mrid, PhotoRecordInfo &outInfo);
	SceInt32 QueryThumbnail(SceInt64 contentId, SceInt32 codecType,
				paf::vector<SceUInt8> &outBlob, float &outAspectRatio);
	SceInt32 RequestRecovery(SceInt32 kind);

	static const char *SortOrderClause(PhotoSortOrder sortOrder);

private:
	dbutil::Connection *m_connection;
	paf::thread::RMutex m_lock;
	paf::string m_dbPath;
	bool m_opened;
};

class ScePhotoThumbJob : public paf::common::SlotJobItem {
public:
	ScePhotoThumbJob(PhotoDBManager *dbManager, SceInt64 contentId,
			 SceInt32 codecType, paf::ui::Widget *targetButton);
	virtual ~ScePhotoThumbJob();

	virtual SceInt32 Run();
	virtual void Finish(SceInt32 result);

private:
	PhotoDBManager *m_dbManager;
	SceInt64 m_contentId;
	SceInt32 m_codecType;
	paf::ui::Widget *m_targetButton;
	paf::vector<SceUInt8> m_thumbBytes;
	float m_aspectRatio;
	bool m_loaded;
};

#endif /* PHOTOIMPORT_PHOTO_DB_H */
