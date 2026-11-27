/*
 * np_score_backend.cpp: SceNpScore HTTP/XML backend implementation
 * (0x813304C4..0x81333A27).
 */
#include "np_community_backend.h"

#include <libkernel/libkernel.h>
extern "C" {
#include <kernel/openpsid.h>
}
#include <libhttp.h>
#include <vshbridge/vshbridge.h>

extern "C" {
extern SceUInt16 sceNetHtons(SceUInt16 host16);
extern SceUInt32 sceNetHtonl(SceUInt32 host32);
extern SceUInt64 sceNetHtonll(SceUInt64 host64);
}

static const char NP_SCORE_XML_PROLOG[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
static const char NP_SCORE_BODY_SEPARATOR[] = "\r\n\r\n";

/* 24-byte Score anticheat HMAC key prefix (0x81546830) */
static const SceUInt8 s_scoreAnticheatKeyPrefix[24] = {
	0xB0, 0xD1, 0xF1, 0x63, 0x53, 0x7C, 0xFC, 0x93,
	0x20, 0x7E, 0x22, 0x6B, 0x67, 0x99, 0xCD, 0xD9,
	0x5E, 0x27, 0x91, 0x76, 0xF8, 0x56, 0x90, 0xD4
};

#define NP_SCORE_ANTICHEAT_MAGIC	0x329B02ADu
#define NP_SCORE_ANTICHEAT_FORMAT	0x00002001u
#define NP_SCORE_ANTICHEAT_FW_VERSION	0x03650011u
#define NP_SCORE_PSCODE_DEVKIT		258u
#define NP_SCORE_PSCODE_TESTKIT		257u

/* Product code structure returned by vshSblAimgrGetPscode (8 bytes) */
struct NpScoreAimgrPscode {
	SceUInt16 companyCode;
	SceUInt16 productCode;
	SceUInt16 productSubCode;
	SceUInt16 factoryCode;
};

/* 432-byte HMAC-SHA1 key material for Score anticheat token */
struct NpScoreAnticheatKey {
	SceUInt8 fixedPrefix[24];
	char gameInfoBase64[256];
	SceUInt8 passphrase[128];
	SceKernelOpenPsId openPsId;
	SceUInt64 scoreValueBe;
};

/* 92-byte signed body inside Score anticheat packet */
struct NpScoreAnticheatBody {
	SceUInt32 formatBe;
	SceUInt32 fwVersionBe;
	SceUInt32 flagsBe;
	SceUInt32 bodySizeBe;
	SceNpOnlineId onlineId;
	SceUInt32 boardIdBe;
	SceKernelOpenPsId openPsId;
	SceUInt64 secureTickBe;
	SceUInt32 productCodeBe;
	SceUInt8 trailingZero[24];
};

/* 120-byte Score anticheat payload (base64-encoded into <anticheat>) */
struct NpScoreAnticheatPacket {
	SceUInt32 magicBe;
	SceUInt32 packetSizeBe;
	SceUInt8 hmacDigest[20];
	NpScoreAnticheatBody body;
};

static char *npScoreWrapXmlString(const char *serialized, SceSize extraReserve)
{
	SceSize prologLen = sceClibStrnlen(NP_SCORE_XML_PROLOG, 0x7FFFFFFFu);
	SceSize bodyLen = sceClibStrnlen(serialized, 0x7FFFFFFFu);
	SceSize allocSize = prologLen + bodyLen + extraReserve + 1;
	char *outXml = static_cast<char *>(npCommunityAlloc(allocSize));
	if (outXml != NULL) {
		sceClibStrlcpy(outXml, NP_SCORE_XML_PROLOG, allocSize);
		sceClibStrlcat(outXml, serialized, allocSize);
	}
	return outXml;
}

/* 0x81332204 */
static int npScoreXmlParseSerialAttr(const BxceDocument *doc, BxceNodeId recordNode,
				     SceUInt32 *outSerialRank)
{
	const char *serialStr = NULL;
	if (npCommunityXmlFindAttr(&serialStr, doc, recordNode, "serial") < 0 ||
	    serialStr == NULL) {
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	}
	int parsed = SceNpCommon_CC8E6AC6(serialStr);
	if (parsed < 0)
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	*outSerialRank = static_cast<SceUInt32>(parsed);
	return 0;
}

/* 0x81332270 */
static int npScoreXmlParseChildInt64(const BxceDocument *doc, BxceNodeId parentNode,
				     const char *tagName, SceInt64 *outValue)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parentNode, tagName);
	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	if (list != NULL) {
		BxceNodeId elem = ScebXCe_6C46D786(list, 0);
		BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
		if (textNode != 0) {
			const char *text = ScebXCe_33825EFF(doc, textNode);
			if (text != NULL) {
				*outValue = npCommunityParseInt64(text);
				ret = 0;
			}
		}
		ScebXCe_1F59F0C4(list);
		ScebXCe_EC560647(list);
	}
	return ret;
}

