/*
 * SceNpUtility: the lookup library's requests. A transaction's request goes to one of the
 * community servers (getprof, searchjid, filter, static-resource) under the client's region
 * and NP environment, or to a URL; the servers answer in XML, which the parsers below read
 * into the caller's structures.
 */

#include "np_utility_internal.h"

#define COMMUNITY_DOMAIN	".community.playstation.net"

/* the requests by type: their server, its scheme and the path on it */
static const NpLookupRequestType s_requestTypes[NP_LOOKUP_REQUEST_NUM] = {
	[NP_LOOKUP_REQUEST_NONE] = { 0, 0, "https", SCE_HTTP_METHOD_POST, "", "", "" },
	[NP_LOOKUP_REQUEST_PROFILE] = { 0, 1, "http", SCE_HTTP_METHOD_POST, "getprof.",
					"/basic_view/func/get_profile", "" },
	[NP_LOOKUP_REQUEST_SEARCH_SERVERS] = { 0, 0, "http", SCE_HTTP_METHOD_GET,
					       "static-resource.", "_searchjid.xml", "" },
	[NP_LOOKUP_REQUEST_SEARCH_JID] = { 0, 1, "http", SCE_HTTP_METHOD_POST, "",
					   "/basic_view/func/search_jid", "" },
	[NP_LOOKUP_REQUEST_AVATAR] = { 1, 1, "http", SCE_HTTP_METHOD_GET, "", "", "" },
	[NP_LOOKUP_REQUEST_URL] = { 1, 1, "http", SCE_HTTP_METHOD_GET, "", "", "" },
	[NP_LOOKUP_REQUEST_CENSOR] = { 0, 1, "http", SCE_HTTP_METHOD_POST, "filter.",
				       "/basic_view/func/censor_message", "" },
	[NP_LOOKUP_REQUEST_SANITIZE] = { 0, 1, "http", SCE_HTTP_METHOD_POST, "filter.",
					 "/basic_view/func/censor_message", "" },
	[NP_LOOKUP_REQUEST_PROFILES] = { 0, 1, "http", SCE_HTTP_METHOD_POST, "getprof.",
					 "/basic_view/func/get_multi_user_profile", "" },
};

/* the community servers' password, each byte XORed with 0x7F */
static const char s_password[32] = {
	0x1d, 0x0f, 0x05, 0x44, 0x16, 0x2e, 0x02, 0x1e, 0x3e, 0x11, 0x21, 0x2f, 0x18, 0x0e, 0x4a, 0x4f,
	0x47, 0x03, 0x0d, 0x48, 0x45, 0x0b, 0x49, 0x44, 0x58, 0x40, 0x10, 0x1b, 0x35, 0x11, 0x10, 0x3d,
};

int SceNpUtility_A0991531(int id, int resolveRetry, SceUInt32 resolveTimeout,
			  SceUInt32 connectTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout)
{
	NpLookupTrans *trans = NULL;
	NpLookupTitle *title = NULL;
	int ret;

	if (resolveRetry == 0 && resolveTimeout == 0 && connectTimeout == 0 && sendTimeout == 0 &&
	    recvTimeout == 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	if (resolveRetry < 0 || (resolveTimeout != 0 && resolveTimeout < 1000000) ||
	    (connectTimeout != 0 && connectTimeout < 10000000) ||
	    (sendTimeout != 0 && sendTimeout < 10000000) ||
	    (recvTimeout != 0 && recvTimeout < 10000000)) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}

	if ((id & 0xF00000) == 0x400000 || (id & 0xF0000000) == 0x40000000 ||
	    (id & 0xF000000) == 0x4000000) {
		ret = SceNpUtility_D03605CD(id, &title);
		if (ret < 0)
			goto end;
	} else if ((id & 0xF00000) == 0x200000 || (id & 0xF0000000) == 0x20000000 ||
		   (id & 0xF000000) == 0x2000000) {
		ret = SceNpUtility_25D26B6F(id, &trans);
		if (ret < 0)
			goto end;
	}

	if (title != NULL) {
		title->resolveRetry = resolveRetry;
		title->resolveTimeout = resolveTimeout;
		title->connectTimeout = connectTimeout;
		title->sendTimeout = sendTimeout;
		title->recvTimeout = recvTimeout;
	} else if (trans != NULL) {
		trans->resolveRetry = resolveRetry;
		trans->resolveTimeout = resolveTimeout;
		trans->connectTimeout = connectTimeout;
		trans->sendTimeout = sendTimeout;
		trans->recvTimeout = recvTimeout;
	} else {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
		goto end;
	}
	ret = 0;
end:
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}

