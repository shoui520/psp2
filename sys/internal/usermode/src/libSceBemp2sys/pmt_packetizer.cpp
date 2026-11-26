#include "be_mp2sys_internal.h"

PmtPacketizer::PmtPacketizer()
{
	m_initialized = 0;
	m_currentNext = 0;
	m_versionNumber = 0;
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
	m_programNumber = 0;
	m_pcrPid = 0;
	beMemset(m_sectionBuf, 0, sizeof(m_sectionBuf));
}

void PmtPacketizer::MakeSectionHeader()
{
	m_sectionBuf[m_curPos++] = 0x02;
	m_sectionBuf[m_curPos++] = 0xB0;
	m_sectionBuf[m_curPos++] = 0x00;
	m_sectionBuf[m_curPos++] = (uint8_t)(m_programNumber >> 8);
	m_sectionBuf[m_curPos++] = (uint8_t)m_programNumber;
	m_sectionBuf[m_curPos++] = (uint8_t)((m_currentNext & 1u) | ((m_versionNumber & 0x1Fu) << 1) | 0xC0u);
	m_sectionBuf[m_curPos++] = m_sectionNumber;
	m_sectionBuf[m_curPos++] = m_lastSectionNumber;
	m_sectionBuf[m_curPos++] = (uint8_t)(((uint8_t)(m_pcrPid >> 8) & 0x1F) | 0xE0);
	m_sectionBuf[m_curPos++] = (uint8_t)m_pcrPid;
	m_sectionBuf[m_curPos++] = 0xF0;
	m_sectionBuf[m_curPos++] = 0x00;

	if (m_onCheckSection != 0) {
		int res = m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
			1020 - m_curPos, 0, 0);
		if (res == 1) {
			beLogPrintf(2, "[DEBUG] %s : Cannot change section - because only header added.\n",
				"MakeSectionHeader");
		}
	}
}

int PmtPacketizer::updateProgramInfoLength()
{
	if (m_state != 3 && m_state != 2)
		return BE_MP2SYS_ERR_INVALID_STATE;
	if (m_curPos < 12)
		return BE_MP2SYS_ERR_INVALID_STATE;

	uint16_t infoLen = (uint16_t)(m_curPos - 12);
	m_sectionBuf[11] = (uint8_t)infoLen;
	m_sectionBuf[10] = (uint8_t)(((infoLen >> 8) & 0x0Fu) | 0xF0u);
	return BE_MP2SYS_OK;
}

int PmtPacketizer::UpdateSectionLengthAndCRC()
{
	if (m_state == 5)
		return BE_MP2SYS_OK;

	if (m_curPos < 3 || m_curPos > 1020)
		return BE_MP2SYS_ERR_SECTION_RANGE;

	if (m_state == 3) {
		if (m_curPos < 12)
			return BE_MP2SYS_ERR_INVALID_STATE;
		uint16_t infoLen = (uint16_t)(m_curPos - 12);
		m_sectionBuf[11] = (uint8_t)infoLen;
		m_sectionBuf[10] = (uint8_t)(((infoLen >> 8) & 0x0Fu) | 0xF0u);
	}

	int32_t secLen = m_curPos + 1;
	m_sectionBuf[2] = (uint8_t)secLen;
	m_sectionBuf[1] = (uint8_t)(((uint32_t)secLen >> 8) | 0xB0u);

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
		m_state = 5;
	}
	return BE_MP2SYS_OK;
}

