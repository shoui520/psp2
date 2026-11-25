/*
 * tel_sms_dbmanager.h: SQLite SMS database migrator, filter/event types,
 * and TelSmsDbManager interface for telephony_plugin
 * (3.65-CEX: 0x813E75C0..0x813ED8B7).
 */
#ifndef TELEPHONY_SMS_DBMANAGER_H
#define TELEPHONY_SMS_DBMANAGER_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include "../common/tel_common.h"

namespace telephony {

enum TelSmsDbEventId {
	TEL_SMS_DB_EVENT_MESSAGE_INSERTED = 1537,
	TEL_SMS_DB_EVENT_STATUS_UPDATED = 1538,
	TEL_SMS_DB_EVENT_MESSAGE_DELETED = 1539,
	TEL_SMS_DB_EVENT_ALL_DELETED = 1540,
	TEL_SMS_DB_EVENT_MESSAGE_RECEIVED = 1542
};

enum TelSmsTimeUnit {
	TEL_SMS_TIME_SECONDS = 0,
	TEL_SMS_TIME_MINUTES = 1,
	TEL_SMS_TIME_HOURS = 2,
	TEL_SMS_TIME_DAYS = 3,
	TEL_SMS_TIME_MONTHS = 4,
	TEL_SMS_TIME_YEARS = 5
};

enum TelSmsFilterSortKey {
	TEL_SMS_SORT_LAST_UPDATE_TIME = 0,
	TEL_SMS_SORT_MESSAGE_COUNT = 1
};

enum TelSmsDbManagerType {
	TEL_SMS_DBMANAGER_SQLITE = 0
};

/* Event payload carrying a message ID for DB insert/update/delete notifications */
class TelSmsDbMessageIdEvent : public TelEventInfo {
public:
	TelSmsDbMessageIdEvent(int eventId, int status, int detail,
			       const TelSmsMessageId &messageId)
		: TelEventInfo(eventId, status, detail), m_messageId(messageId)
	{
		SetCategory(2);
	}
	virtual ~TelSmsDbMessageIdEvent() {}

	const TelSmsMessageId &GetMessageId() const { return m_messageId; }

private:
	TelSmsMessageId m_messageId;
};

/* Event payload carrying a full TelSmsMessage (event 1542) */
class TelSmsDbMessageEvent : public TelEventInfo {
public:
	TelSmsDbMessageEvent(int eventId, const TelSmsMessage &message)
		: TelEventInfo(eventId, -1, -1), m_message(message)
	{
		SetCategory(1);
	}
	virtual ~TelSmsDbMessageEvent() {}

	const TelSmsMessage &GetMessage() const { return m_message; }

private:
	TelSmsMessage m_message;
};

/* Filter descriptor used by TelSmsDbManager query and delete operations */
class TelSmsFilter : public TelObject {
public:
	TelSmsFilter()
		: m_outgoingOnly(false), m_incomingOnly(false), m_allMessages(false)
	{
	}
	virtual ~TelSmsFilter() {}

	const TelSmsMessageId &GetFilterId() const { return m_filterId; }
	void SetFilterId(const TelSmsMessageId &filterId) { m_filterId = filterId; }
	bool IsOutgoingOnly() const { return m_outgoingOnly; }
	void SetOutgoingOnly(bool outgoingOnly) { m_outgoingOnly = outgoingOnly; }
	bool IsIncomingOnly() const { return m_incomingOnly; }
	void SetIncomingOnly(bool incomingOnly) { m_incomingOnly = incomingOnly; }
	bool IsAllMessages() const { return m_allMessages; }
	void SetAllMessages(bool allMessages) { m_allMessages = allMessages; }
	const paf::vector<TelSmsAddress> &GetAddresses() const { return m_addresses; }
	void SetAddresses(const paf::vector<TelSmsAddress> &addresses) { m_addresses = addresses; }

private:
	TelSmsMessageId m_filterId;
	bool m_outgoingOnly;
	bool m_incomingOnly;
	bool m_allMessages;
	paf::vector<TelSmsAddress> m_addresses;
};

/* Migrates legacy telephony SMS database into newly copied template DB (0x813E75C0..0x813E8983) */
class TelSmsDbMigrator {
public:
	TelSmsDbMigrator(const paf::string &oldDbPath, const paf::string &newDbPath);
	~TelSmsDbMigrator();

	TelephonyResult MigrateIfNeeded();

private:
	TelephonyResult RemoveFileIfExists(const paf::string &path);
	TelephonyResult DetachAndClose();
	TelephonyResult OpenAndAttach(const paf::string &targetPath,
				      const paf::string &importPath);
	TelephonyResult MigratePeerTable(paf::vector<SceInt64> *outOldPeerIds);
	TelephonyResult MigrateMessageTable(paf::vector<SceInt64> *outOldMsgIds);
	TelephonyResult MigrateHistoryTable(const paf::vector<SceInt64> &oldMsgIds,
					    const paf::vector<SceInt64> &oldPeerIds);
	TelephonyResult ImportDatabase(const paf::string &oldPath, const paf::string &tempPath);
	TelephonyResult CopyFile(const paf::string &srcPath, const paf::string &dstPath);

	dbutil::Connection *m_connection;
	paf::string m_oldDbPath;
	paf::string m_newDbPath;
	paf::string m_tempDbPath;
};

/* SQLite SMS database manager (vtable 0x81541678, 0x813E89E4..0x813ED8B7) */
class TelSmsDbManager : public TelObservable {
public:
	explicit TelSmsDbManager(const paf::string &dbPath);
	virtual ~TelSmsDbManager();

