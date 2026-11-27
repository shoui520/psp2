/*
 * np_roster_xml.cpp: SceNpRoster XML request builder and streaming SAX response
 * parser (0x81309ACC..0x8130C7E7).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <std/ctype.h>
#include <scetypes.h>
#include <rtc.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_roster.h"

extern "C" long sce_paf_atol(const char *str);

namespace {

static char s_npEnvironment[20];
static char s_fwVersionStr[8];
static char s_jidScratchBuf[512];
static SceUInt8 s_xmlInitialized = 0;

/* --------------------------------------------------------------------------
 * JID formatting and scratch buffer helpers (0x8130C6D8..0x8130C7E7)
 * -------------------------------------------------------------------------- */
/* 0x8130C6D8 */
static int formatNpIdToJid(const SceNpId *npId, char *dstBuf, SceSize dstCapacity,
			   SceSize *outRequired)
{
	NpJid jidParts;
	char workBuf[1024];
	SceSize workUsed = 0;

	int ret = SceNpCommon_40CC4D15(&jidParts, npId, workBuf, 256, &workUsed);
	if (ret < 0)
		return ret;
	jidParts.resource = NULL;
	return SceNpCommon_85699AE2(&jidParts, dstBuf, dstCapacity, outRequired);
}

/* 0x8130C734..0x8130C7D5: single-slot scratch buffer manager */
class RosterScratchBuffer {
public:
	RosterScratchBuffer();
	int Attach(char *externalBuf, SceSize capacity);
	int Checkout(char **outBuf);
	void Checkin();
	SceSize Capacity() const;

private:
	void *(*m_allocFn)(SceSize);
	void (*m_freeFn)(void *);
	char *m_externalBuf;
	char *m_ownedBuf;
	char *m_activeBuf;
	SceSize m_capacity;
	int m_inUseCount;
	int m_maxUsers;
};

/* 0x8130C734 */
RosterScratchBuffer::RosterScratchBuffer()
	: m_allocFn(NULL),
	  m_freeFn(NULL),
	  m_externalBuf(NULL),
	  m_ownedBuf(NULL),
	  m_activeBuf(NULL),
	  m_capacity(0),
	  m_inUseCount(0),
	  m_maxUsers(1)
{
}

/* 0x8130C74A */
int RosterScratchBuffer::Attach(char *externalBuf, SceSize capacity)
{
	if (m_inUseCount != 0)
		return 1;
	m_activeBuf = NULL;
	m_externalBuf = NULL;
	if (m_ownedBuf != NULL) {
		if (m_freeFn != NULL)
			m_freeFn(m_ownedBuf);
		m_ownedBuf = NULL;
	}
	if (externalBuf != NULL) {
		m_externalBuf = externalBuf;
		m_activeBuf = externalBuf;
	} else if (capacity != 0) {
		if (m_allocFn == NULL || m_freeFn == NULL)
			return 3;
		m_ownedBuf = reinterpret_cast<char *>(m_allocFn(capacity));
		if (m_ownedBuf == NULL)
			return 4;
		m_activeBuf = m_ownedBuf;
	}
	m_capacity = capacity;
	return 0;
}

/* 0x8130C79A */
int RosterScratchBuffer::Checkout(char **outBuf)
{
	if (outBuf == NULL)
		return 2;
	if (m_inUseCount == m_maxUsers)
		return 1;
	if (m_inUseCount < 0 || m_inUseCount > m_maxUsers)
		return 3;
	*outBuf = m_activeBuf;
	++m_inUseCount;
	return 0;
}

/* 0x8130C7CA */
void RosterScratchBuffer::Checkin()
{
	--m_inUseCount;
}

SceSize RosterScratchBuffer::Capacity() const
{
	return m_capacity;
}

/* 0x8130C7D6 */
static int formatUnsignedInt(char *dst, SceSize dstCapacity, SceUInt32 value)
{
	return sce_paf_snprintf(dst, dstCapacity, "%u", value);
}

/* --------------------------------------------------------------------------
 * Request XML DOM builder (0x8130A358..0x8130B017)
 * -------------------------------------------------------------------------- */
class RosterXmlDocument {
public:
	RosterXmlDocument();
	~RosterXmlDocument();

	int Begin();
	int End();
	int Serialize(char **outXml, SceSize *outLen);
	static void FreeSerialized(char *xmlText);

	int BuildAcceptFriend(const SceNpId *npId, const char *ticketBase64, SceSize ticketLen,
			      const char *envName, const char *fwVersion);
	int BuildDeleteFriend(const SceNpId *npId, const char *ticketBase64, SceSize ticketLen,
			      const char *envName, const char *fwVersion);
	int BuildGetBlockList(SceUInt32 startIndex, SceUInt32 maxCount,
			      const char *ticketBase64, SceSize ticketLen,
			      const char *envName, const char *fwVersion);
	int BuildGetFriendList(int optionFlags, const char *ticketBase64, SceSize ticketLen,
			       const char *envName, const char *fwVersion);
	int BuildRequestFriend(const SceNpId *npId, const char *comment,
			       const char *ticketBase64, SceSize ticketLen,
			       const char *envName, const char *fwVersion);
	int BuildUpdateBlockList(const NpRosterBlockListEntry *entries, SceSize count,
				 const char *ticketBase64, SceSize ticketLen,
				 const char *envName, const char *fwVersion);

private:
	int BeginRootElement(const char *rootTag, const char *ticketBase64, SceSize ticketLen,
			     const char *envName, const char *fwVersion,
			     BxceAttributeList **outAttrs, BxceNodeId *outRootNode);
	static void DestroyAttrs(BxceAttributeList *attrs);
	int AppendOptionalComment(BxceNodeId itemNode, const char *comment);

	BxceDocument m_document;
	SceUInt8 m_initialized;
	BxceSerializer m_serializer;
	RosterScratchBuffer m_scratch;
	char *m_jidBuf;
};

/* 0x8130AEE8 */
RosterXmlDocument::RosterXmlDocument()
	: m_initialized(0),
	  m_jidBuf(NULL)
{
	ScebXCe_63593407(&m_document);
	ScebXCe_8EA5A077(&m_serializer);
}

/* 0x8130AF10 */
RosterXmlDocument::~RosterXmlDocument()
{
	ScebXCe_D24E80F2();
	ScebXCe_6E809E76();
}

/* 0x8130AF28 */
int RosterXmlDocument::Begin()
{
	if (m_initialized != 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	int ret = m_scratch.Attach(s_jidScratchBuf, sizeof(s_jidScratchBuf));
	if (ret < 0)
		return ret;
	if (m_scratch.Checkout(&m_jidBuf) != 0)
		return ret;
	if (ScebXCe_FAB50608(&m_document, 1) != 0)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;
	m_initialized = 1;
	return 0;
}

/* 0x8130AF8A */
int RosterXmlDocument::End()
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	ScebXCe_B0891B13(&m_document);
	m_scratch.Checkin();
	m_initialized = 0;
	m_jidBuf = NULL;
	return 0;
}

/* 0x8130AFB8 */
int RosterXmlDocument::Serialize(char **outXml, SceSize *outLen)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (outXml == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;
	if (ScebXCe_D7FE96A2(&m_serializer, &m_document, outXml, NULL, 1) != 0)
		return NP_ROSTER_ERROR_XML_ENCODE;
	if (outLen != NULL)
		*outLen = sce_paf_strlen(*outXml);
	return 0;
}

/* 0x8130B00C */
void RosterXmlDocument::FreeSerialized(char *xmlText)
{
	ScebXCe_EC560647(xmlText);
}

