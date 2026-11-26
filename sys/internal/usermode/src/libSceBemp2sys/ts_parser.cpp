#include "be_mp2sys_internal.h"

struct TsPacketHeaderBits {
	uint32_t syncByte : 8;
	uint32_t pidHi : 5;
	uint32_t transportPriority : 1;
	uint32_t payloadUnitStart : 1;
	uint32_t transportError : 1;
	uint32_t pidLo : 8;
	uint32_t continuityCounter : 4;
	uint32_t adaptationFieldControl : 2;
	uint32_t scramblingControl : 2;
};

struct AdaptationFlagsBits {
	uint8_t extensionFlag : 1;
	uint8_t privateDataFlag : 1;
	uint8_t splicingPointFlag : 1;
	uint8_t opcrFlag : 1;
	uint8_t pcrFlag : 1;
	uint8_t esPriority : 1;
	uint8_t randomAccess : 1;
	uint8_t discontinuity : 1;
};

struct LtwBits {
	uint16_t offsetHi : 7;
	uint16_t validFlag : 1;
	uint16_t offsetLo : 8;
};

struct AdaptationExtBits {
	uint16_t extLength : 8;
	uint16_t reserved : 5;
	uint16_t seamlessSpliceFlag : 1;
	uint16_t piecewiseRateFlag : 1;
	uint16_t ltwFlag : 1;
};

void TsParser::resetPcrTracking()
{
	m_hasTwoPcrs = 0;
	m_curPcr = (uint64_t)-1;
	m_prevPcr = (uint64_t)-1;
	m_curPcrStreamPos = 0;
	m_prevPcrStreamPos = 0;
	m_totalBytesParsed = 0;
	m_pcrPid = -1;
}

TsParser::TsParser()
{
	m_hasExtraHeader = 0;
	m_hasTwoPcrs = 0;
	m_bitReader = 0;
	m_demuxer = 0;
	m_curPcr = (uint64_t)-1;
	m_prevPcr = (uint64_t)-1;
	m_curPcrStreamPos = 0;
	m_prevPcrStreamPos = 0;
	m_totalBytesParsed = 0;
	m_pcrPid = -1;
	m_bitrate = -1;
	m_extraHeaderBeforeSync = 1;
	m_extraHeaderSize = 0;
}

TsParser::~TsParser()
{
	m_hasExtraHeader = 0;
	m_hasTwoPcrs = 0;
	m_bitReader = 0;
	m_demuxer = 0;
	m_curPcr = (uint64_t)-1;
	m_prevPcr = (uint64_t)-1;
	m_curPcrStreamPos = 0;
	m_prevPcrStreamPos = 0;
	m_totalBytesParsed = 0;
	m_pcrPid = -1;
	m_bitrate = -1;
	m_extraHeaderBeforeSync = 1;
	m_extraHeaderSize = 0;
}

int TsParser::Init(TsDemuxer *demuxer)
{
	if (m_demuxer != 0 || demuxer == 0)
		return 0;

	m_demuxer = demuxer;
	m_bitrate = -1;
	resetPcrTracking();
	return 1;
}

void TsParser::Release()
{
	m_hasExtraHeader = 0;
	m_hasTwoPcrs = 0;
	m_bitReader = 0;
	m_demuxer = 0;
	m_curPcr = (uint64_t)-1;
	m_prevPcr = (uint64_t)-1;
	m_curPcrStreamPos = 0;
	m_prevPcrStreamPos = 0;
	m_totalBytesParsed = 0;
	m_pcrPid = -1;
	m_bitrate = -1;
	m_extraHeaderBeforeSync = 1;
	m_extraHeaderSize = 0;
}

int TsParser::IsInitialized()
{
	return m_demuxer != 0;
}

int TsParser::parsePayload()
{
	int32_t packetSize = 188;
	beLogPrintf(1, " payload length = %d\n", 188 - m_packetBytesRead);

	if (m_hasExtraHeader && !m_extraHeaderBeforeSync)
		packetSize = m_extraHeaderSize + 188;

	TsPayloadParser *parser = m_currentPidInfo->parser;
	if (parser != 0) {
		if (m_currentPidInfo->payloadUnitStart)
			parser->SetState(1);
		m_currentPidInfo->parser->Parse(m_bitReader, m_demuxer, 188 - m_packetBytesRead);
		if (packetSize != 188) {
			int32_t extra = packetSize - 188;
			if (m_bitReader->m_pos + extra <= m_bitReader->m_size)
				m_bitReader->m_pos += extra;
		}
		return BE_MP2SYS_OK;
	}

	return m_bitReader->advance(packetSize - m_packetBytesRead);
}

