#include "be_mp2sys_internal.h"

StreamInfo::StreamInfo()
	: m_demuxer(0),
	  m_filters(0),
	  m_filterCapacity(0),
	  m_filterCount(0)
{
	m_programs = 0;
	m_programCapacity = 0;
	m_programCount = 0;
	m_patComplete = 0;
	m_sink = 0;
	m_mutex.Initialize(0);
}

void StreamInfo::freePrograms()
{
	if (m_programs != 0) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].esCapacity != 0 && m_programs[i].esList != 0)
				delete[] m_programs[i].esList;
		}
		m_programCapacity = 0;
		m_programCount = 0;
		delete[] m_programs;
		m_programs = 0;
	}
}

int StreamInfo::Release()
{
	m_mutex.Lock(0);
	if (m_filters != 0) {
		delete[] m_filters;
		m_filters = 0;
	}
	freePrograms();
	m_patComplete = 0;
	m_filterCapacity = 0;
	m_filterCount = 0;
	m_mutex.Unlock();
	return 1;
}

StreamInfo::~StreamInfo()
{
	Release();
	m_mutex.Release();
}

int StreamInfo::Init(IStreamInfoSink *sink)
{
	if (m_filters != 0 || sink == 0)
		return 0;

	m_mutex.Lock(0);
	m_filters = new StreamFilterEntry[10];
	if (m_filters == 0) {
		m_mutex.Unlock();
		return 0;
	}

	if (m_programs != 0) {
		delete[] m_programs;
		m_programs = 0;
	}
	m_programCapacity = 0;
	m_programCount = 0;
	m_patComplete = 0;
	m_filterCapacity = 10;
	m_filterCount = 0;
	m_sink = sink;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::onPesHeader(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo)
{
	if (m_sink != 0)
		return m_sink->onPesHeader(pid, ptsHi, ptsLo, dtsHi, dtsLo);
	return 0;
}

int StreamInfo::onPesPayload(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo,
	const uint8_t *data, int32_t size)
{
	if (m_sink != 0)
		return m_sink->onPesPayload(pid, ptsHi, ptsLo, dtsHi, dtsLo, data, size);
	return 0;
}

int StreamInfo::onSection(uint16_t pid, const uint8_t *buf, int32_t length, int32_t status, ISectionStorage *storage)
{
	if (m_sink != 0)
		return m_sink->onSection(pid, buf, length, status, storage);
	return 0;
}

int StreamInfo::onTsPayload(PidInfo *pidInfo, const uint8_t *data, int32_t size)
{
	if (m_sink != 0)
		return m_sink->onTsPayload(pidInfo, data, size);
	return 0;
}

void StreamInfo::onTsPacket(PidInfo *pidInfo, const uint8_t *packet, int32_t packetSize,
	const uint8_t *payload, int32_t payloadSize)
{
	if (m_sink != 0)
		m_sink->onTsPacket(pidInfo, packet, packetSize, payload, payloadSize);
}

int StreamInfo::IsPatComplete()
{
	if (m_filters != 0)
		return m_patComplete;
	return 0;
}

int StreamInfo::GetProgramCount()
{
	if (m_filters == 0)
		return 0;
	if (m_patComplete)
		return m_programCount;
	return 0;
}

int StreamInfo::GetProgramList(int32_t maxCount, uint16_t *outPairs)
{
	if (m_filters == 0)
		return 0;

	int32_t count = 0;
	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount && count < maxCount; ++i) {
			outPairs[2 * count] = m_programs[i].programNumber;
			outPairs[2 * count + 1] = m_programs[i].pmtPid;
			++count;
		}
	}
	m_mutex.Unlock();
	return count;
}

