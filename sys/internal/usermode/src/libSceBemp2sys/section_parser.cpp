#include "be_mp2sys_internal.h"

/* Standard MPEG-2 CRC32 lookup table (MSB-first polynomial) */
static const uint32_t g_mpegCrc32Table[256] = {
	0x00000000u, 0x04C11DB7u, 0x09823B6Eu, 0x0D4326D9u, 0x130476DCu, 0x17C56B6Bu, 0x1A864DB2u, 0x1E475005u,
	0x2608EDB8u, 0x22C9F00Fu, 0x2F8AD6D6u, 0x2B4BCB61u, 0x350C9B64u, 0x31CD86D3u, 0x3C8EA00Au, 0x384FBDBDu,
	0x4C11DB70u, 0x48D0C6C7u, 0x4593E01Eu, 0x4152FDA9u, 0x5F15ADACu, 0x5BD4B01Bu, 0x569796C2u, 0x52568B75u,
	0x6A1936C8u, 0x6ED82B7Fu, 0x639B0DA6u, 0x675A1011u, 0x791D4014u, 0x7DDC5DA3u, 0x709F7B7Au, 0x745E66CDu,
	0x9823B6E0u, 0x9CE2AB57u, 0x91A18D8Eu, 0x95609039u, 0x8B27C03Cu, 0x8FE6DD8Bu, 0x82A5FB52u, 0x8664E6E5u,
	0xBE2B5B58u, 0xBAEA46EFu, 0xB7A96036u, 0xB3687D81u, 0xAD2F2D84u, 0xA9EE3033u, 0xA4AD16EAu, 0xA06C0B5Du,
	0xD4326D90u, 0xD0F37027u, 0xDDB056FEu, 0xD9714B49u, 0xC7361B4Cu, 0xC3F706FBu, 0xCEB42022u, 0xCA753D95u,
	0xF23A8028u, 0xF6FB9D9Fu, 0xFBB8BB46u, 0xFF79A6F1u, 0xE13EF6F4u, 0xE5FFEB43u, 0xE8BCCD9Au, 0xEC7DD02Du,
	0x34867077u, 0x30476DC0u, 0x3D044B19u, 0x39C556AEu, 0x278206ABu, 0x23431B1Cu, 0x2E003DC5u, 0x2AC12072u,
	0x128E9DCFu, 0x164F8078u, 0x1B0CA6A1u, 0x1FCDBB16u, 0x018AEB13u, 0x054BF6A4u, 0x0808D07Du, 0x0CC9CDCAu,
	0x7897AB07u, 0x7C56B6B0u, 0x71159069u, 0x75D48DDEu, 0x6B93DDDBu, 0x6F52C06Cu, 0x6211E6B5u, 0x66D0FB02u,
	0x5E9F46BFu, 0x5A5E5B08u, 0x571D7DD1u, 0x53DC6066u, 0x4D9B3063u, 0x495A2DD4u, 0x44190B0Du, 0x40D816BAu,
	0xACA5C697u, 0xA864DB20u, 0xA527FDF9u, 0xA1E6E04Eu, 0xBFA1B04Bu, 0xBB60ADFCu, 0xB6238B25u, 0xB2E29692u,
	0x8AAD2B2Fu, 0x8E6C3698u, 0x832F1041u, 0x87EE0DF6u, 0x99A95DF3u, 0x9D684044u, 0x902B669Du, 0x94EA7B2Au,
	0xE0B41DE7u, 0xE4750050u, 0xE9362689u, 0xEDF73B3Eu, 0xF3B06B3Bu, 0xF771768Cu, 0xFA325055u, 0xFEF34DE2u,
	0xC6BCF05Fu, 0xC27DEDE8u, 0xCF3ECB31u, 0xCBFFD686u, 0xD5B88683u, 0xD1799B34u, 0xDC3ABDEDu, 0xD8FBA05Au,
	0x690CE0EEu, 0x6DCDFD59u, 0x608EDB80u, 0x644FC637u, 0x7A089632u, 0x7EC98B85u, 0x738AAD5Cu, 0x774BB0EBu,
	0x4F040D56u, 0x4BC510E1u, 0x46863638u, 0x42472B8Fu, 0x5C007B8Au, 0x58C1663Du, 0x558240E4u, 0x51435D53u,
	0x251D3B9Eu, 0x21DC2629u, 0x2C9F00F0u, 0x285E1D47u, 0x36194D42u, 0x32D850F5u, 0x3F9B762Cu, 0x3B5A6B9Bu,
	0x0315D626u, 0x07D4CB91u, 0x0A97ED48u, 0x0E56F0FFu, 0x1011A0FAu, 0x14D0BD4Du, 0x19939B94u, 0x1D528623u,
	0xF12F560Eu, 0xF5EE4BB9u, 0xF8AD6D60u, 0xFC6C70D7u, 0xE22B20D2u, 0xE6EA3D65u, 0xEBA91BBCu, 0xEF68060Bu,
	0xD727BBB6u, 0xD3E6A601u, 0xDEA580D8u, 0xDA649D6Fu, 0xC423CD6Au, 0xC0E2D0DDu, 0xCDA1F604u, 0xC960EBB3u,
	0xBD3E8D7Eu, 0xB9FF90C9u, 0xB4BCB610u, 0xB07DABA7u, 0xAE3AFBA2u, 0xAAFBE615u, 0xA7B8C0CCu, 0xA379DD7Bu,
	0x9B3660C6u, 0x9FF77D71u, 0x92B45BA8u, 0x9675461Fu, 0x8832161Au, 0x8CF30BADu, 0x81B02D74u, 0x857130C3u,
	0x5D8A9099u, 0x594B8D2Eu, 0x5408ABF7u, 0x50C9B640u, 0x4E8EE645u, 0x4A4FFBF2u, 0x470CDD2Bu, 0x43CDC09Cu,
	0x7B827D21u, 0x7F436096u, 0x7200464Fu, 0x76C15BF8u, 0x68860BFDu, 0x6C47164Au, 0x61043093u, 0x65C52D24u,
	0x119B4BE9u, 0x155A565Eu, 0x18197087u, 0x1CD86D30u, 0x029F3D35u, 0x065E2082u, 0x0B1D065Bu, 0x0FDC1BECu,
	0x3793A651u, 0x3352BBE6u, 0x3E119D3Fu, 0x3AD08088u, 0x2497D08Du, 0x2056CD3Au, 0x2D15EBE3u, 0x29D4F654u,
	0xC5A92679u, 0xC1683BCEu, 0xCC2B1D17u, 0xC8EA00A0u, 0xD6AD50A5u, 0xD26C4D12u, 0xDF2F6BCBu, 0xDBEE767Cu,
	0xE3A1CBC1u, 0xE760D676u, 0xEA23F0AFu, 0xEEE2ED18u, 0xF0A5BD1Du, 0xF464A0AAu, 0xF9278673u, 0xFDE69BC4u,
	0x89B8FD09u, 0x8D79E0BEu, 0x803AC667u, 0x84FBDBD0u, 0x9ABC8BD5u, 0x9E7D9662u, 0x933EB0BBu, 0x97FFAD0Cu,
	0xAFB010B1u, 0xAB710D06u, 0xA6322BDFu, 0xA2F33668u, 0xBCB4666Du, 0xB8757BDAu, 0xB5365D03u, 0xB1F740B4u,
};

