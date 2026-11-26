/*
 * nptrophy_sfm.cpp: TROPCONF.SFM / TROP_XX.SFM configuration data model,
 * signature header stripper, and bXCe SAX parser (0x812EBA18..0x812EDAA3).
 */
#include "nptrophy_sfm.h"

/* bXCe C++ incremental parse retry code (0x00851001 = 8720385) */
static const int BXCE_PARSE_INCOMPLETE = 8720385;

/* --- TrophyConfGroup & TrophyConfEntry (0x812EBA18..0x812EBA83) --- */

TrophyConfGroup::TrophyConfGroup(TrophyConfData *owner, int groupId)
	: m_owner(owner), m_groupId(groupId)
{
}

TrophyConfGroup::~TrophyConfGroup()
{
}

TrophyConfEntry::TrophyConfEntry(TrophyConfData *owner, int trophyId, int orderIndex)
	: m_owner(owner), m_trophyId(trophyId), m_grade(0), m_orderIndex(orderIndex),
	  m_groupId(-2), m_hidden(0), m_platinumId(-1)
{
}

TrophyConfEntry::~TrophyConfEntry()
{
}

int TrophyConfEntry::SetGrade(int grade)
{
	if (grade < NP_TROPHY_GRADE_PLATINUM || grade > NP_TROPHY_GRADE_BRONZE)
		return NP_TROPHY_ERR_INVALID_GRADE;
	m_grade = grade;
	return 0;
}

int TrophyConfEntry::SetGroupId(int groupId)
{
	if (groupId == -2 || (groupId != -1 && static_cast<unsigned int>(groupId) >= 32u))
		return NP_TROPHY_ERR_INVALID_GROUP_ID;
	m_groupId = groupId;
	return 0;
}

int TrophyConfEntry::SetPlatinumId(int platinumId)
{
	if (static_cast<unsigned int>(platinumId) >= 128u)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;
	m_platinumId = platinumId;
	return 0;
}

/* --- TrophyConfData (0x812EBA84..0x812EC5CB) --- */

TrophyConfData::TrophyConfData()
	: m_confVersion(0), m_trophySetVersion(0), m_featureFlags(0), m_platformMask(0)
{
	sceClibMemset(m_trophies, 0, sizeof(m_trophies));
	sceClibMemset(m_groups, 0, sizeof(m_groups));
}

TrophyConfData::~TrophyConfData()
{
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] != NULL) {
			delete m_trophies[index];
			m_trophies[index] = NULL;
		}
	}
	for (int index = 0; index < 32; ++index) {
		if (m_groups[index] != NULL) {
			delete m_groups[index];
			m_groups[index] = NULL;
		}
	}
}

int TrophyConfData::SetConfVersion(uint32_t version)
{
	if (version == 0x10001u) {
		m_featureFlags |= 1u;
	} else if (version != 0x10000u) {
		return NP_TROPHY_ERR_UNSUPPORTED_VERSION;
	}
	m_confVersion = version;
	return 0;
}

int TrophyConfData::SetCommId(const TrophyCommId &commId)
{
	if (commId.IsEmpty())
		return NP_TROPHY_ERR_INVALID_COMM_ID;
	m_commId = commId;
	return 0;
}

int TrophyConfData::SetTrophySetVersion(uint32_t version)
{
	if (version == 0)
		return NP_TROPHY_ERR_BROKEN_CONF;
	m_trophySetVersion = version;
	return 0;
}

