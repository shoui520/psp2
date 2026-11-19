/*
 * The NP community helper (np_community.h): SceNpUtility's community, started in three steps
 * and undone in reverse when a step fails, its requests, and a title's metadata from the TMDB
 * server.
 */
#include <stddef.h>
#include <scetypes.h>
#include <np/np_error.h>
#include <libhmac.h>
#include <libhttp/libhttp_error.h>
#include "../np_utility/np_utility_internal.h"
#include "np_community.h"

int npCommunityInit(SceUInt32 type, void *(*alloc)(SceSize size), void (*free)(void *ptr),
		    SceSize size, void *memblock)
{
	int ret;

	if (SceNpUtility_16954FFF() != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;
	switch (type) {
	case NP_COMMUNITY_INIT_DEFAULT:
		SceNpUtility_5FC4B7D5(0);
		break;
	case NP_COMMUNITY_INIT_ALLOCATOR:
		SceNpUtility_4A3FDD3B(0, alloc, free);
		break;
	case NP_COMMUNITY_INIT_PARAM:
		SceNpUtility_B5711AF9(0, size, memblock);
		break;
	default:
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	}
	ret = SceNpUtility_E38548AB(0);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_9F142A92(0);
	if (ret < 0)
		goto undo_first;
	ret = SceNpUtility_17385829(0);
	if (ret < 0)
		goto undo_second;
	SceNpUtility_5D6FC99E(1);
	return 0;

undo_second:
	SceNpUtility_6DF884DA(0);
undo_first:
	SceNpUtility_26B4AC00(0);
	return ret;
}

int npCommunityInitWithAllocator(void *(*alloc)(SceSize size), void (*free)(void *ptr))
{
	return npCommunityInit(NP_COMMUNITY_INIT_ALLOCATOR, alloc, free, 0, NULL);
}

int npCommunityTerm(void)
{
	int ret = 0;

	if (SceNpUtility_16954FFF() != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	SceNpUtility_6DF884DA(0);
	SceNpUtility_26B4AC00(0);
	SceNpUtility_7A86C37E(0);
	SceNpUtility_418E1324(0);
	SceNpUtility_5D6FC99E(0);
	return ret;
}

/* a title context of the community for the user selfNpId; commId NULL: the application's */
int npCommunityCreateTitleCtx(const SceNpCommunicationId *commId, const SceNpId *selfNpId)
{
	int clientId;
	int ret;

	if (SceNpUtility_16954FFF() != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (selfNpId == NULL)
		return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	ret = SceNpUtility_F4B21646(&selfNpId->handle, selfNpId, NP_LOOKUP_SERVICE_INT);
	if (ret == SCE_NP_COMMUNITY_ERROR_CLIENT_HANDLE_ALREADY_EXISTS)
		ret = 0;
	else if (ret < 0)
		return ret;
	clientId = ret;
	ret = SceNpUtility_44A2F444(commId, selfNpId, clientId, NP_LOOKUP_SERVICE_INT);
	if (ret < 0)
		SceNpUtility_ED2B86ED(clientId, NP_LOOKUP_SERVICE_INT);
	return ret;
}

int npCommunityDeleteTitleCtx(SceInt32 titleCtxId)
{
	NpLookupTitle *title = NULL;
	int ret;

	if (SceNpUtility_16954FFF() != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_INT);
	if (ret >= 0) {
		SceNpUtility_CD3A3712(title->client);
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_INT);
		title = NULL;
		ret = SceNpUtility_4013244C(titleCtxId, NP_LOOKUP_SERVICE_INT);
	}
	return ret;
}

int npCommunityCreateRequest(SceInt32 titleCtxId)
{
	NpLookupTitle *title = NULL;
	int ret;

	if (SceNpUtility_16954FFF() != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	} else {
		ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_INT);
		if (ret >= 0) {
			if (title == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				ret = SceNpUtility_548625FC(title, 0, &title->npId, title->id);
				if (ret >= 0) {
					SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_INT);
					return ret;
				}
			}
		}
	}
	if (title != NULL)
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_INT);
	return ret;
}

