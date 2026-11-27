/*
 * nptrophy_net.h: network transport, HTTP connection pool, AES-CBC encrypted
 * asset streaming, server status / XML RPC client, sync session batching,
 * SFM RSA+HMAC signature verification, and conf extraction sources
 * (0x812CC5F0..0x812CC680, 0x812D8114..0x812D8763, 0x812E1114..0x812E7867).
 */
#ifndef SHELL_NP_TROPHY_NET_H
#define SHELL_NP_TROPHY_NET_H

#include <stdarg.h>
#include <stdint.h>
#include <scetypes.h>
#include <libaes.h>
#include <libhttp.h>
#include <libsha1.h>
#include <libhmac.h>
#include <bXCe/bxce_internal.h>
#include "nptrophy_runtime.h"
#include "nptrophy_io.h"
#include "nptrophy_sfm.h"
#include "nptrophy_storage.h"

/* SceNpCommon streaming XML parser layout (matches sys/internal/include/np_common/np_common.h) */
struct TrophyNpXmlElement {
	const char *name;
	int hasText;
};

struct TrophyNpXmlSchema {
	const TrophyNpXmlElement *const *levels;
	SceUInt32 levelCount;
};

struct TrophyNpXmlParser;

typedef int (*TrophyNpXmlCallback)(struct TrophyNpXmlParser *parser, int event, SceUInt32 depth,
				   SceUInt32 index, const char *name, const char *value,
				   void *arg);

struct TrophyNpXmlParser {
	const TrophyNpXmlSchema *schema;
	char name[256];
	char value[256];
	char pending[6];
	SceSize pendingLength;
	SceSize pendingNeeded;
	SceUInt32 depth;
	SceUInt32 index[6];
	SceUInt32 state;
	SceUInt32 flags;
	SceSize length;
	int closed;
	TrophyNpXmlCallback callback;
	void *arg;
};

struct TrophyNpJidParts {
	const char *node;
	const char *domain;
	const char *resource;
	SceUInt32 reserved;
};

extern "C" {
int SceNpCommon_650D111B(TrophyNpXmlParser *parser, const TrophyNpXmlSchema *schema,
			 TrophyNpXmlCallback callback, void *arg);
int SceNpCommon_88B18A5B(TrophyNpXmlParser *parser, const char *data, int size);
int SceNpCommon_5BC54899(TrophyCommId *commId, const char *str);
int SceNpCommon_40CC4D15(TrophyNpJidParts *jid, const SceNpId *npId, char *buf, SceSize bufSize,
			 SceSize *required);
int SceNpCommon_85699AE2(const TrophyNpJidParts *jid, char *buf, SceSize bufSize,
			 SceSize *required);
SceSize sceNpCommonBase64Encode(const SceUInt8 *src, char *dst, SceSize size);
}

/* Additional SceNpTrophy network, XML, and crypto error codes */
#define NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN	-2141907289	/* 0x805516A7 */
#define NP_TROPHY_ERR_AES_ALREADY_OPEN		-2141907279	/* 0x805516B1 */
#define NP_TROPHY_ERR_AES_BAD_PADDING		-2141907278	/* 0x805516B2 */
#define NP_TROPHY_ERR_INVALID_PLATFORM		-2141907263	/* 0x805516C1 */
#define NP_TROPHY_ERR_HTTP_BAD_ARGUMENT		-2141907197	/* 0x80551703 */
#define NP_TROPHY_ERR_STATUS_INVALID		-2141907195	/* 0x80551705 */
#define NP_TROPHY_ERR_HTTP_NOT_OPEN		-2141907194	/* 0x80551706 */
#define NP_TROPHY_ERR_HTTP_ALREADY_OPEN		-2141907193	/* 0x80551707 */
#define NP_TROPHY_ERR_HTTP_STATUS_ERROR		-2141907192	/* 0x80551708 */
#define NP_TROPHY_ERR_HTTP_ICON_NOT_FOUND	-2141907190	/* 0x8055170A */
#define NP_TROPHY_ERR_XML_PARSE_ERROR		-2141907189	/* 0x8055170B */
#define NP_TROPHY_ERR_XML_BUILD_ERROR		-2141907188	/* 0x8055170C */
#define NP_TROPHY_ERR_RPC_HTTP_ERROR		-2141907180	/* 0x80551714 */
#define NP_TROPHY_ERR_SERVICE_MAINTENANCE	-2141907179	/* 0x80551715 */
#define NP_TROPHY_ERR_SERVICE_ENDED		-2141907178	/* 0x80551716 */
#define NP_TROPHY_ERR_SERVICE_NOT_STARTED	-2141907177	/* 0x80551717 */
#define NP_TROPHY_ERR_STATUS_SIG_INVALID	-2141907176	/* 0x80551718 */
#define NP_TROPHY_ERR_SERVER_NO_RESULTS		-2141906873	/* 0x80551847 */

