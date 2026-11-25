/*
 * ScePhotoImportDialogPlugin: AVContent.db manager, SQL query builders,
 * and ScePhotoThumbJob thumbnail loader.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <graphics/surface/surface.h>
#include <common/main_thread_call_list.h>
#include "photo_db.h"

using namespace paf;

static const char *const kSortOrderClauses[] = {
	" ORDER BY created_time ASC",
	" ORDER BY created_time DESC",
	" ORDER BY imported_time ASC",
	" ORDER BY imported_time DESC",
	" ORDER BY appli_title_for_sort ASC",
	" ORDER BY appli_title_for_sort DESC",
	" ORDER BY title ASC",
	" ORDER BY title DESC",
	" ORDER BY title_for_sort ASC",
	" ORDER BY title_for_sort DESC",
	" ORDER BY created_time,imported_time ASC",
	" ORDER BY created_time,imported_time DESC",
	" ORDER BY created_local_time_month ASC",
	" ORDER BY created_local_time_month DESC"
};

static SceInt64 ValueAsInt64(const dbutil::Value &value)
{
	if (value.type == SQLITE_INTEGER)
		return value.data.integer;
	return 0;
}

static paf::string ValueAsText(const dbutil::Value &value)
{
	if (value.type == SQLITE_TEXT && value.data.text != NULL)
		return paf::string(value.data.text);
	return paf::string();
}

PhotoDBManager::PhotoDBManager()
	: m_connection(NULL),
	  m_lock("SceDBManagerLock", 0),
	  m_dbPath(),
	  m_opened(false)
{
}

PhotoDBManager::~PhotoDBManager()
{
	Close();
}

const char *PhotoDBManager::SortOrderClause(PhotoSortOrder sortOrder)
{
	SceUInt32 count = (SceUInt32)(sizeof(kSortOrderClauses) / sizeof(kSortOrderClauses[0]));
	if ((SceUInt32)sortOrder >= count)
		return kSortOrderClauses[PHOTO_SORT_CREATED_TIME_DESC];
	return kSortOrderClauses[sortOrder];
}

SceInt32 PhotoDBManager::Open(const paf::string &mountDbPath, const paf::string &fallbackDbPath,
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

void PhotoDBManager::Close()
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

bool PhotoDBManager::IsOpen() const
{
	return m_opened && m_connection != NULL;
}

SceInt32 PhotoDBManager::RequestRecovery(SceInt32 kind)
{
	if (m_dbPath.empty())
		return -1;
	return SceDbrecoveryUtility_4AB1DAA9(kind, m_dbPath.c_str(), 1);
}

SceInt32 PhotoDBManager::QueryPhotoList(const paf::string &whereClause, PhotoSortOrder sortOrder,
					SceInt32 limit, SceInt32 offset,
					paf::vector<PhotoRecordInfo> &outItems)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	paf::string sql =
		"SELECT mrid,content_type,genre_type,duration,title,created_time,"
		"created_local_time_month,appli_title FROM tbl_vpcontent";
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
		PhotoRecordInfo rec;
		rec.mrid = 0;
		rec.contentType = 0;
		rec.genreType = 0;
		rec.duration = 0;
		rec.width = 0;
		rec.height = 0;
		rec.orientationType = 0;
		rec.codecType = 0;

		dbutil::Value val;
		if (stmt->GetValue(0, &val) == 0)
			rec.mrid = ValueAsInt64(val);
		if (stmt->GetValue(1, &val) == 0)
			rec.contentType = (SceInt32)ValueAsInt64(val);
		if (stmt->GetValue(2, &val) == 0)
			rec.genreType = (SceInt32)ValueAsInt64(val);
		if (stmt->GetValue(3, &val) == 0)
			rec.duration = (SceInt32)ValueAsInt64(val);
		if (stmt->GetValue(4, &val) == 0)
			rec.title = ValueAsText(val);
		if (stmt->GetValue(5, &val) == 0)
			rec.createdTime = ValueAsText(val);
		if (stmt->GetValue(6, &val) == 0)
			rec.createdLocalMonth = ValueAsText(val);
		if (stmt->GetValue(7, &val) == 0)
			rec.appliTitle = ValueAsText(val);

		outItems.push_back(rec);
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 PhotoDBManager::QueryGroupList(const paf::string &groupColumn,
					const paf::string &whereClause,
					PhotoSortOrder sortOrder,
					paf::vector<paf::string> &outGroups,
					paf::vector<SceInt32> &outCounts)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	paf::string sql = "SELECT ";
	sql += groupColumn.c_str();
	sql += ",count(*) FROM tbl_vpcontent";
	if (!whereClause.empty()) {
		sql += " WHERE ";
		sql += whereClause.c_str();
	}
	sql += " GROUP BY ";
	sql += groupColumn.c_str();
	sql += SortOrderClause(sortOrder);

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(sql, &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	while (stmt->Step(1) == SQLITE_ROW) {
		dbutil::Value groupVal;
		dbutil::Value countVal;
		paf::string label;
		SceInt32 count = 0;

		if (stmt->GetValue(0, &groupVal) == 0)
			label = ValueAsText(groupVal);
		if (stmt->GetValue(1, &countVal) == 0)
			count = (SceInt32)ValueAsInt64(countVal);

		outGroups.push_back(label);
		outCounts.push_back(count);
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 PhotoDBManager::QueryPlaylistItems(SceInt64 listId, SceInt32 itemType,
					    paf::vector<SceInt64> &outItemIds)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	char sqlBuf[160];
	sce_paf_snprintf(sqlBuf, sizeof(sqlBuf),
			 "SELECT item_id FROM tbl_avcontentitem WHERE list_id=%lld AND item_type=%d ORDER BY created_time DESC",
			 (long long)listId, (int)itemType);

	dbutil::RefPtr<dbutil::Statement> stmt;
	SceInt32 ret = m_connection->Prepare(paf::string(sqlBuf), &stmt);
	if (ret < 0 || stmt.operator->() == NULL) {
		m_lock.Unlock();
		return ret < 0 ? ret : -1;
	}

	while (stmt->Step(1) == SQLITE_ROW) {
		dbutil::Value val;
		if (stmt->GetValue(0, &val) == 0)
			outItemIds.push_back(ValueAsInt64(val));
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 PhotoDBManager::QueryPhotoDetail(SceInt64 mrid, PhotoRecordInfo &outInfo)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	char sqlBuf[160];
	sce_paf_snprintf(sqlBuf, sizeof(sqlBuf),
			 "SELECT content_path, title, width, height, orientation_type FROM tbl_VPContent WHERE mrid=%lld",
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
		outInfo.contentPath = ValueAsText(val);
	if (stmt->GetValue(1, &val) == 0)
		outInfo.title = ValueAsText(val);
	if (stmt->GetValue(2, &val) == 0)
		outInfo.width = (SceInt32)ValueAsInt64(val);
	if (stmt->GetValue(3, &val) == 0)
		outInfo.height = (SceInt32)ValueAsInt64(val);
	if (stmt->GetValue(4, &val) == 0)
		outInfo.orientationType = (SceInt32)ValueAsInt64(val);

	sce_paf_snprintf(sqlBuf, sizeof(sqlBuf),
			 "SELECT codec_type FROM tbl_Photo WHERE base_id=%lld",
			 (long long)mrid);
	dbutil::RefPtr<dbutil::Statement> photoStmt;
	if (m_connection->Prepare(paf::string(sqlBuf), &photoStmt) == 0 && photoStmt.operator->() != NULL) {
		if (photoStmt->Step(1) == SQLITE_ROW && photoStmt->GetValue(0, &val) == 0)
			outInfo.codecType = (SceInt32)ValueAsInt64(val);
	}

	m_lock.Unlock();
	return SCE_OK;
}

SceInt32 PhotoDBManager::QueryThumbnail(SceInt64 contentId, SceInt32 codecType,
					paf::vector<SceUInt8> &outBlob, float &outAspectRatio)
{
	m_lock.Lock();
	if (!IsOpen()) {
		m_lock.Unlock();
		return -1;
	}

	char sqlBuf[160];
	sce_paf_snprintf(sqlBuf, sizeof(sqlBuf),
			 "SELECT data,size,aspect_ratio FROM tbl_icon WHERE content_id=%lld AND codec_type=%d",
			 (long long)contentId, (int)codecType);

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

	dbutil::Value blobVal;
	dbutil::Value aspectVal;
	outAspectRatio = 1.0f;
	if (stmt->GetValue(2, &aspectVal) == 0 && aspectVal.type == SQLITE_FLOAT)
		outAspectRatio = (float)aspectVal.data.real;

	if (stmt->GetValue(0, &blobVal) == 0 &&
	    blobVal.type == SQLITE_BLOB &&
	    blobVal.data.blob != NULL && blobVal.size > 0) {
		const SceUInt8 *bytes = (const SceUInt8 *)blobVal.data.blob;
		outBlob.resize((size_t)blobVal.size);
		sce_paf_memcpy(&outBlob[0], bytes, (size_t)blobVal.size);
		m_lock.Unlock();
		return SCE_OK;
	}

	m_lock.Unlock();
	return -1;
}

ScePhotoThumbJob::ScePhotoThumbJob(PhotoDBManager *dbManager, SceInt64 contentId,
				   SceInt32 codecType, ui::Widget *targetButton)
	: common::SlotJobItem("ScePhotoThumbJob", NULL),
	  m_dbManager(dbManager),
	  m_contentId(contentId),
	  m_codecType(codecType),
	  m_targetButton(targetButton),
	  m_thumbBytes(),
	  m_aspectRatio(1.0f),
	  m_loaded(false)
{
}

ScePhotoThumbJob::~ScePhotoThumbJob()
{
}

SceInt32 ScePhotoThumbJob::Run()
{
	if (IsCanceled() || m_dbManager == NULL)
		return -1;
	SceInt32 ret = m_dbManager->QueryThumbnail(m_contentId, m_codecType,
						   m_thumbBytes, m_aspectRatio);
	if (ret == SCE_OK && !m_thumbBytes.empty())
		m_loaded = true;
	return ret;
}

void ScePhotoThumbJob::Finish(SceInt32 result)
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
