#include "be_mp2sys_internal.h"

int PesParser::Init()
{
	m_state = 0;
	m_active = 1;
	return 1;
}

void PesParser::Release()
{
	m_active = 0;
}

struct ProgramPacketSeqBits {
	uint16_t seqCounter : 7;
	uint16_t markerOne : 1;
	uint16_t origStuffLen : 6;
	uint16_t mpegId : 1;
	uint16_t markerTwo : 1;
};

struct PesExtensionFlagsBits {
	uint8_t extFlagTwo : 1;
	uint8_t reserved : 3;
	uint8_t pstdBufferFlag : 1;
	uint8_t seqCounterFlag : 1;
	uint8_t packHeaderFlag : 1;
	uint8_t privateDataFlag : 1;
};

struct EscrBits {
	uint8_t midTop2 : 2;
	uint8_t marker0 : 1;
	uint8_t top3 : 3;
	uint8_t reserved : 2;
	uint8_t midHi;
	uint8_t botTop2 : 2;
	uint8_t marker1 : 1;
	uint8_t midLo : 5;
	uint8_t botHi;
	uint8_t extHi : 2;
	uint8_t marker2 : 1;
	uint8_t botLo : 5;
	uint8_t marker3 : 1;
	uint8_t extLo : 7;
};

struct PstdBufferBits {
	uint8_t sizeHi : 5;
	uint8_t scale : 1;
	uint8_t marker : 2;
	uint8_t sizeLo;
};

struct TrickModeBits {
	uint8_t freqTruncation : 2;
	uint8_t intraSliceRefresh : 1;
	uint8_t fieldId : 2;
	uint8_t trickModeControl : 3;
};

struct PesOptionalHeaderBits {
	uint8_t origOrCopy : 1;
	uint8_t copyright : 1;
	uint8_t dataAlignment : 1;
	uint8_t priority : 1;
	uint8_t scrambling : 2;
	uint8_t markerBits : 2;
	uint8_t extensionFlag : 1;
	uint8_t crcFlag : 1;
	uint8_t addCopyFlag : 1;
	uint8_t dsmTrickFlag : 1;
	uint8_t esRateFlag : 1;
	uint8_t escrFlag : 1;
	uint8_t ptsDtsFlags : 2;
	uint8_t headerDataLen;
};

void PesParser::logProgramPacketSeqCounter(const uint8_t *bytes)
{
	const ProgramPacketSeqBits *bits = (const ProgramPacketSeqBits *)bytes;
	beLogPrintf(1, "\t\t program_packet_sequence_counter: %d\n", (int32_t)bits->seqCounter);
	beLogPrintf(1, "\t\t MPEG1_MPEG2_identifier: %d\n", (int32_t)bits->mpegId);
	beLogPrintf(1, "\t\t original_stuff_length: %d\n", (int32_t)bits->origStuffLen);
}

void PesParser::logPesExtensionFlags(const uint8_t *flags)
{
	const PesExtensionFlagsBits *bits = (const PesExtensionFlagsBits *)flags;
	beLogPrintf(1, "\t\t PES_private_data_flag: %d\n", (int32_t)bits->privateDataFlag);
	beLogPrintf(1, "\t\t pack_header_field_flag: %d\n", (int32_t)bits->packHeaderFlag);
	beLogPrintf(1, "\t\t program_packet_sequence_counter_flag: %d\n", (int32_t)bits->seqCounterFlag);
	beLogPrintf(1, "\t\t P-STD_buffer_flag: %d\n", (int32_t)bits->pstdBufferFlag);
	beLogPrintf(1, "\t\t PES_extension_flag_2: %d\n", (int32_t)bits->extFlagTwo);
}