#define TROPHY_HTTP_ERR_NO_CONTENT_LENGTH	-2143088527	/* 0x80431071 */

/* HTTP timeout configuration (connect, recv, send in microseconds) */
struct TrophyHttpTimeouts {
	uint32_t connectTimeoutUsec;
	uint32_t recvTimeoutUsec;
	uint32_t sendTimeoutUsec;
};

/* HTTP template RAII wrapper (0x812E6BC4..0x812E6C5A) */
class TrophyHttpTemplate : public TrophyHeapObject {
public:
	TrophyHttpTemplate();
	virtual ~TrophyHttpTemplate();

	int Init(const char *userAgent, int httpVer, int autoProxyConf);
	int Term();
	int SetAuthCallback(SceHttpAuthInfoCallback callback, void *userArg);
	int GetTemplateId() const { return m_tmplId; }

private:
	int m_tmplId;
};

/* Pooled HTTP keep-alive connection entry (0x812E74AA, 0x812E77F2) */
class TrophyHttpConnPoolEntry : public TrophyHeapObject {
public:
	TrophyHttpConnPoolEntry();
	virtual ~TrophyHttpConnPoolEntry();

	int Init(int tmplId, const char *scheme, const char *serverName, uint16_t port);

	int m_connId;
	int m_tmplId;
	char *m_scheme;
	char *m_serverName;
	uint16_t m_port;
	int m_inUse;
	uint8_t m_evictOnRelease;
	int m_lastError;
	TrophyHttpConnPoolEntry *m_next;
	TrophyHttpConnPoolEntry *m_prev;
};

/* Thread-safe HTTP connection pool (0x812E7586..0x812E77B0) */
class TrophyHttpConnPool : public TrophyHeapObject {
public:
	TrophyHttpConnPool();
	virtual ~TrophyHttpConnPool();

	int Init(uint32_t maxConnections);
	void Term();
	int Acquire(TrophyHttpTemplate *httpTemplate, const char *scheme, const char *serverName,
		    uint16_t port, int *outConnId);
	int Release(int connId);
	int ReportError(int connId, int errorCode);

private:
	uint32_t m_maxConnections;
	uint32_t m_activeCount;
	TrophyHttpConnPoolEntry *m_head;
	TrophyHttpConnPoolEntry *m_tail;
	TrophyLwMutex m_mutex;
};

/* Cancellable HTTP request wrapper (0x812E6E18..0x812E736E) */
class TrophyHttpRequest : public TrophyHeapObject {
public:
	TrophyHttpRequest();
	virtual ~TrophyHttpRequest();

	bool IsOpen() const { return m_reqId != 0; }
	int OpenPooled(TrophyHttpTemplate *httpTemplate, TrophyHttpConnPool *pool, int method,
		       const char *scheme, const char *serverName, uint16_t port,
		       const char *path, uint64_t contentLength);
	int Open(TrophyHttpTemplate *httpTemplate, int method, const char *scheme,
		 const char *serverName, uint16_t port, const char *path, uint64_t contentLength);
	int SetTimeouts(const TrophyHttpTimeouts *timeouts);
	int Close();
	int SetContentLength(uint64_t contentLength);
	int Send(TrophyHandle *handle, const void *postBody, SceSize bodySize);
	int GetStatusCode(TrophyHandle *handle, int *outStatusCode);
	int GetResponseContentLength(TrophyHandle *handle, uint64_t *outContentLength);
	int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

private:
	TrophyHttpConnPool *m_pool;
	int m_connId;
	int m_reqId;
	uint64_t m_bytesRead;
};

/* HTTP response input stream with single-byte chunked probe (0x812E6C6C..0x812E6DD2) */
class TrophyHttpStream : public TrophyInputStream {
public:
	TrophyHttpStream();
	virtual ~TrophyHttpStream() {}

