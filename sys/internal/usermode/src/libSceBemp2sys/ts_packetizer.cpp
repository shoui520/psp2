#include "be_mp2sys_internal.h"

void TsPacketizer::clearPacketCallback()
{
	m_onPacketWritten = 0;
}

TsPacketizer::TsPacketizer()
{
	m_initialized = 0;
	m_pcrFlag = 0;
	m_randomAccessIndicator = 0;
	m_discontinuityIndicator = 0;
	m_scramblingControl = 0;
	m_payloadUnitStart = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_state = 0;
	m_packetPos = 0;
	m_pid = 0xFFFF;
	m_continuityCounter = 0xFF;
	m_pcrExt = 0xFFFF;
	m_pcrBaseHi = -1;
	m_pcrBaseLo = 0xFFFFFFFFu;
	m_onPacketWritten = 0;
}

int TsPacketizer::Release()
{
	if (m_initialized) {
		if (m_state == 3)
			m_onClose(m_userData, m_type);
		m_state = 0;
	}
	m_pid = 0xFFFF;
	m_continuityCounter = 0xFF;
	m_payloadUnitStart = 0;
	m_scramblingControl = 0;
	m_discontinuityIndicator = 0;
	m_randomAccessIndicator = 0;
	m_pcrFlag = 0;
	m_pcrExt = 0xFFFF;
	m_pcrBaseHi = -1;
	m_pcrBaseLo = 0xFFFFFFFFu;
	m_packetPos = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_onPacketWritten = 0;
	m_initialized = 0;
	return 0xFFFF;
}

TsPacketizer::~TsPacketizer()
{
	Release();
}

void TsPacketizer::setPacketCallback(const TsPacketWrittenCallback *cb)
{
	if (cb != 0)
		m_onPacketWritten = *cb;
}

int TsPacketizer::Init(const void *rawParam)
{
	const TsPacketizerInitParam *param = (const TsPacketizerInitParam *)rawParam;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (param->base.type != BE_PACKETIZER_TYPE_TS ||
	    param->base.onOpen == 0 || param->base.onClose == 0 ||
	    param->base.onWrite == 0 || param->base.onOverwrite == 0 ||
	    param->onPacketWritten == 0) {
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	if (m_initialized)
		Release();

	m_state = 1;
	m_packetPos = 0;
	m_pid = param->pid;
	m_continuityCounter = param->continuityCounter & 0x0Fu;
	m_userData = param->base.userData;
	m_onOpen = param->base.onOpen;
	m_type = param->base.type;
	m_onClose = param->base.onClose;
	m_initialized = 1;
	m_onWrite = param->base.onWrite;
	m_onOverwrite = param->base.onOverwrite;
	setPacketCallback(&param->onPacketWritten);
	return BE_MP2SYS_OK;
}

int TsPacketizer::notifyPacketWritten(uint32_t consumedBytes, uint32_t totalBytes)
{
	int res = 0;
	if (m_onPacketWritten != 0)
		res = m_onPacketWritten(m_userData, m_type, m_pid, m_continuityCounter, consumedBytes, totalBytes);
	m_continuityCounter = (uint8_t)((m_continuityCounter + 1) & 0x0F);
	return res;
}

int TsPacketizer::ConstructTSHeader(uint8_t isFirstPacket, int32_t remainBytes)
{
	uint8_t hdr[16];
	int hasAdapt = (isFirstPacket != 0 || remainBytes < 184) ? 1 : 0;

	hdr[0] = 0x47;
	hdr[1] = (uint8_t)((m_pid >> 8) & 0x1Fu);
	if (isFirstPacket != 0)
		hdr[1] = (uint8_t)(((m_payloadUnitStart & 1u) << 6) | ((m_pid >> 8) & 0x1Fu));
	hdr[2] = (uint8_t)m_pid;

	uint8_t afc = (uint8_t)((hasAdapt ? 2u : 0u) | (remainBytes > 0 ? 1u : 0u));
	hdr[3] = (uint8_t)(((m_scramblingControl & 3u) << 6) | ((afc & 3u) << 4) | (m_continuityCounter & 0x0Fu));

	if (!hasAdapt) {
		m_packetPos = 4;
		int written = m_onWrite(m_userData, m_type, hdr, 4);
		if (written != m_packetPos) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"ConstructTSHeader", written, m_packetPos);
			m_packetPos = 0;
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}
		return BE_MP2SYS_OK;
	}

	int32_t adaptLen = (isFirstPacket != 0 && m_pcrFlag != 0) ? 7 : 1;
	if (remainBytes < 184 && 183 - remainBytes > adaptLen)
		adaptLen = 183 - remainBytes;
	if (remainBytes == 0)
		adaptLen = 183;

	hdr[4] = (uint8_t)adaptLen;
	uint8_t adaptFlags = 0;
	if (isFirstPacket != 0) {
		adaptFlags = (uint8_t)(((m_discontinuityIndicator & 1u) << 7) |
			((m_randomAccessIndicator & 1u) << 6) |
			((m_pcrFlag & 1u) << 4));
	}
	hdr[5] = adaptFlags;
	m_packetPos = 6;
	int32_t stuffLen = (uint8_t)(adaptLen - 1);

	if (isFirstPacket != 0 && m_pcrFlag != 0) {
		uint32_t pcrLo = m_pcrBaseLo;
		hdr[6] = (uint8_t)(((m_pcrBaseHi & 1) << 7) | (pcrLo >> 25));
		hdr[7] = (uint8_t)(pcrLo >> 17);
		hdr[8] = (uint8_t)(pcrLo >> 9);
		hdr[9] = (uint8_t)(pcrLo >> 1);
		hdr[10] = (uint8_t)(((pcrLo & 1u) << 7) | 0x7Eu | ((m_pcrExt >> 8) & 1u));
		hdr[11] = (uint8_t)m_pcrExt;
		m_packetPos = 12;
		stuffLen = (uint8_t)(stuffLen - 6);
	}

	int written = m_onWrite(m_userData, m_type, hdr, m_packetPos);
	if (written != m_packetPos) {
		beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
			"ConstructTSHeader", written, m_packetPos);
		m_packetPos = 0;
		return BE_MP2SYS_ERR_WRITER_MISMATCH;
	}

	for (int i = 0; i < 16; ++i)
		hdr[i] = 0xFF;

	while (stuffLen > 0) {
		int32_t chunk = (stuffLen > 16) ? 16 : stuffLen;
		written = m_onWrite(m_userData, m_type, hdr, chunk);
		if (written != m_packetPos + chunk) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"ConstructTSHeader", written, m_packetPos + chunk);
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}
		m_packetPos += chunk;
		stuffLen = (uint8_t)(stuffLen - chunk);
	}
	return BE_MP2SYS_OK;
}

