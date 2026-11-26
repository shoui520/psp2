#include "be_mp2sys_internal.h"

int32_t g_defaultMaxPmtCount = 10;
int32_t g_defaultMaxPesCount = 40;
int32_t g_defaultMaxPrivateCount = 30;

TsDemuxer::TsDemuxer()
{
	m_patPidInfo.pid = 0;
	m_catPidInfo.pid = 1;

	m_reservedState = 0;
	m_eventCallback = 0;
	m_userData = 0;
	m_streamInfo = 0;
	m_maxPrivateCount = 0;
	m_pesHost = 0;
	m_maxPmtCount = 0;
	m_maxPesCount = 0;
	m_privatePidInfos = 0;
	m_ptsOffset = 0;
	m_pmtPidInfos = 0;
	m_pesPidInfos = 0;
	m_pmtParserPool = 0;
	m_privateParserPool = 0;
	m_pesParserPool = 0;
	m_flags = 0;
}

int TsDemuxer::Release()
{
	m_memBlockPool.Destroy();
	m_streamInfo = 0;
	m_pesHost = 0;

	if (m_privatePidInfos != 0)
		delete[] m_privatePidInfos;
	m_privatePidInfos = 0;

	if (m_pmtPidInfos != 0)
		delete[] m_pmtPidInfos;
	m_pmtPidInfos = 0;

	if (m_pesPidInfos != 0)
		delete[] m_pesPidInfos;
	m_pesPidInfos = 0;

	if (m_pmtParserPool != 0)
		delete[] m_pmtParserPool;
	m_pmtParserPool = 0;

	if (m_privateParserPool != 0)
		delete[] m_privateParserPool;
	m_privateParserPool = 0;

	if (m_pesParserPool != 0)
		delete[] m_pesParserPool;
	m_pesParserPool = 0;

	m_maxPesCount = 0;
	m_maxPrivateCount = 0;
	m_maxPmtCount = 0;
	return 0;
}

TsDemuxer::~TsDemuxer()
{
	Release();
}

int TsDemuxer::Init(IPesHostCallback *pesHost, void *userData, ITsDemuxerEventCallback *eventCallback,
	int32_t initialBlocks, int32_t growBlocks, int32_t maxFreeKeep)
{
	if (!m_tsParser.Init(this))
		return 0;

	m_packetCount = 0;
	m_freePmtList = 0;
	m_freePrivateList = 0;
	m_freePesList = 0;

	m_patPidInfo.reset();
	m_patParser.Init();
	m_patPidInfo.parser = &m_patParser;
	m_patPidInfo.pid = 0;

	m_catPidInfo.reset();
	m_catParser.Init();
	m_catPidInfo.parser = &m_catParser;
	m_catPidInfo.pid = 1;

	m_reservedState = 0;
	m_pesHost = pesHost;
	m_eventCallback = eventCallback;
	m_memBlockPool.Destroy();
	m_memBlockPool.Create(4096, initialBlocks, growBlocks, maxFreeKeep);

	m_maxPmtCount = g_defaultMaxPmtCount;
	m_ptsOffset = 0;
	m_maxPesCount = g_defaultMaxPesCount;
	m_maxPrivateCount = g_defaultMaxPrivateCount;

	if (m_privatePidInfos != 0)
		delete[] m_privatePidInfos;
	m_privatePidInfos = new PidInfo[m_maxPrivateCount];

	if (m_pmtPidInfos != 0)
		delete[] m_pmtPidInfos;
	m_pmtPidInfos = new PidInfo[m_maxPmtCount];

	if (m_pesPidInfos != 0)
		delete[] m_pesPidInfos;
	m_pesPidInfos = new PidInfo[m_maxPesCount];

	if (m_pmtParserPool != 0)
		delete[] m_pmtParserPool;
	m_pmtParserPool = new PmtSectionParser[m_maxPmtCount];

	if (m_privateParserPool != 0)
		delete[] m_privateParserPool;
	m_privateParserPool = new PrivateSectionParser[m_maxPrivateCount];

	if (m_pesParserPool != 0)
		delete[] m_pesParserPool;
	m_pesParserPool = new PesPayloadParser[m_maxPesCount];

	for (int32_t i = 0; i < m_maxPmtCount; ++i) {
		if (m_pmtPidInfos != 0)
			m_pmtPidInfos[i].reset();
		if (m_pmtParserPool != 0) {
			m_pmtParserPool[i].Init();
			m_pmtParserPool[i].m_nextFree = m_freePmtList;
			m_freePmtList = &m_pmtParserPool[i];
		}
	}

	if (m_privatePidInfos != 0) {
		for (int32_t i = 0; i < m_maxPrivateCount; ++i)
			m_privatePidInfos[i].reset();
	}
	if (m_privateParserPool != 0) {
		for (int32_t i = 0; i < m_maxPrivateCount; ++i) {
			m_privateParserPool[i].Init();
			m_privateParserPool[i].m_nextFree = m_freePrivateList;
			m_freePrivateList = &m_privateParserPool[i];
		}
	}

	for (int32_t i = 0; i < m_maxPesCount; ++i) {
		if (m_pesPidInfos != 0)
			m_pesPidInfos[i].reset();
		if (m_pesParserPool != 0) {
			m_pesParserPool[i].Init();
			m_pesParserPool[i].m_nextFree = m_freePesList;
			m_freePesList = &m_pesParserPool[i];
		}
	}

	m_userData = userData;
	m_tsParser.resetPcrTracking();
	m_flags = 0;
	return 1;
}