int npCommunityDeleteRequest(SceInt32 reqId)
{
	if (SceNpUtility_16954FFF() != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_4A479D1D(reqId);
}

/* the profiles of count users, each with its result */
int npCommunityGetUserProfiles(SceInt32 reqId, SceInt32 avatarSizeType, SceInt32 panelSizeType,
			       const SceNpId *npIds, SceUInt32 count, NpLookupProfile *profiles,
			       SceInt32 *results)
{
	NpLookupTrans *trans = NULL;
	int ret;

	if (SceNpUtility_16954FFF() != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	} else if (npIds == NULL || profiles == NULL || results == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	} else {
		ret = SceNpUtility_25D26B6F(reqId, &trans);
		if (ret >= 0) {
			if (trans->operation != 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			} else {
				trans->operation = NP_LOOKUP_OPERATION_PROFILES;
				ret = SceNpUtility_C2598A46(reqId, npIds, count, avatarSizeType,
							    panelSizeType, profiles, results);
			}
		}
	}
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

/* the transaction's operation while it reads a title's metadata, and its request type */
#define NP_COMMUNITY_OPERATION_TITLE_INFO	31
#define NP_COMMUNITY_REQUEST_FIRST		30
#define NP_COMMUNITY_REQUEST_TITLE_INFO		31

/* the title-info's state while it is parsed */
typedef struct NpCommunityTitleInfoParse {
	SceUInt32 flags;			/* 0x00: NP_COMMUNITY_TITLE_INFO_* */
	SceUInt32 service;			/* 0x04: the transaction's */
	const char *titleId;			/* 0x08 */
	NpCommunityTitleInfo *info;		/* 0x0C */
	int ver;				/* 0x10: the id's ver */
	SceInt32 locale;			/* 0x14 */
	int inName;				/* 0x18: in a name without a lang */
	int nameInLocale;			/* 0x1C: in a name in the locale's lang */
	int inSname;				/* 0x20 */
	int snameInLocale;			/* 0x24 */
	int iconIs128;				/* 0x28: the icon is 128x128 */
	int iconWithoutLang;			/* 0x2C */
} NpCommunityTitleInfoParse;

#define NP_COMMUNITY_TITLE_INFO_REV		0x02
#define NP_COMMUNITY_TITLE_INFO_ID		0x04
#define NP_COMMUNITY_TITLE_INFO_NAME		0x08
#define NP_COMMUNITY_TITLE_INFO_ICON		0x10
#define NP_COMMUNITY_TITLE_INFO_CONTENT_ID	0x20
#define NP_COMMUNITY_TITLE_INFO_DONE		0x40

/* the title-info's elements: level 1 by index */
#define TITLE_INFO_ID		0
#define TITLE_INFO_NAME		1
#define TITLE_INFO_SNAME	2
#define TITLE_INFO_ICON		3
#define TITLE_INFO_CONTENT_ID	4

static const NpXmlElement s_titleInfo[] = { { "title-info", 0 }, { "", 1 } };
static const NpXmlElement s_titleInfoElements[] = {
	{ "id", 1 }, { "name", 1 }, { "sname", 1 }, { "icon", 1 }, { "content-id", 1 },
	{ "", 1 },
};
static const NpXmlElement *const s_titleInfoLevels[] = { s_titleInfo, s_titleInfoElements };
static const NpXmlSchema s_titleInfoSchema = { s_titleInfoLevels, 2 };

/* the request types after SceNpUtility's: a POST to a server named by the caller, and the
   title-info's GET from tmdb.<environment>.dl.playstation.net */
static const NpLookupRequestType s_requestTypes[] = {
	{ 0, 0, "https", SCE_HTTP_METHOD_POST, "", "", "" },
	{ 1, 1, "http", SCE_HTTP_METHOD_GET, "tmdb.", ".dl.playstation.net/psp2-tmdb/", "" },
};

/* the key of the HMAC that names a title's directory on the server, each byte XORed with 0x7f */
static const SceUChar8 s_tmdbKey[64] = {
	0x8a, 0xa1, 0x19, 0xad, 0x17, 0x71, 0x5a, 0x24, 0x52, 0x88, 0xe1, 0x0b, 0x87, 0xef, 0x94, 0x8c,
	0x36, 0x59, 0x50, 0x1e, 0xf4, 0xb5, 0x9d, 0xd6, 0xd3, 0xb2, 0x91, 0x2e, 0x29, 0xb1, 0xf2, 0x8d,
	0xb2, 0x8d, 0xab, 0xf3, 0x0e, 0x68, 0x43, 0xa3, 0x5a, 0xeb, 0x39, 0x24, 0xf8, 0x3f, 0x22, 0x66,
	0x03, 0x8e, 0xd1, 0xac, 0xc8, 0x96, 0x18, 0x61, 0x94, 0x29, 0xb5, 0x18, 0x2c, 0xbd, 0x99, 0xcf,
};

#define TMDB_KEY_MASK		0x7f

static int npCommunityReadTitleInfo(SceInt32 reqId, const char *titleId, SceInt32 locale,
				    NpCommunityTitleInfo *info);
static int npCommunityCreateTitleRequest(NpLookupTrans *trans, SceUInt32 type,
					 const char *name, SceSize contentLength);

int npCommunityGetTitleInfo(SceInt32 reqId, const char *titleId, SceInt32 locale,
			    NpCommunityTitleInfo *info)
{
	NpLookupTrans *trans = NULL;
	int ret;

	if (SceNpUtility_16954FFF() != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	} else if (titleId == NULL || info == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	} else {
		ret = SceNpUtility_25D26B6F(reqId, &trans);
		if (ret >= 0) {
			if (trans->operation != 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			} else {
				trans->operation = NP_COMMUNITY_OPERATION_TITLE_INFO;
				ret = npCommunityReadTitleInfo(reqId, titleId, locale, info);
			}
		}
	}
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

static int npCommunityBeginTitleInfo(NpLookupXmlParser *parser,
				     NpCommunityTitleInfoParse *state);
static int npCommunityEndTitleInfo(NpCommunityTitleInfoParse *state);

/* the title-info's path: "<title>_<HMAC-SHA1 of the title, in hex>/<title>.xml" */
static __attribute__((noinline)) int npCommunityReadTitleInfo(SceInt32 reqId,
	const char *titleId, SceInt32 locale, NpCommunityTitleInfo *info)
{
	NpLookupTrans *trans = NULL;
	SceSize size = 0;
	char path[256];
	SceUChar8 buf[256];
	SceUChar8 key[sizeof(s_tmdbKey)];
	SceUChar8 digest[SCE_HMAC_SHA1_DIGEST_SIZE];
	char hex[SCE_HMAC_SHA1_DIGEST_SIZE * 2 + 1];
	NpCommunityTitleInfoParse state;
	NpLookupXmlParser parser;
	SceSize read;
	SceSize total;
	int unknownSize;
	int ret;

	sceClibMemset(path, 0, sizeof(path));
	if (info == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	sceClibMemset(info, 0, sizeof(info->rev));
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto end;
	if (trans->operation != NP_COMMUNITY_OPERATION_TITLE_INFO) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}
	sceClibMemset(key, 0, sizeof(key));
	for (SceUInt32 i = 0; i < sizeof(key); i++)
		key[i] = s_tmdbKey[i] ^ TMDB_KEY_MASK;
	ret = sceHmacSha1Digest(key, sizeof(key), titleId,
				sceClibStrnlen(titleId, NP_COMMUNITY_TITLE_ID_SIZE), digest);
	if (ret >= 0) {
		char *out = hex;

		for (SceUInt32 i = 0; i < sizeof(digest); i++, out += 2) {
			SceUInt32 high = digest[i] >> 4;
			SceUInt32 low = digest[i] & 0xF;

			out[0] = high > 9 ? high + 'A' - 10 : high + '0';
			out[1] = low > 9 ? low + 'A' - 10 : low + '0';
		}
		ret = 0;
		out[0] = '\0';
		sceClibSnprintf(path, sizeof(path), "%s_%s/%s.xml", titleId, hex, titleId);
	}
	if (ret < 0)
		goto end;
	ret = npCommunityCreateTitleRequest(trans, NP_COMMUNITY_REQUEST_TITLE_INFO, path, 0);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_34B42669(trans, NULL, 0);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_89C8F74E(trans, NP_LOOKUP_RESPONSE_USER, NULL, &size, NULL, 0);
	if (ret < 0)
		goto end;

	read = 0;
	unknownSize = size == 0;
	sceClibMemset(&state, 0, sizeof(state));
	state.service = trans->service;
	state.titleId = titleId;
	state.locale = locale;
	state.info = info;
	ret = npCommunityBeginTitleInfo(&parser, &state);
	if (ret < 0)
		goto parsed;
	total = 0;
	while (size != 0 || unknownSize) {
		ret = SceNpUtility_A3E828ED(trans, buf,
			size > sizeof(buf) || unknownSize ? sizeof(buf) : size, &read);
		if (ret < 0)
			goto parsed;
		if (read == 0)
			break;
		total += read;
		ret = SceNpUtility_E2135903(&parser, (const char *)buf, read);
		if (ret < 0)
			goto parsed;
		if (!unknownSize)
			size -= read;
	}
	if (size != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	} else {
		ret = npCommunityEndTitleInfo(&state);
		if (ret >= 0 && total == 0)
			ret = NP_LOOKUP_ERROR_NOT_FOUND;
	}
parsed:
	if (ret < 0) {
		if (ret == SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE)
			ret = NP_LOOKUP_ERROR_NOT_FOUND;
		goto end;
	}
	ret = SceNpUtility_866D737A(trans);
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
end:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	if (ret == SCE_HTTP_ERROR_RESOLVER_ENOHOST)
		ret = NP_LOOKUP_ERROR_NOT_FOUND;
	return ret;
}

static int npCommunityTitleInfoHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth,
				       SceUInt32 index, const char *name, const char *value,
				       void *arg);

static __attribute__((noinline)) int npCommunityBeginTitleInfo(NpLookupXmlParser *parser,
	NpCommunityTitleInfoParse *state)
{
	state->flags = 0;
	state->ver = 0;
	state->inName = 0;
	state->nameInLocale = 0;
	state->inSname = 0;
	state->snameInLocale = 0;
	state->iconIs128 = 0;
	state->iconWithoutLang = 0;
	sceClibMemset(state->info, 0, sizeof(*state->info));
	state->info->locale = state->locale;
	return SceNpUtility_31423835(parser, &s_titleInfoSchema, npCommunityTitleInfoHandler,
				     state);
}

static __attribute__((noinline)) int npCommunityEndTitleInfo(NpCommunityTitleInfoParse *state)
{
	if ((state->flags & NP_COMMUNITY_TITLE_INFO_DONE) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

static int npCommunityTitleInfoHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth,
				       SceUInt32 index, const char *name, const char *value,
				       void *arg)
{
	NpCommunityTitleInfoParse *state = arg;

	(void)parser;
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 0 && index == 0 &&
	    sceClibStrncmp("rev", name, 128) == 0) {
		state->info->rev = SceNpCommon_CC8E6AC6(value);
		state->flags |= NP_COMMUNITY_TITLE_INFO_REV;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == TITLE_INFO_ID) {
		sceClibSnprintf(state->info->titleId, sizeof(state->info->titleId), "%s_%02d",
				name, state->ver);
		state->flags |= NP_COMMUNITY_TITLE_INFO_ID;
	}
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == TITLE_INFO_ID)
		state->ver = SceNpCommon_CC8E6AC6(value);

	if (event == NP_XML_EVENT_START && depth == 1 && index == TITLE_INFO_NAME) {
		state->inName = 1;
		state->nameInLocale = 0;
	}
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == TITLE_INFO_NAME &&
	    sceClibStrncmp("lang", name, 128) == 0) {
		state->inName = 0;
		if (SceNpCommon_CC8E6AC6(value) == state->locale)
			state->nameInLocale = 1;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == TITLE_INFO_NAME) {
		if (state->nameInLocale) {
			sceClibStrncpy(state->info->localName, name,
				       sizeof(state->info->localName));
		} else if (state->inName) {
			sceClibStrncpy(state->info->name, name, 128);
			state->flags |= NP_COMMUNITY_TITLE_INFO_NAME;
		}
	}

	if (event == NP_XML_EVENT_START && depth == 1 && index == TITLE_INFO_SNAME) {
		state->inSname = 1;
		state->snameInLocale = 0;
	}
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == TITLE_INFO_SNAME &&
	    sceClibStrncmp("lang", name, 128) == 0) {
		state->inSname = 0;
		if (SceNpCommon_CC8E6AC6(value) == state->locale)
			state->snameInLocale = 1;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == TITLE_INFO_SNAME) {
		if (state->snameInLocale)
			sceClibStrlcpy(state->info->localSname, name,
				       sizeof(state->info->localSname));
		else if (state->inSname)
			sceClibStrlcpy(state->info->sname, name, sizeof(state->info->sname));
	}

	if (event == NP_XML_EVENT_START && depth == 1 && index == TITLE_INFO_ICON) {
		state->iconIs128 = 0;
		state->iconWithoutLang = 1;
	}
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == TITLE_INFO_ICON) {
		if (sceClibStrncmp("lang", name, 128) == 0)
			state->iconWithoutLang = 0;
		if (sceClibStrncmp("type", name, 128) == 0)
			state->iconIs128 = sceClibStrncmp("128x128", value, 128) == 0;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == TITLE_INFO_ICON &&
	    state->iconWithoutLang && state->iconIs128) {
		sceClibStrncpy(state->info->iconUrl, name, sizeof(state->info->iconUrl));
		state->flags |= NP_COMMUNITY_TITLE_INFO_ICON;
	}
	/* the content ID is read only after a 128x128 icon without a lang */
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == TITLE_INFO_CONTENT_ID &&
	    state->iconWithoutLang && state->iconIs128) {
		sceClibStrlcpy(state->info->contentId, name, sizeof(state->info->contentId));
		state->flags |= NP_COMMUNITY_TITLE_INFO_CONTENT_ID;
	}

	if (event == NP_XML_EVENT_END && depth == 0 && index == 0) {
		if ((state->flags & NP_COMMUNITY_TITLE_INFO_ID) == 0 ||
		    (state->flags & NP_COMMUNITY_TITLE_INFO_NAME) == 0)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		state->flags |= NP_COMMUNITY_TITLE_INFO_DONE;
	}
	return 0;
}

