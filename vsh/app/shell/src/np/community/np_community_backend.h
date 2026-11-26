/*
 * np_community_backend.h: shared internal declarations for the NP Community HTTP/XML
 * backend (client/transaction registry, URL builder, profile/avatar/panel API, and
 * XML/HTTP/HMAC helpers: 0x8132A068..0x813304C3 and 0x813F5194..0x813F85B3).
 */
#ifndef SHELL_NP_COMMUNITY_BACKEND_H
#define SHELL_NP_COMMUNITY_BACKEND_H

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include "np_community_ipc.h"
#include "../ipc/np_ipc_internal.h"

/* NP Community backend error codes (0x80550701..0x80550715, 0x80550800..0x805508FF) */
#define NP_COMMUNITY_ERR_ALREADY_INIT		-2141911295	/* 0x80550701 */
#define NP_COMMUNITY_ERR_NOT_INIT		-2141911294	/* 0x80550702 */
#define NP_COMMUNITY_ERR_NO_MEMORY		-2141911293	/* 0x80550703 */
#define NP_COMMUNITY_ERR_INVALID_ARG		-2141911292	/* 0x80550704 */
#define NP_COMMUNITY_ERR_NO_COUNTRY		-2141911291	/* 0x80550705 */
#define NP_COMMUNITY_ERR_ID_EXHAUSTED		-2141911290	/* 0x80550706 */
#define NP_COMMUNITY_ERR_ABORTED		-2141911289	/* 0x80550707 */
#define NP_COMMUNITY_ERR_PARSE_FAIL		-2141911288	/* 0x80550708 */
#define NP_COMMUNITY_ERR_RESPONSE_TOO_LARGE	-2141911287	/* 0x80550709 */
#define NP_COMMUNITY_ERR_HTTP_STATUS		-2141911286	/* 0x8055070A */
#define NP_COMMUNITY_ERR_VERIFY_FAIL		-2141911285	/* 0x8055070B */
#define NP_COMMUNITY_ERR_NULL_POINTER		-2141911284	/* 0x8055070C */
#define NP_COMMUNITY_ERR_UNSUPPORTED_SIG	-2141911283	/* 0x8055070D */
#define NP_COMMUNITY_ERR_TX_NOT_FOUND		-2141911282	/* 0x8055070E */
#define NP_COMMUNITY_ERR_CLIENT_NOT_FOUND	-2141911281	/* 0x8055070F */
#define NP_COMMUNITY_ERR_INVALID_ENDPOINT	-2141911279	/* 0x80550711 */
#define NP_COMMUNITY_ERR_ALREADY_COMPLETE	-2141911278	/* 0x80550712 */
#define NP_COMMUNITY_ERR_UNEXPECTED_PARAM	-2141911276	/* 0x80550714 */
#define NP_COMMUNITY_ERR_CLIENT_EXISTS		-2141911275	/* 0x80550715 */
#define NP_COMMUNITY_ERR_NO_SERVER_FOUND	-2141911274	/* 0x80550716 */

#define NP_COMMUNITY_ERR_SERVER_BASE		-2141911040	/* 0x80550800 */
#define NP_COMMUNITY_ERR_PROFILE_NOT_REGISTERED	-2141911027	/* 0x8055080D */

/* Community server status error codes (0x805508A0..0x805508AC) */
#define NP_COMMUNITY_ERR_SCORE_BEFORE_SERVICE	-2141910880	/* 0x805508A0 */
#define NP_COMMUNITY_ERR_SCORE_END_SERVICE	-2141910879	/* 0x805508A1 */
#define NP_COMMUNITY_ERR_SCORE_MAINTENANCE	-2141910878	/* 0x805508A2 */
#define NP_COMMUNITY_ERR_TUS_BEFORE_SERVICE	-2141910877	/* 0x805508A3 */
#define NP_COMMUNITY_ERR_TUS_END_SERVICE	-2141910876	/* 0x805508A4 */
#define NP_COMMUNITY_ERR_TUS_MAINTENANCE	-2141910875	/* 0x805508A5 */
#define NP_COMMUNITY_ERR_SERVER_HTTP_STATUS	-2141910874	/* 0x805508A6 */
#define NP_COMMUNITY_ERR_OTHER_BEFORE_SERVICE	-2141910870	/* 0x805508AA */
#define NP_COMMUNITY_ERR_OTHER_END_SERVICE	-2141910869	/* 0x805508AB */
#define NP_COMMUNITY_ERR_OTHER_MAINTENANCE	-2141910868	/* 0x805508AC */

