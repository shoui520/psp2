/*
 * tel_sms_dbmanager.cpp: TelSmsDbMigrator and SQLite-backed TelSmsDbManager
 * (3.65-CEX: 0x813E75C0..0x813ED8B7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <buffer/buffer.h>
#include <common/string_util.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <memory/heap_allocator.h>
#include <dbutil/connection.h>
#include <dbutil/record.h>
#include <dbutil/rowid_result.h>
#include <dbutil/statement.h>
#include "../../tel_sms_dbmanager.h"

namespace telephony {

namespace {

static const char kTemplateDbPath[] = "vs0:vsh/shell/telephony/telephony_sms.db";
static const char kImportDbAlias[] = "importdb";
static const int kDbOpenFlags = 0x10000002;
static const int kDbBusyTimeoutMs = 10000;
static const SceSize kCopyChunkBytes = 0x8000;
static const int kDirExistsError = -2147418095; /* 0x80010011 */

static paf::string escapeSqlSingleQuotes(const paf::string &input)
{
	paf::string escaped("");
	for (SceSize idx = 0; idx < input.length(); ++idx) {
		char ch = input.c_str()[idx];
		if (ch == '\'')
			escaped.append("'");
		escaped.append(&ch, 1);
	}
	return escaped;
}

static bool findRowIdIndex(const paf::vector<SceInt64> &idList, SceInt64 target,
			   SceSize *outIndex)
{
	for (SceSize idx = 0; idx < idList.size(); ++idx) {
		if (idList[idx] == target) {
			*outIndex = idx;
			return true;
		}
	}
	return false;
}

static paf::string joinInt64Csv(const paf::vector<SceInt64> &values)
{
	paf::string joined("");
	for (SceSize idx = 0; idx < values.size(); ++idx) {
		if (idx != 0)
			joined.append(", ");
		paf::string part = telFormatString("%llu", (SceUInt64)values[idx]);
		joined.append(part);
	}
	return joined;
}

static paf::string joinQuotedPhonesCsv(const paf::vector<TelSmsAddress> &addresses)
{
	paf::string joined("");
	for (SceSize idx = 0; idx < addresses.size(); ++idx) {
		if (idx != 0)
			joined.append(", ");
		paf::string part = telFormatString("'%s'", addresses[idx].GetNumber().GetDigits());
		joined.append(part);
	}
	return joined;
}

static paf::string joinMessageIdsCsv(const paf::vector<TelSmsMessageId> &ids)
{
	paf::string joined("");
	for (SceSize idx = 0; idx < ids.size(); ++idx) {
		if (idx != 0)
			joined.append(", ");
		paf::string part = telFormatString("%llu", ids[idx].GetValue());
		joined.append(part);
	}
	return joined;
}

static void populateMessageFromStatement(dbutil::Statement *stmt,
					 paf::vector<TelSmsPeerInfo> *peerList,
					 TelSmsMessage *outMessage)
{
	dbutil::Value valMsgId;
	dbutil::Value valKind;
	dbutil::Value valType;
	dbutil::Value valHistoryTime;
	dbutil::Value valPhone;
	dbutil::Value valStatus;
	dbutil::Value valBody;
	dbutil::Value valDeliverTime;

	stmt->GetValue(0, &valMsgId);
	stmt->GetValue(1, &valKind);
	stmt->GetValue(2, &valType);
	stmt->GetValue(3, &valHistoryTime);
	stmt->GetValue(4, &valPhone);
	stmt->GetValue(5, &valStatus);
	stmt->GetValue(6, &valBody);
	stmt->GetValue(7, &valDeliverTime);

	TelSmsMessageId msgId((SceUInt64)valMsgId.data.integer);

	TelSmsAddress peerAddr;
	TelPhoneNumber phone(valPhone.data.text != NULL ? valPhone.data.text : "");
	peerAddr.CopyFrom(phone, "");
	TelSmsPeerInfo peer(peerAddr, (int)valStatus.data.integer, false, 0, 4);
	peerList->push_back(peer);

	paf::string utf8Body;
	if (valBody.data.text != NULL && valBody.size > 0)
		utf8Body.assign(valBody.data.text, (SceSize)valBody.size);
	paf::common::String bodyConv(utf8Body);
	paf::wstring wideBody = bodyConv.GetWString();

	TelDateTime historyDt;
	if (valHistoryTime.data.text != NULL)
		historyDt.ParseIsoString(paf::string(valHistoryTime.data.text));

	TelDateTime deliverDt;
	if (valDeliverTime.data.text != NULL)
		deliverDt.ParseIsoString(paf::string(valDeliverTime.data.text));

	outMessage->SetMessageId(msgId);
	outMessage->SetBody(wideBody);
	outMessage->SetHistoryTime(historyDt);
	outMessage->SetDeliverTime(deliverDt);
}

} /* namespace */

/* --------------------------------------------------------------------------
 * TelSmsDbMigrator (0x813E75C0..0x813E8983)
 * -------------------------------------------------------------------------- */

/* 0x813E75C0 */
TelSmsDbMigrator::TelSmsDbMigrator(const paf::string &oldDbPath, const paf::string &newDbPath)
	: m_connection(NULL), m_oldDbPath(oldDbPath), m_newDbPath(newDbPath),
	  m_tempDbPath(newDbPath)
{
	m_tempDbPath.append(".tmp");
}

TelSmsDbMigrator::~TelSmsDbMigrator()
{
	DetachAndClose();
}

/* 0x813E76A4 */
TelephonyResult TelSmsDbMigrator::RemoveFileIfExists(const paf::string &path)
{
	paf::LocalFileStat fileStat;
	int err = paf::LocalFile::Getstat(path.c_str(), &fileStat);
	if (err != 0)
		return TelephonyResult(err);

	err = paf::LocalFile::RemoveFile(path.c_str());
	return TelephonyResult(err);
}

/* 0x813E7770 */
TelephonyResult TelSmsDbMigrator::DetachAndClose()
{
	TelephonyResult result(TEL_OK);
	if (m_connection != NULL) {
		paf::string alias(kImportDbAlias);
		int detachErr = m_connection->Detach(alias);
		if (detachErr != 0)
			result = TelephonyResult(detachErr);

		int closeErr = m_connection->Close();
		if (closeErr != 0)
			result = TelephonyResult(closeErr);

		delete m_connection;
		m_connection = NULL;
	}
	return result;
}