int PmtPacketizer::Release()
{
	if (m_initialized) {
		if (m_state == 3 || m_state == 4) {
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
	m_programNumber = 0;
	m_pcrPid = 0;
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

PmtPacketizer::~PmtPacketizer()
{
	Release();
}

int PmtPacketizer::Init(const void *rawParam)
{
	const SectionPacketizerInitParam *param = (const SectionPacketizerInitParam *)rawParam;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (param->base.type != BE_PACKETIZER_TYPE_PMT ||
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
	m_versionNumber = 0;
	m_currentNext = 0;
	m_programNumber = 0;
	m_pcrPid = 0;
	m_initialized = 1;
	beMemset(m_sectionBuf, 0, sizeof(m_sectionBuf));

	m_userData = param->base.userData;
	m_type = param->base.type;
	m_onOpen = param->base.onOpen;
	m_onClose = param->base.onClose;
	m_onWrite = param->base.onWrite;
	m_onOverwrite = param->base.onOverwrite;
	setSectionCallbacks(&param->sectionCallbacks);
	return BE_MP2SYS_OK;
}

int PmtPacketizer::ProcessPMTAddElementary(const PmtElementaryParam *param)
{
	int ret = BE_MP2SYS_OK;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	if (m_curPos > 1016) {
		ret = UpdateSectionLengthAndCRC();
		if (ret != BE_MP2SYS_OK)
			return ret;
	}

	if (m_state != 2 && m_state != 3 && m_state != 4) {
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPMTAddElementary", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	if (m_state != 4) {
		ret = updateProgramInfoLength();
		if (ret != BE_MP2SYS_OK)
			return ret;
		m_state = 4;
	}

	uint16_t descLen = param->descriptorLength;
	if (m_curPos + descLen + 5 > 1020) {
		if (m_onCheckSection == 0 ||
		    m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
		        1020 - m_curPos, descLen + 5, 1) == 0) {
			beLogPrintf(2, "[DEBUG] %s : should jump to next section\n", "ProcessPMTAddElementary");
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}
		ret = UpdateSectionLengthAndCRC();
		if (ret != BE_MP2SYS_OK)
			return ret;
		descLen = param->descriptorLength;
		if (m_curPos + descLen + 5 > 1020) {
			beLogPrintf(2, "[DEBUG] %s : too big descriptors_len[%u] / curpos = [%d]\n",
				"ProcessPMTAddElementary", descLen, m_curPos);
			return BE_MP2SYS_ERR_TOO_LARGE;
		}
	}

	m_sectionBuf[m_curPos++] = param->streamType;
	m_sectionBuf[m_curPos++] = (uint8_t)(((uint8_t)(param->elementaryPid >> 8) & 0x1F) | 0xE0);
	m_sectionBuf[m_curPos++] = (uint8_t)param->elementaryPid;
	m_sectionBuf[m_curPos++] = (uint8_t)(((uint8_t)(descLen >> 8) & 0x0F) | 0xF0);
	m_sectionBuf[m_curPos++] = (uint8_t)descLen;
	beMemcpy(m_sectionBuf + m_curPos, param->descriptors, descLen);
	m_curPos += param->descriptorLength;

	if (m_onCheckSection != 0 &&
	    m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
	        1020 - m_curPos, 0, 1) == 1) {
		return UpdateSectionLengthAndCRC();
	}
	return ret;
}

int PmtPacketizer::ProcessPMTAddDescriptors(const PmtDescriptorParam *param)
{
	int ret = BE_MP2SYS_OK;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	if (m_curPos > 1016) {
		ret = UpdateSectionLengthAndCRC();
		if (ret != BE_MP2SYS_OK)
			return ret;
	}

	if (m_state != 2 && m_state != 3) {
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPMTAddDescriptors", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t pos = 0;
	int32_t curPos = m_curPos;
	while (pos + 1 < param->size) {
		uint32_t descLen = (uint8_t)(param->buffer[pos + 1] + 2);
		if (pos + descLen > param->size) {
			beLogPrintf(2, "[DEBUG] %s : maybe corrupted descriptor!\n", "ProcessPMTAddDescriptors");
			return BE_MP2SYS_ERR_CORRUPT_DESCRIPTOR;
		}
		if ((uint32_t)(curPos + (int32_t)descLen) <= 1020u) {
			beMemcpy(m_sectionBuf + curPos, param->buffer + pos, (uint8_t)(param->buffer[pos + 1] + 2));
			curPos += (int32_t)descLen;
			pos += descLen;
		} else {
			int canNext = 0;
			if (m_onCheckSection != 0) {
				canNext = m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
					1020 - curPos, descLen, 0);
			}
			if (canNext == 0) {
				beLogPrintf(2, "[DEBUG] %s : should jump to next section\n", "ProcessPMTAddDescriptors");
				return BE_MP2SYS_ERR_WRITER_MISMATCH;
			}
			m_curPos = curPos;
			ret = UpdateSectionLengthAndCRC();
			if (ret != BE_MP2SYS_OK || m_state == 5)
				return ret;
			curPos = m_curPos;
			if ((uint32_t)(curPos + (int32_t)descLen) > 1020u)
				return BE_MP2SYS_ERR_TOO_LARGE;
			m_state = 3;
		}
	}

	m_curPos = curPos;
	m_state = 3;

	if (curPos > 1017)
		return UpdateSectionLengthAndCRC();

	if (m_onCheckSection != 0 &&
	    m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
	        1020 - curPos, 0, 0) == 1) {
		return UpdateSectionLengthAndCRC();
	}
	return ret;
}

int PmtPacketizer::ProcessPMTHeader(const PmtHeaderParam *param)
{
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	uint32_t state = m_state;
	switch (state) {
	case 1:
	case 6:
		break;
	case 3:
	case 4:
		UpdateSectionLengthAndCRC();
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		state = 6;
		m_state = 6;
		break;
	default:
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPMTHeader", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t totalSize = param->totalDataSize;
	uint32_t secCount = totalSize / 1004u;
	uint32_t rem = totalSize % 1004u;
	if (rem == 0 || (1012u - rem) > 4u * secCount)
		++secCount;

	uint32_t lastSec = secCount - 1u;
	if (lastSec > 256u) {
		beLogPrintf(2, "[DEBUG] %s : too big data size : %u\n", "ProcessPMTHeader", totalSize);
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	if (state != 1) {
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_curPos = 0;
	}

	m_versionNumber = (uint8_t)param->versionNumber & 0x1Fu;
	m_currentNext = (uint8_t)param->currentNext & 1u;
	m_programNumber = param->programNumber;
	m_pcrPid = param->pcrPid;
	m_sectionNumber = 0;
	m_lastSectionNumber = (uint8_t)lastSec;
	MakeSectionHeader();
	m_state = 2;
	return BE_MP2SYS_OK;
}

int PmtPacketizer::ProcessPacketClose(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	if (m_state == 3 || m_state == 4 || m_state == 5) {
		UpdateSectionLengthAndCRC();
		m_state = 6;
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		*outBytes = m_curPos;
		return BE_MP2SYS_OK;
	}

	beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPacketClose", m_state);
	return BE_MP2SYS_ERR_INVALID_STATE;
}

int PmtPacketizer::ProcessPacketOpen(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	switch (m_state) {
	case 3:
	case 4:
		UpdateSectionLengthAndCRC();
		/* fallthrough */
	case 5:
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 6;
		/* fallthrough */
	case 0:
	case 6:
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

int PmtPacketizer::Process(int command, void *arg)
{
	if (!m_initialized)
		return BE_MP2SYS_ERR_NOT_INITIALIZED;

	switch (command) {
	case 0:
		return ProcessPacketOpen((int32_t *)arg);
	case 2:
		return ProcessPacketClose((int32_t *)arg);
	case 8:
		return ProcessPMTHeader((const PmtHeaderParam *)arg);
	case 9:
		return ProcessPMTAddDescriptors((const PmtDescriptorParam *)arg);
	case 10:
		return ProcessPMTAddElementary((const PmtElementaryParam *)arg);
	default:
		return BE_MP2SYS_ERR_UNSUPPORTED_CMD;
	}
}