int TrophyConfData::AddGroup(uint32_t groupId, TrophyConfGroup **outGroup)
{
	if (!SupportsGroups())
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;
	if (groupId >= 32u || m_groups[groupId] != NULL)
		return NP_TROPHY_ERR_INVALID_GROUP_ID;

	TrophyConfGroup *group = new TrophyConfGroup(this, static_cast<int>(groupId));
	if (group == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	m_groups[groupId] = group;
	if (outGroup != NULL)
		*outGroup = group;
	return 0;
}

int TrophyConfData::GetGroup(uint32_t groupId, TrophyConfGroup **outGroup) const
{
	if (!SupportsGroups())
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;
	if (groupId >= 32u || m_groups[groupId] == NULL)
		return NP_TROPHY_ERR_INVALID_GROUP_ID;
	*outGroup = m_groups[groupId];
	return 0;
}

int TrophyConfData::AddTrophy(uint32_t trophyId, int orderIndex, TrophyConfEntry **outEntry)
{
	if (trophyId >= 128u || m_trophies[trophyId] != NULL)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;

	TrophyConfEntry *entry = new TrophyConfEntry(this, static_cast<int>(trophyId), orderIndex);
	if (entry == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	m_trophies[trophyId] = entry;
	if (outEntry != NULL)
		*outEntry = entry;
	return 0;
}

int TrophyConfData::GetTrophy(uint32_t trophyId, TrophyConfEntry **outEntry) const
{
	if (trophyId >= 128u || m_trophies[trophyId] == NULL)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;
	*outEntry = m_trophies[trophyId];
	return 0;
}

int TrophyConfData::Validate()
{
	if (m_commId.IsEmpty())
		return NP_TROPHY_ERR_INVALID_COMM_ID;

	if (m_confVersion == 0x10001u) {
		m_featureFlags |= 1u;
	} else if (m_confVersion != 0x10000u) {
		return NP_TROPHY_ERR_UNSUPPORTED_VERSION;
	}

	if (m_trophySetVersion == 0)
		return NP_TROPHY_ERR_BROKEN_CONF;

	int platinumCount = 0;
	for (int index = 0; index < 128; ++index) {
		TrophyConfEntry *entry = m_trophies[index];
		if (entry == NULL)
			continue;
		if (entry->GetTrophyId() != index)
			return NP_TROPHY_ERR_INVALID_TROPHY_ID;
		int grade = entry->GetGrade();
		if (grade == 0)
			return NP_TROPHY_ERR_INVALID_GRADE;
		int groupId = entry->GetGroupId();
		if (groupId != -2 && groupId != -1) {
			if (static_cast<unsigned int>(groupId) >= 32u || m_groups[groupId] == NULL)
				return NP_TROPHY_ERR_INVALID_GROUP_ID;
		}
		if (grade == NP_TROPHY_GRADE_PLATINUM) {
			if (entry->GetPlatinumId() != -1)
				return NP_TROPHY_ERR_BROKEN_CONF;
			if (m_platformMask == 0)
				return NP_TROPHY_ERR_BROKEN_CONF;
			++platinumCount;
		}
	}

	if (platinumCount > 1)
		return NP_TROPHY_ERR_BROKEN_CONF;

	for (int index = 0; index < 32; ++index) {
		if (m_groups[index] != NULL && m_groups[index]->GetGroupId() != index)
			return NP_TROPHY_ERR_INVALID_GROUP_ID;
	}
	return 0;
}

int TrophyConfData::GetGroupBitset(uint32_t *outMask, uint32_t *outCount) const
{
	if (!SupportsGroups())
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;
	if (outMask != NULL)
		*outMask = 0;
	if (outCount != NULL)
		*outCount = 0;
	for (int index = 0; index < 32; ++index) {
		if (m_groups[index] != NULL) {
			if (outMask != NULL)
				*outMask |= (1u << index);
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::GetAllTrophyBitset(NpTrophyFlagArray *outBits, uint32_t *outCount) const
{
	if (outCount != NULL)
		*outCount = 0;
	if (outBits != NULL)
		sceClibMemset(outBits, 0, sizeof(*outBits));
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] != NULL) {
			if (outBits != NULL)
				outBits->flagBits[index >> 5] |= (1u << (index & 31));
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::GetPlatinumTargetBitset(uint32_t platinumTrophyId, NpTrophyFlagArray *outBits,
					    uint32_t *outCount) const
{
	if (outBits != NULL)
		sceClibMemset(outBits, 0, sizeof(*outBits));
	if (outCount != NULL)
		*outCount = 0;
	if (platinumTrophyId >= 128u || m_trophies[platinumTrophyId] == NULL)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;
	if (m_trophies[platinumTrophyId]->GetGrade() != NP_TROPHY_GRADE_PLATINUM)
		return NP_TROPHY_ERR_NOT_PLATINUM;

	for (uint32_t index = 0; index < 128u; ++index) {
		TrophyConfEntry *entry = m_trophies[index];
		if (entry != NULL &&
		    static_cast<uint32_t>(entry->GetPlatinumId()) == platinumTrophyId &&
		    index != platinumTrophyId) {
			if (outBits != NULL)
				outBits->flagBits[index >> 5] |= (1u << (index & 31));
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::GetHiddenTrophyBitset(NpTrophyFlagArray *outBits, uint32_t *outCount) const
{
	if (outCount != NULL)
		*outCount = 0;
	if (outBits != NULL)
		sceClibMemset(outBits, 0, sizeof(*outBits));
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] != NULL && m_trophies[index]->IsHidden()) {
			if (outBits != NULL)
				outBits->flagBits[index >> 5] |= (1u << (index & 31));
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::GetGradeTrophyBitset(int grade, NpTrophyFlagArray *outBits,
					 uint32_t *outCount) const
{
	if (outCount != NULL)
		*outCount = 0;
	if (outBits != NULL)
		sceClibMemset(outBits, 0, sizeof(*outBits));
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] != NULL && m_trophies[index]->GetGrade() == grade) {
			if (outBits != NULL)
				outBits->flagBits[index >> 5] |= (1u << (index & 31));
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::GetGroupTrophyBitset(int groupId, NpTrophyFlagArray *outBits,
					 uint32_t *outCount) const
{
	if (!SupportsGroups())
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;
	if (outBits != NULL)
		sceClibMemset(outBits, 0, sizeof(*outBits));
	if (outCount != NULL)
		*outCount = 0;
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] != NULL && m_trophies[index]->GetGroupId() == groupId) {
			if (outBits != NULL)
				outBits->flagBits[index >> 5] |= (1u << (index & 31));
			if (outCount != NULL)
				++(*outCount);
		}
	}
	return 0;
}

int TrophyConfData::CountByGrade(TrophyGradeCounts *outCounts) const
{
	sceClibMemset(outCounts, 0, sizeof(*outCounts));
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] == NULL)
			continue;
		switch (m_trophies[index]->GetGrade()) {
		case NP_TROPHY_GRADE_PLATINUM:
			++outCounts->platinum;
			break;
		case NP_TROPHY_GRADE_GOLD:
			++outCounts->gold;
			break;
		case NP_TROPHY_GRADE_SILVER:
			++outCounts->silver;
			break;
		case NP_TROPHY_GRADE_BRONZE:
			++outCounts->bronze;
			break;
		default:
			break;
		}
	}
	return 0;
}

int TrophyConfData::CountGroupByGrade(int groupId, TrophyGradeCounts *outCounts) const
{
	sceClibMemset(outCounts, 0, sizeof(*outCounts));
	for (int index = 0; index < 128; ++index) {
		if (m_trophies[index] == NULL || m_trophies[index]->GetGroupId() != groupId)
			continue;
		switch (m_trophies[index]->GetGrade()) {
		case NP_TROPHY_GRADE_PLATINUM:
			++outCounts->platinum;
			break;
		case NP_TROPHY_GRADE_GOLD:
			++outCounts->gold;
			break;
		case NP_TROPHY_GRADE_SILVER:
			++outCounts->silver;
			break;
		case NP_TROPHY_GRADE_BRONZE:
			++outCounts->bronze;
			break;
		default:
			break;
		}
	}
	return 0;
}

int TrophyConfData::BuildGradeBitsets(TrophyGradeBitsets *outBitsets) const
{
	GetGradeTrophyBitset(NP_TROPHY_GRADE_PLATINUM, &outBitsets->platinum, NULL);
	GetGradeTrophyBitset(NP_TROPHY_GRADE_GOLD, &outBitsets->gold, NULL);
	GetGradeTrophyBitset(NP_TROPHY_GRADE_SILVER, &outBitsets->silver, NULL);
	GetGradeTrophyBitset(NP_TROPHY_GRADE_BRONZE, &outBitsets->bronze, NULL);
	return 0;
}

int TrophyConfData::ExtractTitleDetails(TrophyConfTitleDetails *outDetails) const
{
	sceClibMemset(outDetails, 0, sizeof(*outDetails));
	m_commId.CopyTo(&outDetails->commId);
	outDetails->platformMask = m_platformMask;

	int err = m_titleName.CopyToString(outDetails->titleName, sizeof(outDetails->titleName));
	if (err < 0)
		return err;
	err = m_titleDetail.CopyToString(outDetails->titleDetail, sizeof(outDetails->titleDetail));
	if (err < 0)
		return err;
	err = CountByGrade(&outDetails->gradeCounts);
	if (err < 0)
		return err;

	if (SupportsGroups()) {
		uint32_t groupCount = 0;
		for (int index = 0; index < 32; ++index) {
			if (m_groups[index] != NULL)
				++groupCount;
		}
		outDetails->numGroups = groupCount;
	}
	return 0;
}

int TrophyConfData::ExtractTrophyDetails(uint32_t trophyId,
					 TrophyConfTrophyDetails *outDetails) const
{
	TrophyConfEntry *entry = NULL;
	int err = GetTrophy(trophyId, &entry);
	if (err < 0)
		return err;

	sceClibMemset(outDetails, 0, sizeof(*outDetails));
	outDetails->trophyId = static_cast<int>(trophyId);
	outDetails->grade = entry->GetGrade();
	outDetails->groupId = entry->GetGroupId();
	outDetails->hidden = entry->IsHidden() ? 1 : 0;

	err = entry->GetNameBuffer()->CopyToString(outDetails->name, sizeof(outDetails->name));
	if (err < 0)
		return err;
	err = entry->GetDetailBuffer()->CopyToString(outDetails->detail, sizeof(outDetails->detail));
	return (err < 0) ? err : 0;
}

/* --- TrophySfmSignatureStripper (0x812EC5CC..0x812ECBBB) --- */

TrophySfmSignatureStripper::TrophySfmSignatureStripper()
	: m_source(NULL), m_headerSize(0), m_state(STRIP_STATE_HEADER),
	  m_buffer(NULL), m_bufferCapacity(0), m_bufferLength(0), m_signature(NULL)
{
}

TrophySfmSignatureStripper::~TrophySfmSignatureStripper()
{
	if (m_signature != NULL) {
		npTrophyHeapFree(m_signature);
		m_signature = NULL;
	}
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_buffer = NULL;
	}
}

int TrophySfmSignatureStripper::Init(TrophyInputStream *source)
{
	char *buf = static_cast<char *>(npTrophyHeapAlloc(512));
	if (buf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	m_buffer = buf;
	m_source = source;
	m_bufferCapacity = 512;
	m_bufferLength = 0;
	m_headerSize = 0;
	m_state = STRIP_STATE_HEADER;
	return 0;
}

int TrophySfmSignatureStripper::Term()
{
	if (m_source == NULL)
		return NP_TROPHY_ERR_STREAM_NOT_INIT;
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_buffer = NULL;
	}
	m_bufferCapacity = 0;
	m_bufferLength = 0;
	m_source = NULL;
	m_state = STRIP_STATE_HEADER;
	return 0;
}

int TrophySfmSignatureStripper::ParseSignatureHex(const char *hexText, SceSize availLen,
						  SceSize *outConsumed)
{
	if (m_signature != NULL)
		return NP_TROPHY_ERR_BROKEN_CONF;

	SceSize hexLen = 0;
	while (hexLen < availLen && hexText[hexLen] != ' ' && hexText[hexLen] != '-')
		++hexLen;
	if (hexLen != 320)
		return NP_TROPHY_ERR_BROKEN_CONF;

	if (npTrophyHexDigitValue(hexText[0]) < 0) {
		*outConsumed = 320;
		m_signature = NULL;
		return 0;
	}

	TrophySfmSignature *sig = static_cast<TrophySfmSignature *>(
		npTrophyHeapAlloc(sizeof(TrophySfmSignature)));
	if (sig == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	sceClibMemset(sig, 0, sizeof(*sig));

	for (int index = 0; index < 160; ++index) {
		int hi = npTrophyHexDigitValue(hexText[index * 2]);
		int lo = npTrophyHexDigitValue(hexText[index * 2 + 1]);
		if (hi < 0 || lo < 0) {
			npTrophyHeapFree(sig);
			return NP_TROPHY_ERR_BROKEN_CONF;
		}
		sig->bytes[index] = static_cast<uint8_t>((hi << 4) | lo);
	}
	sig->length = 160;
	m_signature = sig;
	*outConsumed = 320;
	return 0;
}

int TrophySfmSignatureStripper::ConsumeLeadingComment(const char *buf, SceSize availLen,
						      SceSize *outConsumed, int *outDone)
{
	*outDone = 0;
	if (availLen < 4) {
		*outConsumed = 0;
		return 0;
	}
	if (sceClibStrncmp(buf, "<!--", 4) != 0) {
		*outConsumed = 0;
		*outDone = 1;
		return 0;
	}

	SceSize lineEnd = 0;
	while (lineEnd < availLen && buf[lineEnd] != '\n')
		++lineEnd;
	if (lineEnd >= availLen) {
		*outConsumed = 0;
		return 0;
	}

	static const char sigPrefix[] = "<!--Sce-Np-Trophy-Signature: ";
	static const SceSize sigPrefixLen = 29;
	if (sceClibStrncmp(buf, sigPrefix, sigPrefixLen) == 0) {
		SceSize hexConsumed = 0;
		int err = ParseSignatureHex(buf + sigPrefixLen, availLen - sigPrefixLen,
					    &hexConsumed);
		if (err < 0)
			return err;
		if (sceClibStrncmp(buf + sigPrefixLen + hexConsumed, "-->\n", 4) != 0)
			return NP_TROPHY_ERR_BROKEN_CONF;
		*outConsumed = sigPrefixLen + hexConsumed + 4;
		return 0;
	}

	*outConsumed = lineEnd + 1;
	return 0;
}

int TrophySfmSignatureStripper::ReadAndStripHeader(TrophyHandle *handle)
{
	int done = 0;
	while (done == 0) {
		SceSize space = m_bufferCapacity - m_bufferLength;
		if (space == 0)
			return NP_TROPHY_ERR_BROKEN_CONF;

		SceSize got = 0;
		int err = m_source->Read(handle, m_buffer + m_bufferLength, space, &got);
		if (err < 0)
			return err;
		m_bufferLength += got;

		SceSize totalConsumed = 0;
		for (;;) {
			SceSize consumed = 0;
			err = ConsumeLeadingComment(m_buffer + totalConsumed,
						    m_bufferLength - totalConsumed,
						    &consumed, &done);
			if (err < 0)
				return err;
			totalConsumed += consumed;
			if (consumed == 0 || done != 0)
				break;
		}

		if (m_bufferLength > totalConsumed) {
			SceSize remaining = m_bufferLength - totalConsumed;
			sceClibMemmove(m_buffer, m_buffer + totalConsumed, remaining);
			m_bufferLength = remaining;
			sceClibMemset(m_buffer + remaining, 0, m_bufferCapacity - remaining);
		} else {
			sceClibMemset(m_buffer, 0, m_bufferLength);
			m_bufferLength = 0;
		}
		m_headerSize += totalConsumed;
	}
	return 0;
}

int TrophySfmSignatureStripper::ReadBodyChunk(TrophyHandle *handle, void *outBuf, SceSize size,
					      SceSize *outRead)
{
	if (m_bufferLength != 0) {
		SceSize copyLen = (size < m_bufferLength) ? size : m_bufferLength;
		sceClibMemcpy_safe(outBuf, m_buffer, copyLen);
		if (m_bufferLength > copyLen) {
			SceSize remaining = m_bufferLength - copyLen;
			sceClibMemmove(m_buffer, m_buffer + copyLen, remaining);
			m_bufferLength = remaining;
			sceClibMemset(m_buffer + remaining, 0, m_bufferCapacity - remaining);
		} else {
			sceClibMemset(m_buffer, 0, m_bufferLength);
			m_bufferLength = 0;
		}
		*outRead = copyLen;
		return 0;
	}

	SceSize got = 0;
	int err = m_source->Read(handle, outBuf, size, &got);
	if (err < 0)
		return err;
	*outRead = got;
	return 0;
}

int TrophySfmSignatureStripper::StripHeader(TrophyHandle *handle)
{
	if (m_source == NULL)
		return NP_TROPHY_ERR_GENERIC;

	for (;;) {
		if (m_state == STRIP_STATE_HEADER) {
			int err = ReadAndStripHeader(handle);
			if (err < 0) {
				m_state = STRIP_STATE_ERROR;
				return err;
			}
			m_state = STRIP_STATE_BODY;
		} else if (m_state == STRIP_STATE_BODY || m_state == STRIP_STATE_EOF) {
			return 0;
		} else {
			m_state = STRIP_STATE_ERROR;
			return NP_TROPHY_ERR_GENERIC;
		}
	}
}

int TrophySfmSignatureStripper::Read(TrophyHandle *handle, void *outBuf, SceSize size,
				     SceSize *outRead)
{
	if (m_source == NULL)
		return NP_TROPHY_ERR_GENERIC;

	for (;;) {
		if (m_state == STRIP_STATE_HEADER) {
			int err = ReadAndStripHeader(handle);
			if (err < 0) {
				m_state = STRIP_STATE_ERROR;
				return err;
			}
			m_state = STRIP_STATE_BODY;
		} else if (m_state == STRIP_STATE_BODY) {
			int err = ReadBodyChunk(handle, outBuf, size, outRead);
			if (err < 0) {
				m_state = STRIP_STATE_ERROR;
				return err;
			}
			if (*outRead == 0)
				m_state = STRIP_STATE_EOF;
			return 0;
		} else if (m_state == STRIP_STATE_EOF) {
			*outRead = 0;
			return 0;
		} else {
			m_state = STRIP_STATE_ERROR;
			return NP_TROPHY_ERR_GENERIC;
		}
	}
}

int TrophySfmSignatureStripper::GetSignature(TrophySfmSignature *outSig, bool *outHasSig) const
{
	if (m_source == NULL || (m_state != STRIP_STATE_BODY && m_state != STRIP_STATE_EOF))
		return NP_TROPHY_ERR_GENERIC;
	if (m_signature != NULL) {
		sceClibMemcpy_safe(outSig, m_signature, sizeof(*outSig));
		*outHasSig = true;
	} else {
		sceClibMemset(outSig, 0, sizeof(*outSig));
		*outHasSig = false;
	}
	return 0;
}

int TrophySfmSignatureStripper::GetStrippedHeaderSize(SceSize *outSize) const
{
	if (m_source == NULL || (m_state != STRIP_STATE_BODY && m_state != STRIP_STATE_EOF))
		return NP_TROPHY_ERR_GENERIC;
	*outSize = m_headerSize;
	return 0;
}

/* --- TrophySfmParser (0x812ECBC2..0x812EDAA3) --- */

static bool attrValueEquals(const BxceAttr *attr, const char *expected)
{
	if (attr == NULL || attr->value == NULL)
		return false;
	SceSize expectedLen = sceClibStrnlen(expected, 0x7FFFFFFFu);
	return sceClibStrncmp(attr->value, expected, expectedLen + 1) == 0;
}

TrophySfmParser::TrophySfmParser()
	: m_parserInitialized(0), m_savedSlot(-1), m_state(STATE_ROOT), m_lastError(0),
	  m_trophyOrderCounter(0), m_parseOptions(0), m_confData(NULL),
	  m_currentTrophy(NULL), m_currentGroup(NULL)
{
}

TrophySfmParser::~TrophySfmParser()
{
	if (m_confData != NULL) {
		delete m_confData;
		m_confData = NULL;
	}
	if (m_parserInitialized != 0) {
		m_parser.terminate();
		m_parserInitialized = 0;
	}
}

int TrophySfmParser::Init(uint32_t parseOptions)
{
	int err = m_parser.initialize(NULL);
	if (err >= 0) {
		m_parserInitialized = 1;
		m_parser.reset();
		m_parser.setResolveEntity(true);
		m_parser.setSkipIgnorableWhiteSpace(false);
		m_parser.setDocumentHandlerWithOffset(this);

		m_parseOptions = parseOptions;
		m_savedSlot = -1;
		m_state = STATE_ROOT;
		m_lastError = 0;
		m_trophyOrderCounter = 0;
		m_confData = new TrophyConfData();
		if (m_confData != NULL) {
			m_currentTrophy = NULL;
			m_currentGroup = NULL;
			return 0;
		}
		err = NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	if (m_confData != NULL) {
		delete m_confData;
		m_confData = NULL;
	}
	if (m_parserInitialized != 0) {
		m_parser.terminate();
		m_parserInitialized = 0;
	}
	return err;
}

int TrophySfmParser::ParseTrophyAttributes(const bXCe::AttributeList *attrList)
{
	const BxceAttr *idAttr = attrList->getAttribute("id", 2);
	if (idAttr == NULL || idAttr->value == NULL)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;

	unsigned int trophyId = 0;
	if (npTrophyParseUint(idAttr->value, &trophyId) < 0)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;

	int err = m_confData->AddTrophy(trophyId, m_trophyOrderCounter, &m_currentTrophy);
	if (err < 0)
		return err;
	++m_trophyOrderCounter;

	const BxceAttr *hiddenAttr = attrList->getAttribute("hidden", 6);
	if (hiddenAttr != NULL) {
		if (attrValueEquals(hiddenAttr, "yes")) {
			m_currentTrophy->SetHidden(true);
		} else if (!attrValueEquals(hiddenAttr, "no")) {
			return NP_TROPHY_ERR_BROKEN_CONF;
		}
	}

	const BxceAttr *ttypeAttr = attrList->getAttribute("ttype", 5);
	if (ttypeAttr == NULL || ttypeAttr->value == NULL)
		return NP_TROPHY_ERR_INVALID_GRADE;

	int grade = 0;
	if (attrValueEquals(ttypeAttr, "P"))
		grade = NP_TROPHY_GRADE_PLATINUM;
	else if (attrValueEquals(ttypeAttr, "G"))
		grade = NP_TROPHY_GRADE_GOLD;
	else if (attrValueEquals(ttypeAttr, "S"))
		grade = NP_TROPHY_GRADE_SILVER;
	else if (attrValueEquals(ttypeAttr, "B"))
		grade = NP_TROPHY_GRADE_BRONZE;
	else
		return NP_TROPHY_ERR_INVALID_GRADE;

	err = m_currentTrophy->SetGrade(grade);
	if (err < 0)
		return err;

	const BxceAttr *pidAttr = attrList->getAttribute("pid", 3);
	if (pidAttr != NULL && pidAttr->value != NULL && !attrValueEquals(pidAttr, "-1")) {
		unsigned int pidVal = 0;
		if (npTrophyParseUint(pidAttr->value, &pidVal) < 0)
			return -1;
		if (pidVal != 0xFFFFFFFFu) {
			err = m_currentTrophy->SetPlatinumId(static_cast<int>(pidVal));
			if (err < 0)
				return err;
		}
	}

	if (!m_confData->SupportsGroups())
		return 0;

	int groupId = -1;
	const BxceAttr *gidAttr = attrList->getAttribute("gid", 3);
	if (gidAttr != NULL && gidAttr->value != NULL && !attrValueEquals(gidAttr, "-1")) {
		unsigned int gidVal = 0;
		if (npTrophyParseUint(gidAttr->value, &gidVal) < 0)
			return NP_TROPHY_ERR_INVALID_GROUP_ID;
		groupId = static_cast<int>(gidVal);
	}
	if (groupId == -2)
		return 0;
	err = m_currentTrophy->SetGroupId(groupId);
	return (err < 0) ? err : 0;
}

int TrophySfmParser::startElement(void *userData, const char *name, int length,
				  const bXCe::AttributeList *attrList)
{
	(void)userData;
	m_textBuffer.Clear();
	int err = 0;

	switch (m_state) {
	case STATE_ROOT:
		if (length != 10 || sceClibStrncmp(name, "trophyconf", 10) != 0) {
			err = NP_TROPHY_ERR_BROKEN_CONF;
			break;
		}
		m_state = STATE_TROPHYCONF;
		{
			const BxceAttr *verAttr = attrList->getAttribute("version", 7);
			if (verAttr == NULL || verAttr->value == NULL) {
				err = NP_TROPHY_ERR_BROKEN_CONF;
				break;
			}
			uint32_t confVer = 0;
			err = npTrophyParseVersion(verAttr->value, &confVer);
			if (err < 0)
				break;
			err = m_confData->SetConfVersion(confVer);
			if (err < 0)
				break;

			uint32_t platformMask = 0;
			const BxceAttr *platAttr = attrList->getAttribute("platform", 8);
			if (platAttr != NULL && platAttr->value != NULL) {
				SceSize platLen = sceClibStrnlen(platAttr->value, 0x7FFFFFFFu);
				err = npTrophyParsePlatformMask(platAttr->value, platLen,
								&platformMask);
				if (err < 0)
					break;
			} else {
				platformMask = TROPHY_PLATFORM_PS3;
			}
			m_confData->SetPlatformMask(platformMask);
		}
		break;

	case STATE_TROPHYCONF:
		if (length == 8 && sceClibStrncmp(name, "npcommid", 8) == 0) {
			m_state = STATE_NPCOMMID;
		} else if (length == 17 && sceClibStrncmp(name, "trophyset-version", 17) == 0) {
			m_state = STATE_TROPHYSET_VERSION;
		} else if (length == 14 && sceClibStrncmp(name, "parental-level", 14) == 0) {
			m_state = STATE_PARENTAL_LEVEL;
		} else if (length == 10 && sceClibStrncmp(name, "title-name", 10) == 0) {
			m_state = STATE_TITLE_NAME;
			err = m_textBuffer.Append("", 0);
		} else if (length == 12 && sceClibStrncmp(name, "title-detail", 12) == 0) {
			m_state = STATE_TITLE_DETAIL;
			err = m_textBuffer.Append("", 0);
		} else if (length == 6 && sceClibStrncmp(name, "trophy", 6) == 0) {
			m_state = STATE_TROPHY;
			err = ParseTrophyAttributes(attrList);
		} else if (m_confData->GetConfVersion() >= 0x10001u && length == 5 &&
			   sceClibStrncmp(name, "group", 5) == 0) {
			m_state = STATE_GROUP;
			const BxceAttr *idAttr = attrList->getAttribute("id", 2);
			if (idAttr == NULL || idAttr->value == NULL) {
				err = NP_TROPHY_ERR_INVALID_GROUP_ID;
			} else {
				unsigned int groupId = 0;
				if (npTrophyParseUint(idAttr->value, &groupId) < 0)
					err = NP_TROPHY_ERR_INVALID_GROUP_ID;
				else
					err = m_confData->AddGroup(groupId, &m_currentGroup);
			}
		}
		break;

	case STATE_TROPHY:
		if (length == 4 && sceClibStrncmp(name, "name", 4) == 0) {
			m_state = STATE_TROPHY_NAME;
			err = m_textBuffer.Append("", 0);
		} else if (length == 6 && sceClibStrncmp(name, "detail", 6) == 0) {
			m_state = STATE_TROPHY_DETAIL;
			err = m_textBuffer.Append("", 0);
		}
		break;

	case STATE_GROUP:
		if (length == 4 && sceClibStrncmp(name, "name", 4) == 0) {
			m_state = STATE_GROUP_NAME;
			err = m_textBuffer.Append("", 0);
		} else if (length == 6 && sceClibStrncmp(name, "detail", 6) == 0) {
			m_state = STATE_GROUP_DETAIL;
			err = m_textBuffer.Append("", 0);
		}
		break;

	case STATE_NPCOMMID:
	case STATE_TROPHYSET_VERSION:
	case STATE_PARENTAL_LEVEL:
	case STATE_TITLE_NAME:
	case STATE_TITLE_DETAIL:
	case STATE_TROPHY_NAME:
	case STATE_TROPHY_DETAIL:
	case STATE_GROUP_NAME:
	case STATE_GROUP_DETAIL:
		err = NP_TROPHY_ERR_BROKEN_CONF;
		break;

	default:
		break;
	}

	if (err < 0) {
		m_lastError = err;
		m_state = STATE_ERROR;
		return err;
	}
	return 0;
}

int TrophySfmParser::characters(void *userData, const char *ch, int length)
{
	(void)userData;
	(void)ch;
	(void)length;
	return 0;
}

int TrophySfmParser::charactersWithOffset(void *userData, const char *ch, int length, int offset)
{
	(void)userData;
	int err = 0;
	bool offsetOnly = (m_parseOptions & 1u) != 0;

	switch (m_state) {
	case STATE_NPCOMMID:
	case STATE_TROPHYSET_VERSION:
		err = m_textBuffer.Append(ch, static_cast<SceSize>(length));
		break;

	case STATE_TITLE_NAME:
	case STATE_TROPHY_NAME:
	case STATE_GROUP_NAME:
		if (m_textBuffer.GetLength() == 0)
			m_textBuffer.SetStreamOffset(static_cast<uint64_t>(offset));
		err = m_textBuffer.AppendBounded(offsetOnly ? NULL : ch,
						 static_cast<SceSize>(length), 127);
		break;

	case STATE_TITLE_DETAIL:
	case STATE_TROPHY_DETAIL:
	case STATE_GROUP_DETAIL:
		if (m_textBuffer.GetLength() == 0)
			m_textBuffer.SetStreamOffset(static_cast<uint64_t>(offset));
		err = m_textBuffer.AppendBounded(offsetOnly ? NULL : ch,
						 static_cast<SceSize>(length), 1023);
		break;

	default:
		return 0;
	}

	if (err < 0) {
		m_lastError = err;
		m_state = STATE_ERROR;
		return err;
	}
	return 0;
}

int TrophySfmParser::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	int err = 0;

	switch (m_state) {
	case STATE_TROPHYCONF:
		if (length == 10 && sceClibStrncmp(name, "trophyconf", 10) == 0)
			m_state = STATE_ROOT;
		return 0;

	case STATE_NPCOMMID:
		if (length != 8 || sceClibStrncmp(name, "npcommid", 8) != 0)
			return 0;
		{
			TrophyCommId commId;
			err = npTrophyParseCommIdN(m_textBuffer.GetData(), m_textBuffer.GetLength(),
						   &commId);
			if (err >= 0)
				err = m_confData->SetCommId(commId);
			if (err >= 0)
				m_state = STATE_TROPHYCONF;
		}
		break;

	case STATE_TROPHYSET_VERSION:
		if (length != 17 || sceClibStrncmp(name, "trophyset-version", 17) != 0)
			return 0;
		{
			uint32_t setVersion = 0;
			err = npTrophyParseVersionN(m_textBuffer.GetData(),
						    m_textBuffer.GetLength(), &setVersion);
			if (err < 0)
				return err;
			err = m_confData->SetTrophySetVersion(setVersion);
			if (err >= 0)
				m_state = STATE_TROPHYCONF;
		}
		break;

	case STATE_PARENTAL_LEVEL:
		if (length == 14 && sceClibStrncmp(name, "parental-level", 14) == 0)
			m_state = STATE_TROPHYCONF;
		return 0;

	case STATE_TITLE_NAME:
		if (length != 10 || sceClibStrncmp(name, "title-name", 10) != 0)
			return 0;
		err = m_confData->GetTitleNameBuffer()->MoveFrom(&m_textBuffer);
		if (err >= 0)
			m_state = STATE_TROPHYCONF;
		break;

	case STATE_TITLE_DETAIL:
		if (length != 12 || sceClibStrncmp(name, "title-detail", 12) != 0)
			return 0;
		err = m_confData->GetTitleDetailBuffer()->MoveFrom(&m_textBuffer);
		if (err >= 0)
			m_state = STATE_TROPHYCONF;
		break;

	case STATE_TROPHY:
		if (length == 6 && sceClibStrncmp(name, "trophy", 6) == 0) {
			m_state = STATE_TROPHYCONF;
			m_currentTrophy = NULL;
		}
		return 0;

	case STATE_TROPHY_NAME:
		if (length != 4 || sceClibStrncmp(name, "name", 4) != 0)
			return 0;
		err = m_currentTrophy->GetNameBuffer()->MoveFrom(&m_textBuffer);
		if (err >= 0)
			m_state = STATE_TROPHY;
		break;

	case STATE_TROPHY_DETAIL:
		if (length != 6 || sceClibStrncmp(name, "detail", 6) != 0)
			return 0;
		err = m_currentTrophy->GetDetailBuffer()->MoveFrom(&m_textBuffer);
		if (err >= 0)
			m_state = STATE_TROPHY;
		break;

	case STATE_GROUP:
		if (length == 5 && sceClibStrncmp(name, "group", 5) == 0) {
			m_state = STATE_TROPHYCONF;
			m_currentGroup = NULL;
		}
		return 0;

	case STATE_GROUP_NAME:
		if (length != 4 || sceClibStrncmp(name, "name", 4) != 0)
			return 0;
		m_state = STATE_GROUP;
		err = m_currentGroup->GetNameBuffer()->MoveFrom(&m_textBuffer);
		break;

	case STATE_GROUP_DETAIL:
		if (length != 6 || sceClibStrncmp(name, "detail", 6) != 0)
			return 0;
		m_state = STATE_GROUP;
		err = m_currentGroup->GetDetailBuffer()->MoveFrom(&m_textBuffer);
		break;

	default:
		return 0;
	}

	if (err < 0) {
		m_lastError = err;
		m_state = STATE_ERROR;
		return err;
	}
	return 0;
}

int TrophySfmParser::ParseChunk(const char *chunk, int length, bool isFinal)
{
	if (m_savedSlot >= 0) {
		int err = m_parser.restore(m_savedSlot);
		if (err < 0)
			return err;
		m_savedSlot = -1;
	}

	int parseRes = m_parser.parse(chunk, length, isFinal);
	if (parseRes == BXCE_PARSE_INCOMPLETE) {
		int slot = m_parser.save();
		m_savedSlot = slot;
		return (slot < 0) ? slot : 0;
	}
	if (parseRes == 0) {
		int err = m_confData->Validate();
		if (err < 0)
			return err;
		m_state = STATE_DONE;
		return 0;
	}
	return (m_lastError < 0) ? m_lastError : NP_TROPHY_ERR_BROKEN_CONF;
}

int TrophySfmParser::ParseStream(TrophyHandle *handle, TrophyInputStream *input,
				 TrophyHashSink *hashSink)
{
	char *readBuf = static_cast<char *>(npTrophyHeapAlloc(0x10000));
	if (readBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int err = 0;
	for (;;) {
		err = handle->CheckCancel();
		if (err < 0)
			break;
		SceSize got = 0;
		err = npTrophyStreamReadHashed(input, handle, hashSink, readBuf, 0x10000, &got);
		if (err < 0)
			break;
		if (got == 0) {
			err = ParseChunk(readBuf, 0, true);
			if (err >= 0)
				err = 0;
			break;
		}
		err = ParseChunk(readBuf, static_cast<int>(got), false);
		if (err < 0)
			break;
	}

	npTrophyHeapFree(readBuf);
	return err;
}

int TrophySfmParser::TakeConfData(const TrophyCommId *expectedCommId, TrophyConfData **outConfData)
{
	if (m_state != STATE_DONE)
		return NP_TROPHY_ERR_GENERIC;
	if (!expectedCommId->Equals(m_confData->GetCommId()))
		return NP_TROPHY_ERR_INVALID_COMM_ID;
	*outConfData = m_confData;
	m_confData = NULL;
	return 0;
}
