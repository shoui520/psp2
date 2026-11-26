/*
 * nptrophy_sfm.h: TROPCONF.SFM / TROP_XX.SFM configuration data model,
 * XML signature header stripper, and bXCe SAX parser (0x812EBA18..0x812EDAA3).
 */
#ifndef SHELL_NP_TROPHY_SFM_H
#define SHELL_NP_TROPHY_SFM_H

#include <stdint.h>
#include <scetypes.h>
#include <bXCe/bxce_internal.h>
#include "nptrophy_runtime.h"
#include "nptrophy_io.h"

/* Parsed SFM signature from <!--Sce-Np-Trophy-Signature: ...--> */
struct TrophySfmSignature {
	uint8_t bytes[160];
	SceSize length;
};

/* Extracted title/trophy summary structs produced by TrophyConfData (0x812EC4BC, 0x812EC540) */
struct TrophyConfTitleDetails {
	TrophyCommId commId;			/* +0x00 */
	uint32_t platformMask;			/* +0x0C */
	TrophyGradeCounts gradeCounts;		/* +0x10 */
	uint32_t numGroups;			/* +0x20 */
	uint32_t reserved;			/* +0x24 */
	char titleName[128];			/* +0x28 */
	char titleDetail[1024];			/* +0xA8 */
};

struct TrophyConfTrophyDetails {
	int trophyId;				/* +0x000 */
	int grade;				/* +0x004 */
	int groupId;				/* +0x008 */
	char name[128];				/* +0x00C */
	char detail[1024];			/* +0x08C */
	int hidden;				/* +0x48C */
};

class TrophyConfData;

class TrophyConfGroup : public TrophyHeapObject {
public:
	TrophyConfGroup(TrophyConfData *owner, int groupId);
	virtual ~TrophyConfGroup();

	int GetGroupId() const { return m_groupId; }
	TrophyGrowBuffer *GetNameBuffer() { return &m_name; }
	TrophyGrowBuffer *GetDetailBuffer() { return &m_detail; }

private:
	TrophyConfData *m_owner;
	int m_groupId;
	TrophyGrowBuffer m_name;
	TrophyGrowBuffer m_detail;
};

class TrophyConfEntry : public TrophyHeapObject {
public:
	TrophyConfEntry(TrophyConfData *owner, int trophyId, int orderIndex);
	virtual ~TrophyConfEntry();

	int SetGrade(int grade);
	int SetGroupId(int groupId);
	int SetPlatinumId(int platinumId);
	void SetHidden(bool hidden) { m_hidden = hidden ? 1 : 0; }

	int GetTrophyId() const { return m_trophyId; }
	int GetGrade() const { return m_grade; }
	int GetOrderIndex() const { return m_orderIndex; }
	int GetGroupId() const { return m_groupId; }
	bool IsHidden() const { return m_hidden != 0; }
	int GetPlatinumId() const { return m_platinumId; }
	TrophyGrowBuffer *GetNameBuffer() { return &m_name; }
	TrophyGrowBuffer *GetDetailBuffer() { return &m_detail; }

private:
	TrophyConfData *m_owner;
	int m_trophyId;
	int m_grade;
	int m_orderIndex;
	TrophyGrowBuffer m_name;
	TrophyGrowBuffer m_detail;
	int m_groupId;
	int m_hidden;
	int m_platinumId;
};

class TrophyConfData : public TrophyHeapObject {
public:
	TrophyConfData();
	virtual ~TrophyConfData();

	bool SupportsGroups() const { return (m_featureFlags & 1u) != 0; }
	uint32_t GetConfVersion() const { return m_confVersion; }
	uint32_t GetTrophySetVersion() const { return m_trophySetVersion; }
	const TrophyCommId &GetCommId() const { return m_commId; }
	uint32_t GetPlatformMask() const { return m_platformMask; }

	int SetConfVersion(uint32_t version);
	int SetCommId(const TrophyCommId &commId);
	int SetTrophySetVersion(uint32_t version);
	void SetPlatformMask(uint32_t mask) { m_platformMask = mask; }

	int AddGroup(uint32_t groupId, TrophyConfGroup **outGroup);
	int GetGroup(uint32_t groupId, TrophyConfGroup **outGroup) const;
	int AddTrophy(uint32_t trophyId, int orderIndex, TrophyConfEntry **outEntry);
	int GetTrophy(uint32_t trophyId, TrophyConfEntry **outEntry) const;