struct SeamlessSpliceBits {
	uint32_t marker0 : 1;
	uint32_t dtsTop : 3;
	uint32_t spliceType : 4;
	uint32_t dtsMidHi : 8;
	uint32_t marker1 : 1;
	uint32_t dtsMidLo : 7;
	uint32_t dtsBotHi : 8;
	uint32_t marker2 : 1;
	uint32_t dtsBotLo : 7;
};

void TsParser::logSeamlessSplice(const uint8_t *bytes)
{
	const SeamlessSpliceBits *bits = (const SeamlessSpliceBits *)bytes;
	int32_t dtsMid = ((int32_t)bits->dtsMidHi << 7) | (int32_t)bits->dtsMidLo;
	int32_t dtsBot = ((int32_t)bits->dtsBotHi << 7) | (int32_t)bits->dtsBotLo;
	beLogPrintf(1, "\t splice_type: %lld\n", (uint32_t)bits->spliceType);
	beLogPrintf(1, "\t DTS_next_AT[32..30]:%lld, [29..15]:%lld, [14..0]:%lld\n",
		(uint32_t)bits->dtsTop, (int64_t)dtsMid, (int64_t)dtsBot);
}

void TsParser::logLtw(const uint8_t *bytes)
{
	const LtwBits *bits = (const LtwBits *)bytes;
	int32_t offset = ((int32_t)bits->offsetHi << 8) | bits->offsetLo;
	beLogPrintf(1, "\t ltw_valid_flag: %d\n", (int32_t)bits->validFlag);
	beLogPrintf(1, "\t ltw_offset: %d\n", offset);
}

void TsParser::logAdaptationExtHeader(const uint8_t *bytes)
{
	const AdaptationExtBits *bits = (const AdaptationExtBits *)bytes;
	beLogPrintf(1, "\t adaptation_field_extension_length: %d\n", (int32_t)bits->extLength);
	beLogPrintf(1, "\t ltw_flag: %d\n", (int32_t)bits->ltwFlag);
	beLogPrintf(1, "\t piecewise_rate_flag: %d\n", (int32_t)bits->piecewiseRateFlag);
	beLogPrintf(1, "\t seamless_splice_flag: %d\n", (int32_t)bits->seamlessSpliceFlag);
}

void TsParser::logPcrValue(const uint8_t *bytes, uint8_t isPcr)
{
	uint32_t rawBase = *(const uint32_t *)bytes;
	uint16_t rawTail = *(const uint16_t *)(bytes + 4);
	uint32_t baseHighWord = ((rawBase & 0xFFu) << 24)
		| (((rawBase >> 8) & 0xFFu) << 16)
		| (((rawBase >> 16) & 0xFFu) << 8)
		| ((rawBase >> 24) & 0xFFu);
	uint64_t base = (uint64_t)(baseHighWord | ((rawTail & 0x80u) != 0));
	uint64_t ext = (uint64_t)(((rawTail & 1u) << 8) | bytes[5]);
	beLogPrintf(1, "\t %s[base]:%llu, [ext]:%llu\n", isPcr ? "PCR" : "OPCR", base, ext);
}

void TsParser::updateBitrate()
{
	if (!m_hasTwoPcrs)
		return;

	uint64_t curPcr = m_curPcr;
	uint64_t prevPcr = m_prevPcr;
	if ((curPcr / 0xFFFFFFFFULL == prevPcr / 0xFFFFFFFFULL && curPcr > prevPcr) ||
	    (curPcr / 0xFFFFFFFFULL != prevPcr / 0xFFFFFFFFULL && curPcr < prevPcr)) {
		uint64_t bits = 8ULL * (m_curPcrStreamPos - m_prevPcrStreamPos);
		m_bitrate = (int32_t)((bits * 90000ULL) / (curPcr - prevPcr));
	}
}