	int Open(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate, TrophyHttpConnPool *pool,
		 const char *scheme, const char *serverName, uint16_t port, const char *path,
		 const TrophyHttpTimeouts *timeouts);
	bool IsOpen() const { return m_request.IsOpen(); }
	int Close();
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

private:
	TrophyHttpRequest m_request;
	uint8_t m_peekByte;
	uint32_t m_hasPeekByte;
};

/* Streaming AES-128-CBC decryptor for signed CDN trophy assets (0x812E2774..0x812E2C60) */
class TrophyAesCbcDecryptStream : public TrophyHeapObject, public TrophyInputStream {
public:
	TrophyAesCbcDecryptStream();
	virtual ~TrophyAesCbcDecryptStream();

	int Open(const char *keyVersion, const TrophyCommId *commId, TrophyInputStream *source);
	int Close();
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

private:
	int InitCipher(const char *keyVersion, const TrophyCommId *commId);
	int DecryptBlocks(uint8_t *outBuf, SceSize maxBytes, SceSize *outDecrypted);

	TrophyReadBuffer m_readBuf;
	SceAesContext m_aesCtx;
	uint8_t m_cbcChain[16];
	uint8_t m_ivLoaded;
	uint8_t *m_derivedKey;
	uint64_t m_totalDecrypted;
	TrophyInputStream *m_source;
};

/* Base stream for fetching trophy assets (icons / SFM) from PSN CDN (0x812E2360..0x812E26A0) */
class TrophyNetAssetStream : public TrophyHeapObject, public TrophyInputStream {
public:
	TrophyNetAssetStream();
	virtual ~TrophyNetAssetStream();

	int OpenAsset(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
		      TrophyHttpConnPool *pool, const TrophyCommId *commId, const char *assetName,
		      const TrophyHttpTimeouts *timeouts, char *outUrlBuf,
		      const char *keyVersion);
	int OpenAssetDirect(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			    const TrophyCommId *commId, const char *assetName,
			    const TrophyHttpTimeouts *timeouts, char *outUrlBuf,
			    const char *keyVersion);
	bool IsOpen() const { return m_httpStream.IsOpen(); }
	int Close();
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);

protected:
	TrophyHttpStream m_httpStream;
	TrophyHandle *m_boundHandle;
	TrophyAesCbcDecryptStream *m_decryptStream;
	TrophyInputStream *m_activeStream;
};

/* CDN title icon fetcher with language fallback (0x812E2C70..0x812E2DFA) */
class TrophyNetIconStream : public TrophyNetAssetStream {
public:
	TrophyNetIconStream() {}
	virtual ~TrophyNetIconStream() {}

	int OpenTitleIcon(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			  TrophyHttpConnPool *pool, const TrophyCommId *commId, int lang,
			  int smallFlag, bool allowDefaultFallback, char *outUrlBuf);
};

/* CDN TROPCONF/TROP_XX.SFM fetcher with signature stripping (0x812E3080..0x812E328E) */
class TrophyNetConfStream : public TrophyNetAssetStream {
public:
	TrophyNetConfStream() {}
	virtual ~TrophyNetConfStream() {}

	int OpenConf(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
		     TrophyHttpConnPool *pool, const TrophyCommId *commId, int lang,
		     bool allowDefaultFallback, const TrophyHttpTimeouts *timeouts);
	int OpenConfDirect(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			   const TrophyCommId *commId, int lang, bool allowDefaultFallback,
			   const TrophyHttpTimeouts *timeouts);
	int CloseConf();
	TrophySfmSignatureStripper *GetStripper() { return &m_stripper; }

private:
	TrophySfmSignatureStripper m_stripper;
};

/* Cached server status endpoint & sync interval (0x812E3A44..0x812E3A80) */
struct TrophyStatusInfo {
	char *fqdn;
	uint64_t intervalSec;

	TrophyStatusInfo();
	~TrophyStatusInfo();
	void Clear();
	bool IsValid() const { return fqdn != NULL; }
	int Set(const char *hostFqdn, uint64_t interval);
};

/* Signed trophy-status.xml parser (0x812E3DE4..0x812E4016) */
struct TrophyStatusParseTarget {
	char *fqdnBuf;
	SceSize fqdnBufSize;
	uint32_t intervalSec;
};