int RosterXmlDocument::BeginRootElement(const char *rootTag, const char *ticketBase64,
					SceSize ticketLen, const char *envName,
					const char *fwVersion, BxceAttributeList **outAttrs,
					BxceNodeId *outRootNode)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		ScebXCe_B8DDE741(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;

	*outAttrs = attrs;
	if (ScebXCe_E7C8B20D(attrs) != 0)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;

	if (ScebXCe_3DD761CF(attrs, "platform", "psp2") != 0 ||
	    ScebXCe_3DD761CF(attrs, "sv", fwVersion) != 0)
		return NP_ROSTER_ERROR_XML_ENCODE;

	BxceNodeId docRoot = ScebXCe_75E1B095(&m_document);
	BxceNodeId rootNode = ScebXCe_AC0E4F67(&m_document, docRoot, rootTag, attrs, NULL, 0);
	if (rootNode == 0)
		return NP_ROSTER_ERROR_XML_ENCODE;

	SceSize envLen = sce_paf_strlen(envName);
	if (ScebXCe_AC0E4F67(&m_document, rootNode, "env", NULL, envName, envLen) == 0)
		return NP_ROSTER_ERROR_XML_ENCODE;

	if (ScebXCe_AC0E4F67(&m_document, rootNode, "ticket", NULL, ticketBase64, ticketLen) == 0)
		return NP_ROSTER_ERROR_XML_ENCODE;

	*outRootNode = rootNode;
	return 0;
}

void RosterXmlDocument::DestroyAttrs(BxceAttributeList *attrs)
{
	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		ScebXCe_EC560647(attrs);
	}
}

