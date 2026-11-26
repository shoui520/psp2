#include "np_matching2_internal.h"

/*
 * Transport layer, frame dispatch, HMAC verification, and per-command request
 * builders / response & notification parsers (0x0116F0..0x01744F).
 */

typedef s32 (*NpCmdBuildFn)(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
typedef s32 (*NpCmdParseFn)(NpMatching2Conn *conn, void *reqOrHdr, const u8 *payload, u32 payloadLen);

typedef struct NpCmdHandlerEntry {
    NpCmdBuildFn buildFn;
    NpCmdParseFn parseFn;
} NpCmdHandlerEntry;

typedef struct NpSockRegistry {
    s32 sockFds[64];
    s32 maxSockFd;
} NpSockRegistry;

typedef struct SceNpMatching2ServerInfoResp {
    SceNpMatching2ServerId serverId;
    u8 status;
    u8 padByte;
} SceNpMatching2ServerInfoResp;

typedef struct SceNpMatching2GetWorldInfoListResp {
    NpProtoWorldInfo *worldList;
    u32 worldNum;
} SceNpMatching2GetWorldInfoListResp;

typedef struct SceNpMatching2GetUserInfoListResp {
    SceNpMatching2UserInfoOut *userInfoList;
    u32 userInfoNum;
} SceNpMatching2GetUserInfoListResp;

typedef struct SceNpMatching2SearchRoomResp {
    u32 startIndex;
    u32 total;
    u32 roomDataExternalNum;
    SceNpMatching2RoomDataExternalOut *roomDataExternal;
} SceNpMatching2SearchRoomResp;

typedef struct SceNpMatching2GetRoomDataExtListResp {
    SceNpMatching2RoomDataExternalOut *roomDataExternal;
    u32 roomDataExternalNum;
} SceNpMatching2GetRoomDataExtListResp;

typedef struct SceNpMatching2GetRoomMemberExtListResp {
    NpProtoRoomMemberExternal *memberList;
    u32 memberNum;
} SceNpMatching2GetRoomMemberExtListResp;

typedef struct SceNpMatching2CreateJoinRoomResp {
    NpMatching2Room *roomDataInternal;
} SceNpMatching2CreateJoinRoomResp;

typedef struct SceNpMatching2JoinRoomResp {
    NpMatching2Room *roomDataInternal;
} SceNpMatching2JoinRoomResp;

typedef struct SceNpMatching2GetRoomMemberIntListResp {
    NpMatching2RoomMember *memberList;
    u32 memberNum;
    NpMatching2RoomMember *meMember;
    NpMatching2RoomMember *ownerMember;
} SceNpMatching2GetRoomMemberIntListResp;

typedef struct SceNpMatching2RoomDataInternalView {
    SceNpMatching2ServerId serverId;
    u16 serverPad;
    SceNpMatching2WorldId worldId;
    u64 lobbyId;
    SceNpMatching2RoomId roomId;
    u64 passwordSlotMask;
    u32 maxSlots;
    u32 curMemberNum;
    u32 reservedList;
    u32 reservedMe;
    SceNpMatching2RoomGroup *roomGroup;
    u32 roomGroupNum;
    u32 flagAttr;
    SceNpMatching2BinAttrInternal *binAttrInternal;
    u32 binAttrInternalNum;
} SceNpMatching2RoomDataInternalView;

typedef struct SceNpMatching2RoomChatResp {
    s32 filtered;
} SceNpMatching2RoomChatResp;

typedef struct SceNpMatching2RoomChatNotifyEvent {
    u8 castType;
    u8 padBytes[3];
    void *target;
    void *senderProfile;
    void *msg;
    u32 msgLen;
    s32 filtered;
} SceNpMatching2RoomChatNotifyEvent;

typedef struct SceNpMatching2RoomMsgNotifyEvent {
    u8 castType;
    u8 padBytes[3];
    void *target;
    void *senderProfile;
    void *msg;
    u32 msgLen;
    u32 tailPad;
} SceNpMatching2RoomMsgNotifyEvent;

typedef struct SceNpMatching2MemberJoinedEvent {
    NpMatching2RoomMember *roomMember;
    u32 reservedWord;
    NpProtoOptDataBlock optData;
} SceNpMatching2MemberJoinedEvent;

typedef struct SceNpMatching2MemberUpdateEvent {
    NpMatching2RoomMember *roomMember;
    u32 *newFlagAttr;
    u32 *prevFlagAttr;
    u8 *newTeamId;
    SceNpMatching2MemberBinAttrInternal **updatedBinAttr;
    u32 updatedBinAttrNum;
} SceNpMatching2MemberUpdateEvent;

typedef struct SceNpMatching2RoomDataUpdateEvent {
    NpMatching2Room *roomData;
    u32 *newFlagAttr;
    u32 *prevFlagAttr;
    u64 *newPasswordSlotMask;
    u64 *prevPasswordSlotMask;
    SceNpMatching2RoomGroup **updatedRoomGroup;
    u32 updatedRoomGroupNum;
    SceNpMatching2BinAttrInternal **updatedBinAttr;
    u32 updatedBinAttrNum;
} SceNpMatching2RoomDataUpdateEvent;

typedef struct SceNpMatching2MemberLeftEvent {
    NpMatching2RoomMember *roomMember;
    u8 cause;
    u8 padBytes[3];
    NpProtoOptDataBlock optData;
} SceNpMatching2MemberLeftEvent;

typedef struct SceNpMatching2KickedOutEvent {
    u8 cause;
    u8 padBytes[3];
    u32 reservedWord;
    NpProtoOptDataBlock optData;
} SceNpMatching2KickedOutEvent;

typedef struct SceNpMatching2RoomOwnerChangedEvent {
    SceNpMatching2RoomMemberId prevOwner;
    SceNpMatching2RoomMemberId newOwner;
    u8 cause;
    u8 padBytes[3];
    SceNpMatching2SessionPassword *roomPassword;
    NpProtoOptDataBlock optData;
} SceNpMatching2RoomOwnerChangedEvent;

typedef struct SceNpMatching2RoomDestroyedEvent {
    u8 cause;
    u8 padBytes[3];
    s32 errorCode;
    NpProtoOptDataBlock optData;
} SceNpMatching2RoomDestroyedEvent;

static const u16 g_defaultConnPorts[5] = { 443, 3478, 3478, 3478, 3478 };
static NpSockRegistry g_sockRegistry;
static u16 g_txSequence = 0;

/* Forward declarations for command dispatch tables */
static s32 NP_MATCHING2_NOINLINE npTransportLookupCmd(u16 protoCmd, const NpCmdHandlerEntry **entryOut);
static s32 NP_MATCHING2_NOINLINE npTransportHandleResponse(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                           u32 maxBufLen, u32 totalLen, u32 hdrLen);
static s32 NP_MATCHING2_NOINLINE npTransportHandleNotify(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                         u32 maxBufLen, u32 totalLen, u32 hdrLen);
static s32 NP_MATCHING2_NOINLINE npTransportMapNotifyEvent(u16 protoCmd);
static s32 NP_MATCHING2_NOINLINE npTransportVerifyHmac(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                       u16 protoCmd, u8 *hmacField);
static s32 NP_MATCHING2_NOINLINE npTransportParseFrameHdr(const u8 *buf, u32 bufLen,
                                                          NpProtoFrameHeader *hdrOut, u16 *bodyLenOut);
static s32 NP_MATCHING2_NOINLINE npTransportParseRespHdr(const u8 *buf, u32 bufLen, NpProtoBodyHeader *hdrOut);
static s32 NP_MATCHING2_NOINLINE npTransportParseNotifyHdr(const u8 *buf, u32 bufLen, NpProtoBodyHeader *hdrOut);

static s32 NP_MATCHING2_NOINLINE npBuildAuthReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildCreateJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSendRoomChatReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSendRoomMsgReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetServerInfoReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildKeepaliveReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSetUserInfoReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetUserInfoListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetWorldInfoListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSearchRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildCreateRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildJoinRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildLeaveRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildUpdateRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberExtListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomDataExtListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildReserveRoomSlotReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildReleaseRoomSlotReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildReserveJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildRoomKeepaliveReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildLeaveRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberIntListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataIntDispatch(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomMemberDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildGrantRoomOwnerReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildKickoutRoomMemberReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize);

static s32 NP_MATCHING2_NOINLINE npParseGetServerInfoResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseAuthResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetUserInfoListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSearchRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseCreateRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetRoomDataExtListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseReserveRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomKeepaliveResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseCreateJoinRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseJoinRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseLeaveRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberIntListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetRoomDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetRoomMemberDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSendRoomChatResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomChatNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomMsgNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseMemberJoinedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseMemberUpdateNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomDataUpdateNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseMemberLeftNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomOwnerChangedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseRoomDestroyedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseKeepaliveResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetUserInfoResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetWorldInfoListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseJoinRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseLeaveRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseUpdateRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberExtListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseReleaseRoomSlotResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseEmptyResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataIntDispatch(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSendRoomMsgResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseGrantRoomOwnerResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseKickoutRoomMemberResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);
static s32 NP_MATCHING2_NOINLINE npParseSetSigOptResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen);

/* Command dispatch tables indexed by low byte of protoCmd */
static const NpCmdHandlerEntry g_cmdTableSession[9] = {
    { NULL,                         NULL },
    { npBuildGetServerInfoReq,      npParseGetServerInfoResp },
    { npBuildAuthReq,               npParseAuthResp },
    { npBuildKeepaliveReq,          npParseKeepaliveResp },
    { NULL,                         NULL },
    { NULL,                         NULL },
    { npBuildSetUserInfoReq,        npParseSetUserInfoResp },
    { npBuildGetUserInfoListReq,    npParseGetUserInfoListResp },
    { npBuildGetWorldInfoListReq,   npParseGetWorldInfoListResp },
};

static const NpCmdHandlerEntry g_cmdTableLobbyExt[11] = {
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
};

static const NpCmdHandlerEntry g_cmdTableRoomExt[13] = {
    { npBuildSearchRoomReq,             npParseSearchRoomResp },
    { NULL,                             NULL },
    { npBuildCreateRoomExtReq,          npParseCreateRoomExtResp },
    { npBuildJoinRoomExtReq,            npParseJoinRoomExtResp },
    { npBuildLeaveRoomExtReq,           npParseLeaveRoomExtResp },
    { npBuildUpdateRoomExtReq,          npParseUpdateRoomExtResp },
    { npBuildGetRoomMemberExtListReq,   npParseGetRoomMemberExtListResp },
    { npBuildSetRoomDataExtReq,         npParseSetRoomDataExtResp },
    { npBuildGetRoomDataExtListReq,     npParseGetRoomDataExtListResp },
    { npBuildReserveRoomSlotReq,        npParseReserveRoomResp },
    { npBuildReleaseRoomSlotReq,        npParseReleaseRoomSlotResp },
    { npBuildReserveJoinRoomReq,        npParseEmptyResp },
    { npBuildSearchRoomReq,             npParseSearchRoomResp },
};

static const NpCmdHandlerEntry g_cmdTableRoomKeepalive[2] = {
    { NULL,                     NULL },
    { npBuildRoomKeepaliveReq,  npParseRoomKeepaliveResp },
};

static const NpCmdHandlerEntry g_cmdTableLobbyInt[20] = {
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
};

static const NpCmdHandlerEntry g_cmdTableRoomInt[20] = {
    { npBuildCreateJoinRoomReq,         npParseCreateJoinRoomResp },
    { npBuildJoinRoomReq,               npParseJoinRoomResp },
    { npBuildLeaveRoomReq,              npParseLeaveRoomResp },
    { npBuildGetRoomMemberIntListReq,   npParseGetRoomMemberIntListResp },
    { npBuildSetRoomDataIntDispatch,    npParseSetRoomDataIntDispatch },
    { npBuildGetRoomDataIntReq,         npParseGetRoomDataIntResp },
    { npBuildSetRoomMemberDataIntReq,   npParseSetRoomMemberDataIntResp },
    { npBuildGetRoomMemberDataIntReq,   npParseGetRoomMemberDataIntResp },
    { npBuildSendRoomChatReq,           npParseSendRoomChatResp },
    { NULL,                             npParseRoomChatNotify },
    { npBuildSendRoomMsgReq,            npParseSendRoomMsgResp },
    { NULL,                             npParseRoomMsgNotify },
    { NULL,                             npParseMemberJoinedNotify },
    { NULL,                             npParseMemberUpdateNotify },
    { NULL,                             npParseRoomDataUpdateNotify },
    { NULL,                             npParseMemberLeftNotify },
    { npBuildGrantRoomOwnerReq,         npParseGrantRoomOwnerResp },
    { npBuildKickoutRoomMemberReq,      npParseKickoutRoomMemberResp },
    { NULL,                             npParseRoomOwnerChangedNotify },
    { NULL,                             npParseRoomDestroyedNotify },
};

static const NpCmdHandlerEntry g_cmdTableServerNotify[3] = {
    { NULL, NULL }, { NULL, NULL }, { NULL, NULL },
};

static const NpCmdHandlerEntry g_cmdTableRoomPresence[2] = {
    { NULL, NULL }, { NULL, NULL },
};

/* 0x0116F0 */
s32 NP_MATCHING2_NOINLINE npTransportResolveHost(NpMatching2Conn *conn, s32 *isNumericOut, u32 hostIndex)
{
    const char *hostStr;
    u32 numericAddr;
    s32 ret;

    *isNumericOut = 0;
    hostStr = conn->hostList.entries[hostIndex].host;
    if (hostStr == NULL) {
        return (s32)0x80550ca0;
    }

    numericAddr = sceNetInetInetAddr(hostStr);
    if (numericAddr != 0xFFFFFFFFu) {
        conn->peerAddr.sinAddr = numericAddr;
        *isNumericOut = 1;
        return 0;
    }

    conn->resolverBuf = npMatching2Alloc(768);
    ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    if (conn->resolverBuf != NULL) {
        ret = sceNetResolverCreate(&conn->resolverId, conn->resolverBuf, 768);
        if (ret >= 0) {
            ret = sceNetResolverStartNtoAAsync(conn->resolverId,
                                               conn->hostList.entries[hostIndex].host,
                                               &conn->peerAddr.sinAddr,
                                               3000000,
                                               10);
            if (ret >= 0) {
                return 0;
            }
        }
    }

    if (conn->resolverId >= 0) {
        sceNetResolverDelete(conn->resolverId);
        conn->resolverId = -1;
    }
    if (conn->resolverBuf != NULL) {
        npMatching2Free(conn->resolverBuf);
        conn->resolverBuf = NULL;
    }
    return ret;
}