struct TrophyStatusXmlParser {
	TrophyNpXmlParser xmlParser;
	int statusError;
	uint32_t parsedMask;
	TrophyStatusParseTarget *target;

	int Init(TrophyStatusParseTarget *outTarget);
	int VerifyHeaderSignature(char *rawXml, SceSize xmlLen, SceSize bufCap);
	int ParseXml(const char *xmlBody, int bodyLen);

	static int XmlCallback(TrophyNpXmlParser *parser, int event, SceUInt32 depth,
			       SceUInt32 index, const char *name, const char *value,
			       void *userArg);
};

/* Fetches and verifies trophy-status.xml (0x812E3ADC..0x812E3DB8) */
class TrophyStatusFetcher : public TrophyHeapObject {
public:
	TrophyStatusFetcher();
	virtual ~TrophyStatusFetcher();

	int Fetch(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
		  const TrophyHttpTimeouts *timeouts);
	int CopyStatusTo(TrophyStatusInfo *outInfo) const;

private:
	int ParseResponse(char *rawBody, SceSize bodyLen, SceSize bufCap);

	TrophyHttpRequest m_request;
	char *m_fqdnAlloc;
	TrophyStatusXmlParser m_parser;
	TrophyStatusParseTarget m_target;
};

/* Trophy server client managing status cache and HTTP auth (0x812E329C..0x812E359C) */
class TrophyNetClient : public TrophyHeapObject {
public:
	TrophyNetClient();
	virtual ~TrophyNetClient();

	int Init();
	void Term();
	int GetSyncInterval(TrophyHandle *handle, uint64_t *outIntervalSec,
			    const TrophyHttpTimeouts *timeouts);
	int OpenServerRequest(TrophyHandle *handle, TrophyHttpRequest *request, const char *path,
			      bool useHttps, const TrophyHttpTimeouts *timeouts);
	TrophyHttpTemplate *GetHttpTemplate() { return &m_httpTemplate; }

private:
	int EnsureStatusFresh(TrophyHandle *handle, const TrophyHttpTimeouts *timeouts);
	static SceInt32 HttpAuthCallback(SceInt32 request, SceHttpAuthType authType,
					 const char *realm, char *username, char *password,
					 SceBool needEntity, SceUChar8 **entityBody,
					 SceSize *entitySize, SceBool *save, void *userArg);

	TrophyLwMutex m_mutex;
	TrophyHttpTemplate m_httpTemplate;
	uint64_t m_statusFetchedTimeUsec;
	TrophyStatusInfo m_statusInfo;
};

/* Accumulates RFC-3339 date/time text across SAX chunks (0x812E4CD0..0x812E4DF6) */
class TrophyXmlDateAccumulator : public TrophyHeapObject {
public:
	explicit TrophyXmlDateAccumulator(bool requireEndElement);
	virtual ~TrophyXmlDateAccumulator();

	void Reset();
	int Append(const char *chars, SceSize length);
	void MarkComplete() { m_completed = 1; }
	int ParseTick(SceRtcTick *outUtc);

private:
	char *m_text;
	uint8_t m_requireComplete;
	uint8_t m_completed;
};

/* Base SAX response parser using bXCe (0x812E4E34..0x812E515C) */
class TrophyXmlResponseParserBase : public TrophyHeapObject, public BxceDocumentHandler {
public:
	TrophyXmlResponseParserBase();
	virtual ~TrophyXmlResponseParserBase();

	int InitParser();
	int FeedChunk(const char *chunk, int length, bool isFinal);

	virtual int OnStartElement(const char *name, int length,
				   const BxceAttributeList *attrs) = 0;
	virtual int OnEndElement(const char *name, int length) = 0;
	virtual int OnCharacters(const char *chars, int length) = 0;

protected:
	int CheckResultAttribute(const BxceAttributeList *attrs);

private:
	/* BxceDocumentHandler overrides */
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);

	BxceSaxParser *m_saxParser;
	int m_savedSlot;
	int m_callbackError;
};

/* bXCe attribute list & XML request document builders (0x812E5560..0x812E6106) */
class TrophyXmlAttrBuilder {
public:
	TrophyXmlAttrBuilder() : m_attrs(NULL) {}
	~TrophyXmlAttrBuilder();

	int Init();
	int AddAttr(const char *name, const char *value);
	BxceAttributeList *Get() const { return m_attrs; }

private:
	BxceAttributeList *m_attrs;
};

