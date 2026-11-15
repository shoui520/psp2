/*
 * SceNpUtility: what the lookup asks the servers. The online ID's search, a profile or several,
 * their avatars and the comment's check each build the XML of the question, send it, and read
 * the XML of the answer through a parser (xml.c).
 */

#include "np_utility_internal.h"

/* the SDK's languages by what the server may send; ones it cannot know are dropped */
static __attribute__((noinline)) SceInt32 *normalizeLanguages(SceInt32 *languages)
{
	SceInt32 second;
	SceInt32 third;

	if (languages != NULL) {
		second = languages[1];
		third = languages[2];
		if (second >= 0 && (languages[0] < 0 || languages[0] == second)) {
			second = -1;
			languages[0] = languages[1];
			languages[1] = -1;
		}
		if (third >= 0) {
			if (languages[0] < 0 || languages[0] == third) {
				languages[0] = third;
				languages[2] = -1;
			} else if (second < 0 || second == third) {
				languages[1] = third;
				languages[2] = -1;
			}
		}
	}
	return languages;
}

/*
 * Feeds the response, size bytes or all of it when size is 0, to the parser through buffer;
 * *read is what the last read gave.
 */
static inline __attribute__((always_inline)) int feedResponse(NpLookupTrans *trans,
							      NpLookupXmlParser *parser,
							      SceSize size, char *buffer,
							      SceSize *read)
{
	SceSize left = size;
	SceSize length;
	int ret;

	while (left != 0 || size == 0) {
		length = left;
		if (left > 256 || size == 0)
			length = 256;
		ret = SceNpUtility_A3E828ED(trans, buffer, length, read);
		if (ret < 0)
			return ret;
		if (*read == 0)
			break;
		ret = SceNpUtility_E2135903(parser, buffer, *read);
		if (ret < 0)
			return ret;
		if (size != 0)
			left -= *read;
	}
	if (left != 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

/* the NP ID of the online ID the transaction was given, asked of the searchjid servers */
int SceNpUtility_B418204E(int reqId, SceNpId *npId)
{
	char jidBuffer[256];
	char serversBuffer[256];
	NpLookupTrans *trans = NULL;
	SceSize jidSize = 0;
	SceSize jidRead = 0;
	NpLookupServer *servers;
	SceSize serversSize = 0;
	SceSize serversRead = 0;
	NpLookupJidParse jid;
	NpLookupServersParse list;
	char request[256];
	NpLookupXmlParser jidParser;
	NpLookupXmlParser serversParser;
	NpLookupServer *server;
	const char *onlineId;
	int length;
	int ret;
	int answer;

	sceClibMemset(request, 0, sizeof(request));
	if (npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	sceClibMemset(npId, 0, sizeof(*npId));
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto end;
	if (trans->operation != NP_LOOKUP_OPERATION_NP_ID) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}
	onlineId = trans->data;
	if (onlineId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto end;
	}
	if (!SceNpUtility_475B35A8(onlineId)) {
		ret = SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_USER_NPID;
		goto end;
	}

	if (trans->title->client->servers == NULL) {
		servers = NULL;
		serversSize = 0;
		ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_SEARCH_SERVERS, NULL, 0);
		if (ret >= 0) {
			ret = SceNpUtility_34B42669(trans, NULL, 0);
			if (ret >= 0) {
				ret = SceNpUtility_8857763D(trans, 1, NULL, &serversSize, 0, NULL, 0);
				if (ret >= 0) {
					sceClibMemset(&list, 0, sizeof(list));
					list.service = trans->service;
					list.head = &servers;
					answer = SceNpUtility_15227842(&serversParser, 1, 0, &list);
					if (answer >= 0) {
						answer = feedResponse(trans, &serversParser, serversSize,
								      serversBuffer, &serversRead);
						if (answer >= 0)
							answer = SceNpUtility_AB499EA4(&list);
					}
					ret = answer;
					if (ret >= 0) {
						SceNpUtility_866D737A(trans);
						for (server = servers; server != NULL;
						     server = server->next) {
							ret = SceNpUtility_A15150C9(trans->title->client,
										    server);
							if (ret < 0)
								break;
						}
						if (server == NULL) {
							ret = sceKernelLockMutex(
								trans->title->client->mutex, 1, NULL);
							if (ret >= 0) {
								if (trans->title->client->servers == NULL)
									trans->title->client->servers =
										servers;
								sceKernelUnlockMutex(
									trans->title->client->mutex, 1);
								ret = 0;
								goto search;
							}
						}
					}
				}
			}
		}
		SceNpUtility_E916C86A(servers, trans->service);
		goto end;
	}

search:
	if (ret < 0)
		goto end;
	server = trans->title->client->servers;
	length = SceNpUtility_83445FED(request, sizeof(request), onlineId);
	ret = length;
	if (length < 0)
		goto end;
	for (; server != NULL; server = server->next) {
		ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_SEARCH_JID, server->name,
					    length);
		if (ret < 0)
			break;
		ret = SceNpUtility_34B42669(trans, request, length);
		if (ret < 0)
			break;
		ret = SceNpUtility_8857763D(trans, 1, NULL, &jidSize, 0, NULL, 0);
		if (ret < 0)
			break;
		sceClibMemset(&jid, 0, sizeof(jid));
		jid.service = trans->service;
		jid.npId = npId;
		answer = SceNpUtility_2580CDAF(&jidParser, &jid);
		if (answer >= 0) {
			answer = feedResponse(trans, &jidParser, jidSize, jidBuffer, &jidRead);
			if (answer >= 0)
				answer = SceNpUtility_5D75C414(&jid);
		}
		if (answer != SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_USER_NPID) {
			if (answer < 0)
				ret = answer;
			goto end;
		}
		SceNpUtility_866D737A(trans);
		if (server->next == NULL) {
			ret = answer;
			goto end;
		}
	}
end:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

/* the profile of the NP ID the transaction was given, with its avatar and panel sizes asked for */
int SceNpUtility_7BC7E7F7(int reqId, const SceNpId *npId, SceInt32 avatarSizeType,
			  int panelSizeType, NpLookupProfile *profile)
{
	NpLookupTrans *trans = NULL;
	char request[256];
	char region[4];
	SceSize size = 0;
	SceUInt32 sdkVersion;
	SceInt32 limit;
	SceSize length;
	int ret;

	sceClibMemset(request, 0, sizeof(request));
	if (profile == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto end;
	if (trans->operation != NP_LOOKUP_OPERATION_PROFILE) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}
	sceClibMemset(profile, 0, 4);
	sceClibMemcpy_safe(region, &npId->opt[2], 2);
	region[2] = '\0';
	ret = SceNpUtility_8951DA6B(request, sizeof(request), npId, avatarSizeType, panelSizeType,
				    trans->service);
	if (ret < 0)
		goto end;
	length = ret;
	ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_PROFILE, region, length);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_34B42669(trans, request, length);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_8857763D(trans, 1, NULL, &size, 0, NULL, 0);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_1392F18F(trans, profile, NULL, NULL, NULL, size);
	if (ret < 0)
		goto end;
	ret = SceNpUtility_866D737A(trans);
	profile->npId = *npId;
	if (trans->service == NP_LOOKUP_SERVICE_LOOKUP) {
		sdkVersion = SceLibKernel_9F793F84();
		limit = 22;
		if (sdkVersion < 0x3500000) {
			limit = 21;
			if (sdkVersion < 0x3300000) {
				limit = 20;
				if (sdkVersion < 0x3100000) {
					limit = 18;
					if (sdkVersion >= 0x2000000)
						limit = 19;
				}
			}
		}
		if (profile->languages.language1 > limit)
			profile->languages.language1 = 1;
		if (profile->languages.language2 > limit)
			profile->languages.language2 = 1;
		if (profile->languages.language3 > limit)
			profile->languages.language3 = 1;
	}
	normalizeLanguages(&profile->languages.language1);
end:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

/* the profile response's XML read into profile; the rest are what the server named besides */
int SceNpUtility_1392F18F(NpLookupTrans *trans, NpLookupProfile *profile, SceInt32 *ptlp,
			  SceInt32 *update, SceRtcTick *date, SceSize size)
{
	SceSize read = 0;
	NpLookupProfileParse state;
	char buffer[256];
	NpLookupXmlParser parser;
	SceUInt32 operation;
	int mode;
	int ret;

	sceClibMemset(&state, 0, sizeof(state));
	state.profile = profile;
	state.service = trans->service;
	operation = trans->operation;
	if (operation > 0x16) {
		if (operation > 0x18)
			return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		mode = 1;
	} else {
		if (operation != 1)
			return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		mode = 2;
	}
	ret = SceNpUtility_839F805E(&parser, mode, &state);
	if (ret >= 0)
		ret = feedResponse(trans, &parser, size, buffer, &read);
	if (ret >= 0) {
		ret = SceNpUtility_F32ED701(&state);
		if (ret >= 0) {
			if (ptlp != NULL)
				*ptlp = state.ptlp;
			if (update != NULL)
				*update = state.update;
			if (date != NULL)
				*date = state.date;
		}
	}
	return ret;
}

/* the response of several profiles read into profiles, the errors of each into results */
int SceNpUtility_9EEDD7F8(NpLookupTrans *trans, NpLookupProfile *profiles, SceInt32 *results,
			  SceUInt32 count, SceUInt32 requested, SceSize size)
{
	SceSize read = 0;
	NpLookupProfilesParse state;
	char buffer[256];
	NpLookupXmlParser parser;
	int ret;

	sceClibMemset(&state, 0, 0x24);
	state.service = trans->service;
	state.profiles = profiles;
	state.results = results;
	state.count = count;
	state.answered = 0;
	state.requested = requested;
	ret = SceNpUtility_04AA56CB(&parser, &state);
	if (ret >= 0) {
		ret = feedResponse(trans, &parser, size, buffer, &read);
		if (ret >= 0)
			ret = SceNpUtility_A3767D45(&state);
	}
	return ret;
}

/* the profiles of count NP IDs; results[i] gets the error of the i-th */
int SceNpUtility_C2598A46(int reqId, const SceNpId *npIds, SceUInt32 count, SceInt32 avatarSizeType,
			  SceInt32 panelSizeType, NpLookupProfile *profiles, SceInt32 *results)
{
	NpLookupTrans *trans = NULL;
	char *body = NULL;
	char region[4];
	SceSize answered = 0;
	SceSize size = 0;
	SceUInt32 i;
	SceUInt32 j;
	int length;
	int ret;
	int answer;

	if (profiles == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto end;
	}
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto end;
	if (trans->operation != NP_LOOKUP_OPERATION_PROFILES) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto end;
	}
	if (count == 0)
		goto end;
	for (i = 0; i < count; i++) {
		results[i] = NP_LOOKUP_ERROR_PENDING;
		sceClibMemset(&profiles[i], 0, sizeof(profiles[i]));
		sceClibMemcpy_safe(&profiles[i], &npIds[i], sizeof(npIds[i]));
	}
	for (i = 0; i < count; i++) {
		if (results[i] != NP_LOOKUP_ERROR_PENDING)
			continue;
		sceClibMemcpy_safe(region, &npIds[i].opt[2], 2);
		region[2] = '\0';
		length = SceNpUtility_DFFF438A(&body, npIds, count, results, &answered,
					       avatarSizeType, panelSizeType, trans->service);
		ret = length;
		if (ret < 0)
			goto end;
		results[i] = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (body == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto end;
		}
		ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_PROFILES, region, length);
		if (ret < 0)
			goto end;
		answer = SceNpUtility_34B42669(trans, body, length);
		if (answer >= 0) {
			if (body != NULL) {
				SceNpUtility_6C1161AD(body, trans->service);
				body = NULL;
			}
			answer = SceNpUtility_8857763D(trans, 1, NULL, &size, 0, NULL, 0);
			if (answer >= 0) {
				answer = SceNpUtility_9EEDD7F8(trans, profiles, results, count,
							       answered, size);
				if (answer >= 0) {
					ret = SceNpUtility_866D737A(trans);
					continue;
				}
			}
		} else if (body != NULL) {
			SceNpUtility_6C1161AD(body, trans->service);
			body = NULL;
		}
		results[i] = answer;
		ret = SceNpUtility_866D737A(trans);
		if (trans->aborted == 1)
			goto end;
	}
	for (j = 0; j < count; j++) {
		if (results[j] == 0)
			normalizeLanguages(&profiles[j].languages.language1);
	}