AvStreamTypeNameEntry g_avStreamTypeNames[22] = {
	{ BE_AV_STREAM_INVALID,          "INVALID" },
	{ BE_AV_STREAM_MPEG1_VIDEO,      "MPEG1 Video" },
	{ BE_AV_STREAM_MPEG2_VIDEO,      "MPEG2 Video" },
	{ BE_AV_STREAM_MPEG4_VIDEO,      "MPEG4 Video" },
	{ BE_AV_STREAM_AVC_VIDEO,        "H.264/AVC Video" },
	{ BE_AV_STREAM_AC3_AUDIO,        "AC3 Audio" },
	{ BE_AV_STREAM_LPCM_AUDIO,       "LPCM Audio" },
	{ BE_AV_STREAM_ATRAC3PLUS_AUDIO, "ATRAC3plus Audio" },
	{ BE_AV_STREAM_ATRAC9_AUDIO,     "ATRAC9 Audio" },
	{ BE_AV_STREAM_MPEG1_AUDIO,      "MPEG1 Audio" },
	{ BE_AV_STREAM_MPEG2_AUDIO,      "MPEG2 Audio" },
	{ BE_AV_STREAM_MP3_AUDIO,        "MPEG Layer3 Audio" },
	{ BE_AV_STREAM_AAC_AUDIO,        "AAC Audio" },
	{ BE_AV_STREAM_DTS_AUDIO,        "DTS" },
	{ BE_AV_STREAM_DDP_AUDIO,        "Dolby Digital Plus" },
	{ BE_AV_STREAM_CELP_AUDIO,       "CELP Audio" },
	{ BE_AV_STREAM_DTS_HD_LBR,       "DTS HD lbr" },
	{ BE_AV_STREAM_PRIVATE_SECTION,  "Private section" },
	{ BE_AV_STREAM_PRIVATE_DATA,     "Private data" },
	{ BE_AV_STREAM_PES_DATA,         "PES Data" },
	{ BE_AV_STREAM_MPEG4_SUBTITLE,   "MPEG4 Subtitle" },
	{ BE_AV_STREAM_CLOSED_CAPTION,   "Closed Caption" },
};

struct SectionHeaderBits {
	uint8_t tableId;
	uint8_t sectionLenHi : 4;
	uint8_t reserved : 2;
	uint8_t zero : 1;
	uint8_t syntaxIndicator : 1;
	uint8_t sectionLenLo;
};

struct SectionExtHeaderBits {
	uint32_t currentNext : 1;
	uint32_t versionNumber : 5;
	uint32_t reserved : 2;
	uint32_t sectionNumber : 8;
	uint32_t lastSectionNumber : 8;
	uint32_t pad : 8;
};

static inline uint32_t readSecBe16(const uint8_t *p)
{
	uint16_t raw = *(const uint16_t *)p;
	return ((uint32_t)(raw & 0xFFu) << 8) | (raw >> 8);
}

static inline uint32_t readSecBe32(const uint8_t *p)
{
	uint32_t raw = *(const uint32_t *)p;
	return ((uint32_t)(uint8_t)raw << 24)
		| ((uint32_t)(uint8_t)(raw >> 8) << 16)
		| ((uint32_t)(uint8_t)(raw >> 16) << 8)
		| (uint32_t)(uint8_t)(raw >> 24);
}

static inline int32_t readSectionLength(const uint8_t *hdr)
{
	return (int32_t)(((uint32_t)(hdr[1] & 0x0Fu) << 8) | (uint32_t)hdr[2]);
}

static inline int32_t readExtVersion(const uint8_t *extHdr)
{
	const SectionExtHeaderBits *bits = (const SectionExtHeaderBits *)extHdr;
	return (int32_t)bits->versionNumber;
}

static PidInfo *allocPmtPidSlot(TsDemuxer *demuxer)
{
	if (demuxer->m_pmtPidInfos == 0 || demuxer->m_maxPmtCount <= 0)
		return 0;
	for (int32_t i = 0; i < demuxer->m_maxPmtCount; ++i) {
		if (demuxer->m_pmtPidInfos[i].pid == -1) {
			TsPayloadParser *parser = demuxer->m_freePmtList;
			demuxer->m_pmtPidInfos[i].parser = parser;
			demuxer->m_freePmtList = parser->m_nextFree;
			return &demuxer->m_pmtPidInfos[i];
		}
	}
	return 0;
}

static PidInfo *allocPesPidSlot(TsDemuxer *demuxer)
{
	if (demuxer->m_pesPidInfos == 0 || demuxer->m_maxPesCount <= 0)
		return 0;
	for (int32_t i = 0; i < demuxer->m_maxPesCount; ++i) {
		if (demuxer->m_pesPidInfos[i].pid == -1) {
			TsPayloadParser *parser = demuxer->m_freePesList;
			demuxer->m_pesPidInfos[i].parser = parser;
			demuxer->m_freePesList = parser->m_nextFree;
			return &demuxer->m_pesPidInfos[i];
		}
	}
	return 0;
}

uint32_t beCalcMpegCrc32(uint32_t initCrc, const uint8_t *data, int32_t length)
{
	uint32_t crc = initCrc;
	for (int32_t i = 0; i < length; ++i) {
		uint8_t idx = (uint8_t)((crc >> 24) ^ data[i]);
		crc = (crc << 8) ^ g_mpegCrc32Table[idx];
	}
	return crc;
}

/* --------------------------------------------------------------------------
 * SectionParser
 * -------------------------------------------------------------------------- */

