#include "be_mp2sys_internal.h"

/* --------------------------------------------------------------------------
 * Global stream-type predicate hooks
 * -------------------------------------------------------------------------- */

BeStreamTypePredicate g_isVideoStreamFunc = 0;
BeStreamTypePredicate g_isAudioStreamFunc = 0;
void *g_streamPredicateUserData = 0;

/* --------------------------------------------------------------------------
 * Pure-virtual handler and non-pure interface methods
 * -------------------------------------------------------------------------- */

extern "C" int __cxa_pure_virtual(void) { return 0; }

void IStreamInfoSink::onTsPacket(PidInfo *, const uint8_t *, int32_t, const uint8_t *, int32_t) {}

int32_t ITsPayloadParser::GetParserType() { return m_parserType; }
void ITsPayloadParser::SetState(int32_t state) { m_state = state; }

/* --------------------------------------------------------------------------
 * Control parameter structs for PesParser, TsDemuxer, StreamInfo
 * -------------------------------------------------------------------------- */

struct PesSetStateArg {
	int16_t state;
};

struct PesGetInfoArg {
	PesPacketInfo *outInfo;
};

struct DemuxerInitArg {
	IPesHostCallback *pesHost;
	void *userData;
	ITsDemuxerEventCallback *eventCallback;
	int32_t initialBlocks;
	int32_t growBlocks;
	int32_t maxFreeKeep;
	int32_t extraHeaderSize;
	int32_t reservedField;
	StreamInfoHost *streamInfoHost;
};

struct DemuxerFindPidArg {
	int16_t pid;
	PidInfo *outPidInfo;
};

struct DemuxerGetNextChildArg {
	PidInfo *parent;
	PidInfo *current;
	int32_t found;
	PidInfo *next;
};

struct DemuxerFindProgramArg {
	int32_t programNumber;
	PidInfo *outPidInfo;
};

struct DemuxerFindNextEsArg {
	PidInfo *pmtPidInfo;
	PidInfo *currentEs;
	int32_t targetAvStreamType;
	PidInfo *outEs;
};

struct DemuxerFindFirstEsArg {
	PidInfo *pmtPidInfo;
	int32_t targetAvStreamType;
	PidInfo *outEs;
};

struct DemuxerFindEsByPidArg {
	PidInfo *pmtPidInfo;
	uint32_t esPid;
	PidInfo *outEs;
};

struct DemuxerSetPtsOffsetArg {
	uint32_t offsetLo;
	uint32_t offsetHi;
};

struct DemuxerAttachStreamInfoArg {
	StreamInfoHost *host;
};

struct DemuxerIsPesPidArg {
	int16_t pid;
	int32_t isPes;
};

struct DemuxerSetPesCallbackArg {
	int16_t pid;
	IPesParserCallback *callback;
	int32_t success;
};

struct DemuxerGetAvStreamTypeArg {
	int16_t pid;
	int32_t avStreamType;
};

struct DemuxerGetSectionCallbackArg {
	int16_t pid;
	ISectionParserCallback *callback;
};

struct DemuxerSetSectionCallbackArg {
	int16_t pid;
	ISectionParserCallback *callback;
	int32_t success;
};

struct DemuxerGetEsCountArg {
	PidInfo *pmtPidInfo;
	int32_t esCount;
};

struct DemuxerSetPesStateArg {
	int16_t pid;
	int32_t state;
	int32_t success;
};

struct DemuxerSetStreamTypeMapArg {
	uint8_t streamType;
	int32_t newAvStreamType;
	int32_t success;
	int32_t prevAvStreamType;
};

struct DemuxerGetStreamTypeMapArg {
	int8_t streamType;
	int32_t success;
	int32_t avStreamType;
};

struct DemuxerSetFlagsArg {
	uint32_t flags;
	int32_t success;
};

struct StreamInfoInitArg {
	IStreamInfoSink *sink;
};

struct StreamInfoGetProgramListArg {
	int32_t maxCount;
	uint16_t *outPairs;
	int32_t count;
};

struct StreamInfoProgramPidArg {
	uint16_t key;
	uint16_t value;
	int32_t found;
};

struct StreamInfoPmtQueryArg {
	uint16_t pmtPid;
	int32_t result;
};

