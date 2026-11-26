#include "be_mp2sys_internal.h"

PesPacketizer::PesPacketizer()
{
	m_initialized = 0;
	m_streamId = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_state = 0;
	m_writtenBytes = 0;
}

void PesPacketizer::UpdatePESLength()
{
	if (m_writtenBytes < 65541) {
		uint8_t lenBytes[2];
		int32_t pesLen = m_writtenBytes - 6;
		lenBytes[0] = (uint8_t)((uint32_t)pesLen >> 8);
		lenBytes[1] = (uint8_t)pesLen;
		int written = m_onOverwrite(m_userData, m_type, 4, lenBytes, 2);
		if (written != m_writtenBytes) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"UpdatePESLength", written, m_writtenBytes);
		}
	}
}

int PesPacketizer::Release()
{
	if (m_initialized) {
		if (m_state == 3) {
			UpdatePESLength();
			m_onClose(m_userData, m_type);
		}
		m_state = 0;
	}
	m_streamId = 0;
	m_writtenBytes = 0;
	m_userData = 0;
	m_type = 0;
	m_onOpen = 0;
	m_onClose = 0;
	m_onWrite = 0;
	m_onOverwrite = 0;
	m_initialized = 0;
	return BE_MP2SYS_OK;
}

PesPacketizer::~PesPacketizer()
{
	Release();
}

int PesPacketizer::Init(const void *rawParam)
{
	const PesPacketizerInitParam *param = (const PesPacketizerInitParam *)rawParam;
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (param->base.type != BE_PACKETIZER_TYPE_PES ||
	    param->base.onOpen == 0 || param->base.onClose == 0 ||
	    param->base.onWrite == 0 || param->base.onOverwrite == 0 ||
	    param->streamId == 0) {
		return BE_MP2SYS_ERR_INVALID_ARG;
	}

	if (m_initialized)
		Release();

	m_streamId = param->streamId;
	m_userData = param->base.userData;
	m_onOpen = param->base.onOpen;
	m_state = 1;
	m_writtenBytes = 0;
	m_initialized = 1;
	m_type = param->base.type;
	m_onClose = param->base.onClose;
	m_onWrite = param->base.onWrite;
	m_onOverwrite = param->base.onOverwrite;
	return BE_MP2SYS_OK;
}

int PesPacketizer::ProcessPESHeader(const PesHeaderParam *param)
{
	if (param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	switch (m_state) {
	case 1:
		break;
	case 3:
		UpdatePESLength();
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 4;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_writtenBytes = 0;
		break;
	case 4:
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_writtenBytes = 0;
		break;
	default:
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPESHeader", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	int ptsDtsFlags = 0;
	if (param->ptsHi != -1 || param->ptsLo != 0xFFFFFFFFu)
		ptsDtsFlags = 2;
	if ((param->dtsHi != -1 || param->dtsLo != 0xFFFFFFFFu) && ptsDtsFlags == 2)
		ptsDtsFlags = 3;

	int extLen = 0;
	if (ptsDtsFlags == 2)
		extLen = 5;
	else if (ptsDtsFlags == 3)
		extLen = 10;

	uint8_t headerBuf[10];
	headerBuf[0] = 0x00;
	headerBuf[1] = 0x00;
	headerBuf[2] = 0x01;
	headerBuf[3] = m_streamId;
	headerBuf[4] = 0x00;
	headerBuf[5] = 0x00;

	uint8_t fl = param->flags;
	headerBuf[6] = (uint8_t)(0x80u |
		((fl & 0x03u) << 4) |
		(((fl >> 2) & 1u) << 3) |
		(((fl >> 3) & 1u) << 2) |
		(((fl >> 4) & 1u) << 1) |
		((fl >> 5) & 1u));
	headerBuf[7] = (uint8_t)((ptsDtsFlags & 3) << 6);
	headerBuf[8] = (uint8_t)extLen;

	int written = m_onWrite(m_userData, m_type, headerBuf, 9);
	if (written != m_writtenBytes + 9) {
		beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
			"ProcessPESHeader", written, m_writtenBytes + 9);
		return BE_MP2SYS_ERR_WRITER_MISMATCH;
	}
	m_writtenBytes += 9;

	if (extLen > 0) {
		uint32_t ptsLo = param->ptsLo;
		headerBuf[0] = (uint8_t)(((ptsDtsFlags & 3) << 4) | ((param->ptsHi & 1) << 3) | (((ptsLo >> 30) & 3u) << 1) | 1u);
		headerBuf[1] = (uint8_t)(ptsLo >> 22);
		headerBuf[2] = (uint8_t)((((ptsLo >> 15) & 0x7Fu) << 1) | 1u);
		headerBuf[3] = (uint8_t)(ptsLo >> 7);
		headerBuf[4] = (uint8_t)(((ptsLo & 0x7Fu) << 1) | 1u);
	}
	if (extLen > 5) {
		uint32_t dtsLo = param->dtsLo;
		headerBuf[5] = (uint8_t)(((ptsDtsFlags & 1) << 4) | ((param->dtsHi & 1) << 3) | (((dtsLo >> 30) & 3u) << 1) | 1u);
		headerBuf[6] = (uint8_t)(dtsLo >> 22);
		headerBuf[7] = (uint8_t)((((dtsLo >> 15) & 0x7Fu) << 1) | 1u);
		headerBuf[8] = (uint8_t)(dtsLo >> 7);
		headerBuf[9] = (uint8_t)(((dtsLo & 0x7Fu) << 1) | 1u);
	}
	if (extLen > 0) {
		written = m_onWrite(m_userData, m_type, headerBuf, extLen);
		if (written != m_writtenBytes + extLen) {
			beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
				"ProcessPESHeader", written, m_writtenBytes + extLen);
			return BE_MP2SYS_ERR_WRITER_MISMATCH;
		}
		m_writtenBytes += extLen;
	}

	m_state = 2;
	return BE_MP2SYS_OK;
}

