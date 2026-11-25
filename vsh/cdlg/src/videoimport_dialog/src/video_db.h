/*
 * SceVideoImportDialogPlugin: AVContent.db manager, SQL query helpers,
 * and asynchronous thumbnail jobs (CreateThumbnailJob / BasicImageJob).
 */
#ifndef VIDEOIMPORT_VIDEO_DB_H
#define VIDEOIMPORT_VIDEO_DB_H

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

enum VideoSortOrder {
	VIDEO_SORT_TITLE_ASC = 0,
	VIDEO_SORT_TITLE_DESC,
	VIDEO_SORT_CREATED_TIME_ASC,
	VIDEO_SORT_CREATED_TIME_DESC,
	VIDEO_SORT_SIZE_ASC,
	VIDEO_SORT_SIZE_DESC
};

struct VideoRecordInfo {
	SceInt64 mrid;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt64 fileSize;
	SceUInt32 duration;
	SceUInt64 lastUpdatedTime;
	SceInt32 parentalLevel;
	SceInt32 status;
	SceInt32 iconOffset;
	SceInt32 iconSize;
	SceInt32 iconProtect;
	paf::string contentPath;
	paf::string title;
	paf::string description;
	paf::string copyright;
	paf::string appliTitleId;
	paf::string iconPath;
};

class VideoDBManager {
public:
	VideoDBManager();
	~VideoDBManager();

	SceInt32 Open(const paf::string &mountDbPath, const paf::string &fallbackDbPath,
		      SceUInt32 busyTimeoutMs);
	void Close();
	bool IsOpen() const;

	SceInt32 QueryVideoList(const paf::string &whereClause, VideoSortOrder sortOrder,
				SceInt32 limit, SceInt32 offset,
				paf::vector<VideoRecordInfo> &outItems);
	SceInt32 QueryAlbumList(const paf::string &listType,
				paf::vector<SceInt64> &outListIds,
				paf::vector<paf::string> &outTitles);
	SceInt32 QueryAlbumItems(SceInt64 listId, paf::vector<SceInt64> &outItemIds);
	SceInt32 QueryVideoDetail(SceInt64 mrid, VideoRecordInfo &outInfo);
	SceInt32 QueryItemCount(const paf::string &contentType, SceInt32 &outCount);
	SceInt32 RequestRecovery(SceInt32 kind);

	static const char *SortOrderClause(VideoSortOrder sortOrder);

private:
	dbutil::Connection *m_connection;
	paf::thread::RMutex m_lock;
	paf::string m_dbPath;
	bool m_opened;
};

class VideoThumbnailJob : public paf::common::SlotJobItem {
public:
	VideoThumbnailJob(const paf::string &iconPath, SceInt32 iconOffset,
			  SceInt32 iconSize, paf::ui::Widget *targetButton);
	virtual ~VideoThumbnailJob();

	virtual SceInt32 Run();
	virtual void Finish(SceInt32 result);

private:
	paf::string m_iconPath;
	SceInt32 m_iconOffset;
	SceInt32 m_iconSize;
	paf::ui::Widget *m_targetButton;
	paf::vector<SceUInt8> m_thumbBytes;
	bool m_loaded;
};

#endif /* VIDEOIMPORT_VIDEO_DB_H */