struct StreamInfoGetEsListArg {
	uint16_t pmtPid;
	int32_t maxCount;
	EsStreamEntry *outEntries;
	int32_t count;
};

struct StreamInfoGetEsAvTypeArg {
	uint16_t pmtPid;
	uint16_t esPid;
	int32_t avStreamType;
	int32_t found;
};

struct StreamInfoGetPidDescArg {
	uint16_t pmtPid;
	uint16_t targetPid;
	int32_t descLength;
	const uint8_t *descPtr;
};

struct StreamInfoSetFilterArg {
	uint16_t pid;
	uint32_t mask;
	void *callback;
	int32_t result;
};

struct StreamInfoFilterMaskArg {
	uint16_t pid;
	uint32_t mask;
	int32_t result;
};

struct StreamInfoRemoveFilterArg {
	uint16_t pid;
	int32_t result;
};

typedef int (*PesCtrlHandler)(PesParser *parser, int32_t cmd, void *arg);
typedef int (*DemuxerCtrlHandler)(TsDemuxer *demuxer, int32_t cmd, void *arg);
typedef int (*StreamInfoCtrlHandler)(StreamInfo *streamInfo, int32_t cmd, void *arg);

/* --------------------------------------------------------------------------
 * PesParser control handlers (commands 0..1)
 * -------------------------------------------------------------------------- */

static int pesCtrlSetState(PesParser *parser, int32_t cmd, void *rawArg)
{
	if (parser == 0 || cmd != 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	PesSetStateArg *arg = (PesSetStateArg *)rawArg;
	parser->m_state = arg->state;
	return 0;
}

static int pesCtrlGetPacketInfo(PesParser *parser, int32_t cmd, void *rawArg)
{
	if (parser == 0 || cmd != 1)
		return BE_MP2SYS_ERR_INVALID_ARG;
	PesGetInfoArg *arg = (PesGetInfoArg *)rawArg;
	arg->outInfo = &parser->m_packetInfo;
	return 0;
}

static PesCtrlHandler g_pesCtrlTable[2] = {
	pesCtrlSetState,
	pesCtrlGetPacketInfo,
};

/* --------------------------------------------------------------------------
 * TsDemuxer control handlers (commands 0..22)
 * -------------------------------------------------------------------------- */

static int demuxerCtrlFindPidInfo(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerFindPidArg *arg = (DemuxerFindPidArg *)rawArg;
	arg->outPidInfo = demuxer->findPidInfo(arg->pid);
	return 0;
}

static int demuxerCtrlGetNextProgram(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 1)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerGetNextChildArg *arg = (DemuxerGetNextChildArg *)rawArg;
	PidInfo *current = arg->current;
	arg->found = 0;
	arg->next = 0;
	if (current != 0) {
		if (current->parser->GetParserType() == BE_PARSER_TYPE_PMT) {
			arg->found = 1;
			arg->next = current->nextSibling;
		}
	} else if (arg->parent != 0) {
		arg->found = 1;
		arg->next = arg->parent->childList;
	}
	return 0;
}

static int demuxerCtrlFindProgramByNumber(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 2)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerFindProgramArg *arg = (DemuxerFindProgramArg *)rawArg;
	int32_t targetProg = arg->programNumber;
	PidInfo *cur = demuxer->m_patPidInfo.childList;
	while (cur != 0) {
		if (cur->programNumber == targetProg)
			break;
		cur = cur->nextSibling;
	}
	arg->outPidInfo = cur;
	return 0;
}

static int demuxerCtrlFindNextEs(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 3)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerFindNextEsArg *arg = (DemuxerFindNextEsArg *)rawArg;
	PidInfo *pmt = arg->pmtPidInfo;
	PidInfo *current = arg->currentEs;
	int32_t target = arg->targetAvStreamType;

	if (pmt == 0 || pmt->parser == 0 || pmt->parser->GetParserType() != BE_PARSER_TYPE_PMT ||
	    current == 0 || current->parser == 0 || current->parser->GetParserType() != BE_PARSER_TYPE_PES) {
		arg->outEs = 0;
		return 0;
	}

	PidInfo *cur = pmt->childList;
	int seenCurrent = 0;
	while (cur != 0) {
		if (cur == current) {
			seenCurrent = 1;
		} else if (seenCurrent) {
			if (cur->parser != 0 && cur->parser->GetParserType() == BE_PARSER_TYPE_PES) {
				int32_t actualType = getParserPesInfo(cur->parser)->avStreamType;
				if (target == BE_AV_STREAM_VIDEO_ANY) {
					if (isVideoAvStreamType(actualType))
						break;
				} else if (target == BE_AV_STREAM_AUDIO_ANY) {
					if (isAudioAvStreamType(actualType))
						break;
				} else if (actualType == target) {
					break;
				}
			}
		}
		cur = cur->nextSibling;
	}
	arg->outEs = cur;
	return 0;
}

