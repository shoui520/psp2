/*
 * np_tus_backend.cpp: SceNpTus and SceNpTss HTTP/XML backend implementation
 * (0x81333A28..0x8133820B).
 */
#include "np_community_backend.h"

#include <libkernel/libkernel.h>
#include <libhttp.h>

static const char NP_TUS_XML_PROLOG[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
static const char NP_TUS_BODY_SEPARATOR[] = "\r\n\r\n";

/* 32-byte TUS anticheat HMAC key suffix (0x8151E368) */
static const SceUInt8 s_tusAnticheatKeySuffix[32] = {
	0x68, 0xB8, 0xF8, 0xEF, 0xFB, 0xA6, 0xEF, 0x62,
	0x94, 0x6B, 0xB0, 0xD1, 0xB9, 0x69, 0x4D, 0xF9,
	0x29, 0x01, 0x3A, 0xB7, 0x23, 0x93, 0x8F, 0x21,
	0x4E, 0xD6, 0x37, 0x90, 0x7B, 0x7F, 0x9A, 0xE3
};

/* Formatted hex copy of the last TUS passphrase signed (0x815C6200) */
static char s_tusLastPassphraseHex[257];

#define NP_COMMUNITY_TX_TSS		27

/* TUS XML request builder context */
struct NpTusXmlContext {
	BxceDocument *doc;
	BxceNodeId rootNode;
	BxceAttributeList *rootAttrs;
	BxceAttributeList *commIdAttrs;
	BxceAttributeList *signAttrs;
};

static char *npTusWrapXmlString(const char *serialized, SceSize extraReserve)
{
	SceSize prologLen = sceClibStrnlen(NP_TUS_XML_PROLOG, 0x7FFFFFFFu);
	SceSize bodyLen = sceClibStrnlen(serialized, 0x7FFFFFFFu);
	SceSize allocSize = prologLen + bodyLen + extraReserve + 1;
	char *outXml = static_cast<char *>(npCommunityAlloc(allocSize));
	if (outXml != NULL) {
		sceClibStrlcpy(outXml, NP_TUS_XML_PROLOG, allocSize);
		sceClibStrlcat(outXml, serialized, allocSize);
	}
	return outXml;
}

static void npTusFreeXmlContext(NpTusXmlContext *ctx)
{
	if (ctx->rootAttrs != NULL) {
		ScebXCe_9C2F9897(ctx->rootAttrs);
		sceNpCommonFreeNpServerName(ctx->rootAttrs);
		ctx->rootAttrs = NULL;
	}
	if (ctx->commIdAttrs != NULL) {
		ScebXCe_9C2F9897(ctx->commIdAttrs);
		sceNpCommonFreeNpServerName(ctx->commIdAttrs);
		ctx->commIdAttrs = NULL;
	}
	if (ctx->signAttrs != NULL) {
		ScebXCe_9C2F9897(ctx->signAttrs);
		sceNpCommonFreeNpServerName(ctx->signAttrs);
		ctx->signAttrs = NULL;
	}
	if (ctx->doc != NULL) {
		ScebXCe_B0891B13(ctx->doc);
		sceNpCommonFreeNpServerName(ctx->doc);
		ctx->doc = NULL;
	}
}

/* 0x81333A28 */
static int npTusOpenRequest(SceInt32 txId, int endpointId, const NpTusRequestEnv *env,
			    NpCommunityBackendTx **outTx)
{
	if (env == NULL || env->commId == NULL || env->hostName == NULL || env->selfNpId == NULL ||
	    env->ticketBuf == NULL || env->ticketSize == 0 || env->passphrase == NULL) {
		return NP_COMMUNITY_ERR_NULL_POINTER;
	}
	int ret = npCommunityAcquireBackendTx(txId, outTx);
	if (ret >= 0) {
		ret = npCommunityHttpOpenOnConn(*outTx, env->httpConnId, endpointId, env->hostName,
						0);
	}
	return ret;
}

/* 0x813364C8 */
static void npTusAddAnticheatHeaders(SceInt32 httpReqId, const char *xmlBody,
				     const SceNpCommunicationPassphrase *passphrase)
{
	struct {
		SceUInt8 passphraseBytes[128];
		SceUInt8 suffixBytes[32];
	} hmacKey;

	sceClibMemcpy_safe(hmacKey.passphraseBytes, passphrase, sizeof(hmacKey.passphraseBytes));
	sceClibMemcpy_safe(hmacKey.suffixBytes, s_tusAnticheatKeySuffix,
			   sizeof(hmacKey.suffixBytes));

	SceUInt8 digest[20];
	SceSize bodyLen = sceClibStrnlen(xmlBody, 0x7FFFFFFFu);
	npCommunityHmacSha1(&hmacKey, sizeof(hmacKey), xmlBody, bodyLen, digest);

	char digestHex[44];
	sceClibMemset(digestHex, 0, 41);
	npCommunityFormatHexBytes(digestHex, digest, sizeof(digest));
	npCommunityFormatHexBytes(s_tusLastPassphraseHex,
				  reinterpret_cast<const SceUInt8 *>(passphrase), 128);

	sceHttpAddRequestHeader(httpReqId, "Sce-Np-Anticheat-Version", "02", 0);
	sceHttpAddRequestHeader(httpReqId, "Sce-Np-Anticheat", digestHex, 0);
}

/* 0x81336582 */
static int npTusXmlCreateRequestDoc(NpTusXmlContext *outCtx, const NpTusRequestEnv *env)
{
	sceClibMemset(outCtx, 0, sizeof(*outCtx));
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc == NULL)
		return ret;

	BxceAttributeList *rootAttrs = NULL;
	BxceAttributeList *commIdAttrs = NULL;
	BxceAttributeList *signAttrs = NULL;
	BxceNodeId storageNode = 0;

	ret = ScebXCe_FAB50608(doc, 1);
	if (ret == 0) {
		rootAttrs = static_cast<BxceAttributeList *>(sceNpCommonMallocNpServerName(20));
		if (rootAttrs != NULL) {
			ret = ScebXCe_E7C8B20D(rootAttrs);
			if (ret == 0) {
				npCommunityXmlAddPlatformAttrs(rootAttrs);
				BxceNodeId docRoot = ScebXCe_75E1B095(doc);
				storageNode = ScebXCe_AC0E4F67(doc, docRoot, "npstorage", rootAttrs,
							       NULL, 0);
				if (storageNode != 0) {
					ret = npCommunityXmlAppendBase64(doc, storageNode, "ticket",
									 env->ticketBuf,
									 env->ticketSize);
					if (ret >= 0) {
						ret = npCommunityXmlAppendJid(
							doc, storageNode, NULL, "ticketjid",
							env->selfNpId);
					}
					if (ret >= 0) {
						ret = npCommunityXmlAppendOpenPsId(
							doc, storageNode, NULL, "psid");
					}
					int appVer = 0;
					if (ret >= 0) {
						ret = sceAppMgrAppParamGetInt(env->clientPid, 3,
									      &appVer);
					}
					if (ret >= 0) {
						commIdAttrs = static_cast<BxceAttributeList *>(
							sceNpCommonMallocNpServerName(20));
						if (commIdAttrs != NULL) {
							ret = ScebXCe_E7C8B20D(commIdAttrs);
							if (ret == 0) {
								ret = npCommunityXmlAppendCommIdVersion(
									doc, storageNode,
									commIdAttrs, "npcommid",
									env->commId, appVer);
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
					if (ret >= 0) {
						signAttrs = static_cast<BxceAttributeList *>(
							sceNpCommonMallocNpServerName(20));
						if (signAttrs != NULL) {
							ret = ScebXCe_E7C8B20D(signAttrs);
							if (ret == 0) {
								ScebXCe_3DD761CF(signAttrs, "pf",
										 "psp2");
								char commIdStr[16];
								char *jidStr = NULL;
								char *signB64 = NULL;
								sceClibMemset(commIdStr, 0, 13);
								int signRet = SceNpCommon_E426FEE8(
									env->commId, commIdStr, 13);
								if (signRet >= 0) {
									signRet = npCommunityFormatNpIdToJid(
										&jidStr,
										env->selfNpId);
								}
								if (signRet >= 0) {
									SceSize commLen = sceClibStrnlen(
										commIdStr, 0x7FFFFFFFu);
									SceSize jidLen = sceClibStrnlen(
										jidStr, 0x7FFFFFFFu);
									SceSize totalLen = commLen + jidLen;
									char *signInput = static_cast<char *>(
										npCommunityAlloc(totalLen));
									if (signInput != NULL) {
										sceClibMemset(signInput, 0,
											      totalLen);
										sceClibMemcpy_safe(
											signInput, commIdStr,
											commLen);
										sceClibMemcpy_safe(
											signInput + commLen,
											jidStr, jidLen);
										SceUInt8 digest[20];
										npCommunityHmacSha1(
											env->passphrase, 128,
											signInput, totalLen,
											digest);
										signRet = npCommunityBase64EncodeAlloc(
											&signB64, digest,
											sizeof(digest));
										if (signRet >= 0)
											signRet = 0;
										npCommunityFree(signInput);
									} else {
										signRet = NP_COMMUNITY_ERR_NO_MEMORY;
									}
								}
								if (jidStr != NULL)
									npCommunityFree(jidStr);
								ret = signRet;
								if (ret >= 0) {
									SceSize signLen = sceClibStrnlen(
										signB64, 0x7FFFFFFFu);
									if (ScebXCe_AC0E4F67(
										    doc, storageNode,
										    "sign", signAttrs,
										    signB64,
										    signLen) != 0) {
										ret = 0;
									} else {
										ret = NP_COMMUNITY_ERR_NO_MEMORY;
									}
								}
								if (signB64 != NULL)
									npCommunityFree(signB64);
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		} else {
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}

	if (ret >= 0) {
		outCtx->doc = doc;
		outCtx->rootNode = storageNode;
		outCtx->rootAttrs = rootAttrs;
		outCtx->commIdAttrs = commIdAttrs;
		outCtx->signAttrs = signAttrs;
		return 0;
	}

	if (rootAttrs != NULL) {
		ScebXCe_9C2F9897(rootAttrs);
		sceNpCommonFreeNpServerName(rootAttrs);
	}
	if (commIdAttrs != NULL) {
		ScebXCe_9C2F9897(commIdAttrs);
		sceNpCommonFreeNpServerName(commIdAttrs);
	}
	if (signAttrs != NULL) {
		ScebXCe_9C2F9897(signAttrs);
		sceNpCommonFreeNpServerName(signAttrs);
	}
	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return ret;
}

/* 0x813368FA */
static int npTusXmlCreateUserModeNode(BxceDocument *doc, BxceNodeId parentNode,
				      const char *tagName, SceInt32 isVirtualUser,
				      BxceNodeId *outNode, BxceAttributeList **outAttrs)
{
	BxceAttributeList *attrs = static_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(20));
	if (attrs == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	int ret = ScebXCe_E7C8B20D(attrs);
	if (ret == 0) {
		if (isVirtualUser == 0 || isVirtualUser == 1) {
			ScebXCe_3DD761CF(attrs, "mode", (isVirtualUser == 1) ? "1" : "0");
			BxceNodeId node = ScebXCe_AC0E4F67(doc, parentNode, tagName, attrs, NULL,
							   0);
			if (node != 0) {
				*outNode = node;
				*outAttrs = attrs;
				return 0;
			}
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		} else {
			ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
		}
	}
	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x813369AA */
static int npTusXmlAppendSlotNode(BxceDocument *doc, BxceNodeId parentNode, const char *tagName,
				  const char *slotAttrName, SceInt32 slotId,
				  const char *valueAttrName, const SceInt64 *valuePtr,
				  BxceNodeId *outNode)
{
	BxceAttributeList *attrs = static_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(20));
	if (attrs == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	int ret = ScebXCe_E7C8B20D(attrs);
	if (ret == 0) {
		ret = npCommunityXmlAddIntAttr(attrs, slotAttrName, slotId);
		if (ret >= 0 && valuePtr != NULL)
			ret = npCommunityXmlAddInt64Attr(attrs, valueAttrName, *valuePtr);
		if (ret >= 0) {
			BxceNodeId node = ScebXCe_AC0E4F67(doc, parentNode, tagName, attrs, NULL,
							   0);
			if (node != 0) {
				if (outNode != NULL)
					*outNode = node;
				ret = 0;
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}
	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

static int npTusXmlAppendUserIdentity(BxceDocument *doc, BxceNodeId userNode,
				      const SceNpId *targetNpId, SceInt32 isVirtualUser)
{
	if (isVirtualUser == 0)
		return npCommunityXmlAppendJid(doc, userNode, NULL, "jid", targetNpId);
	if (isVirtualUser == 1) {
		const char *onlineIdStr = targetNpId->handle.data;
		SceSize len = sceClibStrnlen(onlineIdStr, 0x7FFFFFFFu);
		if (ScebXCe_AC0E4F67(doc, userNode, "online-id", NULL, onlineIdStr, len) == 0)
			return NP_COMMUNITY_ERR_NO_MEMORY;
		return 0;
	}
	return NP_COMMUNITY_ERR_INVALID_ENDPOINT;
}

/* 0x81336A4E */
static int npTusBuildMultiSlotListXml(char **outXml, const NpTusRequestEnv *env,
				      const SceNpId *targetNpId, SceInt32 isVirtualUser,
				      const SceInt32 *slotIds, const char *itemTagName,
				      SceUInt32 arrayNum)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				BxceNodeId listNode = ScebXCe_AC0E4F67(ctx.doc, ctx.rootNode,
								       "list", NULL, NULL, 0);
				if (listNode != 0) {
					for (SceUInt32 idx = 0; idx < arrayNum; ++idx) {
						ret = npTusXmlAppendSlotNode(
							ctx.doc, listNode, itemTagName, "slot",
							slotIds[idx], NULL, NULL, NULL);
						if (ret < 0)
							break;
					}
					if (ret >= 0) {
						if (ScebXCe_D7FE96A2(&serializer, ctx.doc,
								     &serialized, NULL, 0) == 0 &&
						    serialized != NULL) {
							SceSize sepLen = sceClibStrnlen(
								NP_TUS_BODY_SEPARATOR, 0x7FFFFFFFu);
							char *wrapped = npTusWrapXmlString(
								serialized, sepLen);
							if (wrapped != NULL) {
								*outXml = wrapped;
								ret = 0;
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81336CA6 */
static int npTusBuildMultiUserListXml(char **outXml, const NpTusRequestEnv *env,
				      const SceNpId *targetNpIds, SceInt32 userQueryMode,
				      SceInt32 slotId, const char *itemTagName, SceUInt32 arrayNum)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlAppendSlotNode(ctx.doc, ctx.rootNode, itemTagName, "slot", slotId,
					     NULL, NULL, NULL);
		if (ret >= 0) {
			ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user",
							 userQueryMode, &userNode, &userAttrs);
		}
		if (ret >= 0) {
			if (userQueryMode == 0 || userQueryMode == 1) {
				for (SceUInt32 idx = 0; idx < arrayNum; ++idx) {
					ret = npTusXmlAppendUserIdentity(ctx.doc, userNode,
									 &targetNpIds[idx],
									 userQueryMode);
					if (ret < 0)
						break;
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
		if (ret >= 0) {
			if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
			    serialized != NULL) {
				char *wrapped = npTusWrapXmlString(serialized, 0);
				if (wrapped != NULL) {
					*outXml = wrapped;
					ret = 0;
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81336ED4 */
static int npTusBuildSetMultiSlotVarXml(char **outXml, const NpTusRequestEnv *env,
					const SceNpId *targetNpId, SceInt32 isVirtualUser,
					const SceInt32 *slotIds, const SceInt64 *variables,
					SceUInt32 arrayNum)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				BxceNodeId listNode = ScebXCe_AC0E4F67(ctx.doc, ctx.rootNode,
								       "list", NULL, NULL, 0);
				if (listNode != 0) {
					for (SceUInt32 idx = 0; idx < arrayNum; ++idx) {
						ret = npTusXmlAppendSlotNode(
							ctx.doc, listNode, "variable", "slot",
							slotIds[idx], "value", &variables[idx],
							NULL);
						if (ret < 0)
							break;
					}
					if (ret >= 0) {
						if (ScebXCe_D7FE96A2(&serializer, ctx.doc,
								     &serialized, NULL, 0) == 0 &&
						    serialized != NULL) {
							char *wrapped = npTusWrapXmlString(
								serialized, 0);
							if (wrapped != NULL) {
								*outXml = wrapped;
								ret = 0;
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8133712C */
static int npTusBuildFriendsQueryXml(char **outXml, const NpTusRequestEnv *env, SceInt32 slotId,
				     SceUInt32 startSerialRank, SceInt32 sortType,
				     SceInt32 includeSelf, const char *itemTagName,
				     SceInt32 arrayNum)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceAttributeList *optAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlAppendSlotNode(ctx.doc, ctx.rootNode, itemTagName, "slot", slotId,
					     NULL, NULL, NULL);
		if (ret >= 0) {
			const char *sortField = NULL;
			const char *sortOrder = NULL;
			switch (sortType) {
			case 1:
				sortField = "date";
				sortOrder = "desc";
				break;
			case 2:
				sortField = "date";
				sortOrder = "asc";
				break;
			case 3:
				sortField = "variable";
				sortOrder = "desc";
				break;
			case 4:
				sortField = "variable";
				sortOrder = "asc";
				break;
			default:
				ret = NP_COMMUNITY_ERR_INVALID_ARG;
				break;
			}
			if (ret >= 0) {
				optAttrs = static_cast<BxceAttributeList *>(
					sceNpCommonMallocNpServerName(20));
				if (optAttrs != NULL) {
					ret = ScebXCe_E7C8B20D(optAttrs);
					if (ret == 0) {
						ret = npCommunityXmlAddIntAttr(
							optAttrs, "offset",
							static_cast<SceInt32>(startSerialRank));
					}
					if (ret >= 0) {
						ret = npCommunityXmlAddIntAttr(optAttrs, "limit",
									       arrayNum);
					}
					if (ret >= 0)
						ret = ScebXCe_3DD761CF(optAttrs, "sort", sortField);
					if (ret >= 0)
						ret = ScebXCe_3DD761CF(optAttrs, "order", sortOrder);
					if (ret >= 0) {
						ret = ScebXCe_3DD761CF(
							optAttrs, "owner",
							(includeSelf != 0) ? "true" : "false");
					}
					if (ret >= 0) {
						if (ScebXCe_AC0E4F67(ctx.doc, ctx.rootNode,
								     "option", optAttrs, NULL,
								     0) == 0) {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
		if (ret >= 0) {
			if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
			    serialized != NULL) {
				char *wrapped = npTusWrapXmlString(serialized, 0);
				if (wrapped != NULL) {
					*outXml = wrapped;
					ret = 0;
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	if (optAttrs != NULL) {
		ScebXCe_9C2F9897(optAttrs);
		sceNpCommonFreeNpServerName(optAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813373F6 */
static int npTusBuildAddAndGetVarXml(char **outXml, const NpTusRequestEnv *env,
				     const SceNpId *targetNpId, SceInt32 isVirtualUser,
				     SceInt32 slotId, SceInt64 inVariable,
				     const SceNpId *isLastChangedAuthor,
				     const SceRtcTick *isLastChangedDate)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				ret = npTusXmlAppendSlotNode(ctx.doc, ctx.rootNode, "variable",
							     "slot", slotId, "value", &inVariable,
							     NULL);
			}
			if (ret >= 0 && isLastChangedDate != NULL) {
				ret = npCommunityXmlAppendRtcTick(ctx.doc, ctx.rootNode,
								  "comp-date", NULL,
								  isLastChangedDate, 0);
			}
			if (ret >= 0 && isLastChangedAuthor != NULL) {
				ret = npCommunityXmlAppendJid(ctx.doc, ctx.rootNode, NULL,
							      "comp-setter", isLastChangedAuthor);
			}
			if (ret >= 0) {
				if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
				    serialized != NULL) {
					char *wrapped = npTusWrapXmlString(serialized, 0);
					if (wrapped != NULL) {
						*outXml = wrapped;
						ret = 0;
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81337656 */
static int npTusBuildTryAndSetVarXml(char **outXml, const NpTusRequestEnv *env,
				     const SceNpId *targetNpId, SceInt32 isVirtualUser,
				     SceInt32 slotId, SceInt64 inVariable,
				     const SceInt64 *compareValue,
				     const SceNpId *isLastChangedAuthor,
				     const SceRtcTick *isLastChangedDate, SceInt32 opeType)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				BxceAttributeList *varAttrs = static_cast<BxceAttributeList *>(
					sceNpCommonMallocNpServerName(20));
				if (varAttrs != NULL) {
					ret = ScebXCe_E7C8B20D(varAttrs);
					if (ret == 0) {
						ret = npCommunityXmlAddIntAttr(varAttrs, "slot",
									       slotId);
					}
					if (ret >= 0) {
						const char *opStr = NULL;
						switch (opeType) {
						case 1:
							opStr = "eq";
							break;
						case 2:
							opStr = "ne";
							break;
						case 3:
							opStr = "gt";
							break;
						case 4:
							opStr = "ge";
							break;
						case 5:
							opStr = "lt";
							break;
						case 6:
							opStr = "le";
							break;
						default:
							ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
							break;
						}
						if (ret >= 0)
							ret = ScebXCe_3DD761CF(varAttrs, "try",
									       opStr);
					}
					if (ret >= 0) {
						ret = npCommunityXmlAddInt64Attr(varAttrs, "value",
										 inVariable);
					}
					if (ret >= 0 && compareValue != NULL) {
						ret = npCommunityXmlAddInt64Attr(
							varAttrs, "comp-value", *compareValue);
					}
					if (ret >= 0) {
						if (ScebXCe_AC0E4F67(ctx.doc, ctx.rootNode,
								     "variable", varAttrs, NULL,
								     0) == 0) {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
					ScebXCe_9C2F9897(varAttrs);
					sceNpCommonFreeNpServerName(varAttrs);
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
			if (ret >= 0 && isLastChangedDate != NULL) {
				ret = npCommunityXmlAppendRtcTick(ctx.doc, ctx.rootNode,
								  "comp-date", NULL,
								  isLastChangedDate, 0);
			}
			if (ret >= 0 && isLastChangedAuthor != NULL) {
				ret = npCommunityXmlAppendJid(ctx.doc, ctx.rootNode, NULL,
							      "comp-setter", isLastChangedAuthor);
			}
			if (ret >= 0) {
				if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
				    serialized != NULL) {
					char *wrapped = npTusWrapXmlString(serialized, 0);
					if (wrapped != NULL) {
						*outXml = wrapped;
						ret = 0;
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813379DE */
static int npTusBuildSetDataXml(char **outXml, const NpTusRequestEnv *env,
				const SceNpId *targetNpId, SceInt32 isVirtualUser, SceInt32 slotId,
				const char *itemTagName, const void *infoBytes,
				const SceNpId *isLastChangedAuthor,
				const SceRtcTick *isLastChangedDate, SceSize infoSize,
				SceSize totalSize)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceNodeId dataNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				ret = npTusXmlAppendSlotNode(ctx.doc, ctx.rootNode, itemTagName,
							     "slot", slotId, NULL, NULL,
							     &dataNode);
			}
			if (ret >= 0) {
				ret = npCommunityXmlAppendBase64Checked(ctx.doc, dataNode, "info",
									infoBytes, infoSize);
			}
			if (ret >= 0) {
				ret = npCommunityXmlAppendUint32(ctx.doc, dataNode, "size",
								 totalSize);
			}
			if (ret >= 0 && isLastChangedDate != NULL) {
				ret = npCommunityXmlAppendRtcTick(ctx.doc, ctx.rootNode,
								  "comp-date", NULL,
								  isLastChangedDate, 0);
			}
			if (ret >= 0 && isLastChangedAuthor != NULL) {
				ret = npCommunityXmlAppendJid(ctx.doc, ctx.rootNode, NULL,
							      "comp-setter", isLastChangedAuthor);
			}
			if (ret >= 0) {
				if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
				    serialized != NULL) {
					SceSize sepLen = sceClibStrnlen(NP_TUS_BODY_SEPARATOR,
									0x7FFFFFFFu);
					char *wrapped = npTusWrapXmlString(serialized, sepLen);
					if (wrapped != NULL) {
						*outXml = wrapped;
						ret = 0;
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81337C8E */
static int npTusBuildGetDataXml(char **outXml, const NpTusRequestEnv *env,
				const SceNpId *targetNpId, SceInt32 isVirtualUser, SceInt32 slotId,
				const char *itemTagName)
{
	BxceSerializer serializer;
	NpTusXmlContext ctx;
	BxceNodeId userNode = 0;
	BxceAttributeList *userAttrs = NULL;
	char *serialized = NULL;

	ScebXCe_8EA5A077(&serializer);
	int ret = npTusXmlCreateRequestDoc(&ctx, env);
	if (ret >= 0) {
		ret = npTusXmlCreateUserModeNode(ctx.doc, ctx.rootNode, "user", isVirtualUser,
						 &userNode, &userAttrs);
		if (ret >= 0) {
			ret = npTusXmlAppendUserIdentity(ctx.doc, userNode, targetNpId,
							 isVirtualUser);
			if (ret >= 0) {
				ret = npTusXmlAppendSlotNode(ctx.doc, ctx.rootNode, itemTagName,
							     "slot", slotId, NULL, NULL, NULL);
			}
			if (ret >= 0) {
				if (ScebXCe_D7FE96A2(&serializer, ctx.doc, &serialized, NULL, 0) == 0 &&
				    serialized != NULL) {
					char *wrapped = npTusWrapXmlString(serialized, 0);
					if (wrapped != NULL) {
						*outXml = wrapped;
						ret = 0;
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			}
		}
	}

	if (userAttrs != NULL) {
		ScebXCe_9C2F9897(userAttrs);
		sceNpCommonFreeNpServerName(userAttrs);
	}
	npTusFreeXmlContext(&ctx);
	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

static int npTusSendXmlRequest(NpCommunityBackendTx *tx, const NpTusRequestEnv *env,
			       char **ioReqXml, BxceDocument **outRespDoc,
			       SceUInt64 *outRemainingBytes)
{
	if (*ioReqXml == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	SceSize xmlLen = sceClibStrnlen(*ioReqXml, 0x7FFFFFFFu);
	sceHttpSetRequestContentLength(tx->httpReqId, xmlLen);
	npTusAddAnticheatHeaders(tx->httpReqId, *ioReqXml, env->passphrase);

	int ret = npCommunityHttpSend(tx, *ioReqXml, xmlLen, NULL);
	if (ret < 0)
		return ret;

	npCommunityFree(*ioReqXml);
	*ioReqXml = NULL;

	ret = npCommunityHttpRecvXml(tx, outRespDoc, NP_COMMUNITY_SIG_NONE, outRemainingBytes);
	if (ret >= 0)
		ret = npCommunityXmlCheckResult(*outRespDoc, "npstorage");
	return ret;
}

/* 0x81333A74 */
int npTusBackendSetMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
				     const SceNpId *targetNpId, SceInt32 isVirtualUser,
				     const SceInt32 *slotIds, const SceInt64 *variables,
				     SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || slotIds == NULL || variables == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_SET_MULTI_SLOT_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildSetMultiSlotVarXml(
					&reqXml, env, targetNpId, isVirtualUser, slotIds, variables,
					static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					ret = 0;
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81333BDC */
int npTusBackendAddAndGetVariable(SceInt32 txId, const NpTusRequestEnv *env,
				  const SceNpId *targetNpId, SceInt32 isVirtualUser,
				  SceInt32 slotId, SceInt64 inVariable,
				  const SceNpId *isLastChangedAuthor,
				  const SceRtcTick *isLastChangedDate, NpTusVariable *outVariable,
				  void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || outVariable == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outVariable, 0, sizeof(*outVariable));
		outVariable->ownerId = *targetNpId;

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_ADD_AND_GET_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildAddAndGetVarXml(
					&reqXml, env, targetNpId, isVirtualUser, slotId, inVariable,
					isLastChangedAuthor, isLastChangedDate);
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *varList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						varList = ScebXCe_1FA24DC0(respDoc, storageNode,
									   "variable");
						if (varList != NULL && varList->count != 0) {
							BxceNodeId varNode = ScebXCe_6C46D786(
								varList, 0);
							if (npCommunityXmlParseInt64Attr(
								    respDoc, varNode, "value",
								    &outVariable->variable) >= 0) {
								outVariable->oldVariable =
									outVariable->variable -
									inVariable;
								ret = npCommunityXmlParseChildDate(
									respDoc, varNode, "date",
									&outVariable->lastChangedDate);
								if (ret >= 0) {
									if (npCommunityXmlParseJidChild(
										    respDoc, varNode,
										    "setterjid",
										    &outVariable->lastChangedAuthorId) <
									    0) {
										outVariable->lastChangedAuthorId =
											*targetNpId;
									}
									outVariable->hasData = 1;
									ret = 0;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (varList != NULL) {
						ScebXCe_1F59F0C4(varList);
						ScebXCe_EC560647(varList);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81333ECE */
int npTusBackendGetMultiUserVariable(SceInt32 txId, const NpTusRequestEnv *env,
				     const SceNpId *targetNpIds, SceInt32 userQueryMode,
				     SceInt32 slotId, NpTusVariable *outVars, SceInt32 arrayNum,
				     void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpIds == NULL || slotId == 0 || outVars == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outVars, 0, sizeof(NpTusVariable) * static_cast<SceSize>(arrayNum));
		for (SceInt32 idx = 0; idx < arrayNum; ++idx)
			outVars[idx].ownerId = targetNpIds[idx];

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_MULTI_USER_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiUserListXml(
					&reqXml, env, targetNpIds, userQueryMode, slotId,
					"variable", static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *varList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						listNodes = ScebXCe_1FA24DC0(respDoc, storageNode,
									     "list");
						if (listNodes != NULL && listNodes->count != 0) {
							varList = ScebXCe_1FA24DC0(
								respDoc, storageNode, "variable");
							if (varList != NULL && varList->count != 0) {
								SceUInt32 count = varList->count;
								ret = static_cast<int>(count);
								for (SceUInt32 varIdx = 0;
								     varIdx < count; ++varIdx) {
									BxceNodeId varNode =
										ScebXCe_6C46D786(
											varList,
											varIdx);
									NpTusVariable parsed;
									sceClibMemset(&parsed, 0,
										      sizeof(parsed));
									parsed.hasData = 1;
									parsed.slotId = slotId;

									int parseRet =
										npCommunityXmlParseInt64Attr(
											respDoc,
											varNode,
											"value",
											&parsed.variable);
									if (parseRet >= 0) {
										if (userQueryMode != 0) {
											parseRet = npCommunityXmlParseShortString(
												respDoc, varNode,
												"online-id",
												parsed.ownerId.handle.data);
										} else {
											parseRet = npCommunityXmlParseJidChild(
												respDoc, varNode,
												"jid",
												&parsed.ownerId);
										}
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildDate(
											respDoc, varNode,
											"date",
											&parsed.lastChangedDate);
									}
									if (parseRet < 0) {
										ret = parseRet;
										break;
									}
									if (npCommunityXmlParseJidChild(
										    respDoc, varNode,
										    "setterjid",
										    &parsed.lastChangedAuthorId) <
									    0) {
										parsed.lastChangedAuthorId =
											parsed.ownerId;
									}
									for (SceInt32 matchIdx = 0;
									     matchIdx < arrayNum;
									     ++matchIdx) {
										if (sceClibMemcmp(
											    &outVars[matchIdx].ownerId.handle,
											    &parsed.ownerId.handle,
											    sizeof(SceNpOnlineId)) == 0) {
											outVars[matchIdx] = parsed;
										}
									}
								}
							} else {
								ret = 0;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (varList != NULL) {
						ScebXCe_1F59F0C4(varList);
						ScebXCe_EC560647(varList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x813342F6 */
int npTusBackendGetFriendsVariable(SceInt32 txId, const NpTusRequestEnv *env, SceInt32 slotId,
				   SceInt32 includeSelf, SceUInt32 startSerialRank,
				   SceInt32 sortType, NpTusVariable *outVars, SceInt32 arrayNum,
				   SceUInt32 *outHits, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outVars == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (slotId == 0 || arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outVars, 0, sizeof(NpTusVariable) * static_cast<SceSize>(arrayNum));

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_FRIEND_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildFriendsQueryXml(&reqXml, env, slotId,
								startSerialRank, sortType,
								includeSelf, "variable", arrayNum);
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *varList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						ret = npCommunityXmlParseChildUint(
							respDoc, storageNode, "hits", outHits);
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, storageNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0) {
								BxceNodeId listElem =
									ScebXCe_6C46D786(listNodes,
											 0);
								varList = ScebXCe_1FA24DC0(
									respDoc, listElem,
									"variable");
								if (varList != NULL &&
								    varList->count != 0) {
									SceUInt32 count =
										varList->count;
									ret = static_cast<int>(count);
									for (SceUInt32 varIdx = 0;
									     varIdx < count;
									     ++varIdx) {
										BxceNodeId varNode =
											ScebXCe_6C46D786(
												varList,
												varIdx);
										NpTusVariable parsed;
										sceClibMemset(
											&parsed, 0,
											sizeof(parsed));
										parsed.hasData = 1;
										parsed.slotId = slotId;

										int parseRet =
											npCommunityXmlParseInt64Attr(
												respDoc,
												varNode,
												"value",
												&parsed.variable);
										if (parseRet >= 0) {
											parseRet = npCommunityXmlParseJidChild(
												respDoc, varNode,
												"jid",
												&parsed.ownerId);
										}
										if (parseRet >= 0) {
											parseRet = npCommunityXmlParseChildDate(
												respDoc, varNode,
												"date",
												&parsed.lastChangedDate);
										}
										if (parseRet < 0) {
											ret = parseRet;
											break;
										}
										if (npCommunityXmlParseJidChild(
											    respDoc, varNode,
											    "setterjid",
											    &parsed.lastChangedAuthorId) <
										    0) {
											parsed.lastChangedAuthorId =
												parsed.ownerId;
										}
										outVars[varIdx] =
											parsed;
									}
								} else {
									ret = 0;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (varList != NULL) {
						ScebXCe_1F59F0C4(varList);
						ScebXCe_EC560647(varList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x813346D8 */
int npTusBackendGetMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
				     const SceNpId *targetNpId, SceInt32 isVirtualUser,
				     const SceInt32 *slotIds, NpTusVariable *outVars,
				     SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || slotIds == NULL || outVars == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outVars, 0, sizeof(NpTusVariable) * static_cast<SceSize>(arrayNum));
		for (SceInt32 idx = 0; idx < arrayNum; ++idx)
			outVars[idx].ownerId = *targetNpId;

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiSlotListXml(
					&reqXml, env, targetNpId, isVirtualUser, slotIds,
					"variable", static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *varList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						listNodes = ScebXCe_1FA24DC0(respDoc, storageNode,
									     "list");
						if (listNodes != NULL && listNodes->count != 0) {
							varList = ScebXCe_1FA24DC0(
								respDoc, storageNode, "variable");
							if (varList != NULL && varList->count != 0) {
								SceUInt32 count = varList->count;
								ret = static_cast<int>(count);
								for (SceUInt32 varIdx = 0;
								     varIdx < count; ++varIdx) {
									BxceNodeId varNode =
										ScebXCe_6C46D786(
											varList,
											varIdx);
									NpTusVariable parsed;
									sceClibMemset(&parsed, 0,
										      sizeof(parsed));
									parsed.ownerId = *targetNpId;
									parsed.hasData = 1;

									SceInt32 parsedSlot = 0;
									int parseRet =
										npCommunityXmlParseIntAttr(
											respDoc,
											varNode,
											"slot",
											&parsedSlot);
									if (parseRet >= 0) {
										parsed.slotId =
											parsedSlot;
										parseRet = npCommunityXmlParseInt64Attr(
											respDoc,
											varNode,
											"value",
											&parsed.variable);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildDate(
											respDoc, varNode,
											"date",
											&parsed.lastChangedDate);
									}
									if (parseRet < 0) {
										ret = parseRet;
										break;
									}
									if (npCommunityXmlParseJidChild(
										    respDoc, varNode,
										    "setterjid",
										    &parsed.lastChangedAuthorId) <
									    0) {
										parsed.lastChangedAuthorId =
											parsed.ownerId;
									}
									for (SceInt32 matchIdx = 0;
									     matchIdx < arrayNum;
									     ++matchIdx) {
										if (slotIds[matchIdx] ==
										    parsedSlot) {
											outVars[matchIdx] =
												parsed;
										}
									}
								}
							} else {
								ret = 0;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (varList != NULL) {
						ScebXCe_1F59F0C4(varList);
						ScebXCe_EC560647(varList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81334B16 */
int npTusBackendGetMultiUserDataStatus(SceInt32 txId, const NpTusRequestEnv *env,
				       const SceNpId *targetNpIds, SceInt32 userQueryMode,
				       SceInt32 slotId, NpTusDataStatus *outStatuses,
				       SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpIds == NULL || outStatuses == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL || slotId == 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outStatuses, 0,
			      sizeof(NpTusDataStatus) * static_cast<SceSize>(arrayNum));
		for (SceInt32 idx = 0; idx < arrayNum; ++idx) {
			outStatuses[idx].ownerId = targetNpIds[idx];
			outStatuses[idx].slotId = slotId;
		}

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_MULTI_USER_DATA_STATUS, env,
				       &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiUserListXml(
					&reqXml, env, targetNpIds, userQueryMode, slotId, "data",
					static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *dataList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						listNodes = ScebXCe_1FA24DC0(respDoc, storageNode,
									     "list");
						if (listNodes != NULL && listNodes->count != 0) {
							BxceNodeId listElem = ScebXCe_6C46D786(
								listNodes, 0);
							dataList = ScebXCe_1FA24DC0(
								respDoc, listElem, "data");
							if (dataList != NULL &&
							    dataList->count != 0) {
								SceUInt32 count = dataList->count;
								ret = static_cast<int>(count);
								for (SceUInt32 dataIdx = 0;
								     dataIdx < count; ++dataIdx) {
									BxceNodeId dataNode =
										ScebXCe_6C46D786(
											dataList,
											dataIdx);
									NpTusDataStatus parsed;
									sceClibMemset(&parsed, 0,
										      sizeof(parsed));
									parsed.hasData = 1;

									int parseRet;
									if (userQueryMode != 0) {
										parseRet = npCommunityXmlParseShortString(
											respDoc, dataNode,
											"online-id",
											parsed.ownerId.handle.data);
									} else {
										parseRet = npCommunityXmlParseJidChild(
											respDoc, dataNode,
											"jid",
											&parsed.ownerId);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildUint(
											respDoc, dataNode,
											"size",
											&parsed.dataSize);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseTusDataInfo(
											respDoc, dataNode,
											"info",
											&parsed.info);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildDate(
											respDoc, dataNode,
											"date",
											&parsed.lastChangedDate);
									}
									if (parseRet < 0) {
										ret = parseRet;
										break;
									}
									if (npCommunityXmlParseJidChild(
										    respDoc, dataNode,
										    "setterjid",
										    &parsed.lastChangedAuthorId) <
									    0) {
										parsed.lastChangedAuthorId =
											parsed.ownerId;
									}
									for (SceInt32 matchIdx = 0;
									     matchIdx < arrayNum;
									     ++matchIdx) {
										if (sceClibMemcmp(
											    &outStatuses[matchIdx].ownerId.handle,
											    &parsed.ownerId.handle,
											    sizeof(SceNpOnlineId)) == 0) {
											outStatuses[matchIdx] =
												parsed;
										}
									}
								}
							} else {
								ret = 0;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (dataList != NULL) {
						ScebXCe_1F59F0C4(dataList);
						ScebXCe_EC560647(dataList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81334F6A */
int npTusBackendGetFriendsDataStatus(SceInt32 txId, const NpTusRequestEnv *env, SceInt32 slotId,
				     SceInt32 includeSelf, SceUInt32 startSerialRank,
				     SceInt32 sortType, NpTusDataStatus *outStatuses,
				     SceInt32 arrayNum, SceUInt32 *outHits, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outStatuses == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL || slotId == 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outStatuses, 0,
			      sizeof(NpTusDataStatus) * static_cast<SceSize>(arrayNum));
		for (SceInt32 idx = 0; idx < arrayNum; ++idx)
			outStatuses[idx].slotId = slotId;

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_FRIEND_DATA_STATUS, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildFriendsQueryXml(&reqXml, env, slotId,
								startSerialRank, sortType,
								includeSelf, "data", arrayNum);
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *dataList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						ret = npCommunityXmlParseChildUint(
							respDoc, storageNode, "hits", outHits);
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, storageNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0) {
								BxceNodeId listElem =
									ScebXCe_6C46D786(listNodes,
											 0);
								dataList = ScebXCe_1FA24DC0(
									respDoc, listElem, "data");
								if (dataList != NULL &&
								    dataList->count != 0) {
									SceUInt32 count =
										dataList->count;
									ret = static_cast<int>(count);
									for (SceUInt32 dataIdx = 0;
									     dataIdx < count;
									     ++dataIdx) {
										BxceNodeId dataNode =
											ScebXCe_6C46D786(
												dataList,
												dataIdx);
										NpTusDataStatus parsed;
										sceClibMemset(
											&parsed, 0,
											sizeof(parsed));
										parsed.hasData = 1;

										int parseRet =
											npCommunityXmlParseJidChild(
												respDoc, dataNode,
												"jid",
												&parsed.ownerId);
										if (parseRet >= 0) {
											parseRet = npCommunityXmlParseChildUint(
												respDoc, dataNode,
												"size",
												&parsed.dataSize);
										}
										if (parseRet >= 0) {
											parseRet = npCommunityXmlParseTusDataInfo(
												respDoc, dataNode,
												"info",
												&parsed.info);
										}
										if (parseRet >= 0) {
											parseRet = npCommunityXmlParseChildDate(
												respDoc, dataNode,
												"date",
												&parsed.lastChangedDate);
										}
										if (parseRet < 0) {
											ret = parseRet;
											break;
										}
										if (npCommunityXmlParseJidChild(
											    respDoc, dataNode,
											    "setterjid",
											    &parsed.lastChangedAuthorId) <
										    0) {
											parsed.lastChangedAuthorId =
												parsed.ownerId;
										}
										outStatuses[dataIdx] =
											parsed;
									}
								} else {
									ret = 0;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (dataList != NULL) {
						ScebXCe_1F59F0C4(dataList);
						ScebXCe_EC560647(dataList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8133535C */
int npTusBackendGetMultiSlotDataStatus(SceInt32 txId, const NpTusRequestEnv *env,
				       const SceNpId *targetNpId, SceInt32 isVirtualUser,
				       const SceInt32 *slotIds, NpTusDataStatus *outStatuses,
				       SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || slotIds == NULL || outStatuses == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		sceClibMemset(outStatuses, 0,
			      sizeof(NpTusDataStatus) * static_cast<SceSize>(arrayNum));
		for (SceInt32 idx = 0; idx < arrayNum; ++idx)
			outStatuses[idx].slotId = slotIds[idx];

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_GET_MULTI_SLOT_DATA_STATUS, env,
				       &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiSlotListXml(
					&reqXml, env, targetNpId, isVirtualUser, slotIds, "data",
					static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *dataList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						listNodes = ScebXCe_1FA24DC0(respDoc, storageNode,
									     "list");
						if (listNodes != NULL && listNodes->count != 0) {
							BxceNodeId listElem = ScebXCe_6C46D786(
								listNodes, 0);
							dataList = ScebXCe_1FA24DC0(
								respDoc, listElem, "data");
							if (dataList != NULL &&
							    dataList->count != 0) {
								SceUInt32 count = dataList->count;
								ret = static_cast<int>(count);
								for (SceUInt32 dataIdx = 0;
								     dataIdx < count; ++dataIdx) {
									BxceNodeId dataNode =
										ScebXCe_6C46D786(
											dataList,
											dataIdx);
									NpTusDataStatus parsed;
									sceClibMemset(&parsed, 0,
										      sizeof(parsed));
									parsed.ownerId = *targetNpId;
									parsed.hasData = 1;

									SceInt32 parsedSlot = 0;
									int parseRet =
										npCommunityXmlParseIntAttr(
											respDoc,
											dataNode,
											"slot",
											&parsedSlot);
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildUint(
											respDoc, dataNode,
											"size",
											&parsed.dataSize);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseTusDataInfo(
											respDoc, dataNode,
											"info",
											&parsed.info);
									}
									if (parseRet >= 0) {
										parseRet = npCommunityXmlParseChildDate(
											respDoc, dataNode,
											"date",
											&parsed.lastChangedDate);
									}
									if (parseRet < 0) {
										ret = parseRet;
										break;
									}
									if (npCommunityXmlParseJidChild(
										    respDoc, dataNode,
										    "setterjid",
										    &parsed.lastChangedAuthorId) <
									    0) {
										parsed.lastChangedAuthorId =
											parsed.ownerId;
									}
									for (SceInt32 matchIdx = 0;
									     matchIdx < arrayNum;
									     ++matchIdx) {
										if (slotIds[matchIdx] ==
										    parsedSlot) {
											outStatuses[matchIdx] =
												parsed;
										}
									}
								}
							} else {
								ret = 0;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (dataList != NULL) {
						ScebXCe_1F59F0C4(dataList);
						ScebXCe_EC560647(dataList);
					}
					if (listNodes != NULL) {
						ScebXCe_1F59F0C4(listNodes);
						ScebXCe_EC560647(listNodes);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81335790 */
int npTusBackendTryAndSetVariable(SceInt32 txId, const NpTusRequestEnv *env,
				  const SceNpId *targetNpId, SceInt32 isVirtualUser,
				  SceInt32 slotId, SceInt64 inVariable,
				  const SceInt64 *compareValue,
				  const SceNpId *isLastChangedAuthor,
				  const SceRtcTick *isLastChangedDate, NpTusVariable *outVariable,
				  SceInt32 opeType, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || outVariable == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		outVariable->ownerId = *targetNpId;

		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_TRY_AND_SET_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildTryAndSetVarXml(
					&reqXml, env, targetNpId, isVirtualUser, slotId, inVariable,
					compareValue, isLastChangedAuthor, isLastChangedDate,
					opeType);
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *storageList = ScebXCe_1FA24DC0(
						respDoc, root, "npstorage");
					BxceNodeList *varList = NULL;

					if (storageList != NULL) {
						BxceNodeId storageNode = ScebXCe_6C46D786(
							storageList, 0);
						varList = ScebXCe_1FA24DC0(respDoc, storageNode,
									   "variable");
						if (varList != NULL && varList->count != 0) {
							BxceNodeId varNode = ScebXCe_6C46D786(
								varList, 0);
							if (npCommunityXmlParseInt64Attr(
								    respDoc, varNode, "value",
								    &outVariable->variable) >= 0) {
								if (npCommunityXmlParseInt64Attr(
									    respDoc, varNode, "old",
									    &outVariable->oldVariable) <
								    0) {
									outVariable->oldVariable = 0;
								}
								ret = npCommunityXmlParseChildDate(
									respDoc, varNode, "date",
									&outVariable->lastChangedDate);
								if (ret >= 0) {
									if (npCommunityXmlParseJidChild(
										    respDoc, varNode,
										    "setterjid",
										    &outVariable->lastChangedAuthorId) <
									    0) {
										outVariable->lastChangedAuthorId =
											*targetNpId;
									}
									outVariable->hasData = 1;
									ret = 0;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (storageList != NULL) {
						ScebXCe_1F59F0C4(storageList);
						ScebXCe_EC560647(storageList);
					}
					if (varList != NULL) {
						ScebXCe_1F59F0C4(varList);
						ScebXCe_EC560647(varList);
					}
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81335A9C */
int npTusBackendSetData(SceInt32 txId, const NpTusRequestEnv *env, const SceNpId *targetNpId,
			SceInt32 isVirtualUser, SceInt32 slotId, SceSize totalSize,
			const void *chunkData, SceSize sendSize, const NpTusDataInfo *dataInfo,
			const SceNpId *isLastChangedAuthor, const SceRtcTick *isLastChangedDate,
			void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	NpCommunityChunkedTransfer *xfer = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			xfer = static_cast<NpCommunityChunkedTransfer *>(tx->serviceParam);
			if (xfer == NULL) {
				if (totalSize == 0) {
					ret = NP_COMMUNITY_ERR_INVALID_ARG;
				} else {
					xfer = static_cast<NpCommunityChunkedTransfer *>(
						npCommunityAlloc(sizeof(*xfer)));
					tx->serviceParam = xfer;
					if (xfer == NULL) {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					} else {
						sceClibMemset(xfer, 0, sizeof(*xfer));
						xfer->state = NP_COMMUNITY_XFER_IDLE;
						xfer->totalSize = totalSize;
						xfer->remainingSize = totalSize;
						ret = npCommunityTrackTxAlloc(tx, xfer);
						if (ret < 0) {
							npCommunityFree(xfer);
							tx->serviceParam = NULL;
							xfer = NULL;
						}
					}
				}
			}
		}

		if (ret >= 0 && xfer != NULL) {
			if (xfer->state == NP_COMMUNITY_XFER_FAILED) {
				ret = xfer->lastError;
			} else {
				if (xfer->state == NP_COMMUNITY_XFER_IDLE) {
					ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_TUS_SET_DATA,
								  env->hostName, 0);
					if (ret >= 0) {
						if (tx->serviceType != NP_COMMUNITY_TX_TUS) {
							ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
						} else {
							ret = npTusBuildSetDataXml(
								&reqXml, env, targetNpId,
								isVirtualUser, slotId, "data",
								dataInfo->bytes,
								isLastChangedAuthor,
								isLastChangedDate,
								dataInfo->infoSize, totalSize);
						}
					}
					if (ret >= 0 && reqXml == NULL)
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					if (ret >= 0) {
						SceSize xmlOnlyLen = sceClibStrnlen(reqXml,
										    0x7FFFFFFFu);
						npCommunityHttpAddInfoLengthHeader(tx->httpReqId,
										   xmlOnlyLen);
						npTusAddAnticheatHeaders(tx->httpReqId, reqXml,
									 env->passphrase);
						SceSize sepLen = sceClibStrnlen(
							NP_TUS_BODY_SEPARATOR, 0x7FFFFFFFu);
						sceClibStrncat(reqXml, NP_TUS_BODY_SEPARATOR,
							       sepLen);
						SceSize headerLen = sceClibStrnlen(reqXml,
										   0x7FFFFFFFu);
						sceHttpSetRequestContentLength(
							tx->httpReqId,
							static_cast<SceUInt64>(headerLen) +
								xfer->totalSize);
						ret = npCommunityHttpSend(tx, reqXml, headerLen,
									  NULL);
						if (ret >= 0) {
							npCommunityFree(reqXml);
							reqXml = NULL;
							xfer->state = NP_COMMUNITY_XFER_ACTIVE;
						}
					}
				}

				SceSize sentNow = 0;
				if (ret >= 0 && xfer->state == NP_COMMUNITY_XFER_ACTIVE &&
				    sendSize != 0) {
					SceSize toSend = sendSize;
					if (xfer->remainingSize < toSend)
						toSend = xfer->remainingSize;
					ret = npCommunityHttpSend(tx, chunkData, toSend, &sentNow);
					if (ret >= 0) {
						if (sentNow != toSend) {
							ret = NP_COMMUNITY_ERR_HTTP_STATUS;
						} else {
							xfer->remainingSize -= sentNow;
						}
					}
				}

				if (ret >= 0) {
					if (xfer->remainingSize == 0) {
						ret = npCommunityHttpRecvXml(
							tx, &respDoc, NP_COMMUNITY_SIG_NONE, NULL);
						if (ret >= 0) {
							ret = npCommunityXmlCheckResult(
								respDoc, "npstorage");
						}
						if (ret >= 0) {
							npCommunityCloseTxHttp(tx);
							xfer->state = NP_COMMUNITY_XFER_COMPLETE;
							ret = static_cast<int>(sentNow);
						}
					} else {
						ret = static_cast<int>(sentNow);
					}
				}
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81335D5E */
int npTusBackendGetData(SceInt32 txId, const NpTusRequestEnv *env, const SceNpId *targetNpId,
			SceInt32 isVirtualUser, SceInt32 slotId, NpTusDataStatus *outStatus,
			void *outData, SceSize recvSize, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	NpCommunityChunkedTransfer *xfer = NULL;
	char *reqXml = NULL;
	SceSize bytesRead = 0;
	int ret;

	if (targetNpId == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			xfer = static_cast<NpCommunityChunkedTransfer *>(tx->serviceParam);
			if (xfer == NULL) {
				xfer = static_cast<NpCommunityChunkedTransfer *>(
					npCommunityAlloc(sizeof(*xfer)));
				tx->serviceParam = xfer;
				if (xfer == NULL) {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				} else {
					sceClibMemset(xfer, 0, sizeof(*xfer));
					xfer->state = NP_COMMUNITY_XFER_IDLE;
					xfer->totalSize = outStatus->dataSize;
					xfer->remainingSize = outStatus->dataSize;
					ret = npCommunityTrackTxAlloc(tx, xfer);
					if (ret < 0) {
						npCommunityFree(xfer);
						tx->serviceParam = NULL;
						xfer = NULL;
					}
				}
			}
		}

		if (ret >= 0 && xfer != NULL) {
			if (xfer->state == NP_COMMUNITY_XFER_FAILED) {
				ret = xfer->lastError;
			} else if (xfer->state == NP_COMMUNITY_XFER_COMPLETE) {
				outStatus->dataSize = xfer->totalSize;
				ret = 0;
			} else {
				BxceNodeList *storageList = NULL;
				BxceNodeList *dataList = NULL;

				if (xfer->state == NP_COMMUNITY_XFER_IDLE) {
					ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_TUS_GET_DATA,
								  env->hostName, 0);
					if (ret >= 0) {
						if (tx->serviceType != NP_COMMUNITY_TX_TUS) {
							ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
						} else {
							ret = npTusBuildGetDataXml(
								&reqXml, env, targetNpId,
								isVirtualUser, slotId, "data");
						}
					}
					SceUInt64 remaining = 0;
					if (ret >= 0) {
						ret = npTusSendXmlRequest(tx, env, &reqXml,
									  &respDoc, &remaining);
					}
					if (ret >= 0) {
						BxceNodeId root = ScebXCe_CED6BECF(respDoc);
						storageList = ScebXCe_1FA24DC0(respDoc, root,
									       "npstorage");
						if (storageList != NULL) {
							BxceNodeId storageNode = ScebXCe_6C46D786(
								storageList, 0);
							dataList = ScebXCe_1FA24DC0(
								respDoc, storageNode, "data");
							if (dataList != NULL) {
								BxceNodeId dataNode =
									ScebXCe_6C46D786(dataList,
											 0);
								outStatus->hasData = 1;
								outStatus->ownerId = *targetNpId;
								ret = npCommunityXmlParseChildUint(
									respDoc, dataNode, "size",
									&outStatus->dataSize);
								if (ret >= 0) {
									ret = npCommunityXmlParseTusDataInfo(
										respDoc, dataNode,
										"info",
										&outStatus->info);
								}
								if (ret >= 0) {
									ret = npCommunityXmlParseChildDate(
										respDoc, dataNode,
										"date",
										&outStatus->lastChangedDate);
								}
								if (ret >= 0) {
									if (npCommunityXmlParseJidChild(
										    respDoc, dataNode,
										    "setterjid",
										    &outStatus->lastChangedAuthorId) <
									    0) {
										outStatus->lastChangedAuthorId =
											outStatus->ownerId;
									}
									SceSize sepLen = sceClibStrnlen(
										NP_TUS_BODY_SEPARATOR,
										0x7FFFFFFFu);
									ret = npCommunityHttpExpectBytes(
										tx,
										NP_TUS_BODY_SEPARATOR,
										sepLen);
									if (ret >= 0) {
										remaining -= sepLen;
										xfer->totalSize =
											static_cast<SceSize>(
												remaining);
										xfer->remainingSize =
											static_cast<SceSize>(
												remaining);
										xfer->state =
											NP_COMMUNITY_XFER_ACTIVE;
									}
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					}
				}

				if (ret >= 0 && xfer->state == NP_COMMUNITY_XFER_ACTIVE &&
				    recvSize != 0) {
					SceSize toRecv = recvSize;
					if (xfer->remainingSize < toRecv)
						toRecv = xfer->remainingSize;
					if (outData == NULL) {
						ret = NP_COMMUNITY_ERR_NULL_POINTER;
					} else {
						ret = npCommunityHttpReadExact(tx, outData, toRecv,
									       &bytesRead);
						if (ret >= 0) {
							if (bytesRead != toRecv) {
								ret = NP_COMMUNITY_ERR_HTTP_STATUS;
							} else {
								xfer->remainingSize -= bytesRead;
							}
						}
					}
				}

				if (ret >= 0) {
					if (xfer->remainingSize == 0) {
						npCommunityCloseTxHttp(tx);
						xfer->state = NP_COMMUNITY_XFER_COMPLETE;
					}
					if (outStatus != NULL)
						outStatus->dataSize = xfer->totalSize;
					ret = static_cast<int>(bytesRead);
				}

				if (storageList != NULL) {
					ScebXCe_1F59F0C4(storageList);
					ScebXCe_EC560647(storageList);
				}
				if (dataList != NULL) {
					ScebXCe_1F59F0C4(dataList);
					ScebXCe_EC560647(dataList);
				}
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		if (ret < 0 && xfer != NULL) {
			xfer->lastError = ret;
			xfer->state = NP_COMMUNITY_XFER_FAILED;
		}
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x813361F6 */
int npTusBackendDeleteMultiSlotVariable(SceInt32 txId, const NpTusRequestEnv *env,
					const SceNpId *targetNpId, SceInt32 isVirtualUser,
					const SceInt32 *slotIds, SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || slotIds == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_VAR, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiSlotListXml(
					&reqXml, env, targetNpId, isVirtualUser, slotIds,
					"variable", static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					ret = 0;
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8133635E */
int npTusBackendDeleteMultiSlotData(SceInt32 txId, const NpTusRequestEnv *env,
				    const SceNpId *targetNpId, SceInt32 isVirtualUser,
				    const SceInt32 *slotIds, SceInt32 arrayNum, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (targetNpId == NULL || slotIds == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (arrayNum == 0 || option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npTusOpenRequest(txId, NP_COMMUNITY_EP_TUS_DELETE_MULTI_SLOT_DATA, env, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_TUS) {
				ret = npTusBuildMultiSlotListXml(
					&reqXml, env, targetNpId, isVirtualUser, slotIds, "data",
					static_cast<SceUInt32>(arrayNum));
				if (ret >= 0)
					ret = npTusSendXmlRequest(tx, env, &reqXml, &respDoc, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					ret = 0;
				}
			} else {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81337E78 */
int npTssBackendGetData(SceInt32 txId, const SceNpCommunicationId *commId, SceInt32 slotId,
			NpTssDataStatus *outStatus, void *outData, SceSize maxSize,
			SceSize recvSize, SceInt32 isSmallStorage,
			const NpTssGetDataOptParam *option)
{
	(void)isSmallStorage;
	NpCommunityBackendTx *tx = NULL;
	NpCommunityChunkedTransfer *xfer = NULL;
	SceSize bytesRead = 0;
	int ret;

	if (commId == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (slotId < 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			xfer = static_cast<NpCommunityChunkedTransfer *>(tx->serviceParam);
			if (xfer == NULL) {
				if (tx->serviceType != NP_COMMUNITY_TX_TUS &&
				    tx->serviceType != NP_COMMUNITY_TX_TSS) {
					ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
				} else {
					tx->serviceType = NP_COMMUNITY_TX_TSS;
					xfer = static_cast<NpCommunityChunkedTransfer *>(
						npCommunityAlloc(sizeof(*xfer)));
					tx->serviceParam = xfer;
					if (xfer == NULL) {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					} else {
						sceClibMemset(xfer, 0, sizeof(*xfer));
						xfer->state = NP_COMMUNITY_XFER_IDLE;
						ret = npCommunityTrackTxAlloc(tx, xfer);
						if (ret < 0) {
							npCommunityFree(xfer);
							tx->serviceParam = NULL;
							xfer = NULL;
						} else {
							char commIdStr[16];
							sceClibMemset(commIdStr, 0, 13);
							ret = SceNpCommon_E426FEE8(commId,
										   commIdStr, 13);
							if (ret >= 0) {
								SceNpManager_9554802B(
									"NpTss", commIdStr, NULL);
							}
						}
					}
				}
			}
		}

		if (ret >= 0 && xfer != NULL) {
			if (xfer->state == NP_COMMUNITY_XFER_FAILED) {
				ret = xfer->lastError;
			} else if (xfer->state == NP_COMMUNITY_XFER_COMPLETE) {
				if (outStatus != NULL)
					outStatus->contentLength = xfer->totalSize;
				ret = 0;
			} else {
				if (xfer->state == NP_COMMUNITY_XFER_IDLE) {
					NpCommunityTssUrlParam urlParam;
					urlParam.commId = commId;
					urlParam.slotId = slotId;

					ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_TSS_GET_DATA,
								  &urlParam, 0);
					if (ret >= 0 && option != NULL) {
						if (option->offset != NULL ||
						    option->lastByte != NULL) {
							const SceUInt64 *startPtr =
								reinterpret_cast<const SceUInt64 *>(
									option->offset);
							const SceUInt64 *endPtr =
								reinterpret_cast<const SceUInt64 *>(
									option->lastByte);
							npCommunityHttpAddRangeHeader(
								tx->httpReqId, startPtr, endPtr);
						}
						if (option->ifParam != NULL) {
							const char *hdrName = NULL;
							if (option->ifParam->ifType == 0)
								hdrName = "If-Modified-Since";
							else if (option->ifParam->ifType == 1)
								hdrName = "If-Range";
							else
								ret = NP_COMMUNITY_ERR_INVALID_ARG;
							if (ret >= 0) {
								npCommunityHttpAddDateHeader(
									tx->httpReqId, hdrName,
									&option->ifParam->lastModified);
							}
						}
					}
					if (ret >= 0)
						ret = npCommunityHttpSend(tx, NULL, 0, NULL);
					SceUInt64 respLen = 0;
					if (ret >= 0) {
						ret = npCommunityHttpRecvXml(
							tx, NULL, NP_COMMUNITY_SIG_NONE, &respLen);
					}
					SceInt32 statusCode = 0;
					if (ret >= 0)
						ret = sceHttpGetStatusCode(tx->httpReqId,
									   &statusCode);
					if (ret >= 0) {
						SceUInt64 effectiveLen = 0;
						switch (statusCode) {
						case 200:
							effectiveLen = respLen;
							break;
						case 206:
							outStatus->statusCodeType = 1;
							effectiveLen = respLen;
							break;
						case 304:
							outStatus->statusCodeType = 2;
							effectiveLen = 0;
							respLen = 0;
							break;
						case 403:
							SceNpManager_9554802B(
								"NpTss",
								"IP address may be forbidden",
								NULL);
							effectiveLen = 0;
							respLen = 0;
							break;
						default:
							effectiveLen = 0;
							respLen = 0;
							break;
						}

						SceUInt64 limitLen = maxSize;
						if (effectiveLen > limitLen ||
						    (option != NULL && option->offset != NULL &&
						     statusCode == 206 &&
						     static_cast<SceUInt64>(*option->offset) +
								     effectiveLen >
							     limitLen)) {
							ret = NP_COMMUNITY_ERR_RESPONSE_TOO_LARGE;
						} else {
							xfer->totalSize =
								static_cast<SceSize>(effectiveLen);
							xfer->remainingSize =
								static_cast<SceSize>(respLen);
							outStatus->contentLength =
								static_cast<SceSize>(respLen);
							npCommunityHttpParseDateHeader(
								tx->httpReqId, "Last-Modified",
								&outStatus->lastModified);
							xfer->state = NP_COMMUNITY_XFER_ACTIVE;
						}
					}
				}

				if (ret >= 0 && xfer->state == NP_COMMUNITY_XFER_ACTIVE &&
				    recvSize != 0) {
					SceSize toRecv = recvSize;
					if (xfer->remainingSize < toRecv)
						toRecv = xfer->remainingSize;
					if (outData == NULL) {
						ret = NP_COMMUNITY_ERR_NULL_POINTER;
					} else {
						ret = npCommunityHttpReadExact(tx, outData, toRecv,
									       &bytesRead);
						if (ret >= 0) {
							if (bytesRead != toRecv) {
								ret = NP_COMMUNITY_ERR_HTTP_STATUS;
							} else {
								xfer->remainingSize -= bytesRead;
							}
						}
					}
				}

				if (ret >= 0) {
					if (xfer->remainingSize == 0) {
						npCommunityCloseTxHttp(tx);
						xfer->state = NP_COMMUNITY_XFER_COMPLETE;
					}
					if (outStatus != NULL)
						outStatus->contentLength = xfer->totalSize;
					ret = static_cast<int>(bytesRead);
				}
			}
		}
	}

	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		if (ret < 0 && xfer != NULL) {
			xfer->lastError = ret;
			xfer->state = NP_COMMUNITY_XFER_FAILED;
		}
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}
