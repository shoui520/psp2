/*
 * nptrophy_net_client.cpp: trophy server status & XML RPC client, sync batching,
 * SFM RSA+HMAC signature verification, and CDN conf source extraction
 * (0x812CC5F0..0x812CC680, 0x812D8114..0x812D8763, 0x812E1114..0x812E6B7E).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libhttp.h>
#include <libsha1.h>
#include <libhmac.h>
#include <bXCe/bxce_internal.h>
#include "nptrophy_net.h"

extern "C" {
extern SceInt64 sceKernelGetSystemTimeWide(void);
}

namespace {

static const char *const kDefaultKeyVersion = "01";
static const uint64_t kStatusCacheTtlUsec = 60000000ULL;
static const uint64_t kDefaultSyncIntervalSec = 86400ULL;
static const uint32_t kSfmSigMagic = 0x4C39A48Cu;	/* 1278851468 */
static const uint32_t kSfmDevKeyId = 0xBEEFu;		/* 48879 */

/* Communication IDs excluded from trophy network sync (0x81545D84) */
static const char kExcludedSyncCommIds[2][12] = {
	"NPXS00032",
	"NPXS09292"
};

/* HTTP Basic auth credentials for trophy server, XOR-masked with 0x7F (0x8151BB70..0x8151BBA7) */
static const uint8_t kAuthRealmMasked[11] = {
	0x1C, 0x48, 0x06, 0x52, 0x0B, 0x0D, 0x10, 0x0F, 0x17, 0x06, 0x7F
};
static const uint8_t kAuthUserMasked[12] = {
	0x1C, 0x48, 0x06, 0x52, 0x0B, 0x0D, 0x10, 0x0F, 0x17, 0x06, 0x4F, 0x4D
};
static const uint8_t kAuthPassMasked[32] = {
	0x4A, 0x13, 0x4F, 0x2D, 0x15, 0x01, 0x16, 0x45,
	0x24, 0x18, 0x12, 0x13, 0x5A, 0x1A, 0x43, 0x2E,
	0x0B, 0x0B, 0x28, 0x08, 0x25, 0x49, 0x41, 0x33,
	0x0A, 0x4F, 0x4C, 0x13, 0x1B, 0x59, 0x1C, 0x5B
};

/* HMAC-SHA1 key for trophy-status.xml Sce-Np-Signature, XOR-masked with 0x7F (0x815460A4) */
static const uint8_t kStatusHmacKeyMasked[20] = {
	0x59, 0xCB, 0x60, 0x67, 0x57, 0x4D, 0x87, 0x3C, 0xEF, 0xD7,
	0xB7, 0x5A, 0x30, 0x05, 0x29, 0xC1, 0x8B, 0xFB, 0x75, 0xED
};

/* trophy-status.xml status attribute -> error code table (0x8154607C) */
struct TrophyStatusStringMap {
	const char *name;
	int errorCode;
};

static const TrophyStatusStringMap kStatusStringTable[] = {
	{ "alive",		0 },
	{ "maintenance",	NP_TROPHY_ERR_SERVICE_MAINTENANCE },
	{ "end",		NP_TROPHY_ERR_SERVICE_ENDED },
	{ "before",		NP_TROPHY_ERR_SERVICE_NOT_STARTED },
	{ NULL,			0 }
};

/* trophy-status.xml SceNpCommon XML schema (0x8151BCA4..0x8151BCFC) */
static const TrophyNpXmlElement kStatusXmlLevelRoot[] = {
	{ "nptrophy", 0 },
	{ "", 0 }
};
static const TrophyNpXmlElement kStatusXmlLevelChild[] = {
	{ "trophy-fqdn", 1 },
	{ "interval", 1 },
	{ "", 0 }
};
static const TrophyNpXmlElement *const kStatusXmlLevels[] = {
	kStatusXmlLevelRoot,
	kStatusXmlLevelChild
};
static const TrophyNpXmlSchema kStatusXmlSchema = {
	kStatusXmlLevels,
	2
};

/* SFM RSA-1024 + HMAC-SHA1 verification keys (0x81545D9C, 2 entries of 324 bytes) */
static const TrophySfmVerifyKeyEntry kSfmVerifyKeys[2] = {
	{
		0x00000100u,
		{
			0x49, 0xFC, 0x8B, 0x54, 0x1C, 0x41, 0xC8, 0x92,
			0x8C, 0x58, 0xB3, 0x34, 0xA8, 0x9B, 0x8B, 0xF5,
			0xCF, 0x68, 0x19, 0x2E, 0xC6, 0xAE, 0xF9, 0xB2,
			0xAF, 0xAE, 0x09, 0x38, 0x06, 0x08, 0xF3, 0xF4,
			0xBF, 0x78, 0xF9, 0x38, 0x22, 0x3B, 0x7C, 0x70,
			0x56, 0x81, 0x40, 0x35, 0x02, 0x73, 0x85, 0x50,
			0xB9, 0xAA, 0x10, 0x81, 0x97, 0x27, 0xD3, 0xA7,
			0x92, 0x2A, 0x5B, 0x08, 0x0E, 0x35, 0xE0, 0xFB,
			0x33, 0xAD, 0xF8, 0xA4, 0x0E, 0x0B, 0x11, 0x30,
			0x9B, 0x89, 0xDA, 0xD4, 0x7B, 0xF7, 0x64, 0x38,
			0x55, 0x5C, 0x20, 0x14, 0x55, 0xBB, 0x11, 0x4B,
			0x47, 0x35, 0x7D, 0x76, 0xD9, 0x2F, 0x95, 0xFF,
			0x39, 0x60, 0xEE, 0xC3, 0xC0, 0xCE, 0x0D, 0xB7,
			0x59, 0x86, 0xAB, 0x57, 0xE1, 0x85, 0x91, 0x76,
			0xD6, 0x2C, 0xE4, 0x4A, 0xC5, 0x02, 0x6F, 0x1F,
			0x04, 0x9E, 0xF1, 0x9B, 0xE5, 0xCE, 0x30, 0xCA
		},
		{
			0x01, 0x00, 0x01, 0x00
		},
		{
			0x6A, 0xC0, 0xB3, 0xFB, 0x1E, 0xBB, 0x4F, 0xB7,
			0xBC, 0xC2, 0xA0, 0x0D, 0xA3, 0xA2, 0xA4, 0xBD,
			0xBC, 0xB7, 0x88, 0xEC, 0x45, 0x67, 0x48, 0xCC,
			0x86, 0xCD, 0x9D, 0x09, 0xEF, 0x37, 0x93, 0x7F,
			0x6E, 0xE3, 0x11, 0x67, 0x77, 0x8E, 0x41, 0x4E,
			0x9B, 0x71, 0x1B, 0xE9, 0xD8, 0xCB, 0xB5, 0xF8,
			0x95, 0x6E, 0x45, 0x4E, 0xE9, 0x4A, 0x46, 0xC3,
			0x9D, 0xC4, 0xFF, 0x0D, 0x09, 0x0B, 0x92, 0xB7
		}
	},
	{
		0x0000BEEFu,
		{
			0xCF, 0xB6, 0x3B, 0x09, 0xC8, 0xCE, 0xD0, 0x65,
			0x61, 0x5E, 0x76, 0xC4, 0x0E, 0x75, 0x83, 0x2F,
			0x2D, 0x4E, 0x03, 0xBE, 0xE6, 0x2B, 0x8E, 0x25,
			0x6F, 0x16, 0x03, 0xAB, 0x52, 0x31, 0x5C, 0xDD,
			0xB8, 0x98, 0x19, 0x61, 0x94, 0x1A, 0x1B, 0xAA,
			0x01, 0x47, 0xC0, 0x87, 0xCF, 0xD9, 0x71, 0x72,
			0xBD, 0x95, 0xAB, 0x22, 0xB1, 0x60, 0x00, 0x89,
			0xE5, 0x90, 0x88, 0xC8, 0xB9, 0x13, 0x49, 0x09,
			0xCB, 0xAB, 0xCF, 0xBD, 0x7B, 0x9C, 0x0E, 0xF4,
			0xC0, 0x7B, 0xE5, 0x75, 0xF2, 0x89, 0x25, 0xA6,
			0x44, 0xFB, 0x8E, 0x43, 0xA8, 0x59, 0xB0, 0x84,
			0xD9, 0x56, 0xBA, 0xB0, 0x8B, 0xEA, 0xBF, 0xC2,
			0x7D, 0x69, 0x99, 0x41, 0x07, 0x35, 0x7C, 0x9A,
			0x2F, 0xAC, 0xFC, 0xB2, 0x4C, 0x71, 0x46, 0xC4,
			0x70, 0x82, 0x40, 0xBA, 0x8A, 0x57, 0x88, 0x1B,
			0x09, 0x01, 0xF1, 0xD8, 0x57, 0x41, 0xD2, 0xBD
		},
		{
			0x01, 0x00, 0x01, 0x00
		},
		{
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
		}
	}
};