int SceNpUtility_23B387A5(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
			  void *option)
{
	NpLookupTrans *trans = NULL;
	char *data = NULL;
	int ret;

	if (onlineId == NULL || npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	} else if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceNpUtility_37AD71B9();
		if (ret >= 0) {
			ret = SceNpUtility_25D26B6F(reqId, &trans);
			if (ret >= 0) {
				if (trans->operation != 0) {
					ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
				} else {
					trans->operation = NP_LOOKUP_OPERATION_NP_ID;
					data = SceNpUtility_8CA24D19(SCE_NP_ONLINEID_MAX_LENGTH + 1,
								     trans->service);
					if (data == NULL) {
						ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
					} else {
						sceClibMemset(data, 0, SCE_NP_ONLINEID_MAX_LENGTH + 1);
						sceClibMemcpy_safe(data, onlineId,
								   SCE_NP_ONLINEID_MAX_LENGTH);
						ret = SceNpUtility_46543307(trans, data);
						if (ret >= 0) {
							trans->data = data;
							data = NULL;
							ret = SceNpUtility_B418204E(reqId, npId);
						}
					}
				}
			}
		}
	}

	if (data != NULL) {
		if (trans == NULL)
			return ret;
		SceNpUtility_6C1161AD(data, trans->service);
	}
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

int SceNpUtility_9D8E0730(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			  SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			  SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			  void *avatarImageData, SceSize avatarImageDataMaxSize,
			  SceSize *avatarImageDataSize, int avatarN, void *option)
{
	NpLookupTrans *trans = NULL;
	NpLookupProfile profile;
	int ret;

	sceClibMemset(&profile, 0, sizeof(profile));
	if (npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto end;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	ret = SceNpUtility_37AD71B9();
	if (ret < 0)
		goto end;

	if (avatarImageData != NULL) {
		if (avatarImageDataSize == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			goto end;
		}
		switch ((SceUInt32)avatarSizeType) {
		case SCE_NP_AVATAR_SIZE_LARGE:
			if (avatarImageDataMaxSize < SCE_NP_AVATAR_IMAGE_MAX_SIZE_LARGE) {
				ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
				goto end;
			}
			break;
		case SCE_NP_AVATAR_SIZE_MIDDLE:
			if (avatarImageDataMaxSize < SCE_NP_AVATAR_IMAGE_MAX_SIZE_MIDDLE) {
				ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
				goto end;
			}
			break;
		case SCE_NP_AVATAR_SIZE_SMALL:
			if (avatarImageDataMaxSize < SCE_NP_AVATAR_IMAGE_MAX_SIZE_SMALL) {
				ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
				goto end;
			}
			break;
		case NP_LOOKUP_AVATAR_SIZE_N:
			if (!avatarN) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
				goto end;
			}
			if (avatarImageDataMaxSize < SCE_NP_AVATAR_IMAGE_MAX_SIZE_SMALL) {
				ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
				goto end;
			}
			break;
		default:
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			goto end;
		}
	} else if (!avatarN && avatarSizeType == NP_LOOKUP_AVATAR_SIZE_N) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}

	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto end;
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}
	trans->operation = NP_LOOKUP_OPERATION_PROFILE;
	ret = SceNpUtility_7BC7E7F7(reqId, npId, avatarSizeType, 3, &profile);
	if (ret < 0)
		goto end;

	if (userInfo != NULL) {
		sceClibMemcpy_safe(&userInfo->userId, &profile.npId, sizeof(userInfo->userId));
		sceClibMemcpy_safe(&userInfo->icon, &profile.icon, sizeof(userInfo->icon));
		sceClibMemset(userInfo->reserved, 0, sizeof(userInfo->reserved));
	}
	if (aboutMe != NULL)
		*aboutMe = profile.aboutMe;
	if (languages != NULL)
		*languages = profile.languages;
	if (countryCode != NULL)
		*countryCode = profile.countryCode;
	if (avatarImageData != NULL && trans->aborted != 1) {
		trans->data = NULL;
		ret = SceNpUtility_08893FA1(reqId, &profile.icon, avatarImageDataSize,
					    avatarImageDataMaxSize, avatarImageData, option);
	}
end:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

int SceNpUtility_B2FD5E9D(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			  SceNpAvatarImage *avatarImage, void *option)
{
	NpLookupTrans *trans = NULL;
	int ret;

	if (avatarUrl == NULL || avatarImage == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	} else if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceNpUtility_37AD71B9();
		if (ret >= 0) {
			ret = SceNpUtility_25D26B6F(reqId, &trans);
			if (ret >= 0) {
				if (trans->operation != 0) {
					ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
				} else {
					trans->operation = NP_LOOKUP_OPERATION_AVATAR;
					trans->data = NULL;
					ret = SceNpUtility_08893FA1(reqId, avatarUrl,
								    &avatarImage->size,
								    SCE_NP_AVATAR_IMAGE_MAX_SIZE,
								    avatarImage->data, NULL);
					if (ret >= 0) {
						SceNpUtility_31D3542E(trans);
						return 0;
					}
				}
			}
		}
	}
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}

/* the HTTP authentication of the community servers: digest, realm c7y-basic */
int SceNpUtility_60CC5AFB(SceInt32 request, SceHttpAuthType authType, const char *realm,
			  char *username, char *password, SceBool needEntity,
			  SceUChar8 **entityBody, SceSize *entitySize, SceBool *save, void *userArg)
{
	int i;

	if (save != NULL)
		*save = SCE_FALSE;
	if (authType != SCE_HTTP_AUTH_DIGEST)
		return -1;
	if (realm == NULL)
		return -1;
	if ((SceUInt32)userArg <= NP_LOOKUP_AUTH_NONE || (SceUInt32)userArg > NP_LOOKUP_AUTH_COMMUNITY)
		return -1;
	if (sceClibStrncmp(realm, "c7y-basic", sceClibStrnlen("c7y-basic", 0x7FFFFFFF) + 1) != 0)
		return -1;
	if (username == NULL)
		return -1;
	sceClibStrncpy(username, "c7y-basic07",
		       sceClibStrnlen("c7y-basic07", 0x7FFFFFFF) + 1);
	if (password == NULL)
		return -1;
	for (i = 0; i < sizeof(s_password); i++)
		password[i] = s_password[i] ^ 0x7F;
	password[sizeof(s_password)] = '\0';
	return 0;
}

/* the transaction's connection to server, or to the URL in server when withUrl */
__attribute__((noinline)) int SceNpUtility_01C44B07(int withUrl, NpLookupTrans *trans, const char *server,
			  const char *scheme, SceUShort16 port)
{
	int auth;
	int ret;

	if (withUrl != 0)
		ret = sceHttpCreateConnectionWithURL(trans->title->client->httpTemplate, server,
						     SCE_FALSE);
	else
		ret = sceHttpCreateConnection(trans->title->client->httpTemplate, server, scheme,
					      port, SCE_FALSE);
	if (ret < 0)
		return ret;
	trans->connection = ret;
	if (trans->resolveRetry != 0) {
		ret = sceHttpSetResolveRetry(ret, trans->resolveRetry);
		if (ret < 0)
			return ret;
	}
	if (trans->resolveTimeout != 0) {
		ret = sceHttpSetResolveTimeOut(trans->connection, trans->resolveTimeout);
		if (ret < 0)
			return ret;
	}

	if (sceClibStrncasecmp(server, "getprof", sceClibStrnlen("getprof", 0x7FFFFFFF)) == 0)
		auth = NP_LOOKUP_AUTH_COMMUNITY;
	else if (sceClibStrncasecmp(server, "searchjid", sceClibStrnlen("searchjid", 0x7FFFFFFF)) == 0)
		auth = NP_LOOKUP_AUTH_COMMUNITY;
	else if (sceClibStrncasecmp(server, "filter", sceClibStrnlen("filter", 0x7FFFFFFF)) == 0)
		auth = NP_LOOKUP_AUTH_COMMUNITY;
	else
		auth = NP_LOOKUP_AUTH_NONE;
	sceHttpSetAuthInfoCallback(trans->connection, SceNpUtility_60CC5AFB, (void *)auth);
	return 0;
}