int PesParser::parsePesExtension(BitReader *reader)
{
	beLogPrintf(1, "\t\t******  PES Extension  ******\n");

	const uint8_t *extFlags = reader->currentPtr();
	int ret = reader->advance(1);
	if (ret != 0)
		return ret;

	if (SceBemp2sys_CF2FD310() == 1)
		logPesExtensionFlags(extFlags);

	if ((*extFlags & 0x80) != 0) {
		ret = reader->advance(16);
		if (ret != 0)
			return ret;
	}

	if ((*extFlags & 0x40) != 0) {
		const uint8_t *lenPtr = reader->currentPtr();
		uint8_t packLen = *lenPtr;
		ret = reader->advance((int32_t)packLen + 1);
		if (ret != 0)
			return ret;
	}

	if ((*extFlags & 0x20) != 0) {
		const uint8_t *seqPtr = reader->currentPtr();
		ret = reader->advance(2);
		if (ret != 0)
			return ret;
		if (SceBemp2sys_CF2FD310() == 1)
			logProgramPacketSeqCounter(seqPtr);
	}

	if ((*extFlags & 0x10) != 0) {
		const uint8_t *pstdPtr = reader->currentPtr();
		ret = reader->advance(2);
		if (ret != 0)
			return ret;
		if (SceBemp2sys_CF2FD310() == 1) {
			const PstdBufferBits *pstd = (const PstdBufferBits *)pstdPtr;
			uint32_t bufSize = (uint32_t)pstd->sizeLo | (((uint32_t)(pstd->sizeHi & 0x1F)) << 8);
			beLogPrintf(1, "\t\t P-STD_buffer_scale: %d\n", (int32_t)(pstd->scale & 1));
			beLogPrintf(1, "\t\t P-STD_buffer_size: %d\n", bufSize);
		}
	}

	if ((*extFlags & 0x01) != 0) {
		const uint8_t *extLenPtr = reader->currentPtr();
		ret = reader->advance(1);
		if (ret != 0)
			return ret;
		beLogPrintf(1, "\t\t PES_extension_field_length: %d\n", *extLenPtr & 0x7F);
		ret = reader->advance(*extLenPtr & 0x7F);
		if (ret != 0)
			return ret;
	}

	beLogPrintf(1, "\t\t*****************************\n");
	return ret;
}

int PesParser::parseAdditionalCopyInfo(BitReader *reader)
{
	const uint8_t *copyPtr = reader->currentPtr();
	int ret = reader->advance(1);
	if (ret != 0)
		return ret;
	beLogPrintf(1, "\t additional_copy_info: %d\n", *copyPtr & 0x7F);
	return 0;
}

void PesParser::logTrickModeFields(const uint8_t *bytePtr)
{
	const TrickModeBits *bits = (const TrickModeBits *)bytePtr;
	beLogPrintf(1, "\t field_id: %d\n", (int32_t)bits->fieldId);
	beLogPrintf(1, "\t intra_slice_refresh: %d\n", (int32_t)bits->intraSliceRefresh);
	beLogPrintf(1, "\t frequency_truncation: %d\n", (int32_t)bits->freqTruncation);
}

int PesParser::parseTrickMode(BitReader *reader)
{
	const TrickModeBits *trickPtr = (const TrickModeBits *)reader->currentPtr();
	beLogPrintf(1, "\t trick_mode_control: %d\n", (int32_t)(trickPtr->trickModeControl & 7));

	uint8_t mode = (*(const uint8_t *)trickPtr) & 0xE0;
	if (mode == 0x00 || mode == 0x60) {
		const uint8_t *cur = reader->currentPtr();
		if (SceBemp2sys_CF2FD310() == 1)
			logTrickModeFields(cur);
	} else if (mode == 0x20 || mode == 0x80) {
		const uint8_t *cur = reader->currentPtr();
		beLogPrintf(1, "\t rep_cntrl: %d\n", *cur & 0x1F);
	} else if (mode == 0x40) {
		const TrickModeBits *cur = (const TrickModeBits *)reader->currentPtr();
		beLogPrintf(1, "\t field_id: %d\n", (int32_t)(cur->fieldId & 3));
	}

	return reader->advance(1);
}

int PesParser::parseEsRate(BitReader *reader)
{
	const uint8_t *ratePtr = reader->currentPtr();
	int ret = reader->advance(3);
	if (ret != 0)
		return ret;

	uint32_t esRate = (((uint32_t)(ratePtr[0] & 0x7F)) << 16)
		| ((uint32_t)ratePtr[1] << 8)
		| ((uint32_t)ratePtr[2] >> 1);
	beLogPrintf(1, "\t ES_rate: %d\n", esRate);
	return 0;
}

