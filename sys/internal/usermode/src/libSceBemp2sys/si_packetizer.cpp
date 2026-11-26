#include "be_mp2sys_internal.h"

SiPacketizer::SiPacketizer()
{
	m_initialized = 0;
	m_currentNext = 0;
	m_versionNumber = 0;
	m_privateIndicator = 0;
	m_sectionSyntaxIndicator = 0;
	m_tableId = 0;
	m_lastSectionNumber = 0;
	m_sectionNumber = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_onCheckSection = 0;
	m_onSectionDone = 0;
	m_state = 0;
	m_curPos = 0;
	m_tableIdExtension = 0;
	beMemset(m_sectionBuf, 0, 1024);
}

void SiPacketizer::MakeSectionHeader()
{
	m_sectionBuf[m_curPos++] = m_tableId;
	m_sectionBuf[m_curPos++] = (uint8_t)(((m_sectionSyntaxIndicator & 1u) << 7) |
		((m_privateIndicator & 1u) << 6) | 0x30u);
	m_sectionBuf[m_curPos++] = 0x00;

	if (m_sectionSyntaxIndicator != 0) {
		m_sectionBuf[m_curPos++] = (uint8_t)(m_tableIdExtension >> 8);
		m_sectionBuf[m_curPos++] = (uint8_t)m_tableIdExtension;
		m_sectionBuf[m_curPos++] = (uint8_t)((m_currentNext & 1u) | ((m_versionNumber & 0x1Fu) << 1) | 0xC0u);
		m_sectionBuf[m_curPos++] = m_sectionNumber;
		m_sectionBuf[m_curPos++] = m_lastSectionNumber;
	}

	if (m_onCheckSection != 0) {
		int res = m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
			1020 - m_curPos, 0, 0);
		if (res == 1) {
			beLogPrintf(2, "[DEBUG] %s : Cannot change section - because only header added.\n",
				"MakeSectionHeader");
		}
	}
}

int SiPacketizer::UpdateSectionLengthAndCRC()
{
	if (m_state == 4)
		return BE_MP2SYS_OK;

	if (m_sectionSyntaxIndicator == 1) {
		if (m_curPos < 3 || m_curPos > 4092)
			return BE_MP2SYS_ERR_SECTION_RANGE;

		int32_t secLen = m_curPos + 1;
		m_sectionBuf[2] = (uint8_t)secLen;
		m_sectionBuf[1] = (uint8_t)(m_sectionBuf[1] | ((uint32_t)secLen >> 8));

		uint32_t crc = beCalcMpegCrc32(0xFFFFFFFFu, m_sectionBuf, m_curPos);
		m_sectionBuf[m_curPos++] = (uint8_t)(crc >> 24);
		m_sectionBuf[m_curPos++] = (uint8_t)(crc >> 16);
		m_sectionBuf[m_curPos++] = (uint8_t)(crc >> 8);
		m_sectionBuf[m_curPos++] = (uint8_t)crc;

		int written = m_onWrite(m_userData, m_type, m_sectionBuf, m_curPos);
		if (written != m_curPos) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"UpdateSectionLengthAndCRC", written, m_curPos);
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}

		if (m_onSectionDone != 0)
			m_onSectionDone(m_userData, m_type, m_sectionNumber, m_lastSectionNumber, m_curPos);

		if (m_sectionNumber != m_lastSectionNumber) {
			m_curPos = 0;
			m_sectionNumber += 1;
			MakeSectionHeader();
			m_state = 2;
		} else {
			m_state = 4;
		}
	} else {
		if (m_curPos < 3 || m_curPos > 4096)
			return BE_MP2SYS_ERR_SECTION_RANGE;

		int32_t secLen = m_curPos - 3;
		m_sectionBuf[2] = (uint8_t)secLen;
		m_sectionBuf[1] = (uint8_t)(m_sectionBuf[1] | (uint8_t)secLen);

		int written = m_onWrite(m_userData, m_type, m_sectionBuf, m_curPos);
		if (written != m_curPos) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"UpdateSectionLengthAndCRC", written, m_curPos);
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}

		if (m_onSectionDone != 0)
			m_onSectionDone(m_userData, m_type, m_sectionNumber, m_lastSectionNumber, m_curPos);

		m_state = 4;
	}
	return BE_MP2SYS_OK;
}