/* the transaction's request for path, or for the URL in path when withUrl */
__attribute__((noinline)) int SceNpUtility_A08E60BA(int withUrl, NpLookupTrans *trans, int method, const char *path,
			  SceULong64 contentLength)
{
	int ret;

	if (withUrl == 0)
		ret = sceHttpCreateRequest(trans->connection, method, path, contentLength);
	else
		ret = sceHttpCreateRequestWithURL(trans->connection, method, path, contentLength);
	if (ret < 0)
		return ret;
	trans->request = ret;
	if (trans->connectTimeout != 0) {
		ret = sceHttpSetConnectTimeOut(ret, trans->connectTimeout);
		if (ret < 0)
			return ret;
	}
	if (trans->sendTimeout != 0) {
		ret = sceHttpSetSendTimeOut(trans->request, trans->sendTimeout);
		if (ret < 0)
			return ret;
	}
	if (trans->recvTimeout != 0) {
		ret = sceHttpSetRecvTimeOut(trans->request, trans->recvTimeout);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* NP_LOOKUP_ERROR_URL_TOO_LONG when the server's name would need more than 256 bytes */
static inline int checkServerName(const NpLookupTrans *trans, const NpLookupRequestType *request,
				  const char *name)
{
	int ret = 0;
	SceSize length;

	length = sceClibStrnlen(COMMUNITY_DOMAIN, 0x7FFFFFFF);
	length = sceClibStrnlen(request->host, 0x7FFFFFFF) +
		 sceClibStrnlen(trans->title->client->environment, 0x7FFFFFFF) + length + 1;
	if (name != NULL)
		length += sceClibStrnlen(name, 0x7FFFFFFF) + 1;
	if (length > 256)
		ret = NP_LOOKUP_ERROR_URL_TOO_LONG;
	return ret;
}

/* the transaction's connection and request of the given type; name names the server or URL */
__attribute__((noinline)) int SceNpUtility_C4FE3A57(NpLookupTrans *trans, SceUInt32 type, const char *name,
			  SceSize contentLength)
{
	const NpLookupRequestType *request;
	const char *path;
	char url[256];
	SceUShort16 port;
	int ret;

	sceClibMemset(url, 0, sizeof(url));
	request = &s_requestTypes[type];
	if (request->needsName == 1 && name == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto end;
	}

	switch (type) {
	case NP_LOOKUP_REQUEST_SEARCH_JID:
		sceClibStrncpy(url, name, sizeof(url) - 1);
		break;
	case NP_LOOKUP_REQUEST_PROFILE:
		ret = checkServerName(trans, request, name);
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s.%s%s", request->host, name, trans->title->client->environment,
				COMMUNITY_DOMAIN);
		break;
	case NP_LOOKUP_REQUEST_PROFILES:
		ret = checkServerName(trans, request, name);
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s.%s%s", request->host, name, trans->title->client->environment,
				COMMUNITY_DOMAIN);
		break;
	case NP_LOOKUP_REQUEST_SEARCH_SERVERS:
		ret = checkServerName(trans, request, NULL);
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s%s", request->host, trans->title->client->environment,
				COMMUNITY_DOMAIN);
		break;
	case NP_LOOKUP_REQUEST_AVATAR:
		if (name == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			goto end;
		}
		sceClibSnprintf(url, sizeof(url), "%s", name);
		break;
	case NP_LOOKUP_REQUEST_URL:
		if (name == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			goto end;
		}
		sceClibSnprintf(url, sizeof(url), "%s", name);
		break;
	case NP_LOOKUP_REQUEST_CENSOR:
	case NP_LOOKUP_REQUEST_SANITIZE:
		ret = checkServerName(trans, request, name);
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s.%s%s", request->host, name, trans->title->client->environment,
				COMMUNITY_DOMAIN);
		break;
	default:
		ret = checkServerName(trans, request, trans->title->client->region);
		if (ret < 0)
			goto end;
		sceClibSnprintf(url, sizeof(url), "%s%s.%s%s", request->host,
				trans->title->client->region, trans->title->client->environment, COMMUNITY_DOMAIN);
		break;
	}

	port = 443;
	if (sceClibStrncmp(request->scheme, "http", 64) == 0)
		port = 80;
	ret = SceNpUtility_01C44B07(request->withUrl, trans, url, request->scheme, port);
	if (ret < 0)
		goto end;

	switch (type) {
	case NP_LOOKUP_REQUEST_SEARCH_SERVERS:
		if (sceClibStrnlen(request->path, 0x7FFFFFFF) +
		    sceClibStrnlen("/np/resource/basic/", 0x7FFFFFFF) +
		    sceClibStrnlen(trans->title->client->region, 0x7FFFFFFF) + 1 > 256) {
			ret = NP_LOOKUP_ERROR_URL_TOO_LONG;
			goto end;
		}
		sceClibSnprintf(url, sizeof(url), "%s%s%s", "/np/resource/basic/",
				trans->title->client->region, request->path);
		path = url;
		break;
	case NP_LOOKUP_REQUEST_AVATAR:
	case NP_LOOKUP_REQUEST_URL:
		path = url;
		break;
	default:
		path = request->path;
		break;
	}
	ret = SceNpUtility_A08E60BA(request->withUrl, trans, request->method, path, contentLength);