static int demuxerCtrlFindFirstEs(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 4)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerFindFirstEsArg *arg = (DemuxerFindFirstEsArg *)rawArg;
	PidInfo *pmt = arg->pmtPidInfo;
	int32_t target = arg->targetAvStreamType;
	PidInfo *found = 0;

	if (pmt != 0 && pmt->parser != 0 && pmt->parser->GetParserType() == BE_PARSER_TYPE_PMT) {
		PidInfo *cur = pmt->childList;
		while (cur != 0) {
			if (cur->parser != 0 && cur->parser->GetParserType() == BE_PARSER_TYPE_PES) {
				int32_t actualType = getParserPesInfo(cur->parser)->avStreamType;
				if (target == BE_AV_STREAM_VIDEO_ANY) {
					if (isVideoAvStreamType(actualType)) {
						found = cur;
						break;
					}
				} else if (target == BE_AV_STREAM_AUDIO_ANY) {
					if (isAudioAvStreamType(actualType)) {
						found = cur;
						break;
					}
				} else if (actualType == target) {
					found = cur;
					break;
				}
			}
			cur = cur->nextSibling;
		}
	}
	arg->outEs = found;
	return 0;
}

static int demuxerCtrlFindEsByPid(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 5)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerFindEsByPidArg *arg = (DemuxerFindEsByPidArg *)rawArg;
	PidInfo *cur = arg->pmtPidInfo->childList;
	while (cur != 0) {
		if (cur->pid == (int32_t)(uint16_t)arg->esPid)
			break;
		cur = cur->nextSibling;
	}
	arg->outEs = cur;
	return 0;
}

static int demuxerCtrlGetMaxPmtCount(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 6)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(int32_t *)rawArg = demuxer->m_maxPmtCount;
	return 0;
}

static int demuxerCtrlGetBitrate(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 7)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(int32_t *)rawArg = demuxer->m_tsParser.m_bitrate;
	return 0;
}

static int demuxerCtrlSetPtsOffset(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 8)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetPtsOffsetArg *arg = (DemuxerSetPtsOffsetArg *)rawArg;
	demuxer->m_ptsOffset = (int64_t)(((uint64_t)arg->offsetHi << 32) | arg->offsetLo);
	return 0;
}

static int demuxerCtrlAttachStreamInfo(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 9)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerAttachStreamInfoArg *arg = (DemuxerAttachStreamInfoArg *)rawArg;
	demuxer->attachStreamInfo(arg->host);
	return 0;
}

static int demuxerCtrlIsPesPid(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 10)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerIsPesPidArg *arg = (DemuxerIsPesPidArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->isPes = 0;
	if (info != 0 && info->parser != 0 && info->parser->GetParserType() == BE_PARSER_TYPE_PES)
		arg->isPes = 1;
	return 0;
}

static int demuxerCtrlSetPesCallback(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 11)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetPesCallbackArg *arg = (DemuxerSetPesCallbackArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->success = 0;
	if (info != 0 && info->parser != 0 && info->parser->GetParserType() == BE_PARSER_TYPE_PES) {
		PesPacketInfo *pesInfo = getParserPesInfo(info->parser);
		if (pesInfo != 0)
			pesInfo->callback = arg->callback;
		arg->success = 1;
	}
	return 0;
}

static int demuxerCtrlGetAvStreamType(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 12)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerGetAvStreamTypeArg *arg = (DemuxerGetAvStreamTypeArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->avStreamType = BE_AV_STREAM_INVALID;
	if (info != 0 && info->parser != 0 && info->parser->GetParserType() == BE_PARSER_TYPE_PES) {
		TsPayloadParser *p = info->parser;
		if (getParserPesInfo(p) != 0)
			arg->avStreamType = getParserPesInfo(p)->avStreamType;
	}
	return 0;
}