/* Maximum XML response buffer (96 KiB) */
#define NP_COMMUNITY_MAX_XML_SIZE		0x18000U

/* Client & transaction ID ranges */
#define NP_COMMUNITY_CLIENT_ID_SENTINEL		0x10000000
#define NP_COMMUNITY_CLIENT_ID_MAX		0x20000000
#define NP_COMMUNITY_TX_ID_SENTINEL		0x20000000
#define NP_COMMUNITY_TX_ID_START		0x20000001
#define NP_COMMUNITY_TX_ID_WRAP_LIMIT		0x2FFFFFFF

/* Transaction owner kinds (NpCommunityBackendTx::ownerKind) */
#define NP_COMMUNITY_TX_OWNER_PROFILE		1
#define NP_COMMUNITY_TX_OWNER_COMMUNITY		2

/* Community HTTP endpoint identifiers (passed to npCommunityBuildUrl / npCommunityHttpOpen*) */
enum NpCommunityEndpointId {
	NP_COMMUNITY_EP_GET_SELF_PROFILE		= 1,
	NP_COMMUNITY_EP_REGISTER_SELF_PROFILE		= 2,
	NP_COMMUNITY_EP_GET_PROFILE			= 3,
	NP_COMMUNITY_EP_SEARCH_JID_RES			= 4,
	NP_COMMUNITY_EP_SEARCH_JID			= 5,
	NP_COMMUNITY_EP_LOBBY_SERVER_LIST		= 6,
	NP_COMMUNITY_EP_TROPHY_STATUS			= 7,
	NP_COMMUNITY_EP_RANKING_SERVER_LIST		= 8,
	NP_COMMUNITY_EP_STORAGE_SERVER_LIST		= 9,
	NP_COMMUNITY_EP_RECORD_SCORE			= 10,
	NP_COMMUNITY_EP_RECORD_GAME_DATA		= 11,
	NP_COMMUNITY_EP_GET_GAME_DATA			= 12,
	NP_COMMUNITY_EP_GET_PLAYER_RANK			= 13,
	NP_COMMUNITY_EP_GET_RANKING			= 14,
	NP_COMMUNITY_EP_GET_BOARD_INFO			= 15,
	NP_COMMUNITY_EP_UPLOADER_UPLOAD			= 16,
	NP_COMMUNITY_EP_UPLOADER_DOWNLOAD		= 17,
	NP_COMMUNITY_EP_UPLOADER_LIST			= 18,
	NP_COMMUNITY_EP_UPLOADER_QUOTA			= 19,
	NP_COMMUNITY_EP_UPLOADER_DELETE			= 20,
	NP_COMMUNITY_EP_UPDATE_PTL			= 21,
	NP_COMMUNITY_EP_UPDATE_CI			= 22,
	NP_COMMUNITY_EP_UPDATE_MI			= 23,
	NP_COMMUNITY_EP_CENSOR_COMMENT			= 25,
	NP_COMMUNITY_EP_SANITIZE_COMMENT		= 26,
	NP_COMMUNITY_EP_TSS_GET_DATA			= 28,
	NP_COMMUNITY_EP_CLAN_GET_MEMBER_DATA		= 29,
	NP_COMMUNITY_EP_CLAN_GET_MEMBER_RANK_BY_ID	= 30,
	NP_COMMUNITY_EP_CLAN_GET_MEMBER_RANK_BY_RANGE	= 31,
	NP_COMMUNITY_EP_CLAN_GET_RANK_BY_ID		= 32,
	NP_COMMUNITY_EP_CLAN_GET_RANK_BY_RANGE		= 33,
	NP_COMMUNITY_EP_TUS_SET_DATA			= 34,
	NP_COMMUNITY_EP_TUS_GET_DATA			= 35,
	NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_DATA_STATUS	= 36,
	NP_COMMUNITY_EP_TUS_GET_MULTI_USER_DATA_STATUS	= 37,
	NP_COMMUNITY_EP_TUS_SET_MULTI_SLOT_VAR		= 38,
	NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_VAR		= 39,
	NP_COMMUNITY_EP_TUS_GET_MULTI_USER_VAR		= 40,
	NP_COMMUNITY_EP_TUS_TRY_AND_SET_VAR		= 41,
	NP_COMMUNITY_EP_TUS_ADD_AND_GET_VAR		= 42,
	NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_VAR	= 43,
	NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_DATA	= 44,
	NP_COMMUNITY_EP_GET_AVATAR_CATEGORY		= 45,
	NP_COMMUNITY_EP_GET_AVATAR_LIST			= 46,
	NP_COMMUNITY_EP_GET_PANEL_CATEGORY		= 47,
	NP_COMMUNITY_EP_GET_PANEL_LIST			= 48,
	NP_COMMUNITY_EP_UPDATE_SELF_PROFILE		= 49,
	NP_COMMUNITY_EP_BASIC_TTL			= 50,
	NP_COMMUNITY_EP_REPORT_SERVER_LIST		= 52,
	NP_COMMUNITY_EP_FRIENDLIST_STATUS		= 53,
	NP_COMMUNITY_EP_GET_AVATAR_CATEGORY_LIST	= 55,
	NP_COMMUNITY_EP_GET_AVAILABLE_AVATAR_LIST	= 56,
	NP_COMMUNITY_EP_UPDATE_SELF_AVATAR		= 57,
	NP_COMMUNITY_EP_GET_FRIEND_PLAYER_RANK		= 65,
	NP_COMMUNITY_EP_TUS_GET_FRIEND_VAR		= 66,
	NP_COMMUNITY_EP_TUS_GET_FRIEND_DATA_STATUS	= 67
};

