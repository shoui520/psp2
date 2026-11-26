#include "be_mp2sys_internal.h"

PesPayloadParser::PesPayloadParser()
{
}

PesPayloadParser::~PesPayloadParser()
{
}

int PesPayloadParser::Init()
{
	TsPayloadParser::Init();
	return m_pesParser.Init();
}

int PesPayloadParser::Release()
{
	m_pesParser.Release();
	m_active = 0;
	m_nextFree = 0;
	return 0;
}

int PesPayloadParser::IsActive()
{
	return m_pesParser.m_active;
}

int PesPayloadParser::Parse(BitReader *reader, TsDemuxer *demuxer, int32_t payloadLen)
{
	if (demuxer != 0 && demuxer->m_pesHost != 0)
		return m_pesParser.Parse(reader, demuxer->m_pesHost, payloadLen);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

int32_t PesPayloadParser::GetParserType()
{
	return m_pesParser.m_parserType;
}

void PesPayloadParser::SetState(int32_t state)
{
	m_pesParser.m_state = state;
}

PesPacketInfo *PesPayloadParser::getPesPacketInfo()
{
	return &m_pesParser.m_packetInfo;
}