/* 0x011800 */
s32 NP_MATCHING2_NOINLINE npTransportOpenSocket(NpMatching2Conn *conn)
{
    s32 sockFd;
    s32 optVal;
    s32 nonBlock;
    s32 ret;
    s32 errCode;
    s32 slot;
    s32 maxRemaining;
    s32 wasMax;

    if (conn->connState != 4) {
        return (s32)0x80550ca0;
    }

    sockFd = sceNetInetSocket(2, 1, 0);
    if (sockFd >= 0) {
        conn->sockFd = sockFd;
        optVal = 1;
        if (sceNetInetSetsockopt(sockFd, 6, 1, &optVal, 4) >= 0) {
            for (slot = 0; slot < 64; slot++) {
                if (g_sockRegistry.sockFds[slot] == 0) {
                    g_sockRegistry.sockFds[slot] = sockFd;
                    break;
                }
            }
            if (slot != 64 && g_sockRegistry.maxSockFd < sockFd) {
                g_sockRegistry.maxSockFd = sockFd;
            }
            npWorkerRegisterSockFd(conn->sockFd);

            nonBlock = 1;
            ret = sceNetInetSetsockopt(conn->sockFd, 0xFFFF, 4105, &nonBlock, 4);
            if (ret < 0) {
                ret = sceNetInetGetPspError();
            }
            errCode = ret;
            if (ret >= 0) {
                ret = sceNetInetConnect(conn->sockFd, &conn->peerAddr, 16);
                if (ret >= 0) {
                    return 0;
                }
                if (sceNetInetGetPspError() == (s32)0x80010077) {
                    return 0;
                }
                errCode = sceNetInetGetPspError();
            }
        } else {
            errCode = sceNetInetGetPspError();
        }
    } else {
        errCode = sceNetInetGetPspError();
    }

    if (conn->resolverId >= 0) {
        sceNetResolverStop(conn->resolverId);
        sceNetResolverDelete(conn->resolverId);
        conn->resolverId = -1;
    }
    if (conn->resolverBuf != NULL) {
        npMatching2Free(conn->resolverBuf);
        conn->resolverBuf = NULL;
    }
    if (conn->sockFd <= 0) {
        return errCode;
    }

    npWorkerUnregisterSockFd(conn->sockFd);
    sockFd = conn->sockFd;
    maxRemaining = 0;
    wasMax = (sockFd == g_sockRegistry.maxSockFd);
    for (slot = 0; slot < 64; slot++) {
        s32 curSock = g_sockRegistry.sockFds[slot];
        if (curSock >= maxRemaining && curSock != g_sockRegistry.maxSockFd) {
            maxRemaining = curSock;
        }
        if (curSock == sockFd) {
            g_sockRegistry.sockFds[slot] = 0;
            if (!wasMax) {
                break;
            }
        }
    }
    if (wasMax) {
        g_sockRegistry.maxSockFd = maxRemaining;
    }

    sceNetInetClose(conn->sockFd);
    conn->sockFd = -1;
    if (conn->connState >= 3) {
        conn->connState = 3;
    } else {
        conn->peerAddr.sinAddr = 0;
        conn->connState = 0;
    }
    return errCode;
}

/* 0x011A70 */
s32 NP_MATCHING2_NOINLINE npTransportInit(void)
{
    sceKernelMemset(&g_sockRegistry, 0, sizeof(g_sockRegistry));
    return 0;
}

/* 0x011A9C */
s32 NP_MATCHING2_NOINLINE npTransportTerm(void)
{
    return 0;
}

/* 0x011AA4 */
s32 NP_MATCHING2_NOINLINE npTransportClearNonBlocking(s32 sockFd)
{
    s32 nonBlock = 0;
    s32 ret = sceNetInetSetsockopt(sockFd, 0xFFFF, 4105, &nonBlock, 4);
    if (ret < 0) {
        return sceNetInetGetPspError();
    }
    return ret;
}

/* 0x011AE8 */
s32 NP_MATCHING2_NOINLINE npConnSetKind(u32 connKind, NpMatching2Conn *conn)
{
    conn->peerAddr.sinLen = 16;
    conn->peerAddr.sinFamily = 2;
    conn->sockFd = -1;
    conn->connKind = connKind;
    if (conn->hostList.count != 0) {
        u16 port = conn->hostList.entries[0].port;
        if (port != 0) {
            conn->peerAddr.sinPort = npSwap16(port);
        } else {
            conn->peerAddr.sinPort = npSwap16(g_defaultConnPorts[connKind]);
        }
        return 0;
    }
    return (s32)0x80550ca0;
}

/* 0x011B58 */
s32 NP_MATCHING2_NOINLINE npTransportSelectHostPort(NpMatching2Conn *conn, u32 hostIndex)
{
    if (hostIndex < conn->hostList.count) {
        u16 port = conn->hostList.entries[hostIndex].port;
        if (port == 0) {
            port = g_defaultConnPorts[conn->connKind];
        }
        conn->peerAddr.sinPort = npSwap16(port);
        return 0;
    }
    return (s32)0x80550ca0;
}

/* 0x011BBC */
s32 NP_MATCHING2_NOINLINE npConnResetTransport(NpMatching2Conn *conn)
{
    s32 sockFd;
    s32 slot;
    s32 maxRemaining;
    s32 wasMax;

    if (conn->resolverId >= 0) {
        sceNetResolverStop(conn->resolverId);
        sceNetResolverDelete(conn->resolverId);
        conn->resolverId = -1;
    }
    if (conn->resolverBuf != NULL) {
        npMatching2Free(conn->resolverBuf);
        conn->resolverBuf = NULL;
    }
    if (conn->sockFd <= 0) {
        return (s32)0x80550ca0;
    }

    npWorkerUnregisterSockFd(conn->sockFd);
    sockFd = conn->sockFd;
    maxRemaining = 0;
    wasMax = (sockFd == g_sockRegistry.maxSockFd);
    for (slot = 0; slot < 64; slot++) {
        s32 curSock = g_sockRegistry.sockFds[slot];
        if (curSock >= maxRemaining && curSock != g_sockRegistry.maxSockFd) {
            maxRemaining = curSock;
        }
        if (curSock == sockFd) {
            g_sockRegistry.sockFds[slot] = 0;
            if (!wasMax) {
                break;
            }
        }
    }
    if (wasMax) {
        g_sockRegistry.maxSockFd = maxRemaining;
    }

    sceNetInetClose(conn->sockFd);
    conn->sockFd = -1;
    if (conn->connState >= 3) {
        conn->connState = 3;
    } else {
        conn->peerAddr.sinAddr = 0;
        conn->connState = 0;
    }
    return 0;
}

/* 0x011CF8 */
s32 NP_MATCHING2_NOINLINE npConnMarkConnectPending(NpMatching2Conn *conn)
{
    if (conn->connState == 0) {
        conn->connState = 1;
    } else if (conn->connState == 3) {
        conn->connState = 4;
    }
    return 0;
}

/* 0x011D20 */
s32 NP_MATCHING2_NOINLINE npTransportPollResolver(NpMatching2Conn *conn, s32 *doneOut)
{
    s32 ret;

    *doneOut = 0;
    ret = sceNetResolverPollAsync(conn->resolverId, NULL);
    if (ret == 0) {
        *doneOut = 1;
    }
    if (ret <= 0) {
        sceNetResolverDelete(conn->resolverId);
        conn->resolverId = -1;
        npMatching2Free(conn->resolverBuf);
        conn->resolverBuf = NULL;
    }
    return ret;
}

/* 0x011DA4 */
s32 NP_MATCHING2_NOINLINE npTransportGetMaxSockFdPlusOne(void)
{
    return g_sockRegistry.maxSockFd + 1;
}

/* 0x011DB4 */
s32 NP_MATCHING2_NOINLINE npTransportParseFrames(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                               u32 maxBufLen, u32 *neededLenOut, u32 *consumedOut)
{
    u32 consumed = 0;
    s32 ret = 0;
    NpProtoFrameHeader frameHdr;
    u16 bodyLen = 0;

    while (1) {
        u8 msgType;
        u16 totalLen;
        s32 handleRet;

        while (pktLen != 0 && *pktBuf == 0) {
            pktBuf++;
            pktLen--;
            consumed++;
        }
        if (pktLen == 0) {
            *consumedOut = consumed;
            return ret;
        }
        if (pktLen < 4) {
            ret = SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT;
            *neededLenOut = 4;
            break;
        }

        ret = npTransportParseFrameHdr(pktBuf, pktLen, &frameHdr, &bodyLen);
        if (ret < 0) {
            break;
        }

        totalLen = frameHdr.totalLen;
        msgType = (u8)(frameHdr.versionWord >> 8);
        *neededLenOut = totalLen;

        if (msgType == 2) {
            handleRet = npTransportHandleResponse(conn, pktBuf, pktLen, maxBufLen, totalLen, 4);
        } else if (msgType == 4) {
            handleRet = npTransportHandleNotify(conn, pktBuf, pktLen, maxBufLen, totalLen, 4);
        } else {
            pktBuf += totalLen;
            consumed += totalLen;
            pktLen -= totalLen;
            continue;
        }

        ret = handleRet;
        if (handleRet < 0) {
            if (handleRet == SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT) {
                break;
            }
            if (pktLen < totalLen) {
                ret = (s32)0x80550ccd;
                break;
            }
            ret = 0;
        }

        pktBuf += totalLen;
        consumed += totalLen;
        pktLen -= totalLen;
    }

    *consumedOut = consumed;
    return ret;
}

/* 0x011F34 */
s32 NP_MATCHING2_NOINLINE npTransportBuildReqPacket(NpMatching2Conn *conn, NpMatching2Req *req,
                                                    u8 *buf, u32 bufSize, s32 *writtenLenOut)
{
    const NpCmdHandlerEntry *entry = NULL;
    s32 ret;

    g_txSequence++;
    if (g_txSequence == 0xFFFFu) {
        g_txSequence = 1;
    }
    req->txId = ((u32)req->reqSource << 28) | ((u32)req->reqKind << 20) | (u32)g_txSequence;

    ret = npTransportLookupCmd(req->protoCmd, &entry);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    ret = entry->buildFn(conn, req, buf, bufSize);
    if (ret >= 0 && writtenLenOut != NULL) {
        *writtenLenOut = ret;
    }
    return ret;
}

/* 0x012030 */
static s32 NP_MATCHING2_NOINLINE npTransportLookupCmd(u16 protoCmd, const NpCmdHandlerEntry **entryOut)
{
    u8 lowByte = (u8)(protoCmd & 0xFFu);

    switch ((u8)(protoCmd >> 8)) {
        case 0x10:
            *entryOut = &g_cmdTableSession[lowByte];
            return 0;
        case 0x11:
            *entryOut = &g_cmdTableLobbyExt[lowByte];
            return 0;
        case 0x12:
            *entryOut = &g_cmdTableRoomExt[lowByte];
            return 0;
        case 0x20:
            *entryOut = &g_cmdTableRoomKeepalive[lowByte];
            return 0;
        case 0x31:
            *entryOut = &g_cmdTableLobbyInt[lowByte];
            return 0;
        case 0x32:
            *entryOut = &g_cmdTableRoomInt[lowByte];
            return 0;
        case 0x33:
            *entryOut = &g_cmdTableServerNotify[lowByte];
            return 0;
        case 0x40:
            *entryOut = &g_cmdTableRoomPresence[lowByte];
            return 0;
        default:
            return (s32)0x80550cb0;
    }
}