int SectionParser::Parse(BitReader *reader, TsDemuxer *demuxer, int32_t payloadLen)
{
	if (reader == 0 || demuxer == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	m_bitReader = reader;
	m_demuxer = demuxer;

	if (m_storage.m_demuxer == 0)
		m_storage.Init(demuxer);

	if (m_state == 1) {
		m_memBlock = demuxer->m_memBlockPool.GetBlock();
		if (m_memBlock == 0)
			return BE_MP2SYS_ERR_MEMPOOL_EMPTY;
	}

	if (demuxer->m_memBlockPool.GetBlockData(m_memBlock) == 0)
		return BE_MP2SYS_ERR_MEMPOOL_EMPTY;
	uint8_t *secBuf = demuxer->m_memBlockPool.GetBlockData(m_memBlock);

	int ret = 0;
	int32_t consumed = 0;
	while (consumed < payloadLen) {
		if (m_state == 1) {
			if (GetParserType() == BE_PARSER_TYPE_PAT) {
				if (m_demuxer->m_eventCallback != 0)
					m_demuxer->m_eventCallback->onDemuxerEvent(4, m_demuxer->m_tsParser.m_currentPidInfo);
			}
			if (GetParserType() == BE_PARSER_TYPE_PMT) {
				if (m_demuxer->m_eventCallback != 0)
					m_demuxer->m_eventCallback->onDemuxerEvent(5, m_demuxer->m_tsParser.m_currentPidInfo);
			}

			m_accumulatedBytes = 0;
			const uint8_t *ptrField = m_bitReader->currentPtr();
			uint8_t ptrVal = *ptrField;
			ret = m_bitReader->advance(1);
			if (ret != 0)
				return ret;

			m_pointerField = ptrVal;
			++consumed;
			m_pointerSkipped = 0;
			m_state = 2;
		} else if (m_state == 2) {
			if (m_pointerField != m_pointerSkipped) {
				int32_t toSkip = (int32_t)m_pointerField - (int32_t)m_pointerSkipped;
				if (toSkip > payloadLen - consumed)
					toSkip = payloadLen - consumed;
				ret = m_bitReader->advance(toSkip);
				if (ret != 0)
					return ret;
				m_pointerSkipped = (uint8_t)(m_pointerSkipped + toSkip);
				consumed += toSkip;
			} else {
				if (m_accumulatedBytes < 3) {
					int32_t toCopy = 3 - m_accumulatedBytes;
					if (toCopy > payloadLen - consumed)
						toCopy = payloadLen - consumed;
					beMemcpy(secBuf + m_accumulatedBytes, m_bitReader->currentPtr(), (SceSize)toCopy);
					ret = m_bitReader->advance(toCopy);
					if (ret != 0)
						return ret;
					consumed += toCopy;
					m_accumulatedBytes += toCopy;
				}
				if (m_accumulatedBytes == 3) {
					m_sectionSize = readSectionLength(secBuf) + 3;
					m_state = 3;
				}
			}
		} else if (m_state == 3) {
			if (m_accumulatedBytes < m_sectionSize) {
				int32_t toCopy = m_sectionSize - m_accumulatedBytes;
				if (toCopy > payloadLen - consumed)
					toCopy = payloadLen - consumed;
				beMemcpy(secBuf + m_accumulatedBytes, m_bitReader->currentPtr(), (SceSize)toCopy);
				ret = m_bitReader->advance(toCopy);
				if (ret != 0)
					return ret;
				consumed += toCopy;
				m_accumulatedBytes += toCopy;
			}
			if (m_accumulatedBytes == m_sectionSize) {
				ret = ParseSection();
				m_accumulatedBytes = 0;
				m_state = 4;
				if (ret != 0)
					return ret;
			}
		} else if (m_state == 4) {
			ret = m_bitReader->advance(payloadLen - consumed);
			consumed = payloadLen;
		} else {
			beLogPrintf(3, "[ERROR] %s : unknown SectionParser state: %d\n", "Parse", m_state);
			return BE_MP2SYS_ERR_UNKNOWN_PARSER_STATE;
		}
	}
	return ret;
}

void SectionParser::logSectionHeader()
{
	beLogPrintf(1, "\t table_id: %d\n", (int32_t)((const SectionHeaderBits *)m_sectionHeader)->tableId);
	beLogPrintf(1, "\t section_syntax_indicator: %d\n", (int32_t)((const SectionHeaderBits *)m_sectionHeader)->syntaxIndicator);
	beLogPrintf(1, "\t '0': %d\n", (int32_t)((const SectionHeaderBits *)m_sectionHeader)->zero);
	beLogPrintf(1, "\t reserved: %d\n", (int32_t)((const SectionHeaderBits *)m_sectionHeader)->reserved);
	beLogPrintf(1, "\t section_length: %d\n",
		((int32_t)((const SectionHeaderBits *)m_sectionHeader)->sectionLenHi << 8)
		| ((const SectionHeaderBits *)m_sectionHeader)->sectionLenLo);
}

int SectionParser::parseSectionHeader(BitReader *reader)
{
	int32_t size = reader->m_size;
	int32_t pos = reader->m_pos;
	m_sectionHeader = (pos < size) ? (reader->m_buffer + pos) : 0;
	int ret = BE_MP2SYS_ERR_BUFFER_END;
	if (pos + 3 <= size) {
		reader->m_pos = pos + 3;
		ret = BE_MP2SYS_OK;
	}
	if (SceBemp2sys_CF2FD310() == 1)
		logSectionHeader();
	return ret;
}

void SectionParser::logSectionExtHeader()
{
	beLogPrintf(1, "\t reserved: %d\n", (int32_t)((const SectionExtHeaderBits *)m_sectionExtHeader)->versionNumber);
	beLogPrintf(1, "\t version_number: %d\n", (int32_t)((const SectionExtHeaderBits *)m_sectionExtHeader)->versionNumber);
	beLogPrintf(1, "\t current_next_indicator: %d\n", (int32_t)((const SectionExtHeaderBits *)m_sectionExtHeader)->currentNext);
	beLogPrintf(1, "\t section_number: %d\n", (int32_t)((const SectionExtHeaderBits *)m_sectionExtHeader)->sectionNumber);
	beLogPrintf(1, "\t last_section_number: %d\n", (int32_t)((const SectionExtHeaderBits *)m_sectionExtHeader)->lastSectionNumber);
}

int SectionParser::parseSectionExtHeader(BitReader *reader)
{
	int32_t size = reader->m_size;
	int32_t pos = reader->m_pos;
	m_sectionExtHeader = (pos < size) ? (reader->m_buffer + pos) : 0;
	int ret = BE_MP2SYS_ERR_BUFFER_END;
	if (pos + 3 <= size) {
		reader->m_pos = pos + 3;
		ret = BE_MP2SYS_OK;
	}
	if (SceBemp2sys_CF2FD310() == 1)
		logSectionExtHeader();
	return ret;
}

int SectionParser::notifySectionAndReleaseBlock(int32_t status, const uint8_t *buf)
{
	if (m_demuxer->m_streamInfo != 0) {
		if (m_parserType == BE_PARSER_TYPE_PAT || m_parserType == BE_PARSER_TYPE_PMT) {
			uint16_t curPid = (uint16_t)m_demuxer->m_tsParser.m_currentPidInfo->pid;
			m_demuxer->m_streamInfo->onSection(curPid, buf, m_sectionSize, status, &m_storage);
		}
		IStreamInfoSink *sink = 0;
		if (m_demuxer->m_streamInfo != 0) {
			uint16_t curPid = (uint16_t)m_demuxer->m_tsParser.m_currentPidInfo->pid;
			sink = static_cast<IStreamInfoSink *>(
				m_demuxer->m_streamInfo->FindPidFilter(curPid, 1));
		}
		if (sink != 0) {
			uint16_t curPid = (uint16_t)m_demuxer->m_tsParser.m_currentPidInfo->pid;
			sink->onSection(curPid, buf, m_sectionSize, status, &m_storage);
		}
	}

	if (m_callback != 0) {
		m_callback->onSectionParsed(m_demuxer->m_userData,
			m_demuxer->m_tsParser.m_currentPidInfo->pid,
			buf, m_sectionSize, status, &m_storage);
	}

	if (m_memBlock != 0 && status != 0) {
		m_demuxer->m_memBlockPool.ReturnBlock(m_memBlock);
		m_memBlock = 0;
	}
	return status;
}

/* --------------------------------------------------------------------------
 * PatSectionParser
 * -------------------------------------------------------------------------- */

void PatSectionParser::logProgramEntry(const uint8_t *entry, int32_t index)
{
	uint32_t progNum = readSecBe16(entry);
	uint32_t pid = ((uint32_t)(entry[2] & 0x1Fu) << 8) | (uint32_t)entry[3];
	beLogPrintf(1, "\t\t --- Program [%d] ---\n", index);
	beLogPrintf(1, "\t\t  program_number: %u\n", progNum);
	beLogPrintf(1, "\t\t  PID: %u\n\n", pid);
}

int PatSectionParser::ParseTable()
{
	int32_t lastSecNum = (int32_t)m_sectionExtHeader[2];
	int32_t totalSections = lastSecNum + 1;
	int32_t version = readExtVersion(m_sectionExtHeader);
	BitReader reader;

	for (PidInfo *cur = m_demuxer->m_patPidInfo.childList; cur != 0; cur = cur->nextSibling) {
		cur->pendingPid = -1;
		cur->needsUpdate = 1;
	}

	m_programCount = 0;
	int ret = 0;

	for (int32_t secIdx = 0; secIdx < totalSections; ++secIdx) {
		int32_t secLen = 0;
		uint8_t *secData = m_storage.GetSectionBufAccumulated(&version, (uint32_t)secIdx, &secLen);
		if (secData == 0)
			return BE_MP2SYS_ERR_NO_SECTION_BUF;

		reader.setBuffer(secData, secLen);
		beLogPrintf(1, "\n\t******** PAT Table [%d/%d]********\n", secIdx, lastSecNum);

		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionHeader();
		if (ret != 0)
			return ret;

		const uint8_t *tsIdPtr = reader.currentPtr();
		uint32_t tsId = readSecBe16(tsIdPtr);
		ret = reader.advance(2);
		if (ret != 0)
			return ret;
		beLogPrintf(1, "\t transport_stream_id: %d\n", tsId);

		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionExtHeader();
		if (ret != 0)
			return ret;

		int32_t bodyBytes = secLen - 12;
		if (m_demuxer->m_streamInfo != 0)
			m_demuxer->m_streamInfo->StartNewPATParse(bodyBytes / 4);

		for (int32_t offset = 0; offset < bodyBytes; offset += 4) {
			const uint8_t *entry = reader.currentPtr();
			ret = reader.advance(4);
			if (ret != 0)
				return ret;

			uint32_t progNum = readSecBe16(entry);
			uint16_t pmtPid = (uint16_t)(((uint32_t)(entry[2] & 0x1Fu) << 8) | (uint32_t)entry[3]);

			if (progNum != 0) {
				PidInfo *found = m_demuxer->m_patPidInfo.childList;
				while (found != 0) {
					if (found->programNumber == (int32_t)progNum)
						break;
					found = found->nextSibling;
				}
				if (found != 0) {
					found->pendingPid = (int16_t)pmtPid;
				} else {
					PidInfo *slot = allocPmtPidSlot(m_demuxer);
					if (slot == 0)
						return BE_MP2SYS_ERR_PMT_POOL_FULL;
					slot->pendingPid = (int16_t)pmtPid;
					slot->pid = (int16_t)pmtPid;
					slot->programNumber = (int32_t)progNum;
					slot->nextSibling = m_demuxer->m_patPidInfo.childList;
					m_demuxer->m_patPidInfo.childList = slot;
				}
				if (m_demuxer->m_streamInfo != 0)
					m_demuxer->m_streamInfo->AddProgram(pmtPid, (uint16_t)progNum);
				++m_programCount;
			}

			if (SceBemp2sys_CF2FD310() == 1)
				logProgramEntry(entry, offset >> 2);
		}

		beLogPrintf(1, "\n\t*****************************\n");
	}

	m_demuxer->cleanupRemovedPrograms();
	m_storage.ChangeCurrentVersion(version);
	if (m_demuxer->m_eventCallback != 0)
		m_demuxer->m_eventCallback->onDemuxerEvent(1, m_demuxer->m_tsParser.m_currentPidInfo);
	if (m_demuxer->m_streamInfo != 0)
		m_demuxer->m_streamInfo->FinishPATParse();
	return ret;
}

int PatSectionParser::ParseSection()
{
	BitReader reader;
	uint8_t *secBuf = m_demuxer->m_memBlockPool.GetBlockData(m_memBlock);
	reader.setBuffer(secBuf, m_sectionSize);

	beLogPrintf(1, "\n\t******** PAT Section ********\n");
	int32_t prevVersion = m_storage.m_tables[m_storage.m_currentIndex].version;

	int ret = parseSectionHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	if (m_sectionHeader[0] != 0) {
		beLogPrintf(2, "[DEBUG] %s : table_id (0x%02x) mismatch in PAT table!\n",
			"ParseSection", m_sectionHeader[0]);
		return BE_MP2SYS_ERR_TABLE_ID_MISMATCH;
	}

	const uint8_t *tsIdPtr = reader.currentPtr();
	uint32_t tsId = readSecBe16(tsIdPtr);
	if (reader.advance(2) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);
	beLogPrintf(1, "\t transport_stream_id: %d\n", tsId);

	ret = parseSectionExtHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	if (reader.advance(m_sectionSize - 12) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);

	const uint8_t *crcPtr = reader.currentPtr();
	uint32_t expectedCrc = readSecBe32(crcPtr);
	ret = reader.advance(4);
	if (ret == 0) {
		beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
		uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secBuf, m_sectionSize - 4);
		if (expectedCrc != calcCrc)
			ret = BE_MP2SYS_ERR_CRC_MISMATCH;

		beLogPrintf(1, "\n\t*****************************\n");
		m_storage.AccumulateSection(readExtVersion(m_sectionExtHeader),
			m_sectionExtHeader[1], m_sectionExtHeader[2],
			m_memBlock, m_sectionSize, 1);

		if (m_storage.IsAccumulatedComplete() != 0) {
			if (prevVersion != readExtVersion(m_sectionExtHeader) ||
			    m_storage.IsAccumulatedSameAsCurrent() == 0) {
				beLogPrintf(2, "[DEBUG] %s : New PAT coming {version:[%u->%u]}\n",
					"ParseSection", prevVersion, readExtVersion(m_sectionExtHeader));
				ret = ParseTable();
			}
		}
	}

	return notifySectionAndReleaseBlock(ret, secBuf);
}