end:
	if (body != NULL) {
		if (trans != NULL)
			SceNpUtility_6C1161AD(body, trans->service);
		body = NULL;
	}
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

/* the avatar the NP ID's profile names, read in parts into data */
int SceNpUtility_08893FA1(int reqId, const SceNpAvatarUrl *url, SceSize *outSize, SceSize maxSize,
			  void *data, void *option)
{
	NpLookupTrans *trans;
	NpLookupDownload *download;
	SceUInt64 rest;
	SceSize contentLength;
	SceSize read;
	SceSize length;
	SceSize probeRead;
	SceBool unknown;
	char probe[4];
	int ret;

	if (url == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	download = NULL;
	read = 0;
	contentLength = 0;
	rest = 0;
	trans = NULL;
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto done;
	download = trans->data;
	if (download == NULL) {
		if (trans->operation != NP_LOOKUP_OPERATION_PROFILE &&
		    trans->operation != NP_LOOKUP_OPERATION_AVATAR) {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			download = NULL;
			goto done;
		}
		trans->operation = NP_LOOKUP_OPERATION_AVATAR;
		trans->data = SceNpUtility_8CA24D19(sizeof(*download), trans->service);
		if (trans->data == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			download = NULL;
			goto done;
		}
		sceClibMemset(trans->data, 0, sizeof(*download));
		download = trans->data;
		download->state = NP_LOOKUP_DOWNLOAD_IDLE;
		ret = SceNpUtility_46543307(trans, download);
		if (ret < 0) {
			SceNpUtility_6C1161AD(trans->data, trans->service);
			trans->data = NULL;
			goto done;
		}
		download = trans->data;
	}
	if (download->state == NP_LOOKUP_DOWNLOAD_FAILED) {
		ret = download->result;
		goto done;
	}
	if (download->state == NP_LOOKUP_DOWNLOAD_DONE && outSize != NULL) {
		ret = 0;
		*outSize = download->size;
		goto done;
	}
	unknown = SCE_FALSE;
	length = maxSize;
	if (download->state == NP_LOOKUP_DOWNLOAD_IDLE) {
		ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_AVATAR, url->data, 0);
		if (ret < 0)
			goto done;
		ret = SceNpUtility_34B42669(trans, NULL, 0);
		if (ret < 0)
			goto done;
		ret = SceNpUtility_8857763D(trans, 1, &rest, &contentLength, 0, NULL, 1);
		if (ret < 0)
			goto done;
		rest = contentLength;
		unknown = (contentLength == 0);
		if (contentLength > maxSize) {
			ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
			goto done;
		}
		download->state = NP_LOOKUP_DOWNLOAD_READING;
		download->size = contentLength;
		download->left = contentLength;
	} else if (download->state != NP_LOOKUP_DOWNLOAD_READING) {
		goto finish;
	}
	if (length == 0)
		goto finish;
	if (download->left < length && !unknown)
		length = download->left;
	if (data == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto done;
	}
	ret = SceNpUtility_A3E828ED(trans, data, length, &read);
	if (ret < 0)
		goto done;
	if (length == read) {
		if (!unknown)
			download->left -= read;
	} else if (!unknown) {
		ret = SCE_NP_COMMUNITY_ERROR_HTTP_SERVER;
		goto done;
	}
	if (unknown && length == read) {
		probeRead = 0;
		ret = SceNpUtility_A3E828ED(trans, probe, 1, &probeRead);
		if (ret < 0)
			goto done;
		if (probeRead != 0) {
			ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
			goto done;
		}
	}
finish:
	if (download->left == 0) {
		SceNpUtility_866D737A(trans);
		download->state = NP_LOOKUP_DOWNLOAD_DONE;
	}
	ret = read;
	if (outSize != NULL)
		*outSize = read;
done:
	if (trans != NULL) {
		if (trans->aborted == 1) {
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		} else if (ret >= 0) {
			SceNpUtility_31D3542E(trans);
			goto out;
		}
		if (download != NULL) {
			download->result = ret;
			download->state = NP_LOOKUP_DOWNLOAD_FAILED;
		}
		SceNpUtility_31D3542E(trans);
	}
out:
	if (ret > 0)
		ret = 0;
	return ret;
}