/* 0x01211C */
static s32 NP_MATCHING2_NOINLINE npTransportHandleResponse(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                           u32 maxBufLen, u32 totalLen, u32 hdrLen)
{
    NpProtoBodyHeader respHdr;
    NpMatching2Req *req = NULL;
    const NpCmdHandlerEntry *entry = NULL;
    NpMatching2Conn *nextConn = NULL;
    u8 *bodyPtr = pktBuf + hdrLen;
    u32 bodyLen = pktLen - hdrLen;
    u8 *payloadPtr;
    u32 payloadLen;
    s32 ret = SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT;
    s32 rmRet;

    if (bodyLen < 28) {
        return ret;
    }

    ret = npTransportParseRespHdr(bodyPtr, bodyLen, &respHdr);
    if (ret < 0) {
        return ret;
    }

    payloadPtr = bodyPtr + ret;
    payloadLen = (totalLen < pktLen) ? (totalLen - hdrLen - (u32)ret) : (bodyLen - (u32)ret);

    if (pktLen >= totalLen) {
        s32 skipHmac = (respHdr.flags == 14) ||
                       ((respHdr.cmd == 0x1001 || respHdr.cmd == 0x1007) && respHdr.flags == 30);
        if (!skipHmac) {
            ret = npTransportVerifyHmac(conn, pktBuf, totalLen, respHdr.cmd, bodyPtr + 12);
            if (ret < 0) {
                return ret;
            }
        }
    } else if (maxBufLen >= totalLen) {
        return SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT;
    }

    ret = npReqFindByTxId(respHdr.txId, conn, &req);
    if (ret < 0) {
        return ret;
    }

    if (maxBufLen < totalLen) {
        ret = (s32)0x80550ccd;
    } else if (respHdr.flags != 0 && respHdr.flags != 13) {
        ret = (s32)(0x80550D00u | respHdr.flags);
        if (ret == (s32)0x80550D0C) {
            ret = (s32)0x80550007;
        }
    } else {
        ret = npTransportLookupCmd(respHdr.cmd, &entry);
    }

    if (ret < 0) {
        rmRet = npReqRemoveFromQueue(req, &conn->waitQueue, conn, NULL);
        if (req->protoCmd == 0x1203) {
            npRoomAbortJoinOnError(req, ret, NULL);
        } else {
            npWorkerDispatchReqCb(req, ret, NULL);
        }
        if (rmRet >= 0) {
            npReqAbortOnSubmitFail(conn->server->ctx, req, ret);
        }
        npReqRelease(req);
        return ret;
    }

    ret = entry->parseFn(conn, req, payloadPtr, payloadLen);
    if (ret < 0) {
        rmRet = npReqRemoveFromQueue(req, &conn->waitQueue, conn, NULL);
        if (rmRet >= 0) {
            npReqAbortOnSubmitFail(conn->server->ctx, req, ret);
        }
        npReqRelease(req);
        return ret;
    }

    if (req->protoCmd == 0) {
        ret = npReqRemoveFromQueue(req, &conn->waitQueue, conn, NULL);
        if (ret >= 0) {
            npReqAbortOnSubmitFail(conn->server->ctx, req, 0);
        }
        npReqRelease(req);
        return ret;
    }

    npLockAcquire();
    ret = npReqRemoveFromQueue(req, &conn->waitQueue, conn, NULL);
    if (ret < 0) {
        npLockRelease();
        npWorkerDispatchReqCb(req, ret, NULL);
        npReqRelease(req);
        return ret;
    }

    ret = npConnFindAndRef(conn->server->ctx->ctxId, req->nextConnId, &nextConn);
    if (ret >= 0) {
        req->nextConnId = 0;
        ret = npReqRebindConn(conn->server->ctx->ctxId, req, nextConn);
        if (ret >= 0) {
            ret = npReqEnqueue(req, nextConn, &nextConn->sendQueue);
            if (ret >= 0) {
                npWorkerWakeup();
            }
        }
    }
    if (nextConn != NULL) {
        npConnRelease(nextConn);
    }
    if (ret < 0) {
        npLockRelease();
        npWorkerDispatchReqCb(req, ret, NULL);
        npReqAbortOnSubmitFail(conn->server->ctx, req, ret);
    } else {
        npLockRelease();
    }

    npReqRelease(req);
    return ret;
}

/* 0x0124C4 */
static s32 NP_MATCHING2_NOINLINE npTransportHandleNotify(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                         u32 maxBufLen, u32 totalLen, u32 hdrLen)
{
    NpProtoBodyHeader notifyHdr;
    const NpCmdHandlerEntry *entry = NULL;
    NpMatching2Room *room = NULL;
    u8 *bodyPtr = pktBuf + hdrLen;
    u32 bodyLen = pktLen - hdrLen;
    u8 *payloadPtr;
    u32 payloadLen;
    s32 ret = (s32)0x80550ca4;

    if (bodyLen < 28) {
        return ret;
    }

    ret = npTransportParseNotifyHdr(bodyPtr, bodyLen, &notifyHdr);
    if (ret < 0) {
        return ret;
    }

    payloadPtr = bodyPtr + ret;
    payloadLen = (totalLen < pktLen) ? (totalLen - hdrLen - (u32)ret) : (bodyLen - (u32)ret);

    if (pktLen >= totalLen) {
        ret = npTransportVerifyHmac(conn, pktBuf, totalLen, notifyHdr.cmd, bodyPtr + 12);
        if (ret >= 0) {
            ret = npTransportLookupCmd(notifyHdr.cmd, &entry);
            if (ret >= 0) {
                return entry->parseFn(conn, &notifyHdr, payloadPtr, payloadLen);
            }
        }
        return ret;
    }

    if (maxBufLen < totalLen) {
        ret = (s32)0x80550ccd;
        if (npRoomGetFromConn(conn, &room) >= 0) {
            s32 evId = npTransportMapNotifyEvent(notifyHdr.cmd);
            if (evId != 0) {
                npWorkerDispatchRoomErrorEvent((s32)(notifyHdr.txId >> 28), room, (s32)0x80550ccd, (u32)evId, totalLen);
                return (s32)0x80550ccd;
            }
        }
        return ret;
    }
    return (s32)0x80550ca4;
}

/* 0x012648 */
static s32 NP_MATCHING2_NOINLINE npTransportMapNotifyEvent(u16 protoCmd)
{
    switch (protoCmd) {
        case 0x3209: return 0x2101;
        case 0x320B: return 0x2102;
        case 0x320C: return 0x1101;
        case 0x320D: return 0x1107;
        case 0x320E: return 0x1106;
        case 0x320F: return 0x1102;
        case 0x3212: return 0x1105;
        case 0x3213: return 0x1104;
        default:     return 0;
    }
}

/* 0x0126BC */
static s32 NP_MATCHING2_NOINLINE npTransportVerifyHmac(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen,
                                                       u16 protoCmd, u8 *hmacField)
{
    const void *keyPtr = NULL;
    u32 keyLen = 0;
    NpMatching2Room *room = NULL;
    u8 expectedHmac[16];
    u8 computedDigest[32];
    s32 ret;

    switch ((u8)(protoCmd >> 8)) {
        case 0x10:
            if (protoCmd == 0x1002) {
                return 0;
            }
            if (protoCmd == 0x1001 || protoCmd == 0x1007) {
                keyPtr = conn->server->ctx->passPhrase.data;
                keyLen = 16;
            } else {
                keyPtr = conn->server->sessionKey.bytes;
                keyLen = conn->server->sessionKey.len;
            }
            break;
        case 0x11:
        case 0x12:
        case 0x20:
            keyPtr = conn->server->sessionKey.bytes;
            keyLen = conn->server->sessionKey.len;
            break;
        case 0x31:
            if (protoCmd == 0x3100 || protoCmd == 0x3101) {
                keyPtr = conn->server->sessionKey.bytes;
                keyLen = conn->server->sessionKey.len;
            }
            break;
        case 0x32:
            if (protoCmd == 0x3200 || protoCmd == 0x3201) {
                keyPtr = conn->server->sessionKey.bytes;
                keyLen = conn->server->sessionKey.len;
            } else {
                ret = npRoomGetFromConn(conn, &room);
                if (ret < 0) {
                    return ret;
                }
                keyPtr = room->roomKey.bytes;
                keyLen = room->roomKey.len;
            }
            break;
        case 0x33:
        case 0x40:
            break;
        default:
            return 0;
    }

    sceKernelMemcpy(expectedHmac, hmacField, 16);
    sceKernelMemset(hmacField, 0, 16);
    sceNet_lib_384EFE14(pktBuf, pktLen, keyPtr, keyLen, computedDigest);
    if (sceNet_lib_E0A81C7C(expectedHmac, computedDigest, 16) == 0) {
        return 0;
    }
    return (s32)0x80550cbc;
}

/* 0x012838 */
static s32 NP_MATCHING2_NOINLINE npBuildAuthReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpMatching2Context *ctx = conn->server->ctx;
    NpMatching2UserProfile *profile = NULL;
    const void *ticketPtr = NULL;
    char clientVer[16];
    u8 psid[16];
    const char *onlineName;
    const char *avatarUrl;
    u16 ticketLen;
    s32 ret;

    ret = sceNpCore_52440ABF(clientVer);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }
    ret = npWorkerGetLocalProfile(&profile);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }
    ret = sceNp_9B87B19B(&ticketPtr);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }
    ticketLen = (u16)ret;
    ret = sceOpenPSIDGetOpenPSID(psid);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    onlineName = ((ctx->option & 1) != 0) ? profile->onlineName : NULL;
    avatarUrl = ((ctx->option & 2) != 0) ? profile->avatarUrl : NULL;

    ret = npProtoBuildAuthReq(outBuf, outBufSize, req->txId, clientVer,
                              &ctx->commId, conn->server->serverId,
                              &profile->npId, onlineName, avatarUrl,
                              &ctx->passPhrase, ticketPtr, ticketLen, psid);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x01298C */
static s32 NP_MATCHING2_NOINLINE npBuildCreateJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)req->paramCopy;
    NpMatching2UserProfile *profile = NULL;
    u64 passwordMask = 0;
    s32 hasPasswordMask = 0;
    const void *publicMeta;
    const u64 *maskPtr;
    s32 ret;

    ret = npWorkerGetLocalProfile(&profile);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    if (wrap->req->groupConfig != NULL) {
        ret = npWorkerEncodeRoomGroups((const NpMatching2GroupConfigInput *)wrap->req->groupConfig,
                                       wrap->req->groupConfigNum,
                                       wrap->groupBitmap,
                                       &wrap->groupConfigs,
                                       &wrap->groupConfigCount,
                                       &passwordMask);
        if (ret < 0) {
            npWorkerDispatchReqCb(req, ret, NULL);
            return ret;
        }
        wrap->hasGroupBitmap = 1;
        hasPasswordMask = (wrap->req->roomPassword != NULL);
    } else {
        wrap->hasGroupBitmap = 0;
        if (wrap->req->roomPassword != NULL) {
            hasPasswordMask = 1;
            if (wrap->req->passwordSlotMask != NULL) {
                passwordMask = *wrap->req->passwordSlotMask;
            }
        }
    }

    maskPtr = hasPasswordMask ? &passwordMask : NULL;
    publicMeta = (wrap->hasGroupBitmap == 1) ? wrap->groupBitmap : NULL;

    ret = npProtoBuildCreateJoinRoomReq(outBuf, outBufSize, req->txId,
                                        conn->server->sessionId,
                                        &conn->server->sessionKey,
                                        &conn->server->sessionToken,
                                        &profile->npId,
                                        &conn->server->keepaliveCookie,
                                        &wrap->roomTicket,
                                        wrap->req->flagAttr, 1,
                                        wrap->req->maxSlot,
                                        publicMeta,
                                        wrap->req->roomBinAttrInternal,
                                        wrap->req->roomBinAttrInternalNum,
                                        wrap->req->roomPassword,
                                        maskPtr,
                                        wrap->groupConfigs,
                                        wrap->groupConfigCount,
                                        (const char (*)[20])wrap->allowedHandles,
                                        wrap->req->allowedUserNum,
                                        (const char (*)[20])wrap->blockedHandles,
                                        wrap->req->blockedUserNum,
                                        (const NpMatching2BlobBuf *)wrap->sigOptBlob,
                                        wrap->req->teamId,
                                        2,
                                        wrap->req->roomMemberBinAttrInternal,
                                        NULL,
                                        wrap->req->joinRoomGroupLabel);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x012BA8 */
static s32 NP_MATCHING2_NOINLINE npBuildSendRoomChatReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSendRoomChatWrap *wrap = (NpSendRoomChatWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpMatching2UserProfile *profile = NULL;
    NpProtoMsgOptData optData;
    u8 castMask[16];
    const void *maskPtr;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npWorkerBuildCastMask(room, wrap->req->castType, &wrap->req->target, castMask)) < 0 ||
        (ret = npWorkerGetLocalProfile(&profile)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    sceKernelMemset(&optData, 0, sizeof(optData));
    optData.flag = wrap->req->castType;
    if ((wrap->req->option & 7) != 0) {
        optData.hasProfile = 1;
        sceKernelMemcpy(&optData.npId, &profile->npId, sizeof(SceNpId));
        if ((wrap->req->option & 2) != 0) {
            optData.onlineName = profile->onlineName;
        }
        if ((wrap->req->option & 4) != 0) {
            optData.avatarUrl = profile->avatarUrl;
        }
    }

    maskPtr = (wrap->req->castType != 1) ? castMask : NULL;
    ret = npProtoBuildSendRoomChatReq(outBuf, outBufSize, req->txId,
                                      room->roomHandle, room->meMember->memberId,
                                      &room->roomKey, maskPtr,
                                      wrap->req->msg, (u16)wrap->req->msgLen, &optData);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x012D38 */
static s32 NP_MATCHING2_NOINLINE npBuildSendRoomMsgReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSendRoomMsgWrap *wrap = (NpSendRoomMsgWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpMatching2UserProfile *profile = NULL;
    NpProtoMsgOptData optData;
    u8 castMask[16];
    const void *maskPtr;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npWorkerBuildCastMask(room, wrap->req->castType, &wrap->req->target, castMask)) < 0 ||
        (ret = npWorkerGetLocalProfile(&profile)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    sceKernelMemset(&optData, 0, sizeof(optData));
    optData.flag = wrap->req->castType;
    if ((wrap->req->option & 7) != 0) {
        optData.hasProfile = 1;
        sceKernelMemcpy(&optData.npId, &profile->npId, sizeof(SceNpId));
        if ((wrap->req->option & 2) != 0) {
            optData.onlineName = profile->onlineName;
        }
        if ((wrap->req->option & 4) != 0) {
            optData.avatarUrl = profile->avatarUrl;
        }
    }

    maskPtr = (wrap->req->castType != 1) ? castMask : NULL;
    ret = npProtoBuildSendRoomMsgReq(outBuf, outBufSize, 0, req->txId,
                                     room->roomHandle, room->meMember->memberId,
                                     &room->roomKey, maskPtr,
                                     wrap->req->msg, (u16)wrap->req->msgLen, &optData);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x012ED4 */
static s32 NP_MATCHING2_NOINLINE npBuildGetServerInfoReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    SceNpMatching2ServerInfoResp resp;
    const SceNpMatching2GetServerInfoRequest *param = (const SceNpMatching2GetServerInfoRequest *)req->paramCopy;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    if (req->isMaintenance == 1) {
        resp.serverId = param->serverId;
        resp.status = 4;
        npWorkerDispatchReqCb(req, 0, &resp);
        return 0;
    }

    ret = npProtoBuildGetServerInfoReq(outBuf, outBufSize, req->txId,
                                       conn->server->ctx->passPhrase.data,
                                       &conn->server->ctx->commId,
                                       conn->server->serverId);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x012FA8 */
static s32 NP_MATCHING2_NOINLINE npBuildKeepaliveReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpMatching2ServerNode *server = conn->server;
    u16 worldIds[64];
    u32 count = 0;

    if (server->ownedRoomCount != 0) {
        u32 idx;
        for (idx = 0; idx < 64; idx++) {
            if (server->ownedWorldCounts[idx] != 0) {
                worldIds[count++] = (u16)(idx + 1);
            }
        }
    }

    return npProtoBuildKeepaliveReq(outBuf, outBufSize, req->txId,
                                    server->sessionId, &server->sessionKey,
                                    (count != 0) ? worldIds : NULL, count);
}

/* 0x013024 */
static s32 NP_MATCHING2_NOINLINE npBuildSetUserInfoReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSetUserInfoWrap *wrap = (NpSetUserInfoWrap *)req->paramCopy;
    s32 ret;

    ret = npProtoBuildSetUserInfoReq(outBuf, outBufSize, req->txId,
                                     conn->server->sessionId, &conn->server->sessionKey,
                                     wrap->req->userBinAttr);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x01309C */
static s32 NP_MATCHING2_NOINLINE npBuildGetUserInfoListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGetUserInfoWrap *wrap = (NpGetUserInfoWrap *)req->paramCopy;
    s32 ret;

    ret = npProtoBuildGetUserInfoListReq(outBuf, outBufSize, req->txId,
                                         conn->server->sessionId,
                                         conn->server->ctx->passPhrase.data,
                                         &conn->server->ctx->commId,
                                         wrap->req->serverId,
                                         (const char (*)[20])wrap->npHandles,
                                         wrap->req->npIdNum,
                                         wrap->req->attrId,
                                         wrap->req->attrIdNum);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013140 */
static s32 NP_MATCHING2_NOINLINE npBuildGetWorldInfoListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    s32 ret;

    ret = npProtoBuildGetWorldInfoListReq(outBuf, outBufSize, req->txId,
                                          conn->server->sessionId, &conn->server->sessionKey);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x0131AC */
static s32 NP_MATCHING2_NOINLINE npBuildSearchRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)req->paramCopy;
    SceNpMatching2SearchRoomRequest *searchReq = (SceNpMatching2SearchRoomRequest *)wrap->req;
    NpProtoRangeWire range;
    s32 ret;

    range.startIndex = searchReq->startIndex;
    range.maxNum = searchReq->maxNum;

    ret = npProtoBuildSearchRoomReq(outBuf, outBufSize, req->txId,
                                    conn->server->sessionId, &conn->server->sessionKey,
                                    req->protoCmd,
                                    (u16)(searchReq->worldId & 0xFFFFu),
                                    0, 0,
                                    &range,
                                    searchReq->flagFilter, searchReq->flagAttr,
                                    (const NpProtoIntFilterWire *)searchReq->intFilter, searchReq->intFilterNum,
                                    (const NpProtoBinFilterWire *)searchReq->binFilter, searchReq->binFilterNum,
                                    searchReq->attrId, searchReq->attrIdNum);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x0132A4 */