end:
	return ret;
}

__attribute__((noinline)) int SceNpUtility_34B42669(NpLookupTrans *trans, const void *data, SceSize size)
{
	int ret;

	if (trans->aborted == 1)
		return SCE_NP_COMMUNITY_ERROR_ABORTED;
	ret = sceHttpSendRequest(trans->request, data, size);
	if (ret < 0)
		return SceNpUtility_89484A9C((void *)trans->request, ret);
	return ret;
}

__attribute__((noinline)) int SceNpUtility_866D737A(NpLookupTrans *trans)
{
	if (trans->request > 0) {
		sceHttpDeleteRequest(trans->request);
		trans->request = 0;
	}
	if (trans->connection > 0) {
		sceHttpDeleteConnection(trans->connection);
		trans->connection = 0;
	}
	return 0;
}

/* SCE_HTTP_ERROR_OUT_OF_MEMORY when the HTTP pool has less than 8 KiB free */
__attribute__((noinline)) int SceNpUtility_37AD71B9(void)
{
	SceHttpMemoryPoolStats stats;
	int ret;

	stats.poolSize = 0;
	stats.maxInuseSize = 0;
	stats.currentInuseSize = 0;
	ret = sceHttpGetMemoryPoolStats(&stats);
	if (ret >= 0 && stats.poolSize - stats.currentInuseSize < 0x2000)
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	return ret;
}

/*
 * The response's header checked for the operation. With whole, *size gets the content length;
 * otherwise *size gets the length of the signed information (all of the content when it has
 * none, at most NP_LOOKUP_RESPONSE_MAX_SIZE) and *rest what follows it.
 */