/* 0x813322D8 */
static int npScoreBuildAnticheatToken(char **outBase64,
				      const SceNpCommunicationPassphrase *passphrase,
				      const SceNpId *selfNpId, const NpScoreGameInfo *gameInfo,
				      SceUInt32 boardId, SceInt64 scoreValue, SceUID clientPid)
{
	SceKernelOpenPsId openPsId;
	char *gameInfoEncoded = NULL;

	int ret = sceKernelGetOpenPsId(&openPsId);
	if (ret >= 0) {
		if (gameInfo != NULL && gameInfo->infoSize != 0) {
			ret = npCommunityBase64EncodeAlloc(&gameInfoEncoded, gameInfo->bytes,
							   gameInfo->infoSize);
			if (ret >= 0 && gameInfoEncoded == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}

	if (ret >= 0) {
		NpScoreAnticheatKey keyMaterial;
		sceClibMemset(&keyMaterial, 0, sizeof(keyMaterial));
		sceClibMemcpy_safe(keyMaterial.fixedPrefix, s_scoreAnticheatKeyPrefix,
				   sizeof(keyMaterial.fixedPrefix));
		if (gameInfoEncoded != NULL) {
			SceSize copyLen = sceClibStrnlen(gameInfoEncoded,
							 sizeof(keyMaterial.gameInfoBase64));
			sceClibMemcpy_safe(keyMaterial.gameInfoBase64, gameInfoEncoded, copyLen);
		}
		sceClibMemcpy_safe(keyMaterial.passphrase, passphrase,
				   sizeof(keyMaterial.passphrase));
		sceClibMemcpy_safe(&keyMaterial.openPsId, &openPsId, sizeof(openPsId));
		keyMaterial.scoreValueBe = sceNetHtonll(static_cast<SceUInt64>(scoreValue));

		NpScoreAnticheatPacket packet;
		sceClibMemset(&packet, 0, sizeof(packet));
		packet.magicBe = sceNetHtonl(NP_SCORE_ANTICHEAT_MAGIC);
		packet.packetSizeBe = sceNetHtonl(sizeof(packet));
		packet.body.formatBe = sceNetHtonl(NP_SCORE_ANTICHEAT_FORMAT);
		packet.body.fwVersionBe = sceNetHtonl(NP_SCORE_ANTICHEAT_FW_VERSION);
		packet.body.flagsBe = sceNetHtonl(0);
		packet.body.bodySizeBe = sceNetHtonl(sizeof(packet.body));
		sceClibMemcpy_safe(&packet.body.onlineId, &selfNpId->handle,
				   sizeof(packet.body.onlineId));
		packet.body.boardIdBe = sceNetHtonl(boardId);
		sceClibMemcpy_safe(&packet.body.openPsId, &openPsId, sizeof(openPsId));

		SceRtcTick secureTick;
		ret = vshRtcGetCurrentSecureTick(&secureTick);
		if (ret >= 0) {
			packet.body.secureTickBe = sceNetHtonll(secureTick.tick);

			NpScoreAimgrPscode pscode;
			ret = static_cast<int>(vshSblAimgrGetPscode(&pscode));
			if (ret >= 0) {
				int appAttr = 0;
				ret = sceAppMgrAppParamGetInt(clientPid, 13, &appAttr);
				if (ret >= 0) {
					SceUInt16 productCode = pscode.productCode;
					if ((appAttr & 2) != 0)
						productCode = NP_SCORE_PSCODE_DEVKIT;
					if ((appAttr & 0xC) != 0)
						productCode = NP_SCORE_PSCODE_TESTKIT;

					SceUInt32 netProductCode = sceNetHtons(productCode);
					sceClibMemcpy_safe(&packet.body.productCodeBe,
							   &netProductCode,
							   sizeof(netProductCode));

					npCommunityHmacSha1(&keyMaterial, sizeof(keyMaterial),
							    &packet.body, sizeof(packet.body),
							    packet.hmacDigest);
					ret = npCommunityBase64EncodeAlloc(outBase64, &packet,
									   sizeof(packet));
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}

	if (gameInfoEncoded != NULL)
		npCommunityFree(gameInfoEncoded);
	return ret;
}

/* 0x813324D2 */
static int npScoreBuildBoardInfoXml(char **outXml, const SceNpCommunicationId *commId,
				    SceUInt32 boardId)
{
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendCommId(
							doc, rankingNode, "titleid", commId);
						if (ret >= 0) {
							ret = npCommunityXmlAppendInt(
								doc, rankingNode, "board", NULL,
								static_cast<SceInt32>(boardId));
						}
						if (ret >= 0) {
							if (ScebXCe_D7FE96A2(&serializer, doc,
									     &serialized, NULL, 0) == 0 &&
							    serialized != NULL) {
								char *wrapped = npScoreWrapXmlString(
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
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8133265A */
static int npScoreXmlPopulateOptionAttrs(BxceAttributeList *attrs, const SceInt32 *wantMessage,
					 const SceInt32 *wantInfo, const SceInt32 *pcId,
					 const SceUInt32 *offset, const SceUInt32 *limit,
					 const SceInt32 *includeOwner)
{
	int ret = 0;
	if (wantMessage != NULL) {
		ScebXCe_3DD761CF(attrs, "message", (*wantMessage != 0) ? "true" : "false");
	}
	if (wantInfo != NULL) {
		ScebXCe_3DD761CF(attrs, "info", (*wantInfo != 0) ? "true" : "false");
	}
	if (offset != NULL)
		ret = npCommunityXmlAddIntAttr(attrs, "offset", static_cast<SceInt32>(*offset));
	if (ret >= 0 && pcId != NULL)
		ret = npCommunityXmlAddUint64Attr(attrs, "pc", static_cast<SceUInt64>(static_cast<SceInt64>(*pcId)));
	if (ret >= 0 && limit != NULL)
		ret = npCommunityXmlAddIntAttr(attrs, "limit", static_cast<SceInt32>(*limit));
	if (ret >= 0 && includeOwner != NULL) {
		ScebXCe_3DD761CF(attrs, "owner", (*includeOwner != 0) ? "true" : "false");
	}
	return ret;
}

/* 0x81332740 */
static int npScoreBuildRankingByRangeXml(char **outXml, const SceNpCommunicationId *commId,
					 SceUInt32 boardId, SceInt32 friendsScope,
					 SceUInt32 startRank, SceUInt32 listMax,
					 SceInt32 wantInfo, SceInt32 wantMessage)
{
	(void)friendsScope;
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			BxceAttributeList *optAttrs = NULL;
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendCommId(
							doc, rankingNode, "titleid", commId);
						if (ret >= 0) {
							ret = npCommunityXmlAppendInt(
								doc, rankingNode, "board", NULL,
								static_cast<SceInt32>(boardId));
						}
						if (ret >= 0) {
							char startBuf[12];
							sceClibMemset(startBuf, 0, sizeof(startBuf));
							npCommunityFormatInt32(
								startBuf,
								static_cast<SceInt32>(startRank));
							SceSize len = sceClibStrnlen(startBuf, 0x7FFFFFFFu);
							if (ScebXCe_AC0E4F67(doc, rankingNode, "start",
									     NULL, startBuf, len) == 0) {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
						if (ret >= 0) {
							char maxBuf[12];
							sceClibMemset(maxBuf, 0, sizeof(maxBuf));
							npCommunityFormatInt32(
								maxBuf,
								static_cast<SceInt32>(listMax));
							SceSize len = sceClibStrnlen(maxBuf, 0x7FFFFFFFu);
							if (ScebXCe_AC0E4F67(doc, rankingNode, "list-max",
									     NULL, maxBuf, len) == 0) {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
						if (ret >= 0) {
							optAttrs = static_cast<BxceAttributeList *>(
								sceNpCommonMallocNpServerName(20));
							if (optAttrs != NULL) {
								ret = ScebXCe_E7C8B20D(optAttrs);
								if (ret == 0) {
									npScoreXmlPopulateOptionAttrs(
										optAttrs, &wantMessage,
										&wantInfo, NULL, NULL,
										NULL, NULL);
									if (ScebXCe_AC0E4F67(
										    doc, rankingNode,
										    "option", optAttrs,
										    NULL, 0) != 0) {
										if (ScebXCe_D7FE96A2(
											    &serializer, doc,
											    &serialized, NULL, 0) == 0 &&
										    serialized != NULL) {
											char *wrapped = npScoreWrapXmlString(
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
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
			if (optAttrs != NULL) {
				ScebXCe_9C2F9897(optAttrs);
				sceNpCommonFreeNpServerName(optAttrs);
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813329F8 */
static int npScoreBuildRankingByNpIdXml(char **outXml, const SceNpCommunicationId *commId,
					SceUInt32 boardId, void *unusedExtra,
					const NpScoreNpIdPcId *targets, SceUInt32 arrayNum,
					SceInt32 wantInfo, SceInt32 wantMessage)
{
	(void)unusedExtra;
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			BxceAttributeList *jidAttrs = NULL;
			BxceAttributeList *optAttrs = NULL;

			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendCommId(
							doc, rankingNode, "titleid", commId);
						if (ret >= 0) {
							ret = npCommunityXmlAppendInt(
								doc, rankingNode, "board", NULL,
								static_cast<SceInt32>(boardId));
						}
						if (ret >= 0) {
							SceUInt32 failedCount = 0;
							for (SceUInt32 idx = 0; idx < arrayNum; ++idx) {
								if (targets[idx].pcId != 0) {
									jidAttrs = static_cast<BxceAttributeList *>(
										sceNpCommonMallocNpServerName(20));
									if (jidAttrs == NULL) {
										ret = NP_COMMUNITY_ERR_NO_MEMORY;
										break;
									}
									ret = ScebXCe_E7C8B20D(jidAttrs);
									if (ret != 0)
										break;
									ret = npCommunityXmlAddIntAttr(
										jidAttrs, "pc",
										targets[idx].pcId);
									if (ret < 0)
										break;
								}
								ret = npCommunityXmlAppendJid(
									doc, rankingNode, jidAttrs,
									"jid", &targets[idx].npId);
								if (ret < 0) {
									++failedCount;
									if (failedCount == arrayNum)
										break;
								}
								if (jidAttrs != NULL) {
									ScebXCe_9C2F9897(jidAttrs);
									sceNpCommonFreeNpServerName(jidAttrs);
									jidAttrs = NULL;
								}
							}
						}
						if (ret >= 0) {
							optAttrs = static_cast<BxceAttributeList *>(
								sceNpCommonMallocNpServerName(20));
							if (optAttrs != NULL) {
								ret = ScebXCe_E7C8B20D(optAttrs);
								if (ret == 0) {
									npScoreXmlPopulateOptionAttrs(
										optAttrs, &wantMessage,
										&wantInfo, NULL, NULL,
										NULL, NULL);
									if (ScebXCe_AC0E4F67(
										    doc, rankingNode,
										    "option", optAttrs,
										    NULL, 0) != 0) {
										if (ScebXCe_D7FE96A2(
											    &serializer, doc,
											    &serialized, NULL, 0) == 0 &&
										    serialized != NULL) {
											char *wrapped = npScoreWrapXmlString(
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
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
			if (jidAttrs != NULL) {
				ScebXCe_9C2F9897(jidAttrs);
				sceNpCommonFreeNpServerName(jidAttrs);
			}
			if (optAttrs != NULL) {
				ScebXCe_9C2F9897(optAttrs);
				sceNpCommonFreeNpServerName(optAttrs);
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81332CE2 */
static int npScoreBuildRecordScoreXml(char **outXml, const SceNpCommunicationId *commId,
				      SceUInt32 boardId, SceUID clientPid, SceInt32 pcId,
				      const void *ticketBuf, SceSize ticketSize,
				      const SceNpId *selfNpId, const char *onlineName,
				      const SceNpCommunicationPassphrase *passphrase,
				      SceInt64 scoreValue, const char *commentText,
				      const NpScoreGameInfo *gameInfo,
				      const SceUInt32 *outTmpRank,
				      const SceRtcTick *compareDate)
{
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendBase64(
							doc, rankingNode, "ticket", ticketBuf,
							ticketSize);
						if (ret >= 0) {
							ret = npCommunityXmlAppendJid(
								doc, rankingNode, NULL, "ticketjid",
								selfNpId);
						}
						if (ret >= 0) {
							ret = npCommunityXmlAppendOpenPsId(
								doc, rankingNode, NULL, "psid");
						}
						int appVer = 0;
						if (ret >= 0)
							ret = sceAppMgrAppParamGetInt(clientPid, 3,
										      &appVer);
						if (ret >= 0) {
							BxceAttributeList *titleAttrs = static_cast<BxceAttributeList *>(
								sceNpCommonMallocNpServerName(20));
							if (titleAttrs != NULL) {
								ret = ScebXCe_E7C8B20D(titleAttrs);
								if (ret == 0) {
									ret = npCommunityXmlAppendCommIdVersion(
										doc, rankingNode, titleAttrs,
										"titleid", commId, appVer);
								}
								if (ret >= 0) {
									ret = npCommunityXmlAppendInt(
										doc, rankingNode, "board",
										NULL,
										static_cast<SceInt32>(boardId));
								}
								if (ret >= 0) {
									ret = npCommunityXmlAppendInt(
										doc, rankingNode, "pc",
										NULL, pcId);
								}
								if (ret >= 0) {
									char scoreBuf[24];
									sceClibMemset(scoreBuf, 0, 22);
									npCommunityFormatInt64(
										scoreBuf, scoreValue);
									SceSize len = sceClibStrnlen(
										scoreBuf, 0x7FFFFFFFu);
									if (ScebXCe_AC0E4F67(
										    doc, rankingNode,
										    "score", NULL,
										    scoreBuf, len) == 0) {
										ret = NP_COMMUNITY_ERR_NO_MEMORY;
									}
								}
								if (ret >= 0) {
									ret = npCommunityXmlAppendEscapedText(
										doc, rankingNode, "name",
										onlineName);
								}
								if (ret >= 0) {
									ret = npCommunityXmlAppendBoundedText(
										doc, rankingNode, "message",
										NULL, commentText, 63);
								}
								if (ret >= 0 && gameInfo != NULL &&
								    gameInfo->infoSize != 0) {
									ret = npCommunityXmlAppendBase64Checked(
										doc, rankingNode, "info",
										gameInfo->bytes,
										gameInfo->infoSize);
								}
								if (ret >= 0) {
									BxceAttributeList *tmpRankAttrs = static_cast<BxceAttributeList *>(
										sceNpCommonMallocNpServerName(20));
									if (tmpRankAttrs != NULL) {
										ret = ScebXCe_E7C8B20D(tmpRankAttrs);
										if (ret == 0) {
											ret = npCommunityXmlAddIntAttr(
												tmpRankAttrs, "use",
												(outTmpRank != NULL) ? 1 : 0);
										}
										if (ret >= 0) {
											if (ScebXCe_AC0E4F67(
												    doc, rankingNode,
												    "temp-rank",
												    tmpRankAttrs,
												    NULL, 0) == 0) {
												ret = NP_COMMUNITY_ERR_NO_MEMORY;
											}
										}
										if (ret >= 0 &&
										    compareDate != NULL) {
											ret = npCommunityXmlAppendRtcTick(
												doc, rankingNode,
												"comp-date", NULL,
												compareDate, 1);
										}
										if (ret >= 0) {
											BxceAttributeList *acAttrs = static_cast<BxceAttributeList *>(
												sceNpCommonMallocNpServerName(20));
											if (acAttrs != NULL) {
												ret = ScebXCe_E7C8B20D(acAttrs);
												if (ret == 0) {
													char *acToken = NULL;
													ret = npScoreBuildAnticheatToken(
														&acToken, passphrase,
														selfNpId, gameInfo,
														boardId, scoreValue,
														clientPid);
													if (ret >= 0) {
														ScebXCe_3DD761CF(
															acAttrs, "pf",
															"psp2");
														SceSize acLen = sceClibStrnlen(
															acToken,
															0x7FFFFFFFu);
														if (ScebXCe_AC0E4F67(
															    doc, rankingNode,
															    "anticheat",
															    acAttrs, acToken,
															    acLen) != 0) {
															ret = 0;
														} else {
															ret = NP_COMMUNITY_ERR_NO_MEMORY;
														}
													}
													if (acToken != NULL)
														npCommunityFree(acToken);
												}
												if (ret >= 0) {
													if (ScebXCe_D7FE96A2(
														    &serializer, doc,
														    &serialized, NULL, 0) == 0 &&
													    serialized != NULL) {
														char *wrapped = npScoreWrapXmlString(
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
												ScebXCe_9C2F9897(acAttrs);
												sceNpCommonFreeNpServerName(acAttrs);
											} else {
												ret = NP_COMMUNITY_ERR_NO_MEMORY;
											}
										}
										ScebXCe_9C2F9897(tmpRankAttrs);
										sceNpCommonFreeNpServerName(tmpRankAttrs);
									} else {
										ret = NP_COMMUNITY_ERR_NO_MEMORY;
									}
								}
								ScebXCe_9C2F9897(titleAttrs);
								sceNpCommonFreeNpServerName(titleAttrs);
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813331A2 */
static int npScoreBuildRecordGameDataXml(char **outXml, const SceNpCommunicationId *commId,
					 SceUInt32 boardId, SceInt32 pcId, const void *ticketBuf,
					 SceSize ticketSize, const SceNpId *selfNpId,
					 SceInt64 scoreValue, SceSize totalSize)
{
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					ret = npCommunityXmlAppendBase64(doc, rankingNode, "ticket",
									 ticketBuf, ticketSize);
					if (ret >= 0) {
						ret = npCommunityXmlAppendJid(
							doc, rankingNode, NULL, "ticketjid",
							selfNpId);
					}
					if (ret >= 0) {
						ret = npCommunityXmlAppendCommId(
							doc, rankingNode, "titleid", commId);
					}
					if (ret >= 0) {
						ret = npCommunityXmlAppendInt(
							doc, rankingNode, "board", NULL,
							static_cast<SceInt32>(boardId));
					}
					if (ret >= 0) {
						ret = npCommunityXmlAppendInt(doc, rankingNode,
									      "pc", NULL, pcId);
					}
					if (ret >= 0) {
						char scoreBuf[24];
						sceClibMemset(scoreBuf, 0, 22);
						npCommunityFormatInt64(scoreBuf, scoreValue);
						SceSize len = sceClibStrnlen(scoreBuf, 0x7FFFFFFFu);
						if (ScebXCe_AC0E4F67(doc, rankingNode, "score",
								     NULL, scoreBuf, len) == 0) {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
					if (ret >= 0) {
						ret = npCommunityXmlAppendUint32(
							doc, rankingNode, "size", totalSize);
					}
					if (ret >= 0) {
						if (ScebXCe_D7FE96A2(&serializer, doc, &serialized,
								     NULL, 0) == 0 &&
						    serialized != NULL) {
							SceSize sepLen = sceClibStrnlen(
								NP_SCORE_BODY_SEPARATOR, 0x7FFFFFFFu);
							char *wrapped = npScoreWrapXmlString(
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
				}
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81333418 */
static int npScoreBuildGetGameDataXml(char **outXml, const SceNpCommunicationId *commId,
				      SceUInt32 boardId, SceInt32 pcId, const SceNpId *targetNpId)
{
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					ret = npCommunityXmlAppendCommId(doc, rankingNode,
									 "titleid", commId);
					if (ret >= 0) {
						ret = npCommunityXmlAppendInt(
							doc, rankingNode, "board", NULL,
							static_cast<SceInt32>(boardId));
					}
					if (ret >= 0) {
						BxceAttributeList *jidAttrs = static_cast<BxceAttributeList *>(
							sceNpCommonMallocNpServerName(20));
						if (jidAttrs != NULL) {
							ret = ScebXCe_E7C8B20D(jidAttrs);
							if (ret == 0) {
								ret = npCommunityXmlAddIntAttr(
									jidAttrs, "pc", pcId);
							}
							if (ret >= 0) {
								ret = npCommunityXmlAppendJid(
									doc, rankingNode, jidAttrs,
									"jid", targetNpId);
							}
							if (ret >= 0) {
								if (ScebXCe_D7FE96A2(
									    &serializer, doc,
									    &serialized, NULL, 0) == 0 &&
								    serialized != NULL) {
									SceSize sepLen = sceClibStrnlen(
										NP_SCORE_BODY_SEPARATOR,
										0x7FFFFFFFu);
									char *wrapped = npScoreWrapXmlString(
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
							ScebXCe_9C2F9897(jidAttrs);
							sceNpCommonFreeNpServerName(jidAttrs);
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
					}
				}
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x81333628 */
static int npScoreBuildFilterCommentXml(char **outXml, const SceNpCommunicationId *commId,
					const SceNpId *selfNpId, const char *inComment)
{
	(void)selfNpId;
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendCommId(
							doc, rankingNode, "titleid", commId);
						if (ret >= 0) {
							ret = npCommunityXmlAppendBoundedText(
								doc, rankingNode, "message", NULL,
								inComment, 255);
						}
						if (ret >= 0) {
							if (ScebXCe_D7FE96A2(&serializer, doc,
									     &serialized, NULL, 0) == 0 &&
							    serialized != NULL) {
								char *wrapped = npScoreWrapXmlString(
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
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813337B2 */
static int npScoreBuildFriendsRankingXml(char **outXml, SceInt32 pcIdUnused,
					 const void *ticketBuf, SceSize ticketSize,
					 const SceNpId *selfNpId,
					 const SceNpCommunicationId *commId, SceUInt32 boardId,
					 SceInt32 pcId, SceUInt32 startSerialRank,
					 SceUInt32 arrayNum, SceInt32 wantInfo,
					 SceInt32 wantMessage, SceInt32 includeSelf)
{
	(void)pcIdUnused;
	BxceSerializer serializer;
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	ScebXCe_8EA5A077(&serializer);
	BxceDocument *doc = static_cast<BxceDocument *>(sceNpCommonMallocNpServerName(1124));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(20));
			BxceAttributeList *optAttrs = NULL;
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId rankingNode = ScebXCe_AC0E4F67(
						doc, docRoot, "ranking", rootAttrs, NULL, 0);
					if (rankingNode != 0) {
						ret = npCommunityXmlAppendBase64(
							doc, rankingNode, "ticket", ticketBuf,
							ticketSize);
						if (ret >= 0) {
							ret = npCommunityXmlAppendJid(
								doc, rankingNode, NULL, "ticketjid",
								selfNpId);
						}
						if (ret >= 0) {
							ret = npCommunityXmlAppendCommId(
								doc, rankingNode, "titleid", commId);
						}
						if (ret >= 0) {
							ret = npCommunityXmlAppendInt(
								doc, rankingNode, "board", NULL,
								static_cast<SceInt32>(boardId));
						}
						if (ret >= 0) {
							optAttrs = static_cast<BxceAttributeList *>(
								sceNpCommonMallocNpServerName(20));
							if (optAttrs != NULL) {
								ret = ScebXCe_E7C8B20D(optAttrs);
								if (ret == 0) {
									ret = npScoreXmlPopulateOptionAttrs(
										optAttrs, &wantMessage,
										&wantInfo, &pcId,
										&startSerialRank,
										&arrayNum, &includeSelf);
								}
								if (ret >= 0) {
									if (ScebXCe_AC0E4F67(
										    doc, rankingNode,
										    "option", optAttrs,
										    NULL, 0) != 0) {
										if (ScebXCe_D7FE96A2(
											    &serializer, doc,
											    &serialized, NULL, 0) == 0 &&
										    serialized != NULL) {
											char *wrapped = npScoreWrapXmlString(
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
				ScebXCe_9C2F9897(rootAttrs);
				sceNpCommonFreeNpServerName(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
			if (optAttrs != NULL) {
				ScebXCe_9C2F9897(optAttrs);
				sceNpCommonFreeNpServerName(optAttrs);
			}
		}
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

static int npScoreParseRecordElement(const BxceDocument *doc, BxceNodeId recordNode,
				     NpScoreRankData *outRank, bool wantComment,
				     NpScoreComment *outComment, bool wantGameInfo,
				     NpScoreGameInfo *outGameInfo)
{
	sceClibMemset(outRank, 0, sizeof(*outRank));
	int ret = npScoreXmlParseSerialAttr(doc, recordNode, &outRank->serialRank);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseJidAndPcId(doc, recordNode, &outRank->npId, &outRank->pcId);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseChildUint(doc, recordNode, "rank", &outRank->rank);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseChildUint(doc, recordNode, "max-rank", &outRank->highestRank);
	if (ret < 0)
		return ret;
	ret = npScoreXmlParseChildInt64(doc, recordNode, "score", &outRank->scoreValue);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseText(doc, recordNode, "name",
				      reinterpret_cast<char *>(outRank->onlineNameBuf), 48);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseDataFlag(doc, recordNode, &outRank->hasGameData);
	if (ret < 0)
		return ret;
	ret = npCommunityXmlParseChildDate(doc, recordNode, "date", &outRank->recordDate);
	if (ret < 0)
		return ret;
	if (wantComment) {
		sceClibMemset(outComment, 0, sizeof(*outComment));
		ret = npCommunityXmlParseText(doc, recordNode, "message",
					      outComment->utf8Comment, 63);
		if (ret < 0)
			return ret;
	}
	if (wantGameInfo) {
		sceClibMemset(outGameInfo, 0, sizeof(*outGameInfo));
		ret = npCommunityXmlParseScoreGameInfo(doc, recordNode, outGameInfo);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x813304C4 */
int npScoreBackendGetBoardInfo(SceInt32 txId, const NpScoreRequestEnv *env,
			       const SceNpCommunicationPassphrase *passphrase,
			       NpScoreBoardInfo *outBoardInfo, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->hostName == NULL || env->commId == NULL || outBoardInfo == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpenOnConn(tx, env->httpConnId,
								NP_COMMUNITY_EP_GET_BOARD_INFO,
								env->hostName, 0);
				if (ret >= 0) {
					ret = npScoreBuildBoardInfoXml(&reqXml, env->commId,
								       env->boardId);
					if (ret >= 0) {
						if (reqXml != NULL) {
							SceSize xmlLen = sceClibStrnlen(
								reqXml, 0x7FFFFFFFu);
							sceHttpSetRequestContentLength(
								tx->httpReqId, xmlLen);
							ret = npCommunityHttpSend(tx, reqXml,
										  xmlLen, NULL);
							if (ret >= 0) {
								npCommunityFree(reqXml);
								reqXml = NULL;
								ret = npCommunityHttpRecvXmlEx(
									tx, &respDoc,
									NP_COMMUNITY_SIG_RANKING_HEADER,
									NULL, passphrase);
								if (ret >= 0) {
									ret = npCommunityXmlCheckResult(
										respDoc, "ranking");
								}
								if (ret >= 0) {
									npCommunityCloseTxHttp(tx);
									BxceNodeId root = ScebXCe_CED6BECF(
										respDoc);
									BxceNodeList *rankingList =
										ScebXCe_1FA24DC0(
											respDoc, root,
											"ranking");
									if (rankingList != NULL) {
										BxceNodeId rankingNode =
											ScebXCe_6C46D786(
												rankingList, 0);
										ret = npCommunityXmlParseChildUint(
											respDoc, rankingNode,
											"rank-limit",
											&outBoardInfo->rankLimit);
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"update-option",
												&outBoardInfo->updateMode);
										}
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"sort-option",
												&outBoardInfo->sortMode);
										}
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"data-limit",
												&outBoardInfo->uploadNumLimit);
										}
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"size-limit",
												&outBoardInfo->uploadSizeLimit);
										}
										if (ret >= 0)
											ret = 0;
										ScebXCe_1F59F0C4(rankingList);
										ScebXCe_EC560647(rankingList);
									} else {
										ret = NP_COMMUNITY_ERR_PARSE_FAIL;
									}
								}
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
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

/* 0x813306DE */
int npScoreBackendRecordScore(SceInt32 txId, const NpScoreRequestEnv *env, const SceNpId *selfNpId,
			      SceInt32 pcId, const void *sessionKey,
			      const SceNpCommunicationPassphrase *passphrase,
			      const void *ticketBuf, SceSize ticketSize, SceInt64 scoreValue,
			      const NpScoreComment *comment, const NpScoreGameInfo *gameInfo,
			      SceUInt32 *outTmpRank, const SceRtcTick *compareDate, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || selfNpId == NULL ||
	    sessionKey == NULL || passphrase == NULL || ticketBuf == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpenOnConn(tx, env->httpConnId,
								NP_COMMUNITY_EP_RECORD_SCORE,
								env->hostName, 0);
				if (ret >= 0) {
					const char *commentText =
						(comment != NULL) ? comment->utf8Comment : NULL;
					ret = npScoreBuildRecordScoreXml(
						&reqXml, env->commId, env->boardId, env->clientPid,
						pcId, ticketBuf, ticketSize, selfNpId,
						static_cast<const char *>(sessionKey), passphrase,
						scoreValue, commentText, gameInfo, outTmpRank,
						compareDate);
					if (ret >= 0) {
						if (reqXml != NULL) {
							SceSize xmlLen = sceClibStrnlen(
								reqXml, 0x7FFFFFFFu);
							sceHttpSetRequestContentLength(
								tx->httpReqId, xmlLen);
							ret = npCommunityHttpSend(tx, reqXml,
										  xmlLen, NULL);
							if (ret >= 0) {
								npCommunityFree(reqXml);
								reqXml = NULL;
								ret = npCommunityHttpRecvXml(
									tx, &respDoc,
									NP_COMMUNITY_SIG_NONE, NULL);
								if (ret >= 0) {
									ret = npCommunityXmlCheckResult(
										respDoc, "ranking");
								}
								if (ret >= 0) {
									npCommunityCloseTxHttp(tx);
									BxceNodeId root = ScebXCe_CED6BECF(
										respDoc);
									BxceNodeList *rankingList =
										ScebXCe_1FA24DC0(
											respDoc, root,
											"ranking");
									if (rankingList != NULL) {
										BxceNodeId rankingNode =
											ScebXCe_6C46D786(
												rankingList, 0);
										if (outTmpRank != NULL) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"temp-rank",
												outTmpRank);
										}
										if (ret >= 0)
											ret = 0;
										ScebXCe_1F59F0C4(rankingList);
										ScebXCe_EC560647(rankingList);
									} else {
										ret = NP_COMMUNITY_ERR_PARSE_FAIL;
									}
								}
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
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

/* 0x8133090A */
int npScoreBackendRecordGameData(SceInt32 txId, const NpScoreRequestEnv *env,
				 const SceNpId *selfNpId, SceInt32 pcId, const void *ticketBuf,
				 SceSize ticketSize, SceInt64 scoreValue, SceSize totalSize,
				 SceSize sendSize, const void *chunkData, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	NpCommunityChunkedTransfer *xfer = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || selfNpId == NULL ||
	    ticketBuf == NULL || chunkData == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType != NP_COMMUNITY_TX_SCORE) {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			} else {
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
		}

		if (ret >= 0 && xfer != NULL) {
			if (xfer->state == NP_COMMUNITY_XFER_FAILED) {
				ret = xfer->lastError;
			} else if (xfer->state == NP_COMMUNITY_XFER_COMPLETE) {
				ret = NP_COMMUNITY_ERR_ALREADY_COMPLETE;
			} else {
				if (xfer->state == NP_COMMUNITY_XFER_IDLE) {
					ret = npCommunityHttpOpen(tx,
								  NP_COMMUNITY_EP_RECORD_GAME_DATA,
								  env->hostName, 0);
					if (ret >= 0) {
						ret = npScoreBuildRecordGameDataXml(
							&reqXml, env->commId, env->boardId, pcId,
							ticketBuf, ticketSize, selfNpId, scoreValue,
							xfer->totalSize);
					}
					if (ret >= 0 && reqXml == NULL)
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					if (ret >= 0) {
						SceSize xmlOnlyLen = sceClibStrnlen(reqXml,
										    0x7FFFFFFFu);
						npCommunityHttpAddInfoLengthHeader(tx->httpReqId,
										   xmlOnlyLen);
						SceSize sepLen = sceClibStrnlen(
							NP_SCORE_BODY_SEPARATOR, 0x7FFFFFFFu);
						sceClibStrncat(reqXml, NP_SCORE_BODY_SEPARATOR,
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

				if (ret >= 0 && xfer->state == NP_COMMUNITY_XFER_ACTIVE &&
				    sendSize != 0) {
					SceSize toSend = sendSize;
					if (xfer->remainingSize < toSend)
						toSend = xfer->remainingSize;
					ret = npCommunityHttpSend(tx, chunkData, toSend, NULL);
					if (ret >= 0)
						xfer->remainingSize -= toSend;
				}

				if (ret >= 0 && xfer->remainingSize == 0) {
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
					if (ret >= 0)
						ret = npCommunityXmlCheckResult(respDoc, "ranking");
					if (ret >= 0) {
						npCommunityCloseTxHttp(tx);
						xfer->state = NP_COMMUNITY_XFER_COMPLETE;
						ret = 0;
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
		if (ret < 0 && xfer != NULL) {
			xfer->lastError = ret;
			xfer->state = NP_COMMUNITY_XFER_FAILED;
		}
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x81330BFE */
int npScoreBackendGetGameData(SceInt32 txId, const NpScoreRequestEnv *env, SceInt32 pcId,
			      const SceNpCommunicationPassphrase *passphrase,
			      const SceNpId *targetNpId, SceSize *outTotalSize, SceSize recvSize,
			      void *outData, SceSize offsetHint, void *option)
{
	(void)offsetHint;
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	NpCommunityChunkedTransfer *xfer = NULL;
	char *reqXml = NULL;
	SceSize bytesRead = 0;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || targetNpId == NULL ||
	    outData == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType != NP_COMMUNITY_TX_SCORE) {
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
			} else {
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
			} else if (xfer->state == NP_COMMUNITY_XFER_COMPLETE) {
				if (outTotalSize != NULL)
					*outTotalSize = xfer->totalSize;
				ret = 0;
			} else {
				if (xfer->state == NP_COMMUNITY_XFER_IDLE) {
					ret = npCommunityHttpOpen(tx,
								  NP_COMMUNITY_EP_GET_GAME_DATA,
								  env->hostName, 0);
					if (ret >= 0) {
						ret = npScoreBuildGetGameDataXml(
							&reqXml, env->commId, env->boardId, pcId,
							targetNpId);
					}
					if (ret >= 0 && reqXml == NULL)
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					if (ret >= 0) {
						SceSize xmlLen = sceClibStrnlen(reqXml,
										0x7FFFFFFFu);
						sceHttpSetRequestContentLength(tx->httpReqId,
									       xmlLen);
						ret = npCommunityHttpSend(tx, reqXml, xmlLen, NULL);
						if (ret >= 0) {
							npCommunityFree(reqXml);
							reqXml = NULL;
							SceUInt64 remaining = 0;
							ret = npCommunityHttpRecvXmlEx(
								tx, &respDoc,
								NP_COMMUNITY_SIG_RANKING_HEADER,
								&remaining, passphrase);
							if (ret >= 0) {
								ret = npCommunityXmlCheckResult(
									respDoc, "ranking");
							}
							if (ret >= 0) {
								SceSize sepLen = sceClibStrnlen(
									NP_SCORE_BODY_SEPARATOR,
									0x7FFFFFFFu);
								ret = npCommunityHttpExpectBytes(
									tx, NP_SCORE_BODY_SEPARATOR,
									sepLen);
								if (ret >= 0) {
									remaining -= sepLen;
									xfer->totalSize = static_cast<SceSize>(
										remaining);
									xfer->remainingSize = static_cast<SceSize>(
										remaining);
									xfer->state = NP_COMMUNITY_XFER_ACTIVE;
								}
							}
						}
					}
				}

				if (ret >= 0 && xfer->state == NP_COMMUNITY_XFER_ACTIVE &&
				    recvSize != 0) {
					SceSize toRecv = recvSize;
					if (xfer->remainingSize < toRecv)
						toRecv = xfer->remainingSize;
					ret = npCommunityHttpReadExact(tx, outData, toRecv,
								       &bytesRead);
					if (ret >= 0)
						xfer->remainingSize -= toRecv;
				}

				if (ret >= 0) {
					if (xfer->remainingSize == 0) {
						npCommunityCloseTxHttp(tx);
						xfer->state = NP_COMMUNITY_XFER_COMPLETE;
					}
					if (outTotalSize != NULL)
						*outTotalSize = xfer->totalSize;
					ret = static_cast<int>(bytesRead);
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

/* 0x81330EAC */
int npScoreBackendGetRankingByNpId(SceInt32 txId, const NpScoreRequestEnv *env,
				   const SceNpCommunicationPassphrase *passphrase,
				   const NpScoreNpIdPcId *targets,
				   NpScorePlayerRankData *outRanks, void *unusedFilter,
				   void *unusedExtra, NpScoreComment *outComments,
				   NpScoreGameInfo *outGameInfos, SceSize arrayNum,
				   SceRtcTick *outSortDate, SceUInt32 *outTotalRecord,
				   void *option)
{
	(void)unusedFilter;
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || targets == NULL ||
	    outRanks == NULL || outSortDate == NULL || outTotalRecord == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL || arrayNum == 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		bool wantComment = (outComments != NULL);
		bool wantGameInfo = (outGameInfos != NULL);

		sceClibMemset(outRanks, 0, sizeof(NpScorePlayerRankData) * arrayNum);
		for (SceSize idx = 0; idx < arrayNum; ++idx) {
			outRanks[idx].rankData.npId = targets[idx].npId;
			outRanks[idx].rankData.pcId = targets[idx].pcId;
		}
		if (wantComment)
			sceClibMemset(outComments, 0, sizeof(NpScoreComment) * arrayNum);
		if (wantGameInfo)
			sceClibMemset(outGameInfos, 0, sizeof(NpScoreGameInfo) * arrayNum);

		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpenOnConn(tx, env->httpConnId,
								NP_COMMUNITY_EP_GET_PLAYER_RANK,
								env->hostName, 0);
				if (ret >= 0) {
					ret = npScoreBuildRankingByNpIdXml(
						&reqXml, env->commId, env->boardId, unusedExtra,
						targets, arrayNum, wantGameInfo ? 1 : 0,
						wantComment ? 1 : 0);
					if (ret >= 0) {
						SceSize xmlLen = sceClibStrnlen(reqXml,
										0x7FFFFFFFu);
						sceHttpSetRequestContentLength(tx->httpReqId,
									       xmlLen);
						ret = npCommunityHttpSend(tx, reqXml, xmlLen, NULL);
						if (ret >= 0) {
							npCommunityFree(reqXml);
							reqXml = NULL;
							ret = npCommunityHttpRecvXmlEx(
								tx, &respDoc,
								NP_COMMUNITY_SIG_RANKING_HEADER,
								NULL, passphrase);
							if (ret >= 0) {
								ret = npCommunityXmlCheckResult(
									respDoc, "ranking");
							}
							if (ret >= 0) {
								npCommunityCloseTxHttp(tx);
								BxceNodeId root = ScebXCe_CED6BECF(
									respDoc);
								BxceNodeList *rankingList =
									ScebXCe_1FA24DC0(
										respDoc, root,
										"ranking");
								BxceNodeList *listNodes = NULL;
								BxceNodeList *recordNodes = NULL;

								if (rankingList != NULL) {
									BxceNodeId rankingNode =
										ScebXCe_6C46D786(
											rankingList, 0);
									listNodes = ScebXCe_1FA24DC0(
										respDoc, rankingNode,
										"list");
									if (listNodes != NULL &&
									    listNodes->count != 0) {
										ret = npCommunityXmlParseChildDate(
											respDoc, rankingNode,
											"update",
											outSortDate);
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"total",
												outTotalRecord);
										}
										if (ret >= 0) {
											BxceNodeId listElem =
												ScebXCe_6C46D786(
													listNodes, 0);
											SceInt32 declaredCount = 0;
											ret = npCommunityXmlParseNumberAttr(
												respDoc, listElem,
												&declaredCount);
											if (ret >= 0) {
												recordNodes = ScebXCe_1FA24DC0(
													respDoc, listElem,
													"record");
												if (recordNodes != NULL &&
												    recordNodes->count != 0) {
													SceUInt32 count = static_cast<SceUInt32>(
														declaredCount);
													if (count != recordNodes->count)
														count = recordNodes->count;
													ret = static_cast<int>(count);
													for (SceUInt32 recIdx = 0;
													     recIdx < count; ++recIdx) {
														BxceNodeId recNode =
															ScebXCe_6C46D786(
																recordNodes,
																recIdx);
														NpScorePlayerRankData parsedRank;
														NpScoreComment parsedComment;
														NpScoreGameInfo parsedGameInfo;
														sceClibMemset(
															&parsedRank, 0,
															sizeof(parsedRank));
														parsedRank.hasData = 1;
														int parseRet =
															npScoreParseRecordElement(
																respDoc, recNode,
																&parsedRank.rankData,
																wantComment,
																&parsedComment,
																wantGameInfo,
																&parsedGameInfo);
														if (parseRet < 0) {
															ret = parseRet;
															break;
														}
														for (SceSize matchIdx = 0;
														     matchIdx < arrayNum;
														     ++matchIdx) {
															if (sceClibMemcmp(
																    &outRanks[matchIdx].rankData.npId.handle,
																    &parsedRank.rankData.npId.handle,
																    sizeof(SceNpOnlineId)) == 0 &&
															    outRanks[matchIdx].rankData.pcId ==
																    parsedRank.rankData.pcId) {
																outRanks[matchIdx] = parsedRank;
																if (wantGameInfo)
																	outGameInfos[matchIdx] = parsedGameInfo;
																if (wantComment)
																	outComments[matchIdx] = parsedComment;
																break;
															}
														}
													}
												} else {
													ret = NP_COMMUNITY_ERR_PARSE_FAIL;
												}
											}
										}
									} else {
										ret = NP_COMMUNITY_ERR_PARSE_FAIL;
									}
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}

								if (rankingList != NULL) {
									ScebXCe_1F59F0C4(rankingList);
									ScebXCe_EC560647(rankingList);
								}
								if (listNodes != NULL) {
									ScebXCe_1F59F0C4(listNodes);
									ScebXCe_EC560647(listNodes);
								}
								if (recordNodes != NULL) {
									ScebXCe_1F59F0C4(recordNodes);
									ScebXCe_EC560647(recordNodes);
								}
							}
						}
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

/* 0x813314B2 */
int npScoreBackendGetFriendsRanking(SceInt32 txId, const NpScoreRequestEnv *env,
				    const SceNpId *selfNpId, SceInt32 pcId,
				    const SceNpCommunicationPassphrase *passphrase,
				    const void *ticketBuf, SceSize ticketSize,
				    SceUInt32 startSerialRank, NpScoreRankData *outRanks,
				    NpScoreComment *outComments, NpScoreGameInfo *outGameInfos,
				    SceSize arrayNum, SceRtcTick *outSortDate,
				    SceUInt32 *outTotalRecord, SceUInt32 *outHits,
				    SceInt32 includeSelf, void *option)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || outRanks == NULL ||
	    outSortDate == NULL || outTotalRecord == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL || arrayNum == 0 || startSerialRank == 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		bool wantComment = (outComments != NULL);
		bool wantGameInfo = (outGameInfos != NULL);

		sceClibMemset(outRanks, 0, sizeof(NpScoreRankData) * arrayNum);
		if (wantComment)
			sceClibMemset(outComments, 0, sizeof(NpScoreComment) * arrayNum);
		if (wantGameInfo)
			sceClibMemset(outGameInfos, 0, sizeof(NpScoreGameInfo) * arrayNum);

		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpenOnConn(
					tx, env->httpConnId,
					NP_COMMUNITY_EP_GET_FRIEND_PLAYER_RANK, env->hostName, 0);
				if (ret >= 0) {
					ret = npScoreBuildFriendsRankingXml(
						&reqXml, pcId, ticketBuf, ticketSize, selfNpId,
						env->commId, env->boardId, pcId, startSerialRank,
						arrayNum, wantGameInfo ? 1 : 0, wantComment ? 1 : 0,
						includeSelf);
					if (ret >= 0) {
						SceSize xmlLen = sceClibStrnlen(reqXml,
										0x7FFFFFFFu);
						sceHttpSetRequestContentLength(tx->httpReqId,
									       xmlLen);
						ret = npCommunityHttpSend(tx, reqXml, xmlLen, NULL);
						if (ret >= 0) {
							npCommunityFree(reqXml);
							reqXml = NULL;
							ret = npCommunityHttpRecvXmlEx(
								tx, &respDoc,
								NP_COMMUNITY_SIG_RANKING_HEADER,
								NULL, passphrase);
							if (ret >= 0) {
								ret = npCommunityXmlCheckResult(
									respDoc, "ranking");
							}
							if (ret >= 0) {
								npCommunityCloseTxHttp(tx);
								BxceNodeId root = ScebXCe_CED6BECF(
									respDoc);
								BxceNodeList *rankingList =
									ScebXCe_1FA24DC0(
										respDoc, root,
										"ranking");
								BxceNodeList *listNodes = NULL;
								BxceNodeList *recordNodes = NULL;

								if (rankingList != NULL) {
									BxceNodeId rankingNode =
										ScebXCe_6C46D786(
											rankingList, 0);
									listNodes = ScebXCe_1FA24DC0(
										respDoc, rankingNode,
										"list");
									if (listNodes != NULL &&
									    listNodes->count != 0) {
										ret = npCommunityXmlParseChildDate(
											respDoc, rankingNode,
											"update",
											outSortDate);
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"total",
												outTotalRecord);
										}
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"hits", outHits);
										}
										if (ret >= 0) {
											BxceNodeId listElem =
												ScebXCe_6C46D786(
													listNodes, 0);
											SceInt32 declaredCount = 0;
											ret = npCommunityXmlParseNumberAttr(
												respDoc, listElem,
												&declaredCount);
											if (ret >= 0) {
												SceUInt32 count = static_cast<SceUInt32>(
													declaredCount);
												if (count != 0) {
													recordNodes = ScebXCe_1FA24DC0(
														respDoc, listElem,
														"record");
													if (recordNodes == NULL ||
													    recordNodes->count == 0) {
														ret = NP_COMMUNITY_ERR_PARSE_FAIL;
													} else if (count != recordNodes->count) {
														count = recordNodes->count;
													}
												}
												if (ret >= 0) {
													ret = static_cast<int>(count);
													for (SceUInt32 recIdx = 0;
													     recIdx < count; ++recIdx) {
														BxceNodeId recNode =
															ScebXCe_6C46D786(
																recordNodes,
																recIdx);
														NpScoreRankData parsedRank;
														NpScoreComment parsedComment;
														NpScoreGameInfo parsedGameInfo;
														int parseRet =
															npScoreParseRecordElement(
																respDoc, recNode,
																&parsedRank,
																wantComment,
																&parsedComment,
																wantGameInfo,
																&parsedGameInfo);
														if (parseRet < 0) {
															ret = parseRet;
															break;
														}
														outRanks[recIdx] = parsedRank;
														if (wantComment)
															outComments[recIdx] = parsedComment;
														if (wantGameInfo)
															outGameInfos[recIdx] = parsedGameInfo;
													}
												}
											}
										}
									} else {
										ret = NP_COMMUNITY_ERR_PARSE_FAIL;
									}
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}

								if (rankingList != NULL) {
									ScebXCe_1F59F0C4(rankingList);
									ScebXCe_EC560647(rankingList);
								}
								if (listNodes != NULL) {
									ScebXCe_1F59F0C4(listNodes);
									ScebXCe_EC560647(listNodes);
								}
								if (recordNodes != NULL) {
									ScebXCe_1F59F0C4(recordNodes);
									ScebXCe_EC560647(recordNodes);
								}
							}
						}
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

/* 0x81331A7C */
int npScoreBackendGetRankingByRange(SceInt32 txId, const NpScoreRequestEnv *env,
				    const SceNpCommunicationPassphrase *passphrase,
				    SceUInt32 startSerialRank, NpScoreRankData *outRanks,
				    void *unusedArg, SceInt32 friendsScope,
				    NpScoreComment *outComments, NpScoreGameInfo *outGameInfos,
				    SceSize arrayNum, SceRtcTick *outSortDate,
				    SceUInt32 *outTotalRecord, void *option)
{
	(void)unusedArg;
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env == NULL || env->commId == NULL || env->hostName == NULL || outRanks == NULL ||
	    outSortDate == NULL || outTotalRecord == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (option != NULL || arrayNum == 0 || startSerialRank == 0) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		bool wantComment = (outComments != NULL);
		bool wantGameInfo = (outGameInfos != NULL);

		sceClibMemset(outRanks, 0, sizeof(NpScoreRankData) * arrayNum);
		if (wantComment)
			sceClibMemset(outComments, 0, sizeof(NpScoreComment) * arrayNum);
		if (wantGameInfo)
			sceClibMemset(outGameInfos, 0, sizeof(NpScoreGameInfo) * arrayNum);

		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpenOnConn(tx, env->httpConnId,
								NP_COMMUNITY_EP_GET_RANKING,
								env->hostName, 0);
				if (ret >= 0) {
					ret = npScoreBuildRankingByRangeXml(
						&reqXml, env->commId, env->boardId, friendsScope,
						startSerialRank, arrayNum, wantGameInfo ? 1 : 0,
						wantComment ? 1 : 0);
					if (ret >= 0) {
						SceSize xmlLen = sceClibStrnlen(reqXml,
										0x7FFFFFFFu);
						sceHttpSetRequestContentLength(tx->httpReqId,
									       xmlLen);
						ret = npCommunityHttpSend(tx, reqXml, xmlLen, NULL);
						if (ret >= 0) {
							npCommunityFree(reqXml);
							reqXml = NULL;
							ret = npCommunityHttpRecvXmlEx(
								tx, &respDoc,
								NP_COMMUNITY_SIG_RANKING_HEADER,
								NULL, passphrase);
							if (ret >= 0) {
								ret = npCommunityXmlCheckResult(
									respDoc, "ranking");
							}
							if (ret >= 0) {
								npCommunityCloseTxHttp(tx);
								BxceNodeId root = ScebXCe_CED6BECF(
									respDoc);
								BxceNodeList *rankingList =
									ScebXCe_1FA24DC0(
										respDoc, root,
										"ranking");
								BxceNodeList *listNodes = NULL;
								BxceNodeList *recordNodes = NULL;

								if (rankingList != NULL) {
									BxceNodeId rankingNode =
										ScebXCe_6C46D786(
											rankingList, 0);
									listNodes = ScebXCe_1FA24DC0(
										respDoc, rankingNode,
										"list");
									if (listNodes != NULL &&
									    listNodes->count != 0) {
										ret = npCommunityXmlParseChildDate(
											respDoc, rankingNode,
											"update",
											outSortDate);
										if (ret >= 0) {
											ret = npCommunityXmlParseChildUint(
												respDoc, rankingNode,
												"total",
												outTotalRecord);
										}
										if (ret >= 0) {
											BxceNodeId listElem =
												ScebXCe_6C46D786(
													listNodes, 0);
											SceInt32 declaredCount = 0;
											ret = npCommunityXmlParseNumberAttr(
												respDoc, listElem,
												&declaredCount);
											if (ret >= 0) {
												recordNodes = ScebXCe_1FA24DC0(
													respDoc, listElem,
													"record");
												if (recordNodes != NULL &&
												    recordNodes->count != 0) {
													SceUInt32 count = static_cast<SceUInt32>(
														declaredCount);
													if (count != recordNodes->count)
														count = recordNodes->count;
													ret = static_cast<int>(count);
													for (SceUInt32 recIdx = 0;
													     recIdx < count; ++recIdx) {
														BxceNodeId recNode =
															ScebXCe_6C46D786(
																recordNodes,
																recIdx);
														NpScoreRankData parsedRank;
														NpScoreComment parsedComment;
														NpScoreGameInfo parsedGameInfo;
														int parseRet =
															npScoreParseRecordElement(
																respDoc, recNode,
																&parsedRank,
																wantComment,
																&parsedComment,
																wantGameInfo,
																&parsedGameInfo);
														if (parseRet < 0) {
															ret = parseRet;
															break;
														}
														outRanks[recIdx] = parsedRank;
														if (wantComment)
															outComments[recIdx] = parsedComment;
														if (wantGameInfo)
															outGameInfos[recIdx] = parsedGameInfo;
													}
												} else {
													ret = NP_COMMUNITY_ERR_PARSE_FAIL;
												}
											}
										}
									} else {
										ret = NP_COMMUNITY_ERR_PARSE_FAIL;
									}
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}

								if (rankingList != NULL) {
									ScebXCe_1F59F0C4(rankingList);
									ScebXCe_EC560647(rankingList);
								}
								if (listNodes != NULL) {
									ScebXCe_1F59F0C4(listNodes);
									ScebXCe_EC560647(listNodes);
								}
								if (recordNodes != NULL) {
									ScebXCe_1F59F0C4(recordNodes);
									ScebXCe_EC560647(recordNodes);
								}
							}
						}
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

/* 0x81331FF2 */
int npScoreBackendFilterComment(SceInt32 txId, const NpScoreRequestEnv *env,
				const SceNpId *selfNpId,
				const SceNpCommunicationPassphrase *passphrase,
				const char *inComment, char *outComment, SceInt32 isSanitize)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (env->commId == NULL || env->hostName == NULL || selfNpId == NULL ||
	    passphrase == NULL || inComment == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (sceClibStrnlen(inComment, 0x7FFFFFFFu) > 0xFFu) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				int endpointId = (isSanitize != 0)
							 ? NP_COMMUNITY_EP_SANITIZE_COMMENT
							 : NP_COMMUNITY_EP_CENSOR_COMMENT;
				ret = npCommunityHttpOpenOnConn(tx, env->httpConnId, endpointId,
								env->hostName, 0);
				if (ret >= 0) {
					ret = npScoreBuildFilterCommentXml(&reqXml, env->commId,
									   selfNpId, inComment);
					if (ret >= 0) {
						if (reqXml != NULL) {
							SceSize xmlLen = sceClibStrnlen(
								reqXml, 0x7FFFFFFFu);
							sceHttpSetRequestContentLength(
								tx->httpReqId, xmlLen);
							ret = npCommunityHttpSend(tx, reqXml,
										  xmlLen, NULL);
							if (ret >= 0) {
								npCommunityFree(reqXml);
								reqXml = NULL;
								ret = npCommunityHttpRecvXmlEx(
									tx, &respDoc,
									NP_COMMUNITY_SIG_RANKING_HEADER,
									NULL, passphrase);
								if (ret >= 0) {
									ret = npCommunityXmlCheckResult(
										respDoc, "ranking");
								}
								if (ret >= 0) {
									npCommunityCloseTxHttp(tx);
									if (isSanitize != 0) {
										BxceNodeId root = ScebXCe_CED6BECF(
											respDoc);
										BxceNodeList *rankingList =
											ScebXCe_1FA24DC0(
												respDoc, root,
												"ranking");
										if (rankingList != NULL) {
											BxceNodeId rankingNode =
												ScebXCe_6C46D786(
													rankingList, 0);
											SceSize maxLen = sceClibStrnlen(
												inComment, 0x7FFFFFFFu);
											ret = npCommunityXmlParseText(
												respDoc, rankingNode,
												"message", outComment,
												maxLen);
											if (ret >= 0)
												ret = 0;
											ScebXCe_1F59F0C4(rankingList);
											ScebXCe_EC560647(rankingList);
										} else {
											ret = NP_COMMUNITY_ERR_PARSE_FAIL;
										}
									} else {
										ret = 0;
									}
								}
							}
						} else {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						}
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