/* the image at the URL, read in parts into data */
int SceNpUtility_E64B4101(int reqId, const char *url, SceSize *outSize, SceSize maxSize,
			  void *data, void *option)
{
	NpLookupTrans *trans;
	NpLookupDownload *download;
	SceUInt64 rest;
	SceSize contentLength;
	SceSize read;
	SceSize length;
	SceSize probeRead;
	SceBool unknown;
	char probe[4];
	int ret;

	if (url == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	download = NULL;
	read = 0;
	contentLength = 0;
	rest = 0;
	trans = NULL;
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto done;
	download = trans->data;
	if (download == NULL) {
		if (trans->operation != NP_LOOKUP_OPERATION_PROFILE &&
		    trans->operation != NP_LOOKUP_OPERATION_URL) {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			download = NULL;
			goto done;
		}
		trans->operation = NP_LOOKUP_OPERATION_URL;
		trans->data = SceNpUtility_8CA24D19(sizeof(*download), trans->service);
		if (trans->data == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			download = NULL;
			goto done;
		}
		sceClibMemset(trans->data, 0, sizeof(*download));
		download = trans->data;
		download->state = NP_LOOKUP_DOWNLOAD_IDLE;
		ret = SceNpUtility_46543307(trans, download);
		if (ret < 0) {
			SceNpUtility_6C1161AD(trans->data, trans->service);
			trans->data = NULL;
			goto done;
		}
		download = trans->data;
	}
	if (download->state == NP_LOOKUP_DOWNLOAD_FAILED) {
		ret = download->result;
		goto done;
	}
	if (download->state == NP_LOOKUP_DOWNLOAD_DONE && outSize != NULL) {
		ret = 0;
		*outSize = download->size;
		goto done;
	}
	unknown = SCE_FALSE;
	length = maxSize;
	if (download->state == NP_LOOKUP_DOWNLOAD_IDLE) {
		ret = SceNpUtility_C4FE3A57(trans, NP_LOOKUP_REQUEST_URL, url, 0);
		if (ret < 0)
			goto done;
		ret = SceNpUtility_34B42669(trans, NULL, 0);
		if (ret < 0)
			goto done;
		ret = SceNpUtility_8857763D(trans, 1, &rest, &contentLength, 0, NULL, 1);
		if (ret < 0)
			goto done;
		rest = contentLength;
		unknown = (contentLength == 0);
		if (contentLength > maxSize) {
			ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
			goto done;
		}
		download->state = NP_LOOKUP_DOWNLOAD_READING;
		download->size = contentLength;
		download->left = contentLength;
	} else if (download->state != NP_LOOKUP_DOWNLOAD_READING) {
		goto finish;
	}
	if (length == 0)
		goto finish;
	if (download->left < length && !unknown)
		length = download->left;
	if (data == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto done;
	}
	ret = SceNpUtility_A3E828ED(trans, data, length, &read);
	if (ret < 0)
		goto done;
	if (length == read) {
		if (!unknown)
			download->left -= read;
	} else if (!unknown) {
		ret = SCE_NP_COMMUNITY_ERROR_HTTP_SERVER;
		goto done;
	}
	if (unknown && length == read) {
		probeRead = 0;
		ret = SceNpUtility_A3E828ED(trans, probe, 1, &probeRead);
		if (ret < 0)
			goto done;
		if (probeRead != 0) {
			ret = SCE_NP_COMMUNITY_ERROR_BODY_TOO_LARGE;
			goto done;
		}
	}
finish:
	if (download->left == 0) {
		SceNpUtility_866D737A(trans);
		download->state = NP_LOOKUP_DOWNLOAD_DONE;
	}
	ret = read;
	if (outSize != NULL)
		*outSize = read;
done:
	if (trans != NULL) {
		if (trans->aborted == 1) {
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		} else if (ret >= 0) {
			SceNpUtility_31D3542E(trans);
			goto out;
		}
		if (download != NULL) {
			download->result = ret;
			download->state = NP_LOOKUP_DOWNLOAD_FAILED;
		}
		SceNpUtility_31D3542E(trans);
	}
out:
	if (ret > 0)
		ret = 0;
	return ret;
}

/* the censor response's XML read into the caller's buffer; *passed is whether the comment passed */
int SceNpUtility_4B30CAEF(NpLookupTrans *trans, const char *comment, char *sanitized, int sanitize,
			  int *passed, SceSize size)
{
	SceSize read = 0;
	NpLookupCensorParse state;
	char buffer[256];
	NpLookupXmlParser parser;
	int ret;

	sceClibMemset(&state, 0, 0x18);
	state.service = trans->service;
	state.text = (char *)comment;
	state.sanitized = sanitized;
	state.sanitize = sanitize;
	ret = SceNpUtility_67147C73(&parser, &state);
	if (ret >= 0) {
		ret = feedResponse(trans, &parser, size, buffer, &read);
		if (ret >= 0) {
			ret = SceNpUtility_A9D5542A(&state);
			if (ret >= 0 && passed != NULL)
				*passed = state.passed;
		}
	}
	return ret;
}

/* has the comment checked, or sanitized when sanitize, by the filter server */
int SceNpUtility_C9283605(int reqId, const SceNpId *npId, const char *comment,
			  char *sanitizedComment, int sanitize)
{
	NpLookupTrans *trans = NULL;
	char request[0x500];
	char region[4];
	SceSize size = 0;
	int passed = 0;
	int length;
	int ret;

	sceClibMemset(request, 0, sizeof(request));
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret >= 0) {
		if (trans->operation == NP_LOOKUP_OPERATION_CENSOR ||
		    trans->operation == NP_LOOKUP_OPERATION_SANITIZE) {
			if (sceClibStrnlen(comment, 0x401) != 0) {
				sceClibMemcpy_safe(region, &npId->opt[2], 2);
				region[2] = '\0';
				ret = SceNpUtility_60D70B4A(request, sizeof(request),
							    &trans->title->commId, npId, comment,
							    sanitizedComment, sanitize,
							    trans->service);
				if (ret >= 0) {
					length = ret;
					ret = SceNpUtility_C4FE3A57(trans, trans->operation, region,
								    length);
					if (ret >= 0) {
						ret = SceNpUtility_34B42669(trans, request, length);
						if (ret >= 0) {
							ret = SceNpUtility_8857763D(
								trans, 1, NULL, &size, 0, NULL, 0);
							if (ret >= 0) {
								ret = SceNpUtility_4B30CAEF(
									trans, comment,
									sanitizedComment, sanitize,
									&passed, size);
								if (ret >= 0) {
									ret = SceNpUtility_866D737A(
										trans);
									if (sanitize == 0 && passed != 1)
										ret = SCE_NP_COMMUNITY_SERVER_ERROR_CENSORED;
								}
							}
						}
					}
				}
			} else {
				ret = 0;
				if (sanitize == 1 && sanitizedComment != NULL)
					*sanitizedComment = '\0';
			}
		} else {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		}
	}
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
	return ret;
}