/* 0x8130A358: accept_friend XML body */
int RosterXmlDocument::BuildAcceptFriend(const SceNpId *npId, const char *ticketBase64,
					 SceSize ticketLen, const char *envName,
					 const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (npId == NULL)
		return NP_ROSTER_ERROR_INVALID_ARGUMENT;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("friendlist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		SceSize jidLen = 0;
		ret = formatNpIdToJid(npId, m_jidBuf, m_scratch.Capacity(), &jidLen);
		if (ret >= 0) {
			ScebXCe_B367042F(attrs);
			if (ScebXCe_3DD761CF(attrs, "jid", m_jidBuf) != 0 ||
			    ScebXCe_AC0E4F67(&m_document, rootNode, "item", attrs, NULL, 0) == 0) {
				ret = NP_ROSTER_ERROR_XML_ENCODE;
			} else {
				ret = 0;
			}
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

/* 0x8130A51C: delete_friend XML body */
int RosterXmlDocument::BuildDeleteFriend(const SceNpId *npId, const char *ticketBase64,
					 SceSize ticketLen, const char *envName,
					 const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (npId == NULL)
		return NP_ROSTER_ERROR_INVALID_ARGUMENT;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("friendlist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		SceSize jidLen = 0;
		ret = formatNpIdToJid(npId, m_jidBuf, m_scratch.Capacity(), &jidLen);
		if (ret >= 0) {
			ScebXCe_B367042F(attrs);
			if (ScebXCe_3DD761CF(attrs, "jid", m_jidBuf) != 0 ||
			    ScebXCe_AC0E4F67(&m_document, rootNode, "item", attrs, NULL, 0) == 0) {
				ret = NP_ROSTER_ERROR_XML_ENCODE;
			} else {
				ret = 0;
			}
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

/* 0x8130A6E0: get_block_list XML body */
int RosterXmlDocument::BuildGetBlockList(SceUInt32 startIndex, SceUInt32 maxCount,
					 const char *ticketBase64, SceSize ticketLen,
					 const char *envName, const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("blocklist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		int numLen = formatUnsignedInt(m_jidBuf, m_scratch.Capacity(), startIndex + 1);
		if (numLen < 0) {
			ret = numLen;
		} else if (ScebXCe_AC0E4F67(&m_document, rootNode, "start", NULL, m_jidBuf,
					    static_cast<SceUInt32>(numLen)) == 0) {
			ret = NP_ROSTER_ERROR_XML_ENCODE;
		} else {
			numLen = formatUnsignedInt(m_jidBuf, m_scratch.Capacity(), maxCount);
			if (numLen < 0) {
				ret = numLen;
			} else if (ScebXCe_AC0E4F67(&m_document, rootNode, "max", NULL, m_jidBuf,
						    static_cast<SceUInt32>(numLen)) == 0) {
				ret = NP_ROSTER_ERROR_XML_ENCODE;
			} else {
				ret = 0;
			}
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

/* 0x8130A89C: get_friend_list XML body */
int RosterXmlDocument::BuildGetFriendList(int optionFlags, const char *ticketBase64,
					  SceSize ticketLen, const char *envName,
					  const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("friendlist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		if ((optionFlags & NP_ROSTER_FRIEND_LIST_OPT_UPD_DATE) != 0 &&
		    ScebXCe_AC0E4F67(&m_document, rootNode, "with-upd-date", NULL, NULL, 0) == 0) {
			ret = NP_ROSTER_ERROR_XML_ENCODE;
		} else if ((optionFlags & NP_ROSTER_FRIEND_LIST_OPT_MSG_FLAG) != 0 &&
			   ScebXCe_AC0E4F67(&m_document, rootNode, "with-msg-flag",
					    NULL, NULL, 0) == 0) {
			ret = NP_ROSTER_ERROR_XML_ENCODE;
		} else {
			ret = 0;
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

/* 0x8130AA14: request_friend XML body (with optional XML-escaped <message>) */
int RosterXmlDocument::BuildRequestFriend(const SceNpId *npId, const char *comment,
					  const char *ticketBase64, SceSize ticketLen,
					  const char *envName, const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (npId == NULL)
		return NP_ROSTER_ERROR_INVALID_ARGUMENT;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("friendlist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		SceSize jidLen = 0;
		ret = formatNpIdToJid(npId, m_jidBuf, m_scratch.Capacity(), &jidLen);
		if (ret >= 0) {
			ScebXCe_B367042F(attrs);
			if (ScebXCe_3DD761CF(attrs, "jid", m_jidBuf) != 0) {
				ret = NP_ROSTER_ERROR_XML_ENCODE;
			} else {
				BxceNodeId itemNode = ScebXCe_AC0E4F67(&m_document, rootNode,
								       "item", attrs, NULL, 0);
				if (itemNode == 0)
					ret = NP_ROSTER_ERROR_XML_ENCODE;
				else
					ret = AppendOptionalComment(itemNode, comment);
			}
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

/* 0x8130ACA8: update_block_list XML body */
int RosterXmlDocument::BuildUpdateBlockList(const NpRosterBlockListEntry *entries, SceSize count,
					    const char *ticketBase64, SceSize ticketLen,
					    const char *envName, const char *fwVersion)
{
	if (m_initialized == 0)
		return NP_ROSTER_ERROR_INTERNAL_STATE;

	BxceAttributeList *attrs = NULL;
	BxceNodeId rootNode = 0;
	int ret = BeginRootElement("blocklist", ticketBase64, ticketLen, envName, fwVersion,
				   &attrs, &rootNode);
	if (ret >= 0) {
		BxceNodeId listNode = ScebXCe_AC0E4F67(&m_document, rootNode, "list",
						       NULL, NULL, 0);
		if (listNode == 0) {
			ret = NP_ROSTER_ERROR_XML_ENCODE;
		} else {
			ScebXCe_B367042F(attrs);
			ret = 0;
			for (SceSize pos = 0; pos < count; ++pos) {
				SceSize jidLen = 0;
				ret = formatNpIdToJid(&entries[pos].npId, m_jidBuf,
						      m_scratch.Capacity(), &jidLen);
				if (ret < 0)
					break;
				if (ScebXCe_3DD761CF(attrs, "jid", m_jidBuf) != 0) {
					ret = NP_ROSTER_ERROR_XML_ENCODE;
					break;
				}
				const char *actionStr;
				if (entries[pos].action == 0) {
					actionStr = "add";
				} else if (entries[pos].action == 1) {
					actionStr = "remove";
				} else {
					break;
				}
				if (ScebXCe_3DD761CF(attrs, "type", actionStr) != 0 ||
				    ScebXCe_AC0E4F67(&m_document, listNode, "item",
						     attrs, NULL, 0) == 0) {
					ret = NP_ROSTER_ERROR_XML_ENCODE;
					break;
				}
				ScebXCe_B367042F(attrs);
			}
		}
	}

	DestroyAttrs(attrs);
	return ret;
}

int RosterXmlDocument::AppendOptionalComment(BxceNodeId itemNode, const char *comment)
{
	if (comment == NULL)
		return 0;
	SceSize commentLen = sce_paf_strlen(comment);
	if (commentLen == 0)
		return 0;

	SceSize escapedSize = 0;
	int ret = SceNpCommon_C2BF5C1F(NULL, 0, reinterpret_cast<const SceUInt8 *>(comment),
				       commentLen, &escapedSize);
	if (ret < 0)
		return ret;

	if (escapedSize - 1 == commentLen) {
		if (ScebXCe_AC0E4F67(&m_document, itemNode, "message", NULL, comment,
				     commentLen) == 0)
			return NP_ROSTER_ERROR_XML_ENCODE;
		return 0;
	}

	char *escapedBuf = reinterpret_cast<char *>(sceNpCommonMallocNpServerName(escapedSize));
	if (escapedBuf == NULL)
		return NP_ROSTER_ERROR_XML_ENCODE;

	ret = SceNpCommon_C2BF5C1F(escapedBuf, escapedSize,
				   reinterpret_cast<const SceUInt8 *>(comment), commentLen, NULL);
	if (ret < 0) {
		sceNpCommonFreeNpServerName(escapedBuf);
		return ret;
	}
	if (ScebXCe_AC0E4F67(&m_document, itemNode, "message", NULL, escapedBuf,
			     escapedSize - 1) == 0) {
		sceNpCommonFreeNpServerName(escapedBuf);
		return NP_ROSTER_ERROR_XML_ENCODE;
	}
	sceNpCommonFreeNpServerName(escapedBuf);
	return 0;
}

/* --------------------------------------------------------------------------
 * Streaming SAX response parser hierarchy (0x8130B018..0x8130C6D7)
 * -------------------------------------------------------------------------- */
/* 0x8130B268..0x8130B31B: chainable output buffer writer */
class RosterValueWriter {
public:
	RosterValueWriter();
	virtual ~RosterValueWriter();
	virtual void OnBeforeWrite();

	void Bind(void *buf, SceSize capacity);
	void SetDelegate(RosterValueWriter *delegate);
	void *GetBuffer() const;
	SceSize GetWritten() const;
	void Reset();
	int Write(const void *src, SceSize len);

private:
	SceUInt8 *m_buf;
	SceSize m_capacity;
	SceSize m_written;
	RosterValueWriter *m_delegate;
};

/* 0x8130B268 */
RosterValueWriter::RosterValueWriter()
	: m_buf(NULL),
	  m_capacity(0),
	  m_written(0),
	  m_delegate(NULL)
{
}

RosterValueWriter::~RosterValueWriter()
{
}

void RosterValueWriter::OnBeforeWrite()
{
}

/* 0x8130B280 */
void RosterValueWriter::Bind(void *buf, SceSize capacity)
{
	m_buf = reinterpret_cast<SceUInt8 *>(buf);
	m_capacity = capacity;
}

void RosterValueWriter::SetDelegate(RosterValueWriter *delegate)
{
	m_delegate = delegate;
}

void *RosterValueWriter::GetBuffer() const
{
	return (m_delegate != NULL) ? m_delegate->GetBuffer() : m_buf;
}

SceSize RosterValueWriter::GetWritten() const
{
	return (m_delegate != NULL) ? m_delegate->GetWritten() : m_written;
}

void RosterValueWriter::Reset()
{
	if (m_delegate != NULL)
		m_delegate->Reset();
	else
		m_written = 0;
}

int RosterValueWriter::Write(const void *src, SceSize len)
{
	OnBeforeWrite();
	if (m_delegate != NULL)
		return m_delegate->Write(src, len);
	if (src == NULL || len == 0 || m_buf == NULL)
		return -1;
	if (m_written + len > m_capacity)
		return -1;
	sce_paf_memcpy(m_buf + m_written, src, len);
	m_written += len;
	return 0;
}

/* 0x8130B31C..0x8130B7CF: attribute readers */
enum RosterParseError {
	ROSTER_PARSE_OK		= 0,
	ROSTER_PARSE_INTERNAL	= 1,
	ROSTER_PARSE_SYNTAX	= 2
};

class RosterAttrReader {
public:
	RosterAttrReader();
	virtual ~RosterAttrReader();
	virtual int Parse(const BxceAttributeList *attrs) = 0;

	void SetWriter(RosterValueWriter *writer);
	RosterValueWriter *GetWriter() const;
	int GetError() const;
	void ClearError();

protected:
	RosterValueWriter *m_writer;
	int m_error;
};

/* 0x8130B31C */
RosterAttrReader::RosterAttrReader()
	: m_writer(NULL),
	  m_error(ROSTER_PARSE_OK)
{
}

RosterAttrReader::~RosterAttrReader()
{
}

void RosterAttrReader::SetWriter(RosterValueWriter *writer)
{
	m_writer = writer;
}

RosterValueWriter *RosterAttrReader::GetWriter() const
{
	return m_writer;
}

int RosterAttrReader::GetError() const
{
	return m_error;
}

void RosterAttrReader::ClearError()
{
	m_error = ROSTER_PARSE_OK;
}

/* 0x8130B332, 0x8130B3A8: hex "result" attribute -> 0 or (0x80550B00 | code) */
class ResultAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);

private:
	SceUInt32 DecodeResultCode(const char *hexText);
};

/* 0x8130B3A8 */
int ResultAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "result",
					  static_cast<int>(sce_paf_strlen("result")));
	if (attr == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	SceUInt32 mapped = DecodeResultCode(attr->value);
	if (m_writer == NULL || m_writer->Write(&mapped, sizeof(mapped)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B332 */
SceUInt32 ResultAttrReader::DecodeResultCode(const char *hexText)
{
	if (hexText == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0x80550B00u;
	}
	char *endPtr = NULL;
	long code = sce_paf_strtol(hexText, &endPtr, 16);
	if (endPtr == hexText) {
		m_error = ROSTER_PARSE_SYNTAX;
	} else if (*endPtr == '\0' && *hexText == '\0') {
		m_error = ROSTER_PARSE_SYNTAX;
		code = 0;
	} else if (code == 0 || code == 0x4E) {
		return 0;
	}
	return static_cast<SceUInt32>(code) | 0x80550B00u;
}

/* 0x8130B416: decimal "size" attribute */
class SizeAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int SizeAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "size", static_cast<int>(sce_paf_strlen("size")));
	if (attr == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	SceUInt32 count = static_cast<SceUInt32>(sce_paf_atol(attr->value));
	if (m_writer == NULL || m_writer->Write(&count, sizeof(count)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B482: decimal "total" attribute */
class TotalAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int TotalAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "total",
					  static_cast<int>(sce_paf_strlen("total")));
	if (attr == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	SceUInt32 total = static_cast<SceUInt32>(sce_paf_atol(attr->value));
	if (m_writer == NULL || m_writer->Write(&total, sizeof(total)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B4EE: "jid" attribute -> SceNpId */
class JidAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int JidAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "jid", static_cast<int>(sce_paf_strlen("jid")));
	if (attr == NULL || attr->value == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}

	SceNpId npId;
	sce_paf_memset(&npId, 0, sizeof(npId));

	const char *jid = attr->value;
	SceSize jidLen = static_cast<SceSize>(attr->valueLength);
	SceSize handleLen = 0;
	while (handleLen < jidLen) {
		unsigned char ch = static_cast<unsigned char>(jid[handleLen]);
		if ((sce_paf_look_ctype_table(ch) & 7) == 0 &&
		    ch != '+' && ch != '-' && ch != '_' && ch != '.')
			break;
		++handleLen;
	}

	if (handleLen == 0 || handleLen > 16 || jid[handleLen] != '@') {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	sce_paf_memcpy(npId.handle.data, jid, handleLen);

	const char *domain = jid + handleLen + 1;
	if ((sce_paf_look_ctype_table(static_cast<unsigned char>(domain[0])) & 7) == 0 ||
	    (sce_paf_look_ctype_table(static_cast<unsigned char>(domain[1])) & 7) == 0 ||
	    domain[2] != '.') {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	if ((sce_paf_look_ctype_table(static_cast<unsigned char>(domain[3])) & 7) == 0 ||
	    (sce_paf_look_ctype_table(static_cast<unsigned char>(domain[4])) & 7) == 0 ||
	    domain[5] != '.') {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}

	npId.opt[0] = static_cast<SceUInt8>(domain[0]);
	npId.opt[1] = static_cast<SceUInt8>(domain[1]);
	npId.opt[2] = static_cast<SceUInt8>(domain[3]);
	npId.opt[3] = static_cast<SceUInt8>(domain[4]);

	const char *envScan = domain + 6;
	while (*envScan != '\0' && *envScan != '.')
		++envScan;
	if (*envScan != '.' || sce_paf_strncmp(envScan + 1, "playstation.net", 16) != 0) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}

	npId.reserved[0] = 1;
	if (m_writer == NULL || m_writer->Write(&npId, sizeof(npId)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B688: "subscription" attribute */
class SubscriptionAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int SubscriptionAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "subscription",
					  static_cast<int>(sce_paf_strlen("subscription")));
	if (attr == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	int subVal = sce_paf_atoi(attr->value);
	if (m_writer == NULL || m_writer->Write(&subVal, sizeof(subVal)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B6F4: "upd-date" RFC3339 timestamp attribute */
class UpdateDateAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int UpdateDateAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "upd-date",
					  static_cast<int>(sce_paf_strlen("upd-date")));
	if (attr == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	SceRtcTick utcTick;
	if (sceRtcParseRFC3339(&utcTick, attr->value) < 0) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	if (m_writer == NULL || m_writer->Write(&utcTick, sizeof(utcTick)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B76A: optional "msg-flag" attribute (defaults to 0 when absent) */
class MsgFlagAttrReader : public RosterAttrReader {
public:
	virtual int Parse(const BxceAttributeList *attrs);
};

int MsgFlagAttrReader::Parse(const BxceAttributeList *attrs)
{
	int msgFlag = 0;
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "msg-flag",
					  static_cast<int>(sce_paf_strlen("msg-flag")));
	if (attr != NULL)
		msgFlag = sce_paf_atoi(attr->value);
	if (m_writer == NULL || m_writer->Write(&msgFlag, sizeof(msgFlag)) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130B7D0..0x8130BB07: base XML element state machine */
class RosterElementParser {
public:
	static const SceUInt32 kMaxAttrs = 8;

	RosterElementParser();
	virtual ~RosterElementParser();

	virtual void Reset();
	virtual int ClearAll();
	virtual void OnChildStart(const char *name, const BxceAttributeList *attrs);
	virtual void OnChildEnd(const char *name);

	void AddRequiredAttr(RosterAttrReader *reader);
	void AddOptionalAttr(RosterAttrReader *reader);
	void SetTagName(const char *tagName);
	const char *GetTagName() const;
	void SetWriter(RosterValueWriter *writer);
	RosterValueWriter *GetWriter() const;
	int GetError() const;
	void HandleStartElement(const char *name, const BxceAttributeList *attrs);
	void HandleEndElement(const char *name);

protected:
	void ParseAttributes(const BxceAttributeList *attrs);

	SceUInt8 m_childComplete;
	int m_phase;
	int m_error;
	const char *m_tagName;
	RosterValueWriter *m_writer;
	RosterAttrReader *m_requiredAttrs[kMaxAttrs];
	SceUInt32 m_requiredCount;
	RosterAttrReader *m_optionalAttrs[kMaxAttrs];
	SceUInt32 m_optionalCount;
};

/* 0x8130B7D0 */
RosterElementParser::RosterElementParser()
	: m_childComplete(1),
	  m_phase(0),
	  m_error(ROSTER_PARSE_OK),
	  m_tagName(""),
	  m_writer(NULL),
	  m_requiredCount(0),
	  m_optionalCount(0)
{
}

RosterElementParser::~RosterElementParser()
{
}

void RosterElementParser::Reset()
{
	m_childComplete = 1;
	m_phase = 0;
	m_error = ROSTER_PARSE_OK;
	if (m_writer != NULL)
		m_writer->Reset();
	for (SceUInt32 pos = 0; pos < m_requiredCount; ++pos) {
		if (m_requiredAttrs[pos]->GetWriter() != NULL)
			m_requiredAttrs[pos]->GetWriter()->Reset();
	}
	for (SceUInt32 pos = 0; pos < m_optionalCount; ++pos) {
		if (m_optionalAttrs[pos]->GetWriter() != NULL)
			m_optionalAttrs[pos]->GetWriter()->Reset();
	}
}

int RosterElementParser::ClearAll()
{
	Reset();
	m_tagName = "";
	m_writer = NULL;
	return 0;
}

void RosterElementParser::OnChildStart(const char *name, const BxceAttributeList *attrs)
{
	(void)name;
	(void)attrs;
}

void RosterElementParser::OnChildEnd(const char *name)
{
	(void)name;
}

/* 0x8130BC5C */
void RosterElementParser::AddRequiredAttr(RosterAttrReader *reader)
{
	if (reader != NULL && m_requiredCount < kMaxAttrs)
		m_requiredAttrs[m_requiredCount++] = reader;
}

/* 0x8130BAD4 */
void RosterElementParser::AddOptionalAttr(RosterAttrReader *reader)
{
	if (reader != NULL && m_optionalCount < kMaxAttrs)
		m_optionalAttrs[m_optionalCount++] = reader;
}

void RosterElementParser::SetTagName(const char *tagName)
{
	m_tagName = tagName;
}

const char *RosterElementParser::GetTagName() const
{
	return m_tagName;
}

void RosterElementParser::SetWriter(RosterValueWriter *writer)
{
	m_writer = writer;
}

RosterValueWriter *RosterElementParser::GetWriter() const
{
	return m_writer;
}

int RosterElementParser::GetError() const
{
	return m_error;
}

/* 0x8130BA04 */
void RosterElementParser::HandleStartElement(const char *name, const BxceAttributeList *attrs)
{
	if (m_error != ROSTER_PARSE_OK)
		return;

	if (m_phase == 0) {
		if (sce_paf_strcmp(name, m_tagName) != 0) {
			m_error = ROSTER_PARSE_SYNTAX;
			return;
		}
		ParseAttributes(attrs);
		m_phase = 1;
	} else if (m_phase == 1) {
		OnChildStart(name, attrs);
		m_phase = 2;
	} else if (m_phase == 2) {
		OnChildStart(name, attrs);
	} else if (m_phase == 3) {
		m_error = ROSTER_PARSE_SYNTAX;
	} else {
		m_error = ROSTER_PARSE_INTERNAL;
	}
}

/* 0x8130BA76 */
void RosterElementParser::HandleEndElement(const char *name)
{
	if (m_error != ROSTER_PARSE_OK)
		return;

	if (m_phase == 1) {
		if (sce_paf_strcmp(name, m_tagName) != 0)
			m_error = ROSTER_PARSE_SYNTAX;
		else
			m_phase = 3;
	} else if (m_phase == 2) {
		OnChildEnd(name);
		if (m_childComplete != 0)
			m_phase = 1;
	} else if (m_phase == 0 || m_phase == 3) {
		m_error = ROSTER_PARSE_SYNTAX;
	} else {
		m_error = ROSTER_PARSE_INTERNAL;
	}
}

/* 0x8130B980 */
void RosterElementParser::ParseAttributes(const BxceAttributeList *attrs)
{
	for (SceUInt32 pos = 0; pos < m_requiredCount; ++pos) {
		m_requiredAttrs[pos]->Parse(attrs);
		int attrErr = m_requiredAttrs[pos]->GetError();
		if (attrErr != ROSTER_PARSE_OK) {
			m_error = attrErr;
			return;
		}
	}
	for (SceUInt32 pos = 0; pos < m_optionalCount; ++pos) {
		m_optionalAttrs[pos]->Parse(attrs);
		int attrErr = m_optionalAttrs[pos]->GetError();
		if (attrErr == ROSTER_PARSE_SYNTAX) {
			m_optionalAttrs[pos]->ClearError();
		} else if (attrErr == ROSTER_PARSE_INTERNAL) {
			m_error = ROSTER_PARSE_INTERNAL;
			return;
		}
	}
}

/* 0x8130BD50: <item .../> element parser */
class RosterItemElementParser : public RosterElementParser {
public:
	RosterItemElementParser();
};

/* 0x8130BD50 */
RosterItemElementParser::RosterItemElementParser()
{
	SetTagName("item");
}

/* 0x8130BB08..0x8130BD4F: <list size="N"><item .../>...</list> element parser */
class RosterListElementParser : public RosterElementParser {
public:
	RosterListElementParser();

	void SetItemParser(RosterElementParser *itemParser);
	void SetSizeWriterDelegate(RosterValueWriter *delegate);

	virtual void Reset();
	virtual int ClearAll();
	virtual void OnChildStart(const char *name, const BxceAttributeList *attrs);
	virtual void OnChildEnd(const char *name);

private:
	/* Writes parsed <list size="N"> into both m_expectedCount and optional external output */
	class DualSizeWriter : public RosterValueWriter {
	public:
		DualSizeWriter();
		void SetTarget(SceUInt32 *countDest);
		void SetMirrorOutput(RosterValueWriter *extraOut);
		virtual void OnBeforeWrite();
		int WriteSize(SceUInt32 count);

	private:
		SceUInt32 *m_countDest;
		RosterValueWriter *m_extraOut;
	};

	/* SizeAttrReader subclass that mirrors size into DualSizeWriter's external destination */
	class ListSizeAttrReader : public RosterAttrReader {
	public:
		ListSizeAttrReader();
		void SetWriter(DualSizeWriter *writer);
		virtual int Parse(const BxceAttributeList *attrs);

	private:
		DualSizeWriter *m_dualWriter;
	};

	SceUInt32 m_expectedCount;
	SceUInt32 m_parsedCount;
	RosterElementParser *m_itemParser;
	DualSizeWriter m_sizeWriter;
	ListSizeAttrReader m_sizeReader;
};

RosterListElementParser::DualSizeWriter::DualSizeWriter()
	: m_countDest(NULL),
	  m_extraOut(NULL)
{
}

void RosterListElementParser::DualSizeWriter::SetTarget(SceUInt32 *countDest)
{
	m_countDest = countDest;
	Bind(countDest, sizeof(SceUInt32));
}

void RosterListElementParser::DualSizeWriter::SetMirrorOutput(RosterValueWriter *extraOut)
{
	m_extraOut = extraOut;
}

void RosterListElementParser::DualSizeWriter::OnBeforeWrite()
{
	if (m_extraOut != NULL && m_countDest != NULL)
		m_extraOut->Reset();
}

int RosterListElementParser::DualSizeWriter::WriteSize(SceUInt32 count)
{
	if (m_countDest != NULL)
		*m_countDest = count;
	if (m_extraOut != NULL)
		return m_extraOut->Write(&count, sizeof(count));
	return 0;
}

RosterListElementParser::ListSizeAttrReader::ListSizeAttrReader()
	: m_dualWriter(NULL)
{
}

void RosterListElementParser::ListSizeAttrReader::SetWriter(DualSizeWriter *writer)
{
	m_dualWriter = writer;
	RosterAttrReader::SetWriter(writer);
}

int RosterListElementParser::ListSizeAttrReader::Parse(const BxceAttributeList *attrs)
{
	BxceAttr *attr = ScebXCe_2AD8DA26(attrs, "size", static_cast<int>(sce_paf_strlen("size")));
	if (attr == NULL || m_dualWriter == NULL) {
		m_error = ROSTER_PARSE_SYNTAX;
		return 0;
	}
	SceUInt32 count = static_cast<SceUInt32>(sce_paf_atol(attr->value));
	if (m_dualWriter->WriteSize(count) < 0)
		m_error = ROSTER_PARSE_SYNTAX;
	return 0;
}

/* 0x8130BB08 */
RosterListElementParser::RosterListElementParser()
	: m_expectedCount(0),
	  m_parsedCount(0),
	  m_itemParser(NULL)
{
	SetTagName("list");
	m_sizeWriter.SetTarget(&m_expectedCount);
	m_sizeReader.SetWriter(&m_sizeWriter);
	AddRequiredAttr(&m_sizeReader);
}

/* 0x8130BC58 */
void RosterListElementParser::SetItemParser(RosterElementParser *itemParser)
{
	m_itemParser = itemParser;
}

void RosterListElementParser::SetSizeWriterDelegate(RosterValueWriter *delegate)
{
	m_sizeWriter.SetMirrorOutput(delegate);
}

void RosterListElementParser::Reset()
{
	RosterElementParser::Reset();
	m_expectedCount = 0;
	m_parsedCount = 0;
	if (m_itemParser != NULL)
		m_itemParser->Reset();
}

int RosterListElementParser::ClearAll()
{
	Reset();
	m_tagName = "";
	m_writer = NULL;
	m_expectedCount = 0;
	m_parsedCount = 0;
	m_itemParser = NULL;
	return 0;
}

void RosterListElementParser::OnChildStart(const char *name, const BxceAttributeList *attrs)
{
	if (m_error != ROSTER_PARSE_OK)
		return;
	if (m_itemParser == NULL || m_parsedCount == m_expectedCount) {
		m_error = ROSTER_PARSE_SYNTAX;
		return;
	}
	m_itemParser->HandleStartElement(name, attrs);
	if (m_itemParser->GetError() != ROSTER_PARSE_OK)
		m_error = m_itemParser->GetError();
	else
		m_childComplete = 0;
}

void RosterListElementParser::OnChildEnd(const char *name)
{
	if (m_error != ROSTER_PARSE_OK)
		return;
	if (m_itemParser == NULL || m_parsedCount == m_expectedCount) {
		m_error = ROSTER_PARSE_SYNTAX;
		return;
	}
	m_itemParser->HandleEndElement(name);
	if (m_itemParser->GetError() != ROSTER_PARSE_OK) {
		m_error = m_itemParser->GetError();
		return;
	}
	++m_parsedCount;
	if (m_writer != NULL) {
		RosterValueWriter *itemWriter = m_itemParser->GetWriter();
		const void *itemData = (itemWriter != NULL) ? itemWriter->GetBuffer() : NULL;
		SceSize itemLen = (itemWriter != NULL) ? itemWriter->GetWritten() : 0;
		if (m_writer->Write(itemData, itemLen) < 0)
			m_error = ROSTER_PARSE_SYNTAX;
	}
	m_itemParser->Reset();
	if (m_parsedCount == m_expectedCount)
		m_childComplete = 1;
}

/* --------------------------------------------------------------------------
 * Concrete SAX response handlers (BxceDocumentHandler implementations)
 * -------------------------------------------------------------------------- */
class ResultSaxHandlerBase : public BxceDocumentHandler {
public:
	ResultSaxHandlerBase();
	void BindResult(int *outResult);
	virtual int startDocument(void *userData);

protected:
	RosterElementParser m_rootElement;
	RosterValueWriter m_internalResultWriter;
	ResultAttrReader m_resultReader;
	RosterValueWriter m_externalResultWriter;
};

ResultSaxHandlerBase::ResultSaxHandlerBase()
{
}

/* 0x8130B1BA */
void ResultSaxHandlerBase::BindResult(int *outResult)
{
	m_externalResultWriter.Bind(outResult, sizeof(int));
}

int ResultSaxHandlerBase::startDocument(void *userData)
{
	(void)userData;
	m_internalResultWriter.SetDelegate(&m_externalResultWriter);
	return 0;
}

/* 0x8130B14A, 0x8130B1F4, 0x8130B230: accept_friend response handler */
class AcceptFriendSaxHandler : public ResultSaxHandlerBase {
public:
	AcceptFriendSaxHandler();
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
};

/* 0x8130B14A */
AcceptFriendSaxHandler::AcceptFriendSaxHandler()
{
	m_rootElement.SetTagName("friendlist");
	m_resultReader.SetWriter(&m_internalResultWriter);
	m_rootElement.AddRequiredAttr(&m_resultReader);
}

/* 0x8130B1F4 */
int AcceptFriendSaxHandler::startElement(void *userData, const char *name, int length,
					 const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleStartElement(name, attrList);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130B230 */
int AcceptFriendSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleEndElement(name);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130BD86, 0x8130BDF8, 0x8130BE34: delete_friend response handler */
class DeleteFriendSaxHandler : public ResultSaxHandlerBase {
public:
	DeleteFriendSaxHandler();
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
};

/* 0x8130BD86 */
DeleteFriendSaxHandler::DeleteFriendSaxHandler()
{
	m_rootElement.SetTagName("friendlist");
	m_resultReader.SetWriter(&m_internalResultWriter);
	m_rootElement.AddRequiredAttr(&m_resultReader);
}

/* 0x8130BDF8 */
int DeleteFriendSaxHandler::startElement(void *userData, const char *name, int length,
					 const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleStartElement(name, attrList);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130BE34 */
int DeleteFriendSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleEndElement(name);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130C50A, 0x8130C57C, 0x8130C5B8: request_friend response handler */
class RequestFriendSaxHandler : public ResultSaxHandlerBase {
public:
	RequestFriendSaxHandler();
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
};

/* 0x8130C50A */
RequestFriendSaxHandler::RequestFriendSaxHandler()
{
	m_rootElement.SetTagName("friendlist");
	m_resultReader.SetWriter(&m_internalResultWriter);
	m_rootElement.AddRequiredAttr(&m_resultReader);
}

/* 0x8130C57C */
int RequestFriendSaxHandler::startElement(void *userData, const char *name, int length,
					  const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleStartElement(name, attrList);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130C5B8 */
int RequestFriendSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleEndElement(name);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130C5F2, 0x8130C664, 0x8130C6A0: update_block_list response handler */
class UpdateBlockListSaxHandler : public ResultSaxHandlerBase {
public:
	UpdateBlockListSaxHandler();
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
};

/* 0x8130C5F2 */
UpdateBlockListSaxHandler::UpdateBlockListSaxHandler()
{
	m_rootElement.SetTagName("blocklist");
	m_resultReader.SetWriter(&m_internalResultWriter);
	m_rootElement.AddRequiredAttr(&m_resultReader);
}

/* 0x8130C664 */
int UpdateBlockListSaxHandler::startElement(void *userData, const char *name, int length,
					    const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleStartElement(name, attrList);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130C6A0 */
int UpdateBlockListSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_rootElement.HandleEndElement(name);
	return (m_rootElement.GetError() != ROSTER_PARSE_OK)
		       ? NP_ROSTER_ERROR_INVALID_RESPONSE
		       : 0;
}

/* 0x8130BE6C..0x8130C107: <blocklist result="..."><list size="..." total="..."><item jid="..."/>... */
class BlockListRootParser : public RosterElementParser {
public:
	BlockListRootParser();
	void BindOutputs(RosterValueWriter *resultOut, RosterValueWriter *entriesOut,
			 RosterValueWriter *countOut, RosterValueWriter *totalOut);
	virtual void OnChildStart(const char *name, const BxceAttributeList *attrs);
	virtual void OnChildEnd(const char *name);

private:
	RosterListElementParser m_listParser;
	SceNpId m_curNpId;
	RosterValueWriter m_resultWriter;
	RosterValueWriter m_totalWriter;
	RosterValueWriter m_itemWriter;
	RosterValueWriter m_jidWriter;
	RosterValueWriter m_listOutputWriter;
	ResultAttrReader m_resultReader;
	TotalAttrReader m_totalReader;
	RosterItemElementParser m_itemParser;
	JidAttrReader m_jidReader;
};

/* 0x8130BE6C */
BlockListRootParser::BlockListRootParser()
{
	SetTagName("blocklist");
	sce_paf_memset(&m_curNpId, 0, sizeof(m_curNpId));

	m_itemParser.AddRequiredAttr(&m_jidReader);
	m_listParser.SetItemParser(&m_itemParser);
	m_listParser.AddRequiredAttr(&m_totalReader);

	m_resultReader.SetWriter(&m_resultWriter);
	m_totalReader.SetWriter(&m_totalWriter);
	m_itemParser.SetWriter(&m_itemWriter);
	m_jidReader.SetWriter(&m_jidWriter);
	m_listParser.SetWriter(&m_listOutputWriter);

	m_itemWriter.Bind(&m_curNpId, sizeof(m_curNpId));
	m_jidWriter.Bind(&m_curNpId, sizeof(m_curNpId));
	AddRequiredAttr(&m_resultReader);
}

void BlockListRootParser::BindOutputs(RosterValueWriter *resultOut, RosterValueWriter *entriesOut,
				      RosterValueWriter *countOut, RosterValueWriter *totalOut)
{
	m_resultWriter.SetDelegate(resultOut);
	m_listOutputWriter.SetDelegate(entriesOut);
	m_listParser.SetSizeWriterDelegate(countOut);
	m_totalWriter.SetDelegate(totalOut);
}

void BlockListRootParser::OnChildStart(const char *name, const BxceAttributeList *attrs)
{
	if (sce_paf_strcmp(name, "item") == 0) {
		sce_paf_memset(&m_curNpId, 0, sizeof(m_curNpId));
		m_itemWriter.Write(&m_curNpId, sizeof(m_curNpId));
	}
	m_childComplete = 0;
	m_listParser.HandleStartElement(name, attrs);
	if (m_listParser.GetError() != ROSTER_PARSE_OK)
		m_error = m_listParser.GetError();
}

void BlockListRootParser::OnChildEnd(const char *name)
{
	m_listParser.HandleEndElement(name);
	if (m_listParser.GetError() != ROSTER_PARSE_OK)
		m_error = m_listParser.GetError();
	if (sce_paf_strcmp(name, m_listParser.GetTagName()) == 0)
		m_childComplete = 1;
}

class GetBlockListSaxHandler : public BxceDocumentHandler {
public:
	GetBlockListSaxHandler();
	void BindResult(int *outResult);
	void BindEntries(SceNpId *outEntries, SceUInt32 maxCount);
	void BindCount(SceUInt32 *outCount);
	void BindTotal(SceUInt32 *outTotal);
	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);

private:
	BlockListRootParser m_root;
	RosterValueWriter m_resultOut;
	RosterValueWriter m_entriesOut;
	RosterValueWriter m_countOut;
	RosterValueWriter m_totalOut;
};

GetBlockListSaxHandler::GetBlockListSaxHandler()
{
}

/* 0x8130BFE0 */
void GetBlockListSaxHandler::BindResult(int *outResult)
{
	m_resultOut.Bind(outResult, sizeof(int));
}

/* 0x8130BFEE */
void GetBlockListSaxHandler::BindEntries(SceNpId *outEntries, SceUInt32 maxCount)
{
	m_entriesOut.Bind(outEntries, sizeof(SceNpId) * maxCount);
}

/* 0x8130C000 */
void GetBlockListSaxHandler::BindCount(SceUInt32 *outCount)
{
	m_countOut.Bind(outCount, sizeof(SceUInt32));
}

/* 0x8130C00E */
void GetBlockListSaxHandler::BindTotal(SceUInt32 *outTotal)
{
	m_totalOut.Bind(outTotal, sizeof(SceUInt32));
}

int GetBlockListSaxHandler::startDocument(void *userData)
{
	(void)userData;
	m_root.BindOutputs(&m_resultOut, &m_entriesOut, &m_countOut, &m_totalOut);
	return 0;
}

int GetBlockListSaxHandler::startElement(void *userData, const char *name, int length,
					 const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_root.HandleStartElement(name, attrList);
	return (m_root.GetError() != ROSTER_PARSE_OK) ? NP_ROSTER_ERROR_INVALID_RESPONSE : 0;
}

int GetBlockListSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_root.HandleEndElement(name);
	return (m_root.GetError() != ROSTER_PARSE_OK) ? NP_ROSTER_ERROR_INVALID_RESPONSE : 0;
}

/* 0x8130C108..0x8130C4E7: <friendlist result="..."><list size="..." total="..."><item .../>... */
class FriendListRootParser : public RosterElementParser {
public:
	FriendListRootParser();
	void BindOutputs(RosterValueWriter *resultOut, RosterValueWriter *entriesOut,
			 RosterValueWriter *countOut, RosterValueWriter *totalOut);
	virtual void OnChildStart(const char *name, const BxceAttributeList *attrs);
	virtual void OnChildEnd(const char *name);

private:
	RosterListElementParser m_listParser;
	NpRosterFriendEntry m_curEntry;
	RosterValueWriter m_resultWriter;
	RosterValueWriter m_totalWriter;
	RosterValueWriter m_itemWriter;
	RosterValueWriter m_jidWriter;
	RosterValueWriter m_subscriptionWriter;
	RosterValueWriter m_updateDateWriter;
	RosterValueWriter m_msgFlagWriter;
	RosterValueWriter m_listOutputWriter;
	ResultAttrReader m_resultReader;
	TotalAttrReader m_totalReader;
	RosterItemElementParser m_itemParser;
	JidAttrReader m_jidReader;
	SubscriptionAttrReader m_subscriptionReader;
	UpdateDateAttrReader m_updateDateReader;
	MsgFlagAttrReader m_msgFlagReader;
};

/* 0x8130C108 */
FriendListRootParser::FriendListRootParser()
{
	SetTagName("friendlist");
	sce_paf_memset(&m_curEntry, 0, sizeof(m_curEntry));

	m_itemParser.AddRequiredAttr(&m_jidReader);
	m_itemParser.AddRequiredAttr(&m_subscriptionReader);
	m_itemParser.AddOptionalAttr(&m_updateDateReader);
	m_itemParser.AddOptionalAttr(&m_msgFlagReader);

	m_listParser.SetItemParser(&m_itemParser);
	m_listParser.AddRequiredAttr(&m_totalReader);

	m_resultReader.SetWriter(&m_resultWriter);
	m_totalReader.SetWriter(&m_totalWriter);
	m_itemParser.SetWriter(&m_itemWriter);
	m_jidReader.SetWriter(&m_jidWriter);
	m_subscriptionReader.SetWriter(&m_subscriptionWriter);
	m_updateDateReader.SetWriter(&m_updateDateWriter);
	m_msgFlagReader.SetWriter(&m_msgFlagWriter);
	m_listParser.SetWriter(&m_listOutputWriter);

	m_itemWriter.Bind(&m_curEntry, sizeof(m_curEntry));
	m_jidWriter.Bind(&m_curEntry.npId, sizeof(m_curEntry.npId));
	m_subscriptionWriter.Bind(&m_curEntry.subscription, sizeof(m_curEntry.subscription));
	m_updateDateWriter.Bind(&m_curEntry.updateDate, sizeof(m_curEntry.updateDate));
	m_msgFlagWriter.Bind(&m_curEntry.msgFlag, sizeof(m_curEntry.msgFlag));

	AddRequiredAttr(&m_resultReader);
}

void FriendListRootParser::BindOutputs(RosterValueWriter *resultOut, RosterValueWriter *entriesOut,
				       RosterValueWriter *countOut, RosterValueWriter *totalOut)
{
	m_resultWriter.SetDelegate(resultOut);
	m_listOutputWriter.SetDelegate(entriesOut);
	m_listParser.SetSizeWriterDelegate(countOut);
	m_totalWriter.SetDelegate(totalOut);
}

void FriendListRootParser::OnChildStart(const char *name, const BxceAttributeList *attrs)
{
	if (sce_paf_strcmp(name, "item") == 0) {
		sce_paf_memset(&m_curEntry, 0, sizeof(m_curEntry));
		m_itemWriter.Write(&m_curEntry, sizeof(m_curEntry));
	}
	m_childComplete = 0;
	m_listParser.HandleStartElement(name, attrs);
	if (m_listParser.GetError() != ROSTER_PARSE_OK)
		m_error = m_listParser.GetError();
}

void FriendListRootParser::OnChildEnd(const char *name)
{
	m_listParser.HandleEndElement(name);
	if (m_listParser.GetError() != ROSTER_PARSE_OK)
		m_error = m_listParser.GetError();
	if (sce_paf_strcmp(name, m_listParser.GetTagName()) == 0)
		m_childComplete = 1;
}

class GetFriendListSaxHandler : public BxceDocumentHandler {
public:
	GetFriendListSaxHandler();
	void BindResult(int *outResult);
	void BindEntries(NpRosterFriendEntry *outEntries, SceUInt32 maxCount);
	void BindCount(SceUInt32 *outCount);
	void BindTotal(SceUInt32 *outTotal);
	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);

private:
	FriendListRootParser m_root;
	RosterValueWriter m_resultOut;
	RosterValueWriter m_entriesOut;
	RosterValueWriter m_countOut;
	RosterValueWriter m_totalOut;
};

GetFriendListSaxHandler::GetFriendListSaxHandler()
{
}

/* 0x8130C33A */
void GetFriendListSaxHandler::BindResult(int *outResult)
{
	m_resultOut.Bind(outResult, sizeof(int));
}

/* 0x8130C348 */
void GetFriendListSaxHandler::BindEntries(NpRosterFriendEntry *outEntries, SceUInt32 maxCount)
{
	m_entriesOut.Bind(outEntries, sizeof(NpRosterFriendEntry) * maxCount);
}

/* 0x8130C35A */
void GetFriendListSaxHandler::BindCount(SceUInt32 *outCount)
{
	m_countOut.Bind(outCount, sizeof(SceUInt32));
}

/* 0x8130C368 */
void GetFriendListSaxHandler::BindTotal(SceUInt32 *outTotal)
{
	m_totalOut.Bind(outTotal, sizeof(SceUInt32));
}

int GetFriendListSaxHandler::startDocument(void *userData)
{
	(void)userData;
	m_root.BindOutputs(&m_resultOut, &m_entriesOut, &m_countOut, &m_totalOut);
	return 0;
}

int GetFriendListSaxHandler::startElement(void *userData, const char *name, int length,
					  const BxceAttributeList *attrList)
{
	(void)userData;
	(void)length;
	m_root.HandleStartElement(name, attrList);
	return (m_root.GetError() != ROSTER_PARSE_OK) ? NP_ROSTER_ERROR_INVALID_RESPONSE : 0;
}

int GetFriendListSaxHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	(void)length;
	m_root.HandleEndElement(name);
	return (m_root.GetError() != ROSTER_PARSE_OK) ? NP_ROSTER_ERROR_INVALID_RESPONSE : 0;
}

/* 0x8130B018..0x8130B149: global SAX session manager */
enum RosterSaxState {
	ROSTER_SAX_IDLE		= 0,
	ROSTER_SAX_READY	= 1,
	ROSTER_SAX_PARSING	= 2,
	ROSTER_SAX_DONE		= 3,
	ROSTER_SAX_ERROR	= 4,
	ROSTER_SAX_UNINIT	= 5
};

class RosterSaxSession {
public:
	RosterSaxSession();
	~RosterSaxSession();

	int Init();
	int Term();
	int AttachHandler(BxceDocumentHandler *handler);
	int ParseChunk(const void *chunk, SceSize chunkLen, SceUInt8 isFinal);
	void ResetParser();
	int EndParse();

private:
	BxceSaxParser *m_saxParser;
	BxceDocumentHandler *m_handler;
	int m_state;
};

RosterSaxSession::RosterSaxSession()
	: m_saxParser(NULL),
	  m_handler(NULL),
	  m_state(ROSTER_SAX_UNINIT)
{
}

/* 0x8130B024 */
RosterSaxSession::~RosterSaxSession()
{
	if (m_handler != NULL) {
		delete m_handler;
		m_handler = NULL;
	}
}

/* 0x8130B03C */
int RosterSaxSession::Init()
{
	if (m_state != ROSTER_SAX_UNINIT)
		return NP_ROSTER_ERROR_INTERNAL_STATE;

	m_saxParser = reinterpret_cast<BxceSaxParser *>(ScebXCe_B8DDE741(sizeof(BxceSaxParser)));
	if (m_saxParser == NULL)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;

	if (ScebXCe_C85586BD(m_saxParser) != 0) {
		ScebXCe_EC560647(m_saxParser);
		m_saxParser = NULL;
		return NP_ROSTER_ERROR_INTERNAL_FAIL;
	}

	m_state = ROSTER_SAX_IDLE;
	return 0;
}

/* 0x8130B092 */
int RosterSaxSession::Term()
{
	if (m_state == ROSTER_SAX_UNINIT)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (m_saxParser != NULL) {
		ScebXCe_928189AB(m_saxParser);
		ScebXCe_EC560647(m_saxParser);
		m_saxParser = NULL;
	}
	m_state = ROSTER_SAX_UNINIT;
	return 0;
}

int RosterSaxSession::AttachHandler(BxceDocumentHandler *handler)
{
	if (m_state != ROSTER_SAX_IDLE)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	m_saxParser->handler = handler;
	m_saxParser->characterOffsets = 0;
	m_handler = handler;
	m_state = ROSTER_SAX_READY;
	if (handler == NULL)
		return NP_ROSTER_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x8130B0C8 */
int RosterSaxSession::ParseChunk(const void *chunk, SceSize chunkLen, SceUInt8 isFinal)
{
	if (m_state == ROSTER_SAX_UNINIT)
		return NP_ROSTER_ERROR_INTERNAL_STATE;

	int ret = ScebXCe_4E9A7061(m_saxParser, reinterpret_cast<const char *>(chunk),
				   static_cast<int>(chunkLen), isFinal);
	if (ret == SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS || ret == 0) {
		m_state = (isFinal != 0) ? ROSTER_SAX_DONE : ROSTER_SAX_PARSING;
		return 0;
	}
	m_state = ROSTER_SAX_ERROR;
	return NP_ROSTER_ERROR_INVALID_RESPONSE;
}

/* 0x8130B0BE */
void RosterSaxSession::ResetParser()
{
	if (m_saxParser != NULL)
		ScebXCe_9E06C329(m_saxParser);
}

/* 0x8130B10E */
int RosterSaxSession::EndParse()
{
	if (m_state == ROSTER_SAX_UNINIT || m_state == ROSTER_SAX_IDLE)
		return NP_ROSTER_ERROR_INTERNAL_STATE;
	if (m_handler != NULL) {
		delete m_handler;
		m_handler = NULL;
	}
	if (m_saxParser != NULL) {
		m_saxParser->handler = NULL;
		m_saxParser->characterOffsets = 0;
		ScebXCe_9E06C329(m_saxParser);
	}
	m_state = ROSTER_SAX_IDLE;
	return 0;
}

static RosterSaxSession s_saxSession;

} /* anonymous namespace */

/* --------------------------------------------------------------------------
 * Public XML bridge APIs (0x81309ACC..0x8130A32D)
 * -------------------------------------------------------------------------- */
const char *npRosterGetEnvironment(void)
{
	return s_npEnvironment;
}

/* 0x81309AD6 */
int npRosterXmlInit(void)
{
	if (s_xmlInitialized == 1)
		return NP_ROSTER_ERROR_ALREADY_INITIALIZED;

	int ret = sceNpCommonGetNpEnviroment(s_npEnvironment);
	if (ret < 0)
		return ret;

	ret = s_saxSession.Init();
	if (ret < 0)
		return ret;

	SceNpCommon_0624AE44(s_fwVersionStr);
	s_xmlInitialized = 1;
	return 0;
}

/* 0x81309B40 */
void npRosterXmlTerm(void)
{
	s_saxSession.Term();
	s_xmlInitialized = 0;
}

/* 0x81309B60 -> 0x8130B00C */
int npRosterXmlFreeSerialized(char *xmlText)
{
	if (xmlText == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;
	RosterXmlDocument::FreeSerialized(xmlText);
	return 0;
}

/* 0x81309B76 */
int npRosterXmlParseChunk(const void *chunk, SceSize chunkLen, SceUInt8 isFinal)
{
	int ret = s_saxSession.ParseChunk(chunk, chunkLen, isFinal);
	if (ret < 0 && s_xmlInitialized != 0)
		s_saxSession.ResetParser();
	return ret;
}

/* 0x81309BB2 */
int npRosterXmlEndParse(void)
{
	return s_saxSession.EndParse();
}

/* 0x81309C7A */
int npRosterXmlBuildRequestFriend(const SceNpId *npId, const char *comment,
				  const char *ticketBase64, char **outXml, SceSize *outLen)
{
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildRequestFriend(npId, comment, ticketBase64, ticketLen,
				     s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x81309D3C */
int npRosterXmlBeginParseRequestFriend(int *outResult)
{
	RequestFriendSaxHandler *handler = new RequestFriendSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	return 0;
}

/* 0x81309DA8 */
int npRosterXmlBuildAcceptFriend(const SceNpId *npId, const char *ticketBase64,
				 char **outXml, SceSize *outLen)
{
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildAcceptFriend(npId, ticketBase64, ticketLen,
				    s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x81309E60 */
int npRosterXmlBeginParseAcceptFriend(int *outResult)
{
	AcceptFriendSaxHandler *handler = new AcceptFriendSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	return 0;
}

/* 0x81309ECC */
int npRosterXmlBuildDeleteFriend(const SceNpId *npId, const char *ticketBase64,
				 char **outXml, SceSize *outLen)
{
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildDeleteFriend(npId, ticketBase64, ticketLen,
				    s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x81309F84 */
int npRosterXmlBeginParseDeleteFriend(int *outResult)
{
	DeleteFriendSaxHandler *handler = new DeleteFriendSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	return 0;
}

/* 0x81309FF0 */
int npRosterXmlBuildGetBlockList(SceUInt32 startIndex, SceUInt32 maxCount,
				 const char *ticketBase64, char **outXml, SceSize *outLen)
{
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildGetBlockList(startIndex, maxCount, ticketBase64, ticketLen,
				    s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x8130A0B2 */
int npRosterXmlBeginParseGetBlockList(int *outResult, SceNpId *outEntries, SceUInt32 maxCount,
				      SceUInt32 *outCount, SceUInt32 *outTotal)
{
	GetBlockListSaxHandler *handler = new GetBlockListSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	handler->BindEntries(outEntries, maxCount);
	handler->BindCount(outCount);
	handler->BindTotal(outTotal);
	return 0;
}

/* 0x8130A146 */
int npRosterXmlBuildUpdateBlockList(const NpRosterBlockListEntry *entries, SceSize count,
				    const char *ticketBase64, char **outXml, SceSize *outLen)
{
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildUpdateBlockList(entries, count, ticketBase64, ticketLen,
				       s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x8130A208 */
int npRosterXmlBeginParseUpdateBlockList(int *outResult)
{
	UpdateBlockListSaxHandler *handler = new UpdateBlockListSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	return 0;
}

/* 0x8130A276 */
int npRosterXmlBuildGetFriendList(SceUInt32 maxCount, int optionFlags,
				  const char *ticketBase64, char **outXml, SceSize *outLen)
{
	(void)maxCount;
	RosterXmlDocument doc;
	if (outXml == NULL || ticketBase64 == NULL)
		return NP_ROSTER_ERROR_INTERNAL_ARG;

	int ret = doc.Begin();
	if (ret < 0)
		return ret;

	SceSize ticketLen = sce_paf_strlen(ticketBase64);
	ret = doc.BuildGetFriendList(optionFlags, ticketBase64, ticketLen,
				     s_npEnvironment, s_fwVersionStr);
	if (ret >= 0)
		ret = doc.Serialize(outXml, outLen);
	doc.End();
	return ret;
}

/* 0x81309BE6 */
int npRosterXmlBeginParseGetFriendList(int *outResult, NpRosterFriendEntry *outEntries,
				       SceUInt32 maxCount, SceUInt32 *outCount,
				       SceUInt32 *outTotal)
{
	GetFriendListSaxHandler *handler = new GetFriendListSaxHandler();
	int ret = s_saxSession.AttachHandler(handler);
	if (ret < 0)
		return ret;
	handler->BindResult(outResult);
	handler->BindEntries(outEntries, maxCount);
	handler->BindCount(outCount);
	handler->BindTotal(outTotal);
	return 0;
}