static int demuxerCtrlGetSectionCallback(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 13)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerGetSectionCallbackArg *arg = (DemuxerGetSectionCallbackArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->callback = 0;
	if (info != 0 && info->parser != 0)
		arg->callback = static_cast<SectionParser *>(info->parser)->m_callback;
	return 0;
}

static int demuxerCtrlSetSectionCallback(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 14)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetSectionCallbackArg *arg = (DemuxerSetSectionCallbackArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->success = 0;
	if (info != 0 && info->parser != 0) {
		static_cast<SectionParser *>(info->parser)->m_callback = arg->callback;
		arg->success = 1;
	}
	return 0;
}

static int demuxerCtrlGetCurrentPidInfo(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 15)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(PidInfo **)rawArg = demuxer->m_tsParser.m_currentPidInfo;
	return 0;
}

static int demuxerCtrlSetExtraHeaderSize(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 16)
		return BE_MP2SYS_ERR_INVALID_ARG;
	int32_t extraSize = *(int32_t *)rawArg;
	if (extraSize <= 0) {
		demuxer->m_tsParser.m_hasExtraHeader = 0;
	} else {
		demuxer->m_tsParser.m_extraHeaderSize = extraSize;
		demuxer->m_tsParser.m_extraHeaderBeforeSync = 1;
		demuxer->m_tsParser.m_hasExtraHeader = 1;
	}
	return 0;
}

static int demuxerCtrlGetPatProgramCount(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 17)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(int32_t *)rawArg = demuxer->m_patParser.m_programCount;
	return 0;
}

static int demuxerCtrlGetPmtEsCount(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 18)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerGetEsCountArg *arg = (DemuxerGetEsCountArg *)rawArg;
	PidInfo *pmt = arg->pmtPidInfo;
	int32_t esCount = -1;
	if (pmt != 0 && pmt->parser != 0 && pmt->parser->GetParserType() == BE_PARSER_TYPE_PMT)
		esCount = static_cast<PmtSectionParser *>(pmt->parser)->m_esCount;
	arg->esCount = esCount;
	return 0;
}

static int demuxerCtrlSetPesState(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 19)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetPesStateArg *arg = (DemuxerSetPesStateArg *)rawArg;
	PidInfo *info = demuxer->findPidInfo(arg->pid);
	arg->success = 0;
	if (info != 0 && info->parser != 0 && info->parser->GetParserType() == BE_PARSER_TYPE_PES) {
		info->parser->SetState(arg->state);
		arg->success = 1;
	}
	return 0;
}

static int demuxerCtrlSetStreamTypeMap(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 20)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetStreamTypeMapArg *arg = (DemuxerSetStreamTypeMapArg *)rawArg;
	uint8_t idx = arg->streamType;
	arg->prevAvStreamType = BE_AV_STREAM_INVALID;
	arg->prevAvStreamType = demuxer->m_streamTypeMap[idx];
	demuxer->m_streamTypeMap[idx] = arg->newAvStreamType;
	arg->success = 1;
	return 0;
}

static int demuxerCtrlGetStreamTypeMap(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 21)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerGetStreamTypeMapArg *arg = (DemuxerGetStreamTypeMapArg *)rawArg;
	arg->avStreamType = demuxer->m_streamTypeMap[(int)arg->streamType];
	arg->success = 1;
	return 0;
}

static int demuxerCtrlOrFlags(TsDemuxer *demuxer, int32_t cmd, void *rawArg)
{
	if (demuxer == 0 || cmd != 22)
		return BE_MP2SYS_ERR_INVALID_ARG;
	DemuxerSetFlagsArg *arg = (DemuxerSetFlagsArg *)rawArg;
	demuxer->m_flags |= arg->flags;
	arg->success = 1;
	return 0;
}