static s32 NP_MATCHING2_NOINLINE npBuildCreateRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)req->paramCopy;
    s32 ret;

    ret = npProtoBuildCreateRoomExtReq(outBuf, outBufSize, req->txId,
                                       conn->server->sessionId, &conn->server->sessionKey,
                                       &wrap->roomCookie,
                                       wrap->req->roomSearchableIntAttrExternal,
                                       wrap->req->roomSearchableIntAttrExternalNum,
                                       wrap->req->roomBinAttrExternal,
                                       wrap->req->roomBinAttrExternalNum,
                                       NULL);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x01333C */
static s32 NP_MATCHING2_NOINLINE npBuildJoinRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    const NpMatching2BlobBuf *cookie;
    s32 ret;

    if (req->reqKind == 10) {
        NpJoinRoomWrap *joinWrap = (NpJoinRoomWrap *)req->paramCopy;
        cookie = &joinWrap->roomCookie;
    } else {
        cookie = (const NpMatching2BlobBuf *)req->paramCopy;
    }

    ret = npProtoBuildJoinRoomExtReq(outBuf, outBufSize, req->txId,
                                     conn->server->sessionId, &conn->server->sessionKey,
                                     cookie);
    if (ret < 0 && req->reqKind == 10) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x0133D4 */
static s32 NP_MATCHING2_NOINLINE npBuildLeaveRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    return npProtoBuildLeaveRoomExtReq(outBuf, outBufSize, req->txId,
                                       conn->server->sessionId, &conn->server->sessionKey,
                                       (const NpMatching2BlobBuf *)req->paramCopy);
}

/* 0x01340C */
static s32 NP_MATCHING2_NOINLINE npBuildUpdateRoomExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    const NpMatching2BlobBuf *cookie;

    if (req->reqKind == 11) {
        cookie = (const NpMatching2BlobBuf *)req->paramCopy;
    } else {
        NpLeaveRoomWrap *leaveWrap = (NpLeaveRoomWrap *)req->paramCopy;
        cookie = &leaveWrap->notifyCookie;
    }

    return npProtoBuildUpdateRoomExtReq(outBuf, outBufSize, req->txId,
                                        conn->server->sessionId, &conn->server->sessionKey,
                                        cookie);
}

/* 0x013458 */
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberExtListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGetRoomMemberExtWrap *wrap = (NpGetRoomMemberExtWrap *)req->paramCopy;
    u16 roomNum = (u16)(wrap->roomId & 0xFFFFu);
    u16 worldNum = (u16)((wrap->roomId >> 32) & 0xFFFFu);
    s32 ret;

    ret = npProtoBuildGetRoomMemberExtListReq(outBuf, outBufSize, req->txId,
                                              conn->server->sessionId, &conn->server->sessionKey,
                                              0, roomNum, worldNum,
                                              wrap->attrList, wrap->attrCount);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x0134E0 */
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataExtReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSetRoomDataExtWrap *wrap = (NpSetRoomDataExtWrap *)req->paramCopy;
    u16 roomNum = (u16)(wrap->req->roomId & 0xFFFFu);
    u16 worldNum = (u16)((wrap->req->roomId >> 32) & 0xFFFFu);
    s32 ret;

    ret = npProtoBuildSetRoomDataExtReq(outBuf, outBufSize, req->txId,
                                        conn->server->sessionId, &conn->server->sessionKey,
                                        0, roomNum, worldNum,
                                        wrap->req->roomSearchableIntAttrExternal,
                                        wrap->req->roomSearchableIntAttrExternalNum,
                                        wrap->req->roomBinAttrExternal,
                                        wrap->req->roomBinAttrExternalNum,
                                        NULL);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x01358C */
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomDataExtListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)req->paramCopy;
    SceNpMatching2GetRoomDataExternalListRequest *extReq = (SceNpMatching2GetRoomDataExternalListRequest *)wrap->req;
    s32 ret;

    ret = npProtoBuildGetRoomDataExtListReq(outBuf, outBufSize, req->txId,
                                            conn->server->sessionId, &conn->server->sessionKey,
                                            extReq->roomId, extReq->roomIdNum,
                                            extReq->attrId, extReq->attrIdNum);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013614 */
static s32 NP_MATCHING2_NOINLINE npBuildReserveRoomSlotReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    s32 ret;

    if (req->reqKind == 9) {
        NpCreateJoinWrap *createWrap = (NpCreateJoinWrap *)req->paramCopy;
        ret = npProtoBuildReserveRoomSlotReq(outBuf, outBufSize, req->txId,
                                             conn->server->sessionId, &conn->server->sessionKey,
                                             (u16)(createWrap->req->worldId & 0xFFFFu), 0, 0);
    } else {
        NpJoinRoomWrap *joinWrap = (NpJoinRoomWrap *)req->paramCopy;
        SceNpMatching2RoomId roomId = joinWrap->req->roomId;
        u16 worldNum = (u16)((roomId >> 32) & 0xFFFFu);
        u64 lobbyId = ((roomId & 0xFFFF0000ULL) | ((roomId >> 32) << 32));
        ret = npProtoBuildReserveRoomSlotReq(outBuf, outBufSize, req->txId,
                                             conn->server->sessionId, &conn->server->sessionKey,
                                             worldNum, lobbyId, roomId);
    }

    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013780 */
static s32 NP_MATCHING2_NOINLINE npBuildReleaseRoomSlotReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    return npProtoBuildReleaseRoomSlotReq(outBuf, outBufSize, req->txId,
                                          conn->server->sessionId, &conn->server->sessionKey,
                                          (const NpMatching2BlobBuf *)req->paramCopy);
}

/* 0x0137B8 */
static s32 NP_MATCHING2_NOINLINE npBuildReserveJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    const SceNpMatching2RoomId *roomIdPtr = (const SceNpMatching2RoomId *)req->paramCopy;
    SceNpMatching2RoomId roomId = *roomIdPtr;
    return npProtoBuildReserveJoinRoomReq(outBuf, outBufSize, req->txId,
                                          conn->server->sessionId, &conn->server->sessionKey,
                                          (u32)((roomId >> 32) & 0xFFFFu), roomId);
}

/* 0x0137FC */
static s32 NP_MATCHING2_NOINLINE npBuildRoomKeepaliveReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    s32 ret = (s32)0x80550ca7;

    if (req->reqKind == 9) {
        ret = npProtoBuildRoomKeepaliveReq(outBuf, outBufSize, req->txId,
                                           conn->server->sessionId,
                                           &conn->server->sessionKey,
                                           &conn->server->sessionToken,
                                           &conn->server->keepaliveCookie, 2);
        if (ret >= 0) {
            return ret;
        }
    }

    npWorkerDispatchReqCb(req, ret, NULL);
    return ret;
}

/* 0x013888 */
static s32 NP_MATCHING2_NOINLINE npBuildJoinRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpJoinRoomWrap *wrap = (NpJoinRoomWrap *)req->paramCopy;
    NpMatching2UserProfile *profile = NULL;
    const NpProtoOptDataBlock *optData;
    s32 ret;

    ret = npWorkerGetLocalProfile(&profile);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    optData = (const NpProtoOptDataBlock *)wrap->req->optData;
    if (optData->size == 0) {
        optData = NULL;
    }

    ret = npProtoBuildJoinRoomReq(outBuf, outBufSize, req->txId,
                                  wrap->redirectInfo.extraWord,
                                  &conn->server->sessionKey,
                                  &conn->server->sessionToken,
                                  &conn->server->keepaliveCookie,
                                  NULL,
                                  wrap->req->teamId, 2,
                                  wrap->req->roomMemberBinAttrInternal,
                                  NULL,
                                  wrap->req->roomPassword,
                                  wrap->req->joinRoomGroupLabel,
                                  &wrap->roomTicket,
                                  optData);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013998 */
