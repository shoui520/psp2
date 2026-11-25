/*
 * tel_bbmc_sms_handler.cpp: TelBbmcSmsHandler
 * (3.65-CEX: 0x813D5ADC..0x813D64C7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

namespace {

enum TelSmsLogTag {
	SMS_LOG_LST = 0,
	SMS_LOG_LSTD = 1,
	SMS_LOG_RD = 2,
	SMS_LOG_RDD = 3,
	SMS_LOG_ACK = 4,
	SMS_LOG_DEL = 5
};

enum TelSmsTaskBit {
	SMS_TASK_SEND = 0x01,
	SMS_TASK_LIST = 0x02,
	SMS_TASK_READ = 0x04,
	SMS_TASK_ACK = 0x08,
	SMS_TASK_DELETE = 0x10
};

} /* namespace */

/* 0x813D5ADC */
TelBbmcSmsHandler::TelBbmcSmsHandler(TelBbmcSharedData *sharedData)
	: TelBbmcHandler(sharedData),
	  m_stateMutex("SceTelBbmcSmsHandler", 0),
	  m_sendLength(0),
	  m_readLength(0),
	  m_listCount(0),
	  m_memoryIndex(0)
{
	sce_paf_memset(m_sendBuffer, 0, sizeof(m_sendBuffer));
	sce_paf_memset(m_readBuffer, 0, sizeof(m_readBuffer));
	SetRetryDelayMs(100);
	SetMaxRetries(20);
	Register(TEL_BBMC_LOG_CH_SMS);
}

/* 0x813D5B60 */
TelBbmcSmsHandler::~TelBbmcSmsHandler()
{
	Unregister();
}

bool TelBbmcSmsHandler::CanProcessTimerRetry()
{
	TelBbmcSharedStatus *status = TelBbmcSharedStatus::GetInstance();
	return status == NULL || !status->GetFlag(TEL_BBMC_FLAG_SUSPENDING);
}

SceUInt32 TelBbmcSmsHandler::GetFeatureId()
{
	return TEL_BBMC_FEATURE_SMS;
}

/* 0x813D61F0 */
SceUInt32 TelBbmcSmsHandler::GetEventMask()
{
	return 0x1FFU;
}

SceUInt32 TelBbmcSmsHandler::GetSubscriberEvfBit()
{
	return 1U << GetFeatureId();
}