	virtual TelephonyResult Open();
	virtual TelephonyResult Close();
	virtual TelephonyResult GetDbPath(paf::string *outPath);
	virtual TelephonyResult Interrupt();
	virtual TelephonyResult InsertMessage(TelSmsMessage *message);
	virtual TelephonyResult GetMessageStatus(const TelSmsMessageId &messageId,
						 int *outStatus);
	virtual TelephonyResult UpdateMessageStatus(const TelSmsMessageId &messageId,
						    const int *status);
	virtual TelephonyResult UpdateMessageStatusForPeer(const TelSmsMessageId &messageId,
							   const TelPhoneNumber &phoneNumber,
							   const int *status);
	virtual TelephonyResult DeleteMessages(const paf::vector<TelSmsMessageId> &messageIds);
	virtual TelephonyResult DeleteAllMessages();
	virtual TelephonyResult DeleteMessagesByFilter(const TelSmsFilter &filter);
	virtual TelephonyResult GetMessageById(const TelSmsMessageId &messageId,
					       TelSmsMessage *outMessage);
	virtual TelephonyResult FindConcatenatedMessage(TelSmsTimeUnit timeUnit, int timeOffset,
							const TelSmsAddress &sender,
							int csmsReferenceNumber,
							const int *status,
							TelSmsMessage *outMessage);
	virtual TelephonyResult GetTotalMessageCount(int *outCount);
	virtual TelephonyResult GetMessagesPaged(int offset, int limit,
						 paf::vector<TelSmsMessage> *outMessages);
	virtual TelephonyResult GetLatestMessageStatuses(int limit,
							 paf::vector<TelSmsMessage> *outMessages);
	virtual TelephonyResult GetMessagesBefore(const TelSmsMessageId &cursorId, int limit,
						  paf::vector<TelSmsMessage> *outMessages);
	virtual TelephonyResult GetMessagesAfter(const TelSmsMessageId &cursorId, int limit,
						 paf::vector<TelSmsMessage> *outMessages);
	virtual TelephonyResult GetMessagesBetween(const TelSmsMessageId &upperId,
						   const TelSmsMessageId &lowerId, int limit,
						   paf::vector<TelSmsMessage> *outMessages);
	virtual TelephonyResult GetConnection(dbutil::Connection **outConnection);
	virtual TelephonyResult BuildFilterQueries(const TelSmsFilter &filter,
						   paf::string *outCountSql,
						   paf::string *outSelectSql);
	virtual TelephonyResult BuildFilterListQueries(const int *sortKey,
						       paf::string *outCountSql,
						       paf::string *outSelectSql);
	virtual TelephonyResult LoadFilterPeers(TelSmsFilter *filter);
	virtual TelephonyResult FindMessageIdsByStatus(const int *status,
						       paf::vector<TelSmsMessageId> *outIds);
	virtual TelephonyResult GetConcatenatedPduStatus(const TelSmsMessageId &messageId,
							 TelSmsAddress *outSender,
							 int *outReferenceNumber,
							 int *outTotalNumber,
							 int *outReceivedCount);
	virtual TelephonyResult InsertPdu(const TelSmsMessageId &messageId,
					  const TelSmsAddress &sender, int csmsReferenceNumber,
					  int csmsSequenceNumber, int totalNumber,
					  const TelSmsProtocolDataUnit &pdu,
					  SceInt64 *outRowId);
	virtual TelephonyResult CountReceivedPdus(TelSmsTimeUnit timeUnit, int timeOffset,
						  const TelSmsAddress &sender,
						  int csmsReferenceNumber, const int *status,
						  int *outCount);
	virtual TelephonyResult GetPdu(const TelSmsMessageId &messageId,
				       const TelSmsAddress &sender, int csmsReferenceNumber,
				       int csmsSequenceNumber, int totalNumber,
				       TelSmsProtocolDataUnit *outPdu);
	virtual TelephonyResult DeletePdusByMessageId(const SceUInt64 *messageId);
	virtual TelephonyResult UpdateMessageBody(const TelSmsMessageId &messageId,
						  const char *utf8Body, SceSize length);
	virtual TelephonyResult PruneOldMessages(int keepCount);
	virtual TelephonyResult GetUnreadMessageCount(int *outCount);
	virtual TelephonyResult NotifyMessageArrived(const TelSmsMessage &message);

private:
	TelephonyResult UpdateFilterForPeers(const paf::vector<SceInt64> &peerIds,
					     const TelSmsMessageId &messageId,
					     const int *status, bool isNewMessage);
	TelephonyResult ResolveOrInsertPeer(const TelPhoneNumber &phoneNumber,
					    SceInt64 *outPeerId);
	TelephonyResult FindMessageIdsByFilter(const TelSmsFilter &filter,
					       paf::vector<TelSmsMessageId> *outIds);
	paf::string FormatTimeModifier(TelSmsTimeUnit timeUnit, int timeOffset) const;
	void ReadMessageList(dbutil::RefPtr<dbutil::Statement> *statement,
			     paf::vector<TelSmsMessage> *outMessages, bool appendOrder);

	dbutil::Connection *m_connection;
	paf::string m_dbPath;
};

/* Factory for TelSmsDbManager (0x813E8984..0x813E89E3) */
class TelSmsDbManagerFactory {
public:
	static TelephonyResult Create(int managerType, TelSmsDbManager **outManager,
				      const paf::string &dbPath);
	static TelephonyResult Destroy(TelSmsDbManager *manager);
};

} /* namespace telephony */

#endif /* TELEPHONY_SMS_DBMANAGER_H */