/* the XML of a profile request for the NP ID, with the avatar and panel sizes; its length */
int SceNpUtility_8951DA6B(char *buf, SceSize bufSize, const SceNpId *npId, SceUInt32 avatarSizeType,
			  SceUInt32 panelSizeType, int service)
{
	char version[8];
	const char *avatar;
	const char *panel;
	char *jid = NULL;
	char *p;
	int ret;

	ret = SceNpUtility_14437509(&jid, npId, service);
	if (ret >= 0) {
		if (jid == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto end;
		}
		if (avatarSizeType > 1) {
			if (avatarSizeType <= 2) {
				avatar = "s";
			} else {
				if (avatarSizeType > 3) {
					ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
					goto end;
				}
				avatar = "n";
			}
		} else if (avatarSizeType != 0) {
			avatar = "m";
		} else {
			avatar = "l";
		}
		if (panelSizeType > 1) {
			if (panelSizeType <= 2)
				panel = "s";
			else
				panel = NULL;
		} else if (panelSizeType != 0) {
			panel = "m";
		} else {
			panel = "l";
		}
		p = buf + sceClibSnprintf(buf, bufSize, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
		SceNpUtility_450C80FB(version);
		p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform",
				     "psp2", "sv", version);
		p += sceClibSnprintf(p, bufSize, "<%s>%s</%s>", "jid", jid, "jid");
		p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" />", "avatar", "size", avatar);
		if (panel != NULL)
			p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" />", "panel", "size", panel);
		p += sceClibSnprintf(p, bufSize, "</%s>", "profile");
		ret = p - buf;
	}
