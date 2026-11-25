/*
 * tel_bbmc_job_items.cpp: TelBbmcSharedStatus, TelBbmcNotifyQueue,
 * TelBbmcLogBuffer, TelBbmcLogManager, and TelBbmcLogClient
 * (3.65-CEX: 0x813D3408..0x813D355B, 0x813DAC1E..0x813DBD13).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <rtc/rtc.h>
#include "../../tel_bbmc_adapter.h"

namespace telephony {

namespace {

static TelBbmcSharedStatus *g_sharedStatus = NULL;
static int g_sharedStatusRefCount = 0;
static TelBbmcLogManager *g_logManager = NULL;

static const char kChannelPrefixes[] = "-GSPM...";

enum TelBbmcLogRecordKind {
	LOG_KIND_RESULT_OK = 0,
	LOG_KIND_RESULT_NONE = 1,
	LOG_KIND_RESULT_ERR = 2,
	LOG_KIND_U16 = 3,
	LOG_KIND_U32 = 4,
	LOG_KIND_EVENT_BIT = 5,
	LOG_KIND_EVENT_HI_ONLY = 6,
	LOG_KIND_EVENT_LO_BYTE = 7,
	LOG_KIND_EVENT_HI_PLUS = 8,
	LOG_KIND_EVENT_LOW24 = 9,
	LOG_KIND_STRING = 10,
	LOG_KIND_TIMESTAMP = 11
};

} /* namespace */

/* 0x813DBCDC */
int telCountSetBits(SceUInt32 mask)
{
	SceUInt32 pairs = (mask & 0x55555555U) + ((mask >> 1) & 0x55555555U);
	SceUInt32 nibbles = (pairs & 0x33333333U) + ((pairs >> 2) & 0x33333333U);
	SceUInt32 bytes = (nibbles & 0x0F0F0F0FU) + ((nibbles >> 4) & 0x070F0F0FU);
	SceUInt32 halves = (bytes & 0x00FF00FFU) + ((bytes >> 8) & 0x001F00FFU);
	return (int)((halves & 0xFFFFU) + (halves >> 16));
}

/* TelBbmcNotifyQueue */
TelBbmcNotifyQueue::TelBbmcNotifyQueue(const char *name, SceSize maxCount)
	: m_maxCount(maxCount),
	  m_active(false),
	  m_mutex(name, 0),
	  m_notFullCond(m_mutex, "TelNotifyNotFull", 0),
	  m_notEmptyCond(m_mutex, "TelNotifyNotEmpty", 0)
{
}

TelBbmcNotifyQueue::~TelBbmcNotifyQueue()
{
	Deactivate();
}

void TelBbmcNotifyQueue::Activate()
{
	m_mutex.Lock();
	m_active = true;
	m_mutex.Unlock();
}

void TelBbmcNotifyQueue::Deactivate()
{
	m_mutex.Lock();
	m_active = false;
	m_items.clear();
	m_notFullCond.NotifyAll();
	m_notEmptyCond.NotifyAll();
	m_mutex.Unlock();
}

TelephonyResult TelBbmcNotifyQueue::Push(const TelBbmcNotifyItem &item)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	if (!m_active || m_items.size() >= m_maxCount) {
		res.SetCode(TEL_ERR_FAILED);
	} else {
		m_items.push_back(item);
		m_notEmptyCond.NotifyAll();
	}
	m_mutex.Unlock();
	return res;
}

bool TelBbmcNotifyQueue::Pop(TelBbmcNotifyItem *outItem)
{
	m_mutex.Lock();
	if (m_items.empty()) {
		m_mutex.Unlock();
		return false;
	}
	*outItem = m_items[0];
	m_items.erase(m_items.begin());
	m_notFullCond.NotifyAll();
	m_mutex.Unlock();
	return true;
}

/* TelBbmcSharedStatus (0x813D3408..0x813D355B) */
TelBbmcSharedStatus::TelBbmcSharedStatus()
	: m_mutex("SceTelBbmcSharedData", 0),
	  m_powerStatus(TEL_BBMC_POWER_OFF)
{
	m_flags[TEL_BBMC_FLAG_SUSPENDING] = false;
	m_flags[TEL_BBMC_FLAG_SMS_PENDING_SUSPEND] = false;
	m_flags[TEL_BBMC_FLAG_PACKET_ACTIVE] = false;
}

TelBbmcSharedStatus::~TelBbmcSharedStatus()
{
}