/* --------------------------------------------------------------------------
 * PmtSectionParser
 * -------------------------------------------------------------------------- */

struct PmtEsHeaderBits {
	uint8_t streamType;
	uint8_t pidHi : 5;
	uint8_t reserved0 : 3;
	uint8_t pidLo;
	uint8_t esInfoLenHi : 4;
	uint8_t reserved1 : 4;
	uint8_t esInfoLenLo;
};

struct PmtProgInfoBits {
	uint8_t pcrPidHi : 5;
	uint8_t reserved0 : 3;
	uint8_t pcrPidLo;
	uint8_t progInfoLenHi : 4;
	uint8_t reserved1 : 4;
	uint8_t progInfoLenLo;
};

void PmtSectionParser::logElementaryStream(const uint8_t *entry, int32_t index)
{
	const PmtEsHeaderBits *bits = (const PmtEsHeaderBits *)entry;
	uint32_t elemPid = ((uint32_t)(bits->pidHi & 0x1F) << 8) | (uint32_t)bits->pidLo;
	uint32_t esInfoLen = ((uint32_t)(bits->esInfoLenHi & 0x0F) << 8) | (uint32_t)bits->esInfoLenLo;

	beLogPrintf(1, "\t\t --- Elementary stream [%d] ---\n", index);
	beLogPrintf(1, "\t\t  stream_type: 0x%02llx\n", (uint32_t)bits->streamType);
	beLogPrintf(1, "\t\t  reserved: %lld\n", (uint32_t)(bits->reserved0 & 7));
	beLogPrintf(1, "\t\t  elementary_PID: %u\n", elemPid);
	beLogPrintf(1, "\t\t  reserved: %lld\n", (uint32_t)(bits->reserved1 & 0xF));
	beLogPrintf(1, "\t\t  ES_info_length: %u\n", esInfoLen);
	beLogPrintf(1, "\t\t eleminfos_bin : ");
	const uint8_t *p = entry + 5;
	for (uint32_t i = 0; i < esInfoLen; ++i)
		beLogPrintf(1, "[0x%02x]", *p++);
	beLogPrintf(1, "\n\n");
}