void PesParser::logEscr(const uint8_t *bytes)
{
	const EscrBits *bits = (const EscrBits *)bytes;
	uint32_t baseHigh = (uint32_t)(bits->top3 & 7);
	uint32_t baseMid = (((uint32_t)(bits->midTop2 & 3)) << 13)
		| ((uint32_t)bits->midHi << 5)
		| ((uint32_t)(bits->midLo & 0x1F));
	uint32_t baseLow = (((uint32_t)(bits->botTop2 & 3)) << 13)
		| ((uint32_t)bits->botHi << 5)
		| ((uint32_t)(bits->botLo & 0x1F));
	uint32_t ext = (((uint32_t)(bits->extHi & 3)) << 7)
		| ((uint32_t)(bits->extLo & 0x7F));

	beLogPrintf(1, "\t ESCR: base[32..30]-%llu [29..15]-%llu [14-0]-%llu ext-%llu\n",
		(uint64_t)baseHigh, (uint64_t)baseMid, (uint64_t)baseLow, (uint64_t)ext);
}

int PesParser::parseEscr(BitReader *reader)
{
	const uint8_t *escrPtr = reader->currentPtr();
	int ret = reader->advance(6);
	if (ret != 0)
		return ret;
	if (SceBemp2sys_CF2FD310() == 1)
		logEscr(escrPtr);
	return 0;
}

void PesParser::storePtsOrDts(const uint8_t *bytes, uint8_t isPts)
{
	uint32_t mid15 = ((uint32_t)bytes[2] >> 1) | ((uint32_t)bytes[1] << 7);
	uint32_t low15 = ((uint32_t)bytes[4] >> 1) | ((uint32_t)bytes[3] << 7);
	uint32_t b0 = (uint32_t)(int8_t)bytes[0];
	uint32_t valLo = low15 | (mid15 << 15) | ((uint32_t)(uint8_t)(b0 >> 1) << 30);
	int32_t valHi = (((b0 >> 1) & 3u) != ((b0 >> 1) & 7u)) ? 1 : 0;

	if (isPts != 0) {
		m_packetInfo.ptsHi = valHi;
		m_packetInfo.ptsLo = valLo;
	} else {
		m_packetInfo.dtsHi = valHi;
		m_packetInfo.dtsLo = valLo;
	}
}

void PesParser::logPtsOrDts(const uint8_t *bytes, uint8_t isPts)
{
	uint32_t mid15 = ((uint32_t)bytes[2] >> 1) | ((uint32_t)bytes[1] << 7);
	uint32_t low15 = ((uint32_t)bytes[4] >> 1) | ((uint32_t)bytes[3] << 7);
	uint32_t b0 = (uint32_t)(int8_t)bytes[0];
	uint32_t valLo = ((mid15 << 15) | ((uint32_t)(uint8_t)(b0 >> 1) << 30)) + low15;
	uint32_t valHi = (b0 & 8u) != 0 ? 1u : 0u;
	const char *label = (isPts != 0) ? "PTS" : "DTS";

	beLogPrintf(1, "\t %s[%llu]: [32..30]-%llu [29..15]-%d [14-0]-%d [%u:%u]\n",
		label, (uint32_t)(bytes[0] >> 4), (b0 >> 1) & 7u,
		mid15, low15, valHi, valLo);
}

int PesParser::parsePtsDts(BitReader *reader, uint32_t ptsDtsFlags)
{
	int ret = 0;
	if (ptsDtsFlags >= 2) {
		const uint8_t *ptsPtr = reader->currentPtr();
		ret = reader->advance(5);
		if (ret == 0) {
			if (SceBemp2sys_CF2FD310() == 1)
				logPtsOrDts(ptsPtr, 1);
			{
				uint32_t b0 = (uint32_t)(int8_t)ptsPtr[0];
				uint32_t mid15 = ((uint32_t)ptsPtr[2] >> 1) | ((uint32_t)ptsPtr[1] << 7);
				uint32_t low15 = ((uint32_t)ptsPtr[4] >> 1) | ((uint32_t)ptsPtr[3] << 7);
				m_packetInfo.ptsHi = (((b0 >> 1) & 3u) != ((b0 >> 1) & 7u)) ? 1 : 0;
				m_packetInfo.ptsLo = low15 | (mid15 << 15) | ((uint32_t)(uint8_t)(b0 >> 1) << 30);
			}

			if (ptsDtsFlags == 3) {
				const uint8_t *dtsPtr = reader->currentPtr();
				ret = reader->advance(5);
				if (ret == 0) {
					if (SceBemp2sys_CF2FD310() == 1)
						logPtsOrDts(dtsPtr, 0);
					{
						uint32_t b0 = (uint32_t)(int8_t)dtsPtr[0];
						uint32_t mid15 = ((uint32_t)dtsPtr[2] >> 1) | ((uint32_t)dtsPtr[1] << 7);
						uint32_t low15 = ((uint32_t)dtsPtr[4] >> 1) | ((uint32_t)dtsPtr[3] << 7);
						m_packetInfo.dtsHi = (((b0 >> 1) & 3u) != ((b0 >> 1) & 7u)) ? 1 : 0;
						m_packetInfo.dtsLo = low15 | (mid15 << 15) | ((uint32_t)(uint8_t)(b0 >> 1) << 30);
					}
				}
			}
		}
	}
	return ret;
}