__attribute__((noinline)) int SceNpUtility_8857763D(NpLookupTrans *trans, int operation, SceUInt64 *rest, SceSize *size,
			  int unused, SceUInt8 *signature, int whole)
{
	SceUInt64 contentLength = 0;
	SceUInt32 infoLength = 0;
	int hasContentLength = 0;
	int statusCode = -1;
	int ret;

	ret = SceNpUtility_4FAC4566(trans, &statusCode, &contentLength, &hasContentLength,
				    &infoLength, signature, operation);
	if (ret < 0)
		goto error;
	if (operation == NP_LOOKUP_RESPONSE_USER && contentLength == 0 && hasContentLength) {
		ret = NP_LOOKUP_ERROR_NOT_FOUND;
		goto error;
	}
	if (contentLength == 0 && hasContentLength) {
		ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		goto error;
	}

	if (whole) {
		if (size != NULL)
			*size = contentLength;
		if (rest != NULL)
			*rest = 0;
		return 0;
	}
	if (infoLength != 0) {
		if (infoLength > contentLength && hasContentLength) {
			ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			goto error;
		}
		if (rest != NULL) {
			if (hasContentLength)
				*rest = contentLength - infoLength;
			else
				*rest = 0;
		}
	} else {
		if (contentLength > NP_LOOKUP_RESPONSE_MAX_SIZE && hasContentLength) {
			ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
			goto error;
		}
		infoLength = contentLength;
		if (rest != NULL)
			*rest = 0;
	}
	if (infoLength > NP_LOOKUP_RESPONSE_MAX_SIZE) {
		ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
		goto error;
	}
	if (size != NULL)
		*size = infoLength;
	return 0;
error:
	SceNpUtility_866D737A(trans);
	return ret;
}

int SceNpUtility_89C8F74E(NpLookupTrans *trans, int operation, SceUInt64 *rest, SceSize *size,
			  SceUInt8 *signature, int whole)
{
	return SceNpUtility_8857763D(trans, operation, rest, size, 0, signature, whole);
}

/* a transaction of the client for the operation; the data it is given is copied */
static int createTrans(NpLookupClient *client, int operation, SceUInt32 service,
		       const void *data, int titleId)
{
	void *copy = NULL;
	SceSize size;
	int ret;

	switch (operation) {
	case 0:
		if (data == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			break;
		}
		copy = SceNpUtility_8CA24D19(12, service);
		if (copy == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(copy, data, 12);
		goto create;
	case 3:
		if (data == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			break;
		}
		size = sceClibStrnlen(data, 0x7FFFFFFF) + 1;
		copy = SceNpUtility_8CA24D19(size, service);
		if (copy == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemcpy_safe(copy, data, size);
		goto create;
	case 21:
	case 22:
	case 23:
	create:
		ret = SceNpUtility_A48B44C9(client, operation, service, &copy, titleId);
		if (ret < 0) {
error:
			if (copy != NULL)
				SceNpUtility_6C1161AD(copy, service);
		}
		break;
	default:
		ret = SCE_NP_COMMUNITY_ERROR_UNKNOWN_TYPE;
		break;
	}
	return ret;
}

int SceNpUtility_548625FC(NpLookupTitle *title, int operation, const void *data, int titleId)
{
	return createTrans(title->client, operation, NP_LOOKUP_SERVICE_INT, data, titleId);
}

int SceNpUtility_4A479D1D(int titleId)
{
	return SceNpUtility_D5BB8464(titleId, NP_LOOKUP_SERVICE_INT);
}

int SceNpUtility_23414285(NpLookupTitle *title, int operation, const void *data, int titleId,
			  SceUInt32 service)
{
	return createTrans(title->client, operation, service, data, titleId);
}

int SceNpUtility_B4423DAF(int titleId, SceUInt32 service)
{
	return SceNpUtility_D5BB8464(titleId, service);
}

int SceNpUtility_C63A0F6B(int reqId)
{
	NpLookupTrans *trans = NULL;
	int ret;

	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret >= 0) {
		trans->aborted = 1;
		if (trans->request != 0)
			ret = sceHttpAbortRequest(trans->request);
	}
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}