static s32 NP_MATCHING2_NOINLINE npBuildLeaveRoomReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpLeaveRoomWrap *wrap = (NpLeaveRoomWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildLeaveRoomReq(outBuf, outBufSize, req->txId,
                                        room->roomHandle, room->meMember->memberId,
                                        &room->roomKey, &wrap->optData)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013A44 */
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberIntListReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildGetRoomMemberIntListReq(outBuf, outBufSize, req->txId,
                                                   room->roomHandle, room->meMember->memberId,
                                                   &room->roomKey,
                                                   wrap->attrId, wrap->attrIdNum)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013AF4 */
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildGetRoomDataIntReq(outBuf, outBufSize, req->txId,
                                             room->roomHandle, room->meMember->memberId,
                                             &room->roomKey,
                                             wrap->attrId, wrap->attrIdNum)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013BA4 */
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataIntDispatch(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    if (req->reqKind == 18) {
        return npBuildSetRoomDataIntReq(conn, req, outBuf, outBufSize);
    }
    if (req->reqKind == 23) {
        NpSetSigOptWrap *wrap = (NpSetSigOptWrap *)req->paramCopy;
        NpMatching2Room *room = NULL;
        const NpMatching2BlobBuf *sigAttr;
        s32 ret;

        wrap->sigOptParam.hubMemberId = npSwap16(wrap->sigOptParam.hubMemberId);
        ret = npRoomGetFromConn(conn, &room);
        if (ret < 0) {
            npWorkerDispatchReqCb(req, ret, NULL);
            return ret;
        }

        sigAttr = (wrap->sigOptLen != 0) ? (const NpMatching2BlobBuf *)&wrap->sigOptLen : NULL;
        ret = npProtoBuildSetRoomDataIntReq(outBuf, outBufSize, req->txId,
                                            room->roomHandle, room->meMember->memberId,
                                            &room->roomKey,
                                            0, 0, NULL, 0, NULL,
                                            sigAttr, 1, NULL, 0);
        if (ret < 0) {
            npWorkerDispatchReqCb(req, ret, NULL);
        }
        return ret;
    }
    return 0;
}

/* 0x013CC4 */
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomMemberDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSetRoomMemberDataInternalWrap *wrap = (NpSetRoomMemberDataInternalWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    SceNpMatching2RoomMemberId targetMemberId;
    const NpMatching2BlobBuf *extraAttr;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    targetMemberId = wrap->req->memberId;
    if (targetMemberId == 0) {
        targetMemberId = room->meMember->memberId;
    }
    extraAttr = (wrap->extraAttr.len != 0) ? (const NpMatching2BlobBuf *)&wrap->extraAttr : NULL;

    ret = npProtoBuildSetRoomMemberDataIntReq(outBuf, outBufSize, req->txId,
                                              room->roomHandle, room->meMember->memberId,
                                              &room->roomKey, targetMemberId,
                                              wrap->req->flagFilter, wrap->req->flagAttr,
                                              wrap->req->teamId, 0,
                                              wrap->req->roomMemberBinAttrInternal,
                                              extraAttr, 1);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013DBC */
static s32 NP_MATCHING2_NOINLINE npBuildGetRoomMemberDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildGetRoomMemberDataIntReq(outBuf, outBufSize, req->txId,
                                                   room->roomHandle, room->meMember->memberId,
                                                   &room->roomKey, wrap->memberId,
                                                   wrap->attrId, wrap->attrIdNum)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013E74 */
static s32 NP_MATCHING2_NOINLINE npBuildGrantRoomOwnerReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpGrantRoomOwnerWrap *wrap = (NpGrantRoomOwnerWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildGrantRoomOwnerReq(outBuf, outBufSize, req->txId,
                                             room->roomHandle, room->meMember->memberId,
                                             &room->roomKey, wrap->newOwner, &wrap->optData)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013F24 */
static s32 NP_MATCHING2_NOINLINE npBuildKickoutRoomMemberReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpKickoutMemberWrap *wrap = (NpKickoutMemberWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0 ||
        (ret = npProtoBuildKickoutRoomMemberReq(outBuf, outBufSize, req->txId,
                                                room->roomHandle, room->meMember->memberId,
                                                &room->roomKey, wrap->target,
                                                wrap->blockFlag, 1, &wrap->optData)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x013FE4 */
static s32 NP_MATCHING2_NOINLINE npBuildSetRoomDataIntReq(NpMatching2Conn *conn, NpMatching2Req *req, u8 *outBuf, u32 outBufSize)
{
    NpSetRoomDataInternalWrap *wrap = (NpSetRoomDataInternalWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    const u64 *maskPtr;
    const NpMatching2BlobBuf *extraAttr;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        return ret;
    }

    wrap->hasComputedMask = 0;
    if (room->groupList != NULL) {
        ret = npWorkerBuildPasswordMask(room->groupList, room->groupCount,
                                        (const NpMatching2PasswordConfigInput *)wrap->req->passwordConfig,
                                        wrap->req->passwordConfigNum,
                                        &wrap->computedPasswordMask);
        if (ret < 0) {
            npWorkerDispatchReqCb(req, ret, NULL);
            return ret;
        }
        wrap->hasComputedMask = 1;
    } else if (wrap->req->passwordSlotMask != NULL) {
        wrap->computedPasswordMask = *wrap->req->passwordSlotMask;
        wrap->hasComputedMask = 1;
    }

    maskPtr = (wrap->hasComputedMask == 1) ? &wrap->computedPasswordMask : NULL;
    extraAttr = (wrap->extraAttr.len != 0) ? (const NpMatching2BlobBuf *)&wrap->extraAttr : NULL;

    ret = npProtoBuildSetRoomDataIntReq(outBuf, outBufSize, req->txId,
                                        room->roomHandle, room->meMember->memberId,
                                        &room->roomKey,
                                        wrap->req->flagFilter, wrap->req->flagAttr,
                                        wrap->req->roomBinAttrInternal, wrap->req->roomBinAttrInternalNum,
                                        maskPtr, extraAttr, 1,
                                        wrap->req->ownerPrivilegeRank, wrap->req->ownerPrivilegeRankNum);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    return ret;
}

/* 0x014158 */
static s32 NP_MATCHING2_NOINLINE npParseGetServerInfoResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    const SceNpMatching2GetServerInfoRequest *param = (const SceNpMatching2GetServerInfoRequest *)req->paramCopy;
    SceNpMatching2ServerInfoResp resp;
    u16 worldCount;
    u8 status;
    s32 ret;

    ret = npProtoParseGetServerInfoResp(payload, payloadLen, &worldCount, &status);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        conn->server->worldCount = worldCount;
        conn->server->status = status;
        sceKernelMemset(&resp, 0, sizeof(resp));
        resp.serverId = param->serverId;
        resp.status = status;
        npWorkerDispatchReqCb(req, 0, &resp);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x014220 */
static s32 NP_MATCHING2_NOINLINE npParseAuthResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    u32 sessionId;
    u16 keepaliveSec;
    s32 ret;

    ret = npProtoParseAuthResp(payload, payloadLen,
                               &sessionId,
                               &conn->server->sessionToken,
                               &conn->server->sessionKey,
                               &conn->server->keepaliveCookie,
                               &keepaliveSec);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    npConnResetTransport(conn);
    conn->server->keepaliveIntervalUsec = (u32)keepaliveSec * 1000000u;
    conn->server->sessionId = sessionId;
    conn->server->nextKeepaliveTime = sceKernelGetSystemTimeWide() + (u64)conn->server->keepaliveIntervalUsec;
    npWorkerOnAuthComplete(conn->server->ctx, req, 0);

    if (req->nextProtoCmd != 0) {
        req->protoCmd = req->nextProtoCmd;
        req->nextProtoCmd = 0;
        req->nextConnId = conn->server->mainConnId;
    } else {
        req->protoCmd = 0;
    }
    return 0;
}

/* 0x01432C */
static s32 NP_MATCHING2_NOINLINE npParseGetUserInfoListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2GetUserInfoListResp resp;
    NpProtoUserInfo *wireList = NULL;
    u16 count = 0;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseGetUserInfoListResp(payload, payloadLen, &count, conn->server->serverId, &wireList);
    if (ret >= 0) {
        ret = npWorkerConvertUserInfoList(wireList, count, &resp.userInfoList);
        if (ret >= 0) {
            resp.userInfoNum = count;
            npWorkerDispatchReqCb(req, 0, &resp);
            ret = 0;
        }
    }
    if (wireList != NULL) {
        npProtoFreeUserInfoList(wireList, count);
    }
    if (resp.userInfoList != NULL) {
        npWorkerFreeUserInfoList(resp.userInfoList, (u16)resp.userInfoNum);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x01443C */
static s32 NP_MATCHING2_NOINLINE npParseSearchRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2SearchRoomResp resp;
    NpProtoRoomDataExternal *wireList = NULL;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseSearchRoomResp(payload, payloadLen,
                                     &resp.startIndex, &resp.total, &resp.roomDataExternalNum,
                                     &wireList, conn->server->serverId);
    if (ret >= 0) {
        NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)req->paramCopy;
        SceNpMatching2SearchRoomRequest *searchReq = (SceNpMatching2SearchRoomRequest *)wrap->req;
        ret = npRoomConvertExternalList(wireList, (u16)resp.roomDataExternalNum,
                                        searchReq->attrId, (u16)searchReq->attrIdNum,
                                        &resp.roomDataExternal);
        if (ret >= 0) {
            npWorkerDispatchReqCb(req, 0, &resp);
            ret = 0;
        }
    }
    if (wireList != NULL) {
        npProtoFreeRoomDataExternalList(wireList, (u16)resp.roomDataExternalNum);
    }
    if (resp.roomDataExternal != NULL) {
        npRoomFreeExternalList(resp.roomDataExternal, (u16)resp.roomDataExternalNum);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x01455C */
static s32 NP_MATCHING2_NOINLINE npParseCreateRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)req->paramCopy;
    NpProtoRedirectInfo redirectInfo;
    SceNpMatching2CreateJoinRoomResp resp;
    s32 found = 0;
    s32 ret;

    ret = npProtoParseRedirectResp(payload, payloadLen, &redirectInfo, &found);
    if (ret < 0 || (ret = npRoomAttachToServer(conn->server, wrap->createdRoom)) < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npServerRecordOwnedRoom((u16)((wrap->createdRoom->roomId >> 32) & 0xFFFFu), conn->server);
        sceKernelMemset(&resp, 0, sizeof(resp));
        resp.roomDataInternal = wrap->createdRoom;
        npWorkerDispatchReqCb(req, 0, &resp);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x014628 */
static s32 NP_MATCHING2_NOINLINE npParseGetRoomDataExtListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)req->paramCopy;
    SceNpMatching2GetRoomDataExternalListRequest *extReq = (SceNpMatching2GetRoomDataExternalListRequest *)wrap->req;
    SceNpMatching2GetRoomDataExtListResp resp;
    NpProtoRoomDataExternal *wireList = NULL;
    u16 count = 0;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseGetRoomDataExtListResp(payload, payloadLen, &count, &wireList, conn->server->serverId);
    if (ret >= 0) {
        ret = npRoomConvertExternalList(wireList, count, extReq->attrId, (u16)extReq->attrIdNum, &resp.roomDataExternal);
        if (ret >= 0) {
            resp.roomDataExternalNum = count;
            npWorkerDispatchReqCb(req, 0, &resp);
            ret = 0;
        }
    }
    if (wireList != NULL) {
        npProtoFreeRoomDataExternalList(wireList, count);
    }
    if (resp.roomDataExternal != NULL) {
        npRoomFreeExternalList(resp.roomDataExternal, (u16)resp.roomDataExternalNum);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x014754 */
static s32 NP_MATCHING2_NOINLINE npParseReserveRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpMatching2HostList hostList;
    NpMatching2Conn *keepaliveConn = NULL;
    NpMatching2Conn *roomConn = NULL;
    s32 ret;

    sceKernelMemset(&hostList, 0, sizeof(hostList));
    if (req->reqKind == 9) {
        NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)req->paramCopy;
        ret = npProtoParseReserveRoomResp(payload, payloadLen,
                                          &wrap->roomNum, NULL,
                                          &wrap->roomTicket, &hostList,
                                          wrap->sigOptWire, &wrap->hasSigOpt);
        if (ret >= 0) {
            if (hostList.entries != NULL) {
                ret = npConnCreate(conn->server, 2, &hostList, &keepaliveConn);
                if (ret >= 0) {
                    req->protoCmd = 0x2001;
                    req->nextConnId = keepaliveConn->connId;
                    ret = 0;
                }
            } else {
                ret = (s32)0x80550cbb;
            }
        }
    } else {
        NpJoinRoomWrap *wrap = (NpJoinRoomWrap *)req->paramCopy;
        ret = npProtoParseReserveRoomResp(payload, payloadLen,
                                          NULL, &wrap->redirectInfo,
                                          &wrap->roomTicket, NULL,
                                          wrap->sigOptWire, &wrap->hasSigOpt);
        if (ret >= 0) {
            hostList.entries = (NpMatching2HostEntry *)npMatching2Alloc(sizeof(NpMatching2HostEntry));
            if (hostList.entries != NULL) {
                hostList.count = 1;
                sceKernelMemcpy(hostList.entries[0].host, wrap->redirectInfo.host, 129);
                hostList.entries[0].port = wrap->redirectInfo.port;
                ret = npConnCreate(conn->server, 3, &hostList, &roomConn);
                if (ret >= 0) {
                    req->protoCmd = 0x3201;
                    req->nextConnId = roomConn->connId;
                    ret = 0;
                }
            } else {
                ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    if (hostList.entries != NULL) {
        npMatching2Free(hostList.entries);
    }
    if (ret < 0) {
        if (roomConn != NULL) {
            npConnCloseOrDestroy(roomConn);
        }
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
    }
    return ret;
}

/* 0x01492C */
static s32 NP_MATCHING2_NOINLINE npParseRoomKeepaliveResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpMatching2HostList hostList;
    NpMatching2Conn *roomConn = NULL;
    s32 ret;

    sceKernelMemset(&hostList, 0, sizeof(hostList));
    ret = npProtoParseRoomKeepaliveResp(payload, payloadLen, &hostList);
    if (ret >= 0) {
        npConnCloseOrDestroy(conn);
        ret = npConnCreate(conn->server, 3, &hostList, &roomConn);
        if (ret >= 0) {
            req->protoCmd = 0x3200;
            req->nextConnId = roomConn->connId;
            ret = 0;
        }
    }

    if (hostList.entries != NULL) {
        npMatching2Free(hostList.entries);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
    }
    return ret;
}

/* 0x014A1C */
static s32 NP_MATCHING2_NOINLINE npParseCreateJoinRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *meMember = NULL;
    NpMatching2BlobBuf encryptedRoomKey;
    NpProtoRoomDataInternalWire roomWire;
    NpProtoRoomMemberInternalWire memberWire;
    u8 aesCtx[976];
    u32 offset;
    s32 ret;

    ret = npRoomAlloc(conn->server->serverId,
                      (u16)(wrap->req->worldId & 0xFFFFu),
                      0, wrap->roomNum, conn,
                      wrap->roomEvOpt, wrap->roomMsgOpt, &room);
    if (ret >= 0) {
        if (wrap->req->roomPassword != NULL) {
            room->hasPassword = 1;
            sceKernelMemcpy(&room->password, wrap->req->roomPassword, sizeof(SceNpMatching2SessionPassword));
        } else {
            room->hasPassword = 0;
        }
        wrap->createdRoom = room;

        ret = npProtoParseCreateJoinRoomResp(payload, payloadLen,
                                             &encryptedRoomKey, &wrap->roomCookie,
                                             &room->roomCookie, &roomWire, &memberWire);
        if (ret >= 0) {
            sceKernelMemset(aesCtx, 0, 972);
            ret = sceNetInet_lib_0238B6DF(aesCtx, 128, (s32)(8 * conn->server->sessionKey.len), conn->server->sessionKey.bytes);
            if (ret >= 0) {
                for (offset = 0; offset < encryptedRoomKey.len; offset += 16) {
                    ret = sceNetInet_lib_D609AD36(aesCtx, &encryptedRoomKey.bytes[offset], &room->roomKey.bytes[offset]);
                    if (ret < 0) {
                        break;
                    }
                }
            }
            if (ret >= 0) {
                room->roomKey.len = encryptedRoomKey.len;
                room->roomHandle = roomWire.roomHandle;
                room->flagAttr = roomWire.flagAttr;
                room->maxSlots = roomWire.maxSlot;
                sceKernelMemcpy(&room->sigOptLen, &roomWire.roomAttrLen, 12);

                ret = npWorkerDecodeRoomGroups(roomWire.roomMeta, roomWire.publicMeta,
                                               roomWire.groupList, roomWire.groupCount,
                                               roomWire.passwordSlotMask,
                                               &room->groupList, &room->groupCount);
                if (ret >= 0) {
                    if (room->groupList == NULL) {
                        room->passwordSlotMask = roomWire.passwordSlotMask;
                    }
                    if (room->roomEvOpt.optFlags[2] == 1) {
                        ret = npRoomUpdateDataInternal(room, NULL, roomWire.binAttrInternal, 2,
                                                       NULL, NULL, NULL, NULL, NULL, NULL);
                    }
                }
                if (ret >= 0) {
                    ret = npRoomInsertMember(&memberWire.npId, memberWire.onlineName, memberWire.avatarUrl,
                                             memberWire.joinDate, memberWire.memberId,
                                             wrap->req->teamId, 2, memberWire.flagAttr,
                                             &memberWire.npId, room, &meMember);
                    if (ret >= 0) {
                        if (room->roomEvOpt.optFlags[3] != 1 ||
                            (ret = npRoomCopyMemberBinAttrs(meMember, wrap->req->roomMemberBinAttrInternal,
                                                            wrap->req->roomMemberBinAttrInternalNum,
                                                            &memberWire.joinDate)) >= 0) {
                            req->protoCmd = 0x1202;
                            req->nextConnId = conn->server->mainConnId;
                            ret = 0;
                        }
                    }
                }
            }
        }
    }

    npProtoFreeRoomDataInt(&roomWire);
    npProtoFreeRoomMemberInt(&memberWire);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
    }
    return ret;
}

/* 0x014D54 */
static s32 NP_MATCHING2_NOINLINE npParseJoinRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpJoinRoomWrap *wrap = (NpJoinRoomWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpProtoRoomMemberInternalWire *memberList = NULL;
    NpMatching2BlobBuf encryptedRoomKey;
    NpProtoRoomDataInternalWire roomWire;
    SceNpMatching2RoomMemberId meMemberId = 0;
    u16 memberCount = 0;
    u8 aesCtx[976];
    SceNpMatching2JoinRoomResp resp;
    u32 offset;
    s32 ret;

    ret = npRoomAlloc(conn->server->serverId,
                      (u16)((wrap->req->roomId >> 32) & 0xFFFFu),
                      (u16)((wrap->req->roomId >> 16) & 0xFFFFu),
                      (u16)(wrap->req->roomId & 0xFFFFu),
                      conn, wrap->roomEvOpt, wrap->roomMsgOpt, &room);
    if (ret >= 0) {
        wrap->joinedRoom = room;
        room->roomHandle = wrap->redirectInfo.extraWord;

        ret = npProtoParseJoinRoomResp(payload, payloadLen,
                                       &meMemberId, &encryptedRoomKey,
                                       &wrap->roomCookie, &room->roomCookie,
                                       &roomWire, &memberCount, &memberList);
        if (ret >= 0) {
            sceKernelMemset(aesCtx, 0, 972);
            ret = sceNetInet_lib_0238B6DF(aesCtx, 128, (s32)(8 * conn->server->sessionKey.len), conn->server->sessionKey.bytes);
            if (ret >= 0) {
                for (offset = 0; offset < encryptedRoomKey.len; offset += 16) {
                    ret = sceNetInet_lib_D609AD36(aesCtx, &encryptedRoomKey.bytes[offset], &room->roomKey.bytes[offset]);
                    if (ret < 0) {
                        break;
                    }
                }
            }
            if (ret >= 0) {
                room->roomKey.len = encryptedRoomKey.len;
                room->maxSlots = roomWire.maxSlot;
                room->flagAttr = roomWire.flagAttr;
                sceKernelMemcpy(&room->sigOptLen, &roomWire.roomAttrLen, 12);

                ret = npWorkerDecodeRoomGroups(roomWire.roomMeta, roomWire.publicMeta,
                                               roomWire.groupList, roomWire.groupCount,
                                               roomWire.passwordSlotMask,
                                               &room->groupList, &room->groupCount);
                if (ret >= 0) {
                    if (room->groupList == NULL) {
                        room->passwordSlotMask = roomWire.passwordSlotMask;
                    }
                    if (room->roomEvOpt.optFlags[2] == 1) {
                        ret = npRoomUpdateDataInternal(room, NULL, roomWire.binAttrInternal, 2,
                                                       NULL, NULL, NULL, NULL, NULL, NULL);
                    }
                }
                if (ret >= 0) {
                    ret = npRoomAddMembersFromWire(memberList, memberCount, room);
                    if (ret >= 0) {
                        ret = npRoomAttachToServer(conn->server, wrap->joinedRoom);
                        if (ret >= 0) {
                            sceKernelMemset(&resp, 0, sizeof(resp));
                            resp.roomDataInternal = wrap->joinedRoom;
                            npWorkerDispatchReqCb(req, 0, &resp);
                            req->protoCmd = 0x1203;
                            req->nextConnId = conn->server->mainConnId;
                            ret = 0;
                        }
                    }
                }
            }
        }
    }

    npProtoFreeRoomDataInt(&roomWire);
    if (memberList != NULL) {
        npProtoFreeRoomMemberIntList(memberList, memberCount);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
    }
    return ret;
}

/* 0x01504C */
static s32 NP_MATCHING2_NOINLINE npParseLeaveRoomResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpLeaveRoomWrap *wrap = (NpLeaveRoomWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    SceNpMatching2RoomId roomIdCopy;
    s32 ret;

    ret = npRoomGetFromConn(conn, &room);
    if (ret >= 0) {
        ret = npProtoParseLeaveRoomResp(payload, payloadLen, &wrap->notifyCookie);
        if (ret >= 0) {
            roomIdCopy = room->roomId;
            npWorkerDispatchReqCb(req, 0, &roomIdCopy);
            req->protoCmd = 0;
            if (wrap->notifyCookie.len != 0) {
                req->protoCmd = 0x1205;
                req->nextConnId = conn->server->mainConnId;
            }
        }
        if (room->meMember == room->ownerMember) {
            npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
        }
    }
    npConnCloseOrDestroy(conn);
    return ret;
}

/* 0x015148 */
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberIntListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2GetRoomMemberIntListResp resp;
    NpMatching2Room *room = NULL;
    NpProtoRoomMemberInternalWire *wireList = NULL;
    NpMatching2RoomMember *prevMember = NULL;
    NpMatching2RoomMember *curMember = NULL;
    u16 count = 0;
    u32 idx;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    ret = npProtoParseGetRoomMemberIntListResp(payload, payloadLen, &count, &wireList);
    if (ret >= 0) {
        for (idx = 0; idx < count; idx++) {
            NpProtoRoomMemberInternalWire *wire = &wireList[idx];
            ret = npRoomCreateMember(&wire->npId, wire->onlineName, wire->avatarUrl,
                                     wire->joinDate, wire->memberId,
                                     wire->natType, wire->teamId, wire->flagAttr,
                                     room, &curMember);
            if (ret < 0) {
                break;
            }
            sceKernelMemcpy(curMember->binAttrInternal, &wire->binAttrInternal, sizeof(SceNpMatching2MemberBinAttrInternal));
            sceKernelMemset(&wire->binAttrInternal, 0, sizeof(SceNpMatching2MemberBinAttrInternal));

            if (prevMember != NULL) {
                prevMember->next = curMember;
            } else {
                resp.memberList = curMember;
            }
            if ((s32)curMember->flagAttr < 0) {
                resp.ownerMember = curMember;
            }
            if (curMember->memberId == room->meMember->memberId) {
                resp.meMember = curMember;
            }
            prevMember = curMember;
        }
        if (ret >= 0) {
            resp.memberNum = count;
            npWorkerDispatchReqCb(req, 0, &resp);
            ret = 0;
        }
    }

    npProtoFreeRoomMemberIntList(wireList, count);
    curMember = resp.memberList;
    while (curMember != NULL) {
        NpMatching2RoomMember *nextMember = curMember->next;
        npRoomFreeMember(curMember);
        curMember = nextMember;
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x01536C */
static s32 NP_MATCHING2_NOINLINE npParseGetRoomDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpMatching2Room *room = NULL;
    NpProtoRoomDataInternalWire roomWire;
    SceNpMatching2RoomDataInternalView view;
    SceNpMatching2RoomDataInternalView *viewPtr = NULL;
    u32 attrCount;
    s32 ret;

    sceKernelMemset(&view, 0, sizeof(view));
    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    ret = npProtoParseGetRoomDataIntResp(payload, payloadLen, &roomWire);
    if (ret >= 0) {
        view.serverId = (u16)(room->roomId >> 48);
        view.worldId = (u32)((room->roomId >> 32) & 0xFFFFu);
        view.roomId = room->roomId;
        view.maxSlots = roomWire.maxSlot;
        view.flagAttr = roomWire.flagAttr;

        ret = npWorkerCountActiveSlots(roomWire.roomMeta, &view.curMemberNum);
        if (ret >= 0) {
            if (roomWire.groupList != NULL && roomWire.groupCount != 0) {
                ret = npWorkerDecodeRoomGroups(roomWire.roomMeta, roomWire.publicMeta,
                                               roomWire.groupList, roomWire.groupCount,
                                               roomWire.passwordSlotMask,
                                               &view.roomGroup, &view.roomGroupNum);
            } else {
                view.passwordSlotMask = roomWire.passwordSlotMask;
            }
            if (ret >= 0) {
                for (attrCount = 0; attrCount < 2; attrCount++) {
                    if (roomWire.binAttrInternal[attrCount].attr.id == 0) {
                        break;
                    }
                }
                if (attrCount != 0) {
                    view.binAttrInternal = roomWire.binAttrInternal;
                    view.binAttrInternalNum = attrCount;
                }
                viewPtr = &view;
                npWorkerDispatchReqCb(req, 0, &viewPtr);
                view.binAttrInternal = NULL;
                view.binAttrInternalNum = 0;
                ret = 0;
            }
        }
    }

    npProtoFreeRoomDataInt(&roomWire);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x01552C */
static s32 NP_MATCHING2_NOINLINE npParseSetRoomMemberDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpSetRoomMemberDataInternalWrap *wrap = (NpSetRoomMemberDataInternalWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *member = NULL;
    u32 maxAttrs = g_npMatching2AttrLimits.maxMemberBinAttrInternal;
    SceNpMatching2MemberBinAttrInternal localAttrs[maxAttrs];
    u64 updateDate = 0;
    s32 ret;

    sceKernelMemset(localAttrs, 0, sizeof(SceNpMatching2MemberBinAttrInternal) * maxAttrs);
    ret = npProtoParseSetRoomMemberDataIntResp(payload, payloadLen, &updateDate);
    if (ret >= 0) {
        ret = npRoomGetFromConn(conn, &room);
    }
    if (ret >= 0) {
        ret = npRoomFindMemberById(room, room->meMember->memberId, &member);
    }
    if (ret >= 0) {
        if (room->roomEvOpt.optFlags[3] == 1) {
            u32 idx;
            for (idx = 0; idx < wrap->req->roomMemberBinAttrInternalNum && idx < maxAttrs; idx++) {
                localAttrs[idx].updateDate = updateDate;
                sceKernelMemcpy(&localAttrs[idx].attr, &wrap->req->roomMemberBinAttrInternal[idx], sizeof(SceNpMatching2BinAttr));
                sceKernelMemset((void *)&wrap->req->roomMemberBinAttrInternal[idx], 0, sizeof(SceNpMatching2BinAttr));
            }
            ret = npRoomUpdateMember(member, wrap->req->flagFilter, wrap->req->flagAttr,
                                     wrap->req->teamId, 0, localAttrs,
                                     wrap->req->roomMemberBinAttrInternalNum, NULL, NULL);
        } else {
            ret = npRoomUpdateMember(member, wrap->req->flagFilter, wrap->req->flagAttr,
                                     wrap->req->teamId, 0, NULL, 0, NULL, NULL);
        }
    }

    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x015760 */
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *member = NULL;
    NpProtoRoomMemberInternalWire memberWire;
    NpMatching2RoomMember *respMember = NULL;
    s32 ret;

    sceKernelMemset(&memberWire, 0, sizeof(memberWire));
    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    ret = npProtoParseGetRoomMemberDataIntResp(payload, payloadLen, &memberWire);
    if (ret >= 0) {
        ret = npRoomCreateMember(&memberWire.npId, memberWire.onlineName, memberWire.avatarUrl,
                                 memberWire.joinDate, memberWire.memberId,
                                 memberWire.natType, memberWire.teamId, memberWire.flagAttr,
                                 room, &member);
        if (ret >= 0) {
            sceKernelMemcpy(member->binAttrInternal, &memberWire.binAttrInternal, sizeof(SceNpMatching2MemberBinAttrInternal));
            sceKernelMemset(&memberWire.binAttrInternal, 0, sizeof(SceNpMatching2MemberBinAttrInternal));
            respMember = member;
            npWorkerDispatchReqCb(req, 0, &respMember);
            ret = 0;
        }
    }

    npProtoFreeRoomMemberInt(&memberWire);
    if (member != NULL) {
        npRoomFreeMember(member);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x0158D4 */
static s32 NP_MATCHING2_NOINLINE npParseSendRoomChatResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2RoomChatResp resp;
    u8 msgIdBuf[16];
    u8 filterBuf[16];
    s32 ret;

    (void)conn;
    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseSendRoomChatResp(payload, payloadLen, msgIdBuf, filterBuf);
    if (ret >= 0) {
        resp.filtered = (filterBuf[0] == 2);
        npWorkerDispatchReqCb(req, 0, &resp);
        ret = 0;
    } else {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x015980 */
static s32 NP_MATCHING2_NOINLINE npParseRoomChatNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpProtoBodyHeader *notifyHdr = (NpProtoBodyHeader *)hdrPtr;
    NpMatching2Room *room = NULL;
    NpProtoMsgOptData optData;
    SceNpMatching2RoomChatNotifyEvent ev;
    u32 targetStorage[4];
    u8 castMask[16];
    u8 filterBuf[16];
    SceNpMatching2RoomMemberId srcMemberId = 0;
    s32 hasCastMask = 0;
    s32 targetAllocated = 0;
    void *msgBuf = NULL;
    s32 ret;

    sceKernelMemset(&optData, 0, sizeof(optData));
    sceKernelMemset(targetStorage, 0, 8);
    sceKernelMemset(&ev, 0, sizeof(ev));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseRoomChatNotify(payload, payloadLen,
                                     &srcMemberId, castMask, &hasCastMask,
                                     &msgBuf, &ev.msgLen, &optData, filterBuf);
    ev.msg = msgBuf;
    if (ret >= 0) {
        ret = npWorkerDecodeCastTarget(optData.flag, room, castMask, targetStorage, &targetAllocated);
        if (ret >= 0) {
            ev.castType = optData.flag;
            ev.target = targetStorage;
            if (optData.hasProfile == 1) {
                ev.senderProfile = &optData.npId;
            }
            ev.filtered = (filterBuf[0] == 2);
            npWorkerDispatchRoomMsg((s32)(notifyHdr->txId >> 28), room, srcMemberId, 0x2101, &ev);
            ret = 0;
        }
    }

    if (msgBuf != NULL) {
        npMatching2Free(msgBuf);
    }
    if (optData.onlineName != NULL) {
        npMatching2Free(optData.onlineName);
    }
    if (optData.avatarUrl != NULL) {
        npMatching2Free(optData.avatarUrl);
    }
    if (targetAllocated == 1 && targetStorage[0] != 0) {
        npMatching2Free((void *)targetStorage[0]);
    }
    return ret;
}

/* 0x015B78 */
static s32 NP_MATCHING2_NOINLINE npParseRoomMsgNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpProtoBodyHeader *notifyHdr = (NpProtoBodyHeader *)hdrPtr;
    NpMatching2Room *room = NULL;
    NpProtoMsgOptData optData;
    SceNpMatching2RoomMsgNotifyEvent ev;
    u32 targetStorage[4];
    u8 castMask[16];
    SceNpMatching2RoomMemberId srcMemberId;
    s32 hasCastMask = 0;
    s32 targetAllocated = 0;
    void *msgBuf = NULL;
    s32 ret;

    sceKernelMemset(&optData, 0, sizeof(optData));
    sceKernelMemset(&ev, 0, sizeof(ev));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseRoomMsgNotify(payload, payloadLen,
                                    &srcMemberId, castMask, &hasCastMask,
                                    &msgBuf, &ev.msgLen, &optData);
    ev.msg = msgBuf;
    if (ret >= 0) {
        ret = npWorkerDecodeCastTarget(optData.flag, room, castMask, targetStorage, &targetAllocated);
        if (ret >= 0) {
            ev.castType = optData.flag;
            ev.target = targetStorage;
            if (optData.hasProfile == 1) {
                ev.senderProfile = &optData.npId;
            }
            npWorkerDispatchRoomMsg((s32)(notifyHdr->txId >> 28), room, srcMemberId, 0x2102, &ev);
            ret = 0;
        }
    }

    if (msgBuf != NULL) {
        npMatching2Free(msgBuf);
    }
    if (optData.onlineName != NULL) {
        npMatching2Free(optData.onlineName);
    }
    if (optData.avatarUrl != NULL) {
        npMatching2Free(optData.avatarUrl);
    }
    if (targetAllocated == 1 && targetStorage[0] != 0) {
        npMatching2Free((void *)targetStorage[0]);
    }
    return ret;
}

/* 0x015D3C */
static s32 NP_MATCHING2_NOINLINE npParseMemberJoinedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpProtoBodyHeader *notifyHdr = (NpProtoBodyHeader *)hdrPtr;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *member;
    NpMatching2RoomMember memberCopy;
    SceNpMatching2MemberJoinedEvent ev;
    NpProtoGroupConfigWire groupWire;
    NpMatching2BlobBuf cookie;
    NpProtoRoomMemberInternalWire memberWire;
    s32 hasGroup = 0;
    s32 ret;

    sceKernelMemset(&ev, 0, sizeof(ev));
    sceKernelMemset(&cookie, 0, sizeof(cookie));
    sceKernelMemset(&memberWire, 0, sizeof(memberWire));

    ret = npRoomGetFromConn(conn, &room);
    if (ret >= 0) {
        ret = npProtoParseMemberJoinedNotify(payload, payloadLen,
                                             &memberWire, &cookie,
                                             &groupWire, &hasGroup, &ev.optData);
        if (ret >= 0) {
            ret = npRoomAddMembersFromWire(&memberWire, 1, room);
            if (ret >= 0) {
                if (hasGroup != 1 ||
                    (ret = npWorkerSyncRoomGroups(&groupWire, 1, NULL,
                                                  room->groupList, room->groupCount,
                                                  NULL, NULL)) >= 0) {
                    ret = npRoomFindMemberById(room, memberWire.memberId, &member);
                    if (ret >= 0) {
                        sceKernelMemcpy(&memberCopy, member, sizeof(NpMatching2RoomMember));
                        memberCopy.next = NULL;
                        ev.roomMember = &memberCopy;
                        npWorkerDispatchRoomEvent((s32)(notifyHdr->txId >> 28), room, 0x1101, &ev);
                        ret = npTransportSendRoomExtCookie(conn, &cookie, 0x1203);
                    }
                }
            }
        }
    }

    npProtoFreeRoomMemberInt(&memberWire);
    return ret;
}

/* 0x015EF0 */
static s32 NP_MATCHING2_NOINLINE npParseMemberUpdateNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpProtoBodyHeader *notifyHdr = (NpProtoBodyHeader *)hdrPtr;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *member;
    NpMatching2RoomMember memberCopy;
    SceNpMatching2MemberUpdateEvent ev;
    u32 maxAttrs = g_npMatching2AttrLimits.maxMemberBinAttrInternal;
    SceNpMatching2MemberBinAttrInternal parsedAttrs[maxAttrs];
    SceNpMatching2MemberBinAttrInternal *updatedPtrs[maxAttrs];
    NpMatching2BlobBuf extraAttr;
    SceNpMatching2RoomMemberId memberId = 0;
    u32 flagFilter = 0;
    u32 flagAttr = 0;
    u32 prevFlagAttr = 0;
    u32 updatedCount = 0;
    u8 teamId = 0;
    u8 natType = 0;
    s32 ret;

    sceKernelMemset(parsedAttrs, 0, sizeof(SceNpMatching2MemberBinAttrInternal) * maxAttrs);
    sceKernelMemset(&ev, 0, sizeof(ev));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseMemberUpdateNotify(payload, payloadLen,
                                         &memberId, &flagFilter, &flagAttr,
                                         &teamId, &natType, parsedAttrs, &extraAttr);
    if (ret >= 0) {
        ret = npRoomFindMemberById(room, memberId, &member);
    }
    if (ret >= 0) {
        prevFlagAttr = member->flagAttr;
        updatedCount = maxAttrs;
        ret = npRoomUpdateMember(member, flagFilter, flagAttr, teamId, natType,
                                 parsedAttrs, maxAttrs, updatedPtrs, &updatedCount);
    }

    if (ret >= 0 && room->roomEvOpt.optFlags[1] == 1) {
        sceKernelMemcpy(&memberCopy, member, sizeof(NpMatching2RoomMember));
        memberCopy.next = NULL;
        ev.roomMember = &memberCopy;
        if (flagFilter != 0) {
            ev.newFlagAttr = &memberCopy.flagAttr;
            ev.prevFlagAttr = &prevFlagAttr;
        }
        if (teamId != 0) {
            ev.newTeamId = &memberCopy.teamId;
        }
        if (updatedCount != 0) {
            u32 idx;
            for (idx = 0; idx < updatedCount; idx++) {
                u32 attrIdx;
                for (attrIdx = 0; attrIdx < memberCopy.binAttrInternalCount; attrIdx++) {
                    if (memberCopy.binAttrInternal[attrIdx].attr.id == updatedPtrs[idx]->attr.id) {
                        break;
                    }
                }
                if (attrIdx == memberCopy.binAttrInternalCount) {
                    ret = (s32)0x80550cbd;
                    break;
                }
                updatedPtrs[idx] = &memberCopy.binAttrInternal[attrIdx];
            }
            if (ret >= 0) {
                ev.updatedBinAttr = updatedPtrs;
                ev.updatedBinAttrNum = updatedCount;
                npWorkerDispatchRoomEvent((s32)(notifyHdr->txId >> 28), room, 0x1107, &ev);
            }
        } else {
            npWorkerDispatchRoomEvent((s32)(notifyHdr->txId >> 28), room, 0x1107, &ev);
        }
    }

    if (room->roomEvOpt.optFlags[3] == 0 && member != NULL) {
        npProtoFreeMemberBinAttrIntArray(member->binAttrInternal, (u16)member->binAttrInternalCount);
    }
    npProtoFreeMemberBinAttrIntArray(parsedAttrs, (u16)maxAttrs);
    return ret;
}

/* 0x0161BC */
static s32 NP_MATCHING2_NOINLINE npParseRoomDataUpdateNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpProtoBodyHeader *notifyHdr = (NpProtoBodyHeader *)hdrPtr;
    u32 maxAttrs = g_npMatching2AttrLimits.maxRoomBinAttrInternal;
    u64 prevPasswordMask;
    u32 prevFlagAttr;
    s32 passwordMaskChanged = 0;
    u32 updatedAttrCount = 0;
    u32 updatedGroupCount = 0;
    u32 flagAttr;
    u32 flagFilter;
    s32 hasPasswordMask = 0;
    u64 newPasswordMask;
    NpMatching2Room *room = NULL;
    union {
        SceNpMatching2RoomDataUpdateEvent value;
        u64 alignWords[6];
    } eventFrame;
    SceNpMatching2SignalingOptParam sigOptEvent;
    union {
        NpMatching2BlobBuf value;
        u64 alignWords[66];
    } cookieFrame;
    struct {
        u32 len;
        u8 bytes[12];
    } roomAttr;
    SceNpMatching2RoomGroup *updatedGroupPtrs[15];
    SceNpMatching2BinAttrInternal parsedAttrs[maxAttrs];
    SceNpMatching2BinAttrInternal *updatedAttrPtrs[maxAttrs];
    s32 ret;

    sceKernelMemset(parsedAttrs, 0, sizeof(SceNpMatching2BinAttrInternal) * maxAttrs);
    sceKernelMemset(&cookieFrame.value, 0, sizeof(cookieFrame.value));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseRoomDataUpdateNotify(payload, payloadLen,
                                           &cookieFrame.value, &flagFilter, &flagAttr,
                                           (u16)maxAttrs, parsedAttrs,
                                           (NpMatching2BlobBuf *)&roomAttr,
                                           &newPasswordMask, &hasPasswordMask);
    if (ret >= 0) {
        prevFlagAttr = room->flagAttr;
        prevPasswordMask = room->passwordSlotMask;
        updatedGroupCount = 15;
        updatedAttrCount = maxAttrs;

        ret = npRoomUpdateDataInternal(room,
                                       (flagFilter != 0) ? &flagAttr : NULL,
                                       parsedAttrs, maxAttrs,
                                       updatedAttrPtrs, &updatedAttrCount,
                                       (hasPasswordMask == 1) ? &newPasswordMask : NULL,
                                       &passwordMaskChanged,
                                       updatedGroupPtrs, &updatedGroupCount);
        if (ret >= 0 && room->roomEvOpt.optFlags[0] == 1) {
            if (((notifyHdr->txId >> 20) & 0xFFu) == 23) {
                u32 copyLen = roomAttr.len;
                if (copyLen > 8) {
                    copyLen = 8;
                }
                sceKernelMemset(&sigOptEvent, 0, sizeof(sigOptEvent));
                sceKernelMemcpy(&sigOptEvent, roomAttr.bytes, copyLen);
                sigOptEvent.hubMemberId = npSwap16(sigOptEvent.hubMemberId);
                npWorkerDispatchRoomEvent((s32)(notifyHdr->txId >> 28), room, 0x1108, &sigOptEvent);
            } else {
                sceKernelMemset(&eventFrame.value, 0, sizeof(eventFrame.value));
                eventFrame.value.roomData = room;
                if (flagFilter != 0) {
                    eventFrame.value.newFlagAttr = &room->flagAttr;
                    eventFrame.value.prevFlagAttr = &prevFlagAttr;
                }
                if (passwordMaskChanged == 1) {
                    eventFrame.value.newPasswordSlotMask = &room->passwordSlotMask;
                    eventFrame.value.prevPasswordSlotMask = &prevPasswordMask;
                }
                eventFrame.value.updatedRoomGroup = updatedGroupPtrs;
                eventFrame.value.updatedRoomGroupNum = updatedGroupCount;
                if (updatedAttrCount != 0) {
                    eventFrame.value.updatedBinAttr = updatedAttrPtrs;
                    eventFrame.value.updatedBinAttrNum = updatedAttrCount;
                }
                npWorkerDispatchRoomEvent((s32)(notifyHdr->txId >> 28), room, 0x1106, &eventFrame.value);
            }
        }
    }

    if (room->roomEvOpt.optFlags[2] == 0) {
        npProtoFreeRoomBinAttrIntArray(room->binAttrInternal, (u16)room->binAttrInternalCount);
    }
    npProtoFreeRoomBinAttrIntArray(parsedAttrs, (u16)maxAttrs);
    return ret;
}

/* 0x0164BC */
static s32 NP_MATCHING2_NOINLINE npParseMemberLeftNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *removedMember = NULL;
    NpMatching2RoomMember memberCopy;
    SceNpMatching2MemberLeftEvent leftEv;
    SceNpMatching2KickedOutEvent kickEv;
    NpProtoGroupConfigWire groupWire;
    NpMatching2BlobBuf cookie;
    SceNpId leftNpId;
    SceNpMatching2RoomMemberId leftMemberId = 0;
    s32 hasGroup = 0;
    s32 ret;

    (void)hdrPtr;
    sceKernelMemset(&cookie, 0, sizeof(cookie));
    sceKernelMemset(&leftEv, 0, sizeof(leftEv));
    sceKernelMemset(&kickEv, 0, sizeof(kickEv));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseMemberLeftNotify(payload, payloadLen,
                                       &leftMemberId, &leftNpId, &leftEv.cause,
                                       &cookie, &leftEv.optData, &groupWire, &hasGroup);
    if (ret < 0) {
        return ret;
    }

    ret = npRoomRemoveMemberById(room, leftMemberId, &removedMember);
    if (ret >= 0) {
        if (hasGroup != 1 ||
            (ret = npWorkerSyncRoomGroups(&groupWire, 1, NULL,
                                          room->groupList, room->groupCount,
                                          NULL, NULL)) >= 0) {
            if (leftMemberId == room->meMember->memberId) {
                kickEv.cause = leftEv.cause;
                sceKernelMemcpy(&kickEv.optData, &leftEv.optData, sizeof(NpProtoOptDataBlock));
                npWorkerDispatchRoomEvent(0, room, 0x1103, &kickEv);
            } else {
                sceKernelMemcpy(&memberCopy, removedMember, sizeof(NpMatching2RoomMember));
                memberCopy.next = NULL;
                leftEv.roomMember = &memberCopy;
                npWorkerDispatchRoomEvent(0, room, 0x1102, &leftEv);
            }
            ret = npTransportSendRoomExtCookie(conn, &cookie, 0x1204);
        }
    }

    if (leftMemberId == room->meMember->memberId) {
        if (room->meMember == room->ownerMember) {
            npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
        }
        npConnCloseOrDestroy(conn);
    }
    if (removedMember != NULL) {
        npRoomFreeMember(removedMember);
    }
    return ret;
}

/* 0x0166FC */
static s32 NP_MATCHING2_NOINLINE npParseRoomOwnerChangedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *newOwnerMember = NULL;
    SceNpMatching2RoomOwnerChangedEvent ev;
    SceNpMatching2SessionPassword newPassword;
    NpMatching2BlobBuf cookie;
    SceNpId newOwnerNpId;
    s32 hasPassword = 0;
    s32 ret;

    (void)hdrPtr;
    sceKernelMemset(&cookie, 0, sizeof(cookie));
    sceKernelMemset(&ev, 0, sizeof(ev));

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        return ret;
    }

    ret = npProtoParseRoomOwnerChangedNotify(payload, payloadLen,
                                             &ev.newOwner, &newOwnerNpId, &ev.cause,
                                             &cookie, &newPassword, &hasPassword, &ev.optData);
    if (ret < 0) {
        return ret;
    }

    if (hasPassword == 1) {
        room->hasPassword = 1;
        sceKernelMemcpy(&room->password, &newPassword, sizeof(SceNpMatching2SessionPassword));
    } else {
        room->hasPassword = 0;
    }

    ret = npRoomFindMemberById(room, ev.newOwner, &newOwnerMember);
    if (ret < 0) {
        return ret;
    }

    ev.prevOwner = room->ownerMember->memberId;
    if (room->meMember == room->ownerMember) {
        npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
    } else if (room->meMember->memberId == ev.newOwner) {
        if (hasPassword == 1) {
            ev.roomPassword = &newPassword;
        }
        npServerRecordOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
    }

    ret = npRoomSetOwner(room, newOwnerMember);
    if (ret < 0) {
        return ret;
    }

    npWorkerDispatchRoomEvent(0, room, 0x1105, &ev);
    return npTransportSendRoomExtCookie(conn, &cookie, 0x1204);
}