end:
	if (jid != NULL)
		SceNpUtility_6C1161AD(jid, service);
	return ret;
}

/* the XML of a request for the profiles of the pending NP IDs; *body gets it, *count how many */
int SceNpUtility_DFFF438A(char **body, const SceNpId *npIds, SceUInt32 count, const SceInt32 *results,
			  SceSize *answered, SceUInt32 avatarSizeType, SceUInt32 panelSizeType,
			  int service)
{
	char version[8];
	const char *avatar;
	const char *panel;
	char *jid = NULL;
	char *buf = NULL;
	SceSize bufSize;
	char *p;
	SceUInt32 i;
	int ret;

	*answered = 0;
	if (avatarSizeType > 1) {
		if (avatarSizeType <= 2) {
			avatar = "s";
			bufSize = 80 * count + 256;
		} else {
			if (avatarSizeType > 3)
				return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			avatar = "n";
			bufSize = 80 * count + 256;
		}
	} else {
		bufSize = 80 * count + 256;
		if (avatarSizeType != 0)
			avatar = "m";
		else
			avatar = "l";
	}
	if (panelSizeType > 1) {
		if (panelSizeType <= 2)
			panel = "s";
		else
			panel = NULL;
	} else if (panelSizeType != 0) {
		panel = "m";
	} else {
		panel = "l";
	}
	buf = SceNpUtility_8CA24D19(bufSize, service);
	if (buf == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto end;
	}
	p = buf + sceClibSnprintf(buf, bufSize, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	SceNpUtility_450C80FB(version);
	p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" %s=\"%s\">", "profile", "platform", "psp2",
			     "sv", version);
	for (i = 0; i < count; i++) {
		if (results[i] == NP_LOOKUP_ERROR_PENDING) {
			ret = SceNpUtility_14437509(&jid, &npIds[i], service);
			if (ret < 0)
				goto end;
			if (jid == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
				goto end;
			}
			p += sceClibSnprintf(p, bufSize, "<%s>%s</%s>", "jid", jid, "jid");
			if (jid != NULL) {
				SceNpUtility_6C1161AD(jid, service);
				jid = NULL;
			}
			++*answered;
		}
	}
	p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" />", "avatar", "size", avatar);
	if (panel != NULL)
		p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" />", "panel", "size", panel);
	ret = sceClibSnprintf(p, bufSize, "</%s>", "profile");
	*body = buf;
	ret = (p + ret) - buf;