	int Validate();
	int GetGroupBitset(uint32_t *outMask, uint32_t *outCount) const;
	int GetAllTrophyBitset(NpTrophyFlagArray *outBits, uint32_t *outCount) const;
	int GetPlatinumTargetBitset(uint32_t platinumTrophyId, NpTrophyFlagArray *outBits,
				    uint32_t *outCount) const;
	int GetHiddenTrophyBitset(NpTrophyFlagArray *outBits, uint32_t *outCount) const;
	int GetGradeTrophyBitset(int grade, NpTrophyFlagArray *outBits, uint32_t *outCount) const;
	int GetGroupTrophyBitset(int groupId, NpTrophyFlagArray *outBits, uint32_t *outCount) const;
	int CountByGrade(TrophyGradeCounts *outCounts) const;
	int CountGroupByGrade(int groupId, TrophyGradeCounts *outCounts) const;
	int BuildGradeBitsets(TrophyGradeBitsets *outBitsets) const;
	int ExtractTitleDetails(TrophyConfTitleDetails *outDetails) const;
	int ExtractTrophyDetails(uint32_t trophyId, TrophyConfTrophyDetails *outDetails) const;

	TrophyGrowBuffer *GetTitleNameBuffer() { return &m_titleName; }
	TrophyGrowBuffer *GetTitleDetailBuffer() { return &m_titleDetail; }

private:
	uint32_t m_confVersion;
	TrophyCommId m_commId;
	uint32_t m_trophySetVersion;
	TrophyGrowBuffer m_titleName;
	TrophyGrowBuffer m_titleDetail;
	uint32_t m_featureFlags;
	uint32_t m_platformMask;
	TrophyConfEntry *m_trophies[128];
	TrophyConfGroup *m_groups[32];
};

/* Strips leading XML comments and extracts <!--Sce-Np-Trophy-Signature: ...--> (0x812EC5CC..0x812ECBBB) */
class TrophySfmSignatureStripper : public TrophyInputStream {
public:
	TrophySfmSignatureStripper();
	virtual ~TrophySfmSignatureStripper();

	int Init(TrophyInputStream *source);
	int Term();
	int StripHeader(TrophyHandle *handle);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	int GetSignature(TrophySfmSignature *outSig, bool *outHasSig) const;
	int GetStrippedHeaderSize(SceSize *outSize) const;

private:
	int ParseSignatureHex(const char *hexText, SceSize availLen, SceSize *outConsumed);
	int ConsumeLeadingComment(const char *buf, SceSize availLen, SceSize *outConsumed,
				  int *outDone);
	int ReadAndStripHeader(TrophyHandle *handle);
	int ReadBodyChunk(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

	enum StripState {
		STRIP_STATE_HEADER	= 0,
		STRIP_STATE_BODY	= 1,
		STRIP_STATE_EOF		= 2,
		STRIP_STATE_ERROR	= 3
	};

	TrophyInputStream *m_source;
	SceSize m_headerSize;
	int m_state;
	char *m_buffer;
	SceSize m_bufferCapacity;
	SceSize m_bufferLength;
	TrophySfmSignature *m_signature;
};

/* SAX XML parser for TROPCONF.SFM / TROP_XX.SFM (0x812ECBC2..0x812EDAA3) */
class TrophySfmParser : public TrophyHeapObject, public bXCe::Sax::DocumentHandler {
public:
	TrophySfmParser();
	virtual ~TrophySfmParser();

	int Init(uint32_t parseOptions);
	int ParseChunk(const char *chunk, int length, bool isFinal);
	int ParseStream(TrophyHandle *handle, TrophyInputStream *input, TrophyHashSink *hashSink);
	int TakeConfData(const TrophyCommId *expectedCommId, TrophyConfData **outConfData);

	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int charactersWithOffset(void *userData, const char *ch, int length, int offset);
	virtual int endElement(void *userData, const char *name, int length);

private:
	int ParseTrophyAttributes(const bXCe::AttributeList *attrList);

	enum ParseState {
		STATE_ROOT		= 0,
		STATE_TROPHYCONF	= 1,
		STATE_NPCOMMID		= 2,
		STATE_TROPHYSET_VERSION	= 3,
		STATE_PARENTAL_LEVEL	= 4,
		STATE_TITLE_NAME	= 5,
		STATE_TITLE_DETAIL	= 6,
		STATE_TROPHY		= 7,
		STATE_TROPHY_NAME	= 8,
		STATE_TROPHY_DETAIL	= 9,
		STATE_GROUP		= 10,
		STATE_GROUP_NAME	= 11,
		STATE_GROUP_DETAIL	= 12,
		STATE_ERROR		= 13,
		STATE_DONE		= 14
	};

	bXCe::Sax::Parser m_parser;
	int m_parserInitialized;
	int m_savedSlot;
	int m_state;
	int m_lastError;
	int m_trophyOrderCounter;
	uint32_t m_parseOptions;
	TrophyConfData *m_confData;
	TrophyConfEntry *m_currentTrophy;
	TrophyConfGroup *m_currentGroup;
	TrophyGrowBuffer m_textBuffer;
};

#endif /* SHELL_NP_TROPHY_SFM_H */