void PmtSectionParser::logProgramInfo(const uint8_t *header)
{
	const PmtProgInfoBits *bits = (const PmtProgInfoBits *)header;
	uint32_t pcrPid = ((uint32_t)(bits->pcrPidHi & 0x1F) << 8) | (uint32_t)bits->pcrPidLo;
	uint32_t progInfoLen = ((uint32_t)(bits->progInfoLenHi & 0x0F) << 8) | (uint32_t)bits->progInfoLenLo;

	beLogPrintf(1, "\t reserved: %d\n", (int32_t)(bits->reserved0 & 7));
	beLogPrintf(1, "\t PCR_PID: %u\n", pcrPid);
	beLogPrintf(1, "\t reserved: %d\n", (int32_t)(bits->reserved1 & 0xF));
	beLogPrintf(1, "\t program_info_length: %u\n", progInfoLen);
	beLogPrintf(1, "\t programinfos_bin : ");
	const uint8_t *p = header + 4;
	for (uint32_t i = 0; i < progInfoLen; ++i)
		beLogPrintf(1, "[0x%02x]", *p++);
	beLogPrintf(1, "\n");
}

static const char *lookupAvStreamTypeName(int32_t avStreamType)
{
	for (uint32_t i = 0; i < 22u; ++i) {
		if (g_avStreamTypeNames[i].avStreamType == avStreamType)
			return g_avStreamTypeNames[i].name;
	}
	return "Unknown";
}