static DemuxerCtrlHandler g_demuxerCtrlTable[23] = {
	demuxerCtrlFindPidInfo,
	demuxerCtrlGetNextProgram,
	demuxerCtrlFindProgramByNumber,
	demuxerCtrlFindNextEs,
	demuxerCtrlFindFirstEs,
	demuxerCtrlFindEsByPid,
	demuxerCtrlGetMaxPmtCount,
	demuxerCtrlGetBitrate,
	demuxerCtrlSetPtsOffset,
	demuxerCtrlAttachStreamInfo,
	demuxerCtrlIsPesPid,
	demuxerCtrlSetPesCallback,
	demuxerCtrlGetAvStreamType,
	demuxerCtrlGetSectionCallback,
	demuxerCtrlSetSectionCallback,
	demuxerCtrlGetCurrentPidInfo,
	demuxerCtrlSetExtraHeaderSize,
	demuxerCtrlGetPatProgramCount,
	demuxerCtrlGetPmtEsCount,
	demuxerCtrlSetPesState,
	demuxerCtrlSetStreamTypeMap,
	demuxerCtrlGetStreamTypeMap,
	demuxerCtrlOrFlags,
};

/* --------------------------------------------------------------------------
 * StreamInfo control handlers (commands 0..13)
 * -------------------------------------------------------------------------- */

static int streamInfoCtrlIsPatComplete(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(int32_t *)rawArg = info->IsPatComplete();
	return 0;
}

static int streamInfoCtrlGetProgramCount(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 1)
		return BE_MP2SYS_ERR_INVALID_ARG;
	*(int32_t *)rawArg = info->GetProgramCount();
	return 0;
}

static int streamInfoCtrlGetProgramList(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 2)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoGetProgramListArg *arg = (StreamInfoGetProgramListArg *)rawArg;
	arg->count = info->GetProgramList(arg->maxCount, arg->outPairs);
	return 0;
}

static int streamInfoCtrlGetPmtPidByProgram(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 3)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoProgramPidArg *arg = (StreamInfoProgramPidArg *)rawArg;
	arg->found = info->GetPmtPidByProgramNumber(arg->key, &arg->value);
	return 0;
}

static int streamInfoCtrlIsPmtComplete(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 4)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoPmtQueryArg *arg = (StreamInfoPmtQueryArg *)rawArg;
	arg->result = info->IsPmtComplete(arg->pmtPid);
	return 0;
}

static int streamInfoCtrlGetProgramByPmtPid(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 5)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoProgramPidArg *arg = (StreamInfoProgramPidArg *)rawArg;
	arg->found = info->GetProgramNumberByPmtPid(arg->key, &arg->value);
	return 0;
}

static int streamInfoCtrlGetEsCount(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 6)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoPmtQueryArg *arg = (StreamInfoPmtQueryArg *)rawArg;
	arg->result = info->GetEsCount(arg->pmtPid);
	return 0;
}

static int streamInfoCtrlGetEsList(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 7)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoGetEsListArg *arg = (StreamInfoGetEsListArg *)rawArg;
	arg->count = info->GetEsList(arg->pmtPid, arg->maxCount, arg->outEntries);
	return 0;
}

static int streamInfoCtrlGetEsAvStreamType(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 8)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoGetEsAvTypeArg *arg = (StreamInfoGetEsAvTypeArg *)rawArg;
	int32_t avType = BE_AV_STREAM_INVALID;
	int found = info->GetEsAvStreamType(arg->pmtPid, arg->esPid, &avType);
	arg->found = found;
	arg->avStreamType = avType;
	return 0;
}

static int streamInfoCtrlGetPidDescriptors(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 9)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoGetPidDescArg *arg = (StreamInfoGetPidDescArg *)rawArg;
	arg->descPtr = info->GetPidDescriptors(arg->pmtPid, arg->targetPid, &arg->descLength);
	return 0;
}

static int streamInfoCtrlSetPidFilter(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 10)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoSetFilterArg *arg = (StreamInfoSetFilterArg *)rawArg;
	arg->result = info->SetPidFilter(arg->pid, arg->mask, arg->callback);
	return 0;
}

static int streamInfoCtrlAddPidFilterMask(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 11)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoFilterMaskArg *arg = (StreamInfoFilterMaskArg *)rawArg;
	arg->result = info->AddPidFilterMask(arg->pid, arg->mask);
	return 0;
}

static int streamInfoCtrlRemovePidFilterMask(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 12)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoFilterMaskArg *arg = (StreamInfoFilterMaskArg *)rawArg;
	arg->result = info->RemovePidFilterMask(arg->pid, arg->mask);
	return 0;
}