int TsPacketizer::ProcessPayloadData(const PacketizerPayloadParam *param)
{
	if (param == 0 || param->buffer == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	if (m_state != 3 && m_state != 2) {
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPayloadData", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	uint32_t consumed = 0;
	while (consumed < param->size) {
		int32_t remain = (int32_t)(param->size - consumed);
		uint8_t isFirst = (m_state == 2) ? 1 : 0;
		if (m_hasPointerField && isFirst)
			++remain;

		if (ConstructTSHeader(isFirst, remain) != BE_MP2SYS_OK)
			break;

		if (m_hasPointerField && isFirst && m_packetPos < 188) {
			uint8_t pointerField = 0;
			int written = m_onWrite(m_userData, m_type, &pointerField, 1);
			if (written != m_packetPos + 1) {
				beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
					"ProcessPayloadData", written, m_packetPos + 1);
				return BE_MP2SYS_ERR_WRITER_MISMATCH;
			}
			m_packetPos += 1;
		}

		m_state = 3;
		uint32_t chunk = (uint32_t)(188 - m_packetPos);
		if (chunk > param->size - consumed)
			chunk = param->size - consumed;

		if (chunk != 0) {
			int written = m_onWrite(m_userData, m_type, param->buffer + consumed, chunk);
			if (written != m_packetPos + (int32_t)chunk) {
				beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
					"ProcessPayloadData", written, m_packetPos + (int32_t)chunk);
				return BE_MP2SYS_ERR_WRITER_MISMATCH;
			}
		}

		m_packetPos += (int32_t)chunk;
		consumed += chunk;

		if (notifyPacketWritten(consumed, param->size) == 1)
			break;
	}
	return BE_MP2SYS_OK;
}

int TsPacketizer::ProcessTSHeader(const TsHeaderParam *param)
{
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	switch (m_state) {
	case 1:
		break;
	case 3:
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 4;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_packetPos = 0;
		break;
	case 4:
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_packetPos = 0;
		break;
	default:
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessTSHeader", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	m_payloadUnitStart = (uint8_t)param->payloadUnitStart & 1u;
	m_scramblingControl = (uint8_t)param->scramblingControl & 3u;
	m_discontinuityIndicator = (uint8_t)param->discontinuityIndicator & 1u;
	m_randomAccessIndicator = (uint8_t)param->randomAccessIndicator & 1u;
	m_pcrFlag = (uint8_t)param->pcrFlag & 1u;
	m_pcrExt = param->pcrExt;
	m_pcrBaseHi = param->pcrBaseHi;
	m_pcrBaseLo = param->pcrBaseLo;
	m_hasPointerField = param->hasPointerField != 0;
	m_state = 2;
	return BE_MP2SYS_OK;
}

int TsPacketizer::ProcessPacketClose(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	if (m_state == 3) {
		m_state = 4;
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		*outBytes = m_packetPos;
		return BE_MP2SYS_OK;
	}

	beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPacketClose", m_state);
	return BE_MP2SYS_ERR_INVALID_STATE;
}

int TsPacketizer::ProcessPacketOpen(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	switch (m_state) {
	case 0:
	case 4:
		*outBytes = m_packetPos;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_packetPos = 0;
		return BE_MP2SYS_OK;
	case 1:
		*outBytes = m_packetPos;
		m_packetPos = 0;
		return BE_MP2SYS_OK;
	case 2:
		return BE_MP2SYS_ERR_INVALID_STATE;
	case 3:
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 4;
		*outBytes = m_packetPos;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_packetPos = 0;
		return BE_MP2SYS_OK;
	default:
		return BE_MP2SYS_ERR_INVALID_STATE;
	}
}

int TsPacketizer::Process(int command, void *arg)
{
	if (!m_initialized)
		return BE_MP2SYS_ERR_NOT_INITIALIZED;

	switch (command) {
	case 0:
		return ProcessPacketOpen((int32_t *)arg);
	case 1:
		return ProcessPayloadData((const PacketizerPayloadParam *)arg);
	case 2:
		return ProcessPacketClose((int32_t *)arg);
	case 12:
		return ProcessTSHeader((const TsHeaderParam *)arg);
	default:
		return BE_MP2SYS_ERR_UNSUPPORTED_CMD;
	}
}