int PmtSectionParser::ParseTable()
{
	int32_t lastSecNum = (int32_t)m_sectionExtHeader[2];
	int32_t totalSections = lastSecNum + 1;
	int32_t version = readExtVersion(m_sectionExtHeader);
	BitReader reader;

	PidInfo *pmtPidInfo = m_demuxer->m_tsParser.m_currentPidInfo;
	for (PidInfo *cur = pmtPidInfo->childList; cur != 0; cur = cur->nextSibling) {
		cur->pendingPid = -1;
		cur->needsUpdate = 1;
	}

	m_esCount = 0;
	int ret = 0;

	for (int32_t secIdx = 0; secIdx < totalSections; ++secIdx) {
		int32_t secLen = 0;
		uint8_t *secData = m_storage.GetSectionBufAccumulated(&version, (uint32_t)secIdx, &secLen);
		if (secData == 0)
			return BE_MP2SYS_ERR_NO_SECTION_BUF;

		reader.setBuffer(secData, secLen);
		beLogPrintf(1, "\n\t******** PMT Table [%d/%d]********\n", secIdx, lastSecNum);

		const uint8_t *secHdr = reader.currentPtr();
		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionHeader();
		if (ret != 0)
			return ret;

		const uint8_t *progNumPtr = reader.currentPtr();
		uint32_t progNum = readSecBe16(progNumPtr);
		if (reader.advance(2) != 0)
			return BE_MP2SYS_ERR_BUFFER_END;
		beLogPrintf(1, "\t program_number: %d\n", progNum);

		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionExtHeader();
		if (ret != 0)
			return ret;

		const uint8_t *progInfoHdr = reader.currentPtr();
		if (reader.advance(4) != 0)
			return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secData);
		if (SceBemp2sys_CF2FD310() == 1)
			logProgramInfo(progInfoHdr);

		int32_t progInfoLen = (int32_t)(((uint32_t)(progInfoHdr[2] & 0x0Fu) << 8) | (uint32_t)progInfoHdr[3]);
		int32_t sectionLength = readSectionLength(secHdr);
		reader.advance(progInfoLen);

		int32_t esLoopBytes = sectionLength - progInfoLen - 13;
		if (m_demuxer->m_streamInfo != 0) {
			PidInfo *curInfo = m_demuxer->m_tsParser.m_currentPidInfo;
			uint16_t curPmtPid = (uint16_t)(curInfo->needsUpdate ? curInfo->pendingPid : curInfo->pid);
			m_demuxer->m_streamInfo->StartNewPMTParse(curPmtPid, esLoopBytes / 5);
		}

		int32_t esBytesProcessed = 0;
		int32_t esIndex = 0;
		while (esBytesProcessed < esLoopBytes) {
			const uint8_t *esEntry = reader.currentPtr();
			if (reader.advance(5) != 0)
				return BE_MP2SYS_ERR_BUFFER_END;
			if (SceBemp2sys_CF2FD310() == 1)
				logElementaryStream(esEntry, esIndex);

			uint8_t streamType = esEntry[0];
			int32_t avStreamType = m_demuxer->m_streamTypeMap[streamType];
			uint16_t esPid = (uint16_t)(((uint32_t)(esEntry[1] & 0x1Fu) << 8) | (uint32_t)esEntry[2]);
			int32_t esInfoLen = (int32_t)(((uint32_t)(esEntry[3] & 0x0Fu) << 8) | (uint32_t)esEntry[4]);
			int32_t curStorageVer = m_storage.m_tables[m_storage.m_currentIndex].version;
			int32_t extVer = readExtVersion(m_sectionExtHeader);
			int32_t descConsumed = 0;

			if (curStorageVer != extVer || pmtPidInfo->needsUpdate == 1) {
				int isTracked = isVideoAvStreamType(avStreamType)
					|| isAudioAvStreamType(avStreamType)
					|| avStreamType == BE_AV_STREAM_PRIVATE_DATA
					|| avStreamType == BE_AV_STREAM_PES_DATA;
				if (isTracked) {
					PidInfo *esNode = pmtPidInfo->childList;
					while (esNode != 0) {
						if (esNode->pid == (int32_t)esPid)
							break;
						esNode = esNode->nextSibling;
					}
					if (esNode != 0) {
						esNode->pendingPid = (int16_t)esPid;
						esNode->needsUpdate = 0;
					} else {
						esNode = allocPesPidSlot(m_demuxer);
						if (esNode == 0)
							return BE_MP2SYS_ERR_PES_POOL_FULL;
						esNode->pendingPid = (int16_t)esPid;
						esNode->pid = (int16_t)esPid;
						esNode->programNumber = pmtPidInfo->programNumber;
						esNode->nextSibling = pmtPidInfo->childList;
						pmtPidInfo->childList = esNode;
						esNode->needsUpdate = 0;
					}

					esNode->descriptorPtr = reader.currentPtr();
					esNode->descriptorLength = esInfoLen;

					if (avStreamType == BE_AV_STREAM_PRIVATE_DATA) {
						const uint8_t *tagPtr = reader.currentPtr();
						reader.advance(1);
						descConsumed = 1;
						uint8_t tag = *tagPtr;
						if (tag == 0x2B) {
							avStreamType = BE_AV_STREAM_AAC_AUDIO;
						} else if (tag == 0x1C) {
							reader.advance(1);
							const uint8_t *profilePtr = reader.currentPtr();
							descConsumed = 2;
							uint8_t profile = *profilePtr;
							if (profile == 1 ||
							    (profile >= 80 && profile <= 83) ||
							    (profile >= 88 && profile <= 91) ||
							    (profile >= 96 && profile <= 99)) {
								avStreamType = BE_AV_STREAM_AAC_AUDIO;
							}
						}
					}

					int32_t resolvedBefore = avStreamType;
					if (esNode->descriptorLength > 0 && (m_demuxer->m_flags & 1u) != 0) {
						if (m_demuxer->m_eventCallback != 0) {
							m_demuxer->m_eventCallback->onResolveStreamTypeWithDesc(
								streamType, &avStreamType, esNode,
								esNode->descriptorPtr, esNode->descriptorLength, version);
						}
					}
					if (avStreamType == resolvedBefore && m_demuxer->m_eventCallback != 0)
						m_demuxer->m_eventCallback->onResolveStreamType(streamType, &avStreamType);

					PesPacketInfo *pesInfo = getParserPesInfo(esNode->parser);
					pesInfo->streamType = streamType;
					pesInfo->avStreamType = avStreamType;
					++m_esCount;
				}
			}

			if (m_demuxer->m_streamInfo != 0) {
				PidInfo *curInfo = m_demuxer->m_tsParser.m_currentPidInfo;
				uint16_t curPmtPid = (uint16_t)(curInfo->needsUpdate ? curInfo->pendingPid : curInfo->pid);
				m_demuxer->m_streamInfo->AddElementaryStreamInfo(curPmtPid, esPid, streamType, avStreamType);
			}

			reader.advance(esInfoLen - descConsumed);
			esBytesProcessed += esInfoLen + 5;
			++esIndex;
		}

		const uint8_t *crcPtr = reader.currentPtr();
		uint32_t expectedCrc = readSecBe32(crcPtr);
		ret = reader.advance(4);
		if (ret != 0)
			return ret;

		beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
		uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secData, secLen - 4);
		if (expectedCrc != calcCrc)
			ret = BE_MP2SYS_ERR_CRC_MISMATCH;

		beLogPrintf(1, "\n\t*****************************\n");

		m_pcrPid = (int16_t)(((uint32_t)(progInfoHdr[0] & 0x1Fu) << 8) | (uint32_t)progInfoHdr[1]);
		if (m_demuxer->findPidInfo(m_pcrPid) == 0) {
			PidInfo *pcrSlot = allocPesPidSlot(m_demuxer);
			if (pcrSlot == 0)
				return BE_MP2SYS_ERR_PES_POOL_FULL;
			pcrSlot->pendingPid = m_pcrPid;
			pcrSlot->pid = m_pcrPid;
			pcrSlot->programNumber = pmtPidInfo->programNumber;
			pcrSlot->nextSibling = pmtPidInfo->childList;
			pmtPidInfo->childList = pcrSlot;
			pcrSlot->needsUpdate = 0;
		}
	}

	m_demuxer->cleanupRemovedStreams(pmtPidInfo);
	pmtPidInfo->pid = pmtPidInfo->pendingPid;
	pmtPidInfo->needsUpdate = 0;

	if (pmtPidInfo != 0 && pmtPidInfo->parser != 0 &&
	    pmtPidInfo->parser->GetParserType() == BE_PARSER_TYPE_PMT) {
		beLogPrintf(3, "==== MapInfo [%d:(PID)%d] ====\n", pmtPidInfo->programNumber, pmtPidInfo->pid);
		for (PidInfo *es = pmtPidInfo->childList; es != 0; es = es->nextSibling) {
			if (es->parser != 0 && es->parser->GetParserType() == BE_PARSER_TYPE_PES) {
				PesPacketInfo *pesInfo = getParserPesInfo(es->parser);
				beLogPrintf(3, "PID[%d], streamType = [0x%02x], AvStreamType=[%d:%s]\n",
					es->pid, pesInfo->streamType, pesInfo->avStreamType,
					lookupAvStreamTypeName(pesInfo->avStreamType));
			}
		}
		beLogPrintf(3, "==========================\n");
	}

	m_storage.ChangeCurrentVersion(version);
	if (m_demuxer->m_eventCallback != 0)
		m_demuxer->m_eventCallback->onDemuxerEvent(2, m_demuxer->m_tsParser.m_currentPidInfo);
	if (m_demuxer->m_streamInfo != 0) {
		PidInfo *curInfo = m_demuxer->m_tsParser.m_currentPidInfo;
		uint16_t curPmtPid = (uint16_t)(curInfo->needsUpdate ? curInfo->pendingPid : curInfo->pid);
		m_demuxer->m_streamInfo->FinishPMTParse(curPmtPid);
	}
	return ret;
}