TelBbmcSharedStatus *TelBbmcSharedStatus::Acquire()
{
	if (g_sharedStatusRefCount == 0 && g_sharedStatus == NULL)
		g_sharedStatus = new TelBbmcSharedStatus();
	++g_sharedStatusRefCount;
	return g_sharedStatus;
}

void TelBbmcSharedStatus::Release()
{
	if (g_sharedStatusRefCount > 0)
		--g_sharedStatusRefCount;
	if (g_sharedStatusRefCount == 0 && g_sharedStatus != NULL) {
		delete g_sharedStatus;
		g_sharedStatus = NULL;
	}
}

TelBbmcSharedStatus *TelBbmcSharedStatus::GetInstance()
{
	return g_sharedStatus;
}

/* 0x813D3486 */
TelephonyResult TelBbmcSharedStatus::CheckReady() const
{
	TelephonyResult res(TEL_OK);
	if (m_powerStatus == TEL_BBMC_POWER_OFF)
		res.SetCode(TEL_ERR_MODEM_OFF);
	else if (m_powerStatus == TEL_BBMC_POWER_STARTING)
		res.SetCode(TEL_ERR_MODEM_BUSY);
	else if (m_powerStatus == TEL_BBMC_POWER_FAILURE)
		res.SetCode(TEL_ERR_MODEM_FATAL);
	return res;
}

/* 0x813D34D6 */
void TelBbmcSharedStatus::SetPowerStatus(int status)
{
	m_mutex.Lock();
	m_powerStatus = status;
	m_mutex.Unlock();
}

/* 0x813D34F0 */
bool TelBbmcSharedStatus::LatchSmsDuringSuspend()
{
	m_mutex.Lock();
	bool pending = m_flags[TEL_BBMC_FLAG_SMS_PENDING_SUSPEND];
	if (m_flags[TEL_BBMC_FLAG_SUSPENDING]) {
		pending = true;
		m_flags[TEL_BBMC_FLAG_SMS_PENDING_SUSPEND] = true;
	}
	m_mutex.Unlock();
	return pending;
}

/* 0x813D3518 */
void TelBbmcSharedStatus::SetFlag(int flagIndex, bool value)
{
	m_mutex.Lock();
	m_flags[flagIndex] = value;
	m_mutex.Unlock();
}

/* 0x813D353C */
bool TelBbmcSharedStatus::GetFlag(int flagIndex)
{
	m_mutex.Lock();
	bool value = m_flags[flagIndex];
	m_mutex.Unlock();
	return value;
}

/* TelBbmcLogBuffer (0x813DAC4C..0x813DAD03) */
TelBbmcLogBuffer::TelBbmcLogBuffer()
	: m_frozen(false),
	  m_committedSize(0),
	  m_writePos(0),
	  m_savedWritePos(0),
	  m_readPos(0),
	  m_savedReadPos(0),
	  m_buffer(NULL),
	  m_capacity(0)
{
}

TelBbmcLogBuffer::~TelBbmcLogBuffer()
{
	if (m_buffer != NULL) {
		delete[] m_buffer;
		m_buffer = NULL;
	}
}

TelephonyResult TelBbmcLogBuffer::Allocate(SceSize capacity)
{
	m_capacity = capacity;
	m_buffer = new SceUInt8[capacity];
	if (m_buffer == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);
	return TelephonyResult(TEL_OK);
}

void TelBbmcLogBuffer::Reset()
{
	m_frozen = false;
	m_committedSize = 0;
	m_writePos = 0;
	m_readPos = 0;
}

void TelBbmcLogBuffer::RewindRead()
{
	m_readPos = 0;
}