/* XML signature verification modes (passed to npCommunityHttpRecvXmlEx) */
enum NpCommunityXmlSigMode {
	NP_COMMUNITY_SIG_NONE			= 1,
	NP_COMMUNITY_SIG_RANKING_HEADER		= 2,
	NP_COMMUNITY_SIG_COMMENT_RANKING_LIST	= 3,
	NP_COMMUNITY_SIG_COMMENT_STORAGE_LIST	= 4,
	NP_COMMUNITY_SIG_COMMENT_SEARCH_JID	= 5,
	NP_COMMUNITY_SIG_COMMENT_TROPHY_STATUS	= 6,
	NP_COMMUNITY_SIG_COMMENT_TTL		= 7,
	NP_COMMUNITY_SIG_COMMENT_FRIEND_STATUS	= 8,
	NP_COMMUNITY_SIG_COMMENT_EXTRA		= 9,
	NP_COMMUNITY_SIG_COMMENT_SKIP_HMAC	= 10
};

/* Chunked binary upload/download state attached to NpCommunityBackendTx::serviceParam (16 bytes) */
enum NpCommunityTransferState {
	NP_COMMUNITY_XFER_IDLE			= 0,
	NP_COMMUNITY_XFER_ACTIVE		= 1,
	NP_COMMUNITY_XFER_COMPLETE		= 2,
	NP_COMMUNITY_XFER_FAILED		= 3
};