static int streamInfoCtrlRemovePidFilter(StreamInfo *info, int32_t cmd, void *rawArg)
{
	if (info == 0 || cmd != 13)
		return BE_MP2SYS_ERR_INVALID_ARG;
	StreamInfoRemoveFilterArg *arg = (StreamInfoRemoveFilterArg *)rawArg;
	arg->result = info->RemovePidFilter(arg->pid);
	return 0;
}

static StreamInfoCtrlHandler g_streamInfoCtrlTable[14] = {
	streamInfoCtrlIsPatComplete,
	streamInfoCtrlGetProgramCount,
	streamInfoCtrlGetProgramList,
	streamInfoCtrlGetPmtPidByProgram,
	streamInfoCtrlIsPmtComplete,
	streamInfoCtrlGetProgramByPmtPid,
	streamInfoCtrlGetEsCount,
	streamInfoCtrlGetEsList,
	streamInfoCtrlGetEsAvStreamType,
	streamInfoCtrlGetPidDescriptors,
	streamInfoCtrlSetPidFilter,
	streamInfoCtrlAddPidFilterMask,
	streamInfoCtrlRemovePidFilterMask,
	streamInfoCtrlRemovePidFilter,
};

/* --------------------------------------------------------------------------
 * Module entry points & C exports
 * -------------------------------------------------------------------------- */