/* 0x813E781A */
TelephonyResult TelSmsDbMigrator::MigrateHistoryTable(const paf::vector<SceInt64> &oldMsgIds,
						      const paf::vector<SceInt64> &oldPeerIds)
{
	paf::string selectSql = telFormatString(
		"SELECT HistoryID, MessageID, PeerID, Status, CreatedTime, LastUpdateTime "
		"FROM %s.SmsHistoryTbl;",
		kImportDbAlias);

	dbutil::RowidResult rows;
	int err = m_connection->Execute(selectSql, 0, NULL, &rows, NULL, NULL);
	if (err != 0)
		return TelephonyResult(err);

	paf::string columns("HistoryID, MessageID, PeerID, Status, CreatedTime, LastUpdateTime ");
	int rowCount = (int)rows.m_rowids.size();
	for (int rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
		dbutil::RefPtr<dbutil::Record> record;
		rows.OpenRecord(rowIdx, columns, &record);
		if (record.Get() == NULL)
			continue;

		dbutil::Value valHistoryId;
		dbutil::Value valMessageId;
		dbutil::Value valPeerId;
		dbutil::Value valStatus;
		dbutil::Value valCreated;
		dbutil::Value valUpdated;
		record->GetValue(0, &valHistoryId);
		record->GetValue(1, &valMessageId);
		record->GetValue(2, &valPeerId);
		record->GetValue(3, &valStatus);
		record->GetValue(4, &valCreated);
		record->GetValue(5, &valUpdated);

		SceSize mappedMsgIdx = 0;
		SceSize mappedPeerIdx = 0;
		if (!findRowIdIndex(oldMsgIds, valMessageId.data.integer, &mappedMsgIdx))
			continue;
		if (!findRowIdIndex(oldPeerIds, valPeerId.data.integer, &mappedPeerIdx))
			continue;

		paf::string insertSql = telFormatString(
			"INSERT INTO SmsHistoryTbl(MessageID, PeerID, Status, CreatedTime, "
			"LastUpdateTime )VALUES(%llu, %llu, %llu, '%s', '%s');",
			(SceUInt64)(mappedMsgIdx + 1), (SceUInt64)(mappedPeerIdx + 1),
			(SceUInt64)valStatus.data.integer,
			valCreated.data.text != NULL ? valCreated.data.text : "",
			valUpdated.data.text != NULL ? valUpdated.data.text : "");

		if (m_connection->Execute(insertSql, 0, NULL, NULL, NULL, NULL) != 0)
			return TelephonyResult(TEL_ERR_DB_FAILED);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E7BAA */
TelephonyResult TelSmsDbMigrator::MigrateMessageTable(paf::vector<SceInt64> *outOldMsgIds)
{
	dbutil::RowidResult rows;
	paf::string selectSql = telFormatString(
		"SELECT MessageID, MessageKind, MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"CreatedTime, LastUpdateTime, Message, DeliverTime "
		"FROM %s.SmsMessageTbl;",
		kImportDbAlias);

	int err = m_connection->Execute(selectSql, 0, NULL, &rows, NULL, NULL);
	if (err != 0)
		return TelephonyResult(err);

	paf::string columns(
		"MessageID, MessageKind, MessageType, "
		"strftime('%Y-%m-%d %H:%M:%f', HistoryTime, 'localtime'), "
		"CreatedTime, LastUpdateTime, Message, DeliverTime ");

	int rowCount = (int)rows.m_rowids.size();
	for (int rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
		dbutil::RefPtr<dbutil::Record> record;
		rows.OpenRecord(rowIdx, columns, &record);
		if (record.Get() == NULL)
			continue;

		dbutil::Value valMessageId;
		dbutil::Value bindValues[7];
		record->GetValue(0, &valMessageId);
		for (int col = 0; col < 7; ++col)
			record->GetValue(col + 1, &bindValues[col]);

		paf::string insertSql(
			"INSERT INTO SmsMessageTbl(MessageKind, MessageType, HistoryTime, "
			"CreatedTime, LastUpdateTime, Message, DeliverTime )"
			"VALUES(?, ?, julianday(?), ?, ?, ?, ?);");

		dbutil::RefPtr<dbutil::Statement> stmt;
		if (m_connection->Prepare(insertSql, &stmt) != 0)
			return TelephonyResult(TEL_ERR_DB_FAILED);

		bool bindOk = true;
		for (int col = 0; col < 7; ++col) {
			if (stmt->Bind(col + 1, &bindValues[col], false) != 0) {
				bindOk = false;
				break;
			}
		}
		if (!bindOk) {
			stmt->Finalize();
			return TelephonyResult(TEL_ERR_DB_FAILED);
		}

		unsigned int stepRes = (unsigned int)stmt->Step(1);
		stmt->Finalize();
		if (stepRes >= 2U)
			return TelephonyResult(TEL_ERR_DB_FAILED);

		outOldMsgIds->push_back(valMessageId.data.integer);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E7EAA */
TelephonyResult TelSmsDbMigrator::MigratePeerTable(paf::vector<SceInt64> *outOldPeerIds)
{
	paf::string selectSql = telFormatString(
		"SELECT PeerID, PhoneNumber, CreatedTime, LastUpdateTime FROM %s.PeerTbl;",
		kImportDbAlias);

	dbutil::RowidResult rows;
	int err = m_connection->Execute(selectSql, 0, NULL, &rows, NULL, NULL);
	if (err != 0)
		return TelephonyResult(err);

	paf::string columns("PeerID, PhoneNumber, CreatedTime, LastUpdateTime ");
	int rowCount = (int)rows.m_rowids.size();
	for (int rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
		dbutil::RefPtr<dbutil::Record> record;
		rows.OpenRecord(rowIdx, columns, &record);
		if (record.Get() == NULL)
			continue;

		dbutil::Value valPeerId;
		dbutil::Value valPhone;
		dbutil::Value valCreated;
		dbutil::Value valUpdated;
		record->GetValue(0, &valPeerId);
		record->GetValue(1, &valPhone);
		record->GetValue(2, &valCreated);
		record->GetValue(3, &valUpdated);

		paf::string insertSql = telFormatString(
			"INSERT INTO PeerTbl(PhoneNumber, CreatedTime, LastUpdateTime )"
			"VALUES('%s', '%s', '%s');",
			valPhone.data.text != NULL ? valPhone.data.text : "",
			valCreated.data.text != NULL ? valCreated.data.text : "",
			valUpdated.data.text != NULL ? valUpdated.data.text : "");

		if (m_connection->Execute(insertSql, 0, NULL, NULL, NULL, NULL) != 0)
			return TelephonyResult(TEL_ERR_DB_FAILED);

		outOldPeerIds->push_back(valPeerId.data.integer);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E80CC */
TelephonyResult TelSmsDbMigrator::OpenAndAttach(const paf::string &targetPath,
						const paf::string &importPath)
{
	if (m_connection != NULL)
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	m_connection = new (telGetAllocator()) dbutil::Connection();
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	int err = m_connection->Open(targetPath, kDbOpenFlags);
	if (err != 0) {
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}

	err = m_connection->SetBusyTimeout(kDbBusyTimeoutMs);
	if (err != 0) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}

	paf::string pragmaSql("PRAGMA journal_mode=OFF;");
	err = m_connection->Execute(pragmaSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}

	paf::string alias(kImportDbAlias);
	err = m_connection->Attach(importPath, alias);
	if (err != 0) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E8292 */
TelephonyResult TelSmsDbMigrator::ImportDatabase(const paf::string &oldPath,
						 const paf::string &tempPath)
{
	TelephonyResult res = OpenAndAttach(tempPath, oldPath);
	if (!res.IsSuccess())
		return res;

	int beginErr = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (beginErr != 0) {
		res = TelephonyResult(beginErr);
	} else {
		paf::vector<SceInt64> oldPeerIds;
		res = MigratePeerTable(&oldPeerIds);
		if (res.IsSuccess()) {
			paf::vector<SceInt64> oldMsgIds;
			res = MigrateMessageTable(&oldMsgIds);
			if (res.IsSuccess()) {
				res = MigrateHistoryTable(oldMsgIds, oldPeerIds);
				if (res.IsSuccess()) {
					int commitErr = m_connection->Commit();
					if (commitErr != 0)
						res = TelephonyResult(commitErr);
				}
			}
		}
	}

	if (!res.IsSuccess()) {
		DetachAndClose();
		return res;
	}
	return DetachAndClose();
}

/* 0x813E84A0 */
TelephonyResult TelSmsDbMigrator::CopyFile(const paf::string &srcPath,
					   const paf::string &dstPath)
{
	paf::LocalFile srcFile;
	paf::LocalFile dstFile;
	paf::LocalFileStat srcStat;

	int err = paf::LocalFile::Getstat(srcPath.c_str(), &srcStat);
	if (err != 0)
		return TelephonyResult(err);

	paf::common::SharedPtr<paf::MallocBuffer> copyBuf =
		paf::MallocBuffer::Allocate(kCopyChunkBytes);
	if (copyBuf.get() == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	paf::LocalFile::OpenArg openArg;
	openArg.filename = srcPath.c_str();
	openArg.flag = 1;
	openArg.mode = srcStat.st_mode;

	err = srcFile.Open(&openArg);
	if (err == 0) {
		openArg.filename = dstPath.c_str();
		openArg.flag = 1539;
		openArg.mode = 0x106;
		err = dstFile.Open(&openArg);
	}

	if (err == 0) {
		SceOff remaining = srcFile.GetFileSize();
		while (remaining > 0) {
			SceSize chunk = remaining > (SceOff)kCopyChunkBytes
						? kCopyChunkBytes
						: (SceSize)remaining;
			int bytesRead = srcFile.Read(copyBuf->GetAddr(), chunk);
			if (bytesRead == 0)
				break;
			if (bytesRead < 0) {
				err = bytesRead;
				break;
			}
			int bytesWritten = dstFile.Write(copyBuf->GetAddr(), (SceSize)bytesRead);
			if (bytesWritten < 0) {
				err = bytesWritten;
				break;
			}
			remaining -= bytesRead;
		}
	}

	srcFile.Close();
	dstFile.Close();
	return TelephonyResult(err);
}

/* 0x813E873A */
TelephonyResult TelSmsDbMigrator::MigrateIfNeeded()
{
	if (paf::LocalFile::Exists(m_newDbPath.c_str()))
		return TelephonyResult(TEL_OK);

	/* Ensure parent directories exist for the target DB path */
	SceSize pathLen = m_newDbPath.length();
	char *dirScratch = (char *)telMalloc(pathLen + 1);
	if (dirScratch != NULL) {
		const char *basePtr = m_newDbPath.c_str();
		const char *cursor = basePtr + 1;
		while (true) {
			const char *slash = sce_paf_strchr(cursor, '/');
			if (slash == NULL)
				break;
			SceSize prefixLen = (SceSize)(slash - basePtr);
			sce_paf_strncpy(dirScratch, basePtr, prefixLen);
			dirScratch[prefixLen] = '\0';
			int mkdirErr = paf::Dir::Create(dirScratch, 0x106);
			if (mkdirErr != 0 && mkdirErr != kDirExistsError)
				break;
			cursor = slash + 1;
		}
		telFree(dirScratch);
	}

	RemoveFileIfExists(m_tempDbPath);

	TelephonyResult res = CopyFile(paf::string(kTemplateDbPath), m_tempDbPath);
	if (res.IsSuccess()) {
		if (paf::LocalFile::Exists(m_oldDbPath.c_str()))
			res = ImportDatabase(m_oldDbPath, m_tempDbPath);
	}

	if (!res.IsSuccess()) {
		RemoveFileIfExists(m_tempDbPath);
		return res;
	}

	int renameErr = paf::LocalFile::RenameFile(m_tempDbPath.c_str(), m_newDbPath.c_str());
	return TelephonyResult(renameErr);
}

/* --------------------------------------------------------------------------
 * TelSmsDbManager (0x813E89E4..0x813ED8B7)
 * -------------------------------------------------------------------------- */

/* 0x813E89E4 */
TelSmsDbManager::TelSmsDbManager(const paf::string &dbPath)
	: TelObservable("SceTelDbMgrInterface"), m_connection(NULL), m_dbPath(dbPath)
{
}

/* 0x813E8A4E */
TelSmsDbManager::~TelSmsDbManager()
{
	Close();
}

/* 0x813E8AC0 */
TelephonyResult TelSmsDbManager::Open()
{
	if (m_connection != NULL)
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	m_connection = new (telGetAllocator()) dbutil::Connection();
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);

	int err = m_connection->Open(m_dbPath, kDbOpenFlags);
	if (err != 0) {
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}

	err = m_connection->SetBusyTimeout(kDbBusyTimeoutMs);
	if (err != 0) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
		return TelephonyResult(err);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E8B62 */
TelephonyResult TelSmsDbManager::Close()
{
	if (m_connection != NULL) {
		m_connection->Close();
		delete m_connection;
		m_connection = NULL;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E8B82 */
TelephonyResult TelSmsDbManager::GetDbPath(paf::string *outPath)
{
	*outPath = m_dbPath;
	return TelephonyResult(TEL_OK);
}

/* 0x813E8B98 */
TelephonyResult TelSmsDbManager::Interrupt()
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	m_connection->Interrupt();
	return TelephonyResult(TEL_OK);
}

/* 0x813E8BBC */
TelephonyResult TelSmsDbManager::UpdateFilterForPeers(const paf::vector<SceInt64> &peerIds,
						      const TelSmsMessageId &messageId,
						      const int *status, bool isNewMessage)
{
	if (peerIds.empty())
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	paf::string peerCsv = joinInt64Csv(peerIds);
	paf::string whereClause = telFormatString("WHERE PeerID IN (%s)", peerCsv.c_str());
	paf::string querySql = telFormatString(
		"SELECT FilterID, Count(FilterPeerID) AS peerCnt FROM FilterPeerTbl %s "
		"GROUP BY FilterID; ",
		whereClause.c_str());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(querySql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	paf::vector<SceInt64> matchingFilters;
	while (stmt->Step(1) == 0) {
		dbutil::Value valFilterId;
		dbutil::Value valPeerCnt;
		stmt->GetValue(0, &valFilterId);
		stmt->GetValue(1, &valPeerCnt);
		if ((SceSize)valPeerCnt.data.integer == peerIds.size())
			matchingFilters.push_back(valFilterId.data.integer);
	}
	stmt->Finalize();

	int filterType = (peerIds.size() == 1U) ? 2 : 1;
	SceUInt64 msgIdVal = messageId.GetValue();
	paf::string setClause("");
	int initialUnread = 0;

	switch (*status) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (isNewMessage) {
			setClause = telFormatString(
				"LatestMessageID = %llu, MessageCount = MessageCount + 1, ",
				msgIdVal);
		} else {
			setClause = telFormatString("LatestMessageID = %llu, ", msgIdVal);
		}
		initialUnread = 0;
		break;
	case 6:
		return TelephonyResult(TEL_OK);
	case 7:
		setClause = telFormatString(
			"LatestMessageID = %llu, MessageCount = MessageCount + 1, "
			"UnreadCount = UnreadCount + 1, ",
			msgIdVal);
		initialUnread = 1;
		break;
	case 8:
		setClause = telFormatString(
			"LatestMessageID = %llu, UnreadCount = UnreadCount - 1, ", msgIdVal);
		initialUnread = 0;
		break;
	default:
		return TelephonyResult(TEL_ERR_IO_FAILED);
	}

	if (!matchingFilters.empty()) {
		for (SceSize idx = 0; idx < matchingFilters.size(); ++idx) {
			paf::string updateSql = telFormatString(
				"UPDATE FilterTbl SET %s LastUpdateTime = datetime('now') "
				"WHERE FilterType = %d AND FilterID = %llu ;",
				setClause.c_str(), filterType, (SceUInt64)matchingFilters[idx]);
			int execErr = m_connection->Execute(updateSql, 0, NULL, NULL, NULL, NULL);
			if (execErr != 0) {
				m_connection->Rollback();
				return TelephonyResult(execErr);
			}
		}
	} else {
		paf::string insertFilterSql = telFormatString(
			"INSERT INTO FilterTbl(FilterType, LatestMessageID, MessageCount, "
			"UnreadCount, CreatedTime, LastUpdateTime) "
			"VALUES(%d, %llu, %d, %d, datetime('now'), datetime('now'));",
			filterType, msgIdVal, 1, initialUnread);
		int execErr = m_connection->Execute(insertFilterSql, 0, NULL, NULL, NULL, NULL);
		if (execErr != 0) {
			m_connection->Rollback();
			return TelephonyResult(execErr);
		}

		SceInt64 newFilterId = m_connection->GetLastInsertRowid();
		for (SceSize idx = 0; idx < peerIds.size(); ++idx) {
			paf::string insertPeerSql = telFormatString(
				"INSERT INTO FilterPeerTbl(FilterID, PeerID, CreatedTime, "
				"LastUpdateTime) VALUES(%llu, %llu, datetime('now'), "
				"datetime('now'));",
				(SceUInt64)newFilterId, (SceUInt64)peerIds[idx]);
			int peerErr = m_connection->Execute(insertPeerSql, 0, NULL, NULL, NULL, NULL);
			if (peerErr != 0) {
				m_connection->Rollback();
				return TelephonyResult(peerErr);
			}
		}
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813E93DE */
TelephonyResult TelSmsDbManager::ResolveOrInsertPeer(const TelPhoneNumber &phoneNumber,
						     SceInt64 *outPeerId)
{
	paf::string selectSql = telFormatString(
		"SELECT PeerID FROM PeerTbl WHERE PhoneNumber = '%s';", phoneNumber.GetDigits());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) == 0) {
		dbutil::Value valPeerId;
		stmt->GetValue(0, &valPeerId);
		*outPeerId = valPeerId.data.integer;
		stmt->Step(1);
		stmt->Finalize();
		return TelephonyResult(TEL_OK);
	}

	stmt->Finalize();
	paf::string insertSql = telFormatString(
		"INSERT INTO PeerTbl(PhoneNumber, CreatedTime, LastUpdateTime) "
		"VALUES('%s', datetime('now'), datetime('now'));",
		phoneNumber.GetDigits());
	err = m_connection->Execute(insertSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}
	*outPeerId = m_connection->GetLastInsertRowid();
	return TelephonyResult(TEL_OK);
}

/* 0x813E958C */
TelephonyResult TelSmsDbManager::InsertMessage(TelSmsMessage *message)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	if (!message->IsValid())
		return TelephonyResult(TEL_ERR_IO_FAILED);

	int beginErr = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (beginErr != 0)
		return TelephonyResult(beginErr);

	int initialStatus = message->GetFirstPeer().GetStatus();
	int messageKind = message->GetMessageKind();
	int messageType = message->GetMessageType();

	paf::common::String bodyConv(message->GetBody());
	paf::string escapedBody = escapeSqlSingleQuotes(bodyConv.GetString());

	paf::vector<SceInt64> peerIds;
	const paf::vector<TelSmsPeerInfo> &peers = message->GetPeers();
	for (SceSize idx = 0; idx < peers.size(); ++idx) {
		SceInt64 peerId = 0;
		TelPhoneNumber phone(peers[idx].GetAddress().GetNumber().GetDigits());
		TelephonyResult peerRes = ResolveOrInsertPeer(phone, &peerId);
		if (!peerRes.IsSuccess())
			return peerRes;
		if (peerId != 0)
			peerIds.push_back(peerId);
	}

	paf::string deliverTimeStr = message->GetDeliverTime().FormatIsoString();
	paf::string insertMsgSql = telFormatString(
		"INSERT INTO SmsMessageTbl(MessageKind, MessageType, HistoryTime, CreatedTime, "
		"LastUpdateTime, DeliverTime )VALUES(%d, %d, julianday('now'), datetime('now'), "
		"datetime('now'), '%s');",
		messageKind, messageType, deliverTimeStr.c_str());

	int execErr = m_connection->Execute(insertMsgSql, 0, NULL, NULL, NULL, NULL);
	if (execErr != 0) {
		m_connection->Rollback();
		return TelephonyResult(execErr);
	}

	SceInt64 insertedRowId = m_connection->GetLastInsertRowid();
	TelSmsMessageId insertedMsgId((SceUInt64)insertedRowId);

	int commitErr = m_connection->Commit();
	if (commitErr != 0) {
		m_connection->Rollback();
		return TelephonyResult(commitErr);
	}

	/* Bind escaped message text blob */
	paf::string updateMsgSql = telFormatString(
		"UPDATE SmsMessageTbl SET Message = ? WHERE rowid = %llu;",
		(SceUInt64)insertedRowId);
	dbutil::RefPtr<dbutil::Statement> stmt;
	int prepErr = m_connection->Prepare(updateMsgSql, &stmt);
	if (prepErr != 0)
		return TelephonyResult(prepErr);

	dbutil::Value blobValue;
	sce_paf_memset(&blobValue, 0, sizeof(blobValue));
	blobValue.type = SQLITE_BLOB;
	blobValue.data.blob = escapedBody.c_str();
	blobValue.size = (int)escapedBody.length();

	int bindErr = stmt->Bind(1, &blobValue, false);
	if (bindErr != 0)
		return TelephonyResult(bindErr);

	unsigned int stepRes = (unsigned int)stmt->Step(1);
	if (stepRes >= 2U)
		return TelephonyResult((int)stepRes);
	stmt->Finalize();

	/* Insert SmsHistoryTbl rows and update conversation filters */
	beginErr = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (beginErr != 0)
		return TelephonyResult(beginErr);

	for (SceSize idx = 0; idx < peerIds.size(); ++idx) {
		paf::string historySql = telFormatString(
			"INSERT INTO SmsHistoryTbl(MessageID, PeerID, Status, CreatedTime, "
			"LastUpdateTime) VALUES(%llu, %llu, %d, datetime('now'), datetime('now'));",
			insertedMsgId.GetValue(), (SceUInt64)peerIds[idx], initialStatus);
		int histErr = m_connection->Execute(historySql, 0, NULL, NULL, NULL, NULL);
		if (histErr != 0) {
			m_connection->Rollback();
			return TelephonyResult(histErr);
		}
	}

	for (SceSize idx = 0; idx < peerIds.size(); ++idx) {
		paf::vector<SceInt64> singlePeer;
		singlePeer.push_back(peerIds[idx]);
		TelephonyResult filterRes =
			UpdateFilterForPeers(singlePeer, insertedMsgId, &initialStatus, true);
		if (!filterRes.IsSuccess())
			return filterRes;
	}

	if (peerIds.size() > 1U) {
		TelephonyResult groupFilterRes =
			UpdateFilterForPeers(peerIds, insertedMsgId, &initialStatus, true);
		if (!groupFilterRes.IsSuccess())
			return groupFilterRes;
	}

	commitErr = m_connection->Commit();
	if (commitErr != 0) {
		m_connection->Rollback();
		return TelephonyResult(commitErr);
	}

	TelephonyResult reloadRes = GetMessageById(insertedMsgId, message);
	TelSmsDbMessageIdEvent notifyEvent(TEL_SMS_DB_EVENT_MESSAGE_INSERTED, initialStatus, -1,
					   insertedMsgId);
	NotifyObservers(notifyEvent);
	return reloadRes;
}

/* 0x813EA01A */
TelephonyResult TelSmsDbManager::UpdateMessageStatus(const TelSmsMessageId &messageId,
						     const int *status)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string updateSql = telFormatString(
		"UPDATE SmsHistoryTbl SET Status = %d, LastUpdateTime = datetime('now') "
		"WHERE MessageID = %llu;",
		*status, messageId.GetValue());

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	err = m_connection->Execute(updateSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	TelSmsDbMessageIdEvent notifyEvent(TEL_SMS_DB_EVENT_STATUS_UPDATED, *status, -1,
					   messageId);
	NotifyObservers(notifyEvent);
	return TelephonyResult(TEL_OK);
}

/* 0x813EA17C */
TelephonyResult TelSmsDbManager::UpdateMessageStatusForPeer(const TelSmsMessageId &messageId,
							    const TelPhoneNumber &phoneNumber,
							    const int *status)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	if (sce_paf_strncmp(phoneNumber.GetDigits(), "", 32) == 0)
		return UpdateMessageStatus(messageId, status);

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	paf::string updateSql = telFormatString(
		"UPDATE SmsHistoryTbl SET Status = %d, LastUpdateTime = datetime('now') "
		"WHERE MessageID = %llu AND PeerID IN "
		"(SELECT DISTINCT PeerId FROM PeerTbl WHERE PhoneNumber = '%s');",
		*status, messageId.GetValue(), phoneNumber.GetDigits());

	err = m_connection->Execute(updateSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	SceInt64 peerId = 0;
	TelephonyResult peerRes = ResolveOrInsertPeer(phoneNumber, &peerId);
	if (!peerRes.IsSuccess())
		return peerRes;

	paf::vector<SceInt64> singlePeer;
	singlePeer.push_back(peerId);
	TelephonyResult filterRes = UpdateFilterForPeers(singlePeer, messageId, status, false);
	if (!filterRes.IsSuccess())
		return filterRes;

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	TelSmsDbMessageIdEvent notifyEvent(TEL_SMS_DB_EVENT_STATUS_UPDATED, *status, -1,
					   messageId);
	NotifyObservers(notifyEvent);
	return TelephonyResult(TEL_OK);
}

/* 0x813EA496 */
TelephonyResult TelSmsDbManager::DeleteMessages(const paf::vector<TelSmsMessageId> &messageIds)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	for (SceSize idx = 0; idx < messageIds.size(); ++idx) {
		paf::string deleteSql = telFormatString(
			"DELETE FROM SmsMessageTbl WHERE MessageID = %llu;",
			messageIds[idx].GetValue());

		int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
		if (err != 0)
			return TelephonyResult(err);

		err = m_connection->Execute(deleteSql, 0, NULL, NULL, NULL, NULL);
		if (err != 0) {
			m_connection->Rollback();
			return TelephonyResult(err);
		}

		err = m_connection->Commit();
		if (err != 0) {
			m_connection->Rollback();
			return TelephonyResult(err);
		}

		TelSmsDbMessageIdEvent notifyEvent(TEL_SMS_DB_EVENT_MESSAGE_DELETED, -1, -1,
						   messageIds[idx]);
		NotifyObservers(notifyEvent);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813EA672 */
TelephonyResult TelSmsDbManager::DeleteAllMessages()
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	static const char *const kClearStatements[4] = {
		"DELETE FROM SmsMessageTbl;",
		"DELETE FROM SmsPduTbl;",
		"DELETE FROM PeerTbl;",
		"DELETE FROM SmsHistoryTbl;"
	};

	for (int idx = 0; idx < 4; ++idx) {
		paf::string sql(kClearStatements[idx]);
		err = m_connection->Execute(sql, 0, NULL, NULL, NULL, NULL);
		if (err != 0) {
			m_connection->Rollback();
			return TelephonyResult(err);
		}
	}

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	TelEventInfo notifyEvent(TEL_SMS_DB_EVENT_ALL_DELETED, -1, -1);
	NotifyObservers(notifyEvent);
	return TelephonyResult(TEL_OK);
}

/* 0x813EA8D0 */
TelephonyResult TelSmsDbManager::FindMessageIdsByFilter(const TelSmsFilter &filter,
							paf::vector<TelSmsMessageId> *outIds)
{
	const paf::vector<TelSmsAddress> &addresses = filter.GetAddresses();
	if (addresses.empty())
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	paf::string phoneCsv = joinQuotedPhonesCsv(addresses);
	paf::string whereClause =
		telFormatString("WHERE PeerTbl.PhoneNumber IN (%s)", phoneCsv.c_str());
	paf::string selectSql = telFormatString(
		"SELECT DISTINCT SmsMessageTbl.MessageID FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON SmsHistoryTbl.PeerID = PeerTbl.PeerID %s;",
		whereClause.c_str());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	outIds->clear();
	do {
		dbutil::Value valMsgId;
		stmt->GetValue(0, &valMsgId);
		outIds->push_back(TelSmsMessageId((SceUInt64)valMsgId.data.integer));
	} while (stmt->Step(1) == 0);

	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EAC8A */
TelephonyResult TelSmsDbManager::DeleteMessagesByFilter(const TelSmsFilter &filter)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string whereClause("");
	if (filter.IsOutgoingOnly()) {
		whereClause = "WHERE MessageType = 1";
	} else if (filter.IsIncomingOnly()) {
		whereClause = "WHERE MessageType = 2";
	} else {
		if (filter.GetAddresses().empty())
			return DeleteAllMessages();

		paf::vector<TelSmsMessageId> matchingIds;
		FindMessageIdsByFilter(filter, &matchingIds);
		if (!matchingIds.empty()) {
			paf::string idCsv = joinMessageIdsCsv(matchingIds);
			whereClause = telFormatString("WHERE MessageID IN (%s)", idCsv.c_str());
		}
	}

	paf::string deleteSql =
		telFormatString("DELETE FROM SmsMessageTbl %s ;", whereClause.c_str());

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	err = m_connection->Execute(deleteSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	TelEventInfo notifyEvent(TEL_SMS_DB_EVENT_ALL_DELETED, -1, -1);
	NotifyObservers(notifyEvent);
	return TelephonyResult(TEL_OK);
}

/* 0x813EB05E */
TelephonyResult TelSmsDbManager::GetMessageById(const TelSmsMessageId &messageId,
						TelSmsMessage *outMessage)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	if (!messageId.IsValid())
		return TelephonyResult(TEL_ERR_FAILED);

	paf::string selectSql = telFormatString(
		"SELECT SmsMessageTbl.MessageID, SmsMessageTbl.MessageKind, "
		"SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"PeerTbl.PhoneNumber, SmsHistoryTbl.Status, SmsMessageTbl.Message, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', DeliverTime, 'localtime') "
		"FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsMessageTbl.MessageID = %llu ;",
		messageId.GetValue());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	paf::vector<TelSmsPeerInfo> peers;
	do {
		populateMessageFromStatement(stmt.Get(), &peers, outMessage);
	} while (stmt->Step(1) == 0);

	outMessage->SetPeers(peers);
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EB40A */
paf::string TelSmsDbManager::FormatTimeModifier(TelSmsTimeUnit timeUnit, int timeOffset) const
{
	switch (timeUnit) {
	case TEL_SMS_TIME_SECONDS:
		return telFormatString("%d seconds", timeOffset);
	case TEL_SMS_TIME_MINUTES:
		return telFormatString("%d minutes", timeOffset);
	case TEL_SMS_TIME_HOURS:
		return telFormatString("%d hours", timeOffset);
	case TEL_SMS_TIME_DAYS:
		return telFormatString("%d days", timeOffset);
	case TEL_SMS_TIME_MONTHS:
		return telFormatString("%d months", timeOffset);
	case TEL_SMS_TIME_YEARS:
		return telFormatString("%d years", timeOffset);
	default:
		return paf::string("");
	}
}

/* 0x813EB55C */
TelephonyResult TelSmsDbManager::FindConcatenatedMessage(TelSmsTimeUnit timeUnit, int timeOffset,
							 const TelSmsAddress &sender,
							 int csmsReferenceNumber,
							 const int *status,
							 TelSmsMessage *outMessage)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string modifier = FormatTimeModifier(timeUnit, timeOffset);
	paf::string selectSql = telFormatString(
		"SELECT SmsMessageTbl.MessageID, SmsMessageTbl.MessageKind, "
		"SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"PeerTbl.PhoneNumber, SmsHistoryTbl.Status, SmsMessageTbl.Message, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', DeliverTime, 'localtime') "
		"FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsMessageTbl.MessageID IN ( "
		"SELECT SmsHistoryTbl.MessageID FROM SmsHistoryTbl "
		"WHERE SmsHistoryTbl.PeerID IN( "
		"SELECT PeerTbl.PeerID FROM PeerTbl WHERE PeerTbl.PhoneNumber IN ('%s') ) ) "
		"AND SmsMessageTbl.MessageID IN ( "
		"SELECT SmsPduTbl.MessageID FROM SmsPduTbl "
		"WHERE SmsPduTbl.csmsReferenceNumber = %d "
		"GROUP BY SmsPduTbl.CsmsSequenceNumber "
		"ORDER BY SmsPduTbl.CsmsSequenceNumber ASC ) "
		"AND datetime('now', '%s') < SmsMessageTbl.LastUpdateTime "
		"AND SmsHistoryTbl.Status = %d ;",
		sender.GetNumber().GetDigits(), csmsReferenceNumber, modifier.c_str(), *status);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	paf::vector<TelSmsPeerInfo> peers;
	do {
		populateMessageFromStatement(stmt.Get(), &peers, outMessage);
	} while (stmt->Step(1) == 0);

	outMessage->SetPeers(peers);
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EB8FE */
TelephonyResult TelSmsDbManager::GetTotalMessageCount(int *outCount)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql("SELECT COUNT(*) FROM SmsMessageTbl;");
	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	*outCount = 0;
	while (stmt->Step(1) == 0) {
		dbutil::Value valCount;
		stmt->GetValue(0, &valCount);
		*outCount = (int)valCount.data.integer;
	}
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EB9CC */
void TelSmsDbManager::ReadMessageList(dbutil::RefPtr<dbutil::Statement> *statement,
				      paf::vector<TelSmsMessage> *outMessages, bool appendOrder)
{
	outMessages->clear();
	TelSmsAddress emptyAddr;

	while ((*statement)->Step(1) == 0) {
		dbutil::Value valMsgId;
		dbutil::Value valStatus;
		(*statement)->GetValue(0, &valMsgId);
		(*statement)->GetValue(1, &valStatus);

		TelSmsMessageId msgId((SceUInt64)valMsgId.data.integer);
		TelSmsPeerInfo peer(emptyAddr, (int)valStatus.data.integer, false, 0, 4);
		paf::common::String emptyStr;
		TelDateTime emptyTime;

		TelSmsMessage msg;
		msg.SetReceivedData(msgId, peer, emptyStr.GetWString(), emptyTime);
		if (appendOrder)
			outMessages->push_back(msg);
		else
			outMessages->insert(outMessages->begin(), msg);
	}
	(*statement)->Finalize();
}

/* 0x813EBBC2 */
TelephonyResult TelSmsDbManager::GetMessagesPaged(int offset, int limit,
						  paf::vector<TelSmsMessage> *outMessages)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT MessageID, DataType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"Peer, Status, Message, datetime(CreatedTime, 'localtime'), "
		"datetime(LastUpdateTime, 'localtime') "
		"FROM SmsHistoryTbl ORDER BY MessageID DESC LIMIT %d OFFSET %d;",
		limit, offset);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	ReadMessageList(&stmt, outMessages, true);
	return TelephonyResult(TEL_OK);
}

/* 0x813EBC82 */
TelephonyResult TelSmsDbManager::GetLatestMessageStatuses(int limit,
							  paf::vector<TelSmsMessage> *outMessages)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql =
		telFormatString("SELECT MessageID, Status FROM SmsHistoryTbl LIMIT %d;", limit);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	ReadMessageList(&stmt, outMessages, true);
	return TelephonyResult(TEL_OK);
}

/* 0x813EBD3C */
TelephonyResult TelSmsDbManager::GetMessagesBefore(const TelSmsMessageId &cursorId, int limit,
						   paf::vector<TelSmsMessage> *outMessages)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT SmsMessageTbl.MessageID, SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"PeerTbl.PhoneNumber, SmsHistoryTbl.Status, SmsMessageTbl.Message, "
		"datetime(SmsMessageTbl.CreatedTime, 'localtime'), "
		"datetime(SmsMessageTbl.LastUpdateTime, 'localtime'), "
		"datetime(SmsMessageTbl.DeliverTime, 'localtime') "
		"FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsMessageTbl.MessageID < %llu "
		"ORDER BY SmsMessageTbl.MessageID DESC LIMIT %d;",
		cursorId.GetValue(), limit);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	ReadMessageList(&stmt, outMessages, true);
	return TelephonyResult(TEL_OK);
}

/* 0x813EBE08 */
TelephonyResult TelSmsDbManager::GetMessagesAfter(const TelSmsMessageId &cursorId, int limit,
						  paf::vector<TelSmsMessage> *outMessages)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT SmsMessageTbl.MessageID, SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"PeerTbl.PhoneNumber, SmsHistoryTbl.Status, SmsMessageTbl.Message, "
		"datetime(SmsMessageTbl.CreatedTime, 'localtime'), "
		"datetime(SmsMessageTbl.LastUpdateTime, 'localtime'), "
		"datetime(SmsMessageTbl.DeliverTime, 'localtime') "
		"FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsMessageTbl.MessageID > %llu "
		"ORDER BY SmsMessageTbl.MessageID DESC LIMIT %d;",
		cursorId.GetValue(), limit);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	ReadMessageList(&stmt, outMessages, false);
	return TelephonyResult(TEL_OK);
}

/* 0x813EBED4 */
TelephonyResult TelSmsDbManager::GetMessagesBetween(const TelSmsMessageId &upperId,
						    const TelSmsMessageId &lowerId, int limit,
						    paf::vector<TelSmsMessage> *outMessages)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT SmsMessageTbl.MessageID, SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"PeerTbl.PhoneNumber, SmsHistoryTbl.Status, SmsMessageTbl.Message, "
		"datetime(SmsMessageTbl.CreatedTime, 'localtime'), "
		"datetime(SmsMessageTbl.LastUpdateTime, 'localtime'), "
		"datetime(SmsMessageTbl.DeliverTime, 'localtime') "
		"FROM SmsMessageTbl "
		"INNER JOIN SmsHistoryTbl ON SmsMessageTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsMessageTbl.MessageID BETWEEN %llu AND %llu "
		"ORDER BY SmsMessageTbl.MessageID DESC LIMIT %d;",
		lowerId.GetValue(), upperId.GetValue(), limit);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	ReadMessageList(&stmt, outMessages, true);
	return TelephonyResult(TEL_OK);
}

