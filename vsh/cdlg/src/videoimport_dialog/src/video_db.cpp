/*
 * SceVideoImportDialogPlugin: AVContent.db manager, SQL query helpers,
 * and asynchronous video thumbnail loader.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include "video_db.h"

using namespace paf;

static const char *const kVideoSortClauses[] = {
	" ORDER BY title_for_sort ASC",
	" ORDER BY title_for_sort DESC",
	" ORDER BY created_time_for_sort ASC",
	" ORDER BY created_time_for_sort DESC",
	" ORDER BY size ASC",
	" ORDER BY size DESC"
};

static SceInt64 VideoValueInt64(const dbutil::Value &value)
{
	if (value.type == SQLITE_INTEGER)
		return value.data.integer;
	return 0;
}

static paf::string VideoValueText(const dbutil::Value &value)
{
	if (value.type == SQLITE_TEXT && value.data.text != NULL)
		return paf::string(value.data.text);
	return paf::string();
}

VideoDBManager::VideoDBManager()
	: m_connection(NULL),
	  m_lock("SceDBManagerLock", 0),
	  m_dbPath(),
	  m_opened(false)
{
}

VideoDBManager::~VideoDBManager()
{
	Close();
}

const char *VideoDBManager::SortOrderClause(VideoSortOrder sortOrder)
{
	SceUInt32 count = (SceUInt32)(sizeof(kVideoSortClauses) / sizeof(kVideoSortClauses[0]));
	if ((SceUInt32)sortOrder >= count)
		return kVideoSortClauses[VIDEO_SORT_TITLE_ASC];
	return kVideoSortClauses[sortOrder];
}

SceInt32 VideoDBManager::Open(const paf::string &mountDbPath, const paf::string &fallbackDbPath,
			      SceUInt32 busyTimeoutMs)
{
	(void)busyTimeoutMs;
	m_lock.Lock();
	if (m_opened) {
		m_lock.Unlock();
		return SCE_OK;
	}

	m_connection = new dbutil::Connection();
	if (m_connection == NULL) {
		m_lock.Unlock();
		return -1;
	}

	paf::string targetPath = mountDbPath.empty() ? fallbackDbPath : mountDbPath;
	SceInt32 ret = m_connection->Open(targetPath, 1);
	if (ret < 0 && !fallbackDbPath.empty() && targetPath != fallbackDbPath) {
		targetPath = fallbackDbPath;
		ret = m_connection->Open(targetPath, 1);
	}
	if (ret < 0) {
		delete m_connection;
		m_connection = NULL;
		m_lock.Unlock();
		return ret;
	}

	m_dbPath = targetPath;
	m_opened = true;
	m_lock.Unlock();
	return SCE_OK;
}

void VideoDBManager::Close()
{
	m_lock.Lock();
	if (m_connection != NULL) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
	}
	m_opened = false;
	m_lock.Unlock();
}

bool VideoDBManager::IsOpen() const
{
	return m_opened && m_connection != NULL;
}

SceInt32 VideoDBManager::RequestRecovery(SceInt32 kind)
{
	if (m_dbPath.empty())
		return -1;
	return SceDbrecoveryUtility_4AB1DAA9(kind, m_dbPath.c_str(), 1);
}

SceInt32 VideoDBManager::QueryVideoList(const paf::string &whereClause, VideoSortOrder sortOrder,
					SceInt32 limit, SceInt32 offset,
					paf::vector<VideoRecordInfo> &outItems)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	paf::string sql =
		"SELECT mrid,title,size,duration,last_updated_time,icon_offset,icon_size,"
		"icon_path,icon_protect,parental_level,status FROM tbl_vpcontent";
	if (!whereClause.empty()) {
		sql += " WHERE ";
		sql += whereClause.c_str();
	}
	sql += SortOrderClause(sortOrder);

	if (limit > 0) {
		char limitBuf[48];
		sce_paf_snprintf(limitBuf, sizeof(limitBuf), " LIMIT %d OFFSET %d",
				 (int)limit, (int)(offset > 0 ? offset : 0));
		sql += limitBuf;
	}

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(sql, &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	while (stmt->Step(1) == SQLITE_ROW) {
		VideoRecordInfo rec;
		rec.mrid = 0;
		rec.width = 0;
		rec.height = 0;
		rec.fileSize = 0;
		rec.duration = 0;
		rec.lastUpdatedTime = 0;
		rec.parentalLevel = 0;
		rec.status = 0;
		rec.iconOffset = 0;
		rec.iconSize = 0;
		rec.iconProtect = 0;

		dbutil::Value val;
		if (stmt->GetValue(0, &val) == 0)
			rec.mrid = VideoValueInt64(val);
		if (stmt->GetValue(1, &val) == 0)
			rec.title = VideoValueText(val);
		if (stmt->GetValue(2, &val) == 0)
			rec.fileSize = (SceUInt64)VideoValueInt64(val);
		if (stmt->GetValue(3, &val) == 0)
			rec.duration = (SceUInt32)VideoValueInt64(val);
		if (stmt->GetValue(4, &val) == 0)
			rec.lastUpdatedTime = (SceUInt64)VideoValueInt64(val);
		if (stmt->GetValue(5, &val) == 0)
			rec.iconOffset = (SceInt32)VideoValueInt64(val);
		if (stmt->GetValue(6, &val) == 0)
			rec.iconSize = (SceInt32)VideoValueInt64(val);
		if (stmt->GetValue(7, &val) == 0)
			rec.iconPath = VideoValueText(val);
		if (stmt->GetValue(8, &val) == 0)
			rec.iconProtect = (SceInt32)VideoValueInt64(val);
		if (stmt->GetValue(9, &val) == 0)
			rec.parentalLevel = (SceInt32)VideoValueInt64(val);
		if (stmt->GetValue(10, &val) == 0)
			rec.status = (SceInt32)VideoValueInt64(val);

		outItems.push_back(rec);
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 VideoDBManager::QueryAlbumList(const paf::string &listType,
					paf::vector<SceInt64> &outListIds,
					paf::vector<paf::string> &outTitles)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	paf::string sql = "SELECT mrid,title FROM tbl_avcontentlist WHERE type='";
	sql += listType.c_str();
	sql += "' ORDER BY title_for_sort ASC";

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(sql, &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	while (stmt->Step(1) == SQLITE_ROW) {
		dbutil::Value idVal;
		dbutil::Value titleVal;
		SceInt64 listId = 0;
		paf::string title;
		if (stmt->GetValue(0, &idVal) == 0)
			listId = VideoValueInt64(idVal);
		if (stmt->GetValue(1, &titleVal) == 0)
			title = VideoValueText(titleVal);
		outListIds.push_back(listId);
		outTitles.push_back(title);
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 VideoDBManager::QueryAlbumItems(SceInt64 listId, paf::vector<SceInt64> &outItemIds)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	char sqlBuf[224];
	sce_paf_snprintf(
		sqlBuf, sizeof(sqlBuf),
		"SELECT item_id FROM tbl_avcontentitem INNER JOIN tbl_vpcontent ON "
		"tbl_avcontentitem.item_id = tbl_vpcontent.mrid WHERE tbl_avcontentitem.list_id=%lld "
		"ORDER BY tbl_vpcontent.title_for_sort ASC",
		(long long)listId);

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(paf::string(sqlBuf), &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	while (stmt->Step(1) == SQLITE_ROW) {
		dbutil::Value val;
		if (stmt->GetValue(0, &val) == 0)
			outItemIds.push_back(VideoValueInt64(val));
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 VideoDBManager::QueryVideoDetail(SceInt64 mrid, VideoRecordInfo &outInfo)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	char sqlBuf[224];
	sce_paf_snprintf(
		sqlBuf, sizeof(sqlBuf),
		"SELECT content_path, title, width, height, size, duration, appli_title_id, "
		"last_updated_time, description, copyright FROM tbl_VPContent WHERE mrid=%lld",
		(long long)mrid);

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(paf::string(sqlBuf), &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	if (stmt->Step(1) != SQLITE_ROW) {
		m_lock.Unlock();
		return -1;
	}

	outInfo.mrid = mrid;
	dbutil::Value val;
	if (stmt->GetValue(0, &val) == 0)
		outInfo.contentPath = VideoValueText(val);
	if (stmt->GetValue(1, &val) == 0)
		outInfo.title = VideoValueText(val);
	if (stmt->GetValue(2, &val) == 0)
		outInfo.width = (SceUInt32)VideoValueInt64(val);
	if (stmt->GetValue(3, &val) == 0)
		outInfo.height = (SceUInt32)VideoValueInt64(val);
	if (stmt->GetValue(4, &val) == 0)
		outInfo.fileSize = (SceUInt64)VideoValueInt64(val);
	if (stmt->GetValue(5, &val) == 0)
		outInfo.duration = (SceUInt32)VideoValueInt64(val);
	if (stmt->GetValue(6, &val) == 0)
		outInfo.appliTitleId = VideoValueText(val);
	if (stmt->GetValue(7, &val) == 0)
		outInfo.lastUpdatedTime = (SceUInt64)VideoValueInt64(val);
	if (stmt->GetValue(8, &val) == 0)
		outInfo.description = VideoValueText(val);
	if (stmt->GetValue(9, &val) == 0)
		outInfo.copyright = VideoValueText(val);

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 VideoDBManager::QueryItemCount(const paf::string &contentType, SceInt32 &outCount)
{
	outCount = 0;
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	paf::string sql = "SELECT count(mrid) FROM tbl_vpcontent WHERE content_type='";
	sql += contentType.c_str();
	sql += "' AND drm_string IS NULL";

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(sql, &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	if (stmt->Step(1) == SQLITE_ROW) {
		dbutil::Value val;
		if (stmt->GetValue(0, &val) == 0)
			outCount = (SceInt32)VideoValueInt64(val);
	}

	m_lock.Unlock();
	return SCE_OK;
}

VideoThumbnailJob::VideoThumbnailJob(const paf::string &iconPath, SceInt32 iconOffset,
				     SceInt32 iconSize, ui::Widget *targetButton)
	: common::SlotJobItem("CreateThumbnailJob", NULL),
	  m_iconPath(iconPath),
	  m_iconOffset(iconOffset),
	  m_iconSize(iconSize),
	  m_targetButton(targetButton),
	  m_thumbBytes(),
	  m_loaded(false)
{
}

VideoThumbnailJob::~VideoThumbnailJob()
{
}

SceInt32 VideoThumbnailJob::Run()
{
	if (IsCanceled() || m_iconPath.empty() || m_iconSize <= 0)
		return -1;

	SceInt32 err = SCE_OK;
	common::SharedPtr<File> file = LocalFile::Open(m_iconPath.c_str(), 1, 0, &err);
	if (err < 0 || file.get() == NULL)
		return err < 0 ? err : -1;

	if (m_iconOffset > 0)
		file->Seek(m_iconOffset, 0);

	m_thumbBytes.resize((size_t)m_iconSize);
	SceInt32 bytesRead = file->Read(&m_thumbBytes[0], (size_t)m_iconSize);
	file->Close();

	if (bytesRead <= 0) {
		m_thumbBytes.clear();
		return -1;
	}

	m_thumbBytes.resize((size_t)bytesRead);
	m_loaded = true;
	return SCE_OK;
}

void VideoThumbnailJob::Finish(SceInt32 result)
{
	if (IsCanceled() || result != SCE_OK || !m_loaded || m_targetButton == NULL)
		return;
	if (m_thumbBytes.empty())
		return;

	graph::Surface::LoadOption opt;
	intrusive_ptr<graph::Surface> surface =
		graph::Surface::Load(NULL, &m_thumbBytes[0], (SceInt64)m_thumbBytes.size(), &opt);
	if (surface.get() != NULL)
		m_targetButton->SetTexture(surface);
}