int StreamInfo::GetPmtPidByProgramNumber(uint16_t programNumber, uint16_t *outPmtPid)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].programNumber == programNumber) {
				*outPmtPid = m_programs[i].pmtPid;
				m_mutex.Unlock();
				return 1;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

int StreamInfo::IsPmtComplete(uint16_t pmtPid)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].pmtPid == pmtPid) {
				m_mutex.Unlock();
				return m_programs[i].pmtComplete;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

int StreamInfo::GetProgramNumberByPmtPid(uint16_t pmtPid, uint16_t *outProgramNumber)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].pmtPid == pmtPid) {
				*outProgramNumber = m_programs[i].programNumber;
				m_mutex.Unlock();
				return 1;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

int StreamInfo::GetEsCount(uint16_t pmtPid)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].pmtPid == pmtPid) {
				if (m_programs[i].pmtComplete && m_programs[i].esList != 0) {
					m_mutex.Unlock();
					return m_programs[i].esCount;
				}
				break;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

int StreamInfo::GetEsList(uint16_t pmtPid, int32_t maxCount, EsStreamEntry *outEntries)
{
	if (m_filters == 0)
		return 0;

	int32_t count = 0;
	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].pmtPid == pmtPid) {
				if (m_programs[i].pmtComplete && m_programs[i].esList != 0) {
					for (int32_t j = 0; j < m_programs[i].esCount; ++j) {
						if (count >= maxCount)
							break;
						EsStreamEntry *src = &m_programs[i].esList[j];
						outEntries[count].streamType = src->streamType;
						outEntries[count].pid = src->pid;
						outEntries[count].avStreamType = src->avStreamType;
						++count;
					}
				}
				break;
			}
		}
	}
	m_mutex.Unlock();
	return count;
}

int StreamInfo::GetEsAvStreamType(uint16_t pmtPid, uint16_t esPid, int32_t *outAvStreamType)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (m_patComplete) {
		for (int32_t i = 0; i < m_programCount; ++i) {
			if (m_programs[i].pmtPid == pmtPid) {
				if (m_programs[i].pmtComplete && m_programs[i].esList != 0) {
					for (int32_t j = 0; j < m_programs[i].esCount; ++j) {
						if (m_programs[i].esList[j].pid == esPid) {
							*outAvStreamType = m_programs[i].esList[j].avStreamType;
							m_mutex.Unlock();
							return 1;
						}
					}
				}
				break;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

const uint8_t *StreamInfo::GetPidDescriptors(uint16_t pmtPid, uint16_t targetPid, int32_t *outDescLength)
{
	(void)pmtPid;
	if (m_filters == 0 || m_demuxer == 0)
		return 0;

	*outDescLength = 0;
	const uint8_t *result = 0;
	m_mutex.Lock(0);
	if (m_patComplete) {
		PidInfo *info = m_demuxer->findPidInfo((int16_t)targetPid);
		if (info != 0) {
			*outDescLength = info->descriptorLength;
			result = info->descriptorPtr;
		}
	}
	m_mutex.Unlock();
	return result;
}

int StreamInfo::expandFilterTable()
{
	int32_t newCap = m_filterCapacity + 10;
	StreamFilterEntry *newArr = new StreamFilterEntry[newCap];
	if (newArr != 0) {
		for (int32_t i = 0; i < m_filterCount; ++i)
			newArr[i] = m_filters[i];
		StreamFilterEntry *oldArr = m_filters;
		m_filters = newArr;
		m_filterCapacity = m_filterCapacity + 10;
		delete[] oldArr;
	}
	return 0;
}

int StreamInfo::SetPidFilter(uint16_t pid, uint32_t mask, void *callback)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	for (int32_t i = 0; i < m_filterCapacity; ++i) {
		if (m_filters[i].pid == pid) {
			m_filters[i].mask = mask;
			m_filters[i].callback = callback;
			m_mutex.Unlock();
			return 1;
		}
	}

	if (m_filterCapacity == m_filterCount + 1) {
		if (!expandFilterTable()) {
			m_mutex.Unlock();
			return 0;
		}
	}

	m_filters[m_filterCount].mask = mask;
	m_filters[m_filterCount].pid = pid;
	m_filters[m_filterCount].callback = callback;
	++m_filterCount;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::AddPidFilterMask(uint16_t pid, uint32_t mask)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	StreamFilterEntry *found = 0;
	for (int32_t i = 0; i < m_filterCapacity; ++i) {
		if (m_filters[i].pid == pid) {
			found = &m_filters[i];
			break;
		}
	}
	if (found != 0)
		found->mask |= mask;
	m_mutex.Unlock();
	return found != 0;
}

int StreamInfo::RemovePidFilterMask(uint16_t pid, uint32_t mask)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	StreamFilterEntry *found = 0;
	for (int32_t i = 0; i < m_filterCapacity; ++i) {
		if (m_filters[i].pid == pid) {
			found = &m_filters[i];
			break;
		}
	}
	if (found != 0)
		found->mask &= ~mask;
	m_mutex.Unlock();
	return found != 0;
}

int StreamInfo::RemovePidFilter(uint16_t pid)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	int found = 0;
	for (int32_t i = 0; i < m_filterCapacity; ++i) {
		if (found)
			m_filters[i - 1] = m_filters[i];
		if (m_filters[i].pid == pid)
			found = 1;
	}
	if (found) {
		m_filters[m_filterCount].pid = 0xFFFF;
		m_filters[m_filterCount].mask = 0;
		m_filters[m_filterCount].callback = 0;
		--m_filterCount;
	}
	m_mutex.Unlock();
	return 0;
}