/* SceNpUtility_C4FE3A57 for the request types after SceNpUtility's */
static __attribute__((noinline)) int npCommunityCreateTitleRequest(NpLookupTrans *trans,
	SceUInt32 type, const char *name, SceSize contentLength)
{
	const NpLookupRequestType *request;
	const char *path;
	char url[256];
	SceUShort16 port;
	int ret;

	sceClibMemset(url, 0, sizeof(url));
	request = &s_requestTypes[type - NP_COMMUNITY_REQUEST_FIRST];
	if (request->needsName == 1 && name == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto end;
	}
	if (type == NP_COMMUNITY_REQUEST_TITLE_INFO) {
		if (name == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			goto end;
		}
		sceClibSnprintf(url, sizeof(url), "%s://%s%s%s%s", request->scheme,
				request->host, trans->title->client->environment, request->path,
				name);
	} else {
		const char *region = trans->title->client->region;
		SceSize length;

		ret = 0;
		length = sceClibStrnlen(".community.playstation.net", 0x7FFFFFFF);
		length = sceClibStrnlen(request->host, 0x7FFFFFFF) +
			 sceClibStrnlen(trans->title->client->environment, 0x7FFFFFFF) +
			 length + 1;
		if (region != NULL)
			length += sceClibStrnlen(region, 0x7FFFFFFF) + 1;
		if (length > 256)
			ret = NP_LOOKUP_ERROR_URL_TOO_LONG;
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s.%s%s", request->host,
				trans->title->client->region, trans->title->client->environment,
				".community.playstation.net");
	}
	port = 443;
	if (sceClibStrncmp(request->scheme, "http", 64) == 0)
		port = 80;
	ret = SceNpUtility_01C44B07(request->withUrl, trans, url, request->scheme, port);
	if (ret < 0)
		goto end;
	path = type == NP_COMMUNITY_REQUEST_TITLE_INFO ? url : request->path;
	ret = SceNpUtility_A08E60BA(request->withUrl, trans, request->method, path,
				    contentLength);
end:
	return ret;
}
