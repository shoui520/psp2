/*
 * np_community_profile.cpp: NP Community profile, avatar, and panel HTTP/XML
 * operations and request XML builders (0x8132DA12..0x813304C3).
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include "np_community_backend.h"
#include "../manager/np_manager_core.h"

#define NP_XML_PROLOG		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
#define NP_BXCE_DOC_SIZE	1124U
#define NP_BXCE_ATTR_SIZE	20U

static void npProfileFreeBxceNodeList(BxceNodeList *list)
{
	if (list != NULL) {
		ScebXCe_1F59F0C4(list);
		ScebXCe_EC560647(list);
	}
}

static void npProfileFreeBxceDoc(BxceDocument *doc)
{
	if (doc != NULL) {
		ScebXCe_B0891B13(doc);
		ScebXCe_EC560647(doc);
	}
}

static void npProfileFreeOwnedBxceDoc(BxceDocument *doc)
{
	if (doc != NULL) {
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}
}

static void npProfileFreeOwnedAttrs(BxceAttributeList *attrs)
{
	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
}

static int npProfileSerializeDocWithProlog(BxceSerializer *serializer, BxceDocument *doc,
					   char **outXml)
{
	char *serialized = NULL;
	int ret = NP_COMMUNITY_ERR_NO_MEMORY;

	if (ScebXCe_D7FE96A2(serializer, doc, &serialized, NULL, 0) == 0 && serialized != NULL) {
		SceSize prologLen = sceClibStrnlen(NP_XML_PROLOG, 0x7FFFFFFFU);
		SceSize bodyLen = sceClibStrnlen(serialized, 0x7FFFFFFFU);
		SceSize totalLen = prologLen + bodyLen + 1U;
		char *xmlBuf = static_cast<char *>(npCommunityAlloc(totalLen));
		if (xmlBuf != NULL) {
			sceClibStrlcpy(xmlBuf, NP_XML_PROLOG, totalLen);
			sceClibStrlcat(xmlBuf, serialized, totalLen);
			*outXml = xmlBuf;
			ret = 0;
		}
	}

	if (serialized != NULL)
		ScebXCe_EC560647(serialized);
	return ret;
}

static int npProfileParseIntChildText(const BxceDocument *doc, BxceNodeId parent,
				      const char *tagName, SceInt32 *outValue)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL) {
			*outValue = SceNpCommon_CC8E6AC6(text);
			ret = 0;
		}
	}

	npProfileFreeBxceNodeList(list);
	return ret;
}

/* 0x8132F44C */
static int npProfileAppendAvatarOrPanelElem(BxceDocument *doc, BxceNodeId parentNode,
					    const char *tagName, SceInt32 sizeType,
					    const SceInt32 *assetIdPtr, const char *passAttrName,
					    const void *passBytes, BxceNodeId *outElemNode,
					    BxceAttributeList **outAttrs)
{
	const char *sizeStr = NULL;
	SceSize avatarTagLen = sceClibStrnlen("avatar", 0x7FFFFFFFU);
	SceSize panelTagLen = sceClibStrnlen("panel", 0x7FFFFFFFU);

	if (sceClibStrncmp(tagName, "avatar", avatarTagLen) == 0) {
		switch (sizeType) {
		case 0:
			sizeStr = "l";
			break;
		case 1:
			sizeStr = "m";
			break;
		case 2:
			sizeStr = "s";
			break;
		case 3:
			sizeStr = "n";
			break;
		default:
			return NP_COMMUNITY_ERR_INVALID_ARG;
		}
	} else if (sceClibStrncmp(tagName, "panel", panelTagLen) == 0) {
		if (sizeType != 0)
			return NP_COMMUNITY_ERR_INVALID_ARG;
		sizeStr = "s";
	} else {
		return NP_COMMUNITY_ERR_INVALID_ARG;
	}

	BxceAttributeList *attrs = static_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
	if (attrs == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	char *encodedPass = NULL;
	int ret = ScebXCe_E7C8B20D(attrs);
	if (ret == 0)
		ret = ScebXCe_3DD761CF(attrs, "size", sizeStr);
	if (ret == 0 && assetIdPtr != NULL) {
		ret = npCommunityXmlAddIntAttr(attrs, "id", *assetIdPtr);
		if (ret >= 0 && passBytes != NULL) {
			ret = npCommunityBase64EncodeAlloc(&encodedPass, passBytes, 8U);
			if (ret >= 0)
				ret = ScebXCe_3DD761CF(attrs, passAttrName, encodedPass);
		}
	}

	if (ret >= 0) {
		BxceNodeId elem = ScebXCe_AC0E4F67(doc, parentNode, tagName, attrs, NULL, 0U);
		if (elem != 0U) {
			if (encodedPass != NULL)
				npCommunityFree(encodedPass);
			*outElemNode = elem;
			*outAttrs = attrs;
			return 0;
		}
		ret = NP_COMMUNITY_ERR_NO_MEMORY;
	}

	if (encodedPass != NULL)
		npCommunityFree(encodedPass);
	npProfileFreeOwnedAttrs(attrs);
	return ret;
}

/* 0x8132F614 */
static int npProfileBuildSelfProfileXml(char **outXml, const void *ticketBuf, SceSize ticketSize,
					const char *envName, SceInt32 avatarSizeType)
{
	char *ticketBase64 = NULL;
	BxceNodeId avatarNode = 0U;
	BxceAttributeList *avatarAttrs = NULL;
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId profileNode = ScebXCe_AC0E4F67(
						doc, docRoot, "profile", rootAttrs, NULL, 0U);
					if (profileNode != 0U) {
						ret = npCommunityBase64EncodeAlloc(
							&ticketBase64, ticketBuf, ticketSize);
						if (ret >= 0) {
							SceSize b64Len = sceClibStrnlen(
								ticketBase64, 0x7FFFFFFFU);
							SceSize envLen = sceClibStrnlen(
								envName, 0x7FFFFFFFU);
							if (ScebXCe_AC0E4F67(doc, profileNode,
									     "ticket", NULL,
									     ticketBase64,
									     b64Len) != 0U &&
							    ScebXCe_AC0E4F67(doc, profileNode,
									     "env", NULL, envName,
									     envLen) != 0U) {
								ret = npProfileAppendAvatarOrPanelElem(
									doc, profileNode, "avatar",
									avatarSizeType, NULL, "pap",
									NULL, &avatarNode,
									&avatarAttrs);
								if (ret >= 0)
									ret = npProfileSerializeDocWithProlog(
										&serializer, doc,
										outXml);
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedAttrs(avatarAttrs);
	npProfileFreeOwnedBxceDoc(doc);
	if (ticketBase64 != NULL)
		npCommunityFree(ticketBase64);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8132F83A */
static int npProfileBuildGetProfileXml(char **outXml, const SceNpId *targetNpId,
				       SceInt32 avatarSizeType)
{
	char *jidStr = NULL;
	BxceNodeId avatarNode = 0U;
	BxceAttributeList *avatarAttrs = NULL;
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId profileNode = ScebXCe_AC0E4F67(
						doc, docRoot, "profile", rootAttrs, NULL, 0U);
					if (profileNode != 0U) {
						ret = npCommunityFormatNpIdToJid(&jidStr,
										 targetNpId);
						if (ret >= 0) {
							if (jidStr != NULL) {
								SceSize jidLen = sceClibStrnlen(
									jidStr, 0x7FFFFFFFU);
								if (ScebXCe_AC0E4F67(doc,
										     profileNode,
										     "jid", NULL,
										     jidStr,
										     jidLen) !=
								    0U) {
									ret = npProfileAppendAvatarOrPanelElem(
										doc, profileNode,
										"avatar",
										avatarSizeType,
										NULL, "pap", NULL,
										&avatarNode,
										&avatarAttrs);
									if (ret >= 0)
										ret = npProfileSerializeDocWithProlog(
											&serializer,
											doc,
											outXml);
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
				npProfileFreeOwnedAttrs(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedAttrs(avatarAttrs);
	npProfileFreeOwnedBxceDoc(doc);
	if (jidStr != NULL)
		npCommunityFree(jidStr);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8132FA28 */
static int npProfileBuildCategoryListXml(char **outXml, SceInt32 langCode, SceInt32 startOffset,
					 SceInt32 maxCount)
{
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId profileNode = ScebXCe_AC0E4F67(
						doc, docRoot, "profile", rootAttrs, NULL, 0U);
					if (profileNode != 0U) {
						ret = npCommunityXmlAppendInt(doc, profileNode,
									      "start", NULL,
									      startOffset);
						if (ret >= 0)
							ret = npCommunityXmlAppendInt(
								doc, profileNode, "max", NULL,
								maxCount);
						if (ret >= 0)
							ret = npCommunityXmlAppendInt(
								doc, profileNode, "lang", NULL,
								langCode);
						if (ret >= 0)
							ret = npProfileSerializeDocWithProlog(
								&serializer, doc, outXml);
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedBxceDoc(doc);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8132FBDC */
static int npProfileBuildAvatarListXml(char **outXml, const void *ticketBuf, SceSize ticketSize,
				       const SceInt32 *categoryIdPtr, SceInt32 sizeType,
				       SceInt32 startOffset, SceInt32 maxCount)
{
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			BxceAttributeList *avatarAttrs = NULL;
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					avatarAttrs = static_cast<BxceAttributeList *>(
						sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
					if (avatarAttrs != NULL) {
						ret = ScebXCe_E7C8B20D(avatarAttrs);
						if (ret == 0) {
							npCommunityXmlAddPlatformAttrs(rootAttrs);
							BxceNodeId docRoot = ScebXCe_75E1B095(doc);
							BxceNodeId profileNode = ScebXCe_AC0E4F67(
								doc, docRoot, "profile", rootAttrs,
								NULL, 0U);
							if (profileNode != 0U) {
								ret = npCommunityXmlAppendInt(
									doc, profileNode, "start",
									NULL, startOffset);
								if (ret >= 0)
									ret = npCommunityXmlAppendInt(
										doc, profileNode,
										"max", NULL,
										maxCount);
								if (ret >= 0)
									ret = npCommunityXmlAppendAvatarElement(
										doc, profileNode,
										avatarAttrs,
										"avatar", ticketBuf,
										ticketSize,
										"avatarticket",
										categoryIdPtr, NULL,
										&sizeType);
								if (ret >= 0)
									ret = npProfileSerializeDocWithProlog(
										&serializer, doc,
										outXml);
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
				npProfileFreeOwnedAttrs(avatarAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedBxceDoc(doc);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8132FDF4 */
static int npProfileBuildUpdateProfileXml(char **outXml, const void *ticketBuf, SceSize ticketSize,
					  const char *envName, const char *onlineName,
					  const SceInt32 *languages, const char *aboutMe,
					  const SceInt32 *avatarIdPtr, const void *avatarPass,
					  SceInt32 avatarSizeType, const SceInt32 *panelIdPtr,
					  const void *panelPass, SceInt32 panelSizeType)
{
	char *ticketBase64 = NULL;
	BxceNodeId avatarNode = 0U;
	BxceNodeId panelNode = 0U;
	BxceAttributeList *avatarAttrs = NULL;
	BxceAttributeList *panelAttrs = NULL;
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId profileNode = ScebXCe_AC0E4F67(
						doc, docRoot, "profile", rootAttrs, NULL, 0U);
					if (profileNode != 0U) {
						ret = npCommunityBase64EncodeAlloc(
							&ticketBase64, ticketBuf, ticketSize);
						if (ret >= 0) {
							SceSize b64Len = sceClibStrnlen(
								ticketBase64, 0x7FFFFFFFU);
							SceSize envLen = sceClibStrnlen(
								envName, 0x7FFFFFFFU);
							if (ScebXCe_AC0E4F67(doc, profileNode,
									     "ticket", NULL,
									     ticketBase64,
									     b64Len) != 0U &&
							    ScebXCe_AC0E4F67(doc, profileNode,
									     "env", NULL, envName,
									     envLen) != 0U) {
								if (onlineName != NULL)
									ret = npCommunityXmlAppendEscapedText(
										doc, profileNode,
										"onlinename",
										onlineName);
								if (ret >= 0 && languages != NULL)
									ret = npCommunityXmlAppendLanguages(
										doc, profileNode,
										languages);
								if (ret >= 0 && aboutMe != NULL)
									ret = npCommunityXmlAppendBoundedText(
										doc, profileNode,
										"aboutme", NULL,
										aboutMe, 63U);
								if (ret >= 0)
									ret = npProfileAppendAvatarOrPanelElem(
										doc, profileNode,
										"avatar",
										avatarSizeType,
										avatarIdPtr, "pap",
										avatarPass,
										&avatarNode,
										&avatarAttrs);
								if (ret >= 0 &&
								    panelIdPtr != NULL)
									ret = npProfileAppendAvatarOrPanelElem(
										doc, profileNode,
										"panel",
										panelSizeType,
										panelIdPtr, "ppp",
										panelPass,
										&panelNode,
										&panelAttrs);
								if (ret >= 0)
									ret = npProfileSerializeDocWithProlog(
										&serializer, doc,
										outXml);
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedAttrs(panelAttrs);
	npProfileFreeOwnedAttrs(avatarAttrs);
	npProfileFreeOwnedBxceDoc(doc);
	if (ticketBase64 != NULL)
		npCommunityFree(ticketBase64);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813300E2 */
static int npProfileBuildUpdateAvatarXml(char **outXml, const void *ticketBuf, SceSize ticketSize,
					 const SceNpId *selfNpId, SceInt32 avatarId,
					 SceInt32 sizeType)
{
	BxceNodeId avatarNode = 0U;
	BxceAttributeList *avatarAttrs = NULL;
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					npCommunityXmlAddPlatformAttrs(rootAttrs);
					BxceNodeId docRoot = ScebXCe_75E1B095(doc);
					BxceNodeId profileNode = ScebXCe_AC0E4F67(
						doc, docRoot, "profile", rootAttrs, NULL, 0U);
					if (profileNode != 0U) {
						ret = npCommunityXmlAppendBase64(
							doc, profileNode, "avatarticket", ticketBuf,
							ticketSize);
						if (ret >= 0)
							ret = npCommunityXmlAppendJid(
								doc, profileNode, NULL, "ticketjid",
								selfNpId);
						if (ret >= 0)
							ret = npProfileAppendAvatarOrPanelElem(
								doc, profileNode, "avatar",
								sizeType, &avatarId, NULL, NULL,
								&avatarNode, &avatarAttrs);
						if (ret >= 0)
							ret = npProfileSerializeDocWithProlog(
								&serializer, doc, outXml);
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedAttrs(avatarAttrs);
	npProfileFreeOwnedBxceDoc(doc);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x813302C0 */
static int npProfileBuildPanelListXml(char **outXml, const SceInt32 *categoryIdPtr,
				      SceInt32 sizeType, SceInt32 startOffset, SceInt32 maxCount)
{
	BxceSerializer serializer;
	ScebXCe_8EA5A077(&serializer);

	int ret = NP_COMMUNITY_ERR_NO_MEMORY;
	BxceDocument *doc = static_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(NP_BXCE_DOC_SIZE));
	if (doc != NULL) {
		ret = ScebXCe_FAB50608(doc, 1U);
		if (ret == 0) {
			BxceAttributeList *rootAttrs = static_cast<BxceAttributeList *>(
				sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
			BxceAttributeList *panelAttrs = NULL;
			if (rootAttrs != NULL) {
				ret = ScebXCe_E7C8B20D(rootAttrs);
				if (ret == 0) {
					panelAttrs = static_cast<BxceAttributeList *>(
						sceNpCommonMallocNpServerName(NP_BXCE_ATTR_SIZE));
					if (panelAttrs != NULL) {
						ret = ScebXCe_E7C8B20D(panelAttrs);
						if (ret == 0) {
							npCommunityXmlAddPlatformAttrs(rootAttrs);
							BxceNodeId docRoot = ScebXCe_75E1B095(doc);
							BxceNodeId profileNode = ScebXCe_AC0E4F67(
								doc, docRoot, "profile", rootAttrs,
								NULL, 0U);
							if (profileNode != 0U) {
								ret = npCommunityXmlAppendInt(
									doc, profileNode, "start",
									NULL, startOffset);
								if (ret >= 0)
									ret = npCommunityXmlAppendInt(
										doc, profileNode,
										"max", NULL,
										maxCount);
								if (ret >= 0)
									ret = npCommunityXmlAppendPanelElement(
										doc, profileNode,
										panelAttrs, "panel",
										categoryIdPtr, NULL,
										&sizeType);
								if (ret >= 0)
									ret = npProfileSerializeDocWithProlog(
										&serializer, doc,
										outXml);
							} else {
								ret = NP_COMMUNITY_ERR_NO_MEMORY;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					}
				}
				npProfileFreeOwnedAttrs(rootAttrs);
				npProfileFreeOwnedAttrs(panelAttrs);
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npProfileFreeOwnedBxceDoc(doc);
	ScebXCe_D24E80F2(&serializer);
	return ret;
}

/* 0x8132DA12 */
int npProfileGetProfile(int reqHandle, const void *npIdOrJid, int langCode, void *outProfile)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outProfile == NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			const SceNpId *targetNpId = static_cast<const SceNpId *>(npIdOrJid);
			NpCommunityProfileData *profile =
				static_cast<NpCommunityProfileData *>(outProfile);
			sceClibMemset(profile, 0, sizeof(SceUInt32));

			char regionCode[4];
			sceClibMemcpy_safe(regionCode, targetNpId->opt + 2, 2U);
			regionCode[2] = '\0';

			ret = npProfileBuildGetProfileXml(&reqXml, targetNpId, langCode);
			if (ret >= 0) {
				if (reqXml == NULL) {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				} else {
					SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
					ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_GET_PROFILE,
								  regionCode, xmlLen);
					if (ret >= 0) {
						SceSize sentSize = 0U;
						ret = npCommunityHttpSend(tx, reqXml, xmlLen,
									  &sentSize);
					}
					if (ret >= 0) {
						npCommunityFree(reqXml);
						reqXml = NULL;
						ret = npCommunityHttpRecvXml(tx, &respDoc,
									     NP_COMMUNITY_SIG_NONE,
									     NULL);
					}
					if (ret >= 0)
						ret = npCommunityXmlCheckResult(respDoc, "profile");
					if (ret >= 0) {
						npCommunityCloseTxHttp(tx);
						BxceNodeId root = ScebXCe_CED6BECF(respDoc);
						BxceNodeList *profileList =
							ScebXCe_1FA24DC0(respDoc, root, "profile");
						if (profileList != NULL) {
							BxceNodeId profileNode =
								ScebXCe_6C46D786(profileList, 0U);
							profile->npId = *targetNpId;
							ret = npCommunityXmlParseOnlineName(
								respDoc, profileNode,
								profile->onlineName.data, NULL,
								NULL);
							if (ret >= 0)
								ret = npCommunityXmlParseCountry(
									respDoc, profileNode,
									&profile->countryCode);
							if (ret >= 0)
								ret = npCommunityXmlParseLanguages(
									respDoc, profileNode,
									profile->languages);
							if (ret >= 0)
								ret = npCommunityXmlParseAboutMe(
									respDoc, profileNode,
									profile->aboutMe);
							if (ret >= 0)
								ret = npCommunityXmlParseAvatarUrl(
									respDoc, profileNode,
									profile->avatarUrl,
									&profile->avatarId);
							if (ret >= 0) {
								ret = npCommunityXmlParsePanelUrl(
									respDoc, profileNode,
									&profile->panel);
								if (ret == 0)
									ret = npCommunityXmlParseBgColorFallback(
										respDoc,
										profileNode,
										&profile->panel);
							}
							if (ret >= 0) {
								if (npCommunityXmlParseChildUint(
									    respDoc, profileNode,
									    "plusicon",
									    &profile->plusIcon) <
								    0) {
									profile->plusIcon = 0U;
								}
								ret = 0;
							}
							npProfileFreeBxceNodeList(profileList);
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					}
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132DCA2 */
int npProfileUpdateProfile(int reqHandle, const void *aboutMe, const void *avatarUrl,
			   const void *languages, const int *panelColor, const void *panelId,
			   int countryCode, const int *privacyFlag, const void *commId,
			   int avatarId, void *outProfile, int *outStatus, int *outFlags,
			   void *outStamp, const void *customBuf, SceSize customSize)
{
	/*
	 * Parameter mapping matching npmanager_job.cpp / IDA (0x8132DCA2):
	 *   aboutMe      -> onlineName (const char *)
	 *   avatarUrl    -> languages (const SceInt32 *)
	 *   languages    -> aboutMeText (const char *)
	 *   panelColor   -> avatarIdPtr (const SceInt32 *)
	 *   panelId      -> avatarPass (const void *, 8 bytes)
	 *   countryCode  -> avatarSizeType (SceInt32)
	 *   privacyFlag  -> panelIdPtr (const SceInt32 *)
	 *   commId       -> panelPass (const void *, 8 bytes)
	 *   avatarId     -> panelSizeType (SceInt32)
	 *   outProfile   -> NpCommunityProfileData *
	 *   outStatus    -> ptlpFlag (SceInt32 *)
	 *   outFlags     -> onlineNameUpdFlag (SceInt32 *)
	 *   outStamp     -> onlineNameUpdDate (SceRtcTick *)
	 *   customBuf    -> ticketBuf (const void *)
	 *   customSize   -> ticketSize (SceSize)
	 */
	const char *onlineName = static_cast<const char *>(aboutMe);
	const SceInt32 *langPtr = static_cast<const SceInt32 *>(avatarUrl);
	const char *aboutMeText = static_cast<const char *>(languages);
	const SceInt32 *avatarIdPtr = static_cast<const SceInt32 *>(panelColor);
	const void *avatarPass = panelId;
	SceInt32 avatarSizeType = countryCode;
	const SceInt32 *panelIdPtr = static_cast<const SceInt32 *>(privacyFlag);
	const void *panelPass = commId;
	SceInt32 panelSizeType = avatarId;

	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (customBuf == NULL || outProfile == NULL ||
	    (onlineName == NULL && langPtr == NULL && aboutMeText == NULL &&
	     avatarIdPtr == NULL && panelIdPtr == NULL)) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else {
		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildUpdateProfileXml(
				&reqXml, customBuf, customSize, tx->client->envName, onlineName,
				langPtr, aboutMeText, avatarIdPtr, avatarPass, avatarSizeType,
				panelIdPtr, panelPass, panelSizeType);
			if (ret >= 0) {
				if (reqXml == NULL) {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				} else {
					ret = npCommunityHttpOpen(
						tx, NP_COMMUNITY_EP_UPDATE_SELF_PROFILE, NULL, 0U);
					if (ret >= 0) {
						SceSize xmlLen =
							sceClibStrnlen(reqXml, 0x7FFFFFFFU);
						sceHttpSetRequestContentLength(
							tx->httpReqId,
							static_cast<SceULong64>(xmlLen));
						SceSize sentSize = 0U;
						ret = npCommunityHttpSend(tx, reqXml, xmlLen,
									  &sentSize);
					}
					if (ret >= 0)
						ret = npCommunityHttpRecvXml(tx, &respDoc,
									     NP_COMMUNITY_SIG_NONE,
									     NULL);
					if (ret >= 0)
						ret = npCommunityXmlCheckResult(respDoc, "profile");
					if (ret >= 0) {
						BxceNodeId root = ScebXCe_CED6BECF(respDoc);
						BxceNodeList *profileList =
							ScebXCe_1FA24DC0(respDoc, root, "profile");
						if (profileList != NULL) {
							BxceNodeId profileNode =
								ScebXCe_6C46D786(profileList, 0U);
							NpCommunityProfileData *profile =
								static_cast<NpCommunityProfileData *>(
									outProfile);
							sceClibMemset(
								profile, 0,
								sizeof(NpCommunityProfileData));

							ret = npCommunityXmlParseJidChild(
								respDoc, profileNode, "jid",
								&profile->npId);
							if (ret >= 0)
								ret = npCommunityXmlParseOnlineName(
									respDoc, profileNode,
									profile->onlineName.data,
									outFlags,
									static_cast<SceRtcTick *>(
										outStamp));
							if (ret >= 0)
								ret = npCommunityXmlParseCountry(
									respDoc, profileNode,
									&profile->countryCode);
							if (ret >= 0)
								ret = npCommunityXmlParseLanguages(
									respDoc, profileNode,
									profile->languages);
							if (ret >= 0)
								ret = npCommunityXmlParseAboutMe(
									respDoc, profileNode,
									profile->aboutMe);
							if (ret >= 0)
								ret = npCommunityXmlParseAvatarUrl(
									respDoc, profileNode,
									profile->avatarUrl,
									&profile->avatarId);
							if (ret >= 0) {
								ret = npCommunityXmlParsePanelUrl(
									respDoc, profileNode,
									&profile->panel);
								if (ret == 0)
									ret = npCommunityXmlParseBgColorFallback(
										respDoc,
										profileNode,
										&profile->panel);
							}
							if (ret >= 0) {
								if (npCommunityXmlParseChildUint(
									    respDoc, profileNode,
									    "plusicon",
									    &profile->plusIcon) <
								    0) {
									profile->plusIcon = 0U;
								}
								if (outStatus != NULL)
									ret = npProfileParseIntChildText(
										respDoc,
										profileNode, "ptlp",
										outStatus);
							}
							if (ret >= 0) {
								npAccountUpdateCachedProfile(
									langPtr, aboutMeText,
									reinterpret_cast<
										const SceUInt32 *>(
										avatarIdPtr),
									&profile->panel,
									profile->avatarUrl);
								ret = 0;
							}
							npProfileFreeBxceNodeList(profileList);
						} else {
							ret = NP_COMMUNITY_ERR_PARSE_FAIL;
						}
					}
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132E01C */
static int npProfileResolveRegionalLanguage(const char *countryCode, const SceInt32 *languages)
{
	SceInt32 lang = languages[0];
	if (lang == 1) {
		if (sceClibStrncasecmp(countryCode, "US", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CA", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "MX", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "BR", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CL", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "PE", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "AR", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CO", 3U) == 0) {
			return 1;
		}
		return 18;
	}
	if (lang == 7) {
		if (sceClibStrncasecmp(countryCode, "BR", 3U) == 0)
			return 17;
		return 7;
	}
	if (lang == 3) {
		if (sceClibStrncasecmp(countryCode, "MX", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CL", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "AR", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "PE", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CO", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "PA", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "CR", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "EC", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "GT", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "SV", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "PY", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "HN", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "BO", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "UY", 3U) == 0 ||
		    sceClibStrncasecmp(countryCode, "NI", 3U) == 0) {
			return 20;
		}
		return 3;
	}
	if (lang == 2) {
		if (sceClibStrncasecmp(countryCode, "CA", 3U) == 0)
			return 22;
		return 2;
	}
	return lang;
}

/* 0x8132E250 */
int npProfileFetchOwnProfile(int profileReqHandle, void *outProfile, int *outStatus, int *outFlags,
			     SceUInt64 *outTimestamp, void *outStamp, const void *ticketBuf,
			     SceSize ticketSize, int mode)
{
	/*
	 * Parameter mapping matching np_manager_core.cpp / IDA (0x8132E250):
	 *   outProfile   -> NpCommunityProfileData *
	 *   outStatus    -> isExistingProfile (int *: 1 if get_self_profile succeeded, 0 if registered)
	 *   outFlags     -> ptlpFlag (int *)
	 *   outTimestamp -> onlineNameUpdFlag (SceInt32 *, passed as SceUInt64 * in caller)
	 *   outStamp     -> onlineNameUpdDate (SceRtcTick *)
	 *   ticketBuf    -> const void *
	 *   ticketSize   -> SceSize
	 *   mode         -> avatarSizeType (int)
	 */
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outProfile == NULL || outStatus == NULL || outFlags == NULL || ticketBuf == NULL) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		NpCommunityProfileData *profile =
			static_cast<NpCommunityProfileData *>(outProfile);
		sceClibMemset(profile, 0, sizeof(NpCommunityProfileData));

		ret = npCommunityAcquireBackendTx(profileReqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildSelfProfileXml(&reqXml, ticketBuf, ticketSize,
							   tx->client->envName, mode);
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_GET_SELF_PROFILE,
							  NULL, xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
			}

			int newlyRegistered = 0;
			if (ret >= 0) {
				ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret == NP_COMMUNITY_ERR_PROFILE_NOT_REGISTERED) {
					*outStatus = 0;
					npCommunityCloseTxHttp(tx);
					npProfileFreeBxceDoc(respDoc);
					respDoc = NULL;

					SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
					ret = npCommunityHttpOpen(
						tx, NP_COMMUNITY_EP_REGISTER_SELF_PROFILE, NULL,
						xmlLen);
					if (ret >= 0) {
						SceSize sentSize = 0U;
						ret = npCommunityHttpSend(tx, reqXml, xmlLen,
									  &sentSize);
					}
					if (ret >= 0)
						ret = npCommunityHttpRecvXml(tx, &respDoc,
									     NP_COMMUNITY_SIG_NONE,
									     NULL);
					if (ret >= 0)
						ret = npCommunityXmlCheckResult(respDoc, "profile");
					newlyRegistered = 1;
				} else {
					*outStatus = 1;
				}
			}

			if (ret >= 0) {
				BxceNodeId root = ScebXCe_CED6BECF(respDoc);
				BxceNodeList *profileList =
					ScebXCe_1FA24DC0(respDoc, root, "profile");
				if (profileList != NULL) {
					BxceNodeId profileNode =
						ScebXCe_6C46D786(profileList, 0U);
					ret = npCommunityXmlParseJidChild(respDoc, profileNode,
									  "jid", &profile->npId);
					if (ret >= 0) {
						tx->client->npId = profile->npId;
						ret = npCommunityXmlParseOnlineName(
							respDoc, profileNode,
							profile->onlineName.data,
							reinterpret_cast<SceInt32 *>(outTimestamp),
							static_cast<SceRtcTick *>(outStamp));
					}
					if (ret >= 0) {
						tx->client->onlineName = profile->onlineName;
						ret = npCommunityXmlParseCountry(
							respDoc, profileNode,
							&profile->countryCode);
					}
					if (ret >= 0)
						ret = npCommunityXmlParseLanguages(
							respDoc, profileNode, profile->languages);
					if (ret >= 0)
						ret = npCommunityXmlParseAboutMe(respDoc,
										 profileNode,
										 profile->aboutMe);
					if (ret >= 0)
						ret = npCommunityXmlParseAvatarUrl(
							respDoc, profileNode, profile->avatarUrl,
							&profile->avatarId);
					if (ret >= 0) {
						if (npCommunityXmlParseChildUint(
							    respDoc, profileNode, "plusicon",
							    &profile->plusIcon) < 0) {
							profile->plusIcon = 0U;
						}
						ret = npProfileParseIntChildText(
							respDoc, profileNode, "ptlp", outFlags);
					}
					if (ret >= 0) {
						ret = npCommunityXmlParsePanelUrl(respDoc,
										  profileNode,
										  &profile->panel);
						if (ret == 0)
							ret = npCommunityXmlParseBgColorFallback(
								respDoc, profileNode,
								&profile->panel);
					}
					npProfileFreeBxceNodeList(profileList);
				} else {
					ret = NP_COMMUNITY_ERR_PARSE_FAIL;
				}
			}

			if (ret >= 0) {
				npProfileFreeBxceDoc(respDoc);
				respDoc = NULL;
				if (reqXml != NULL) {
					npCommunityFree(reqXml);
					reqXml = NULL;
				}
				npCommunityCloseTxHttp(tx);

				if (newlyRegistered == 1) {
					int regionalLang = npProfileResolveRegionalLanguage(
						profile->countryCode.data, profile->languages);
					if (regionalLang < 0) {
						ret = regionalLang;
					} else if (regionalLang != profile->languages[0]) {
						profile->languages[0] = regionalLang;
						ret = npProfileUpdateProfile(
							profileReqHandle, NULL, profile->languages,
							NULL, NULL, NULL, mode, NULL, NULL, 0,
							profile, outFlags,
							reinterpret_cast<int *>(outTimestamp),
							outStamp, ticketBuf, ticketSize);
					}
				}
				if (ret >= 0) {
					npCommunityReleaseBackendTx(tx);
					return 0;
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132E6C2 */
static int npProfileGetAvatarCategoriesInternal(int reqHandle, int langCode, int startOffset,
						void *outEntries, SceUInt32 maxCount, int *outTotal,
						int useCatalogList)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outEntries == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (maxCount == 0U) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		NpCommunityCategoryEntry *entries =
			static_cast<NpCommunityCategoryEntry *>(outEntries);
		sceClibMemset(entries, 0, sizeof(NpCommunityCategoryEntry) * maxCount);

		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildCategoryListXml(&reqXml, langCode, startOffset,
							    static_cast<SceInt32>(maxCount));
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				int endpoint = (useCatalogList != 0)
						       ? NP_COMMUNITY_EP_GET_AVATAR_CATEGORY_LIST
						       : NP_COMMUNITY_EP_GET_AVATAR_CATEGORY;
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, endpoint, NULL, xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
				if (ret >= 0)
					ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret >= 0) {
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *profileList =
						ScebXCe_1FA24DC0(respDoc, root, "profile");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *catNodes = NULL;
					if (profileList != NULL) {
						BxceNodeId profileNode =
							ScebXCe_6C46D786(profileList, 0U);
						ret = npCommunityXmlParseChildUint(
							respDoc, profileNode, "categories",
							reinterpret_cast<SceUInt32 *>(outTotal));
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, profileNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0U) {
								BxceNodeId listNode =
									ScebXCe_6C46D786(listNodes,
											 0U);
								catNodes = ScebXCe_1FA24DC0(
									respDoc, listNode,
									"category");
								if (catNodes != NULL &&
								    catNodes->count != 0U &&
								    maxCount >= catNodes->count) {
									for (SceUInt32 idx = 0U;
									     idx < catNodes->count;
									     ++idx) {
										BxceNodeId catNode =
											ScebXCe_6C46D786(
												catNodes,
												idx);
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											catNode,
											"id",
											&entries[idx]
												 .categoryId);
										if (ret < 0)
											break;
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											catNode,
											"avatars",
											&entries[idx]
												 .itemCount);
										if (ret < 0) {
											if (ret !=
											    NP_COMMUNITY_ERR_PARSE_FAIL)
												break;
											entries[idx]
												.itemCount =
												0;
											entries[idx]
												.countMissing =
												1U;
										}
										ret = npCommunityXmlParseText(
											respDoc,
											catNode,
											NULL,
											entries[idx]
												.name,
											384U);
										if (ret < 0)
											break;
									}
									if (ret >= 0)
										ret = static_cast<
											int>(
											catNodes->count);
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
					npProfileFreeBxceNodeList(profileList);
					npProfileFreeBxceNodeList(listNodes);
					npProfileFreeBxceNodeList(catNodes);
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132E9A8 */
int npProfileGetAvatarList(int reqHandle, int categoryId, int startOffset, void *outEntries,
			   SceUInt32 maxCount, int *outTotal)
{
	return npProfileGetAvatarCategoriesInternal(reqHandle, categoryId, startOffset, outEntries,
						    maxCount, outTotal, 0);
}

/* 0x8132E9C0 */
int npProfileGetCatalogList(int reqHandle, int categoryId, int startOffset, void *outEntries,
			    SceUInt32 maxCount, int *outTotal)
{
	return npProfileGetAvatarCategoriesInternal(reqHandle, categoryId, startOffset, outEntries,
						    maxCount, outTotal, 1);
}

/* 0x8132E9D8 */
static int npProfileGetAvatarUrlsInternal(int reqHandle, const void *ticketBuf, SceSize ticketSize,
					  const int *categoryIdPtr, int sizeType, int startOffset,
					  int *outIds, void *outUrls, int *outIsPremium,
					  SceUInt32 maxCount, int *outTotal, int *outPremiumCount,
					  int useAvailableEndpoint)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outIds == NULL || outUrls == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (maxCount == 0U) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		NpCommunityUrlString *urls = static_cast<NpCommunityUrlString *>(outUrls);
		sceClibMemset(outIds, 0, sizeof(SceInt32) * maxCount);
		sceClibMemset(urls, 0, sizeof(NpCommunityUrlString) * maxCount);

		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildAvatarListXml(&reqXml, ticketBuf, ticketSize,
							  categoryIdPtr, sizeType, startOffset,
							  static_cast<SceInt32>(maxCount));
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				int endpoint = (useAvailableEndpoint != 0)
						       ? NP_COMMUNITY_EP_GET_AVAILABLE_AVATAR_LIST
						       : NP_COMMUNITY_EP_GET_AVATAR_LIST;
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, endpoint, NULL, xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
				if (ret >= 0)
					ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret >= 0) {
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *profileList =
						ScebXCe_1FA24DC0(respDoc, root, "profile");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *urlNodes = NULL;
					if (profileList != NULL) {
						BxceNodeId profileNode =
							ScebXCe_6C46D786(profileList, 0U);
						ret = npCommunityXmlParseChildUintAndAttr(
							respDoc, profileNode, "avatars", "premium",
							reinterpret_cast<SceUInt32 *>(outTotal),
							reinterpret_cast<SceUInt32 *>(
								outPremiumCount));
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, profileNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0U) {
								BxceNodeId listNode =
									ScebXCe_6C46D786(listNodes,
											 0U);
								urlNodes = ScebXCe_1FA24DC0(
									respDoc, listNode,
									"avatarurl");
								if (urlNodes == NULL ||
								    urlNodes->count == 0U) {
									ret = 0;
								} else if (maxCount >=
									   urlNodes->count) {
									for (SceUInt32 idx = 0U;
									     idx < urlNodes->count;
									     ++idx) {
										BxceNodeId urlNode =
											ScebXCe_6C46D786(
												urlNodes,
												idx);
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											urlNode,
											"id",
											&outIds[idx]);
										if (ret < 0)
											break;
										ret = npCommunityXmlParseText(
											respDoc,
											urlNode,
											NULL,
											urls[idx]
												.url,
											127U);
										if (ret < 0)
											break;
										if (outIsPremium !=
										    NULL)
											npCommunityXmlParsePremiumAttr(
												respDoc,
												urlNode,
												"type",
												&outIsPremium
													[idx]);
									}
									if (ret >= 0)
										ret = static_cast<
											int>(
											urlNodes->count);
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
					npProfileFreeBxceNodeList(profileList);
					npProfileFreeBxceNodeList(listNodes);
					npProfileFreeBxceNodeList(urlNodes);
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132ECCE */
int npProfileGetAvatarUrls(int reqHandle, const int *categoryIdPtr, int startOffset, int sortMode,
			   int *outIds, void *outUrls, SceUInt32 maxCount, int *outTotal)
{
	return npProfileGetAvatarUrlsInternal(reqHandle, NULL, 0U, categoryIdPtr, startOffset,
					      sortMode, outIds, outUrls, NULL, maxCount, outTotal,
					      NULL, 0);
}

/* 0x8132ED02 */
int npProfileGetAvatarBatch(int reqHandle, const void *ticketBuf, SceSize ticketSize,
			    const int *categoryIdPtr, int startOffset, int sortMode, int *outIds,
			    void *outUrls, int *outExtraIds, SceUInt32 maxCount, int *outTotal,
			    int *outNextOffset)
{
	return npProfileGetAvatarUrlsInternal(reqHandle, ticketBuf, ticketSize, categoryIdPtr,
					      startOffset, sortMode, outIds, outUrls, outExtraIds,
					      maxCount, outTotal, outNextOffset, 1);
}

/* 0x8132ED42 */
int npProfileGetAvatarUrl(int reqHandle, const void *ticketBuf, SceSize ticketSize, int avatarId,
			  int sizeType, void *outUrl)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (ticketBuf == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else {
		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildUpdateAvatarXml(&reqXml, ticketBuf, ticketSize,
							    &tx->client->npId, avatarId, sizeType);
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_UPDATE_SELF_AVATAR,
							  NULL, xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
				if (ret >= 0)
					ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret >= 0) {
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *profileList =
						ScebXCe_1FA24DC0(respDoc, root, "profile");
					if (profileList != NULL) {
						BxceNodeId profileNode =
							ScebXCe_6C46D786(profileList, 0U);
						SceInt32 parsedAvatarId = 0;
						ret = npCommunityXmlParseAvatarUrl(
							respDoc, profileNode,
							static_cast<char *>(outUrl),
							&parsedAvatarId);
						if (ret >= 0) {
							npAccountUpdateCachedProfile(
								NULL, NULL,
								reinterpret_cast<const SceUInt32 *>(
									&parsedAvatarId),
								NULL, outUrl);
							ret = 0;
						}
						npProfileFreeBxceNodeList(profileList);
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132EEB4 */
int npProfileGetPanelCategories(int reqHandle, int startOffset, int sortMode, void *outEntries,
				SceUInt32 maxCount, int *outTotal)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outEntries == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (maxCount == 0U) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		NpCommunityCategoryEntry *entries =
			static_cast<NpCommunityCategoryEntry *>(outEntries);
		sceClibMemset(entries, 0, sizeof(NpCommunityCategoryEntry) * maxCount);

		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildCategoryListXml(&reqXml, startOffset, sortMode,
							    static_cast<SceInt32>(maxCount));
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_GET_PANEL_CATEGORY,
							  NULL, xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
				if (ret >= 0)
					ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret >= 0) {
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *profileList =
						ScebXCe_1FA24DC0(respDoc, root, "profile");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *catNodes = NULL;
					if (profileList != NULL) {
						BxceNodeId profileNode =
							ScebXCe_6C46D786(profileList, 0U);
						ret = npCommunityXmlParseChildUint(
							respDoc, profileNode, "categories",
							reinterpret_cast<SceUInt32 *>(outTotal));
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, profileNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0U) {
								BxceNodeId listNode =
									ScebXCe_6C46D786(listNodes,
											 0U);
								catNodes = ScebXCe_1FA24DC0(
									respDoc, listNode,
									"category");
								if (catNodes != NULL &&
								    catNodes->count != 0U &&
								    maxCount >= catNodes->count) {
									for (SceUInt32 idx = 0U;
									     idx < catNodes->count;
									     ++idx) {
										BxceNodeId catNode =
											ScebXCe_6C46D786(
												catNodes,
												idx);
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											catNode,
											"id",
											&entries[idx]
												 .categoryId);
										if (ret < 0)
											break;
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											catNode,
											"panels",
											&entries[idx]
												 .itemCount);
										if (ret < 0)
											break;
										ret = npCommunityXmlParseText(
											respDoc,
											catNode,
											NULL,
											entries[idx]
												.name,
											384U);
										if (ret < 0)
											break;
									}
									if (ret >= 0)
										ret = static_cast<
											int>(
											catNodes->count);
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
					npProfileFreeBxceNodeList(profileList);
					npProfileFreeBxceNodeList(listNodes);
					npProfileFreeBxceNodeList(catNodes);
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8132F16A */
int npProfileGetPanelUrls(int reqHandle, const int *categoryIdPtr, int startOffset, int sortMode,
			  int *outIds, int *outBgColors, void *outUrls, SceUInt32 maxCount,
			  int *outTotal)
{
	BxceDocument *respDoc = NULL;
	NpCommunityBackendTx *tx = NULL;
	char *reqXml = NULL;
	int ret;

	if (outIds == NULL || outUrls == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else if (maxCount == 0U) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		NpCommunityUrlString *urls = static_cast<NpCommunityUrlString *>(outUrls);
		sceClibMemset(outIds, 0, sizeof(SceInt32) * maxCount);
		sceClibMemset(urls, 0, sizeof(NpCommunityUrlString) * maxCount);

		ret = npCommunityAcquireBackendTx(reqHandle, &tx);
		if (ret >= 0) {
			ret = npProfileBuildPanelListXml(&reqXml, categoryIdPtr, startOffset,
							 sortMode, static_cast<SceInt32>(maxCount));
			if (ret >= 0 && reqXml == NULL)
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			if (ret >= 0) {
				SceSize xmlLen = sceClibStrnlen(reqXml, 0x7FFFFFFFU);
				ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_GET_PANEL_LIST, NULL,
							  xmlLen);
				if (ret >= 0) {
					SceSize sentSize = 0U;
					ret = npCommunityHttpSend(tx, reqXml, xmlLen, &sentSize);
				}
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc,
								     NP_COMMUNITY_SIG_NONE, NULL);
				if (ret >= 0)
					ret = npCommunityXmlCheckResult(respDoc, "profile");
				if (ret >= 0) {
					BxceNodeId root = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *profileList =
						ScebXCe_1FA24DC0(respDoc, root, "profile");
					BxceNodeList *listNodes = NULL;
					BxceNodeList *urlNodes = NULL;
					if (profileList != NULL) {
						BxceNodeId profileNode =
							ScebXCe_6C46D786(profileList, 0U);
						ret = npCommunityXmlParseChildUint(
							respDoc, profileNode, "panels",
							reinterpret_cast<SceUInt32 *>(outTotal));
						if (ret >= 0) {
							listNodes = ScebXCe_1FA24DC0(
								respDoc, profileNode, "list");
							if (listNodes != NULL &&
							    listNodes->count != 0U) {
								BxceNodeId listNode =
									ScebXCe_6C46D786(listNodes,
											 0U);
								urlNodes = ScebXCe_1FA24DC0(
									respDoc, listNode,
									"panelurl");
								if (urlNodes == NULL ||
								    urlNodes->count == 0U) {
									ret = 0;
								} else if (maxCount >=
									   urlNodes->count) {
									for (SceUInt32 idx = 0U;
									     idx < urlNodes->count;
									     ++idx) {
										BxceNodeId urlNode =
											ScebXCe_6C46D786(
												urlNodes,
												idx);
										ret = npCommunityXmlParseIntAttr(
											respDoc,
											urlNode,
											"id",
											&outIds[idx]);
										if (ret < 0)
											break;
										ret = npCommunityXmlParseHexAttr(
											respDoc,
											urlNode,
											"bgc",
											reinterpret_cast<
												SceUInt32 *>(
												&outBgColors
													[idx]));
										if (ret < 0)
											break;
										ret = npCommunityXmlParseText(
											respDoc,
											urlNode,
											NULL,
											urls[idx]
												.url,
											127U);
										if (ret < 0)
											break;
									}
									if (ret >= 0)
										ret = static_cast<
											int>(
											urlNodes->count);
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
					npProfileFreeBxceNodeList(profileList);
					npProfileFreeBxceNodeList(listNodes);
					npProfileFreeBxceNodeList(urlNodes);
				}
			}
		}
	}

	npProfileFreeBxceDoc(respDoc);
	if (reqXml != NULL)
		npCommunityFree(reqXml);
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}