void *StreamInfo::FindPidFilter(uint16_t pid, uint32_t requiredMask)
{
	if (m_filters == 0)
		return 0;

	void *cb = 0;
	m_mutex.Lock(0);
	for (int32_t i = 0; i < m_filterCount; ++i) {
		if (m_filters[i].pid == pid) {
			if ((m_filters[i].mask & requiredMask) == requiredMask)
				cb = m_filters[i].callback;
			break;
		}
	}
	m_mutex.Unlock();
	return cb;
}

int StreamInfo::StartNewPATParse(int32_t estimatedProgramCount)
{
	if (m_filters == 0)
		return 0;

	m_mutex.Lock(0);
	if (estimatedProgramCount > m_programCapacity && m_programs != 0)
		freePrograms();

	if (m_programs == 0) {
		m_programs = new ProgramEntry[estimatedProgramCount];
		if (m_programs == 0) {
			beLogPrintf(2, "[DEBUG] %s : malloc errorn\n", "StartNewPATParse");
			m_mutex.Unlock();
			return 0;
		}
		m_programCapacity = estimatedProgramCount;
	}

	for (int32_t i = 0; i < estimatedProgramCount; ++i) {
		m_programs[i].pmtPid = 0xFFFF;
		m_programs[i].esCapacity = 0;
		m_programs[i].esCount = 0;
		if (m_programs[i].esList != 0)
			delete[] m_programs[i].esList;
		m_programs[i].esList = 0;
		m_programs[i].pmtComplete = 0;
	}
	m_patComplete = 0;
	m_programCount = 0;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::AddProgram(uint16_t pmtPid, uint16_t programNumber)
{
	if (m_filters == 0 || pmtPid == 0xFFFF || m_patComplete == 1)
		return 0;

	m_mutex.Lock(0);
	ProgramEntry *entry = 0;
	for (int32_t i = 0; i < m_programCount; ++i) {
		if (m_programs[i].pmtPid == pmtPid) {
			entry = &m_programs[i];
			break;
		}
	}

	if (entry == 0) {
		if (m_programCount >= m_programCapacity) {
			beLogPrintf(2, "[DEBUG] %s : the count of program is over the estimated value!\n", "AddProgram");
			m_mutex.Unlock();
			return 0;
		}
		entry = &m_programs[m_programCount++];
	}

	entry->pmtPid = 0xFFFF;
	entry->esCapacity = 0;
	entry->esCount = 0;
	if (entry->esList != 0)
		delete[] entry->esList;
	entry->pmtPid = pmtPid;
	entry->esList = 0;
	entry->pmtComplete = 0;
	entry->programNumber = programNumber;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::FinishPATParse()
{
	if (m_filters == 0 || m_patComplete == 1)
		return 0;

	m_mutex.Lock(0);
	m_patComplete = 1;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::StartNewPMTParse(uint16_t pmtPid, int32_t estimatedEsCount)
{
	if (m_filters == 0 || pmtPid == 0xFFFF || !m_patComplete || m_programs == 0)
		return 0;

	m_mutex.Lock(0);
	ProgramEntry *entry = 0;
	for (int32_t i = 0; i < m_programCount; ++i) {
		if (m_programs[i].pmtPid == pmtPid) {
			entry = &m_programs[i];
			break;
		}
	}

	if (entry == 0) {
		m_mutex.Unlock();
		return 0;
	}

	if (entry->esList != 0 && estimatedEsCount > entry->esCapacity) {
		delete[] entry->esList;
		entry->esList = 0;
		entry->esCapacity = 0;
	}

	if (entry->esList == 0) {
		entry->esList = new EsStreamEntry[estimatedEsCount];
		if (entry->esList == 0) {
			beLogPrintf(2, "[DEBUG] %s : malloc error!\n", "StartNewPMTParse");
			m_mutex.Unlock();
			return 0;
		}
		entry->esCapacity = estimatedEsCount;
	}

	for (int32_t i = 0; i < estimatedEsCount; ++i) {
		entry->esList[i].pid = 0xFFFF;
		entry->esList[i].avStreamType = BE_AV_STREAM_INVALID;
	}
	entry->pmtComplete = 0;
	entry->esCount = 0;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::AddElementaryStreamInfo(uint16_t pmtPid, uint16_t esPid, uint8_t streamType, int32_t avStreamType)
{
	if (m_filters == 0 || pmtPid == 0xFFFF || esPid == 0xFFFF || !m_patComplete || m_programs == 0)
		return 0;

	m_mutex.Lock(0);
	ProgramEntry *prog = 0;
	for (int32_t i = 0; i < m_programCount; ++i) {
		if (m_programs[i].pmtPid == pmtPid) {
			prog = &m_programs[i];
			break;
		}
	}

	if (prog == 0 || prog->pmtComplete == 1 || prog->esList == 0) {
		m_mutex.Unlock();
		return 0;
	}

	EsStreamEntry *es = 0;
	for (int32_t i = 0; i < prog->esCount; ++i) {
		if (prog->esList[i].pid == esPid) {
			es = &prog->esList[i];
			break;
		}
	}

	if (es == 0) {
		if (prog->esCount >= prog->esCapacity) {
			beLogPrintf(2, "[DEBUG] %s : the count of es is over the estimated value!\n",
				"AddElementaryStreamInfo");
			m_mutex.Unlock();
			return 0;
		}
		es = &prog->esList[prog->esCount++];
	}

	es->streamType = streamType;
	es->pid = 0xFFFF;
	es->pid = esPid;
	es->avStreamType = avStreamType;
	m_mutex.Unlock();
	return 1;
}

int StreamInfo::FinishPMTParse(uint16_t pmtPid)
{
	if (m_filters == 0 || pmtPid == 0xFFFF || !m_patComplete || m_programs == 0)
		return 0;

	m_mutex.Lock(0);
	for (int32_t i = 0; i < m_programCount; ++i) {
		if (m_programs[i].pmtPid == pmtPid) {
			if (m_programs[i].pmtComplete != 1 && m_programs[i].esList != 0) {
				m_programs[i].pmtComplete = 1;
				m_mutex.Unlock();
				return 1;
			}
			break;
		}
	}
	m_mutex.Unlock();
	return 0;
}