struct NpCommunityChunkedTransfer {
	SceInt32 state;				/* +0x00 */
	SceSize totalSize;			/* +0x04 */
	SceSize remainingSize;			/* +0x08 */
	SceInt32 lastError;			/* +0x0C */
};

/* TSS URL parameter bundle passed to npCommunityBuildUrl for EP_TSS_GET_DATA */
struct NpCommunityTssUrlParam {
	const SceNpCommunicationId *commId;	/* +0x00 */
	SceInt32 slotId;			/* +0x04 */
};

/* Profile panel descriptor (144 bytes = 0x90) */
struct NpCommunityPanelInfo {
	SceUInt32 bgColor;			/* +0x00 */
	SceInt32 panelType;			/* +0x04 (2 = panelurl, 0 = ucbgp color) */
	SceInt32 panelId;			/* +0x08 */
	char url[128];				/* +0x0C */
	SceUInt32 tailReserved;			/* +0x8C */
};

/* Full profile structure returned by get_profile / get_self_profile / update_self_profile (456 bytes = 0x1C8) */
struct NpCommunityProfileData {
	SceNpId npId;				/* +0x000 (36 bytes) */
	SceNpOnlineName onlineName;		/* +0x024 (52 bytes) */
	char avatarUrl[128];			/* +0x058 */
	SceNpCountryCode countryCode;		/* +0x0D8 (4 bytes) */
	SceInt32 languages[4];			/* +0x0DC (16 bytes) */
	SceInt32 avatarId;			/* +0x0EC */
	char aboutMe[64];			/* +0x0F0 */
	NpCommunityPanelInfo panel;		/* +0x130 (144 bytes) */
	SceUInt32 plusIcon;			/* +0x1C0 */
	SceUInt32 tailReserved;			/* +0x1C4 */
};

/* Avatar / panel category list item (412 bytes = 0x19C) */
struct NpCommunityCategoryEntry {
	SceInt32 categoryId;			/* +0x000 */
	SceInt32 itemCount;			/* +0x004 */
	char name[385];				/* +0x008 */
	SceUInt8 countMissing;			/* +0x189 */
	SceUInt8 tailPad[18];			/* +0x18A */
};

/* Avatar / panel URL list slot (128 bytes) */
struct NpCommunityUrlString {
	char url[128];
};

/* bXCe opaque handles for XML attribute iteration, DOM serialization, and SAX/DOM parsing */
typedef SceUInt32 BxceAttrNodeId;

struct BxceSerializer {
	SceUInt8 opaque[8];
};

struct BxceDomParser {
	SceUInt8 parserHead[172];		/* +0x00 */
	SceInt32 parseStatus;			/* +0xAC */
	SceUInt8 parserMiddle[32];		/* +0xB0 */
	BxceDocument *document;			/* +0xD0 */
	SceUInt8 parserTail[16];		/* +0xD4..+0xE3 (228 bytes total) */
};

extern "C" {
/* Additional bXCe exports used by NP Community XML parser/serializer */
extern void *ScebXCe_B8DDE741(SceSize size);
extern int ScebXCe_8EA5A077(BxceSerializer *serializer);
extern int ScebXCe_D24E80F2(BxceSerializer *serializer);
extern int ScebXCe_D7FE96A2(BxceSerializer *serializer, BxceDocument *doc, char **outXml,
			    SceSize *outLen, int formatFlags);
extern int ScebXCe_11D1FC0D(BxceDomParser *parser);
extern int ScebXCe_8FA315F1(BxceDomParser *parser);
extern int ScebXCe_A40944DD(BxceDomParser *parser, int option);
extern int ScebXCe_BC0D9A6B(BxceDomParser *parser, const char *xmlText, SceSize xmlLen,
			    int isFinal);
extern BxceAttrNodeId ScebXCe_B034C595(const BxceDocument *doc, BxceNodeId node);
extern BxceAttrNodeId ScebXCe_134F491C(const BxceDocument *doc, BxceAttrNodeId attrNode);
extern const char *ScebXCe_B4989A58(const BxceDocument *doc, BxceAttrNodeId attrNode);
extern const char *ScebXCe_84383D8E(const BxceDocument *doc, BxceAttrNodeId attrNode);

/* Additional SceNpCommon exports */
extern SceInt64 SceNpCommon_E1FF4E70(const char *str);

/* Additional libhttp private/extended exports */
extern int sceHttpsEnableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(SceInt32 id, SceUInt32 sslFlags);
extern int sceHttpSetAcceptEncodingGZIPEnabled(SceInt32 id, SceBool enable);
}