int PesPacketizer::ProcessPacketClose(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	if (m_state == 3) {
		UpdatePESLength();
		m_state = 4;
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		*outBytes = m_writtenBytes;
		return BE_MP2SYS_OK;
	}

	beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPacketClose", m_state);
	return BE_MP2SYS_ERR_INVALID_STATE;
}

int PesPacketizer::ProcessPacketOpen(int32_t *outBytes)
{
	if (outBytes == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	*outBytes = -1;
	switch (m_state) {
	case 0:
	case 4:
		*outBytes = m_writtenBytes;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_writtenBytes = 0;
		return BE_MP2SYS_OK;
	case 1:
		*outBytes = m_writtenBytes;
		m_writtenBytes = 0;
		return BE_MP2SYS_OK;
	case 2:
		return BE_MP2SYS_ERR_INVALID_STATE;
	case 3:
		UpdatePESLength();
		if (m_onClose(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_CLOSE_CALLBACK;
		m_state = 4;
		*outBytes = m_writtenBytes;
		if (m_onOpen(m_userData, m_type) != 0)
			return BE_MP2SYS_ERR_OPEN_CALLBACK;
		m_state = 1;
		m_writtenBytes = 0;
		return BE_MP2SYS_OK;
	default:
		return BE_MP2SYS_ERR_INVALID_STATE;
	}
}

int PesPacketizer::ProcessPayloadData(const PacketizerPayloadParam *param)
{
	if (param == 0 || param->buffer == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;

	if (m_state != 3 && m_state != 2) {
		beLogPrintf(2, "[DEBUG] %s : invalid state : %u\n", "ProcessPayloadData", m_state);
		return BE_MP2SYS_ERR_INVALID_STATE;
	}

	int written = m_onWrite(m_userData, m_type, param->buffer, param->size);
	int32_t expected = m_writtenBytes + (int32_t)param->size;
	if (written != expected) {
		beLogPrintf(2, "[DEBUG] %s : Odd packet length calculation -- writer = %d, packetizer = %d\n",
			"ProcessPayloadData", written, expected);
		return BE_MP2SYS_ERR_WRITER_MISMATCH;
	}

	m_writtenBytes = expected;
	m_state = 3;
	return BE_MP2SYS_OK;
}

int PesPacketizer::Process(int command, void *arg)
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
	case 3:
		return ProcessPESHeader((const PesHeaderParam *)arg);
	default:
		return BE_MP2SYS_ERR_UNSUPPORTED_CMD;
	}
}