class TrophyXmlDocBuilder {
public:
	TrophyXmlDocBuilder() : m_doc(NULL) {}
	~TrophyXmlDocBuilder();

	int Init(const char *rootTag, const TrophyXmlAttrBuilder *rootAttrs,
		 BxceNodeId *outRootNode);
	void Term();
	int Serialize(char **outXml, SceSize *outLen) const;
	static void FreeSerialized(char *xml);

	int AppendElement(BxceNodeId parent, const char *tagName,
			  const TrophyXmlAttrBuilder *attrs, const char *text, SceSize textLen,
			  BxceNodeId *outNode);
	int AppendUintElement(BxceNodeId parent, const char *tagName,
			      const TrophyXmlAttrBuilder *attrs, uint32_t value,
			      BxceNodeId *outNode);
	int AppendNpIdJidElement(BxceNodeId parent, const char *tagName, const SceNpId *npId);
	int AppendTicketElement(BxceNodeId parent, const void *ticketBuf, SceSize ticketSize);
	int AppendTicketAndJid(BxceNodeId parent, const SceNpId *npId, const void *ticketBuf,
			       SceSize ticketSize);
	int AppendDateTimeElement(BxceNodeId parent, const char *tagName, const SceRtcTick *utc);
	int AppendPsIdElement(BxceNodeId parent);
	int AppendPagingElements(BxceNodeId parent, const uint32_t *paging);
	int AppendTitleInfoElement(BxceNodeId parent, const TrophyCommId *commId, int platform,
				   BxceNodeId *outInfoNode);
	int AppendSignElement(BxceNodeId parent, const void *signBuf);
	int AppendTrophyUnlockElement(BxceNodeId parent, int trophyId, int grade,
				      const SceRtcTick *unlockUtc);
	int AppendPlatformElement(BxceNodeId parent, int platform);
	int AppendPlatformMaskElements(BxceNodeId parent, uint32_t platformMask);

private:
	BxceDocument *m_doc;
};

/* Base class for authenticated XML POST RPCs (0x812E4978..0x812E4C08) */
class TrophyXmlRpcBase : public TrophyHeapObject {
public:
	TrophyXmlRpcBase();
	virtual ~TrophyXmlRpcBase();

	void SetTimeouts(const TrophyHttpTimeouts *timeouts) { m_timeouts = *timeouts; }

protected:
	int BeginRequestDoc(TrophyHandle *handle, const SceNpId *npId);
	int SendRequestDoc(TrophyHandle *handle, TrophyNetClient *netClient, const char *path,
			   bool useHttps);
	int ReceiveAndParseResponse(TrophyHandle *handle, TrophyXmlResponseParserBase *parser);

	TrophyHttpTimeouts m_timeouts;
	TrophyHttpRequest m_request;
	TrophyXmlDocBuilder m_docBuilder;
};

/* Modified title privacy entry (32B) & paginated fetcher (0x812E1B7C..0x812E1D5F, 0x812E363C..0x812E3A36) */
struct TrophyModifiedPrivacyEntry {
	TrophyCommId commId;
	uint32_t reserved;
	SceRtcTick lastUpdated;
	int platform;
};

class TrophyNetTitleListFetcher : public TrophyHeapObject {
public:
	TrophyNetTitleListFetcher();
	virtual ~TrophyNetTitleListFetcher();

	int Init(const SceNpId *npId, const SceRtcTick *sinceUtc);
	int FetchNextPage(TrophyHandle *handle, TrophyNetClient *netClient);
	int GetEntry(int index, TrophyModifiedPrivacyEntry *outEntry) const;
	uint32_t GetPageCount() const { return m_fetchedCount; }
	uint32_t GetTotalCount() const { return m_totalCount; }

private:
	SceNpId m_npId;
	SceRtcTick m_sinceUtc;
	uint32_t m_startIndex;
	uint32_t m_pageSize;
	uint32_t m_fetchedCount;
	uint32_t m_totalCount;
	TrophyModifiedPrivacyEntry *m_entries;
};

/* Trophy sync request & response wire payloads (0x812E45AA..0x812E4968) */
struct TrophySyncRequestUnlock {
	int trophyId;
	int grade;
	SceRtcTick unlockUtc;
};

