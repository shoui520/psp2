/*
 * np_community_server_list.cpp: NP Community server-list and status-XML fetchers
 * (0x8133820C..0x81338ADB).
 */
#include "np_community_backend.h"

#include <libkernel/libkernel.h>

/* Server list service type IDs (NpCommunityBackendTx::serviceType) */
#define NP_COMMUNITY_SVC_BASIC_TTL		1
#define NP_COMMUNITY_SVC_LOBBY			19
#define NP_COMMUNITY_SVC_TROPHY			20
#define NP_COMMUNITY_SVC_STORAGE		21
#define NP_COMMUNITY_SVC_REPORT			32
#define NP_COMMUNITY_SVC_FRIEND			34

/* Friendlist service status codes (NpCommunityServerNode::statusCode) */
#define NP_FRIEND_STATUS_BEFORE			1u
#define NP_FRIEND_STATUS_END			2u
#define NP_FRIEND_STATUS_ALIVE			3u
#define NP_FRIEND_STATUS_MAINTENANCE		4u

static int npCommunityParseFriendStatusNodes(NpCommunityBackendTx *tx, const BxceDocument *respDoc,
					     BxceNodeId rootElem, NpCommunityServerNode **outList)
{
	if (outList == NULL)
		return NP_COMMUNITY_ERR_INVALID_ARG;

	NpCommunityServerNode *nodes = static_cast<NpCommunityServerNode *>(
		npCommunityAlloc(sizeof(NpCommunityServerNode) * 2));
	if (nodes == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(nodes, 0, sizeof(NpCommunityServerNode) * 2);
	nodes[0].serviceRole = 0;
	nodes[0].next = &nodes[1];
	nodes[1].serviceRole = 1;

	int ret = 0;
	for (NpCommunityServerNode *cur = nodes; cur != NULL; cur = cur->next) {
		const char *tagName = NULL;
		if (cur->serviceRole == 0) {
			tagName = "update";
		} else if (cur->serviceRole == 1) {
			tagName = "view";
		} else {
			ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			break;
		}

		BxceNodeList *tagList = ScebXCe_1FA24DC0(respDoc, rootElem, tagName);
		const char *statusStr = NULL;
		if (tagList == NULL) {
			ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			break;
		}
		BxceNodeId tagElem = ScebXCe_6C46D786(tagList, 0);
		if (tagElem == 0 ||
		    npCommunityXmlFindAttr(&statusStr, respDoc, tagElem, "status") < 0 ||
		    statusStr == NULL) {
			ScebXCe_1F59F0C4(tagList);
			ScebXCe_EC560647(tagList);
			ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			break;
		}

		if (sceClibStrncasecmp(statusStr, "alive",
				       sceClibStrnlen("alive", 0x7FFFFFFFu) + 1) == 0) {
			cur->statusCode = NP_FRIEND_STATUS_ALIVE;
		} else if (sceClibStrncasecmp(statusStr, "maintenance",
					      sceClibStrnlen("maintenance", 0x7FFFFFFFu) + 1) == 0) {
			cur->statusCode = NP_FRIEND_STATUS_MAINTENANCE;
		} else if (sceClibStrncasecmp(statusStr, "before",
					      sceClibStrnlen("before", 0x7FFFFFFFu) + 1) == 0) {
			cur->statusCode = NP_FRIEND_STATUS_BEFORE;
		} else if (sceClibStrncasecmp(statusStr, "end",
					      sceClibStrnlen("end", 0x7FFFFFFFu) + 1) == 0) {
			cur->statusCode = NP_FRIEND_STATUS_END;
		} else {
			cur->statusCode = 0;
		}

		ScebXCe_1F59F0C4(tagList);
		ScebXCe_EC560647(tagList);
	}

	if (ret >= 0) {
		ret = npCommunityTrackTxAlloc(tx, nodes);
		if (ret >= 0) {
			*outList = nodes;
			return 0;
		}
	}

	npCommunityFree(nodes);
	return ret;
}

/* 0x8133820C */
int npCommunityGetServerListEx(SceInt32 txId, NpCommunityServerNode **outList,
			       SceInt64 *outIntervals)
{
	NpCommunityBackendTx *tx = NULL;
	BxceDocument *respDoc = NULL;
	NpCommunityServerNode *allocatedNodes = NULL;
	SceInt32 allocatedCount = 0;

	int ret = npCommunityAcquireBackendTx(txId, &tx);
	if (ret >= 0) {
		SceInt32 svcType = tx->serviceType;
		if (outList == NULL && svcType != NP_COMMUNITY_SVC_BASIC_TTL) {
			ret = NP_COMMUNITY_ERR_INVALID_ARG;
		} else {
			const char *rootTag = NULL;
			const char *fqdnTag = NULL;
			int serviceCategory = 0;
			int endpointId = 0;
			int sigMode = 0;
			SceInt32 intervalCount = 0;
			SceInt32 defaultIntervalSec = 0;
			const char *intervalTags[3] = { NULL, NULL, NULL };

			switch (svcType) {
			case NP_COMMUNITY_SVC_BASIC_TTL:
				rootTag = "param";
				serviceCategory = 5;
				endpointId = NP_COMMUNITY_EP_BASIC_TTL;
				sigMode = NP_COMMUNITY_SIG_COMMENT_TTL;
				defaultIntervalSec = 864000;
				intervalCount = 3;
				intervalTags[0] = "interv" "al0";
				intervalTags[1] = "interv" "al1";
				intervalTags[2] = "interv" "al2";
				break;
			case NP_COMMUNITY_SVC_LOBBY:
				rootTag = "lobby";
				fqdnTag = "lobby-fqdn";
				serviceCategory = 1;
				endpointId = NP_COMMUNITY_EP_LOBBY_SERVER_LIST;
				sigMode = NP_COMMUNITY_SIG_COMMENT_RANKING_LIST;
				break;
			case NP_COMMUNITY_SVC_TROPHY:
				rootTag = "nptrophy";
				fqdnTag = "trophy-fqdn";
				serviceCategory = 3;
				endpointId = NP_COMMUNITY_EP_TROPHY_STATUS;
				sigMode = NP_COMMUNITY_SIG_COMMENT_SEARCH_JID;
				defaultIntervalSec = 86400;
				intervalCount = 1;
				intervalTags[0] = "interval";
				break;
			case NP_COMMUNITY_SVC_STORAGE:
				rootTag = "npstorage";
				fqdnTag = "storage-fqdn";
				serviceCategory = 4;
				endpointId = NP_COMMUNITY_EP_STORAGE_SERVER_LIST;
				sigMode = NP_COMMUNITY_SIG_COMMENT_TROPHY_STATUS;
				break;
			case NP_COMMUNITY_SVC_REPORT:
				rootTag = "npreport";
				fqdnTag = "report-fqdn";
				serviceCategory = 6;
				endpointId = NP_COMMUNITY_EP_REPORT_SERVER_LIST;
				sigMode = NP_COMMUNITY_SIG_COMMENT_FRIEND_STATUS;
				break;
			case NP_COMMUNITY_SVC_FRIEND:
				rootTag = "npfriend";
				serviceCategory = 7;
				endpointId = NP_COMMUNITY_EP_FRIENDLIST_STATUS;
				sigMode = NP_COMMUNITY_SIG_COMMENT_EXTRA;
				break;
			default:
				ret = NP_COMMUNITY_ERR_INVALID_ENDPOINT;
				break;
			}

			if (ret >= 0) {
				ret = npCommunityHttpOpen(tx, endpointId, tx->serviceParam, 0);
				if (ret >= 0)
					ret = npCommunityHttpSend(tx, NULL, 0, NULL);
				if (ret >= 0)
					ret = npCommunityHttpRecvXml(tx, &respDoc, sigMode, NULL);
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId docRoot = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *rootList = ScebXCe_1FA24DC0(respDoc, docRoot,
										  rootTag);
					BxceNodeList *fqdnList = NULL;

					if (rootList != NULL) {
						BxceNodeId rootElem = ScebXCe_6C46D786(rootList, 0);
						if (svcType != NP_COMMUNITY_SVC_BASIC_TTL &&
						    svcType != NP_COMMUNITY_SVC_FRIEND) {
							ret = npCommunityXmlCheckServerStatus(
								respDoc, rootElem, serviceCategory);
						}
						if (ret >= 0 &&
						    svcType != NP_COMMUNITY_SVC_TROPHY &&
						    svcType != NP_COMMUNITY_SVC_BASIC_TTL &&
						    svcType != NP_COMMUNITY_SVC_FRIEND &&
						    svcType != NP_COMMUNITY_SVC_REPORT) {
							ret = npCommunityXmlVerifyTitleId(
								respDoc, rootElem,
								static_cast<const SceNpCommunicationId *>(
									tx->serviceParam));
						}

						if (ret >= 0 &&
						    svcType != NP_COMMUNITY_SVC_BASIC_TTL &&
						    svcType != NP_COMMUNITY_SVC_FRIEND) {
							fqdnList = ScebXCe_1FA24DC0(
								respDoc, rootElem, fqdnTag);
							if (fqdnList == NULL) {
								ret = NP_COMMUNITY_ERR_NO_SERVER_FOUND;
							} else if (fqdnList->count == 0) {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							} else {
								allocatedCount =
									static_cast<SceInt32>(
										fqdnList->count);
								SceSize allocBytes =
									sizeof(NpCommunityServerNode) *
									static_cast<SceSize>(
										allocatedCount);
								allocatedNodes = static_cast<NpCommunityServerNode *>(
									npCommunityAlloc(allocBytes));
								if (allocatedNodes == NULL) {
									ret = NP_COMMUNITY_ERR_NO_MEMORY;
								} else {
									sceClibMemset(allocatedNodes,
										      0, allocBytes);
									for (SceInt32 idx = 0;
									     idx < allocatedCount;
									     ++idx) {
										BxceNodeId hostElem =
											ScebXCe_6C46D786(
												fqdnList,
												static_cast<SceUInt32>(
													idx));
										BxceNodeId textNode =
											ScebXCe_FEA6E4A1(
												respDoc,
												hostElem);
										if (textNode == 0) {
											ret = NP_COMMUNITY_ERR_PARSE_FAIL;
											break;
										}
										const char *hostStr =
											ScebXCe_33825EFF(
												respDoc,
												textNode);
										if (hostStr == NULL) {
											ret = NP_COMMUNITY_ERR_PARSE_FAIL;
											break;
										}
										SceSize hostLen = sceClibStrnlen(
											hostStr,
											0x7FFFFFFFu);
										char *hostCopy = static_cast<char *>(
											npCommunityAlloc(
												hostLen + 1));
										allocatedNodes[idx].hostName =
											hostCopy;
										if (hostCopy == NULL) {
											ret = NP_COMMUNITY_ERR_NO_MEMORY;
											break;
										}
										sceClibStrncpy(
											hostCopy,
											hostStr,
											hostLen + 1);
										if (idx + 1 <
										    allocatedCount) {
											allocatedNodes[idx].next =
												&allocatedNodes[idx + 1];
										}
									}
									if (ret >= 0) {
										ret = npCommunityTrackTxAlloc(
											tx,
											allocatedNodes);
										if (ret >= 0) {
											SceInt32 trackedIdx = 0;
											for (;
											     trackedIdx <
											     allocatedCount;
											     ++trackedIdx) {
												ret = npCommunityTrackTxAlloc(
													tx,
													const_cast<char *>(
														allocatedNodes[trackedIdx].hostName));
												if (ret < 0)
													break;
											}
											if (ret < 0) {
												for (;
												     trackedIdx <
												     allocatedCount;
												     ++trackedIdx) {
													npCommunityFree(
														const_cast<char *>(
															allocatedNodes[trackedIdx].hostName));
												}
												npCommunityFree(
													allocatedNodes);
												allocatedNodes = NULL;
											} else {
												*outList =
													allocatedNodes;
												allocatedNodes = NULL;
											}
										}
									}
								}
							}
						}

						if (ret >= 0 &&
						    svcType == NP_COMMUNITY_SVC_FRIEND) {
							ret = npCommunityParseFriendStatusNodes(
								tx, respDoc, rootElem, outList);
						}

						if (ret >= 0) {
							if (svcType == NP_COMMUNITY_SVC_TROPHY ||
							    svcType == NP_COMMUNITY_SVC_BASIC_TTL) {
								for (SceInt32 idx = 0;
								     idx < intervalCount; ++idx) {
									SceUInt32 secVal = 0;
									if (npCommunityXmlParseChildUint(
										    respDoc, rootElem,
										    intervalTags[idx],
										    &secVal) < 0) {
										secVal = static_cast<SceUInt32>(
											defaultIntervalSec);
									}
									if (outIntervals != NULL) {
										outIntervals[idx] =
											1000000LL *
											static_cast<SceInt64>(
												secVal);
									}
								}
							}
							ret = 0;
						}
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}

					if (rootList != NULL) {
						ScebXCe_1F59F0C4(rootList);
						ScebXCe_EC560647(rootList);
					}
					if (fqdnList != NULL) {
						ScebXCe_1F59F0C4(fqdnList);
						ScebXCe_EC560647(fqdnList);
					}
				}
			}
		}
	}

	if (respDoc != NULL) {
		ScebXCe_B0891B13(respDoc);
		ScebXCe_EC560647(respDoc);
	}
	if (allocatedNodes != NULL) {
		for (SceInt32 idx = 0; idx < allocatedCount; ++idx) {
			if (allocatedNodes[idx].hostName != NULL)
				npCommunityFree(const_cast<char *>(allocatedNodes[idx].hostName));
		}
		npCommunityFree(allocatedNodes);
	}
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}

