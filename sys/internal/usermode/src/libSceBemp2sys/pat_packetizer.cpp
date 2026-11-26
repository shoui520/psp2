#include "be_mp2sys_internal.h"

PatPacketizer::PatPacketizer()
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
	m_transportStreamId = 0;
	m_networkPid = 16;
	beMemset(m_sectionBuf, 0, sizeof(m_sectionBuf));
}

void PatPacketizer::MakeSectionHeader()
{
	m_sectionBuf[m_curPos++] = 0x00;
	m_sectionBuf[m_curPos++] = 0xB0;
	m_sectionBuf[m_curPos++] = 0x00;
	m_sectionBuf[m_curPos++] = (uint8_t)(m_transportStreamId >> 8);
	m_sectionBuf[m_curPos++] = (uint8_t)m_transportStreamId;
	m_sectionBuf[m_curPos++] = (uint8_t)((m_currentNext & 1u) | ((m_versionNumber & 0x1Fu) << 1) | 0xC0u);
	m_sectionBuf[m_curPos++] = m_sectionNumber;
	m_sectionBuf[m_curPos++] = m_lastSectionNumber;

	if (m_sectionNumber == 0) {
		m_sectionBuf[m_curPos++] = 0x00;
		m_sectionBuf[m_curPos++] = 0x00;
		m_sectionBuf[m_curPos++] = (uint8_t)(((uint8_t)(m_networkPid >> 8) & 0x1F) | 0xE0);
		m_sectionBuf[m_curPos++] = (uint8_t)m_networkPid;
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

int PatPacketizer::UpdateSectionLengthAndCRC()
{
	if (m_state == 4)
		return BE_MP2SYS_OK;

	if (m_curPos < 3 || m_curPos > 1020)
		return BE_MP2SYS_ERR_SECTION_RANGE;

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
		m_state = 4;
	}
	return BE_MP2SYS_OK;
}

int PatPacketizer::Release()
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
	m_transportStreamId = 0;
	m_networkPid = 16;
	m_currentNext = 0;
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

PatPacketizer::~PatPacketizer()
{
	Release();
}

int PatPacketizer::Init(const void *rawParam)
{
	const SectionPacketizerInitParam *param = (const SectionPacketizerInitParam *)rawParam;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (param->base.type != BE_PACKETIZER_TYPE_PAT ||
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
	m_transportStreamId = 0;
	m_networkPid = 16;
	m_currentNext = 0;
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

int PatPacketizer::ProcessPATAddProgram(const PatProgramParam *param)
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
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPATAddProgram", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	m_sectionBuf[m_curPos++] = (uint8_t)(param->programNumber >> 8);
	m_sectionBuf[m_curPos++] = (uint8_t)param->programNumber;
	m_sectionBuf[m_curPos++] = (uint8_t)(((uint8_t)(param->pmtPid >> 8) & 0x1F) | 0xE0);
	m_sectionBuf[m_curPos++] = (uint8_t)param->pmtPid;
	m_state = 3;

	if (m_curPos > 1016)
		return UpdateSectionLengthAndCRC();

	if (m_onCheckSection != 0 &&
	    m_onCheckSection(m_userData, m_type, m_sectionNumber, m_lastSectionNumber,
	        1020 - m_curPos, 0, 1) == 1 &&
	    1020 - m_curPos < 4) {
		return UpdateSectionLengthAndCRC();
	}
	return ret;
}

int PatPacketizer::ProcessPATHeader(const PatHeaderParam *param)
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
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPATHeader", state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t totalBytes = 4u * param->programCount + 4u;
	uint32_t secCount = totalBytes / 1012u;
	if (totalBytes % 1012u != 0)
		++secCount;

	uint32_t lastSec = secCount - 1u;
	if (lastSec > 256u) {
		beLogPrintf(2, "[DEBUG] %s : too many program count : %u\n", "ProcessPATHeader", param->programCount);
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	if (state != 1) {
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_curPos = 0;
	}

	m_versionNumber = (uint8_t)param->versionNumber & 0x1Fu;
	m_transportStreamId = param->transportStreamId;
	m_networkPid = param->networkPid;
	m_sectionNumber = 0;
	m_currentNext = (uint8_t)param->currentNext & 1u;
	m_lastSectionNumber = (uint8_t)lastSec;
	MakeSectionHeader();
	m_state = 2;
	return BE_MP2SYS_OK;
}

int PatPacketizer::ProcessPacketClose(int32_t *outBytes)
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

int PatPacketizer::ProcessPacketOpen(int32_t *outBytes)
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

int PatPacketizer::Process(int command, void *arg)
{
	if (!m_initialized)
		return BE_MP2SYS_ERR_NOT_INITIALIZED;

	switch (command) {
	case 0:
		return ProcessPacketOpen((int32_t *)arg);
	case 2:
		return ProcessPacketClose((int32_t *)arg);
	case 4:
		return ProcessPATHeader((const PatHeaderParam *)arg);
	case 5:
		return ProcessPATAddProgram((const PatProgramParam *)arg);
	default:
		return BE_MP2SYS_ERR_UNSUPPORTED_CMD;
	}
}
