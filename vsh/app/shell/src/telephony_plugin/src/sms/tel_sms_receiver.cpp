/*
 * tel_sms_receiver.cpp: reads incoming SMS PDUs from BBMC, parses them,
 * handles Class-0 Flash SMS directly, reassembles concatenated multi-part
 * SMS segments in SQLite, and deletes acknowledged PDUs from the modem
 * (3.65-CEX: 0x812533B4..0x81254877).
 */
#include <scetypes.h>
#include <common/string_util.h>
#include "../tel_internal.h"

namespace telephony {

static const int kSmsStatusIncomplete = 6;
static const int kSmsStatusReceived = 7;
static const int kSmsErrDbFull = -2146422267;	/* 0x80102A05 */
static const int kSmsConcatWindowMinutes = -10;

/* 0x812533B4 */
TelSmsReceiver::TelSmsReceiver(TelBbmcAdapterInterface *bbmcAdapter,
			       TelSmsDbManager *dbManager,
			       TelSmsPduParser *pduParser)
	: TelObservable("SceTelSmsReceiverIf"),
	  m_bbmcAdapter(bbmcAdapter),
	  m_dbManager(dbManager),
	  m_pduParser(pduParser),
	  m_maxStoredMessages(100)
{
}

/* 0x8125341C */
TelephonyResult TelSmsReceiver::Initialize()
{
	m_maxStoredMessages = 100;
	return TelephonyResult(TEL_OK);
}

/* 0x8125342A */
TelephonyResult TelSmsReceiver::ReassembleConcatenatedMessage(
	const TelSmsMessageId &messageId,
	const TelSmsAddress &sender,
	int *referenceNumber,
	unsigned int *totalSegments,
	int *receivedCount,
	bool *outCompleted,
	bool *outRetryNeeded)
{
	(void)receivedCount;
	TelephonyResult res(TEL_OK);
	*outCompleted = false;
	*outRetryNeeded = false;

	paf::wstring combinedBody;
	for (unsigned int seq = 1; seq <= *totalSegments; ++seq) {
		TelSmsProtocolDataUnit segPdu;
		TelSmsMessage segMsg;
		res = m_dbManager->GetPdu(messageId, sender, *referenceNumber,
					  (int)seq, (int)*totalSegments, &segPdu);
		if (!res.IsSuccess())
			continue;

		res = m_pduParser->Parse(segPdu, &segMsg);
		if (!res.IsSuccess()) {
			*outCompleted = false;
			*outRetryNeeded = false;
			break;
		}
		combinedBody.append(segMsg.GetBody());
	}

	paf::common::String converter(combinedBody);
	paf::string utf8Body = converter.GetString();

	res = m_dbManager->UpdateMessageBody(messageId, utf8Body.c_str(), utf8Body.size());
	if (!res.IsSuccess()) {
		*outCompleted = false;
		*outRetryNeeded = true;
		return res;
	}

	int receivedStatus = kSmsStatusReceived;
	res = m_dbManager->UpdateMessageStatus(messageId, &receivedStatus);
	if (res.GetCode() == kSmsErrDbFull) {
		*outCompleted = false;
		*outRetryNeeded = false;
	} else if (res.IsSuccess()) {
		*outCompleted = true;
		*outRetryNeeded = false;
	} else {
		*outCompleted = false;
		*outRetryNeeded = true;
	}
	return res;
}

/* 0x812536F6 */
TelephonyResult TelSmsReceiver::CheckPendingConcatenatedMessages(bool *outCompleted,
								 bool *outRetryNeeded)
{
	int incompleteStatus = kSmsStatusIncomplete;
	paf::vector<TelSmsMessageId> pendingIds;
	TelephonyResult res = m_dbManager->FindMessageIdsByStatus(&incompleteStatus, &pendingIds);
	if (!res.IsSuccess())
		return res;

	for (SceSize idx = 0; idx < pendingIds.size(); ++idx) {
		TelSmsAddress sender;
		int refNum = 0;
		int totalSegments = 0;
		int receivedCount = 0;
		res = m_dbManager->GetConcatenatedPduStatus(pendingIds[idx], &sender,
							    &refNum, &totalSegments,
							    &receivedCount);
		if (!res.IsSuccess()) {
			res.SetCode(TEL_OK);
			continue;
		}

		if (receivedCount == totalSegments && totalSegments > 0) {
			unsigned int segCount = (unsigned int)totalSegments;
			ReassembleConcatenatedMessage(pendingIds[idx], sender,
						      &refNum, &segCount, &receivedCount,
						      outCompleted, outRetryNeeded);
			if (*outCompleted) {
				m_dbManager->PruneOldMessages(m_maxStoredMessages);
				break;
			}
		}
	}

	return res;
}

/* 0x812538D2 */
TelephonyResult TelSmsReceiver::ReceiveNextSms(bool *outQueueEmpty,
					       bool *outMessageCompleted,
					       bool *outRetryNeeded)
{
	*outQueueEmpty = false;
	*outMessageCompleted = false;
	*outRetryNeeded = false;

	TelephonyResult res = CheckPendingConcatenatedMessages(outMessageCompleted,
							       outRetryNeeded);
	if (*outMessageCompleted)
		return res;

	TelSmsProtocolDataUnit rawPdu;
	SceUInt32 modemMemoryIndex = 0;
	res = m_bbmcAdapter->ReadSms(&rawPdu, &modemMemoryIndex);
	if (res.GetCode() == TEL_ERR_NOT_FOUND) {
		res.SetCode(TEL_OK);
		*outQueueEmpty = true;
		return res;
	}
	if (!res.IsSuccess()) {
		*outQueueEmpty = true;
		return res;
	}

	TelSmsMessage parsedMsg;
	res = m_pduParser->Parse(rawPdu, &parsedMsg);
	if (!res.IsSuccess()) {
		m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
		return res;
	}

	/* Class-0 Flash SMS (messageKind == 0): display immediately without storing in DB */
	if (parsedMsg.GetMessageKind() == 0) {
		m_dbManager->NotifyMessageArrived(parsedMsg);
		m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
		return res;
	}

	TelSmsMessageBuilder builder(&parsedMsg);
	builder.SetDirectionOutgoing(false);

	TelSmsPeerInfo peerCopy(parsedMsg.GetFirstPeer());
	TelSmsAddress senderAddr(peerCopy.GetAddress());

	bool isConcat = parsedMsg.IsConcatenated();
	int refNum = 0;
	int seqNum = 0;
	unsigned int totalSegments = 1;
	int existingCount = 0;
	bool shouldDeleteFromModem = true;
	bool existingFound = false;

	if (isConcat) {
		refNum = parsedMsg.GetReferenceNumber();
		seqNum = parsedMsg.GetMessageType();
		totalSegments = (unsigned int)parsedMsg.GetMessageKind();
		int incompleteStatus = kSmsStatusIncomplete;
		res = m_dbManager->CountReceivedPdus(TEL_SMS_TIME_MINUTES,
						     kSmsConcatWindowMinutes,
						     senderAddr, refNum,
						     &incompleteStatus, &existingCount);
		if (!res.IsSuccess()) {
			existingCount = 0;
		} else if (existingCount != 0) {
			res = m_dbManager->FindConcatenatedMessage(TEL_SMS_TIME_MINUTES,
								   kSmsConcatWindowMinutes,
								   senderAddr, refNum,
								   &incompleteStatus,
								   &parsedMsg);
			if (res.GetCode() == TEL_ERR_NOT_FOUND) {
				shouldDeleteFromModem = false;
				existingFound = true;
			} else if (!res.IsSuccess()) {
				*outRetryNeeded = true;
				return res;
			} else {
				existingFound = true;
			}
		}
	}

	if (!existingFound) {
		TelSmsPeerInfo incompletePeer(parsedMsg.GetFirstPeer());
		incompletePeer.SetStatus(kSmsStatusIncomplete);
		parsedMsg.SetSinglePeer(incompletePeer);

		res = m_dbManager->InsertMessage(&parsedMsg);
		if (res.GetCode() == kSmsErrDbFull) {
			return res;
		}
		if (!res.IsSuccess()) {
			*outRetryNeeded = true;
			return res;
		}
	}

	TelSmsMessageId msgId(parsedMsg.GetMessageId());
	SceInt64 pduRowId = 0;
	res = m_dbManager->InsertPdu(msgId, senderAddr, refNum, seqNum,
				     (int)totalSegments, rawPdu, &pduRowId);
	if (!res.IsSuccess()) {
		*outRetryNeeded = true;
		return res;
	}

	if (!isConcat) {
		/* Single-part SMS: read back PDU, parse body, store UTF-8 text, mark received */
		TelSmsProtocolDataUnit storedPdu;
		TelSmsMessage decodedMsg;
		res = m_dbManager->GetPdu(msgId, senderAddr, 0, 0, 1, &storedPdu);
		if (!res.IsSuccess()) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			*outRetryNeeded = true;
			if (!delRes.IsSuccess())
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return delRes;
		}

		res = m_pduParser->Parse(storedPdu, &decodedMsg);
		if (!res.IsSuccess()) {
			m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return res;
		}

		paf::common::String converter(decodedMsg.GetBody());
		paf::string utf8Body = converter.GetString();
		res = m_dbManager->UpdateMessageBody(msgId, utf8Body.c_str(), utf8Body.size());
		if (!res.IsSuccess()) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			*outRetryNeeded = true;
			if (!delRes.IsSuccess())
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return delRes;
		}