/* 0x813D5C2A */
TelephonyResult TelBbmcSmsHandler::SendSms(const TelSmsProtocolDataUnit &pdu)
{
	TelephonyResult res(TEL_OK);
	ResetRetryCount(-1);

	do {
		m_sync.Lock();
		m_sync.IncrementActiveCount();
		m_sendLength = pdu.GetLength();
		sce_paf_memset(m_sendBuffer, 0, sizeof(m_sendBuffer));
		pdu.GetBytes(m_sendBuffer, sizeof(m_sendBuffer));
		res.SetCode(TEL_OK);
		BeginSyncWait(0x22U);

		if (PostTask(SMS_TASK_SEND) >= 0) {
			SceUInt32 lastEvent = 0;
			int status = 0;
			SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
			if ((matched & 0x20U) != 0)
				res.SetCode(TEL_ERR_SMS_SEND);
		} else {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

/* 0x813D5D1E */
TelephonyResult TelBbmcSmsHandler::AckSms(bool deleteMessage)
{
	TelephonyResult res(TEL_OK);
	ResetRetryCount(-1);

	do {
		m_sync.Lock();
		m_sync.IncrementActiveCount();
		res.SetCode(TEL_OK);
		BeginSyncWait(deleteMessage ? 0x110U : 0x40000000U);

		int postRet = PostTask(deleteMessage ? SMS_TASK_DELETE : SMS_TASK_ACK);
		if (postRet >= 0) {
			SceUInt32 lastEvent = 0;
			int status = 0;
			SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
			if (deleteMessage) {
				if ((matched & 0x100U) != 0)
					res.SetCode(TEL_ERR_SMS_DELETE);
			} else {
				if ((matched & 0x40000000U) != 0)
					res.SetCode(TEL_ERR_SMS_ACK);
			}
		} else {
			res.SetCode(TEL_ERR_BBMC_DRIVER);
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

/* 0x813D5E14 */
TelephonyResult TelBbmcSmsHandler::ReadSmsAtIndex()
{
	TelephonyResult res(TEL_OK);
	bool readStarted = false;
	ResetRetryCount(-1);

	do {
		m_sync.Lock();
		m_sync.IncrementActiveCount();
		res.SetCode(TEL_OK);

		if (!readStarted) {
			BeginSyncWait(0x88U);
			if (PostTask(SMS_TASK_READ) < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			} else {
				readStarted = true;
				SceUInt32 lastEvent = 0;
				int status = 0;
				SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
				if ((matched & 0x80U) != 0) {
					readStarted = (status >= 0);
					res.SetCode(TEL_ERR_SMS_READ);
				}
			}
		}

		if (readStarted) {
			m_stateMutex.Lock();
			sce_paf_memset(m_readBuffer, 0, sizeof(m_readBuffer));
			SceUInt32 format = 0;
			int ret = sceBbmcSMSReadDone(m_subscriberId, m_readBuffer, &format);
			m_readLength = ret;
			LogResult(SMS_LOG_RDD, ret);
			if (ret < 0)
				res.SetCode(TEL_ERR_SMS_READ);
			m_stateMutex.Unlock();
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

/* 0x813D5F56 */
TelephonyResult TelBbmcSmsHandler::FetchSmsList()
{
	TelephonyResult res(TEL_OK);
	bool listStarted = false;
	ResetRetryCount(-1);

	do {
		m_sync.Lock();
		m_sync.IncrementActiveCount();
		res.SetCode(TEL_OK);

		if (!listStarted) {
			BeginSyncWait(0x44U);
			if (PostTask(SMS_TASK_LIST) < 0) {
				res.SetCode(TEL_ERR_BBMC_DRIVER);
			} else {
				listStarted = true;
				SceUInt32 lastEvent = 0;
				int status = 0;
				SceUInt32 matched = WaitSync(&lastEvent, &status, 30000000);
				if ((matched & 0x40U) != 0) {
					listStarted = (status >= 0);
					res.SetCode(TEL_ERR_SMS_LIST);
				}
			}
		}

		if (listStarted) {
			m_stateMutex.Lock();
			int ret = sceBbmcSMSGetListDone(m_subscriberId, &m_listCount,
							&m_memoryIndex, 1);
			LogResult(SMS_LOG_LSTD, ret);
			if (ret < 0)
				res.SetCode(TEL_ERR_SMS_LIST);
			m_stateMutex.Unlock();
		}

		m_sync.DecrementActiveCount();
		m_sync.Unlock();
	} while (ConsumeRetryAndDelay(res));

	return res;
}

/* 0x813D6090 */
TelephonyResult TelBbmcSmsHandler::ReadNextSms(TelSmsProtocolDataUnit *outPdu,
					       SceUInt32 *outMemoryIndex)
{
	TelephonyResult res = FetchSmsList();
	if (!res.IsSuccess())
		return res;

	if (m_listCount == 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	*outMemoryIndex = m_memoryIndex;
	res = ReadSmsAtIndex();
	if (res.IsSuccess()) {
		outPdu->SetBytes(m_readBuffer, (SceSize)m_readLength);
		res = AckSms(false);
	}
	return res;
}

/* 0x813D614E */
TelephonyResult TelBbmcSmsHandler::DeleteSms(const SceUInt32 *memoryIndex)
{
	TelephonyResult res = FetchSmsList();
	if (!res.IsSuccess())
		return res;

	if (m_listCount == 0)
		return TelephonyResult(TEL_ERR_NOT_FOUND);

	m_memoryIndex = *memoryIndex;
	res = ReadSmsAtIndex();
	if (res.IsSuccess())
		res = AckSms(true);
	return res;
}

/* 0x813D61F6 */
void TelBbmcSmsHandler::ProcessTasks(SceUInt32 taskBits)
{
	if ((taskBits & SMS_TASK_SEND) != 0) {
		int ret = sceBbmcSMSSend(m_subscriberId, m_sendBuffer, m_sendLength, 6);
		if (ret < 0)
			SignalSyncError(0x20U, ret);
	}
	if ((taskBits & SMS_TASK_LIST) != 0) {
		int ret = sceBbmcSMSGetListStart(m_subscriberId);
		LogResult(SMS_LOG_LST, ret);
		if (ret < 0)
			SignalSyncError(0x40U, ret);
	}
	if ((taskBits & SMS_TASK_READ) != 0) {
		int ret = sceBbmcSMSReadStart(m_subscriberId, m_memoryIndex);
		LogResult(SMS_LOG_RD, ret);
		if (ret < 0)
			SignalSyncError(0x80U, ret);
	}
	if ((taskBits & SMS_TASK_ACK) != 0) {
		int ret = sceBbmcSMSReadAck(m_subscriberId, m_memoryIndex, 0);
		LogResult(SMS_LOG_ACK, ret);
		SignalSyncError(0x40000000U, ret);
	}
	if ((taskBits & SMS_TASK_DELETE) != 0) {
		int ret = sceBbmcSMSReadAck(m_subscriberId, m_memoryIndex, 1);
		LogResult(SMS_LOG_DEL, ret);
		if (ret < 0)
			SignalSyncError(0x100U, ret);
	}
}

void TelBbmcSmsHandler::OnBroadcastFlags(SceUInt32 flags)
{
	(void)flags;
}

/* 0x813D6332 */
int TelBbmcSmsHandler::HandleCallbackEvent(int notifyCount, SceUInt32 eventBits,
					   SceUInt32 *outTaskBits)
{
	(void)notifyCount;
	(void)outTaskBits;
	SceUInt32 handled = 0;
	LogEventMask(eventBits);

	if ((eventBits & 0x001U) != 0) {
		handled |= 0x001U;
		TelBbmcSharedStatus *sharedStatus = TelBbmcSharedStatus::GetInstance();
		if (sharedStatus == NULL || !sharedStatus->LatchSmsDuringSuspend())
			PushNotify(TEL_BBMC_EVENT_SMS_RECEIVED, 0);
	}
	if ((eventBits & 0x002U) != 0)
		handled |= 0x002U;
	if ((eventBits & 0x004U) != 0)
		handled |= 0x004U;
	if ((eventBits & 0x008U) != 0)
		handled |= 0x008U;
	if ((eventBits & 0x020U) != 0)
		handled |= 0x020U;
	if ((eventBits & 0x010U) != 0)
		handled |= 0x010U;
	if ((eventBits & 0x040U) != 0)
		handled |= 0x040U;
	if ((eventBits & 0x080U) != 0)
		handled |= 0x080U;
	if ((eventBits & 0x100U) != 0)
		handled |= 0x100U;

	if (handled != eventBits)
		handled = eventBits;
	ClearAndSignalEvent(handled);
	return 0;
}

/* 0x813D640A */
bool TelBbmcSmsHandler::ShouldRetryResult(const TelephonyResult &res)
{
	switch (res.GetCode()) {
	case TEL_ERR_SMS_SEND:
	case TEL_ERR_SMS_LIST:
	case TEL_ERR_SMS_READ:
	case TEL_ERR_SMS_DELETE:
	case TEL_ERR_SMS_ACK:
		return true;
	default:
		return TelBbmcHandler::ShouldRetryResult(res);
	}
}

/* 0x813D643E */
const char *TelBbmcSmsHandler::GetLogTagName(int tagId) const
{
	switch (tagId) {
	case SMS_LOG_LST:
		return "Lst";
	case SMS_LOG_LSTD:
		return "LstD";
	case SMS_LOG_RD:
		return "Rd";
	case SMS_LOG_RDD:
		return "RdD";
	case SMS_LOG_ACK:
		return "Ack";
	case SMS_LOG_DEL:
		return "Del";
	default:
		return "";
	}
}

} /* namespace telephony */