/* 0x016890 */
static s32 NP_MATCHING2_NOINLINE npParseRoomDestroyedNotify(NpMatching2Conn *conn, void *hdrPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Room *room = NULL;
    SceNpMatching2RoomDestroyedEvent ev;
    NpMatching2BlobBuf cookie;
    s32 ret;

    (void)hdrPtr;
    sceKernelMemset(&cookie, 0, sizeof(cookie));
    sceKernelMemset(&ev, 0, sizeof(ev));

    ret = npRoomGetFromConn(conn, &room);
    if (ret >= 0) {
        ret = npRoomFindByIdAndRef(room->server->ctx, room->roomId, &room);
        if (ret >= 0) {
            ret = npProtoParseRoomDestroyedNotify(payload, payloadLen, &ev.cause, &cookie, &ev.optData);
            if (ret >= 0) {
                npWorkerCancelConnReqs(conn, (s32)0x80550c18);
                npWorkerDispatchRoomEvent(0, room, 0x1104, &ev);
                npRoomDestroy(room);
                ret = npTransportSendRoomExtCookie(conn, &cookie, 0x1205);
            }
            if (room->meMember == room->ownerMember) {
                npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
            }
            npRoomRelease(room);
        }
    }

    npConnCloseOrDestroy(conn);
    return ret;
}