void PesParser::logPesOptionalHeader(const uint8_t *bytes)
{
	const PesOptionalHeaderBits *bits = (const PesOptionalHeaderBits *)bytes;
	beLogPrintf(1, "\t '10': %d\n", (int32_t)bits->markerBits);
	beLogPrintf(1, "\t PES_scrambling_control: %d\n", (int32_t)bits->scrambling);
	beLogPrintf(1, "\t PES_priority: %d\n", (int32_t)bits->priority);
	beLogPrintf(1, "\t data_alignment_indicator: %d\n", (int32_t)bits->dataAlignment);
	beLogPrintf(1, "\t copyright: %d\n", (int32_t)bits->copyright);
	beLogPrintf(1, "\t original_or_copy: %d\n", (int32_t)bits->origOrCopy);
	beLogPrintf(1, "\t PTS_DTS_flags: %d\n", (int32_t)bits->ptsDtsFlags);
	beLogPrintf(1, "\t ESCR_flag: %d\n", (int32_t)bits->escrFlag);
	beLogPrintf(1, "\t ES_rate_flag: %d\n", (int32_t)bits->esRateFlag);
	beLogPrintf(1, "\t DSM_trick_mode_flag: %d\n", (int32_t)bits->dsmTrickFlag);
	beLogPrintf(1, "\t additional_copy_info_flag: %d\n", (int32_t)bits->addCopyFlag);
	beLogPrintf(1, "\t PES_CRC_flag: %d\n", (int32_t)bits->crcFlag);
	beLogPrintf(1, "\t PES_extension_flag: %d\n", (int32_t)bits->extensionFlag);
	beLogPrintf(1, "\t PES_header_data_length: %d\n", (int32_t)bits->headerDataLen);
}

void PesParser::logPesStartHeader(const uint8_t *bytes)
{
	uint32_t firstWord = *(const uint32_t *)bytes;
	uint16_t rawLen = *(const uint16_t *)(bytes + 4);
	uint32_t packetLen = ((uint32_t)(rawLen & 0xFFu) << 8) | (rawLen >> 8);
	uint32_t startCode = ((firstWord & 0xFFu) << 16) | (firstWord & 0xFF00u) | ((firstWord >> 16) & 0xFFu);
	beLogPrintf(1, "\t packet_start_code: 0x%06x\n", startCode);
	beLogPrintf(1, "\t stream_id: 0x%02x\n", bytes[3]);
	beLogPrintf(1, "\t PES_packet_length: %d\n", packetLen);
}