/* 0x8133888E */
int npTusBackendGetServerList(SceInt32 txId, NpCommunityServerNode **outList)
{
	return npCommunityGetServerListEx(txId, outList, NULL);
}

/* 0x81338898 */
int npScoreBackendGetServerInfo(SceInt32 txId, const SceNpCommunicationId *commId,
				NpScoreServerHosts *outHosts)
{
	NpCommunityBackendTx *tx = NULL;
	BxceDocument *respDoc = NULL;
	int ret;

	if (commId == NULL || outHosts == NULL) {
		ret = NP_COMMUNITY_ERR_NULL_POINTER;
	} else {
		sceClibMemset(outHosts, 0, sizeof(*outHosts));
		ret = npCommunityAcquireBackendTx(txId, &tx);
		if (ret >= 0) {
			if (tx->serviceType == NP_COMMUNITY_TX_SCORE) {
				ret = npCommunityHttpOpen(tx, NP_COMMUNITY_EP_RANKING_SERVER_LIST,
							  commId, 0);
				if (ret >= 0)
					ret = npCommunityHttpSend(tx, NULL, 0, NULL);
				if (ret >= 0) {
					ret = npCommunityHttpRecvXml(
						tx, &respDoc,
						NP_COMMUNITY_SIG_COMMENT_STORAGE_LIST, NULL);
				}
				if (ret >= 0) {
					npCommunityCloseTxHttp(tx);
					BxceNodeId docRoot = ScebXCe_CED6BECF(respDoc);
					BxceNodeList *rankingList = ScebXCe_1FA24DC0(
						respDoc, docRoot, "ranking");
					if (rankingList != NULL) {
						BxceNodeId rankingNode = ScebXCe_6C46D786(
							rankingList, 0);
						ret = npCommunityXmlCheckServerStatus(
							respDoc, rankingNode, 2);
						if (ret >= 0) {
							ret = npCommunityXmlVerifyTitleId(
								respDoc, rankingNode, commId);
						}
						if (ret >= 0) {
							BxceNodeList *recordFqdnList =
								ScebXCe_1FA24DC0(respDoc,
										 rankingNode,
										 "record-fqdn");
							if (recordFqdnList != NULL) {
								BxceNodeId elem = ScebXCe_6C46D786(
									recordFqdnList, 0);
								BxceNodeId textNode =
									ScebXCe_FEA6E4A1(respDoc,
											 elem);
								const char *hostStr =
									(textNode != 0)
										? ScebXCe_33825EFF(
											  respDoc,
											  textNode)
										: NULL;
								if (hostStr != NULL &&
								    sceClibStrnlen(hostStr,
										   0x7FFFFFFFu) <=
									    0x7Fu) {
									sceClibStrncpy(
										outHosts->recordHost,
										hostStr, 0x7Fu);
									ret = 0;
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
								ScebXCe_1F59F0C4(recordFqdnList);
								ScebXCe_EC560647(recordFqdnList);
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
						if (ret >= 0) {
							BxceNodeList *viewFqdnList =
								ScebXCe_1FA24DC0(respDoc,
										 rankingNode,
										 "view-fqdn");
							if (viewFqdnList != NULL) {
								BxceNodeId elem = ScebXCe_6C46D786(
									viewFqdnList, 0);
								BxceNodeId textNode =
									ScebXCe_FEA6E4A1(respDoc,
											 elem);
								const char *hostStr =
									(textNode != 0)
										? ScebXCe_33825EFF(
											  respDoc,
											  textNode)
										: NULL;
								if (hostStr != NULL &&
								    sceClibStrnlen(hostStr,
										   0x7FFFFFFFu) <=
									    0x7Fu) {
									sceClibStrncpy(
										outHosts->viewHost,
										hostStr, 0x7Fu);
									ret = 0;
								} else {
									ret = NP_COMMUNITY_ERR_PARSE_FAIL;
								}
								ScebXCe_1F59F0C4(viewFqdnList);
								ScebXCe_EC560647(viewFqdnList);
							} else {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							}
						}
						ScebXCe_1F59F0C4(rankingList);
						ScebXCe_EC560647(rankingList);
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
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
	if (tx != NULL) {
		if (tx->aborted == 1)
			ret = NP_COMMUNITY_ERR_ABORTED;
		npCommunityReleaseBackendTx(tx);
	}
	return ret;
}