/* 0x0169D8 */
static s32 NP_MATCHING2_NOINLINE npTransportParseFrameHdr(const u8 *buf, u32 bufLen,
                                                          NpProtoFrameHeader *hdrOut, u16 *bodyLenOut)
{
    s32 ret = npProtoParseFrameHeader(buf, bufLen, hdrOut);
    if (ret >= 0 && bodyLenOut != NULL) {
        *bodyLenOut = (u16)(hdrOut->totalLen - 4);
    }
    return ret;
}

/* 0x016A28 */
static s32 NP_MATCHING2_NOINLINE npTransportParseRespHdr(const u8 *buf, u32 bufLen, NpProtoBodyHeader *hdrOut)
{
    return npProtoParseRespHeader(buf, bufLen, hdrOut);
}

/* 0x016A44 */
static s32 NP_MATCHING2_NOINLINE npTransportParseNotifyHdr(const u8 *buf, u32 bufLen, NpProtoBodyHeader *hdrOut)
{
    return npProtoParseNotifyHeader(buf, bufLen, hdrOut);
}

/* 0x016A60 */
static s32 NP_MATCHING2_NOINLINE npParseKeepaliveResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;

    if (npProtoParseKeepaliveResp(payload, payloadLen, &conn->server->keepaliveCookie) >= 0 &&
        req->hasOptParam == 1 && req->optParam.cbFunc != NULL) {
        npWorkerDispatchReqCb(req, 0, NULL);
    }
    req->protoCmd = 0;
    return 0;
}