int SiPacketizer::Release()
{
	if (m_initialized) {
		if (m_state == 3) {
			UpdateSectionLengthAndCRC();
			m_onClose(m_userData, m_type);
		}
		m_state = 0;
	}
	m_lastSectionNumber = 0;
	m_sectionNumber = 0;
	m_curPos = 0;
	m_versionNumber = 0;
	m_currentNext = 0;
	m_tableId = 0;
	m_sectionSyntaxIndicator = 0;
	m_privateIndicator = 0;
	m_tableIdExtension = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_onCheckSection = 0;
	m_onSectionDone = 0;
	m_initialized = 0;
	return BE_MP2SYS_OK;
}

SiPacketizer::~SiPacketizer()
{
	Release();
}

int SiPacketizer::Init(const void *rawParam)
{
	const SectionPacketizerInitParam *param = (const SectionPacketizerInitParam *)rawParam;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (param->base.type != BE_PACKETIZER_TYPE_SI ||
	    param->base.onOpen == 0 || param->base.onClose == 0 ||
	    param->base.onWrite == 0 || param->base.onOverwrite == 0 ||
	    param->sectionCallbacks.onSectionDone == 0 || param->sectionCallbacks.onCheckSection == 0) {
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	if (m_initialized)
		Release();

	m_state = 0;
	m_curPos = 0;
	m_lastSectionNumber = 0;
	m_sectionNumber = 0;
	m_tableId = 0;
	m_sectionSyntaxIndicator = 0;
	m_privateIndicator = 0;
	m_tableIdExtension = 0;
	m_versionNumber = 0;
	m_currentNext = 0;
	m_initialized = 1;
	beMemset(m_sectionBuf, 0, 1024);

	m_userData = param->base.userData;
	m_type = param->base.type;
	m_onOpen = param->base.onOpen;
	m_onClose = param->base.onClose;
	m_onWrite = param->base.onWrite;
	m_onOverwrite = param->base.onOverwrite;
	setSectionCallbacks(&param->sectionCallbacks);
	return BE_MP2SYS_OK;
}

int SiPacketizer::ProcessSIPayload(const PacketizerPayloadParam *param)
{
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	if (m_state != 2 && m_state != 3) {
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessSIPayload", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t maxBytes = (m_sectionSyntaxIndicator == 1) ? 4092u : 4096u;
	int32_t curPos = m_curPos;
	uint32_t consumed = 0;
	int ret = BE_MP2SYS_OK;

	while (consumed < param->size) {
		uint32_t rem = param->size - consumed;
		if ((int32_t)(rem + m_curPos) <= (int32_t)maxBytes) {
			beMemcpy(m_sectionBuf + curPos, param->buffer + consumed, rem);
			consumed += rem;
			curPos += (int32_t)rem;
		} else {
			uint32_t chunk = maxBytes - (uint32_t)m_curPos;
			beMemcpy(m_sectionBuf + curPos, param->buffer + consumed, chunk);
			consumed += chunk;
			int32_t newPos = curPos + (int32_t)chunk;
			int canNext = 0;
			if (m_onCheckSection != 0) {
				canNext = m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
					m_curPos - curPos, param->size - consumed, 1);
			}
			if (canNext == 0) {
				beLogPrintf(2, "[DEBUG] %s : should jump to next section\n", "ProcessSIPayload");
				return BE_MP2SYS_ERR_WRITER_MISMATCH;
			}
			m_curPos = newPos;
			ret = UpdateSectionLengthAndCRC();
			if (ret != BE_MP2SYS_OK || m_state == 4)
				return ret;
			curPos = m_curPos;
			if ((uint32_t)curPos + (param->size - consumed) > maxBytes)
				return BE_MP2SYS_ERR_TOO_LARGE;
			m_state = 3;
		}
	}

	m_curPos = curPos;
	m_state = 3;

	if (m_onCheckSection != 0 &&
	    m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
	        (int32_t)maxBytes - curPos, 0, 1) == 1) {
		return UpdateSectionLengthAndCRC();
	}
	return ret;
}