extern "C" {

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

void module_exit(void)
{
}

void SceBemp2sys_491AD648(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn)
{
	beSetAllocator(allocFn, reallocFn, freeFn);
}

void SceBemp2sys_0287CA06(BeLogFunc logFn)
{
	g_logFunc = logFn;
}

int32_t SceBemp2sys_69F282FF(int32_t level)
{
	if (level >= 0 && level <= 3)
		g_logLevel = level;
	return level;
}

int SceBemp2sys_CF2FD310(void)
{
	return g_logLevel;
}

void SceBemp2sys_7F2D4A1D(BeStreamTypePredicate isVideoFn, BeStreamTypePredicate isAudioFn, void *userData)
{
	g_isVideoStreamFunc = isVideoFn;
	g_isAudioStreamFunc = isAudioFn;
	g_streamPredicateUserData = userData;
}

int SceBemp2sys_89D9D1CD(BePacketizer **outPacketizer, uint32_t type)
{
	BePacketizer *pkt = createPacketizer(type);
	*outPacketizer = pkt;
	if (pkt == 0)
		return BE_MP2SYS_ERR_ALLOC_FAILED;
	return 0;
}

void SceBemp2sys_D55C2CBD(BePacketizer *packetizer)
{
	if (packetizer != 0)
		delete packetizer;
}

int SceBemp2sys_7F5BB72A(BePacketizer *packetizer, const void *param)
{
	if (packetizer == 0 || param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	return packetizer->Init(param);
}

void SceBemp2sys_F34C3596(BePacketizer *packetizer)
{
	if (packetizer != 0)
		packetizer->Release();
}

int SceBemp2sys_C7D9EB73(BePacketizer *packetizer, int command, void *arg)
{
	if (packetizer == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	return packetizer->Process(command, arg);
}

int SceBemp2sys_E6C1E8B4(PesParserHandle **outHandle)
{
	PesParserHandle *handle = new PesParserHandle();
	if (handle != 0 && handle->m_parser == 0) {
		delete handle;
		handle = 0;
	}
	*outHandle = handle;
	if (handle == 0)
		return BE_MP2SYS_ERR_ALLOC_FAILED;
	return 0;
}

void SceBemp2sys_9ED9AFC4(PesParserHandle *handle)
{
	if (handle != 0)
		delete handle;
}

int SceBemp2sys_BAC58BBE(PesParserHandle *handle, const void *param)
{
	if (handle == 0 || param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (handle->m_parser != 0 && handle->m_parser->Init() != 0)
		return 0;
	return BE_MP2SYS_ERR_INIT_FAILED;
}

void SceBemp2sys_65F75F5C(PesParserHandle *handle)
{
	if (handle != 0 && handle->m_parser != 0)
		handle->m_parser->Release();
}

int SceBemp2sys_A480BF6B(PesParserHandle *handle, int32_t cmd, void *arg)
{
	if (handle != 0 && arg != 0 && cmd >= 0 && cmd < 2 && handle->m_parser != 0)
		return g_pesCtrlTable[cmd](handle->m_parser, cmd, arg);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

int SceBemp2sys_06F6B63B(PesParserHandle *handle, IPesHostCallback *host, const uint8_t *buf, int32_t size)
{
	if (handle == 0 || host == 0 || buf == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	BitReader reader;
	reader.setBuffer(buf, size);
	if (handle->m_parser != 0)
		return handle->m_parser->Parse(&reader, host, size);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

int SceBemp2sys_BBB64B8E(TsDemuxerHandle **outHandle)
{
	TsDemuxerHandle *handle = new TsDemuxerHandle();
	if (handle != 0 && handle->m_demuxer == 0) {
		delete handle;
		handle = 0;
	}
	*outHandle = handle;
	if (handle == 0)
		return BE_MP2SYS_ERR_ALLOC_FAILED;
	return 0;
}

void SceBemp2sys_9EA526F4(TsDemuxerHandle *handle)
{
	if (handle != 0)
		delete handle;
}

int SceBemp2sys_4EE90DC7(TsDemuxerHandle *handle, const DemuxerInitArg *param)
{
	if (handle == 0 || param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (handle->m_demuxer == 0 ||
	    !handle->m_demuxer->Init(param->pesHost, param->userData, param->eventCallback,
			param->initialBlocks, param->growBlocks, param->maxFreeKeep)) {
		return BE_MP2SYS_ERR_INVALID_ARG;
	}
	handle->m_demuxer->attachStreamInfo(param->streamInfoHost);
	TsDemuxer *demuxer = handle->m_demuxer;
	int32_t extraSize = param->extraHeaderSize;
	if (extraSize <= 0) {
		demuxer->m_tsParser.m_hasExtraHeader = 0;
	} else {
		demuxer->m_tsParser.m_extraHeaderSize = extraSize;
		demuxer->m_tsParser.m_extraHeaderBeforeSync = 1;
		demuxer->m_tsParser.m_hasExtraHeader = 1;
	}
	return 0;
}

void SceBemp2sys_D8F3128B(TsDemuxerHandle *handle)
{
	(void)handle;
}

int SceBemp2sys_541D24AE(TsDemuxerHandle *handle, int32_t cmd, void *arg)
{
	if (handle != 0 && arg != 0 && cmd >= 0 && cmd < 23 && handle->m_demuxer != 0)
		return g_demuxerCtrlTable[cmd](handle->m_demuxer, cmd, arg);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

int SceBemp2sys_6901B91F(TsDemuxerHandle *handle, const uint8_t *buf, int32_t size,
	int32_t *outConsumed, uint64_t userTimestamp)
{
	if (outConsumed != 0 && handle != 0 && handle->m_demuxer != 0)
		return handle->m_demuxer->Parse(buf, size, outConsumed, userTimestamp);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

int SceBemp2sys_81EB635C(StreamInfoHandle **outHandle)
{
	StreamInfoHandle *handle = new StreamInfoHandle();
	if (handle != 0 && handle->m_streamInfo == 0) {
		delete handle;
		handle = 0;
	}
	*outHandle = handle;
	if (handle == 0)
		return BE_MP2SYS_ERR_ALLOC_FAILED;
	return 0;
}

void SceBemp2sys_C877BD9E(StreamInfoHandle *handle)
{
	if (handle != 0)
		delete handle;
}

int SceBemp2sys_958E24CE(StreamInfoHandle *handle, const StreamInfoInitArg *param)
{
	if (handle == 0 || param == 0)
		return BE_MP2SYS_ERR_INVALID_ARG;
	if (handle->m_streamInfo != 0 && handle->m_streamInfo->Init(param->sink) != 0)
		return 0;
	return BE_MP2SYS_ERR_INIT_FAILED;
}

void SceBemp2sys_6D48B143(StreamInfoHandle *handle)
{
	if (handle != 0 && handle->m_streamInfo != 0)
		handle->m_streamInfo->Release();
}

int SceBemp2sys_25E3FEF3(StreamInfoHandle *handle, int32_t cmd, void *arg)
{
	if (handle != 0 && arg != 0 && cmd >= 0 && cmd < 14 && handle->m_streamInfo != 0)
		return g_streamInfoCtrlTable[cmd](handle->m_streamInfo, cmd, arg);
	return BE_MP2SYS_ERR_INVALID_ARG;
}

} /* extern "C" */