TelephonyResult TelBbmcLogBuffer::WriteBytes(const void *src, SceSize size)
{
	if (m_frozen)
		return TelephonyResult(TEL_ERR_MODEM_BUSY);
	if (size > m_capacity - m_writePos)
		return TelephonyResult(TEL_ERR_BUFFER_TOO_SMALL);
	if (size > 0) {
		sce_paf_memcpy(m_buffer + m_writePos, src, size);
		m_writePos += size;
	}
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogBuffer::AdvanceRead(SceSize amount)
{
	if (m_writePos - m_readPos < amount)
		return TelephonyResult(TEL_ERR_BUFFER_TOO_SMALL);
	m_readPos += amount;
	return TelephonyResult(TEL_OK);
}

/* TelBbmcLogManager (0x813DAC90..0x813DB9FB) */
TelBbmcLogManager::TelBbmcLogManager()
	: m_mutex("SceTelBbmcLogManager", 0),
	  m_logBuffer(NULL)
{
	for (int ch = 0; ch < 5; ++ch)
		m_clients[ch] = NULL;
	sce_paf_memset(m_lineBuf, 0, sizeof(m_lineBuf));
}

TelBbmcLogManager::~TelBbmcLogManager()
{
	if (m_logBuffer != NULL) {
		delete m_logBuffer;
		m_logBuffer = NULL;
	}
}

/* 0x813DAC90 */
TelephonyResult TelBbmcLogManager::InitBuffer()
{
	for (int ch = 0; ch < 5; ++ch)
		m_clients[ch] = NULL;
	m_logBuffer = new TelBbmcLogBuffer();
	if (m_logBuffer == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);
	return m_logBuffer->Allocate(1024);
}

/* 0x813DAD04 */
TelephonyResult TelBbmcLogManager::Initialize()
{
	if (g_logManager != NULL)
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	g_logManager = new TelBbmcLogManager();
	if (g_logManager == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);
	return g_logManager->InitBuffer();
}

/* 0x813DADB4 */
TelephonyResult TelBbmcLogManager::Terminate()
{
	if (g_logManager != NULL) {
		delete g_logManager;
		g_logManager = NULL;
	}
	return TelephonyResult(TEL_OK);
}

/* 0x813DAE2C */
TelBbmcLogManager *TelBbmcLogManager::GetInstance()
{
	return g_logManager;
}

void TelBbmcLogManager::RegisterClient(int channelId, TelBbmcLogClient *client)
{
	if (channelId >= 0 && channelId < 5)
		m_clients[channelId] = client;
}

void TelBbmcLogManager::UnregisterClient(int channelId)
{
	if (channelId >= 0 && channelId < 5)
		m_clients[channelId] = NULL;
}

/* 0x813DAE3A */
void TelBbmcLogManager::RewindRead()
{
	m_mutex.Lock();
	if (m_logBuffer != NULL)
		m_logBuffer->RewindRead();
	m_mutex.Unlock();
}

/* 0x813DB352 */
void TelBbmcLogManager::ClearLog()
{
	m_mutex.Lock();
	if (m_logBuffer != NULL)
		m_logBuffer->Reset();
	m_mutex.Unlock();
}

/* 0x813DAE5A */
TelephonyResult TelBbmcLogManager::FormatRecord(const SceUInt8 *record, SceSize *outConsumed,
						SceSize *outTextLen)
{
	SceUInt8 header = record[0];
	int channel = (header >> 4) & 7;
	int kind = header & 0x0F;
	const SceUInt8 *cursor = record + 1;

	*outConsumed = 1;
	m_lineBuf[0] = kChannelPrefixes[channel];
	m_lineBuf[1] = ':';

	char *out = m_lineBuf + 2;
	if (kind <= LOG_KIND_U32) {
		*out++ = 'A';
		if (kind == LOG_KIND_U16)
			*out++ = '2';
		else if (kind == LOG_KIND_U32)
			*out++ = '4';
		*out++ = ':';

		int tagId = cursor[0];
		cursor++;
		*outConsumed += 1;

		const char *tagName = "";
		if (channel < 5 && m_clients[channel] != NULL)
			tagName = m_clients[channel]->GetLogTagName(tagId);
		SceSize tagLen = sce_paf_strlen(tagName);
		if (tagLen > 16)
			tagLen = 16;
		sce_paf_memcpy(out, tagName, tagLen);
		out += tagLen;
	} else if (kind >= LOG_KIND_EVENT_BIT && kind <= LOG_KIND_EVENT_LOW24) {
		*out++ = 'E';
	} else if (kind == LOG_KIND_STRING) {
		*out++ = 'S';
	} else if (kind == LOG_KIND_TIMESTAMP) {
		*out++ = 'T';
	} else {
		*out++ = '?';
	}

	*out++ = ':';
	SceSize consumed = (SceSize)(cursor - record);

	switch (kind) {
	case LOG_KIND_RESULT_OK:
		*out++ = '0';
		break;
	case LOG_KIND_RESULT_NONE:
		out--;
		break;
	case LOG_KIND_RESULT_ERR:
	case LOG_KIND_U32: {
		SceUInt32 val = ((SceUInt32)cursor[0] << 24) | ((SceUInt32)cursor[1] << 16) |
				((SceUInt32)cursor[2] << 8) | (SceUInt32)cursor[3];
		sce_paf_snprintf(out, 9, "%08x", val);
		out += 8;
		consumed += 4;
		break;
	}
	case LOG_KIND_U16: {
		SceUInt16 val = (SceUInt16)(((SceUInt16)cursor[0] << 8) | cursor[1]);
		sce_paf_snprintf(out, 5, "%04x", (int)(SceInt16)val);
		out += 4;
		consumed += 2;
		break;
	}
	case LOG_KIND_EVENT_BIT:
		sce_paf_snprintf(out, 9, "%08x", 1U << (cursor[0] & 31));
		out += 8;
		consumed += 1;
		break;
	case LOG_KIND_EVENT_HI_ONLY:
	case LOG_KIND_EVENT_HI_PLUS:
		sce_paf_snprintf(out, 9, "%08x", (SceUInt32)cursor[0] << 24);
		out += 8;
		if (kind == LOG_KIND_EVENT_HI_PLUS)
			*out++ = '+';
		consumed += 1;
		break;
	case LOG_KIND_EVENT_LO_BYTE:
		sce_paf_snprintf(out, 9, "%08x", (SceUInt32)cursor[0]);
		out += 8;
		consumed += 1;
		break;
	case LOG_KIND_EVENT_LOW24: {
		SceUInt32 val = ((SceUInt32)cursor[0] << 16) | ((SceUInt32)cursor[1] << 8) |
				(SceUInt32)cursor[2];
		sce_paf_snprintf(out, 9, "%08x", val);
		out += 8;
		consumed += 3;
		break;
	}
	case LOG_KIND_STRING: {
		SceSize strLen = sce_paf_strlen((const char *)cursor);
		SceSize paddedLen = strLen + 1 + ((strLen & 1) != 0 ? 1 : 0);
		SceSize copyLen = strLen < 32 ? strLen : 32;
		sce_paf_snprintf(out, copyLen + 1, "%s", (const char *)cursor);
		out += copyLen;
		consumed += paddedLen;
		break;
	}
	case LOG_KIND_TIMESTAMP: {
		SceUInt32 totalSec = ((SceUInt32)cursor[0] << 16) |
				     ((SceUInt32)cursor[1] << 8) | (SceUInt32)cursor[2];
		sce_paf_snprintf(out, 9, "%02d%02d%02d%02d",
				 (int)(totalSec / 86400U + 1U),
				 (int)((totalSec % 86400U) / 3600U),
				 (int)((totalSec % 3600U) / 60U),
				 (int)(totalSec % 60U));
		out += 8;
		consumed += 3;
		break;
	}
	default:
		break;
	}

	*out++ = '\n';
	*out = '\0';
	*outConsumed = consumed;
	*outTextLen = sce_paf_strlen(m_lineBuf);
	return TelephonyResult(TEL_OK);
}

/* 0x813DB22C */
int TelBbmcLogManager::ReadFormatted(char *outBuffer, SceSize maxLen)
{
	TelephonyResult res(TEL_OK);
	m_mutex.Lock();
	sce_paf_memset(outBuffer, 0, maxLen);

	int totalWritten = 0;
	while (res.IsSuccess() && m_logBuffer != NULL && m_logBuffer->HasUnread()) {
		SceSize consumed = 0;
		SceSize textLen = 0;
		res = FormatRecord(m_logBuffer->GetReadPointer(), &consumed, &textLen);
		if (!res.IsSuccess())
			break;

		m_logBuffer->SaveReadPos();
		res = m_logBuffer->AdvanceRead(consumed);
		if (res.IsSuccess() && maxLen >= textLen) {
			sce_paf_memcpy(outBuffer, m_lineBuf, textLen);
			outBuffer += textLen;
			totalWritten += (int)textLen;
			maxLen -= textLen;
		} else {
			m_logBuffer->RollbackReadPos();
			break;
		}
	}

	m_mutex.Unlock();
	return totalWritten;
}

/* 0x813DB376 */
TelephonyResult TelBbmcLogManager::WriteTaggedRecord(SceUInt8 headerByte, SceUInt8 tagId,
						     const void *payload, SceSize payloadSize)
{
	if (m_logBuffer == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	if (m_logBuffer->IsFrozen())
		return TelephonyResult(TEL_ERR_MODEM_BUSY);

	m_logBuffer->SaveWritePos();
	TelephonyResult res = m_logBuffer->WriteBytes(&headerByte, 1);
	if (res.IsSuccess())
		res = m_logBuffer->WriteBytes(&tagId, 1);
	if (res.IsSuccess() && payloadSize > 0)
		res = m_logBuffer->WriteBytes(payload, payloadSize);

	if (!res.IsSuccess())
		m_logBuffer->RollbackAndFreeze();
	else
		m_logBuffer->CommitWrite();
	return res;
}

/* 0x813DB5E6 */
TelephonyResult TelBbmcLogManager::WriteRawRecord(SceUInt8 headerByte, const void *payload,
						  SceSize payloadSize, bool alignEven)
{
	if (m_logBuffer == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);
	if (m_logBuffer->IsFrozen())
		return TelephonyResult(TEL_ERR_MODEM_BUSY);

	m_logBuffer->SaveWritePos();
	TelephonyResult res = m_logBuffer->WriteBytes(&headerByte, 1);
	if (res.IsSuccess() && payloadSize > 0)
		res = m_logBuffer->WriteBytes(payload, payloadSize);
	if (res.IsSuccess() && alignEven && (payloadSize & 1) == 0) {
		SceUInt8 zero = 0;
		res = m_logBuffer->WriteBytes(&zero, 1);
	}

	if (!res.IsSuccess())
		m_logBuffer->RollbackAndFreeze();
	else
		m_logBuffer->CommitWrite();
	return res;
}

/* 0x813DB52C */
TelephonyResult TelBbmcLogManager::LogResult(int channelId, SceUInt8 tagId, int resultCode)
{
	m_mutex.Lock();
	SceUInt8 chBits = (SceUInt8)((channelId & 7) << 4);
	TelephonyResult res(TEL_OK);
	if (resultCode != 0) {
		SceUInt8 beBytes[4];
		beBytes[0] = (SceUInt8)((resultCode >> 24) & 0xFF);
		beBytes[1] = (SceUInt8)((resultCode >> 16) & 0xFF);
		beBytes[2] = (SceUInt8)((resultCode >> 8) & 0xFF);
		beBytes[3] = (SceUInt8)(resultCode & 0xFF);
		res = WriteTaggedRecord((SceUInt8)(chBits | LOG_KIND_RESULT_ERR), tagId,
					beBytes, 4);
	} else {
		res = WriteTaggedRecord((SceUInt8)(chBits | LOG_KIND_RESULT_OK), tagId, NULL, 0);
	}
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcLogManager::LogU16(int channelId, SceUInt8 tagId, SceUInt16 value)
{
	m_mutex.Lock();
	SceUInt8 beBytes[2];
	beBytes[0] = (SceUInt8)((value >> 8) & 0xFF);
	beBytes[1] = (SceUInt8)(value & 0xFF);
	SceUInt8 header = (SceUInt8)(((channelId & 7) << 4) | LOG_KIND_U16);
	TelephonyResult res = WriteTaggedRecord(header, tagId, beBytes, 2);
	m_mutex.Unlock();
	return res;
}

TelephonyResult TelBbmcLogManager::LogU32(int channelId, SceUInt8 tagId, SceUInt32 value)
{
	m_mutex.Lock();
	SceUInt8 beBytes[4];
	beBytes[0] = (SceUInt8)((value >> 24) & 0xFF);
	beBytes[1] = (SceUInt8)((value >> 16) & 0xFF);
	beBytes[2] = (SceUInt8)((value >> 8) & 0xFF);
	beBytes[3] = (SceUInt8)(value & 0xFF);
	SceUInt8 header = (SceUInt8)(((channelId & 7) << 4) | LOG_KIND_U32);
	TelephonyResult res = WriteTaggedRecord(header, tagId, beBytes, 4);
	m_mutex.Unlock();
	return res;
}

/* 0x813DB79C */
TelephonyResult TelBbmcLogManager::LogEventMask(int channelId, SceUInt32 eventMask)
{
	m_mutex.Lock();
	SceUInt8 chBits = (SceUInt8)((channelId & 7) << 4);
	int bitCount = telCountSetBits(eventMask);
	TelephonyResult res(TEL_OK);

	if (bitCount <= 0) {
		m_mutex.Unlock();
		return res;
	}

	if (bitCount == 1) {
		SceUInt8 bitIndex = (SceUInt8)telFindFirstSetBit(eventMask);
		res = WriteRawRecord((SceUInt8)(chBits | LOG_KIND_EVENT_BIT), &bitIndex, 1, false);
	} else if ((eventMask & 0xFFFFFF00U) == 0) {
		SceUInt8 lowByte = (SceUInt8)(eventMask & 0xFFU);
		res = WriteRawRecord((SceUInt8)(chBits | LOG_KIND_EVENT_LO_BYTE), &lowByte, 1,
				     false);
	} else {
		SceUInt32 low24 = eventMask & 0x00FFFFFFU;
		if ((eventMask & 0xFF000000U) != 0) {
			SceUInt8 kind = (low24 != 0) ? LOG_KIND_EVENT_HI_PLUS
						     : LOG_KIND_EVENT_HI_ONLY;
			SceUInt8 hiByte = (SceUInt8)((eventMask >> 24) & 0xFFU);
			res = WriteRawRecord((SceUInt8)(chBits | kind), &hiByte, 1, false);
		}
		if (res.IsSuccess() && low24 != 0) {
			SceUInt8 bytes[3];
			bytes[0] = (SceUInt8)((low24 >> 16) & 0xFFU);
			bytes[1] = (SceUInt8)((low24 >> 8) & 0xFFU);
			bytes[2] = (SceUInt8)(low24 & 0xFFU);
			res = WriteRawRecord((SceUInt8)(chBits | LOG_KIND_EVENT_LOW24), bytes, 3,
					     false);
		}
	}

	m_mutex.Unlock();
	return res;
}

/* 0x813DB906 */
TelephonyResult TelBbmcLogManager::LogTimestamp(int channelId)
{
	SceUInt32 packedSec = 0;
	paf::rtc::Tick tick = 0;
	paf::rtc::DateTime dt;
	paf::rtc::DateTimeEx dtEx;

	if (paf::rtc::GetCurrentLocalTick(&tick) == 0 &&
	    paf::rtc::ConvertTickToDateTime(&tick, &dt, &dtEx) == 0) {
		packedSec = (SceUInt32)(dt.second + 60U * (dt.minute + 60U * (dt.hour +
			    24U * (dt.day - 1U))));
	}

	m_mutex.Lock();
	SceUInt8 bytes[3];
	bytes[0] = (SceUInt8)((packedSec >> 16) & 0xFFU);
	bytes[1] = (SceUInt8)((packedSec >> 8) & 0xFFU);
	bytes[2] = (SceUInt8)(packedSec & 0xFFU);
	SceUInt8 header = (SceUInt8)(((channelId & 7) << 4) | LOG_KIND_TIMESTAMP);
	TelephonyResult res = WriteRawRecord(header, bytes, 3, false);
	m_mutex.Unlock();
	return res;
}

/* TelBbmcLogClient (0x813DB9FC..0x813DBCDB) */
TelBbmcLogClient::TelBbmcLogClient()
	: m_manager(NULL), m_channelId(0)
{
}

TelBbmcLogClient::~TelBbmcLogClient()
{
	Unregister();
}

TelephonyResult TelBbmcLogClient::Register(int channelId)
{
	TelephonyResult res(TEL_OK);
	if (m_manager == NULL) {
		m_manager = TelBbmcLogManager::GetInstance();
		if (m_manager != NULL) {
			m_channelId = channelId;
			m_manager->RegisterClient(channelId, this);
		} else {
			res.SetCode(TEL_ERR_NOT_INITIALIZED);
		}
	}
	return res;
}

TelephonyResult TelBbmcLogClient::Unregister()
{
	if (m_manager != NULL) {
		m_manager->UnregisterClient(m_channelId);
		m_manager = NULL;
		m_channelId = 0;
	}
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogClient::LogResult(SceUInt8 tagId, int resultCode)
{
	if (m_manager != NULL)
		return m_manager->LogResult(m_channelId, tagId, resultCode);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogClient::LogU16(SceUInt8 tagId, SceUInt16 value)
{
	if (m_manager != NULL)
		return m_manager->LogU16(m_channelId, tagId, value);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogClient::LogU32(SceUInt8 tagId, SceUInt32 value)
{
	if (m_manager != NULL)
		return m_manager->LogU32(m_channelId, tagId, value);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogClient::LogEventMask(SceUInt32 eventMask)
{
	if (m_manager != NULL)
		return m_manager->LogEventMask(m_channelId, eventMask);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelBbmcLogClient::LogTimestamp()
{
	if (m_manager != NULL)
		return m_manager->LogTimestamp(m_channelId);
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