end:
	if (jid != NULL)
		SceNpUtility_6C1161AD(jid, service);
	if (ret < 0 && buf != NULL)
		SceNpUtility_6C1161AD(buf, service);
	return ret;
}

/* the XML of a search for an online ID: the searchjid request; its length */
int SceNpUtility_83445FED(char *buf, SceSize bufSize, const char *onlineId)
{
	char version[8];
	char *p;

	p = buf + sceClibSnprintf(buf, bufSize, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	SceNpUtility_450C80FB(version);
	p += sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" %s=\"%s\">", "searchjid", "platform", "psp2",
			     "sv", version);
	p += sceClibSnprintf(p, bufSize, "<%s>%s</%s>", "online-id", onlineId, "online-id");
	p += sceClibSnprintf(p, bufSize, "</%s>", "searchjid");
	return p - buf;
}

/* the XML of a request to check, or sanitize, a comment; its length */
int SceNpUtility_60D70B4A(char *buf, SceSize bufSize, const SceNpCommunicationId *commId,
			  const SceNpId *npId, const char *comment, char *sanitizedComment,
			  int sanitize, int service)
{
	char version[8];
	char commIdText[16];
	char psid[36];
	SceKernelOpenPsId openPsId;
	char *jid = NULL;
	char *escaped = NULL;
	char *p;
	char *q;
	SceSize length;
	SceUInt32 i;
	int ret;
	int written;

	sceClibMemset(psid, 0, 0x21);
	ret = SceNpUtility_14437509(&jid, npId, service);
	if (ret >= 0) {
		if (jid == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto end;
		}
		p = buf + sceClibSnprintf(buf, bufSize, "%s", "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
		SceNpUtility_450C80FB(version);
		q = p + sceClibSnprintf(p, bufSize, "<%s %s=\"%s\" %s=\"%s\">", "npfilter",
					"platform", "psp2", "sv", version);
		sceClibMemset(commIdText, 0, 13);
		written = SceNpCommon_E426FEE8(commId, commIdText, 13);
		if (written >= 0)
			written = sceClibSnprintf(q, 0x80, "<%s>%s</%s>", "npcommid", commIdText,
						  "npcommid");
		q += written + sceClibSnprintf(q + written, bufSize, "<%s>%s</%s>", "jid", jid, "jid");
		ret = sceKernelGetOpenPsId(&openPsId);
		if (ret >= 0) {
			p = psid;
			for (i = 0; i < 16; i++) {
				written = sceClibSnprintf(p, 33 - (p - psid), "%02x",
							  openPsId.id[i]);
				if (written < 0) {
					ret = SCE_NP_COMMUNITY_ERROR_NO_RESOURCE;
					goto end;
				}
				p += written;
			}
			q += sceClibSnprintf(q, bufSize, "<%s %s=\"%s\">%s</%s>", "psid", "type",
					     "open", psid, "psid");
			if (sanitize != 0) {
				q += sceClibSnprintf(q, bufSize, "<%s>", "message");
				ret = SceNpUtility_B8DFA4D1(&escaped, comment, service);
				if (ret < 0)
					goto end;
			} else {
				q += sceClibSnprintf(q, bufSize, "<%s %s=\"%s\">", "message", "type",
						     "ng");
				ret = SceNpUtility_B8DFA4D1(&escaped, comment, service);
				if (ret < 0)
					goto end;
			}
			length = sceClibStrnlen(escaped, 0x400);
			sceClibMemcpy_safe(q, escaped, length);
			q += length + sceClibSnprintf(q + length, bufSize, "</%s>", "message");
			q += sceClibSnprintf(q, bufSize, "</%s>", "npfilter");
			ret = q - buf;
		}
	}
end:
	if (jid != NULL) {
		SceNpUtility_6C1161AD(jid, service);
		jid = NULL;
	}
	if (escaped != NULL)
		SceNpUtility_6C1161AD(escaped, service);
	return ret;
}