/* Client / Transaction lifecycle & HTTP transport (np_community_client.cpp, 0x8132A068..0x8132DA11) */
int npCommunityHttpSend(NpCommunityBackendTx *tx, const void *body, SceSize size,
			SceSize *outSentSize);
int npCommunityInitBackend(void);
int npCommunityEnsureUser(const SceNpOnlineId *onlineId);
int npCommunitySetUserEnv(const SceNpOnlineId *onlineId, const char *envName,
			  const char *countryCode);
int npCommunityRemoveUser(const SceNpOnlineId *onlineId);
int npCommunityTrackTxAlloc(NpCommunityBackendTx *tx, void *allocPtr);
int npCommunityCloseTxHttp(NpCommunityBackendTx *tx);
int npCommunityBuildUrl(char **outUrl, SceInt32 *outMethod, NpCommunityBackendTx *tx,
			int endpointId, const void *endpointParam);
int npCommunityHttpOpenOnConn(NpCommunityBackendTx *tx, SceInt32 connId, int endpointId,
			      const void *endpointParam, SceSize contentLength);
int npCommunityHttpOpen(NpCommunityBackendTx *tx, int endpointId, const void *endpointParam,
			SceSize contentLength);

/* Profile / Avatar / Panel HTTP operations (np_community_profile.cpp, 0x8132DA12..0x813304C3) */
int npProfileFetchOwnProfile(int profileReqHandle, void *outProfile, int *outStatus,
			     int *outFlags, SceUInt64 *outTimestamp, void *outStamp,
			     const void *ticketBuf, SceSize ticketSize, int mode);

/* Server list & service status lookup (np_community_server_list.cpp, 0x8133820C..0x81338ADB) */
int npCommunityGetServerListEx(SceInt32 txId, NpCommunityServerNode **outList,
			       SceInt64 *outIntervals);

/* XML, HTTP response, formatting & HMAC helpers (np_community_xml.cpp, 0x813F5194..0x813F85B3) */
void npCommunityHmacSha1(const void *key, SceSize keyLen, const void *data, SceSize dataLen,
			 SceUInt8 *outDigest);
int npCommunityBase64EncodeAlloc(char **outBase64, const void *src, SceSize srcLen);
int npCommunityBase64DecodeAlloc(void **outBytes, const char *src, SceSize srcLen);
void npCommunityXmlAddPlatformAttrs(BxceAttributeList *attrs);
int npCommunityXmlAddIntAttr(BxceAttributeList *attrs, const char *name, SceInt32 value);
int npCommunityXmlAddInt64Attr(BxceAttributeList *attrs, const char *name, SceInt64 value);
int npCommunityXmlAddUint64Attr(BxceAttributeList *attrs, const char *name, SceUInt64 value);
int npCommunityXmlAppendCommId(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       const SceNpCommunicationId *commId);
int npCommunityXmlAppendCommIdVersion(BxceDocument *doc, BxceNodeId parent,
				      BxceAttributeList *attrs, const char *tagName,
				      const SceNpCommunicationId *commId, SceInt32 version);