int SiPacketizer::ProcessSIHeader(const SiHeaderParam *param)
{
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	uint32_t state = m_state;
	switch (state) {
	case 1:
	case 5:
		break;
	case 3:
		UpdateSectionLengthAndCRC();
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		state = 5;
		m_state = 5;
		break;
	default:
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessSIHeader", state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t totalSize = param->totalDataSize;
	uint32_t secCount = totalSize / 1012u;
	uint32_t rem = totalSize % 1012u;
	if ((param->sectionSyntaxIndicator & 1) == 0) {
		if (totalSize > 4093u) {
			beLogPrintf(2, "[DEBUG] %s : too big data size : %u\n", "ProcessSIHeader", totalSize);
			return BE_MP2SYS_ERR_TOO_LARGE;
		}
		secCount = 1;
	} else {
		if (rem == 0 || (1012u - rem) > 4u * secCount)
			++secCount;
		if (secCount > 256u) {
			beLogPrintf(2, "[DEBUG] %s : too big data size : %u\n", "ProcessSIHeader", totalSize);
			return BE_MP2SYS_ERR_INVALID_ARG;
		}
	}

	uint8_t lastSec = (uint8_t)(secCount - 1u);
	if (state != 1) {
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_curPos = 0;
	}

	m_tableId = param->tableId;
	m_sectionSyntaxIndicator = (uint8_t)param->sectionSyntaxIndicator & 1u;
	m_privateIndicator = (uint8_t)param->privateIndicator & 1u;
	m_tableIdExtension = param->tableIdExtension;
	m_versionNumber = (uint8_t)param->versionNumber & 0x1Fu;
	m_sectionNumber = 0;
	m_currentNext = (uint8_t)param->currentNext & 1u;
	m_lastSectionNumber = lastSec;
	MakeSectionHeader();
	m_state = 2;
	return BE_MP2SYS_OK;
}

int SiPacketizer::ProcessPacketClose(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	if (m_state == 3 || m_state == 4) {
		UpdateSectionLengthAndCRC();
		m_state = 5;
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		*outBytes = m_curPos;
		return BE_MP2SYS_OK;
	}

	beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPacketClose", m_state);
	return BE_MP2SYS_ERR_INVALID_STATE;
}

int SiPacketizer::ProcessPacketOpen(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	switch (m_state) {
	case 3:
		UpdateSectionLengthAndCRC();
		/* fallthrough */
	case 4:
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 5;
		/* fallthrough */
	case 0:
	case 5:
		*outBytes = m_curPos;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_curPos = 0;
		return BE_MP2SYS_OK;
	case 1:
		*outBytes = m_curPos;
		m_curPos = 0;
		return BE_MP2SYS_OK;
	case 2:
	default:
		return BE_MP2SYS_ERR_INVALID_STATE;
	}
}

int SiPacketizer::Process(int command, void *arg)
{
	if (!m_initialized)
		return BE_MP2SYS_ERR_NOT_INITIALIZED;

	switch (command) {
	case 0:
		return ProcessPacketOpen((int32_t *)arg);
	case 1:
		return ProcessSIPayload((const PacketizerPayloadParam *)arg);
	case 2:
		return ProcessPacketClose((int32_t *)arg);
	case 11:
		return ProcessSIHeader((const SiHeaderParam *)arg);
	default:
		return BE_MP2SYS_ERR_UNSUPPORTED_CMD;
	}
}