void TsDemuxer::resetHandles()
{
	m_reservedState = 0;
	m_patPidInfo.pid = 0;
	m_catPidInfo.pid = 1;
	m_eventCallback = 0;
	m_userData = 0;
	m_streamInfo = 0;
	m_ptsOffset = 0;
	m_maxPrivateCount = 0;
	m_pesHost = 0;
	m_maxPmtCount = 0;
	m_maxPesCount = 0;
	m_privatePidInfos = 0;
	m_pmtPidInfos = 0;
	m_pesPidInfos = 0;
	m_pmtParserPool = 0;
	m_privateParserPool = 0;
	m_pesParserPool = 0;
}

int TsDemuxer::Parse(const uint8_t *buffer, int32_t size, int32_t *outConsumed, uint64_t userTimestamp)
{
	m_bitReader.setBuffer(buffer, size);
	int ret = m_tsParser.Parse(&m_bitReader, userTimestamp);
	*outConsumed = m_bitReader.m_pos;
	++m_packetCount;
	return ret;
}

void TsDemuxer::cleanupRemovedPrograms()
{
	PidInfo *cur = m_patPidInfo.childList;
	PidInfo *prev = 0;

	while (cur != 0) {
		PidInfo *next = cur->nextSibling;
		if (cur->needsUpdate == 1 && cur->pendingPid == -1) {
			cur->parser->Init();
			cur->parser->m_nextFree = m_freePmtList;
			m_freePmtList = cur->parser;

			if (prev != 0)
				prev->nextSibling = next;
			else
				m_patPidInfo.childList = next;

			PidInfo *es = cur->childList;
			while (es != 0) {
				es->parser->Init();
				es->parser->m_nextFree = m_freePesList;
				m_freePesList = es->parser;
				PidInfo *nextEs = es->nextSibling;
				es->reset();
				es = nextEs;
			}
			cur->reset();
		} else {
			prev = cur;
		}
		cur = next;
	}
}

void TsDemuxer::cleanupRemovedStreams(PidInfo *pmtPidInfo)
{
	PidInfo *cur = pmtPidInfo->childList;
	PidInfo *prev = 0;

	while (cur != 0) {
		PidInfo *next = cur->nextSibling;
		if (cur->needsUpdate == 1 && cur->pendingPid == -1) {
			cur->parser->Init();
			cur->parser->m_nextFree = m_freePesList;
			m_freePesList = cur->parser;

			if (prev != 0)
				prev->nextSibling = next;
			else
				pmtPidInfo->childList = next;

			cur->reset();
		} else {
			prev = cur;
		}
		cur = next;
	}
}

void TsDemuxer::attachStreamInfo(StreamInfoHost *host)
{
	if (host != 0 && host->m_handle != 0 && host->m_handle->m_streamInfo != 0) {
		m_streamInfo = host->m_handle->m_streamInfo;
		m_streamInfo->m_demuxer = this;
	} else {
		m_streamInfo = 0;
	}
}