int npCommunityXmlAppendInt(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			    const BxceAttributeList *attrs, SceInt32 value);
int npCommunityXmlAppendBase64(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       const void *src, SceSize srcLen);
int npCommunityXmlAppendAvatarElement(BxceDocument *doc, BxceNodeId parent,
				      BxceAttributeList *attrs, const char *tagName,
				      const void *ticketBuf, SceSize ticketSize,
				      const char *ticketTagName, const SceInt32 *categoryIdPtr,
				      const SceInt32 *avatarIdPtr, const SceInt32 *sizeTypePtr);
int npCommunityXmlAppendPanelElement(BxceDocument *doc, BxceNodeId parent,
				     BxceAttributeList *attrs, const char *tagName,
				     const SceInt32 *categoryIdPtr, const SceInt32 *panelIdPtr,
				     const SceInt32 *sizeTypePtr);
int npCommunityXmlAppendJid(BxceDocument *doc, BxceNodeId parent, const BxceAttributeList *attrs,
			    const char *tagName, const SceNpId *npId);
int npCommunityXmlAppendOpenPsId(BxceDocument *doc, BxceNodeId parent,
				 const BxceAttributeList *attrs, const char *tagName);
int npCommunityXmlAppendUint32(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       SceUInt32 value);
int npCommunityXmlAppendEscapedText(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				    const char *text);
int npCommunityXmlAppendLanguages(BxceDocument *doc, BxceNodeId parent, const SceInt32 *languages);
int npCommunityXmlAppendBoundedText(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				    const BxceAttributeList *attrs, const char *text,
				    SceSize maxLen);
int npCommunityXmlAppendRtcTick(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				const BxceAttributeList *attrs, const SceRtcTick *utcTick,
				int stripFracSeconds);
int npCommunityXmlAppendBase64Checked(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				      const void *src, SceSize srcLen);
int npCommunityXmlFindAttr(const char **outValue, const BxceDocument *doc, BxceNodeId node,
			   const char *attrName);
int npCommunityXmlCheckResult(const BxceDocument *doc, const char *rootTag);
int npCommunityHttpReadHeaders(NpCommunityBackendTx *tx, SceInt32 *outStatusCode,
			       SceUInt64 *outContentLength, SceUInt32 *outInfoLength,
			       SceUInt8 *outSignature, int sigMode);
int npCommunityHttpRecvXml(NpCommunityBackendTx *tx, BxceDocument **outDoc, int sigMode,
			   SceUInt64 *outRemainingBytes);
int npCommunityHttpRecvXmlEx(NpCommunityBackendTx *tx, BxceDocument **outDoc, int sigMode,
			     SceUInt64 *outRemainingBytes, const void *passphrase);
int npCommunityHttpExpectBytes(NpCommunityBackendTx *tx, const void *expected, SceSize size);
int npCommunityXmlParseNumberAttr(const BxceDocument *doc, BxceNodeId node, SceInt32 *outCount);
int npCommunityXmlParseIntAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
			       SceInt32 *outValue);
int npCommunityXmlParseInt64Attr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
				 SceInt64 *outValue);
int npCommunityXmlParseHexAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
			       SceUInt32 *outValue);
int npCommunityXmlParseChildUint(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				 SceUInt32 *outValue);
int npCommunityXmlParseDataFlag(const BxceDocument *doc, BxceNodeId parent, SceInt32 *outHasData);
int npCommunityXmlParseChildDate(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				 SceRtcTick *outTick);
int npCommunityXmlParseText(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
			    char *outBuf, SceSize maxLen);
int npCommunityXmlParseTusDataInfo(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				   NpTusDataInfo *outInfo);
int npCommunityXmlParseScoreGameInfo(const BxceDocument *doc, BxceNodeId parent,
				     NpScoreGameInfo *outInfo);
int npCommunityXmlCheckServerStatus(const BxceDocument *doc, BxceNodeId node, int serviceCategory);
int npCommunityXmlParseJidChild(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				SceNpId *outNpId);
int npCommunityXmlParseJidAndPcId(const BxceDocument *doc, BxceNodeId parent, SceNpId *outNpId,
				  SceInt32 *outPcId);