int PmtSectionParser::ParseSection()
{
	uint8_t *secBuf = m_demuxer->m_memBlockPool.GetBlockData(m_memBlock);
	BitReader reader;
	reader.setBuffer(secBuf, m_sectionSize);

	beLogPrintf(1, "\n\t******** PMT Section ********\n");
	int32_t prevVersion = m_storage.m_tables[m_storage.m_currentIndex].version;

	int ret = parseSectionHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	if (m_sectionHeader[0] != 2) {
		beLogPrintf(2, "[DEBUG] %s : table_id (0x%02x) mismatch in PMT table!\n",
			"ParseSection", m_sectionHeader[0]);
		return BE_MP2SYS_ERR_TABLE_ID_MISMATCH;
	}

	const uint8_t *progNumPtr = reader.currentPtr();
	uint32_t progNum = readSecBe16(progNumPtr);
	if (reader.advance(2) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);
	beLogPrintf(1, "\t program_number: %d\n", progNum);

	PidInfo *curPmtInfo = m_demuxer->m_tsParser.m_currentPidInfo;
	if ((uint32_t)curPmtInfo->programNumber != progNum) {
		beLogPrintf(3, "[ERROR] %s : program number mismatch!![%d][%d]\n",
			"ParseSection", curPmtInfo->programNumber, progNum);
		return BE_MP2SYS_ERR_PROGRAM_MISMATCH;
	}

	ret = parseSectionExtHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	const uint8_t *progInfoHdr = reader.currentPtr();
	if (reader.advance(4) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);
	if (SceBemp2sys_CF2FD310() == 1)
		logProgramInfo(progInfoHdr);

	int32_t progInfoLen = (int32_t)(((uint32_t)(progInfoHdr[2] & 0x0Fu) << 8) | (uint32_t)progInfoHdr[3]);
	int32_t sectionLength = readSectionLength(m_sectionHeader);
	reader.advance(progInfoLen);

	if (reader.advance(sectionLength - progInfoLen - 13) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);

	const uint8_t *crcPtr = reader.currentPtr();
	uint32_t expectedCrc = readSecBe32(crcPtr);
	ret = reader.advance(4);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
	uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secBuf, m_sectionSize - 4);
	if (expectedCrc != calcCrc)
		ret = BE_MP2SYS_ERR_CRC_MISMATCH;

	beLogPrintf(1, "\n\t*****************************\n");
	m_storage.AccumulateSection(readExtVersion(m_sectionExtHeader),
		m_sectionExtHeader[1], m_sectionExtHeader[2],
		m_memBlock, m_sectionSize, 1);

	if (m_storage.IsAccumulatedComplete() != 0) {
		if (prevVersion != readExtVersion(m_sectionExtHeader) ||
		    curPmtInfo->needsUpdate == 1 ||
		    m_storage.IsAccumulatedSameAsCurrent() == 0) {
			curPmtInfo->needsUpdate = 1;
			ret = ParseTable();
		}
	}

	return notifySectionAndReleaseBlock(ret, secBuf);
}

/* --------------------------------------------------------------------------
 * CatSectionParser
 * -------------------------------------------------------------------------- */