class TrophySyncRequestPayload : public TrophyHeapObject {
public:
	TrophySyncRequestPayload();
	virtual ~TrophySyncRequestPayload();

	int Init(const TrophyCommId *commId, int maxUnlocks);
	int SetCommSign(const void *signBuf);
	int AddUnlock(int trophyId, int grade, const SceRtcTick *unlockUtc);
	int FilterByTrophyMask(const NpTrophyFlagArray *validMask);
	void Reset();

	const TrophyCommId &GetCommId() const { return m_commId; }
	const uint8_t *GetCommSign() const { return m_commSign; }
	uint32_t GetUnlockCount() const { return m_unlockCount; }
	const TrophySyncRequestUnlock *GetUnlocks() const { return m_unlocks; }

private:
	TrophyCommId m_commId;
	uint8_t *m_commSign;
	uint32_t m_unlockCount;
	TrophySyncRequestUnlock *m_unlocks;
	uint32_t m_unlockCapacity;
};

struct TrophySyncResultUnlock {
	int trophyId;
	int grade;
	SceRtcTick unlockUtc;
};

struct TrophySyncResultRarity {
	int trophyId;
	int rarityLevel;
	uint32_t earnRatePermille;
};

struct TrophySyncResultTitleMeta {
	TrophyCommId commId;
	uint32_t reserved;
	SceRtcTick lastUpdated;
	TrophyGradeCounts gradeCounts;
	uint32_t platformMask;
	int hidden;
};

class TrophySyncResultPayload : public TrophyHeapObject {
public:
	TrophySyncResultPayload();
	virtual ~TrophySyncResultPayload();

	int Init(uint32_t maxTrophies);
	int FindTrophyUnlock(int trophyId, uint64_t *outServerTick, int *outUnlocked) const;
	bool HasTrophyUnlock(int trophyId) const;
	void GetLastUpdated(uint64_t *outTick) const { *outTick = m_meta.lastUpdated.tick; }

	TrophySyncResultTitleMeta m_meta;
	uint32_t m_unlockCount;
	TrophySyncResultUnlock *m_unlocks;
	uint32_t m_rarityCount;
	TrophySyncResultRarity *m_rarities;
	uint32_t m_unlockCapacity;
	uint32_t m_rarityCapacity;
};

/* Sync session base and local-batch derived classes (0x812E1D60..0x812E2350, 0x812D8114..0x812D8763) */
class TrophySyncTitleRequestBase : public TrophyHeapObject {
public:
	TrophySyncTitleRequestBase();
	virtual ~TrophySyncTitleRequestBase();

	int InitPayload(const TrophyCommId *commId, int maxUnlocks);
	void ResetPayload();
	int AddUnlockPayload(int trophyId, int grade, const SceRtcTick *unlockUtc);
	int FilterByConfData(const TrophyConfData *confData);
	const TrophyCommId *GetCommId() const;
	int SetCommSign(const void *signBuf);
	TrophySyncRequestPayload *GetPayload() const { return m_payload; }

	TrophySyncTitleRequestBase *m_next;
	TrophySyncTitleRequestBase *m_prev;

protected:
	TrophySyncRequestPayload *m_payload;
};

class TrophySyncTitleRequest : public TrophySyncTitleRequestBase {
public:
	TrophySyncTitleRequest();
	virtual ~TrophySyncTitleRequest();

	int Init(const TrophyCommId *commId, uint32_t maxTransEntries);
	int GetTransEntry(uint32_t index, TrophyTransBatchEntry *outEntry) const;
	uint32_t GetTransCount() const { return m_transCount; }
	int AddRegisterTrans(const TrophyTransRegisterEntry *entry);
	int AddUnlockTrans(const TrophyTransUnlockEntry *entry);
	void Reset();

private:
	TrophyTransBatchEntry *m_transEntries;
	uint32_t m_transCapacity;
	uint32_t m_transCount;
};

class TrophySyncTitleResultBase : public TrophyHeapObject {
public:
	TrophySyncTitleResultBase();
	virtual ~TrophySyncTitleResultBase();

	int InitPayload(uint32_t maxTrophies);
	void DestroyPayload();
	bool HasTrophyUnlock(int trophyId) const;
	int FindTrophyUnlock(int trophyId, uint64_t *outServerTick, int *outUnlocked) const;
	void GetLastUpdated(uint64_t *outTick) const;
	TrophySyncResultPayload *GetPayload() const { return m_payload; }