/* 0x813EBFBE */
TelephonyResult TelSmsDbManager::GetConnection(dbutil::Connection **outConnection)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	*outConnection = m_connection;
	return TelephonyResult(TEL_OK);
}

/* 0x813EBFE2 */
TelephonyResult TelSmsDbManager::BuildFilterQueries(const TelSmsFilter &filter,
						    paf::string *outCountSql,
						    paf::string *outSelectSql)
{
	paf::string whereClause("");
	if (filter.IsOutgoingOnly()) {
		whereClause = "WHERE SmsMessageTbl.MessageType = 1";
	} else if (filter.IsIncomingOnly()) {
		whereClause = "WHERE SmsMessageTbl.MessageType = 2";
	} else if (!filter.IsAllMessages()) {
		const paf::vector<TelSmsAddress> &addresses = filter.GetAddresses();
		if (addresses.empty())
			return TelephonyResult(TEL_ERR_NOT_FOUND);

		paf::string phoneCsv = joinQuotedPhonesCsv(addresses);
		whereClause = telFormatString(
			"WHERE SmsMessageTbl.MessageID IN ( "
			"SELECT SmsHistoryTbl.MessageID FROM SmsHistoryTbl "
			"WHERE SmsHistoryTbl.PeerID IN ( "
			"SELECT PeerTbl.PeerID FROM PeerTbl WHERE PeerTbl.PhoneNumber IN (%s) ) ) ",
			phoneCsv.c_str());
	}

	*outSelectSql = telFormatString(
		"SELECT DISTINCT SmsMessageTbl.MessageID, SmsMessageTbl.MessageType, "
		"strftime('%%Y-%%m-%%d %%H:%%M:%%f', HistoryTime, 'localtime'), "
		"SmsMessageTbl.Message, datetime(SmsMessageTbl.CreatedTime, 'localtime'), "
		"datetime(SmsMessageTbl.LastUpdateTime, 'localtime'), "
		"datetime(SmsMessageTbl.DeliverTime, 'localtime') "
		"FROM SmsMessageTbl %s ORDER BY SmsMessageTbl.MessageID DESC;",
		whereClause.c_str());

	*outCountSql = telFormatString(
		"SELECT DISTINCT COUNT(*) FROM SmsMessageTbl %s "
		"ORDER BY SmsMessageTbl.MessageID DESC;",
		whereClause.c_str());

	return TelephonyResult(TEL_OK);
}