void TsParser::logAdaptationFlags(const uint8_t *flags)
{
	const AdaptationFlagsBits *bits = (const AdaptationFlagsBits *)flags;
	beLogPrintf(1, "\t discontinuity_indicator: %d\n", (int32_t)bits->discontinuity);
	beLogPrintf(1, "\t random_access_indicator: %d\n", (int32_t)bits->randomAccess);
	beLogPrintf(1, "\t elementary_stream_priority_indicator: %d\n", (int32_t)bits->esPriority);
	beLogPrintf(1, "\t PCR_flag: %d\n", (int32_t)bits->pcrFlag);
	beLogPrintf(1, "\t OPCR_flag: %d\n", (int32_t)bits->opcrFlag);
	beLogPrintf(1, "\t splicing_point_flag: %d\n", (int32_t)bits->splicingPointFlag);
	beLogPrintf(1, "\t transport_private_data_flag: %d\n", (int32_t)bits->privateDataFlag);
	beLogPrintf(1, "\t adaptation_field_extension_flag: %d\n", (int32_t)bits->extensionFlag);
}

static inline uint64_t decodePcrBase(const uint8_t *pcrBytes)
{
	uint32_t rawBase = *(const uint32_t *)pcrBytes;
	uint16_t rawTail = *(const uint16_t *)(pcrBytes + 4);
	uint32_t baseHighWord = ((uint32_t)(uint8_t)rawBase << 24)
		| ((uint32_t)(uint8_t)(rawBase >> 8) << 16)
		| ((uint32_t)(uint8_t)(rawBase >> 16) << 8)
		| (uint32_t)(uint8_t)(rawBase >> 24);
	return (uint64_t)(baseHighWord | ((rawTail & 0x80u) != 0));
}