int PesParser::parseHeader()
{
	BitReader reader;
	reader.setBuffer(m_headerBuf, m_headerSize);

	TsDemuxer *demuxer = 0;
	if (m_host->isDemuxerHost() == 1)
		demuxer = static_cast<PesDemuxerHost *>(m_host)->m_demuxerHandle->m_demuxer;

	m_packetInfo.dtsHi = -1;
	m_packetInfo.dtsLo = (uint32_t)-1;
	m_packetInfo.ptsHi = -1;
	m_packetInfo.ptsLo = (uint32_t)-1;

	beLogPrintf(1, "\n\t********** PES HEADER *******\n");

	const uint8_t *startHdr = reader.currentPtr();
	int ret = reader.advance(6);
	if (ret != 0)
		return ret;

	if (SceBemp2sys_CF2FD310() == 1)
		logPesStartHeader(startHdr);

	if (m_hasOptionalHeader != 0) {
		const uint8_t *optHdr = reader.currentPtr();
		ret = reader.advance(3);
		if (ret != 0)
			return ret;

		if (SceBemp2sys_CF2FD310() == 1)
			logPesOptionalHeader(optHdr);

		ret = parsePtsDts(&reader, (uint32_t)(optHdr[1] >> 6));
		if (ret != 0)
			return ret;

		if (demuxer != 0) {
			int64_t ptsOffset = demuxer->m_ptsOffset;
			if (m_packetInfo.ptsLo != (uint32_t)-1 && m_packetInfo.ptsHi != -1 && ptsOffset != 0) {
				uint64_t pts = ((uint64_t)(uint32_t)m_packetInfo.ptsHi << 32) | m_packetInfo.ptsLo;
				pts += (uint64_t)ptsOffset;
				m_packetInfo.ptsHi = (int32_t)((pts >> 32) & 1u);
				m_packetInfo.ptsLo = (uint32_t)pts;
			}
			if (m_packetInfo.dtsLo != (uint32_t)-1 && m_packetInfo.dtsHi != -1 && ptsOffset != 0) {
				uint64_t dts = ((uint64_t)(uint32_t)m_packetInfo.dtsHi << 32) | m_packetInfo.dtsLo;
				dts += (uint64_t)ptsOffset;
				m_packetInfo.dtsHi = (int32_t)((dts >> 32) & 1u);
				m_packetInfo.dtsLo = (uint32_t)dts;
			}
		}

		if ((optHdr[1] & 0x20) != 0) {
			ret = parseEscr(&reader);
			if (ret != 0)
				return ret;
		}
		if ((optHdr[1] & 0x10) != 0) {
			ret = parseEsRate(&reader);
			if (ret != 0)
				return ret;
		}
		if ((optHdr[1] & 0x08) != 0) {
			ret = parseTrickMode(&reader);
			if (ret != 0)
				return ret;
		}
		if ((optHdr[1] & 0x04) != 0) {
			ret = parseAdditionalCopyInfo(&reader);
			if (ret != 0)
				return ret;
		}
		if ((optHdr[1] & 0x02) != 0) {
			ret = reader.advance(2);
			if (ret != 0)
				return ret;
		}
		if ((optHdr[1] & 0x01) != 0) {
			ret = parsePesExtension(&reader);
			if (ret != 0)
				return ret;
		}
	}

	if (demuxer != 0 && demuxer->m_streamInfo != 0 &&
	    demuxer->m_streamInfo->FindPidFilter((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid, 2) != 0) {
		IStreamInfoSink *sink = 0;
		if (demuxer->m_streamInfo != 0) {
			sink = static_cast<IStreamInfoSink *>(
				demuxer->m_streamInfo->FindPidFilter((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid, 2));
		}
		sink->onPesHeader((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid,
			m_packetInfo.ptsHi, m_packetInfo.ptsLo,
			m_packetInfo.dtsHi, m_packetInfo.dtsLo);
	}

	if (m_packetInfo.callback != 0) {
		void *userData = m_host->getUserData();
		m_packetInfo.callback->onPesHeader(userData, &m_packetInfo);
	}

	if (demuxer != 0)
		demuxer->m_tsParser.m_currentPidInfo->randomAccessIndicator = 0;

	beLogPrintf(1, "\n\t*****************************\n");
	return ret;
}

void PesParser::updateOptionalHeaderFlag(int32_t streamId)
{
	uint8_t hasOpt = 0;
	if (streamId != 0xBC && streamId != 0xBE && streamId != 0xBF
		&& streamId != 0xF0 && streamId != 0xF1 && streamId != 0xFF
		&& streamId != 0xF2 && streamId != 0xF8) {
		hasOpt = 1;
	}
	m_hasOptionalHeader = hasOpt;
}

int PesParser::Parse(BitReader *reader, IPesHostCallback *host, int32_t payloadLen)
{
	if (reader == 0 || host == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	m_bitReader = reader;
	m_host = host;

	TsDemuxer *demuxer = 0;
	if (host->isDemuxerHost() == 1)
		demuxer = static_cast<PesDemuxerHost *>(m_host)->m_demuxerHandle->m_demuxer;

	int ret = 0;
	int32_t consumed = 0;
	while (consumed < payloadLen) {
		if (m_state == 1) {
			m_headerPos = 0;
			m_headerSize = 0;
			m_state = 2;
			m_hasOptionalHeader = 0;
		} else if (m_state == 2) {
			if (m_headerPos < 6) {
				int32_t toCopy = 6 - m_headerPos;
				if (toCopy > payloadLen - consumed)
					toCopy = payloadLen - consumed;
				beMemcpy(m_headerBuf + m_headerPos, m_bitReader->currentPtr(), (SceSize)toCopy);
				ret = m_bitReader->advance(toCopy);
				if (ret != 0)
					return ret;
				m_headerPos += toCopy;
				consumed += toCopy;
			}
			if (m_headerPos == 6 && m_headerSize == 0) {
				uint32_t firstWord = *(const uint32_t *)m_headerBuf;
				uint32_t startCode = ((firstWord & 0xFFu) << 16) | (firstWord & 0xFF00u) | ((firstWord >> 16) & 0xFFu);
				if (startCode != 1u)
					return BE_MP2SYS_ERR_BAD_PES_START_CODE;
				uint8_t streamId = m_headerBuf[3];
				uint16_t rawLen = *(const uint16_t *)(m_headerBuf + 4);
				uint32_t packetLen = ((uint32_t)(rawLen & 0xFFu) << 8) | (rawLen >> 8);
				m_packetInfo.streamId = streamId;
				m_packetInfo.packetLength = packetLen;
				m_packetInfo.bytesRead = 0;
				updateOptionalHeaderFlag(streamId);
				if (!m_hasOptionalHeader)
					m_state = 3;
				m_headerSize = 6;
			}
			if (m_headerPos < 9 && m_headerSize != 0) {
				int32_t toCopy = 9 - m_headerPos;
				if (toCopy > payloadLen - consumed)
					toCopy = payloadLen - consumed;
				beMemcpy(m_headerBuf + m_headerPos, m_bitReader->currentPtr(), (SceSize)toCopy);
				ret = m_bitReader->advance(toCopy);
				if (ret != 0)
					return ret;
				m_headerPos += toCopy;
				consumed += toCopy;
				m_packetInfo.bytesRead += (uint32_t)toCopy;
			}
			if (m_headerPos == 9) {
				m_headerSize += (int32_t)m_headerBuf[8] + 3;
				m_state = 3;
			}
		} else if (m_state == 3) {
			if (m_headerPos < m_headerSize) {
				int32_t toCopy = m_headerSize - m_headerPos;
				if (toCopy > payloadLen - consumed)
					toCopy = payloadLen - consumed;
				beMemcpy(m_headerBuf + m_headerPos, m_bitReader->currentPtr(), (SceSize)toCopy);
				ret = m_bitReader->advance(toCopy);
				if (ret != 0)
					return ret;
				m_headerPos += toCopy;
				consumed += toCopy;
				m_packetInfo.bytesRead += (uint32_t)toCopy;
			}
			if (m_headerPos == m_headerSize) {
				ret = parseHeader();
				m_headerPos = 0;
				m_state = 4;
				if (ret != 0)
					return ret;
			}
		} else if (m_state == 4) {
			if (m_hasOptionalHeader != 0) {
				int32_t chunkLen = payloadLen - consumed;
				if (m_packetInfo.packetLength != 0) {
					if (m_packetInfo.bytesRead + (uint32_t)chunkLen > m_packetInfo.packetLength)
						chunkLen = (int32_t)(m_packetInfo.packetLength - m_packetInfo.bytesRead);
				}
				if (demuxer != 0 && demuxer->m_streamInfo != 0 &&
				    demuxer->m_streamInfo->FindPidFilter((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid, 2) != 0) {
					IStreamInfoSink *sink = 0;
					if (demuxer->m_streamInfo != 0) {
						sink = static_cast<IStreamInfoSink *>(
							demuxer->m_streamInfo->FindPidFilter((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid, 2));
					}
					sink->onPesPayload((uint16_t)demuxer->m_tsParser.m_currentPidInfo->pid,
						m_packetInfo.ptsHi, m_packetInfo.ptsLo,
						m_packetInfo.dtsHi, m_packetInfo.dtsLo,
						m_bitReader->currentPtr(), chunkLen);
				}
				if (m_packetInfo.callback != 0) {
					void *userData = m_host->getUserData();
					m_packetInfo.callback->onPesPayload(userData, &m_packetInfo,
						m_bitReader->currentPtr(), chunkLen);
				}
				m_packetInfo.bytesRead += (uint32_t)chunkLen;
			}
			ret = m_bitReader->advance(payloadLen - consumed);
			if (ret != 0)
				return ret;
			consumed = payloadLen;
		} else {
			if (m_state != 0) {
				int32_t pidVal = -1;
				if (demuxer != 0)
					pidVal = demuxer->m_tsParser.m_currentPidInfo->pid;
				beLogPrintf(3, "[ERROR] %s : unknown PESParser[%d] state: %d\n", "Parse", pidVal, m_state);
			}
			return BE_MP2SYS_ERR_UNKNOWN_PARSER_STATE;
		}
	}
	return ret;
}