int CatSectionParser::ParseTable()
{
	int32_t lastSecNum = (int32_t)m_sectionExtHeader[2];
	int32_t totalSections = lastSecNum + 1;
	int32_t version = readExtVersion(m_sectionExtHeader);
	BitReader reader;

	int ret = 0;
	for (int32_t secIdx = 0; secIdx < totalSections; ++secIdx) {
		int32_t secLen = 0;
		uint8_t *secData = m_storage.GetSectionBufAccumulated(&version, (uint32_t)secIdx, &secLen);
		if (secData == 0)
			return BE_MP2SYS_ERR_NO_SECTION_BUF;

		reader.setBuffer(secData, secLen);
		beLogPrintf(1, "\n\t******** CA Table [%d/%d]********\n", secIdx, lastSecNum);

		const uint8_t *secHdr = reader.currentPtr();
		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionHeader();
		if (ret != 0)
			return ret;

		if (reader.advance(2) != 0)
			return BE_MP2SYS_ERR_BUFFER_END;

		ret = reader.advance(3);
		if (SceBemp2sys_CF2FD310() == 1)
			logSectionExtHeader();
		if (ret != 0)
			return ret;

		int32_t secLength = readSectionLength(secHdr);
		if (secLength > 9) {
			do {
				const uint8_t *descPtr = reader.currentPtr();
				if (reader.advance(6) != 0)
					return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secData);
				if (descPtr[0] != 0) {
					beLogPrintf(2, "[DEBUG] %s : bad descriptor in CA section!!\n", "ParseTable");
					return BE_MP2SYS_ERR_BAD_CA_DESCRIPTOR;
				}
				if (reader.advance((int32_t)descPtr[1] - 4) != 0)
					return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secData);
			} while (secLength - 9 > 0);
		}

		const uint8_t *crcPtr = reader.currentPtr();
		uint32_t expectedCrc = readSecBe32(crcPtr);
		ret = reader.advance(4);
		if (ret != 0)
			return ret;

		beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
		uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secData, secLen - 4);
		if (expectedCrc != calcCrc)
			ret = BE_MP2SYS_ERR_CRC_MISMATCH;

		beLogPrintf(1, "\n\t*****************************\n");
	}

	m_storage.ChangeCurrentVersion(version);
	if (m_demuxer->m_eventCallback != 0)
		m_demuxer->m_eventCallback->onDemuxerEvent(6, m_demuxer->m_tsParser.m_currentPidInfo);
	return ret;
}

int CatSectionParser::ParseSection()
{
	uint8_t *secBuf = m_demuxer->m_memBlockPool.GetBlockData(m_memBlock);
	BitReader reader;
	reader.setBuffer(secBuf, m_sectionSize);

	beLogPrintf(1, "\n\t******** CA  Section ********\n");
	int32_t prevVersion = m_storage.m_tables[m_storage.m_currentIndex].version;

	int ret = parseSectionHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	if (m_sectionHeader[0] != 1) {
		beLogPrintf(2, "[DEBUG] %s : table_id (0x%02x) mismatch in CA table!\n",
			"ParseSection", m_sectionHeader[0]);
		return BE_MP2SYS_ERR_TABLE_ID_MISMATCH;
	}

	if (reader.advance(2) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);

	ret = parseSectionExtHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	int32_t secLength = readSectionLength(m_sectionHeader);
	if (reader.advance(secLength - 9) != 0)
		return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);

	const uint8_t *crcPtr = reader.currentPtr();
	uint32_t expectedCrc = readSecBe32(crcPtr);
	ret = reader.advance(4);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
	int32_t extVer = readExtVersion(m_sectionExtHeader);
	uint8_t secNum = m_sectionExtHeader[1];
	uint8_t lastSecNum = m_sectionExtHeader[2];
	uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secBuf, m_sectionSize - 4);
	if (expectedCrc != calcCrc)
		ret = BE_MP2SYS_ERR_CRC_MISMATCH;

	m_storage.AccumulateSection(extVer, secNum, lastSecNum, m_memBlock, m_sectionSize, 1);
	if (m_storage.IsAccumulatedComplete() != 0) {
		if (prevVersion != readExtVersion(m_sectionExtHeader) ||
		    m_storage.IsAccumulatedSameAsCurrent() == 0) {
			ParseTable();
		}
	}

	beLogPrintf(1, "\n\t*****************************\n");
	return notifySectionAndReleaseBlock(ret, secBuf);
}

/* --------------------------------------------------------------------------
 * PrivateSectionParser
 * -------------------------------------------------------------------------- */

int PrivateSectionParser::commitPrivateVersion()
{
	int32_t ver;
	if ((m_sectionHeader[1] & 0x80) != 0)
		ver = readExtVersion(m_sectionExtHeader);
	else
		ver = (m_storage.m_tables[m_storage.m_currentIndex].version + 1) & 0x1F;
	m_storage.ChangeCurrentVersion(ver);
	return 0;
}

int PrivateSectionParser::ParseSection()
{
	uint8_t *secBuf = m_demuxer->m_memBlockPool.GetBlockData(m_memBlock);
	BitReader reader;
	reader.setBuffer(secBuf, m_sectionSize);

	beLogPrintf(1, "\n\t******* Private Section *****\n");
	int32_t prevVersion = m_storage.m_tables[m_storage.m_currentIndex].version;

	int ret = parseSectionHeader(&reader);
	if (ret != 0)
		return notifySectionAndReleaseBlock(ret, secBuf);

	if ((m_sectionHeader[1] & 0x80) == 0) {
		reader.advance(readSectionLength(m_sectionHeader));
		m_storage.SetSection((prevVersion + 1) & 0x1F, 0, 0, m_memBlock, m_sectionSize, 0);
		ret = commitPrivateVersion();
	} else {
		const uint8_t *extIdPtr = reader.currentPtr();
		uint32_t tableIdExt = readSecBe16(extIdPtr);
		if (reader.advance(2) != 0)
			return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);
		beLogPrintf(1, "\t table_id_extension: %d\n", tableIdExt);

		ret = parseSectionExtHeader(&reader);
		if (ret != 0)
			return notifySectionAndReleaseBlock(ret, secBuf);

		int32_t secLength = readSectionLength(m_sectionHeader);
		if (reader.advance(secLength - 9) != 0)
			return notifySectionAndReleaseBlock(BE_MP2SYS_ERR_BUFFER_END, secBuf);

		const uint8_t *crcPtr = reader.currentPtr();
		uint32_t expectedCrc = readSecBe32(crcPtr);
		ret = reader.advance(4);
		if (ret != 0)
			return notifySectionAndReleaseBlock(ret, secBuf);

		beLogPrintf(1, "\t CRC: 0x%08x\n", expectedCrc);
		int32_t extVer = readExtVersion(m_sectionExtHeader);
		uint8_t secNum = m_sectionExtHeader[1];
		uint8_t lastSecNum = m_sectionExtHeader[2];
		uint32_t calcCrc = beCalcMpegCrc32(0xFFFFFFFFu, secBuf, m_sectionSize - 4);
		if (expectedCrc != calcCrc)
			ret = BE_MP2SYS_ERR_CRC_MISMATCH;

		m_storage.AccumulateSection(extVer, secNum, lastSecNum, m_memBlock, m_sectionSize, 1);
		if (m_storage.IsAccumulatedComplete() != 0) {
			if (prevVersion != readExtVersion(m_sectionExtHeader) ||
			    m_storage.IsAccumulatedSameAsCurrent() == 0) {
				ret = commitPrivateVersion();
			}
		}
	}

	beLogPrintf(1, "\n\t*****************************\n");
	return notifySectionAndReleaseBlock(ret, secBuf);
}