	TrophySyncTitleResultBase *m_next;
	TrophySyncTitleResultBase *m_prev;
	TrophySyncTitleRequestBase *m_pairedRequest;

protected:
	TrophySyncResultPayload *m_payload;
};

class TrophySyncTitleResult : public TrophySyncTitleResultBase {
public:
	TrophySyncTitleResult() {}
	virtual ~TrophySyncTitleResult() {}

	uint32_t GetPairedTransCount() const;
	int GetPairedTransStatus(uint32_t index, TrophyTransBatchEntry *outEntry,
				 bool *outAccepted) const;
};

class TrophySyncSessionBase : public TrophyHeapObject {
public:
	TrophySyncSessionBase();
	virtual ~TrophySyncSessionBase();

	int AppendRequest(TrophySyncTitleRequestBase *request);
	void RemoveRequest(TrophySyncTitleRequestBase *request);
	void AppendResult(TrophySyncTitleResultBase *result);
	TrophySyncTitleResultBase *FindResult(const TrophyCommId *commId);
	void ClearResults();
	void ClearAll();
	int ExecuteSyncRpc(TrophyHandle *handle, TrophyNetClient *netClient, const SceNpId *npId);
	void SetTimeouts(const TrophyHttpTimeouts *timeouts) { m_timeouts = *timeouts; }

protected:
	TrophySyncTitleRequestBase *m_reqHead;
	TrophySyncTitleRequestBase *m_reqTail;
	TrophySyncTitleResultBase *m_resHead;
	TrophySyncTitleResultBase *m_resTail;
	uint32_t m_reqCount;
	uint32_t m_resCount;
	TrophyHttpTimeouts m_timeouts;
};

class TrophyNetSyncBatch : public TrophySyncSessionBase {
public:
	TrophyNetSyncBatch() {}
	virtual ~TrophyNetSyncBatch() {}

	int AddTitle(const TrophyCommId *commId, uint32_t maxTransEntries,
		     TrophySyncTitleRequestBase **outRequest);
	int ExecuteWithFilter(TrophyHandle *handle, TrophyNetClient *netClient,
			      const SceNpId *npId);

private:
	int FetchServerConf(TrophyHandle *handle, TrophyHttpTemplate *httpTemplate,
			    const TrophyCommId *commId, TrophyConfData **outConfData);
	int ValidateTitleForSync(TrophyHandle *handle, TrophyNetClient *netClient,
				 TrophySyncTitleRequestBase *request, bool *outShouldSync);
};

/* SFM RSA-1024 + HMAC-SHA1 signature verifier (0x812CC5F0..0x812CC680, 0x812E189C..0x812E1A4F) */
struct TrophySfmVerifyKeyEntry {
	uint32_t keyId;
	uint8_t rsaModulus[128];
	uint8_t rsaExponent[128];
	uint8_t hmacKey[64];
};

struct TrophySignatureVerifierCore {
	SceSha1Context sha1Ctx;
	SceHmacSha1Context hmacCtx;

	static int ParseHeader(const TrophySfmSignature *sig, uint32_t *outMagic,
			       uint32_t *outKeyId);
	int Init(const TrophySfmVerifyKeyEntry *keyEntry);
	int Update(const void *bytes, SceSize size);
	int Verify(const TrophySfmVerifyKeyEntry *keyEntry, const TrophySfmSignature *sig);
};

class TrophySfmSignatureVerifier : public TrophyHeapObject, public TrophyHashSink {
public:
	TrophySfmSignatureVerifier();
	virtual ~TrophySfmSignatureVerifier() {}

	int Init(const TrophySfmSignature *sig);
	virtual int Update(const void *bytes, SceSize size);
	int Verify(const TrophySfmSignature *sig);

private:
	TrophySignatureVerifierCore m_core;
	const TrophySfmVerifyKeyEntry *m_keyEntry;
};

/* Parsed TROPCONF/TROP_XX handle (0x812E1A50..0x812E1AF7) */
class TrophyConfHandle : public TrophyHeapObject {
public:
	TrophyConfHandle();
	virtual ~TrophyConfHandle();