/* Numeric and attribute parsing helpers (0x812E4F4C, 0x812E518E..0x812E5484, 0x812E5752) */
static int parseHexResultCode(const char *str)
{
	if (str == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	uint64_t acc = 0;
	while ((sceClibLookCtypeTable(*str) & 0x44) != 0) {
		int nibble = npTrophyHexDigitValue(*str);
		acc = (acc << 4) | static_cast<uint64_t>(nibble >= 0 ? nibble : 0);
		++str;
	}

	uint32_t code = static_cast<uint32_t>(acc);
	if (code == 0)
		return 0;
	if (code >= 0x100u)
		code = 0x0Bu;
	return static_cast<int>((code & 0xFFu) | 0x80551800u);
}

/* 0x812E518E */
static uint64_t parseUnsignedBase(const char *str, int base)
{
	if (str == NULL)
		return 0;

	uint64_t acc = 0;
	if (base == 16) {
		while ((sceClibLookCtypeTable(*str) & 0x44) != 0) {
			int ch = *str;
			acc *= 16u;
			if (ch >= '0' && ch <= '9')
				acc += static_cast<uint64_t>(ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				acc += static_cast<uint64_t>(ch - 'A' + 10);
			else if (ch >= 'a' && ch <= 'f')
				acc += static_cast<uint64_t>(ch - 'a' + 10);
			++str;
		}
	} else if (base == 10) {
		while ((sceClibLookCtypeTable(*str) & 4) != 0) {
			acc = acc * 10u + static_cast<uint64_t>(*str - '0');
			++str;
		}
	}
	return acc;
}

static int formatUnsignedBase(uint32_t value, char *outBuf, SceSize outSize, uint32_t base)
{
	uint32_t tmp = value;
	SceSize digits = 0;
	do {
		tmp /= base;
		++digits;
	} while (tmp != 0);

	if (outBuf != NULL) {
		if (digits + 1 > outSize)
			return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
		tmp = value;
		for (SceSize idx = digits; idx > 0; --idx) {
			uint32_t rem = tmp % base;
			tmp /= base;
			outBuf[idx - 1] = static_cast<char>((base == 16 && rem > 9)
							    ? (rem + 'a' - 10)
							    : (rem + '0'));
		}
		outBuf[digits] = '\0';
	}
	return static_cast<int>(digits);
}

static int parseGradeCountsAttrs(const BxceAttributeList *attrs, TrophyGradeCounts *outCounts)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "platinum", 8);
	if (attr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	outCounts->platinum = static_cast<uint32_t>(parseUnsignedBase(attr->value, 10));

	attr = ScebXCe_2AD8DA26(attrs, "gold", 4);
	if (attr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	outCounts->gold = static_cast<uint32_t>(parseUnsignedBase(attr->value, 10));

	attr = ScebXCe_2AD8DA26(attrs, "silver", 6);
	if (attr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	outCounts->silver = static_cast<uint32_t>(parseUnsignedBase(attr->value, 10));

	attr = ScebXCe_2AD8DA26(attrs, "bronze", 6);
	if (attr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	outCounts->bronze = static_cast<uint32_t>(parseUnsignedBase(attr->value, 10));
	return 0;
}

static int parseServerTrophyId(const char *str, int *outTrophyId)
{
	uint32_t trophyId = static_cast<uint32_t>(parseUnsignedBase(str, 10)) - 1u;
	*outTrophyId = static_cast<int>(trophyId);
	return (trophyId < 128u) ? 0 : NP_TROPHY_ERR_INVALID_TROPHY_ID;
}

static int parseServerTrophyGrade(const char *str, int *outGrade)
{
	if (str == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	uint32_t wireType = static_cast<uint32_t>(parseUnsignedBase(str, 10));
	switch (wireType) {
	case 0:
		*outGrade = 4; /* Bronze */
		return 0;
	case 1:
		*outGrade = 3; /* Silver */
		return 0;
	case 2:
		*outGrade = 2; /* Gold */
		return 0;
	case 3:
		*outGrade = 1; /* Platinum */
		return 0;
	default:
		return NP_TROPHY_ERR_INVALID_GRADE;
	}
}

static int parseSinglePlatformName(const char *str, int *outPlatform)
{
	if (str == NULL) {
		*outPlatform = -1;
	} else if (sceClibStrncmp(str, "ps4", 4) == 0) {
		*outPlatform = 3;
	} else if (sceClibStrncmp(str, "ps3", 4) == 0) {
		*outPlatform = 1;
	} else if (sceClibStrncmp(str, "psp2", 5) == 0) {
		*outPlatform = 2;
	} else {
		*outPlatform = -1;
	}
	return 0;
}

static int parsePlatformMaskStr(const char *str, uint32_t *outMask)
{
	*outMask = 0;
	if (str == NULL)
		return 0;
	SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
	return npTrophyParsePlatformMask(str, len, outMask);
}

static int parseBoolTrueStr(const char *str, int *outValue)
{
	if (str != NULL) {
		SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
		*outValue = (sceClibStrncmp(str, "true", len) == 0) ? 1 : 0;
	}
	return 0;
}

/* 0x812E5484 */
static int accumulateDecimalDigits(uint32_t *ioValue, const char *chars, SceSize length)
{
	(void)length;
	uint32_t val = *ioValue;
	const char *cursor = chars;
	while ((sceClibLookCtypeTable(*cursor) & 4) != 0) {
		val = val * 10u + static_cast<uint32_t>(*cursor - '0');
		++cursor;
	}
	*ioValue = val;
	return 0;
}

static int parseRarityAttrs(TrophySyncResultRarity *outRarity, const BxceAttributeList *attrs)
{
	BxceAttr *idAttr = ScebXCe_2AD8DA26(attrs, "id", 2);
	if (idAttr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	int ret = parseServerTrophyId(idAttr->value, &outRarity->trophyId);
	if (ret < 0)
		return ret;

	BxceAttr *rateAttr = ScebXCe_2AD8DA26(attrs, "rate", 4);
	if (rateAttr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	uint32_t ratePermille = 0;
	const char *cursor = rateAttr->value;
	while (cursor != NULL) {
		if ((sceClibLookCtypeTable(*cursor) & 4) != 0) {
			ratePermille = ratePermille * 10u + static_cast<uint32_t>(*cursor - '0');
			++cursor;
		} else if (*cursor == '.') {
			++cursor;
		} else {
			break;
		}
	}
	if (ratePermille > 1000u)
		ratePermille = 0;
	outRarity->earnRatePermille = ratePermille;

	BxceAttr *rareAttr = ScebXCe_2AD8DA26(attrs, "rare", 4);
	if (rareAttr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	uint64_t rareVal = parseUnsignedBase(rareAttr->value, 10);
	if (rareVal > 3)
		return NP_TROPHY_ERR_GENERIC;
	outRarity->rarityLevel = static_cast<int>(rareVal);
	return 0;
}

static const TrophySfmVerifyKeyEntry *findSfmVerifyKey(uint32_t keyId, bool requireOfficial)
{
	if (keyId == kSfmDevKeyId) {
		if (requireOfficial)
			return NULL;
	}
	for (SceSize i = 0; i < sizeof(kSfmVerifyKeys) / sizeof(kSfmVerifyKeys[0]); ++i) {
		if (kSfmVerifyKeys[i].keyId == keyId)
			return &kSfmVerifyKeys[i];
	}
	return NULL;
}

/* =========================================================================
 * Modified title privacy XML parser & RPC (0x812E363C..0x812E3A36, 0x812E6154..0x812E62A6)
 * ========================================================================= */

struct ModifiedPrivacyNode : public TrophyHeapObject {
	ModifiedPrivacyNode *next;
	ModifiedPrivacyNode *prev;
	TrophyModifiedPrivacyEntry entry;
	TrophyXmlDateAccumulator dateAcc;

	ModifiedPrivacyNode();
	int ParseInfoAttrs(const BxceAttributeList *attrs);
	int BeginLastUpdated();
	int AppendLastUpdated(const char *chars, SceSize length);
	int EndLastUpdated();
};

ModifiedPrivacyNode::ModifiedPrivacyNode() : next(NULL), prev(NULL), dateAcc(true)
{
	sceClibMemset(&entry, 0, sizeof(entry));
}

/* 0x812E6154 */
int ModifiedPrivacyNode::ParseInfoAttrs(const BxceAttributeList *attrs)
{
	BxceAttr *commAttr = ScebXCe_2AD8DA26(attrs, "npcommid", 8);
	if (commAttr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	int ret = SceNpCommon_5BC54899(&entry.commId, commAttr->value);
	if (ret < 0)
		return ret;
	BxceAttr *pfAttr = ScebXCe_2AD8DA26(attrs, "pf", 2);
	if (pfAttr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	ret = parseSinglePlatformName(pfAttr->value, &entry.platform);
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x812E61B6 */
int ModifiedPrivacyNode::BeginLastUpdated()
{
	dateAcc.Reset();
	return 0;
}

/* 0x812E61C2 */
int ModifiedPrivacyNode::AppendLastUpdated(const char *chars, SceSize length)
{
	int ret = dateAcc.Append(chars, length);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812E61D4 */
int ModifiedPrivacyNode::EndLastUpdated()
{
	dateAcc.MarkComplete();
	return 0;
}

struct ModifiedPrivacyList {
	ModifiedPrivacyNode *head;
	ModifiedPrivacyNode *tail;

	ModifiedPrivacyList();
	~ModifiedPrivacyList();
	int AppendNew(ModifiedPrivacyNode **outNode);
	int CopyOut(TrophyModifiedPrivacyEntry **outSlots, uint32_t maxSlots, uint32_t *outCount);
};

ModifiedPrivacyList::ModifiedPrivacyList() : head(NULL), tail(NULL) {}

/* 0x812E61E0 */
ModifiedPrivacyList::~ModifiedPrivacyList()
{
	while (head != NULL) {
		ModifiedPrivacyNode *node = head;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			tail = node->prev;
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			head = node->next;
		node->next = NULL;
		node->prev = NULL;
		delete node;
	}
}

/* 0x812E6220 */
int ModifiedPrivacyList::AppendNew(ModifiedPrivacyNode **outNode)
{
	ModifiedPrivacyNode *node = new ModifiedPrivacyNode();
	if (node == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	*outNode = node;
	node->next = NULL;
	node->prev = tail;
	if (tail != NULL)
		tail->next = node;
	if (head == NULL)
		head = node;
	tail = node;
	return 0;
}

/* 0x812E62A6 */
int ModifiedPrivacyList::CopyOut(TrophyModifiedPrivacyEntry **outSlots, uint32_t maxSlots,
				 uint32_t *outCount)
{
	uint32_t count = 0;
	for (ModifiedPrivacyNode *cur = head; cur != NULL; cur = cur->next) {
		if (count >= maxSlots)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		if (cur->entry.commId.IsEmpty())
			return NP_TROPHY_ERR_XML_PARSE_ERROR;
		int ret = cur->dateAcc.ParseTick(&cur->entry.lastUpdated);
		if (ret < 0)
			return ret;
		sceClibMemcpy_safe(outSlots[count], &cur->entry, sizeof(cur->entry));
		++count;
	}
	*outCount = count;
	return 0;
}

class ModifiedPrivacyXmlParser : public TrophyXmlResponseParserBase {
public:
	ModifiedPrivacyXmlParser();
	virtual ~ModifiedPrivacyXmlParser();
	virtual int OnStartElement(const char *name, int length, const BxceAttributeList *attrs);
	virtual int OnEndElement(const char *name, int length);
	virtual int OnCharacters(const char *chars, int length);

	ModifiedPrivacyList m_list;
	int m_state;
	uint32_t m_totalCount;
	ModifiedPrivacyNode *m_currentNode;
};

ModifiedPrivacyXmlParser::ModifiedPrivacyXmlParser()
	: m_state(-1), m_totalCount(0), m_currentNode(NULL)
{
}

ModifiedPrivacyXmlParser::~ModifiedPrivacyXmlParser()
{
}

int ModifiedPrivacyXmlParser::OnStartElement(const char *name, int length,
					     const BxceAttributeList *attrs)
{
	(void)length;
	switch (m_state) {
	case -1:
		if (sceClibStrncmp(name, "nptrophy", 9) == 0) {
			int ret = CheckResultAttribute(attrs);
			if (ret < 0)
				return ret;
			m_state = 0;
		}
		return 0;
	case 0:
		if (sceClibStrncmp(name, "title", 6) == 0)
			m_state = 1;
		else if (sceClibStrncmp(name, "list", 5) == 0)
			m_state = 2;
		return 0;
	case 1:
	case 4:
		return 0;
	case 2:
		if (sceClibStrncmp(name, "info", 5) == 0) {
			int ret = m_list.AppendNew(&m_currentNode);
			if (ret < 0)
				return ret;
			ret = m_currentNode->ParseInfoAttrs(attrs);
			if (ret < 0)
				return ret;
			m_state = 3;
		}
		return 0;
	case 3:
		if (sceClibStrncmp(name, "last-updated", 13) == 0) {
			int ret = m_currentNode->BeginLastUpdated();
			if (ret < 0)
				return ret;
			m_state = 4;
		}
		return 0;
	default:
		return NP_TROPHY_ERR_GENERIC;
	}
}

int ModifiedPrivacyXmlParser::OnEndElement(const char *name, int length)
{
	(void)length;
	switch (m_state) {
	case -1:
		return 0;
	case 0:
		if (sceClibStrncmp(name, "nptrophy", 9) == 0)
			m_state = -1;
		return 0;
	case 1:
		if (sceClibStrncmp(name, "title", 6) == 0)
			m_state = 0;
		return 0;
	case 2:
		if (sceClibStrncmp(name, "list", 5) == 0)
			m_state = 0;
		return 0;
	case 3:
		if (sceClibStrncmp(name, "info", 5) == 0)
			m_state = 2;
		return 0;
	case 4:
		if (sceClibStrncmp(name, "last-updated", 13) == 0) {
			m_currentNode->EndLastUpdated();
			m_state = 3;
		}
		return 0;
	default:
		return NP_TROPHY_ERR_GENERIC;
	}
}

int ModifiedPrivacyXmlParser::OnCharacters(const char *chars, int length)
{
	switch (m_state) {
	case -1:
	case 0:
	case 2:
		return 0;
	case 1:
		return accumulateDecimalDigits(&m_totalCount, chars,
					       static_cast<SceSize>(length));
	case 3:
		m_currentNode = NULL;
		return 0;
	case 4:
		return m_currentNode->AppendLastUpdated(chars, static_cast<SceSize>(length));
	default:
		return NP_TROPHY_ERR_GENERIC;
	}
}

class ModifiedPrivacyRpc : public TrophyXmlRpcBase {
public:
	ModifiedPrivacyRpc();
	virtual ~ModifiedPrivacyRpc();
	int Send(TrophyHandle *handle, TrophyNetClient *netClient, const SceNpId *npId,
		 const SceRtcTick *sinceUtc, const uint32_t *paging, const uint32_t *platformMask);
	int Receive(TrophyHandle *handle, uint32_t *outCounts,
		    TrophyModifiedPrivacyEntry **outSlots, uint32_t maxSlots);
};

/* 0x812E386E */
ModifiedPrivacyRpc::ModifiedPrivacyRpc()
{
}

ModifiedPrivacyRpc::~ModifiedPrivacyRpc()
{
}

/* 0x812E38C2 */
int ModifiedPrivacyRpc::Send(TrophyHandle *handle, TrophyNetClient *netClient, const SceNpId *npId,
			     const SceRtcTick *sinceUtc, const uint32_t *paging,
			     const uint32_t *platformMask)
{
	int ret = BeginRequestDoc(handle, npId);
	if (ret < 0)
		return ret;
	if (sinceUtc != NULL) {
		ret = m_docBuilder.AppendDateTimeElement(0, "since", sinceUtc);
		if (ret < 0)
			return ret;
	}
	ret = m_docBuilder.AppendPagingElements(0, paging);
	if (ret < 0)
		return ret;
	if (platformMask != NULL) {
		ret = m_docBuilder.AppendPlatformMaskElements(0, *platformMask);
		if (ret < 0)
			return ret;
	}
	return SendRequestDoc(handle, netClient, "/trophy/sec/get_modified_title_privacy", true);
}

/* 0x812E393A */
int ModifiedPrivacyRpc::Receive(TrophyHandle *handle, uint32_t *outCounts,
				TrophyModifiedPrivacyEntry **outSlots, uint32_t maxSlots)
{
	ModifiedPrivacyXmlParser parser;
	int ret = parser.InitParser();
	if (ret < 0)
		return ret;
	ret = ReceiveAndParseResponse(handle, &parser);
	if (ret < 0)
		return ret;
	ret = parser.m_list.CopyOut(outSlots, maxSlots, &outCounts[0]);
	if (ret < 0)
		return ret;
	outCounts[1] = parser.m_totalCount;
	return 0;
}

/* =========================================================================
 * Sync trophies XML parser & RPC (0x812E4168..0x812E4598, 0x812E6324..0x812E6B7E)
 * ========================================================================= */

struct SyncRespTrophyNode : public TrophyHeapObject {
	SyncRespTrophyNode *next;
	SyncRespTrophyNode *prev;
	TrophySyncResultUnlock unlock;
	TrophyXmlDateAccumulator dateAcc;

	SyncRespTrophyNode();
};

SyncRespTrophyNode::SyncRespTrophyNode() : next(NULL), prev(NULL), dateAcc(true)
{
	sceClibMemset(&unlock, 0, sizeof(unlock));
}

struct SyncRespRarityNode : public TrophyHeapObject {
	SyncRespRarityNode *next;
	SyncRespRarityNode *prev;
	TrophySyncResultRarity rarity;

	SyncRespRarityNode();
};

SyncRespRarityNode::SyncRespRarityNode() : next(NULL), prev(NULL)
{
	sceClibMemset(&rarity, 0, sizeof(rarity));
}

struct SyncRespTitleNode : public TrophyHeapObject {
	SyncRespTitleNode *next;
	SyncRespTitleNode *prev;
	TrophySyncResultTitleMeta meta;
	SyncRespTrophyNode *trophyHead;
	SyncRespTrophyNode *trophyTail;
	SyncRespRarityNode *rarityHead;
	SyncRespRarityNode *rarityTail;
	TrophyXmlDateAccumulator lastUpdatedAcc;

	SyncRespTitleNode();
	~SyncRespTitleNode();
	int AppendTrophy(SyncRespTrophyNode **outNode);
	int AppendRarity(SyncRespRarityNode **outNode);
	int CopyToPayload(TrophySyncResultPayload *dst);
};

SyncRespTitleNode::SyncRespTitleNode()
	: next(NULL), prev(NULL), trophyHead(NULL), trophyTail(NULL),
	  rarityHead(NULL), rarityTail(NULL), lastUpdatedAcc(true)
{
	sceClibMemset(&meta, 0, sizeof(meta));
}

/* 0x812E6636 */
SyncRespTitleNode::~SyncRespTitleNode()
{
	while (rarityHead != NULL) {
		SyncRespRarityNode *node = rarityHead;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			rarityTail = node->prev;
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			rarityHead = node->next;
		node->next = NULL;
		node->prev = NULL;
		delete node;
	}
	while (trophyHead != NULL) {
		SyncRespTrophyNode *node = trophyHead;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			trophyTail = node->prev;
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			trophyHead = node->next;
		node->next = NULL;
		node->prev = NULL;
		delete node;
	}
}

/* 0x812E6324 */
int SyncRespTitleNode::AppendTrophy(SyncRespTrophyNode **outNode)
{
	SyncRespTrophyNode *node = new SyncRespTrophyNode();
	if (node == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	*outNode = node;
	node->next = NULL;
	node->prev = trophyTail;
	if (trophyTail != NULL)
		trophyTail->next = node;
	if (trophyHead == NULL)
		trophyHead = node;
	trophyTail = node;
	return 0;
}

/* 0x812E64E6 */
int SyncRespTitleNode::AppendRarity(SyncRespRarityNode **outNode)
{
	SyncRespRarityNode *node = new SyncRespRarityNode();
	if (node == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	*outNode = node;
	node->next = NULL;
	node->prev = rarityTail;
	if (rarityTail != NULL)
		rarityTail->next = node;
	if (rarityHead == NULL)
		rarityHead = node;
	rarityTail = node;
	return 0;
}

/* 0x812E6564 */
int SyncRespTitleNode::CopyToPayload(TrophySyncResultPayload *dst)
{
	if (meta.commId.IsEmpty())
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	int ret = lastUpdatedAcc.ParseTick(&meta.lastUpdated);
	if (ret < 0)
		return ret;

	dst->m_meta = meta;

	uint32_t unlockIdx = 0;
	for (SyncRespTrophyNode *cur = trophyHead; cur != NULL; cur = cur->next) {
		if (unlockIdx >= dst->m_unlockCapacity)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		ret = cur->dateAcc.ParseTick(&cur->unlock.unlockUtc);
		if (ret < 0)
			return ret;
		dst->m_unlocks[unlockIdx] = cur->unlock;
		++unlockIdx;
	}
	dst->m_unlockCount = unlockIdx;

	uint32_t rarityIdx = 0;
	for (SyncRespRarityNode *cur = rarityHead; cur != NULL; cur = cur->next) {
		if (rarityIdx >= dst->m_rarityCapacity)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		dst->m_rarities[rarityIdx] = cur->rarity;
		++rarityIdx;
	}
	dst->m_rarityCount = rarityIdx;
	return 0;
}

struct SyncRespListParser {
	SyncRespTitleNode *head;
	SyncRespTitleNode *tail;
	int state;
	SyncRespTitleNode *curTitle;
	SyncRespTrophyNode *curTrophy;
	SyncRespRarityNode *curRarity;
	uint8_t expectHiddenAttr;

	SyncRespListParser();
	~SyncRespListParser();
	int AppendTitle(SyncRespTitleNode **outNode);
	int OnStart(const char *name, const BxceAttributeList *attrs, bool *outEntered);
	int OnEnd(const char *name, bool *outExited);
	int OnChars(const char *chars, SceSize length);
	int CopyOut(TrophySyncResultPayload **outPayloads, uint32_t maxCount, uint32_t *outCount);
};

/* 0x812E677C */
SyncRespListParser::SyncRespListParser()
	: head(NULL), tail(NULL), state(-1), curTitle(NULL), curTrophy(NULL),
	  curRarity(NULL), expectHiddenAttr(0)
{
}

/* 0x812E66AA */
SyncRespListParser::~SyncRespListParser()
{
	while (head != NULL) {
		SyncRespTitleNode *node = head;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			tail = node->prev;
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			head = node->next;
		node->next = NULL;
		node->prev = NULL;
		delete node;
	}
}

/* 0x812E66E6 */
int SyncRespListParser::AppendTitle(SyncRespTitleNode **outNode)
{
	SyncRespTitleNode *node = new SyncRespTitleNode();
	if (node == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	*outNode = node;
	node->next = NULL;
	node->prev = tail;
	if (tail != NULL)
		tail->next = node;
	if (head == NULL)
		head = node;
	tail = node;
	return 0;
}

/* 0x812E6792 */
int SyncRespListParser::OnStart(const char *name, const BxceAttributeList *attrs, bool *outEntered)
{
	if (outEntered != NULL)
		*outEntered = false;

	int ret = 0;
	switch (state) {
	case -1:
		if (sceClibStrncmp(name, "list", 5) == 0)
			state = 0;
		break;
	case 0:
		if (sceClibStrncmp(name, "info", 5) == 0) {
			ret = AppendTitle(&curTitle);
			if (ret < 0)
				return ret;
			BxceAttr *commAttr = ScebXCe_2AD8DA26(attrs, "npcommid", 8);
			if (commAttr == NULL)
				return NP_TROPHY_ERR_XML_PARSE_ERROR;
			ret = SceNpCommon_5BC54899(&curTitle->meta.commId, commAttr->value);
			if (ret < 0)
				return ret;
			BxceAttr *pfAttr = ScebXCe_2AD8DA26(attrs, "pf", 2);
			if (pfAttr == NULL)
				return NP_TROPHY_ERR_XML_PARSE_ERROR;
			ret = parsePlatformMaskStr(pfAttr->value, &curTitle->meta.platformMask);
			if (ret < 0)
				return ret;
			if (expectHiddenAttr != 0) {
				BxceAttr *hidAttr = ScebXCe_2AD8DA26(attrs, "hidden", 6);
				if (hidAttr == NULL)
					return NP_TROPHY_ERR_XML_PARSE_ERROR;
				ret = parseBoolTrueStr(hidAttr->value, &curTitle->meta.hidden);
				if (ret < 0)
					return ret;
			}
			state = 1;
		}
		break;
	case 1:
		if (sceClibStrncmp(name, "types", 6) == 0) {
			ret = parseGradeCountsAttrs(attrs, &curTitle->meta.gradeCounts);
			if (ret < 0)
				return ret;
		} else if (sceClibStrncmp(name, "trophies", 9) == 0) {
			state = 2;
		} else if (sceClibStrncmp(name, "last-updated", 13) == 0) {
			curTitle->lastUpdatedAcc.Reset();
			state = 4;
		} else if (sceClibStrncmp(name, "rarities", 9) == 0) {
			state = 5;
		}
		break;
	case 2:
		if (sceClibStrncmp(name, "trophy", 7) == 0) {
			ret = curTitle->AppendTrophy(&curTrophy);
			if (ret < 0)
				return ret;
			BxceAttr *idAttr = ScebXCe_2AD8DA26(attrs, "id", 2);
			if (idAttr == NULL)
				return NP_TROPHY_ERR_XML_PARSE_ERROR;
			ret = parseServerTrophyId(idAttr->value, &curTrophy->unlock.trophyId);
			if (ret < 0)
				return ret;
			BxceAttr *gradeAttr = ScebXCe_2AD8DA26(attrs, "type", 4);
			if (gradeAttr == NULL)
				return NP_TROPHY_ERR_XML_PARSE_ERROR;
			ret = parseServerTrophyGrade(gradeAttr->value, &curTrophy->unlock.grade);
			if (ret < 0)
				return ret;
			curTrophy->dateAcc.Reset();
			state = 3;
		}
		break;
	case 3:
	case 4:
	case 6:
		break;
	case 5:
		if (sceClibStrncmp(name, "rarity", 7) == 0) {
			ret = curTitle->AppendRarity(&curRarity);
			if (ret < 0)
				return ret;
			ret = parseRarityAttrs(&curRarity->rarity, attrs);
			if (ret < 0)
				return ret;
			state = 6;
		}
		break;
	default:
		return NP_TROPHY_ERR_GENERIC;
	}

	if (state != -1 && outEntered != NULL)
		*outEntered = true;
	return 0;
}

/* 0x812E6A18 */
int SyncRespListParser::OnEnd(const char *name, bool *outExited)
{
	if (outExited != NULL)
		*outExited = false;

	switch (state) {
	case -1:
		break;
	case 0:
		if (sceClibStrncmp(name, "list", 5) == 0)
			state = -1;
		break;
	case 1:
		if (sceClibStrncmp(name, "info", 5) == 0) {
			curTitle = NULL;
			state = 0;
		}
		break;
	case 2:
		if (sceClibStrncmp(name, "trophies", 9) == 0)
			state = 1;
		break;
	case 3:
		if (sceClibStrncmp(name, "trophy", 7) == 0) {
			curTrophy->dateAcc.MarkComplete();
			curTrophy = NULL;
			state = 2;
		}
		break;
	case 4:
		if (sceClibStrncmp(name, "last-updated", 13) == 0) {
			curTitle->lastUpdatedAcc.MarkComplete();
			state = 1;
		}
		break;
	case 5:
		if (sceClibStrncmp(name, "rarities", 9) == 0)
			state = 1;
		break;
	case 6:
		if (sceClibStrncmp(name, "rarity", 7) == 0) {
			curRarity = NULL;
			state = 5;
		}
		break;
	default:
		return NP_TROPHY_ERR_GENERIC;
	}

	if (state == -1 && outExited != NULL)
		*outExited = true;
	return 0;
}

/* 0x812E6B24 */
int SyncRespListParser::OnChars(const char *chars, SceSize length)
{
	switch (state) {
	case -1:
	case 0:
	case 1:
	case 2:
	case 5:
	case 6:
		return 0;
	case 3:
		return curTrophy->dateAcc.Append(chars, length);
	case 4:
		return curTitle->lastUpdatedAcc.Append(chars, length);
	default:
		return NP_TROPHY_ERR_GENERIC;
	}
}

/* 0x812E6B7E */
int SyncRespListParser::CopyOut(TrophySyncResultPayload **outPayloads, uint32_t maxCount,
				uint32_t *outCount)
{
	uint32_t count = 0;
	for (SyncRespTitleNode *cur = head; cur != NULL; cur = cur->next) {
		if (count >= maxCount)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		int ret = cur->CopyToPayload(outPayloads[count]);
		if (ret < 0)
			return ret;
		++count;
	}
	*outCount = count;
	return 0;
}

class SyncResponseXmlParser : public TrophyXmlResponseParserBase {
public:
	SyncResponseXmlParser();
	virtual ~SyncResponseXmlParser();
	virtual int OnStartElement(const char *name, int length, const BxceAttributeList *attrs);
	virtual int OnEndElement(const char *name, int length);
	virtual int OnCharacters(const char *chars, int length);

	SyncRespListParser m_listParser;
	int m_state;
};

SyncResponseXmlParser::SyncResponseXmlParser() : m_state(-1) {}

SyncResponseXmlParser::~SyncResponseXmlParser() {}

int SyncResponseXmlParser::OnStartElement(const char *name, int length,
					  const BxceAttributeList *attrs)
{
	(void)length;
	if (m_state == -1) {
		if (sceClibStrncmp(name, "nptrophy", 9) == 0) {
			int ret = CheckResultAttribute(attrs);
			if (ret < 0)
				return ret;
			m_state = 0;
		}
		return 0;
	}
	if (m_state == 0) {
		bool entered = false;
		int ret = m_listParser.OnStart(name, attrs, &entered);
		if (ret < 0)
			return ret;
		if (entered)
			m_state = 1;
		return 0;
	}
	if (m_state == 1)
		return m_listParser.OnStart(name, attrs, NULL);
	return NP_TROPHY_ERR_GENERIC;
}

int SyncResponseXmlParser::OnEndElement(const char *name, int length)
{
	(void)length;
	if (m_state == -1)
		return 0;
	if (m_state == 0) {
		if (sceClibStrncmp(name, "nptrophy", 9) == 0)
			m_state = -1;
		return 0;
	}
	if (m_state == 1) {
		bool exited = false;
		int ret = m_listParser.OnEnd(name, &exited);
		if (ret < 0)
			return ret;
		if (exited)
			m_state = 0;
		return 0;
	}
	return NP_TROPHY_ERR_GENERIC;
}

int SyncResponseXmlParser::OnCharacters(const char *chars, int length)
{
	if (m_state == -1 || m_state == 0)
		return 0;
	if (m_state == 1)
		return m_listParser.OnChars(chars, static_cast<SceSize>(length));
	return NP_TROPHY_ERR_GENERIC;
}

class SyncTrophiesRpc : public TrophyXmlRpcBase {
public:
	SyncTrophiesRpc();
	virtual ~SyncTrophiesRpc();
	int Send(TrophyHandle *handle, TrophyNetClient *netClient, const SceNpId *npId,
		 TrophySyncRequestPayload *const *requests, uint32_t requestCount);
	int Receive(TrophyHandle *handle, TrophySyncResultPayload **outResults,
		    uint32_t maxResults, uint32_t *outResultCount);
};

/* 0x812E4320 */
SyncTrophiesRpc::SyncTrophiesRpc()
{
}

SyncTrophiesRpc::~SyncTrophiesRpc()
{
}

/* 0x812E4374 */
int SyncTrophiesRpc::Send(TrophyHandle *handle, TrophyNetClient *netClient, const SceNpId *npId,
			  TrophySyncRequestPayload *const *requests, uint32_t requestCount)
{
	int ret = BeginRequestDoc(handle, npId);
	if (ret < 0)
		return ret;
	ret = m_docBuilder.AppendPsIdElement(0);
	if (ret < 0)
		return ret;

	BxceNodeId listNode = 0;
	ret = m_docBuilder.AppendElement(0, "list", NULL, NULL, 0, &listNode);
	if (ret < 0)
		return ret;

	for (uint32_t i = 0; i < requestCount; ++i) {
		const TrophySyncRequestPayload *req = requests[i];
		BxceNodeId infoNode = 0;
		ret = m_docBuilder.AppendTitleInfoElement(listNode, &req->GetCommId(), 2,
							  &infoNode);
		if (ret < 0)
			return ret;
		if (req->GetCommSign() != NULL) {
			ret = m_docBuilder.AppendSignElement(infoNode, req->GetCommSign());
			if (ret < 0)
				return ret;
		}
		uint32_t unlockCount = req->GetUnlockCount();
		if (unlockCount != 0) {
			BxceNodeId trophiesNode = 0;
			ret = m_docBuilder.AppendElement(infoNode, "trophies", NULL, NULL,
							 0, &trophiesNode);
			if (ret < 0)
				return ret;
			const TrophySyncRequestUnlock *unlocks = req->GetUnlocks();
			for (uint32_t j = 0; j < unlockCount; ++j) {
				ret = m_docBuilder.AppendTrophyUnlockElement(
					trophiesNode, unlocks[j].trophyId,
					unlocks[j].grade, &unlocks[j].unlockUtc);
				if (ret < 0)
					return ret;
			}
		}
	}

	return SendRequestDoc(handle, netClient, "/trophy/sec/sync_trophies", true);
}

/* 0x812E44A6 */
int SyncTrophiesRpc::Receive(TrophyHandle *handle, TrophySyncResultPayload **outResults,
			     uint32_t maxResults, uint32_t *outResultCount)
{
	SyncResponseXmlParser parser;
	int ret = parser.InitParser();
	if (ret < 0)
		return ret;
	ret = ReceiveAndParseResponse(handle, &parser);
	if (ret < 0)
		return ret;
	return parser.m_listParser.CopyOut(outResults, maxResults, outResultCount);
}

/* Stream wrapper returned by TrophyNetConfSource::OpenOwnedAssetStream (0x812E17B4..0x812E188C) */
class TrophyNetSourceAssetStream : public TrophyConfSourceStream {
public:
	int Open(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
		 const TrophyCommId *commId, const char *assetName)
	{
		return m_stream.OpenAssetDirect(handle, httpTemplate, commId, assetName, NULL,
						NULL, kDefaultKeyVersion);
	}

	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
	{
		return m_stream.Read(handle, outBuf, size, outRead);
	}

	virtual int CloseAndRelease()
	{
		int ret = m_stream.Close();
		delete this;
		return ret;
	}

private:
	TrophyNetAssetStream m_stream;
};

} /* namespace */

/* =========================================================================
 * TrophyStatusInfo, TrophyStatusXmlParser, TrophyStatusFetcher, TrophyNetClient
 * (0x812E329C..0x812E4016)
 * ========================================================================= */

/* 0x812E3A44 */
TrophyStatusInfo::TrophyStatusInfo() : fqdn(NULL), intervalSec(0) {}

/* 0x812E3A54 */
TrophyStatusInfo::~TrophyStatusInfo()
{
	if (fqdn != NULL)
		TrophyHeapObject::operator delete(fqdn);
}

/* 0x812E3A64 */
void TrophyStatusInfo::Clear()
{
	if (fqdn != NULL) {
		TrophyHeapObject::operator delete(fqdn);
		fqdn = NULL;
	}
	intervalSec = 0;
}

/* 0x812E3A80 */
int TrophyStatusInfo::Set(const char *hostFqdn, uint64_t interval)
{
	if (hostFqdn == NULL)
		return NP_TROPHY_ERR_STATUS_INVALID;
	SceSize len = sceClibStrnlen(hostFqdn, 0x7FFFFFFFu);
	if (len == 0)
		return NP_TROPHY_ERR_STATUS_INVALID;

	char *copy = static_cast<char *>(TrophyHeapObject::operator new(len + 1));
	if (copy == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	sceClibMemcpy_safe(copy, hostFqdn, len);
	copy[len] = '\0';

	if (fqdn != NULL)
		TrophyHeapObject::operator delete(fqdn);
	fqdn = copy;
	intervalSec = interval;
	return 0;
}

/* 0x812E3DE4 */
int TrophyStatusXmlParser::Init(TrophyStatusParseTarget *outTarget)
{
	sceClibMemset(this, 0, sizeof(*this));
	int ret = SceNpCommon_650D111B(&xmlParser, &kStatusXmlSchema, XmlCallback, this);
	if (ret < 0)
		return ret;
	target = outTarget;
	return 0;
}

/* 0x812E3E1A */
int TrophyStatusXmlParser::VerifyHeaderSignature(char *rawXml, SceSize xmlLen, SceSize bufCap)
{
	if (xmlLen < 65)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	SceSize bodyLen = xmlLen - 66;
	SceSize padLen = (64u - (bodyLen & 0x3Fu)) & 0x3Fu;
	if (bufCap < xmlLen + padLen)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	char headerLine[68];
	sceClibMemcpy_safe(headerLine, rawXml, 65);
	headerLine[65] = '\0';
	if (sceClibStrncmp(headerLine, "Sce-Np-Signature:", 17) != 0)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	const char *colon = sceClibStrchr(headerLine, ':');
	if (colon == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	do {
		++colon;
	} while ((sceClibLookCtypeTable(*colon) & 8) != 0);

	uint8_t expectedSig[20];
	for (SceSize i = 0; i < 20; ++i) {
		int hi = npTrophyHexDigitValue(colon[i * 2]);
		int lo = npTrophyHexDigitValue(colon[i * 2 + 1]);
		uint8_t highBits = (hi >= 0) ? static_cast<uint8_t>(hi << 4) : 0xF0u;
		uint8_t lowBits = (lo >= 0) ? static_cast<uint8_t>(lo) : 0xFFu;
		expectedSig[i] = static_cast<uint8_t>(highBits | lowBits);
	}

	for (SceSize i = 0; i < padLen; ++i)
		rawXml[xmlLen + i] = '\0';

	uint8_t hmacKey[20];
	for (SceSize i = 0; i < 20; ++i)
		hmacKey[i] = kStatusHmacKeyMasked[i] ^ 0x7Fu;

	uint8_t computedSig[20];
	sceHmacSha1Digest(hmacKey, sizeof(hmacKey), rawXml + 66,
			  static_cast<SceUInt32>(bodyLen + padLen), computedSig);

	if (sceClibMemcmp(expectedSig, computedSig, sizeof(expectedSig)) != 0)
		return NP_TROPHY_ERR_STATUS_SIG_INVALID;
	return 66;
}

/* 0x812E3FF2 */
int TrophyStatusXmlParser::ParseXml(const char *xmlBody, int bodyLen)
{
	int ret = SceNpCommon_88B18A5B(&xmlParser, xmlBody, bodyLen);
	if (ret < 0)
		return ret;
	return ((parsedMask & 8u) != 0) ? 0 : NP_TROPHY_ERR_XML_PARSE_ERROR;
}

/* 0x812E4016 */
int TrophyStatusXmlParser::XmlCallback(TrophyNpXmlParser *parser, int event, SceUInt32 depth,
				       SceUInt32 index, const char *name, const char *value,
				       void *userArg)
{
	(void)parser;
	TrophyStatusXmlParser *self = static_cast<TrophyStatusXmlParser *>(userArg);

	if (event == 4 && depth == 0 && index == 0 && sceClibStrncmp("status", name, 7) == 0) {
		self->statusError = 0;
		bool matched = false;
		for (SceSize i = 0; kStatusStringTable[i].name != NULL; ++i) {
			SceSize len = sceClibStrnlen(kStatusStringTable[i].name, 0x7FFFFFFFu);
			if (sceClibStrncmp(kStatusStringTable[i].name, value, len) == 0) {
				self->statusError = kStatusStringTable[i].errorCode;
				matched = true;
				break;
			}
		}
		if (matched)
			self->parsedMask |= 1u;
		else
			self->statusError = NP_TROPHY_ERR_XML_PARSE_ERROR;
	}

	if (event == 3 && depth == 1 && index == 0) {
		sceClibStrncpy(self->target->fqdnBuf, value, self->target->fqdnBufSize);
		self->target->fqdnBuf[self->target->fqdnBufSize - 1] = '\0';
		self->parsedMask |= 2u;
	}

	if (event == 3 && depth == 1 && index == 1) {
		unsigned int intervalVal = 0;
		int ret = npTrophyParseUint(value, &intervalVal);
		if (ret < 0)
			return ret;
		self->target->intervalSec = intervalVal;
		self->parsedMask |= 4u;
	}

	if (event == 2 && depth == 0 && index == 0) {
		if ((self->parsedMask & 1u) == 0 || (self->parsedMask & 2u) == 0)
			return NP_TROPHY_ERR_XML_PARSE_ERROR;
		if (self->statusError != 0)
			return self->statusError;
		self->parsedMask |= 8u;
	}
	return 0;
}

/* 0x812E3ADC */
TrophyStatusFetcher::TrophyStatusFetcher() : m_fqdnAlloc(NULL)
{
	sceClibMemset(&m_target, 0, sizeof(m_target));
}

/* 0x812E3B0C */
TrophyStatusFetcher::~TrophyStatusFetcher()
{
	if (m_fqdnAlloc != NULL)
		TrophyHeapObject::operator delete(m_fqdnAlloc);
}

/* 0x812E3B70 */
int TrophyStatusFetcher::ParseResponse(char *rawBody, SceSize bodyLen, SceSize bufCap)
{
	m_fqdnAlloc = static_cast<char *>(TrophyHeapObject::operator new(512));
	if (m_fqdnAlloc == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	m_target.fqdnBuf = m_fqdnAlloc;
	m_target.fqdnBufSize = 512;
	m_target.intervalSec = 0;

	int ret = m_parser.Init(&m_target);
	if (ret >= 0) {
		int xmlOffset = m_parser.VerifyHeaderSignature(rawBody, bodyLen, bufCap);
		ret = xmlOffset;
		if (xmlOffset >= 0) {
			ret = m_parser.ParseXml(rawBody + xmlOffset,
						static_cast<int>(bodyLen) - xmlOffset);
			if (ret >= 0)
				return 0;
		}
	}

	TrophyHeapObject::operator delete(m_fqdnAlloc);
	m_fqdnAlloc = NULL;
	return ret;
}

/* 0x812E3BEE */
int TrophyStatusFetcher::Fetch(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			       const TrophyHttpTimeouts *timeouts)
{
	char npEnv[20];
	char hostName[512];

	int ret = npTrophyGetNpEnvironment(npEnv);
	if (ret < 0)
		return ret;

	sceClibSnprintf(hostName, sizeof(hostName),
			"static-resource.%s.community.playstation.net", npEnv);

	ret = m_request.Open(httpTemplate, 0, "http", hostName, 80,
			     "/np/resource/trophy/trophy-status.xml", 0);
	if (ret < 0)
		return ret;

	if (timeouts != NULL) {
		ret = m_request.SetTimeouts(timeouts);
		if (ret < 0)
			return ret;
	}

	ret = m_request.Send(handle, NULL, 0);
	if (ret < 0)
		return ret;

	char *bodyBuf = NULL;
	int statusCode = 0;
	ret = m_request.GetStatusCode(handle, &statusCode);
	if (ret >= 0) {
		if (statusCode != 200) {
			ret = NP_TROPHY_ERR_XML_PARSE_ERROR;
		} else {
			uint64_t contentLength = 0;
			ret = m_request.GetResponseContentLength(handle, &contentLength);
			SceSize readCap = 0;
			SceSize allocSize = 0;
			if (ret == TROPHY_HTTP_ERR_NO_CONTENT_LENGTH) {
				readCap = 2048;
				allocSize = 2049;
				ret = 0;
			} else if (ret >= 0) {
				if (contentLength == 0) {
					ret = NP_TROPHY_ERR_XML_PARSE_ERROR;
				} else {
					readCap = static_cast<SceSize>(contentLength) + 64;
					allocSize = readCap + 1;
				}
			}

			if (ret >= 0) {
				bodyBuf = static_cast<char *>(
					TrophyHeapObject::operator new(allocSize));
				if (bodyBuf == NULL) {
					ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
				} else {
					SceSize totalRead = 0;
					while (true) {
						SceSize chunkRead = 0;
						ret = m_request.Read(handle, bodyBuf + totalRead,
								     readCap - totalRead,
								     &chunkRead);
						if (ret < 0)
							break;
						totalRead += chunkRead;
						if (chunkRead == 0) {
							bodyBuf[totalRead] = '\0';
							m_request.Close();
							ret = ParseResponse(bodyBuf, totalRead,
									    readCap);
							if (ret >= 0) {
								TrophyHeapObject::operator delete(
									bodyBuf);
								return 0;
							}
							break;
						}
					}
				}
			}
		}
	}

	if (m_fqdnAlloc != NULL) {
		TrophyHeapObject::operator delete(m_fqdnAlloc);
		m_fqdnAlloc = NULL;
	}
	if (m_request.IsOpen())
		m_request.Close();
	if (bodyBuf != NULL)
		TrophyHeapObject::operator delete(bodyBuf);
	return ret;
}

/* 0x812E3DB8 */
int TrophyStatusFetcher::CopyStatusTo(TrophyStatusInfo *outInfo) const
{
	if (m_target.fqdnBuf == NULL)
		return NP_TROPHY_ERR_STATUS_INVALID;
	return outInfo->Set(m_target.fqdnBuf, m_target.intervalSec);
}

/* 0x812E336C */
TrophyNetClient::TrophyNetClient() : m_statusFetchedTimeUsec(0) {}

/* 0x812E33A4 */
TrophyNetClient::~TrophyNetClient()
{
	Term();
}

/* 0x812E341E */
int TrophyNetClient::Init()
{
	int ret = m_mutex.Init("SceNpTrophyNetClient", false);
	if (ret >= 0) {
		ret = m_httpTemplate.Init(NULL, 2, 0);
		if (ret >= 0) {
			ret = m_httpTemplate.SetAuthCallback(HttpAuthCallback, this);
			if (ret >= 0)
				return 0;
		}
	}
	m_httpTemplate.Term();
	m_mutex.Term();
	return ret;
}

/* 0x812E3480 */
void TrophyNetClient::Term()
{
	m_httpTemplate.Term();
	m_statusInfo.Clear();
	m_mutex.Term();
}

/* 0x812E329C */
int TrophyNetClient::EnsureStatusFresh(TrophyHandle *handle, const TrophyHttpTimeouts *timeouts)
{
	TrophyStatusFetcher fetcher;
	uint64_t nowUsec = static_cast<uint64_t>(sceKernelGetSystemTimeWide());

	m_mutex.Lock();
	if (m_statusInfo.IsValid() && nowUsec < m_statusFetchedTimeUsec + kStatusCacheTtlUsec) {
		m_mutex.Unlock();
		return 0;
	}
	m_mutex.Unlock();

	int ret = fetcher.Fetch(handle, &m_httpTemplate, timeouts);
	if (ret < 0)
		return ret;

	m_mutex.Lock();
	ret = fetcher.CopyStatusTo(&m_statusInfo);
	if (ret >= 0)
		m_statusFetchedTimeUsec = nowUsec;
	m_mutex.Unlock();
	return ret;
}

/* 0x812E349E */
int TrophyNetClient::GetSyncInterval(TrophyHandle *handle, uint64_t *outIntervalSec,
				     const TrophyHttpTimeouts *timeouts)
{
	int ret = EnsureStatusFresh(handle, timeouts);
	if (ret < 0)
		return ret;

	m_mutex.Lock();
	*outIntervalSec = (m_statusInfo.intervalSec != 0) ? m_statusInfo.intervalSec
							  : kDefaultSyncIntervalSec;
	m_mutex.Unlock();
	return 0;
}

/* 0x812E34EC */
int TrophyNetClient::OpenServerRequest(TrophyHandle *handle, TrophyHttpRequest *request,
				       const char *path, bool useHttps,
				       const TrophyHttpTimeouts *timeouts)
{
	int ret = EnsureStatusFresh(handle, timeouts);
	if (ret >= 0) {
		m_mutex.Lock();
		const char *scheme = useHttps ? "https" : "http";
		uint16_t port = useHttps ? 443 : 80;
		ret = request->Open(&m_httpTemplate, 1, scheme, m_statusInfo.fqdn, port, path, 0);
		if (ret >= 0 && timeouts != NULL)
			ret = request->SetTimeouts(timeouts);
		m_mutex.Unlock();
		if (ret >= 0)
			return 0;
	}

	if (request->IsOpen())
		request->Close();
	return ret;
}

/* 0x812E359C */
SceInt32 TrophyNetClient::HttpAuthCallback(SceInt32 request, SceHttpAuthType authType,
					   const char *realm, char *username, char *password,
					   SceBool needEntity, SceUChar8 **entityBody,
					   SceSize *entitySize, SceBool *save, void *userArg)
{
	(void)request;
	(void)needEntity;
	(void)entityBody;
	(void)entitySize;
	(void)userArg;

	*save = SCE_FALSE;
	if (authType != SCE_HTTP_AUTH_BASIC)
		return -1;

	for (SceSize i = 0; i < sizeof(kAuthRealmMasked); ++i) {
		if (realm[i] != static_cast<char>(kAuthRealmMasked[i] ^ 0x7Fu))
			return -1;
	}

	for (SceSize i = 0; i < sizeof(kAuthUserMasked); ++i)
		username[i] = static_cast<char>(kAuthUserMasked[i] ^ 0x7Fu);
	username[sizeof(kAuthUserMasked)] = '\0';

	for (SceSize i = 0; i < sizeof(kAuthPassMasked); ++i)
		password[i] = static_cast<char>(kAuthPassMasked[i] ^ 0x7Fu);
	password[sizeof(kAuthPassMasked)] = '\0';
	return 0;
}

/* =========================================================================
 * TrophyXmlDateAccumulator & TrophyXmlResponseParserBase (0x812E4CD0..0x812E515C)
 * ========================================================================= */

/* 0x812E4CD0 */
TrophyXmlDateAccumulator::TrophyXmlDateAccumulator(bool requireEndElement)
	: m_text(NULL), m_requireComplete(requireEndElement ? 1 : 0), m_completed(0)
{
}

/* 0x812E4CE8 */
TrophyXmlDateAccumulator::~TrophyXmlDateAccumulator()
{
	if (m_text != NULL)
		TrophyHeapObject::operator delete(m_text);
}

/* 0x812E4D3C */
void TrophyXmlDateAccumulator::Reset()
{
	if (m_text != NULL) {
		TrophyHeapObject::operator delete(m_text);
		m_text = NULL;
	}
	m_completed = 0;
}

/* 0x812E4D52 */
int TrophyXmlDateAccumulator::AppendText(const char *chars, SceSize length)
{
	if (m_text == NULL) {
		char *fresh = static_cast<char *>(TrophyHeapObject::operator new(length + 1));
		if (fresh == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		sceClibMemcpy_safe(fresh, chars, length);
		fresh[length] = '\0';
		m_text = fresh;
		return 0;
	}

	SceSize curLen = sceClibStrnlen(m_text, 0x7FFFFFFFu);
	char *grown = static_cast<char *>(TrophyHeapObject::operator new(curLen + length + 1));
	if (grown == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	sceClibMemcpy_safe(grown, m_text, curLen);
	sceClibMemcpy_safe(grown + curLen, chars, length);
	grown[curLen + length] = '\0';
	TrophyHeapObject::operator delete(m_text);
	m_text = grown;
	return 0;
}

/* 0x812E4DDE */
int TrophyXmlDateAccumulator::Append(const char *chars, SceSize length)
{
	int ret = AppendText(chars, length);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812E4DF0 */
void TrophyXmlDateAccumulator::MarkComplete()
{
	m_completed = 1;
}

/* 0x812E4DF6 */
int TrophyXmlDateAccumulator::ParseTick(SceRtcTick *outUtc)
{
	if (m_requireComplete != 0 && m_completed == 0)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	if (m_text == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;

	int ret = sceRtcParseDateTime(outUtc, m_text);
	if (ret < 0)
		return ret;

	TrophyHeapObject::operator delete(m_text);
	m_text = NULL;
	return 0;
}

/* 0x812E4E34 */
TrophyXmlResponseParserBase::TrophyXmlResponseParserBase()
	: m_saxParser(NULL), m_savedSlot(-1), m_callbackError(0)
{
}

/* 0x812E4E4E */
TrophyXmlResponseParserBase::~TrophyXmlResponseParserBase()
{
	if (m_saxParser != NULL) {
		ScebXCe_928189AB(m_saxParser);
		TrophyHeapObject::operator delete(m_saxParser);
		m_saxParser = NULL;
	}
}

/* 0x812E50FC */
int TrophyXmlResponseParserBase::InitParser()
{
	m_saxParser = static_cast<BxceSaxParser *>(
		TrophyHeapObject::operator new(sizeof(BxceSaxParser)));
	if (m_saxParser == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	if (ScebXCe_C85586BD(m_saxParser) != 0) {
		TrophyHeapObject::operator delete(m_saxParser);
		m_saxParser = NULL;
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	ScebXCe_9E06C329(m_saxParser);
	ScebXCe_FF7CB737(&m_saxParser->parser, 1);
	ScebXCe_A40944DD(&m_saxParser->parser, 0);
	m_saxParser->handler = this;
	m_saxParser->characterOffsets = 0;
	m_savedSlot = -1;
	m_callbackError = 0;
	return 0;
}

/* 0x812E4EDA */
int TrophyXmlResponseParserBase::FeedChunk(const char *chunk, int length, bool isFinal)
{
	if (m_savedSlot >= 0)
		ScebXCe_A12B0F90(m_saxParser, static_cast<SceUInt8>(m_savedSlot));

	int parseRes = ScebXCe_4E9A7061(m_saxParser, chunk, length, isFinal ? 1 : 0);
	if (parseRes == 0)
		return 0;
	if (parseRes == -2138758912) { /* BXCE_NEED_MORE_DATA */
		m_savedSlot = ScebXCe_3CAF2B78(m_saxParser);
		return 0;
	}
	if (parseRes == -2138759168) /* BXCE_OUT_OF_MEMORY */
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	if (m_callbackError < 0)
		return m_callbackError;
	return NP_TROPHY_ERR_XML_PARSE_ERROR;
}

/* 0x812E515C */
int TrophyXmlResponseParserBase::CheckResultAttribute(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "result", 6);
	if (attr == NULL)
		return NP_TROPHY_ERR_XML_PARSE_ERROR;
	return parseHexResultCode(attr->value);
}

/* 0x812E4FDE */
int TrophyXmlResponseParserBase::startElement(void *userData, const char *name, int length,
					      const BxceAttributeList *attrList)
{
	(void)userData;
	int ret = OnStartElement(name, length, attrList);
	if (ret < 0)
		m_callbackError = ret;
	return ret;
}

/* 0x812E501A */
int TrophyXmlResponseParserBase::characters(void *userData, const char *ch, int length)
{
	(void)userData;
	int ret = OnCharacters(ch, length);
	if (ret < 0)
		m_callbackError = ret;
	return ret;
}

/* 0x812E4FFE */
int TrophyXmlResponseParserBase::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	int ret = OnEndElement(name, length);
	if (ret < 0)
		m_callbackError = ret;
	return ret;
}

/* 0x812E5036 */
int TrophyXmlResponseParserBase::entityData(void *userData, int entityType, const char *name,
					    int length)
{
	switch (entityType) {
	case 0:
		characters(userData, "&", 1);
		break;
	case 1:
		characters(userData, "\"", 1);
		break;
	case 2:
		characters(userData, "<", 1);
		break;
	case 3:
		characters(userData, ">", 1);
		break;
	case 4:
		characters(userData, "'", 1);
		break;
	case 6:
		characters(userData, name, length);
		break;
	default:
		characters(userData, "&", 1);
		characters(userData, name, length);
		characters(userData, ";", 1);
		break;
	}
	return 0;
}

/* =========================================================================
 * TrophyXmlAttrBuilder & TrophyXmlDocBuilder (0x812E5560..0x812E6106)
 * ========================================================================= */

/* 0x812E5560 */
TrophyXmlAttrBuilder::~TrophyXmlAttrBuilder()
{
	if (m_attrs != NULL) {
		ScebXCe_9C2F9897(m_attrs);
		TrophyHeapObject::operator delete(m_attrs);
		m_attrs = NULL;
	}
}

/* 0x812E557C */
int TrophyXmlAttrBuilder::Init()
{
	m_attrs = static_cast<BxceAttributeList *>(
		TrophyHeapObject::operator new(sizeof(BxceAttributeList)));
	if (m_attrs == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = ScebXCe_E7C8B20D(m_attrs);
	if (ret < 0) {
		TrophyHeapObject::operator delete(m_attrs);
		m_attrs = NULL;
		return NP_TROPHY_ERR_XML_BUILD_ERROR;
	}
	return (ret != 0) ? NP_TROPHY_ERR_XML_BUILD_ERROR : 0;
}

/* 0x812E55C6 */
int TrophyXmlAttrBuilder::AddAttr(const char *name, const char *value)
{
	int ret = ScebXCe_3DD761CF(m_attrs, name, value);
	if (ret < 0)
		return ret;
	return (ret != 0) ? NP_TROPHY_ERR_XML_BUILD_ERROR : 0;
}

/* 0x812E595C */
TrophyXmlDocBuilder::~TrophyXmlDocBuilder()
{
	Term();
}

/* 0x812E5976 */
int TrophyXmlDocBuilder::Init(const char *rootTag, const TrophyXmlAttrBuilder *rootAttrs,
			      BxceNodeId *outRootNode)
{
	m_doc = static_cast<BxceDocument *>(TrophyHeapObject::operator new(sizeof(BxceDocument)));
	if (m_doc == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	if (ScebXCe_FAB50608(m_doc, 1) != 0) {
		TrophyHeapObject::operator delete(m_doc);
		m_doc = NULL;
		return NP_TROPHY_ERR_XML_BUILD_ERROR;
	}

	BxceNodeId docRoot = ScebXCe_75E1B095(m_doc);
	BxceNodeId node = ScebXCe_AC0E4F67(m_doc, docRoot, rootTag,
					   (rootAttrs != NULL) ? rootAttrs->Get() : NULL,
					   NULL, 0);
	if (node == 0) {
		ScebXCe_B0891B13(m_doc);
		TrophyHeapObject::operator delete(m_doc);
		m_doc = NULL;
		return NP_TROPHY_ERR_XML_BUILD_ERROR;
	}
	if (outRootNode != NULL)
		*outRootNode = node;
	return 0;
}

/* 0x812E5A26 */
void TrophyXmlDocBuilder::Term()
{
	if (m_doc != NULL) {
		ScebXCe_B0891B13(m_doc);
		TrophyHeapObject::operator delete(m_doc);
		m_doc = NULL;
	}
}

/* 0x812E5A3E */
int TrophyXmlDocBuilder::Serialize(char **outXml, SceSize *outLen) const
{
	BxceSerializer *ser = static_cast<BxceSerializer *>(
		TrophyHeapObject::operator new(sizeof(BxceSerializer)));
	if (ser == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ScebXCe_DC8483D4(ser);
	int ret = ScebXCe_D7FE96A2(ser, m_doc, outXml, NULL, 0);
	if (ret == 0) {
		*outLen = sceClibStrnlen(*outXml, 0x7FFFFFFFu);
	} else {
		ret = NP_TROPHY_ERR_XML_BUILD_ERROR;
		if (*outXml != NULL) {
			ScebXCe_EC560647(*outXml);
			*outXml = NULL;
		}
	}

	ScebXCe_BE1AD985();
	TrophyHeapObject::operator delete(ser);
	return ret;
}

/* 0x812E5ABC */
void TrophyXmlDocBuilder::FreeSerialized(char *xml)
{
	if (xml != NULL)
		ScebXCe_EC560647(xml);
}

/* 0x812E55E4 */
int TrophyXmlDocBuilder::AppendElement(BxceNodeId parent, const char *tagName,
				       const TrophyXmlAttrBuilder *attrs, const char *text,
				       SceSize textLen, BxceNodeId *outNode)
{
	if (parent == 0)
		parent = ScebXCe_CED6BECF(m_doc);

	BxceNodeId node = ScebXCe_AC0E4F67(m_doc, parent, tagName,
					   (attrs != NULL) ? attrs->Get() : NULL,
					   text, textLen);
	if (node == 0)
		return NP_TROPHY_ERR_XML_BUILD_ERROR;
	if (outNode != NULL)
		*outNode = node;
	return 0;
}

/* 0x812E5802 */
int TrophyXmlDocBuilder::AppendUintElement(BxceNodeId parent, const char *tagName,
					   const TrophyXmlAttrBuilder *attrs, uint32_t value,
					   BxceNodeId *outNode)
{
	int digits = formatUnsignedBase(value, NULL, 0, 10);
	if (digits < 0)
		return digits;

	SceSize bufSize = static_cast<SceSize>(digits) + 1;
	char *numBuf = static_cast<char *>(TrophyHeapObject::operator new(bufSize));
	if (numBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = formatUnsignedBase(value, numBuf, bufSize, 10);
	if (ret >= 0) {
		ret = AppendElement(parent, tagName, attrs, numBuf,
				    static_cast<SceSize>(digits), outNode);
	}
	TrophyHeapObject::operator delete(numBuf);
	return ret;
}

/* 0x812E563E */
int TrophyXmlDocBuilder::AppendNpIdJidElement(BxceNodeId parent, const char *tagName,
					      const SceNpId *npId)
{
	SceNpId sanitized;
	sceClibMemcpy_safe(&sanitized, npId, sizeof(sanitized));
	sceClibMemset(sanitized.opt, 0, 4);

	TrophyNpJidParts jidParts;
	SceSize workSize = 0;
	int ret = SceNpCommon_40CC4D15(&jidParts, &sanitized, NULL, 0, &workSize);
	if (ret < 0)
		return ret;

	char *workBuf = static_cast<char *>(TrophyHeapObject::operator new(workSize));
	char *jidStr = NULL;
	if (workBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = SceNpCommon_40CC4D15(&jidParts, &sanitized, workBuf, workSize, NULL);
	if (ret >= 0) {
		SceSize jidLen = 0;
		ret = SceNpCommon_85699AE2(&jidParts, NULL, 0, &jidLen);
		if (ret >= 0) {
			jidStr = static_cast<char *>(TrophyHeapObject::operator new(jidLen));
			if (jidStr == NULL) {
				ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				ret = SceNpCommon_85699AE2(&jidParts, jidStr, jidLen, NULL);
				if (ret >= 0) {
					ret = AppendElement(parent, tagName, NULL, jidStr, jidLen,
							    NULL);
				}
			}
		}
	}

	TrophyHeapObject::operator delete(workBuf);
	if (jidStr != NULL)
		TrophyHeapObject::operator delete(jidStr);
	return ret;
}

/* 0x812E58BA */
int TrophyXmlDocBuilder::AppendTicketElement(BxceNodeId parent, const void *ticketBuf,
					     SceSize ticketSize)
{
	SceSize encodedCap = 4u * ((ticketSize + 2u) / 3u) + 1u;
	char *encodedBuf = static_cast<char *>(TrophyHeapObject::operator new(encodedCap));
	if (encodedBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	sceClibMemset(encodedBuf, 0, encodedCap);
	int encodedLen = static_cast<int>(
		sceNpCommonBase64Encode(static_cast<const SceUInt8 *>(ticketBuf),
					encodedBuf, ticketSize));
	int ret = encodedLen;
	if (encodedLen >= 0) {
		encodedBuf[encodedLen] = '\0';
		ret = AppendElement(parent, "ticket", NULL, encodedBuf,
				    static_cast<SceSize>(encodedLen), NULL);
	}
	TrophyHeapObject::operator delete(encodedBuf);
	return ret;
}

/* 0x812E5B58 */
int TrophyXmlDocBuilder::AppendTicketAndJid(BxceNodeId parent, const SceNpId *npId,
					    const void *ticketBuf, SceSize ticketSize)
{
	int ret = AppendTicketElement(parent, ticketBuf, ticketSize);
	if (ret < 0)
		return ret;
	return AppendNpIdJidElement(parent, "ticketjid", npId);
}

/* 0x812E5ACA */
int TrophyXmlDocBuilder::AppendDateTimeElement(BxceNodeId parent, const char *tagName,
					       const SceRtcTick *utc)
{
	char isoBuf[128];
	int ret = npTrophyFormatRfc3339(isoBuf, sizeof(isoBuf), utc, 0);
	if (ret < 0)
		return ret;
	SceSize isoLen = sceClibStrnlen(isoBuf, 0x7FFFFFFFu);
	return AppendElement(parent, tagName, NULL, isoBuf, isoLen, NULL);
}

/* 0x812E5B88 */
int TrophyXmlDocBuilder::AppendPsIdElement(BxceNodeId parent)
{
	char *hexBuf = static_cast<char *>(TrophyHeapObject::operator new(33));
	if (hexBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	SceKernelOpenPsId openPsId;
	int ret = npTrophyGetOpenPsId(&openPsId);
	if (ret >= 0)
		ret = npTrophyFormatOpenPsIdHex(&openPsId, hexBuf, 33);
	if (ret >= 0)
		ret = AppendElement(parent, "psid", NULL, hexBuf, 33, NULL);

	TrophyHeapObject::operator delete(hexBuf);
	return ret;
}

/* 0x812E5C28 */
int TrophyXmlDocBuilder::AppendPagingElements(BxceNodeId parent, const uint32_t *paging)
{
	uint32_t startOneBased = (paging != NULL) ? (paging[0] + 1u) : 1u;
	uint32_t maxCount = (paging != NULL) ? paging[1] : 0u;

	int ret = AppendUintElement(parent, "start", NULL, startOneBased, NULL);
	if (ret < 0)
		return ret;
	return AppendUintElement(parent, "max", NULL, maxCount, NULL);
}

/* 0x812E5C80 */
int TrophyXmlDocBuilder::AppendTitleInfoElement(BxceNodeId parent, const TrophyCommId *commId,
						int platform, BxceNodeId *outInfoNode)
{
	char commStr[16];
	sceClibMemset(commStr, 0, sizeof(commStr));
	int ret = SceNpCommon_E426FEE8(reinterpret_cast<const NpTrophyCommId *>(commId),
				       commStr, 13);
	if (ret < 0)
		return ret;

	TrophyXmlAttrBuilder attrs;
	ret = attrs.Init();
	if (ret < 0)
		return ret;
	ret = attrs.AddAttr("npcommid", commStr);
	if (ret < 0)
		return ret;

	if (platform != -1) {
		const char *pfStr = NULL;
		if (platform == 1)
			pfStr = "ps3";
		else if (platform == 2)
			pfStr = "psp2";
		else if (platform == 3)
			pfStr = "ps4";
		else
			return NP_TROPHY_ERR_INVALID_PLATFORM;

		ret = attrs.AddAttr("pf", pfStr);
		if (ret < 0)
			return ret;
	}

	return AppendElement(parent, "info", &attrs, NULL, 0, outInfoNode);
}

/* 0x812E5E38 */
int TrophyXmlDocBuilder::AppendSignElement(BxceNodeId parent, const void *signBuf)
{
	char *encodedBuf = static_cast<char *>(TrophyHeapObject::operator new(217));
	if (encodedBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	sceClibMemset(encodedBuf, 0, 217);
	int encodedLen = static_cast<int>(
		sceNpCommonBase64Encode(static_cast<const SceUInt8 *>(signBuf), encodedBuf, 160));
	int ret = encodedLen;
	if (encodedLen >= 0) {
		encodedBuf[encodedLen] = '\0';
		ret = AppendElement(parent, "sign", NULL, encodedBuf,
				    static_cast<SceSize>(encodedLen), NULL);
	}
	TrophyHeapObject::operator delete(encodedBuf);
	return ret;
}

/* 0x812E5EB8 */
int TrophyXmlDocBuilder::AppendTrophyUnlockElement(BxceNodeId parent, int trophyId, int grade,
						   const SceRtcTick *unlockUtc)
{
	char isoBuf[128];
	int ret = npTrophyFormatRfc3339(isoBuf, sizeof(isoBuf), unlockUtc, 0);
	if (ret < 0)
		return ret;

	char idBuf[16];
	ret = formatUnsignedBase(static_cast<uint32_t>(trophyId + 1), idBuf, sizeof(idBuf), 10);
	if (ret < 0)
		return ret;

	const char *gradeStr = NULL;
	switch (grade) {
	case 1:
		gradeStr = "3"; /* Platinum */
		break;
	case 2:
		gradeStr = "2"; /* Gold */
		break;
	case 3:
		gradeStr = "1"; /* Silver */
		break;
	case 4:
		gradeStr = "0"; /* Bronze */
		break;
	default:
		return NP_TROPHY_ERR_INVALID_GRADE;
	}

	TrophyXmlAttrBuilder attrs;
	ret = attrs.Init();
	if (ret < 0)
		return ret;
	ret = attrs.AddAttr("id", idBuf);
	if (ret < 0)
		return ret;
	ret = attrs.AddAttr("type", gradeStr);
	if (ret < 0)
		return ret;

	SceSize isoLen = sceClibStrnlen(isoBuf, 0x7FFFFFFFu);
	return AppendElement(parent, "trophy", &attrs, isoBuf, isoLen, NULL);
}

/* 0x812E607C */
int TrophyXmlDocBuilder::AppendPlatformElement(BxceNodeId parent, int platform)
{
	const char *pfStr = NULL;
	if (platform == 1)
		pfStr = "ps3";
	else if (platform == 2)
		pfStr = "psp2";
	else if (platform == 3)
		pfStr = "ps4";
	else
		return NP_TROPHY_ERR_INVALID_PLATFORM;

	SceSize pfLen = sceClibStrnlen(pfStr, 0x7FFFFFFFu);
	return AppendElement(parent, "pf", NULL, pfStr, pfLen, NULL);
}

/* 0x812E6106 */
int TrophyXmlDocBuilder::AppendPlatformMaskElements(BxceNodeId parent, uint32_t platformMask)
{
	int ret = 0;
	if ((platformMask & TROPHY_PLATFORM_PS4) != 0) {
		ret = AppendPlatformElement(parent, 3);
		if (ret < 0)
			return ret;
	}
	if ((platformMask & TROPHY_PLATFORM_PS3) != 0) {
		ret = AppendPlatformElement(parent, 1);
		if (ret < 0)
			return ret;
	}
	if ((platformMask & TROPHY_PLATFORM_PSP2) != 0) {
		ret = AppendPlatformElement(parent, 2);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* =========================================================================
 * TrophyXmlRpcBase (0x812E4978..0x812E4C08)
 * ========================================================================= */

/* 0x812E4968 */
int TrophyXmlRpcBase::SetTimeouts(const TrophyHttpTimeouts *timeouts)
{
	m_timeouts = *timeouts;
	return 0;
}

/* 0x812E4978 */
TrophyXmlRpcBase::TrophyXmlRpcBase()
{
	sceClibMemset(&m_timeouts, 0, sizeof(m_timeouts));
}

/* 0x812E49A8 */
TrophyXmlRpcBase::~TrophyXmlRpcBase()
{
	if (m_request.IsOpen())
		m_request.Close();
}

/* 0x812E4A34 */
int TrophyXmlRpcBase::InitBaseDoc()
{
	TrophyXmlAttrBuilder rootAttrs;
	int ret = rootAttrs.Init();
	if (ret >= 0)
		ret = rootAttrs.AddAttr("platform", "psp2");
	if (ret >= 0)
		ret = rootAttrs.AddAttr("sv", "1.00");
	if (ret >= 0)
		ret = m_docBuilder.Init("nptrophy", &rootAttrs, NULL);
	return ret;
}

/* 0x812E4AEE */
int TrophyXmlRpcBase::BeginRequestDoc(TrophyHandle *handle, const SceNpId *npId)
{
	void *ticketBuf = NULL;
	SceSize ticketSize = 0;
	int ret = TrophyTicketClient::FetchTicketAlloc(handle, &ticketBuf, &ticketSize);
	if (ret >= 0) {
		ret = InitBaseDoc();
		if (ret >= 0)
			ret = m_docBuilder.AppendTicketAndJid(0, npId, ticketBuf, ticketSize);
	}
	if (ticketBuf != NULL)
		TrophyTicketClient::FreeTicketBuf(ticketBuf);
	return ret;
}

/* 0x812E4B5A */
int TrophyXmlRpcBase::SendRequestDoc(TrophyHandle *handle, TrophyNetClient *netClient,
				     const char *path, bool useHttps)
{
	char *xmlBody = NULL;
	SceSize xmlLen = 0;
	int ret = m_docBuilder.Serialize(&xmlBody, &xmlLen);
	if (ret >= 0) {
		m_docBuilder.Term();
		ret = netClient->OpenServerRequest(handle, &m_request, path, useHttps,
						   &m_timeouts);
		if (ret >= 0) {
			SceSize bodyLen = sceClibStrnlen(xmlBody, 0x7FFFFFFFu);
			ret = m_request.SetContentLength(bodyLen);
			if (ret >= 0)
				ret = m_request.Send(handle, xmlBody, xmlLen);
		}
	}
	if (xmlBody != NULL)
		TrophyXmlDocBuilder::FreeSerialized(xmlBody);
	return ret;
}

/* 0x812E4C08 */
int TrophyXmlRpcBase::ReceiveAndParseResponse(TrophyHandle *handle,
					      TrophyXmlResponseParserBase *parser)
{
	int statusCode = 0;
	int ret = m_request.GetStatusCode(handle, &statusCode);
	if (ret < 0)
		return ret;
	if (statusCode != 200)
		return NP_TROPHY_ERR_RPC_HTTP_ERROR;

	char *chunkBuf = static_cast<char *>(TrophyHeapObject::operator new(0x2000));
	if (chunkBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	while (true) {
		SceSize chunkRead = 0;
		ret = m_request.Read(handle, chunkBuf, 0x1FFF, &chunkRead);
		if (ret < 0)
			break;
		chunkBuf[chunkRead] = '\0';
		if (chunkRead == 0) {
			ret = parser->FeedChunk(chunkBuf, 0, true);
			if (ret >= 0)
				m_request.Close();
			break;
		}
		ret = parser->FeedChunk(chunkBuf, static_cast<int>(chunkRead), false);
		if (ret < 0)
			break;
	}

	TrophyHeapObject::operator delete(chunkBuf);
	return ret;
}

/* =========================================================================
 * TrophyNetTitleListFetcher (0x812E1B7C..0x812E1D5F)
 * ========================================================================= */

/* 0x812E1B7C */
TrophyNetTitleListFetcher::TrophyNetTitleListFetcher()
	: m_startIndex(0), m_pageSize(0), m_fetchedCount(0), m_totalCount(0), m_entries(NULL)
{
	sceClibMemset(&m_npId, 0, sizeof(m_npId));
	m_sinceUtc.tick = 0;
}

/* 0x812E1BB0 */
TrophyNetTitleListFetcher::~TrophyNetTitleListFetcher()
{
	if (m_entries != NULL)
		TrophyHeapObject::operator delete(m_entries);
}

/* 0x812E1C04 */
int TrophyNetTitleListFetcher::Init(const SceNpId *npId, const SceRtcTick *sinceUtc)
{
	sceClibMemcpy_safe(&m_npId, npId, sizeof(m_npId));
	m_sinceUtc = *sinceUtc;
	m_entries = static_cast<TrophyModifiedPrivacyEntry *>(
		TrophyHeapObject::operator new(10 * sizeof(TrophyModifiedPrivacyEntry)));
	if (m_entries == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	sceClibMemset(m_entries, 0, 10 * sizeof(TrophyModifiedPrivacyEntry));
	m_startIndex = 0;
	m_pageSize = 10;
	m_fetchedCount = 0;
	m_totalCount = 0;
	return 0;
}

/* 0x812E1C54 */
int TrophyNetTitleListFetcher::FetchNextPage(TrophyHandle *handle, TrophyNetClient *netClient)
{
	ModifiedPrivacyRpc rpc;
	uint32_t platformMask = TROPHY_PLATFORM_PSP2;
	TrophyModifiedPrivacyEntry **slotPtrs = static_cast<TrophyModifiedPrivacyEntry **>(
		TrophyHeapObject::operator new(m_pageSize * sizeof(TrophyModifiedPrivacyEntry *)));
	if (slotPtrs == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	for (uint32_t i = 0; i < m_pageSize; ++i)
		slotPtrs[i] = &m_entries[i];

	uint32_t paging[2] = { m_startIndex, m_pageSize };
	int ret = rpc.Send(handle, netClient, &m_npId, &m_sinceUtc, paging, &platformMask);
	if (ret >= 0) {
		uint32_t counts[2] = { 0, 0 };
		ret = rpc.Receive(handle, counts, slotPtrs, m_pageSize);
		if (ret == NP_TROPHY_ERR_SERVER_NO_RESULTS) {
			m_fetchedCount = 0;
			m_totalCount = 0;
			ret = 0;
		} else if (ret >= 0) {
			m_fetchedCount = counts[0];
			m_totalCount = counts[1];
		}
		if (ret >= 0)
			m_startIndex += 10;
	}

	TrophyHeapObject::operator delete(slotPtrs);
	return ret;
}

/* 0x812E1D34 */
int TrophyNetTitleListFetcher::GetEntry(int index, TrophyModifiedPrivacyEntry *outEntry) const
{
	if (index < 0 || static_cast<uint32_t>(index) >= m_fetchedCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outEntry = m_entries[index];
	return 0;
}

/* =========================================================================
 * Sync Request / Result Payloads & Session Batching
 * (0x812D8114..0x812D8763, 0x812E1D60..0x812E2350, 0x812E45AA..0x812E4968)
 * ========================================================================= */

/* 0x812E4770 */
TrophySyncRequestPayload::TrophySyncRequestPayload()
	: m_commSign(NULL), m_unlockCount(0), m_unlocks(NULL), m_unlockCapacity(0)
{
}

/* 0x812E4794 */
TrophySyncRequestPayload::~TrophySyncRequestPayload()
{
	if (m_unlocks != NULL)
		TrophyHeapObject::operator delete(m_unlocks);
	if (m_commSign != NULL)
		TrophyHeapObject::operator delete(m_commSign);
}

/* 0x812E47F8 */
int TrophySyncRequestPayload::Init(const TrophyCommId *commId, int maxUnlocks)
{
	m_commId = *commId;
	if (maxUnlocks > 0) {
		m_unlocks = static_cast<TrophySyncRequestUnlock *>(
			TrophyHeapObject::operator new(
				static_cast<SceSize>(maxUnlocks) * sizeof(TrophySyncRequestUnlock)));
		if (m_unlocks == NULL) {
			m_unlockCapacity = 0;
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		m_unlockCount = 0;
	}
	m_unlockCapacity = static_cast<uint32_t>(maxUnlocks > 0 ? maxUnlocks : 0);
	return 0;
}

/* 0x812E4836 */
int TrophySyncRequestPayload::SetCommSign(const void *signBuf)
{
	uint8_t *copy = static_cast<uint8_t *>(TrophyHeapObject::operator new(160));
	if (copy == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	sceClibMemcpy_safe(copy, signBuf, 160);
	if (m_commSign != NULL)
		TrophyHeapObject::operator delete(m_commSign);
	m_commSign = copy;
	return 0;
}

/* 0x812E485E */
int TrophySyncRequestPayload::AddUnlock(int trophyId, int grade, const SceRtcTick *unlockUtc)
{
	if (m_unlockCount >= m_unlockCapacity)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	m_unlocks[m_unlockCount].trophyId = trophyId;
	m_unlocks[m_unlockCount].grade = grade;
	m_unlocks[m_unlockCount].unlockUtc = *unlockUtc;
	++m_unlockCount;
	return 0;
}

/* 0x812E4894 */
int TrophySyncRequestPayload::FilterByTrophyMask(const NpTrophyFlagArray *validMask)
{
	uint32_t idx = 0;
	while (idx < m_unlockCount) {
		int trophyId = m_unlocks[idx].trophyId;
		bool valid = (trophyId >= 0 && trophyId < 128) &&
			     ((validMask->flagBits[trophyId >> 5] &
			       (1u << (trophyId & 31))) != 0);
		if (valid) {
			++idx;
		} else {
			if (idx + 1 < m_unlockCount) {
				sceClibMemmove(&m_unlocks[idx], &m_unlocks[idx + 1],
					       (m_unlockCount - idx - 1) *
					       sizeof(TrophySyncRequestUnlock));
			}
			--m_unlockCount;
		}
	}
	return 0;
}

/* 0x812E4944 */
void TrophySyncRequestPayload::Reset()
{
	if (m_commSign != NULL) {
		TrophyHeapObject::operator delete(m_commSign);
		m_commSign = NULL;
	}
	m_unlockCount = 0;
	if (m_unlocks != NULL && m_unlockCapacity != 0)
		sceClibMemset(m_unlocks, 0, m_unlockCapacity * sizeof(TrophySyncRequestUnlock));
}

/* 0x812E45AA */
TrophySyncResultPayload::TrophySyncResultPayload()
	: m_unlockCount(0), m_unlocks(NULL), m_rarityCount(0), m_rarities(NULL),
	  m_unlockCapacity(0), m_rarityCapacity(0)
{
	sceClibMemset(&m_meta, 0, sizeof(m_meta));
}

/* 0x812E45D2 */
TrophySyncResultPayload::~TrophySyncResultPayload()
{
	if (m_unlocks != NULL)
		TrophyHeapObject::operator delete(m_unlocks);
	if (m_rarities != NULL)
		TrophyHeapObject::operator delete(m_rarities);
}

/* 0x812E4636 */
int TrophySyncResultPayload::Init(uint32_t maxTrophies)
{
	sceClibMemset(&m_meta, 0, sizeof(m_meta));
	if (maxTrophies != 0) {
		m_unlocks = static_cast<TrophySyncResultUnlock *>(
			TrophyHeapObject::operator new(
				maxTrophies * sizeof(TrophySyncResultUnlock)));
		if (m_unlocks == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		sceClibMemset(m_unlocks, 0, maxTrophies * sizeof(TrophySyncResultUnlock));
		for (uint32_t i = 0; i < maxTrophies; ++i)
			m_unlocks[i].trophyId = -1;

		m_rarities = static_cast<TrophySyncResultRarity *>(
			TrophyHeapObject::operator new(
				maxTrophies * sizeof(TrophySyncResultRarity)));
		if (m_rarities == NULL) {
			TrophyHeapObject::operator delete(m_unlocks);
			m_unlocks = NULL;
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		sceClibMemset(m_rarities, 0, maxTrophies * sizeof(TrophySyncResultRarity));
		for (uint32_t i = 0; i < maxTrophies; ++i)
			m_rarities[i].trophyId = -1;
	}
	m_unlockCount = maxTrophies;
	m_unlockCapacity = maxTrophies;
	m_rarityCount = maxTrophies;
	m_rarityCapacity = maxTrophies;
	return 0;
}

/* 0x812E46DA */
int TrophySyncResultPayload::FindTrophyUnlock(int trophyId, uint64_t *outServerTick,
					      int *outUnlocked) const
{
	for (uint32_t i = 0; i < m_unlockCount; ++i) {
		if (m_unlocks[i].trophyId == trophyId) {
			*outServerTick = m_unlocks[i].unlockUtc.tick;
			*outUnlocked = 1;
			return 0;
		}
	}
	*outServerTick = 0;
	*outUnlocked = 0;
	return 0;
}

/* 0x812E4730 */
bool TrophySyncResultPayload::HasTrophyUnlock(int trophyId) const
{
	for (uint32_t i = 0; i < m_unlockCount; ++i) {
		if (m_unlocks[i].trophyId == trophyId)
			return true;
	}
	return false;
}

/* 0x812E4764 */
int TrophySyncResultPayload::GetLastUpdated(uint64_t *outTick) const
{
	*outTick = m_meta.lastUpdated.tick;
	return 0;
}

/* 0x812E1D60 */
TrophySyncTitleRequestBase::TrophySyncTitleRequestBase()
	: m_next(NULL), m_prev(NULL), m_payload(NULL)
{
}

/* 0x812E1D74 */
TrophySyncTitleRequestBase::~TrophySyncTitleRequestBase()
{
	if (m_payload != NULL)
		delete m_payload;
}

/* 0x812E1DD6 */
int TrophySyncTitleRequestBase::InitPayload(const TrophyCommId *commId, int maxUnlocks)
{
	TrophySyncRequestPayload *payload = new TrophySyncRequestPayload();
	if (payload == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	int ret = payload->Init(commId, maxUnlocks);
	if (ret < 0) {
		delete payload;
		return ret;
	}
	m_payload = payload;
	return 0;
}

/* 0x812E1E32 */
void TrophySyncTitleRequestBase::ResetPayload()
{
	if (m_payload != NULL)
		m_payload->Reset();
}

/* 0x812E1E3C */
int TrophySyncTitleRequestBase::AddUnlockPayload(int trophyId, int grade,
						 const SceRtcTick *unlockUtc)
{
	return m_payload->AddUnlock(trophyId, grade, unlockUtc);
}

/* 0x812E1E4E */
int TrophySyncTitleRequestBase::FilterByConfData(const TrophyConfData *confData)
{
	NpTrophyFlagArray mask;
	int ret = confData->GetAllTrophyBitset(&mask, NULL);
	if (ret < 0)
		return ret;
	return m_payload->FilterByTrophyMask(&mask);
}

/* 0x812E1E90 */
const TrophyCommId *TrophySyncTitleRequestBase::GetCommId() const
{
	return &m_payload->GetCommId();
}

/* 0x812E1E96 */
int TrophySyncTitleRequestBase::SetCommSign(const void *signBuf)
{
	return m_payload->SetCommSign(signBuf);
}

/* 0x812D8520 */
TrophySyncTitleRequest::TrophySyncTitleRequest()
	: m_transEntries(NULL), m_transCapacity(0), m_transCount(0)
{
}

/* 0x812D8540 */
TrophySyncTitleRequest::~TrophySyncTitleRequest()
{
	if (m_transEntries != NULL)
		TrophyHeapObject::operator delete(m_transEntries);
}

/* 0x812D858A */
int TrophySyncTitleRequest::Init(const TrophyCommId *commId, uint32_t maxTransEntries)
{
	int ret = InitPayload(commId, static_cast<int>(maxTransEntries));
	if (ret >= 0) {
		uint32_t capacity = maxTransEntries + 1;
		SceSize allocBytes = capacity * sizeof(TrophyTransBatchEntry);
		m_transEntries = static_cast<TrophyTransBatchEntry *>(
			TrophyHeapObject::operator new(allocBytes));
		if (m_transEntries != NULL) {
			sceClibMemset(m_transEntries, 0, allocBytes);
			m_transCapacity = capacity;
			m_transCount = 0;
			return 0;
		}
		ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	if (m_transEntries != NULL) {
		TrophyHeapObject::operator delete(m_transEntries);
		m_transEntries = NULL;
	}
	m_transCapacity = 0;
	if (m_payload != NULL) {
		delete m_payload;
		m_payload = NULL;
	}
	return ret;
}

/* 0x812D85E4 */
int TrophySyncTitleRequest::GetTransEntry(uint32_t index, TrophyTransBatchEntry *outEntry) const
{
	if (index >= m_transCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outEntry = m_transEntries[index];
	return 0;
}

/* 0x812D8614 */
int TrophySyncTitleRequest::AddRegisterTrans(const TrophyTransRegisterEntry *entry)
{
	if (m_transCount >= m_transCapacity)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	m_transEntries[m_transCount].slotIndex = static_cast<int>(entry->slotIndex);
	m_transEntries[m_transCount].transType = static_cast<int>(entry->transType);
	m_transEntries[m_transCount].trophyId = 0;
	++m_transCount;
	return 0;
}

/* 0x812D8644 */
int TrophySyncTitleRequest::AddUnlockTrans(const TrophyTransUnlockEntry *entry)
{
	if (m_transCount >= m_transCapacity)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	int ret = AddUnlockPayload(entry->trophyId, entry->grade, &entry->secureTick);
	if (ret < 0)
		return ret;
	m_transEntries[m_transCount].slotIndex = static_cast<int>(entry->slotIndex);
	m_transEntries[m_transCount].transType = static_cast<int>(entry->transType);
	m_transEntries[m_transCount].trophyId = entry->trophyId;
	++m_transCount;
	return 0;
}

/* 0x812D8694 */
void TrophySyncTitleRequest::Reset()
{
	if (m_transEntries != NULL && m_transCapacity != 0)
		sceClibMemset(m_transEntries, 0, m_transCapacity * sizeof(TrophyTransBatchEntry));
	m_transCount = 0;
	ResetPayload();
}

/* 0x812E1EA8 */
TrophySyncTitleResultBase::TrophySyncTitleResultBase()
	: m_next(NULL), m_prev(NULL), m_pairedRequest(NULL), m_payload(NULL)
{
}

/* 0x812E1EBC */
TrophySyncTitleResultBase::~TrophySyncTitleResultBase()
{
	if (m_payload != NULL)
		delete m_payload;
}

/* 0x812E1F1E */
int TrophySyncTitleResultBase::InitPayload(uint32_t maxTrophies)
{
	TrophySyncResultPayload *payload = new TrophySyncResultPayload();
	if (payload == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	int ret = payload->Init(maxTrophies);
	if (ret < 0) {
		delete payload;
		return ret;
	}
	m_payload = payload;
	return 0;
}

/* 0x812E1F70 */
void TrophySyncTitleResultBase::DestroyPayload()
{
	if (m_payload != NULL) {
		delete m_payload;
		m_payload = NULL;
	}
}

/* 0x812E1F88 */
bool TrophySyncTitleResultBase::HasTrophyUnlock(int trophyId) const
{
	return m_payload->HasTrophyUnlock(trophyId);
}

/* 0x812E1F92 */
int TrophySyncTitleResultBase::FindTrophyUnlock(int trophyId, uint64_t *outServerTick,
						int *outUnlocked) const
{
	return m_payload->FindTrophyUnlock(trophyId, outServerTick, outUnlocked);
}

/* 0x812E1F9C */
void TrophySyncTitleResultBase::GetLastUpdated(uint64_t *outTick) const
{
	m_payload->GetLastUpdated(outTick);
}

/* 0x812D8708 */
uint32_t TrophySyncTitleResult::GetPairedTransCount() const
{
	const TrophySyncTitleRequest *req =
		static_cast<const TrophySyncTitleRequest *>(m_pairedRequest);
	return req->GetTransCount();
}

/* 0x812D8712 */
int TrophySyncTitleResult::GetPairedTransStatus(uint32_t index, TrophyTransBatchEntry *outEntry,
						bool *outAccepted) const
{
	const TrophySyncTitleRequest *req =
		static_cast<const TrophySyncTitleRequest *>(m_pairedRequest);
	int ret = req->GetTransEntry(index, outEntry);
	if (ret < 0)
		return ret;

	if (outEntry->transType == 1) {
		*outAccepted = true;
		return 0;
	}
	if (outEntry->transType == 2) {
		*outAccepted = HasTrophyUnlock(outEntry->trophyId);
		return 0;
	}
	return NP_TROPHY_ERR_GENERIC;
}

/* 0x812E20DC */
TrophySyncSessionBase::TrophySyncSessionBase()
	: m_reqHead(NULL), m_reqTail(NULL), m_resHead(NULL), m_resTail(NULL),
	  m_reqCount(0), m_resCount(0)
{
	sceClibMemset(&m_timeouts, 0, sizeof(m_timeouts));
}

/* 0x812E21A4 */
TrophySyncSessionBase::~TrophySyncSessionBase()
{
	ClearAll();
}

/* 0x812E1FA6 */
int TrophySyncSessionBase::AppendRequest(TrophySyncTitleRequestBase *request)
{
	const TrophyCommId *newComm = request->GetCommId();
	for (TrophySyncTitleRequestBase *cur = m_reqHead; cur != NULL; cur = cur->m_next) {
		if (newComm->Equals(*cur->GetCommId()))
			return NP_TROPHY_ERR_GENERIC;
	}

	request->m_next = NULL;
	request->m_prev = m_reqTail;
	if (m_reqTail != NULL)
		m_reqTail->m_next = request;
	if (m_reqHead == NULL)
		m_reqHead = request;
	m_reqTail = request;
	++m_reqCount;
	return 0;
}

/* 0x812E202E */
void TrophySyncSessionBase::RemoveRequest(TrophySyncTitleRequestBase *request)
{
	if (request->m_next != NULL)
		request->m_next->m_prev = request->m_prev;
	else
		m_reqTail = request->m_prev;
	if (request->m_prev != NULL)
		request->m_prev->m_next = request->m_next;
	else
		m_reqHead = request->m_next;
	request->m_next = NULL;
	request->m_prev = NULL;
	delete request;
	--m_reqCount;
}

/* 0x812E2062 */
void TrophySyncSessionBase::AppendResult(TrophySyncTitleResultBase *result)
{
	result->m_next = NULL;
	result->m_prev = m_resTail;
	if (m_resTail != NULL)
		m_resTail->m_next = result;
	if (m_resHead == NULL)
		m_resHead = result;
	m_resTail = result;
}

/* 0x812E207C */
TrophySyncTitleResultBase *TrophySyncSessionBase::FindResult(const TrophyCommId *commId)
{
	TrophySyncTitleResultBase *curRes = m_resHead;
	for (uint32_t i = 0; i < m_resCount && curRes != NULL; ++i, curRes = curRes->m_next) {
		if (commId->Equals(curRes->GetPayload()->m_meta.commId)) {
			TrophySyncTitleRequestBase *matchedReq = NULL;
			for (TrophySyncTitleRequestBase *curReq = m_reqHead; curReq != NULL;
			     curReq = curReq->m_next) {
				if (commId->Equals(*curReq->GetCommId())) {
					matchedReq = curReq;
					break;
				}
			}
			curRes->m_pairedRequest = matchedReq;
			return (matchedReq != NULL) ? curRes : NULL;
		}
	}
	return NULL;
}

/* 0x812E2100 */
void TrophySyncSessionBase::ClearResults()
{
	while (m_resHead != NULL) {
		TrophySyncTitleResultBase *node = m_resHead;
		m_resHead = node->m_next;
		delete node;
	}
	m_resTail = NULL;
	m_resCount = 0;
}

/* 0x812E213C */
void TrophySyncSessionBase::ClearAll()
{
	while (m_reqHead != NULL) {
		TrophySyncTitleRequestBase *node = m_reqHead;
		m_reqHead = node->m_next;
		delete node;
	}
	m_reqTail = NULL;
	m_reqCount = 0;
	ClearResults();
}

/* 0x812E21F8 */
int TrophySyncSessionBase::ExecuteSyncRpc(TrophyHandle *handle, TrophyNetClient *netClient,
					  const SceNpId *npId)
{
	if (m_reqCount == 0)
		return 0;

	TrophySyncRequestPayload **reqPayloads = static_cast<TrophySyncRequestPayload **>(
		TrophyHeapObject::operator new(m_reqCount * sizeof(TrophySyncRequestPayload *)));
	if (reqPayloads == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	TrophySyncResultPayload **resPayloads = static_cast<TrophySyncResultPayload **>(
		TrophyHeapObject::operator new(m_reqCount * sizeof(TrophySyncResultPayload *)));
	if (resPayloads == NULL) {
		TrophyHeapObject::operator delete(reqPayloads);
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	uint32_t idx = 0;
	TrophySyncTitleRequestBase *curReq = m_reqHead;
	TrophySyncTitleResultBase *curRes = m_resHead;
	while (curReq != NULL && curRes != NULL) {
		reqPayloads[idx] = curReq->GetPayload();
		resPayloads[idx] = curRes->GetPayload();
		++idx;
		curReq = curReq->m_next;
		curRes = curRes->m_next;
	}

	SyncTrophiesRpc rpc;
	SceNpId localNpId;
	int ret = npTrophyCopyNonEmptyNpId(npId, &localNpId);
	if (ret >= 0) {
		rpc.SetTimeouts(&m_timeouts);
		ret = rpc.Send(handle, netClient, &localNpId, reqPayloads, m_reqCount);
		if (ret >= 0) {
			ret = rpc.Receive(handle, resPayloads, m_reqCount, &m_resCount);
			if (ret == NP_TROPHY_ERR_SERVER_NO_RESULTS) {
				m_resCount = 0;
				ret = 0;
			}
		}
	}

	TrophyHeapObject::operator delete(reqPayloads);
	TrophyHeapObject::operator delete(resPayloads);
	return ret;
}

/* 0x812D8168 */
int TrophyNetSyncBatch::AddTitle(const TrophyCommId *commId, uint32_t maxTransEntries,
				 TrophySyncTitleRequestBase **outRequest)
{
	*outRequest = NULL;
	TrophySyncTitleRequest *req = new TrophySyncTitleRequest();
	if (req == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = req->Init(commId, maxTransEntries);
	if (ret >= 0)
		ret = AppendRequest(req);
	if (ret < 0) {
		delete req;
		return ret;
	}
	*outRequest = req;
	return 0;
}

/* 0x812D81E8 */
int TrophyNetSyncBatch::FetchServerConf(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
					const TrophyCommId *commId, TrophyConfData **outConfData)
{
	TrophyNetConfStream confStream;
	TrophySignedSfmReader reader;

	int ret = confStream.OpenConfDirect(handle, httpTemplate, commId, -2, false, &m_timeouts);
	if (ret < 0)
		return ret;

	ret = reader.Init(0);
	if (ret >= 0)
		ret = reader.Parse(handle, confStream.GetStripper());
	if (ret >= 0)
		ret = reader.TakeConfData(commId, outConfData);
	if (ret >= 0)
		ret = confStream.CloseConf();

	if (ret < 0 && confStream.IsOpen())
		confStream.CloseConf();
	return ret;
}

/* 0x812D82D2 */
int TrophyNetSyncBatch::ValidateTitleForSync(TrophyHandle *handle, TrophyNetClient *netClient,
					     TrophySyncTitleRequestBase *request,
					     bool *outShouldSync)
{
	TrophyConfData *serverConf = NULL;
	int ret = FetchServerConf(handle, netClient->GetHttpTemplate(), request->GetCommId(),
				  &serverConf);
	if (ret == NP_TROPHY_ERR_HTTP_NOT_FOUND) {
		*outShouldSync = false;
		ret = 0;
	} else if (ret >= 0) {
		if ((serverConf->GetPlatformMask() & TROPHY_PLATFORM_PSP2) != 0) {
			ret = request->FilterByConfData(serverConf);
			if (ret >= 0) {
				*outShouldSync = true;
				ret = 0;
			}
		} else {
			*outShouldSync = false;
			ret = 0;
		}
	}

	if (serverConf != NULL)
		delete serverConf;
	return ret;
}

/* 0x812D83BC */
int TrophyNetSyncBatch::ExecuteWithFilter(TrophyHandle *handle, TrophyNetClient *netClient,
					  const SceNpId *npId)
{
	TrophySyncTitleRequestBase *cur = m_reqHead;
	while (cur != NULL) {
		TrophySyncTitleRequestBase *next = cur->m_next;
		const TrophyCommId *commId = cur->GetCommId();

		bool excluded = false;
		for (SceSize i = 0; i < 2; ++i) {
			if (commId->EqualsString(kExcludedSyncCommIds[i])) {
				excluded = true;
				break;
			}
		}
		if (excluded) {
			RemoveRequest(cur);
		} else {
			bool shouldSync = false;
			int ret = ValidateTitleForSync(handle, netClient, cur, &shouldSync);
			if (ret < 0) {
				ClearResults();
				return ret;
			}
			if (!shouldSync)
				RemoveRequest(cur);
		}
		cur = next;
	}

	for (TrophySyncTitleRequestBase *req = m_reqHead; req != NULL; req = req->m_next) {
		TrophySyncTitleResult *res = new TrophySyncTitleResult();
		if (res == NULL) {
			ClearResults();
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		int ret = res->InitPayload(128);
		if (ret < 0) {
			delete res;
			ClearResults();
			return ret;
		}
		AppendResult(res);
	}

	int ret = ExecuteSyncRpc(handle, netClient, npId);
	if (ret < 0)
		ClearResults();
	return ret;
}

/* =========================================================================
 * SFM RSA-1024 + HMAC-SHA1 Verifier (0x812CC5F0..0x812CC680, 0x812E189C..0x812E1A4F)
 * ========================================================================= */

/* 0x812CC5F0 */
int TrophySignatureVerifierCore::ParseHeader(const TrophySfmSignature *sig, uint32_t *outMagic,
					     uint32_t *outKeyId)
{
	if (sig->length < 12)
		return NP_TROPHY_ERR_SIG_VERIFIER_MISSING;

	if (outMagic != NULL) {
		*outMagic = (static_cast<uint32_t>(sig->bytes[0]) << 24) |
			    (static_cast<uint32_t>(sig->bytes[1]) << 16) |
			    (static_cast<uint32_t>(sig->bytes[2]) << 8) |
			    static_cast<uint32_t>(sig->bytes[3]);
	}
	if (outKeyId != NULL) {
		*outKeyId = (static_cast<uint32_t>(sig->bytes[4]) << 8) |
			    static_cast<uint32_t>(sig->bytes[5]);
	}
	return 0;
}

/* 0x812CC62C */
int TrophySignatureVerifierCore::Init(const TrophySfmVerifyKeyEntry *keyEntry)
{
	int ret = sceSha1BlockInit(&sha1Ctx);
	if (ret < 0)
		return ret;
	ret = sceHmacSha1BlockInit(&hmacCtx, keyEntry->hmacKey, sizeof(keyEntry->hmacKey));
	return (ret >= 0) ? 0 : ret;
}

/* 0x812CC656 */
int TrophySignatureVerifierCore::Update(const void *bytes, SceSize size)
{
	int ret = sceSha1BlockUpdate(&sha1Ctx, bytes, static_cast<SceUInt32>(size));
	if (ret < 0)
		return ret;
	ret = sceHmacSha1BlockUpdate(&hmacCtx, bytes, static_cast<SceUInt32>(size));
	return (ret >= 0) ? 0 : ret;
}

/* 0x812CC680 */
int TrophySignatureVerifierCore::Verify(const TrophySfmVerifyKeyEntry *keyEntry,
					const TrophySfmSignature *sig)
{
	uint8_t computedHmac[20];
	int ret = sceHmacSha1BlockUpdate(&hmacCtx, sig->bytes, 12);
	if (ret < 0)
		return ret;
	ret = sceHmacSha1BlockResult(&hmacCtx, computedHmac);
	if (ret < 0)
		return ret;

	uint8_t diff = 0;
	for (SceSize i = 0; i < sizeof(computedHmac); ++i)
		diff |= static_cast<uint8_t>(sig->bytes[12 + i] ^ computedHmac[i]);
	if (diff != 0)
		return NP_TROPHY_ERR_SIG_VERIFIER_FAILED;

	uint8_t computedSha1[20];
	ret = sceSha1BlockUpdate(&sha1Ctx, sig->bytes, 32);
	if (ret < 0)
		return ret;
	ret = sceSha1BlockResult(&sha1Ctx, computedSha1);
	if (ret < 0)
		return ret;

	TrophyRsaSlice digestSlice = { computedSha1, sizeof(computedSha1) };
	TrophyRsaSlice sigSlice = { sig->bytes + 32, 128 };
	TrophyRsaKeyPair pubKey = { keyEntry->rsaModulus, keyEntry->rsaExponent };
	if (GcAuthRsaVerify1024(&sigSlice, &digestSlice, &pubKey, 5) != 0)
		return NP_TROPHY_ERR_SIG_VERIFIER_FAILED;
	return 0;
}

TrophySfmSignatureVerifier::TrophySfmSignatureVerifier() : m_keyEntry(NULL)
{
	sceClibMemset(&m_core, 0, sizeof(m_core));
}

static int remapSigVerifyError(int err)
{
	if (err == NP_TROPHY_ERR_SIG_VERIFIER_MISSING)
		return NP_TROPHY_ERR_SIG_FORMAT_INVALID;
	if (err == NP_TROPHY_ERR_SIG_VERIFIER_FAILED)
		return NP_TROPHY_ERR_SIG_VERIFY_FAILED;
	return err;
}

/* 0x812E189C */
int TrophySfmSignatureVerifier::Init(const TrophySfmSignature *sig)
{
	uint32_t magic = 0;
	uint32_t keyId = 0;
	int ret = TrophySignatureVerifierCore::ParseHeader(sig, &magic, &keyId);
	if (ret >= 0 && magic != kSfmSigMagic)
		ret = NP_TROPHY_ERR_SIG_FORMAT_INVALID;

	if (ret >= 0) {
		bool requireOfficial = true;
		ret = npTrophyRequireSfmSignature(&requireOfficial);
		if (ret >= 0) {
			const TrophySfmVerifyKeyEntry *keyEntry =
				findSfmVerifyKey(keyId, requireOfficial);
			if (keyEntry == NULL) {
				ret = NP_TROPHY_ERR_SIG_VERIFY_FAILED;
			} else {
				ret = m_core.Init(keyEntry);
				if (ret >= 0) {
					m_keyEntry = keyEntry;
					return 0;
				}
			}
		}
	}
	return remapSigVerifyError(ret);
}

/* 0x812E19CE */
int TrophySfmSignatureVerifier::Update(const void *bytes, SceSize size)
{
	int ret = m_core.Update(bytes, size);
	return (ret >= 0) ? 0 : remapSigVerifyError(ret);
}

/* 0x812E1A0A */
int TrophySfmSignatureVerifier::Verify(const TrophySfmSignature *sig)
{
	int ret = m_core.Verify(m_keyEntry, sig);
	return (ret >= 0) ? 0 : remapSigVerifyError(ret);
}

/* =========================================================================
 * TrophyConfHandle, TrophyConfSourceBase, TrophyNetConfSource
 * (0x812E1114..0x812E188C, 0x812E1A50..0x812E1AF7)
 * ========================================================================= */

/* 0x812E1AB0 */
TrophyConfHandle::TrophyConfHandle()
	: m_confData(NULL), m_strippedHeaderSize(0), m_lang(-2)
{
}

/* 0x812E1A50 */
TrophyConfHandle::~TrophyConfHandle()
{
	if (m_confData != NULL)
		delete m_confData;
}

/* 0x812E1ACA */
int TrophyConfHandle::Assign(TrophyConfData **ioConfData, SceSize strippedHeaderSize, int lang)
{
	if (m_confData != NULL)
		delete m_confData;
	m_confData = *ioConfData;
	*ioConfData = NULL;
	m_strippedHeaderSize = strippedHeaderSize;
	m_lang = lang;
	return 0;
}

/* 0x812CD778 / 0x812CD7A2 */
TrophyConfData *TrophyConfHandle::DetachConfData()
{
	TrophyConfData *detached = m_confData;
	m_confData = NULL;
	return detached;
}

/* 0x812E1114 */
TrophyConfSourceBase::TrophyConfSourceBase(int sourceKind) : m_sourceKind(sourceKind) {}

int TrophyConfSourceBase::OpenAssetStream(TrophyHandle *handle, const char *filename,
					  TrophyInputStream **outStream)
{
	TrophyConfSourceStream *owned = NULL;
	int ret = OpenOwnedAssetStream(handle, filename, &owned);
	if (ret < 0)
		return ret;
	*outStream = owned;
	return 0;
}

/* 0x812E1610 */
int TrophyConfSourceBase::ExtractOneAsset(TrophyConfHandle **ioHandle, TrophyHandle *handle,
					  TrophyConfSourceBase *source, const char *assetName,
					  const TrophyExtractOptions *options,
					  TrophyAssetDesc *outDesc)
{
	TrophyAssetExtractor extractor;
	TrophyConfSourceStream *stream = NULL;

	if (*ioHandle != NULL) {
		delete *ioHandle;
		*ioHandle = NULL;
	}

	int ret = source->OpenOwnedAssetStream(handle, assetName, &stream);
	if (ret >= 0) {
		ret = extractor.Begin(assetName, stream, options, NULL);
		if (ret >= 0) {
			ret = extractor.Process(handle, outDesc);
			if (ret >= 0 && outDesc->kind == TROPHY_ASSET_SFM)
				ret = extractor.TakeConfHandle(ioHandle);
			if (ret >= 0)
				ret = extractor.End();
		}
	}

	if (extractor.IsActive())
		extractor.End();
	if (stream != NULL)
		stream->CloseAndRelease();
	return ret;
}

/* 0x812E11D2 */
int TrophyConfSourceBase::ExtractAssets(TrophyHandle *handle, const TrophyExtractOptions *options,
					TrophyConfMeta *outMeta,
					TrophyExtractedAssetReport *outReport,
					TrophyConfData **outDefaultConf,
					TrophyConfHandle **outLocalizedConf)
{
	TrophyConfData *defaultConf = NULL;
	TrophyConfHandle *localizedHandle = NULL;
	char assetName[32];
	TrophyAssetDesc assetDesc;

	/* 1. Extract TROPCONF.SFM (always parsed to know groups & trophies) */
	TrophyConfHandle *defaultHandle = NULL;
	int ret = npTrophyFormatSfmFilename(assetName, -2);
	if (ret >= 0) {
		if (TrophyAssetExtractor::ShouldExtractAsset(assetName, options)) {
			ret = ExtractOneAsset(&defaultHandle, handle, this, assetName, options,
					      &assetDesc);
			if (ret >= 0)
				outReport->RecordAsset(&assetDesc);
		} else {
			TrophyExtractOptions parseOnlyOpts = *options;
			parseOnlyOpts.flags |= 0x10000u;
			ret = ExtractOneAsset(&defaultHandle, handle, this, assetName,
					      &parseOnlyOpts, &assetDesc);
		}
		if (ret >= 0) {
			if (defaultHandle == NULL) {
				ret = NP_TROPHY_ERR_GENERIC;
			} else {
				defaultConf = defaultHandle->DetachConfData();
				delete defaultHandle;
				defaultHandle = NULL;
			}
		}
	}
	if (defaultHandle != NULL)
		delete defaultHandle;

	/* 2. Extract localized TROP_XX.SFM (falling back to TROP.SFM) */
	if (ret >= 0) {
		TrophyConfHandle *sfmHandle = NULL;
		ret = npTrophyFormatSfmFilename(assetName, options->lang);
		if (ret >= 0 && TrophyAssetExtractor::ShouldExtractAsset(assetName, options)) {
			ret = ExtractOneAsset(&sfmHandle, handle, this, assetName, options,
					      &assetDesc);
			if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND) {
				ret = npTrophyFormatSfmFilename(assetName, -1);
				if (ret >= 0) {
					ret = ExtractOneAsset(&sfmHandle, handle, this, assetName,
							      options, &assetDesc);
				}
			}
			if (ret >= 0) {
				outReport->RecordAsset(&assetDesc);
				localizedHandle = sfmHandle;
				sfmHandle = NULL;
			}
		}
		if (sfmHandle != NULL)
			delete sfmHandle;
	}

	/* 3. Extract title icon ICON0[_XX].PNG */
	if (ret >= 0) {
		TrophyConfHandle *iconHandle = NULL;
		ret = npTrophyFormatTitleIconFilename(assetName, options->lang, 0);
		if (ret >= 0 && TrophyAssetExtractor::ShouldExtractAsset(assetName, options)) {
			ret = ExtractOneAsset(&iconHandle, handle, this, assetName, options,
					      &assetDesc);
			if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND) {
				ret = npTrophyFormatTitleIconFilename(assetName, -1, 0);
				if (ret >= 0) {
					ret = ExtractOneAsset(&iconHandle, handle, this, assetName,
							      options, &assetDesc);
				}
			}
			if (ret >= 0)
				outReport->RecordAsset(&assetDesc);
		}
		if (iconHandle != NULL)
			delete iconHandle;
	}

	/* 4. Extract group icons GRxxx[_XX].PNG */
	if (ret >= 0 && defaultConf->SupportsGroups()) {
		uint32_t groupMask = 0;
		ret = defaultConf->GetGroupBitset(&groupMask, NULL);
		for (int grp = 0; ret >= 0 && grp < 32; ++grp) {
			if ((groupMask & (1u << grp)) == 0)
				continue;
			TrophyConfHandle *grpHandle = NULL;
			ret = npTrophyFormatGroupIconFilename(assetName, grp, options->lang, 0);
			if (ret >= 0 &&
			    TrophyAssetExtractor::ShouldExtractAsset(assetName, options)) {
				ret = ExtractOneAsset(&grpHandle, handle, this, assetName,
						      options, &assetDesc);
				if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND) {
					ret = npTrophyFormatGroupIconFilename(assetName, grp, -1,
									      0);
					if (ret >= 0) {
						ret = ExtractOneAsset(&grpHandle, handle, this,
								      assetName, options,
								      &assetDesc);
					}
				}
				if (ret >= 0)
					outReport->RecordAsset(&assetDesc);
			}
			if (grpHandle != NULL)
				delete grpHandle;
		}
	}

	/* 5. Extract per-trophy icons TROPxxx.PNG */
	if (ret >= 0) {
		NpTrophyFlagArray trophyMask;
		ret = defaultConf->GetAllTrophyBitset(&trophyMask, NULL);
		for (int id = 0; ret >= 0 && id < 128; ++id) {
			if ((trophyMask.flagBits[id >> 5] & (1u << (id & 31))) == 0)
				continue;
			TrophyConfHandle *trpHandle = NULL;
			ret = npTrophyFormatTrophyIconFilename(assetName, id, 0);
			if (ret >= 0 &&
			    TrophyAssetExtractor::ShouldExtractAsset(assetName, options)) {
				ret = ExtractOneAsset(&trpHandle, handle, this, assetName,
						      options, &assetDesc);
				if (ret >= 0)
					outReport->RecordAsset(&assetDesc);
			}
			if (trpHandle != NULL)
				delete trpHandle;
		}
	}

	if (ret >= 0) {
		outMeta->Clear();
		outMeta->sourceType = static_cast<uint32_t>(m_sourceKind);
		if (outDefaultConf != NULL) {
			*outDefaultConf = defaultConf;
			defaultConf = NULL;
		}
		if (outLocalizedConf != NULL) {
			*outLocalizedConf = localizedHandle;
			localizedHandle = NULL;
		}
	}

	if (defaultConf != NULL)
		delete defaultConf;
	if (localizedHandle != NULL)
		delete localizedHandle;
	return ret;
}

/* 0x812E1718 */
TrophyNetConfSource::TrophyNetConfSource() : TrophyConfSourceBase(2), m_netClient(NULL) {}

/* 0x812E1774 */
int TrophyNetConfSource::Open(TrophyHandle *handle, TrophyNetClient *netClient,
			      const TrophyCommId *commId)
{
	(void)handle;
	SetCommId(commId);
	m_netClient = netClient;
	return 0;
}

/* 0x812E1788 */
int TrophyNetConfSource::Close()
{
	if (!IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	m_netClient = NULL;
	return 0;
}

/* 0x812E17A6 */
int TrophyNetConfSource::EstimateSpace(TrophyHandle *handle, const TrophyExtractOptions *options,
				       uint64_t *outUr0Bytes, uint64_t *outUx0Bytes)
{
	(void)handle;
	(void)options;
	if (outUr0Bytes != NULL)
		*outUr0Bytes = 52428800ULL; /* 50 MiB */
	if (outUx0Bytes != NULL)
		*outUx0Bytes = 0;
	return 0;
}

/* 0x812E17B4 */
int TrophyNetConfSource::OpenOwnedAssetStream(TrophyHandle *handle, const char *assetName,
					      TrophyConfSourceStream **outStream)
{
	TrophyNetSourceAssetStream *stream = new TrophyNetSourceAssetStream();
	if (stream == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = stream->Open(handle, m_netClient->GetHttpTemplate(), &GetCommId(), assetName);
	if (ret < 0) {
		delete stream;
		return (ret == NP_TROPHY_ERR_HTTP_STATUS_ERROR) ? NP_TROPHY_ERR_ASSET_NOT_FOUND
								: ret;
	}
	*outStream = stream;
	return 0;
}

/* =========================================================================
 * Batch 3 bridge functions declared in nptrophy_storage.h
 * ========================================================================= */

TrophySfmSignatureVerifier *npTrophyCreateSfmVerifier()
{
	return new TrophySfmSignatureVerifier();
}

int npTrophySfmVerifierInit(TrophySfmSignatureVerifier *verifier, const TrophySfmSignature *sig)
{
	return verifier->Init(sig);
}

int npTrophySfmVerifierVerify(TrophySfmSignatureVerifier *verifier, const TrophySfmSignature *sig)
{
	return verifier->Verify(sig);
}

TrophyHashSink *npTrophySfmVerifierAsHashSink(TrophySfmSignatureVerifier *verifier)
{
	return verifier;
}

TrophyConfHandle *npTrophyCreateConfHandle()
{
	return new TrophyConfHandle();
}

int npTrophyConfHandleAssign(TrophyConfHandle *handle, TrophyConfData **ioConfData,
			     SceSize strippedHeaderSize, int lang)
{
	return handle->Assign(ioConfData, strippedHeaderSize, lang);
}

const TrophyConfData *npTrophyConfHandleGetData(const TrophyConfHandle *handle)
{
	return handle->GetConfData();
}

SceSize npTrophyConfHandleGetHeaderSize(const TrophyConfHandle *handle)
{
	return handle->GetStrippedHeaderSize();
}

bool npTrophyNetIsSyncAvailable()
{
	return npTrophyNetIsOnline();
}

int npTrophyNetFetchIconUrl(TrophyHandle *handle, TrophyNetClient *netClient,
			    const TrophyCommId *commId, int lang, char *outUrlBuf,
			    SceSize outUrlSize)
{
	(void)outUrlSize;
	TrophyNetIconStream iconStream;
	int ret = iconStream.OpenTitleIcon(handle, netClient->GetHttpTemplate(), NULL, commId,
					   lang, 0, true, outUrlBuf);
	if (ret >= 0)
		iconStream.Close();
	return ret;
}

int npTrophyNetGetSyncInterval(TrophyNetClient *netClient, TrophyHandle *handle,
			       uint64_t *outIntervalSec, const uint32_t *timeouts)
{
	TrophyHttpTimeouts httpTimeouts;
	const TrophyHttpTimeouts *ptr = NULL;
	if (timeouts != NULL) {
		httpTimeouts.connectTimeoutUsec = timeouts[0];
		httpTimeouts.recvTimeoutUsec = timeouts[1];
		httpTimeouts.sendTimeoutUsec = timeouts[2];
		ptr = &httpTimeouts;
	}
	return netClient->GetSyncInterval(handle, outIntervalSec, ptr);
}

int npTrophyNetExecuteSyncBatch(TrophySyncSessionBase *syncSession, TrophyHandle *handle,
				TrophyNetClient *netClient, const SceNpId *npId)
{
	TrophyNetSyncBatch *batch = static_cast<TrophyNetSyncBatch *>(syncSession);
	return batch->ExecuteWithFilter(handle, netClient, npId);
}

int npTrophySyncSessionInitTitle(TrophySyncSessionBase *session, const TrophyCommId *commId,
				 uint32_t maxTransEntries,
				 TrophySyncTitleRequestBase **outRequest)
{
	TrophyNetSyncBatch *batch = static_cast<TrophyNetSyncBatch *>(session);
	return batch->AddTitle(commId, maxTransEntries, outRequest);
}

int npTrophySyncSessionSetTimeouts(TrophySyncSessionBase *session, const uint32_t *timeouts)
{
	TrophyHttpTimeouts httpTimeouts;
	httpTimeouts.connectTimeoutUsec = timeouts[0];
	httpTimeouts.recvTimeoutUsec = timeouts[1];
	httpTimeouts.sendTimeoutUsec = timeouts[2];
	session->SetTimeouts(&httpTimeouts);
	return 0;
}

TrophySyncSessionBase *npTrophyCreateSyncSession()
{
	return new TrophyNetSyncBatch();
}

TrophySyncTitleResultBase *npTrophySyncSessionFindResult(TrophySyncSessionBase *session,
							 const TrophyCommId *commId)
{
	return session->FindResult(commId);
}

void npTrophySyncRequestReset(TrophySyncTitleRequestBase *req)
{
	static_cast<TrophySyncTitleRequest *>(req)->Reset();
}

int npTrophySyncRequestSetServerTag(TrophySyncTitleRequestBase *req, const void *tagBuf)
{
	return req->SetCommSign(tagBuf);
}

int npTrophySyncRequestAddRegister(TrophySyncTitleRequestBase *req,
				   const TrophyTransRegisterEntry *entry)
{
	return static_cast<TrophySyncTitleRequest *>(req)->AddRegisterTrans(entry);
}

int npTrophySyncRequestAddUnlock(TrophySyncTitleRequestBase *req,
				 const TrophyTransUnlockEntry *entry)
{
	return static_cast<TrophySyncTitleRequest *>(req)->AddUnlockTrans(entry);
}

int npTrophySyncResultGetTransCount(const TrophySyncTitleResultBase *result)
{
	return static_cast<int>(
		static_cast<const TrophySyncTitleResult *>(result)->GetPairedTransCount());
}

int npTrophySyncResultGetTransStatus(const TrophySyncTitleResultBase *result, uint32_t index,
				     TrophyTransBatchEntry *outEntry, bool *outAccepted)
{
	return static_cast<const TrophySyncTitleResult *>(result)->GetPairedTransStatus(
		index, outEntry, outAccepted);
}

int npTrophySyncResultGetTrophyState(const TrophySyncTitleResultBase *result, int trophyId,
				     uint64_t *outServerTick, int *outUnlocked)
{
	return result->FindTrophyUnlock(trophyId, outServerTick, outUnlocked);
}

int npTrophySyncResultGetServerTimestamp(const TrophySyncTitleResultBase *result,
					 uint64_t *outTimestamp)
{
	result->GetLastUpdated(outTimestamp);
	return 0;
}