		int receivedStatus = kSmsStatusReceived;
		res = m_dbManager->UpdateMessageStatus(msgId, &receivedStatus);
		if (res.GetCode() == kSmsErrDbFull) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			if (!delRes.IsSuccess()) {
				*outRetryNeeded = true;
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			}
			return delRes;
		}
		if (!res.IsSuccess()) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			*outRetryNeeded = true;
			if (!delRes.IsSuccess())
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return delRes;
		}

		m_dbManager->PruneOldMessages(m_maxStoredMessages);
		*outMessageCompleted = true;
		m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
		return res;
	}

	/* Concatenated SMS: check if all segments have now arrived */
	int incompleteStatus = kSmsStatusIncomplete;
	res = m_dbManager->CountReceivedPdus(TEL_SMS_TIME_MINUTES, kSmsConcatWindowMinutes,
					     senderAddr, refNum, &incompleteStatus,
					     &existingCount);
	if (!res.IsSuccess()) {
		*outRetryNeeded = true;
		return res;
	}

	if ((unsigned int)existingCount == totalSegments) {
		paf::wstring combinedBody;
		for (unsigned int seg = 1; seg <= totalSegments; ++seg) {
			TelSmsProtocolDataUnit segPdu;
			TelSmsMessage segMsg;
			res = m_dbManager->GetPdu(msgId, senderAddr, refNum,
						  (int)seg, (int)totalSegments, &segPdu);
			if (!res.IsSuccess())
				continue;

			res = m_pduParser->Parse(segPdu, &segMsg);
			if (!res.IsSuccess())
				break;

			combinedBody.append(segMsg.GetBody());
		}

		paf::common::String converter(combinedBody);
		paf::string utf8Body = converter.GetString();
		res = m_dbManager->UpdateMessageBody(msgId, utf8Body.c_str(), utf8Body.size());
		if (!res.IsSuccess()) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			*outRetryNeeded = true;
			if (!delRes.IsSuccess())
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return delRes;
		}

		int receivedStatus = kSmsStatusReceived;
		res = m_dbManager->UpdateMessageStatus(msgId, &receivedStatus);
		if (res.GetCode() == kSmsErrDbFull) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			if (!delRes.IsSuccess()) {
				*outRetryNeeded = true;
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			}
			return delRes;
		}
		if (!res.IsSuccess()) {
			SceUInt64 rowKey = (SceUInt64)pduRowId;
			TelephonyResult delRes = m_dbManager->DeletePdusByMessageId(&rowKey);
			*outRetryNeeded = true;
			if (!delRes.IsSuccess())
				m_bbmcAdapter->DeleteSms(&modemMemoryIndex);
			return delRes;
		}

		m_dbManager->PruneOldMessages(m_maxStoredMessages);
		*outMessageCompleted = true;
	}

	if (shouldDeleteFromModem)
		res = m_bbmcAdapter->DeleteSms(&modemMemoryIndex);

	return res;
}

} /* namespace telephony */