	int Assign(TrophyConfData **ioConfData, SceSize strippedHeaderSize, int lang);
	TrophyConfData *DetachConfData();
	const TrophyConfData *GetConfData() const { return m_confData; }
	SceSize GetStrippedHeaderSize() const { return m_strippedHeaderSize; }
	int GetLang() const { return m_lang; }

private:
	TrophyConfData *m_confData;
	SceSize m_strippedHeaderSize;
	int m_lang;
};

/* Abstract trophy conf/icon source and PSN CDN implementation (0x812E1114..0x812E188C) */
class TrophyConfSourceStream : public TrophyHeapObject, public TrophyInputStream {
public:
	virtual ~TrophyConfSourceStream() {}
	virtual int CloseAndRelease() = 0;
};

struct TrophyConfOriginInfo {
	int sourceKind;
	uint32_t reserved;
	TrophySha1Digest digest;

	void Clear()
	{
		sourceKind = 0;
		reserved = 0;
		digest.Clear();
	}
	bool Equals(const TrophyConfOriginInfo &other) const
	{
		return sourceKind == other.sourceKind && reserved == other.reserved &&
		       digest.Equals(other.digest);
	}
};

class TrophyConfSourceBase : public TrophyConfSource {
public:
	explicit TrophyConfSourceBase(int sourceKind);
	virtual ~TrophyConfSourceBase() {}

	virtual int OpenOwnedAssetStream(TrophyHandle *handle, const char *assetName,
					 TrophyConfSourceStream **outStream) = 0;

	virtual int OpenAssetStream(TrophyHandle *handle, const char *filename,
				    TrophyInputStream **outStream);
	virtual int ExtractAssets(TrophyHandle *handle, const TrophyExtractOptions *options,
				  TrophyConfMeta *outMeta, TrophyExtractedAssetReport *outReport,
				  TrophyConfData **outDefaultConf,
				  TrophyConfHandle **outLocalizedConf);

	void SetCommId(const TrophyCommId *commId) { m_commId = *commId; }
	const TrophyCommId &GetCommId() const { return m_commId; }
	int GetSourceKind() const { return m_sourceKind; }

private:
	static int ExtractOneAsset(TrophyConfHandle **ioHandle, TrophyHandle *handle,
				   TrophyConfSourceBase *source, const char *assetName,
				   const TrophyExtractOptions *options,
				   TrophyAssetDesc *outDesc);

	TrophyCommId m_commId;
	int m_sourceKind;
};

class TrophyNetConfSource : public TrophyConfSourceBase {
public:
	TrophyNetConfSource();
	virtual ~TrophyNetConfSource() {}

	int Open(TrophyHandle *handle, TrophyNetClient *netClient, const TrophyCommId *commId);
	virtual int IsOpen() const { return (m_netClient != NULL) ? 1 : 0; }
	virtual int Close();
	virtual int EstimateSpace(TrophyHandle *handle, const TrophyExtractOptions *options,
				  uint64_t *outUr0Bytes, uint64_t *outUx0Bytes);
	virtual int OpenOwnedAssetStream(TrophyHandle *handle, const char *assetName,
					 TrophyConfSourceStream **outStream);

private:
	TrophyNetClient *m_netClient;
};

/* Network utility functions (0x812E26B2, 0x812E2704, 0x812E2E08, 0x812E738A, 0x812E73C8) */
bool npTrophyNetIsOnline();
int npTrophyFormatUrl(char *outUrl, const char *scheme, const char *host, int partCount, ...);
int npTrophyFormatTrophyHost(char *outHost, const char *npEnv);
int npTrophyFormatSignedAssetPath(char *outPath, const char *keyVersion, const char *npEnv,
				  const TrophyCommId *commId, const char *assetName,
				  const char *encPrefix);
int npTrophyAppendSignedAssetSuffix(char *pathBuf, SceSize pathSize, const char *keyVersion,
				    const TrophyCommId *commId, const char *assetName,
				    const char *encPrefix);

/* RSA-1024 PKCS#1 (1.5) SHA-1 verification bridge (0x81377996) */
struct TrophyRsaSlice {
	const uint8_t *bytes;
	SceSize length;
};

struct TrophyRsaKeyPair {
	const uint8_t *modulus;
	const uint8_t *exponent;
};

extern "C" int GcAuthRsaVerify1024(const TrophyRsaSlice *signature, const TrophyRsaSlice *digest,
				   const TrophyRsaKeyPair *publicKey, int hashType);

#endif /* SHELL_NP_TROPHY_NET_H */