int npCommunityXmlParseOnlineName(const BxceDocument *doc, BxceNodeId parent, char *outOnlineName,
				  SceInt32 *outUpdFlag, SceRtcTick *outUpdDate);
int npCommunityXmlParseCountry(const BxceDocument *doc, BxceNodeId parent,
			       SceNpCountryCode *outCountry);
int npCommunityXmlParseLanguages(const BxceDocument *doc, BxceNodeId parent,
				 SceInt32 *outLanguages);
int npCommunityXmlParseAboutMe(const BxceDocument *doc, BxceNodeId parent, char *outAboutMe);
int npCommunityXmlParseAvatarUrl(const BxceDocument *doc, BxceNodeId parent, char *outAvatarUrl,
				 SceInt32 *outAvatarId);
int npCommunityXmlParsePanelUrl(const BxceDocument *doc, BxceNodeId parent,
				NpCommunityPanelInfo *outPanel);
int npCommunityXmlParseShortString(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				   char *outBuf);
int npCommunityXmlParseBgColorFallback(const BxceDocument *doc, BxceNodeId parent,
				       NpCommunityPanelInfo *outPanel);
int npCommunityXmlParsePremiumAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
				   SceInt32 *outIsPremium);
int npCommunityXmlParseChildUintAndAttr(const BxceDocument *doc, BxceNodeId parent,
					const char *tagName, const char *attrName,
					SceUInt32 *outChildValue, SceUInt32 *outAttrValue);
void npCommunityFormatInt32(char *dst, SceInt32 value);
void npCommunityFormatUint64(char *dst, SceUInt64 value);
void npCommunityFormatInt64(char *dst, SceInt64 value);
SceInt64 npCommunityParseInt64(const char *text);
int npCommunityParseHexSignature(SceUInt8 *outDigest, const char *hexStr, SceSize hexLen);
int npCommunityHttpReadExact(NpCommunityBackendTx *tx, void *dst, SceSize size, SceSize *outRead);
int npCommunityHttpReadCommentSig(NpCommunityBackendTx *tx, SceUInt8 *outDigest,
				  SceUInt64 *ioContentLength);
void npCommunityFormatHexBytes(char *dst, const SceUInt8 *src, SceSize len);
void npCommunityHttpAddInfoLengthHeader(SceInt32 httpReqId, SceSize infoLen);
void npCommunityHttpAddRangeHeader(SceInt32 httpReqId, const SceUInt64 *startByte,
				   const SceUInt64 *endByte);
void npCommunityHttpAddDateHeader(SceInt32 httpReqId, const char *headerName,
				  const SceRtcTick *utcTick);
int npCommunityHttpParseDateHeader(SceInt32 httpReqId, const char *headerName,
				   SceRtcTick *outTick);
int npCommunityParseJidToNpId(SceNpId *outNpId, const char *jidStr);
int npCommunityFormatNpIdToJid(char **outJidStr, const SceNpId *npId);
int npCommunityEscapeXmlAlloc(char **outEscaped, const char *src);
int npCommunityUnescapeXmlAlloc(char **outUnescaped, const char *src);
int npCommunityXmlVerifyTitleId(const BxceDocument *doc, BxceNodeId node,
				const SceNpCommunicationId *expectedCommId);
void npCommunityCompactLanguages(SceInt32 *languages);
int npCommunityFormatOpenPsId(char *outHex);
SceSize npCommunityAppendBounded(char *dst, const char *src, SceSize maxTotal);
int npCommunityVerifyCommentHmac(const void *body, SceSize bodyLen, const SceUInt8 *expectedDigest,
				 int sigMode);
int npCommunityVerifyPassphraseHmac(const void *body, SceSize bodyLen,
				    const SceUInt8 *expectedDigest, int sigMode,
				    const void *passphrase);

#endif /* SHELL_NP_COMMUNITY_BACKEND_H */