/* 0x813EC264 */
TelephonyResult TelSmsDbManager::BuildFilterListQueries(const int *sortKey,
							paf::string *outCountSql,
							paf::string *outSelectSql)
{
	const char *orderColumn = NULL;
	if (*sortKey == TEL_SMS_SORT_LAST_UPDATE_TIME)
		orderColumn = "LastUpdateTime";
	else if (*sortKey == TEL_SMS_SORT_MESSAGE_COUNT)
		orderColumn = "MessageCount";
	else
		return TelephonyResult(TEL_ERR_IO_FAILED);

	*outSelectSql = telFormatString(
		"SELECT FilterID, FilterType, LatestMessageID, MessageCount, UnreadCount, "
		"datetime(CreatedTime, 'localtime'), datetime(LastUpdateTime, 'localtime') "
		"FROM FilterTbl ORDER BY %s DESC;",
		orderColumn);

	*outCountSql = paf::string("SELECT COUNT(*) FROM FilterTbl;");
	return TelephonyResult(TEL_OK);
}

/* 0x813EC38A */
TelephonyResult TelSmsDbManager::LoadFilterPeers(TelSmsFilter *filter)
{
	paf::string selectSql = telFormatString(
		"SELECT PeerTbl.PhoneNumber FROM PeerTbl "
		"INNER JOIN FilterPeerTbl ON PeerTbl.PeerID = FilterPeerTbl.PeerID "
		"WHERE FilterPeerTbl.FilterID = %llu;",
		filter->GetFilterId().GetValue());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	paf::vector<TelSmsAddress> peerAddresses;
	do {
		dbutil::Value valPhone;
		stmt->GetValue(0, &valPhone);
		TelSmsAddress addr;
		TelPhoneNumber phone(valPhone.data.text != NULL ? valPhone.data.text : "");
		addr.CopyFrom(phone, "");
		peerAddresses.push_back(addr);
	} while (stmt->Step(1) == 0);

	stmt->Finalize();
	filter->SetAddresses(peerAddresses);
	return TelephonyResult(TEL_OK);
}