int TsParser::parseAdaptationField()
{
	m_currentPidInfo->adaptationFieldLength = -1;
	m_currentPidInfo->hasAdaptationField = 1;

	const uint8_t *lenPtr = m_bitReader->currentPtr();
	uint8_t afLen = *lenPtr;
	int ret = m_bitReader->advance(1);
	++m_packetBytesRead;
	m_currentPidInfo->adaptationFieldLength = afLen;

	beLogPrintf(1, "\n\t--- Adaptation Fields\n");
	beLogPrintf(1, "\t adaptation_field_length: %d\n", m_currentPidInfo->adaptationFieldLength);

	if (m_currentPidInfo->adaptationFieldLength == 0)
		return ret;

	const uint8_t *flagsPtr = m_bitReader->currentPtr();
	ret = m_bitReader->advance(1);
	if (ret != BE_MP2SYS_OK)
		return ret;
	++m_packetBytesRead;

	m_currentPidInfo->randomAccessIndicator = (*flagsPtr & 0x40) != 0 ? 1 : 0;
	if (SceBemp2sys_CF2FD310() == 1)
		logAdaptationFlags(flagsPtr);

	int32_t consumed = 1;

	if ((*flagsPtr & 0x10) != 0) {
		const uint8_t *pcrPtr = m_bitReader->currentPtr();
		uint64_t pcrPos = m_totalBytesParsed + (uint64_t)(m_bitReader->m_pos - m_parseStartOffset);
		ret = m_bitReader->advance(6);
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_packetBytesRead += 6;
		consumed += 6;

		if (SceBemp2sys_CF2FD310() == 1)
			logPcrValue(pcrPtr, 1);

		if (m_hasTwoPcrs) {
			if (m_pcrPid == m_currentPidInfo->pid) {
				m_prevPcr = m_curPcr;
				m_prevPcrStreamPos = m_curPcrStreamPos;
				m_curPcrStreamPos = pcrPos;
				m_curPcr = decodePcrBase(pcrPtr);
				updateBitrate();
				if (m_demuxer->m_eventCallback != 0)
					m_demuxer->m_eventCallback->onDemuxerEvent(7, m_currentPidInfo);
			}
		} else if (m_prevPcr == (uint64_t)-1) {
			m_prevPcrStreamPos = pcrPos;
			m_prevPcr = decodePcrBase(pcrPtr);
			m_pcrPid = m_currentPidInfo->pid;
		} else if (m_curPcr == (uint64_t)-1 && m_pcrPid == m_currentPidInfo->pid) {
			m_curPcrStreamPos = pcrPos;
			m_curPcr = decodePcrBase(pcrPtr);
			m_hasTwoPcrs = 1;
		}
	}

	if ((*flagsPtr & 0x08) != 0) {
		const uint8_t *opcrPtr = m_bitReader->currentPtr();
		ret = m_bitReader->advance(6);
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_packetBytesRead += 6;
		consumed += 6;
		if (SceBemp2sys_CF2FD310() == 1)
			logPcrValue(opcrPtr, 0);
	}

	if ((*flagsPtr & 0x04) != 0) {
		const uint8_t *spPtr = m_bitReader->currentPtr();
		int8_t spliceCountdown = (int8_t)*spPtr;
		ret = m_bitReader->advance(1);
		if (ret != BE_MP2SYS_OK)
			return ret;
		++m_packetBytesRead;
		++consumed;
		beLogPrintf(1, "\t splicing_count_down: %d\n", spliceCountdown);
	}

	if ((*flagsPtr & 0x02) != 0) {
		const uint8_t *privLenPtr = m_bitReader->currentPtr();
		uint8_t privLen = *privLenPtr;
		ret = m_bitReader->advance(1);
		if (ret != BE_MP2SYS_OK)
			return ret;
		++m_packetBytesRead;
		beLogPrintf(1, "\t transport_private_data_length: %d\n", privLen);

		ret = m_bitReader->advance(privLen);
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_packetBytesRead += privLen;
		consumed += privLen + 1;
	}

	if ((*flagsPtr & 0x01) != 0) {
		const AdaptationExtBits *ext = (const AdaptationExtBits *)m_bitReader->currentPtr();
		ret = m_bitReader->advance(2);
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_packetBytesRead += 2;
		consumed += 2;

		if (SceBemp2sys_CF2FD310() == 1)
			logAdaptationExtHeader((const uint8_t *)ext);

		int16_t extConsumed = 1;
		if (ext->ltwFlag != 0) {
			const uint8_t *ltwPtr = m_bitReader->currentPtr();
			ret = m_bitReader->advance(2);
			if (ret != BE_MP2SYS_OK)
				return ret;
			m_packetBytesRead += 2;
			consumed += 2;
			extConsumed += 2;
			if (SceBemp2sys_CF2FD310() == 1)
				logLtw(ltwPtr);
		}
		if (ext->piecewiseRateFlag != 0) {
			const uint8_t *prPtr = m_bitReader->currentPtr();
			ret = m_bitReader->advance(3);
			if (ret != BE_MP2SYS_OK)
				return ret;
			m_packetBytesRead += 3;
			consumed += 3;
			extConsumed += 3;
			if (SceBemp2sys_CF2FD310() == 1) {
				int32_t rate = prPtr[2] | (prPtr[1] << 8) | ((prPtr[0] & 0x3F) << 16);
				beLogPrintf(1, "\t piecewise_rate: %d\n", rate);
			}
		}
		if (ext->seamlessSpliceFlag != 0) {
			const uint8_t *ssPtr = m_bitReader->currentPtr();
			ret = m_bitReader->advance(5);
			if (ret != BE_MP2SYS_OK)
				return ret;
			m_packetBytesRead += 5;
			consumed += 5;
			extConsumed += 5;
			if (SceBemp2sys_CF2FD310() == 1)
				logSeamlessSplice(ssPtr);
		}

		int32_t extStuff = (int32_t)ext->extLength - (int32_t)extConsumed;
		ret = m_bitReader->advance(extStuff);
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_packetBytesRead += extStuff;
		consumed += extStuff;
		beLogPrintf(1, "\t adaptation extension field stuffing bytes: %d\n", extStuff);
	}

	int32_t stuffBytes = (int32_t)m_currentPidInfo->adaptationFieldLength - consumed;
	ret = m_bitReader->advance(stuffBytes);
	if (ret != BE_MP2SYS_OK)
		return ret;
	m_packetBytesRead += stuffBytes;
	beLogPrintf(1, "\t adaptation field stuffing bytes: %d\n", stuffBytes);
	return BE_MP2SYS_OK;
}

void TsParser::logTsHeader(const uint8_t *header)
{
	const TsPacketHeaderBits *bits = (const TsPacketHeaderBits *)header;
	int32_t pid = ((int32_t)bits->pidHi << 8) | bits->pidLo;
	beLogPrintf(1, "\n==== TS HEADER[%d] ====\n", m_demuxer->m_packetCount);
	beLogPrintf(1, " sync_byte: 0x%02x\n", (int32_t)bits->syncByte);
	beLogPrintf(1, " transport_error_indicator: %d\n", (int32_t)bits->transportError);
	beLogPrintf(1, " payload_unit_start_indicator: %d\n", (int32_t)bits->payloadUnitStart);
	beLogPrintf(1, " transport_priority: %d\n", (int32_t)bits->transportPriority);
	beLogPrintf(1, " PID: %d\n", pid);
	beLogPrintf(1, " transport_scrambling_control: %d\n", (int32_t)bits->scramblingControl);
	beLogPrintf(1, " adaptation_field_control: %d\n", (int32_t)bits->adaptationFieldControl);
	beLogPrintf(1, " continuity_counter: %d\n", (int32_t)bits->continuityCounter);
}