/* 0x016AD4 */
static s32 NP_MATCHING2_NOINLINE npParseSetUserInfoResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseSetUserInfoResp();
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016B38 */
static s32 NP_MATCHING2_NOINLINE npParseGetWorldInfoListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2GetWorldInfoListResp resp;
    s32 ret;

    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseGetWorldInfoListResp(payload, payloadLen, &resp.worldNum, &resp.worldList, conn->server->serverId);
    if (ret >= 0) {
        npWorkerDispatchReqCb(req, 0, &resp);
        ret = 0;
    }
    if (resp.worldList != NULL) {
        npMatching2Free(resp.worldList);
    }
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016C08 */
static s32 NP_MATCHING2_NOINLINE npParseJoinRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseJoinRoomExtResp();
    if (ret < 0) {
        npRoomAbortJoinOnError(req, ret, NULL);
    } else {
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016C64 */
static s32 NP_MATCHING2_NOINLINE npParseLeaveRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseLeaveRoomExtResp();
    req->protoCmd = 0;
    return ret;
}

/* 0x016C8C */
static s32 NP_MATCHING2_NOINLINE npParseUpdateRoomExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseUpdateRoomExtResp();
    req->protoCmd = 0;
    return ret;
}

/* 0x016CB4 */
static s32 NP_MATCHING2_NOINLINE npParseGetRoomMemberExtListResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    SceNpMatching2GetRoomMemberExtListResp resp;
    u16 count = 0;
    s32 ret;

    (void)conn;
    sceKernelMemset(&resp, 0, sizeof(resp));
    ret = npProtoParseGetRoomMemberExtListResp(payload, payloadLen, &count, &resp.memberList);
    if (ret >= 0) {
        resp.memberNum = count;
        npWorkerDispatchReqCb(req, 0, &resp);
        ret = 0;
    }
    npProtoFreeRoomMemberExtList(resp.memberList, (u16)resp.memberNum);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016D6C */
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataExtResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseSetRoomDataExtResp();
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016DD0 */
static s32 NP_MATCHING2_NOINLINE npParseReleaseRoomSlotResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseReleaseRoomSlotResp();
    req->protoCmd = 0;
    return ret;
}

/* 0x016DF8 */
static s32 NP_MATCHING2_NOINLINE npParseEmptyResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseEmptyResp();
    req->protoCmd = 0;
    return ret;
}

/* 0x016E20 */
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataIntDispatch(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;

    if (req->reqKind == 18) {
        return npParseSetRoomDataIntResp(conn, req, payload, payloadLen);
    }
    if (req->reqKind == 23) {
        return npParseSetSigOptResp(conn, req, payload, payloadLen);
    }
    return 0;
}

/* 0x016E74 */
static s32 NP_MATCHING2_NOINLINE npParseSendRoomMsgResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    u32 msgIdOut = 0;
    s32 ret;

    (void)conn;
    ret = npProtoParseSendRoomMsgResp(payload, payloadLen, &msgIdOut);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016EE4 */
static s32 NP_MATCHING2_NOINLINE npParseGrantRoomOwnerResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseGrantRoomOwnerResp();
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016F48 */
static s32 NP_MATCHING2_NOINLINE npParseKickoutRoomMemberResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    s32 ret;

    (void)conn;
    (void)payload;
    (void)payloadLen;
    ret = npProtoParseKickoutRoomMemberResp();
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x016FAC */
s32 NP_MATCHING2_NOINLINE npTransportSendRoomExtCookie(NpMatching2Conn *conn, NpMatching2BlobBuf *cookie, u16 protoCmd)
{
    NpMatching2Conn *mainConn = NULL;
    NpMatching2Req *extReq = NULL;
    SceNpMatching2RequestOptParam optParam;
    s32 ret;

    if (cookie->len == 0) {
        return 0;
    }

    ret = npConnFindAndRef(conn->server->ctx->ctxId, conn->server->mainConnId, &mainConn);
    if (ret >= 0) {
        sceKernelMemset(&optParam, 0, sizeof(optParam));
        optParam.timeout = 60000000;
        ret = npReqCreate(conn->server->ctx->ctxId, 0, 11, 0, protoCmd, cookie, mainConn, &optParam, 0, NULL, &extReq);
        if (ret >= 0) {
            ret = npReqEnqueue(extReq, mainConn, &mainConn->sendQueue);
            if (ret >= 0) {
                npWorkerWakeup();
                if (mainConn != NULL) {
                    npConnRelease(mainConn);
                }
                return 0;
            }
        }
    }

    if (extReq != NULL) {
        npReqAbortOnSubmitFail(mainConn->server->ctx, extReq, ret);
    }
    if (mainConn != NULL) {
        npConnRelease(mainConn);
    }
    return 0;
}

/* 0x0170E4 */
static s32 NP_MATCHING2_NOINLINE npParseSetRoomDataIntResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpSetRoomDataInternalWrap *wrap = (NpSetRoomDataInternalWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    u32 maxAttrs = g_npMatching2AttrLimits.maxRoomBinAttrInternal;
    SceNpMatching2BinAttrInternal localAttrs[maxAttrs];
    NpMatching2BlobBuf cookie;
    u64 updateDate = 0;
    const u32 *flagPtr;
    const u64 *maskPtr;
    s32 ret;

    sceKernelMemset(localAttrs, 0, sizeof(SceNpMatching2BinAttrInternal) * maxAttrs);
    sceKernelMemset(&cookie, 0, sizeof(cookie));

    ret = npProtoParseSetRoomDataIntResp(payload, payloadLen, &cookie, &updateDate);
    if (ret >= 0) {
        ret = npRoomGetFromConn(conn, &room);
    }
    if (ret >= 0) {
        flagPtr = (wrap->req->flagFilter != 0) ? &wrap->req->flagAttr : NULL;
        maskPtr = (wrap->hasComputedMask == 1) ? &wrap->computedPasswordMask : NULL;

        if (room->roomEvOpt.optFlags[2] == 1) {
            u32 idx;
            for (idx = 0; idx < wrap->req->roomBinAttrInternalNum && idx < maxAttrs; idx++) {
                localAttrs[idx].updateDate = updateDate;
                localAttrs[idx].updateMemberId = room->meMember->memberId;
                sceKernelMemcpy(&localAttrs[idx].attr, &wrap->req->roomBinAttrInternal[idx], sizeof(SceNpMatching2BinAttr));
                sceKernelMemset((void *)&wrap->req->roomBinAttrInternal[idx], 0, sizeof(SceNpMatching2BinAttr));
            }
            ret = npRoomUpdateDataInternal(room, flagPtr, localAttrs,
                                           wrap->req->roomBinAttrInternalNum,
                                           NULL, NULL, maskPtr, NULL, NULL, NULL);
        } else {
            ret = npRoomUpdateDataInternal(room, flagPtr, NULL, 0,
                                           NULL, NULL, maskPtr, NULL, NULL, NULL);
        }
    }

    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
    } else {
        npWorkerDispatchReqCb(req, 0, NULL);
        npTransportSendRoomExtCookie(conn, &cookie, 0x120A);
        ret = 0;
    }
    req->protoCmd = 0;
    return ret;
}

/* 0x017358 */
static s32 NP_MATCHING2_NOINLINE npParseSetSigOptResp(NpMatching2Conn *conn, void *reqPtr, const u8 *payload, u32 payloadLen)
{
    NpMatching2Req *req = (NpMatching2Req *)reqPtr;
    NpSetSigOptWrap *wrap = (NpSetSigOptWrap *)req->paramCopy;
    NpMatching2Room *room = NULL;
    NpMatching2BlobBuf cookie;
    u64 updateDate = 0;
    s32 ret;

    sceKernelMemset(&cookie, 0, sizeof(cookie));
    ret = npProtoParseSetRoomDataIntResp(payload, payloadLen, &cookie, &updateDate);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    ret = npRoomGetFromConn(conn, &room);
    if (ret < 0) {
        npWorkerDispatchReqCb(req, ret, NULL);
        req->protoCmd = 0;
        return ret;
    }

    room->sigOptLen = wrap->sigOptLen;
    room->sigOptParam = wrap->sigOptParam;
    npWorkerDispatchReqCb(req, 0, NULL);
    npTransportSendRoomExtCookie(conn, &cookie, 0x120A);
    req->protoCmd = 0;
    return 0;
}