/* 0x813EC53C */
TelephonyResult TelSmsDbManager::FindMessageIdsByStatus(const int *status,
							paf::vector<TelSmsMessageId> *outIds)
{
	outIds->clear();
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT SmsHistoryTbl.MessageID FROM SmsHistoryTbl "
		"WHERE SmsHistoryTbl.Status = %d ;",
		*status);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) == 0) {
		dbutil::Value valMsgId;
		stmt->GetValue(0, &valMsgId);
		outIds->push_back(TelSmsMessageId((SceUInt64)valMsgId.data.integer));
	}
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EC6D6 */
TelephonyResult TelSmsDbManager::GetConcatenatedPduStatus(const TelSmsMessageId &messageId,
							  TelSmsAddress *outSender,
							  int *outReferenceNumber,
							  int *outTotalNumber,
							  int *outReceivedCount)
{
	TelPhoneNumber emptyPhone("");
	outSender->CopyFrom(emptyPhone, "");
	*outReferenceNumber = 0;
	*outTotalNumber = 0;
	*outReceivedCount = 0;

	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string metaSql = telFormatString(
		"SELECT PeerTbl.PhoneNumber, SmsPduTbl.CsmsReferenceNumber, SmsPduTbl.TotalNumber "
		"FROM SmsPduTbl "
		"INNER JOIN SmsHistoryTbl ON SmsPduTbl.MessageID = SmsHistoryTbl.MessageID "
		"INNER JOIN PeerTbl ON PeerTbl.PeerID = SmsHistoryTbl.PeerID "
		"WHERE SmsPduTbl.MessageID = %llu ;",
		messageId.GetValue());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(metaSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	err = stmt->Step(1);
	if (err != 0)
		return TelephonyResult(err);

	dbutil::Value valPhone;
	dbutil::Value valRef;
	dbutil::Value valTotal;
	stmt->GetValue(0, &valPhone);
	stmt->GetValue(1, &valRef);
	stmt->GetValue(2, &valTotal);

	TelPhoneNumber phone(valPhone.data.text != NULL ? valPhone.data.text : "");
	outSender->CopyFrom(phone, "");
	*outReferenceNumber = (int)valRef.data.integer;
	*outTotalNumber = (int)valTotal.data.integer;
	stmt->Finalize();

	paf::string countSql = telFormatString(
		"SELECT count(*) FROM ( SELECT * FROM SmsPduTbl "
		"WHERE SmsPduTbl.MessageID = %llu GROUP BY SmsPduTbl.CsmsSequenceNumber ) ;",
		messageId.GetValue());

	err = m_connection->Prepare(countSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	err = stmt->Step(1);
	if (err != 0)
		return TelephonyResult(err);

	dbutil::Value valCount;
	stmt->GetValue(0, &valCount);
	*outReceivedCount = (int)valCount.data.integer;
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813EC944 */
TelephonyResult TelSmsDbManager::InsertPdu(const TelSmsMessageId &messageId,
					   const TelSmsAddress &sender, int csmsReferenceNumber,
					   int csmsSequenceNumber, int totalNumber,
					   const TelSmsProtocolDataUnit &pdu, SceInt64 *outRowId)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	SceInt64 peerId = 0;
	TelPhoneNumber phone(sender.GetNumber().GetDigits());
	TelephonyResult peerRes = ResolveOrInsertPeer(phone, &peerId);
	if (!peerRes.IsSuccess())
		return peerRes;

	paf::string insertSql = telFormatString(
		"INSERT INTO SmsPduTbl(MessageID, PeerID, CsmsReferenceNumber, "
		"CsmsSequenceNumber, TotalNumber, CreatedTime, LastUpdateTime, Pdu, DeliverTime) "
		"VALUES(%llu, %llu, %d, %d, %d, datetime('now'), datetime('now'), ?, "
		"datetime('now'));",
		messageId.GetValue(), (SceUInt64)peerId, csmsReferenceNumber, csmsSequenceNumber,
		totalNumber);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(insertSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	dbutil::Value blobValue;
	sce_paf_memset(&blobValue, 0, sizeof(blobValue));
	blobValue.type = SQLITE_BLOB;
	blobValue.data.blob = pdu.GetRawData();
	blobValue.size = (int)pdu.GetLength();

	err = stmt->Bind(1, &blobValue, false);
	if (err != 0)
		return TelephonyResult(err);

	unsigned int stepRes = (unsigned int)stmt->Step(1);
	if (stepRes >= 2U)
		return TelephonyResult((int)stepRes);

	*outRowId = m_connection->GetLastInsertRowid();
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813ECB4A */
TelephonyResult TelSmsDbManager::CountReceivedPdus(TelSmsTimeUnit timeUnit, int timeOffset,
						   const TelSmsAddress &sender,
						   int csmsReferenceNumber, const int *status,
						   int *outCount)
{
	*outCount = 0;
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	SceInt64 peerId = 0;
	TelPhoneNumber phone(sender.GetNumber().GetDigits());
	TelephonyResult peerRes = ResolveOrInsertPeer(phone, &peerId);
	if (!peerRes.IsSuccess())
		return peerRes;

	paf::string modifier = FormatTimeModifier(timeUnit, timeOffset);
	paf::string selectSql = telFormatString(
		"SELECT Count(*) FROM ( SELECT * FROM SmsPduTbl "
		"INNER JOIN SmsHistoryTbl ON SmsPduTbl.MessageID = SmsHistoryTbl.MessageID "
		"WHERE SmsPduTbl.PeerId = %llu AND SmsPduTbl.CsmsReferenceNumber = %d "
		"AND datetime('now', '%s') < SmsPduTbl.LastUpdateTime "
		"AND SmsHistoryTbl.Status = %d GROUP BY SmsPduTbl.CsmsSequenceNumber ) ;",
		(SceUInt64)peerId, csmsReferenceNumber, modifier.c_str(), *status);

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	err = stmt->Step(1);
	if (err == 0) {
		dbutil::Value valCount;
		stmt->GetValue(0, &valCount);
		*outCount = (int)valCount.data.integer;
	}
	stmt->Finalize();
	return TelephonyResult(err);
}

/* 0x813ECD26 */
TelephonyResult TelSmsDbManager::GetPdu(const TelSmsMessageId &messageId,
					const TelSmsAddress &sender, int csmsReferenceNumber,
					int csmsSequenceNumber, int totalNumber,
					TelSmsProtocolDataUnit *outPdu)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	if (!messageId.IsValid())
		return TelephonyResult(TEL_ERR_FAILED);

	SceInt64 peerId = 0;
	TelPhoneNumber phone(sender.GetNumber().GetDigits());
	TelephonyResult peerRes = ResolveOrInsertPeer(phone, &peerId);
	if (!peerRes.IsSuccess())
		return peerRes;

	paf::string selectSql = telFormatString(
		"SELECT SmsPduTbl.Pdu FROM SmsPduTbl "
		"WHERE SmsPduTbl.MessageID = %llu AND SmsPduTbl.PeerID = %llu "
		"AND SmsPduTbl.csmsReferenceNumber = %d "
		"AND SmsPduTbl.csmsSequenceNumber = %d AND SmsPduTbl.TotalNumber = %d;",
		messageId.GetValue(), (SceUInt64)peerId, csmsReferenceNumber, csmsSequenceNumber,
		totalNumber);

	dbutil::RefPtr<dbutil::Statement> stmt;
	if (m_connection->Prepare(selectSql, &stmt) != 0)
		return TelephonyResult(TEL_OK);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	do {
		dbutil::Value valBlob;
		stmt->GetValue(0, &valBlob);
		outPdu->SetBytes(valBlob.data.blob, (SceSize)valBlob.size);
	} while (stmt->Step(1) == 0);

	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813ECF6E */
TelephonyResult TelSmsDbManager::DeletePdusByMessageId(const SceUInt64 *messageId)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string deleteSql =
		telFormatString("DELETE FROM SmsPduTbl WHERE MessageID = %llu ;", *messageId);

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	err = m_connection->Execute(deleteSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813ED092 */
TelephonyResult TelSmsDbManager::GetMessageStatus(const TelSmsMessageId &messageId,
						  int *outStatus)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql = telFormatString(
		"SELECT SmsHistoryTbl.Status FROM SmsHistoryTbl WHERE MessageID = %llu;",
		messageId.GetValue());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(selectSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	if (stmt->Step(1) != 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	dbutil::Value valStatus;
	stmt->GetValue(0, &valStatus);
	*outStatus = (int)valStatus.data.integer;
	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813ED19A */
TelephonyResult TelSmsDbManager::UpdateMessageBody(const TelSmsMessageId &messageId,
						   const char *utf8Body, SceSize length)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string updateSql = telFormatString(
		"UPDATE SmsMessageTbl SET Message=? WHERE MessageID = %llu;",
		messageId.GetValue());

	dbutil::RefPtr<dbutil::Statement> stmt;
	int err = m_connection->Prepare(updateSql, &stmt);
	if (err != 0)
		return TelephonyResult(err);

	dbutil::Value blobValue;
	sce_paf_memset(&blobValue, 0, sizeof(blobValue));
	blobValue.type = SQLITE_BLOB;
	blobValue.data.blob = utf8Body;
	blobValue.size = (int)length;

	err = stmt->Bind(1, &blobValue, false);
	if (err != 0)
		return TelephonyResult(err);

	unsigned int stepRes = (unsigned int)stmt->Step(1);
	if (stepRes >= 2U)
		return TelephonyResult((int)stepRes);

	stmt->Finalize();
	return TelephonyResult(TEL_OK);
}

/* 0x813ED30A */
TelephonyResult TelSmsDbManager::PruneOldMessages(int keepCount)
{
	if (keepCount == 0)
		return TelephonyResult(TEL_OK);

	int err = m_connection->Begin(DBUTIL_TRANSACTION_DEFERRED);
	if (err != 0)
		return TelephonyResult(err);

	paf::string prunePduSql = telFormatString(
		"DELETE FROM SmsPduTbl WHERE MessageID IN "
		"(SELECT MessageID FROM SmsMessageTbl ORDER BY MessageID DESC LIMIT -1 OFFSET %d );",
		keepCount);
	err = m_connection->Execute(prunePduSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	paf::string pruneHistorySql = telFormatString(
		"DELETE FROM SmsHistoryTbl WHERE MessageID IN "
		"(SELECT MessageID FROM SmsMessageTbl ORDER BY MessageID DESC LIMIT -1 OFFSET %d );",
		keepCount);
	err = m_connection->Execute(pruneHistorySql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	paf::string pruneMsgSql = telFormatString(
		"DELETE FROM SmsMessageTbl WHERE MessageID IN"
		"( SELECT MessageID FROM SmsMessageTBl ORDER BY MessageID DESC LIMIT -1 OFFSET %d);",
		keepCount);
	err = m_connection->Execute(pruneMsgSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	paf::string oneMonthAgo = FormatTimeModifier(TEL_SMS_TIME_MONTHS, -1);

	paf::string stalePduSql = telFormatString(
		"DELETE FROM SmsPduTbl WHERE  datetime( 'now', '%s' ) > LastUpdateTime "
		"AND MessageID IN (SELECT MessageID FROM SmsHistoryTbl "
		"WHERE Status = 6 AND datetime( 'now', '%s') > LastUpdateTime);",
		oneMonthAgo.c_str(), oneMonthAgo.c_str());
	err = m_connection->Execute(stalePduSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	paf::string staleMsgSql = telFormatString(
		"DELETE FROM SmsMessageTbl WHERE  datetime( 'now', '%s' ) > LastUpdateTime "
		"AND MessageID IN (SELECT MessageID FROM SmsHistoryTbl "
		"WHERE Status = 6 AND datetime( 'now', '%s') > LastUpdateTime);",
		oneMonthAgo.c_str(), oneMonthAgo.c_str());
	err = m_connection->Execute(staleMsgSql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	paf::string staleHistorySql = telFormatString(
		"DELETE FROM SmsHistoryTbl WHERE Status = 6 AND datetime( 'now', '%s') > LastUpdateTime;",
		oneMonthAgo.c_str());
	err = m_connection->Execute(staleHistorySql, 0, NULL, NULL, NULL, NULL);
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}

	err = m_connection->Commit();
	if (err != 0) {
		m_connection->Rollback();
		return TelephonyResult(err);
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813ED6E0 */
TelephonyResult TelSmsDbManager::GetUnreadMessageCount(int *outCount)
{
	if (m_connection == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	paf::string selectSql("SELECT COUNT(*) FROM SmsHistoryTbl WHERE Status = 7");
	dbutil::RefPtr<dbutil::Statement> stmt;
	if (m_connection->Prepare(selectSql, &stmt) == 0) {
		while (stmt->Step(1) == 0) {
			dbutil::Value valCount;
			stmt->GetValue(0, &valCount);
			*outCount = (int)valCount.data.integer;
		}
		stmt->Finalize();
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813ED78C */
TelephonyResult TelSmsDbManager::NotifyMessageArrived(const TelSmsMessage &message)
{
	TelSmsDbMessageEvent notifyEvent(TEL_SMS_DB_EVENT_MESSAGE_RECEIVED, message);
	NotifyObservers(notifyEvent);
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