int TsParser::ParseHeader(uint64_t userTimestamp)
{
	m_packetBytesRead = 0;
	int32_t skipped = 0;
	const uint8_t *hdr = 0;

	for (;;) {
		if (m_bitReader->m_pos + 4 > m_bitReader->m_size) {
			beLogPrintf(3, "[ERROR] %s : bitstream is not enough![%u:%u]\n",
				"ParseHeader", m_bitReader->m_pos, m_bitReader->m_size);
			return BE_MP2SYS_ERR_BITSTREAM_SHORT;
		}
		hdr = m_bitReader->currentPtr();
		if (hdr[0] == 0x47)
			break;
		if (++skipped > 188) {
			beLogPrintf(3, "[ERROR] %s : cannot find sync_byte!\n", "ParseHeader");
			return BE_MP2SYS_ERR_NO_SYNC_BYTE;
		}
		int ret = m_bitReader->advance(1);
		if (ret != BE_MP2SYS_OK)
			return ret;
	}

	if (m_hasExtraHeader && m_extraHeaderBeforeSync && skipped < m_extraHeaderSize) {
		int32_t maxProbe = m_extraHeaderSize - skipped;
		const uint8_t *probe = m_bitReader->currentPtr() + 1;
		int foundLater = 0;
		int32_t laterOffset = 0;
		for (int32_t off = 1; off <= maxProbe && m_bitReader->m_pos + off + 4 <= m_bitReader->m_size; ++off) {
			if (probe[off - 1] == 0x47) {
				foundLater = 1;
				laterOffset = off;
			}
		}
		if (foundLater) {
			m_bitReader->advance(laterOffset);
			hdr = m_bitReader->currentPtr();
		}
	}

	int32_t packetSize = 188;
	if (m_hasExtraHeader && !m_extraHeaderBeforeSync)
		packetSize = m_extraHeaderSize + 188;

	m_bitReader->advance(4);
	m_packetBytesRead = 4;

	const TsPacketHeaderBits *hdrBits = (const TsPacketHeaderBits *)hdr;

	if (SceBemp2sys_CF2FD310() == 1)
		logTsHeader(hdr);

	if (hdrBits->transportError != 0) {
		beLogPrintf(2, "[DEBUG] %s : TP error indicator bit set!!\n", "ParseHeader");
		m_bitReader->advance(packetSize - m_packetBytesRead);
		return BE_MP2SYS_ERR_TRANSPORT_ERROR;
	}

	int16_t pid = (int16_t)(((int32_t)hdrBits->pidHi << 8) | (int32_t)hdrBits->pidLo);
	if (pid == 0x1FFF) {
		m_bitReader->advance(packetSize - m_packetBytesRead);
		return BE_MP2SYS_OK;
	}

	PidInfo *pidInfo = m_demuxer->findPidInfo(pid);
	m_currentPidInfo = pidInfo;

	if (pidInfo == 0) {
		uint32_t afc = hdrBits->adaptationFieldControl;
		if (hdrBits->payloadUnitStart != 0 && (afc == 1 || afc == 3)) {
			const uint8_t *pay = m_bitReader->currentPtr();
			const uint8_t *pes = pay;
			int canCheck = 1;
			if (afc == 3) {
				uint8_t afLen = pay[0];
				pes = pay + afLen + 1;
				if ((uint32_t)(183 - afLen) < 4u)
					canCheck = 0;
			}
			if (canCheck && pes[0] == 0 && pes[1] == 0 && pes[2] == 1) {
				beLogPrintf(2, "[DEBUG] %s : unregistered PES packet? PID[%d], stream_id=0x%02x\n",
					"ParseHeader", (int32_t)pid, pes[3]);
			}
		}

		PidInfo *slot = 0;
		if (m_demuxer->m_privatePidInfos != 0) {
			for (int32_t i = 0; i < m_demuxer->m_maxPrivateCount; ++i) {
				if (m_demuxer->m_privatePidInfos[i].pid == -1) {
					slot = &m_demuxer->m_privatePidInfos[i];
					slot->parser = m_demuxer->m_freePrivateList;
					m_demuxer->m_freePrivateList = slot->parser->m_nextFree;
					break;
				}
			}
		}

		m_currentPidInfo = slot;
		slot->pid = pid;
		if (m_demuxer->m_eventCallback != 0)
			m_demuxer->m_eventCallback->onDemuxerEvent(3, m_currentPidInfo);

		pidInfo = m_currentPidInfo;
		SectionParser *privParser = static_cast<SectionParser *>(pidInfo->parser);
		if (privParser->m_callback == 0) {
			if (pidInfo != 0 && pidInfo->parser != 0 && pidInfo->parser->GetParserType() == BE_PARSER_TYPE_PRIVATE) {
				pidInfo->parser->Init();
				pidInfo->parser->m_nextFree = m_demuxer->m_freePrivateList;
				m_demuxer->m_freePrivateList = pidInfo->parser;
				pidInfo->reset();
			}
			m_currentPidInfo = 0;
			m_bitReader->advance(packetSize - m_packetBytesRead);
			return BE_MP2SYS_ERR_PARSER_INACTIVE;
		}
	}

	pidInfo->userTimestamp = userTimestamp;
	pidInfo->payloadUnitStart = (uint8_t)hdrBits->payloadUnitStart;
	pidInfo->scramblingControl = (uint8_t)hdrBits->scramblingControl;
	pidInfo->continuityCounter = (int8_t)hdrBits->continuityCounter;

	uint32_t afc = hdrBits->adaptationFieldControl;
	if (afc == 2 || afc == 3)
		parseAdaptationField();
	else
		pidInfo->hasAdaptationField = 0;

	int32_t payloadStartPos = m_bitReader->m_pos;
	if (m_demuxer->m_streamInfo != 0 &&
	    m_demuxer->m_streamInfo->FindPidFilter((uint16_t)m_currentPidInfo->pid, 8) != 0) {
		const uint8_t *pktPtr = m_bitReader->currentPtr() - payloadStartPos;
		IStreamInfoSink *pktSink = 0;
		if (m_demuxer->m_streamInfo != 0) {
			pktSink = static_cast<IStreamInfoSink *>(
				m_demuxer->m_streamInfo->FindPidFilter((uint16_t)m_currentPidInfo->pid, 8));
		}
		pktSink->onTsPacket(m_currentPidInfo, pktPtr, 188, m_bitReader->currentPtr(), 188 - payloadStartPos);
	}
	if (m_demuxer->m_streamInfo != 0 &&
	    m_demuxer->m_streamInfo->FindPidFilter((uint16_t)m_currentPidInfo->pid, 4) != 0) {
		IStreamInfoSink *paySink = 0;
		if (m_demuxer->m_streamInfo != 0) {
			paySink = static_cast<IStreamInfoSink *>(
				m_demuxer->m_streamInfo->FindPidFilter((uint16_t)m_currentPidInfo->pid, 4));
		}
		paySink->onTsPayload(m_currentPidInfo, m_bitReader->currentPtr(), 188 - payloadStartPos);
	}

	beLogPrintf(1, "====================================\n");

	int ret;
	int32_t remainAfterHeader;
	if (hdrBits->adaptationFieldControl == 1 || hdrBits->adaptationFieldControl == 3) {
		ret = parsePayload();
		remainAfterHeader = packetSize - m_packetBytesRead;
	} else {
		remainAfterHeader = packetSize - m_packetBytesRead;
		ret = m_bitReader->advance(remainAfterHeader);
	}

	int32_t actuallyConsumed = m_bitReader->m_pos - payloadStartPos;
	if (actuallyConsumed < remainAfterHeader)
		m_bitReader->advance(remainAfterHeader - actuallyConsumed);

	return ret;
}

int TsParser::Parse(BitReader *reader, uint64_t userTimestamp)
{
	m_bitReader = reader;
	if (reader == 0) {
		beLogPrintf(2, "[DEBUG] %s : Invalid bitstream buffer set!\n", "Parse");
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	m_parseStartOffset = reader->m_pos;
	int ret = ParseHeader(userTimestamp);
	m_totalBytesParsed += (uint64_t)(m_bitReader->m_pos - m_parseStartOffset);
	return ret;
}
