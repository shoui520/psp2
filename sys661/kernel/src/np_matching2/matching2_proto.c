#include "np_matching2_internal.h"

#define NP_EPOCH_TO_RTC_OFFSET 62135596800000000ULL

/* TLV wire-format schema table (matches .rodata @ 0x2da80, 118 entries) */
typedef struct NpTlvAttrSchema {
    u16 minLen;
    u16 maxLen;
    u8 wireType;
    u8 pad;
} NpTlvAttrSchema;

typedef struct NpTlvSpec {
    u16 tag;
    u16 pad;
    u32 required;
} NpTlvSpec;

typedef s32 (*NpTlvCallback)(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
typedef s32 (*NpTlvItemCallback)(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);

static const NpTlvAttrSchema g_tlvAttrTable[118] = {
    [1]   = {   0, 65535, 2, 0 },
    [2]   = {   4, 65535, 0, 0 },
    [3]   = {   8, 65535, 0, 0 },
    [4]   = {   4,     0, 0, 0 },
    [5]   = {   2,     0, 4, 0 },
    [6]   = {  12,     0, 0, 0 },
    [7]   = {   6,     9, 0, 0 },
    [8]   = {   5,   261, 0, 0 },
    [9]   = {   0,   128, 1, 0 },
    [10]  = {   6,   134, 0, 0 },
    [11]  = {  24,     0, 0, 0 },
    [12]  = {   0,    48, 1, 0 },
    [13]  = {   0,   127, 1, 0 },
    [14]  = {   0,    16, 1, 0 },
    [15]  = {   1,     8, 1, 0 },
    [16]  = {  12,     0, 1, 0 },
    [17]  = {   0, 65535, 2, 0 },
    [18]  = {  32,     0, 1, 0 },
    [19]  = {   1,     0, 3, 0 },
    [20]  = {   4,     0, 5, 0 },
    [21]  = { 128,     0, 2, 0 },
    [22]  = {   2,     0, 4, 0 },
    [23]  = {   1,     0, 3, 0 },
    [25]  = {   1,     0, 3, 0 },
    [26]  = {   1,  1024, 1, 0 },
    [27]  = {   1,  1024, 2, 0 },
    [28]  = {   1,     0, 3, 0 },
    [29]  = {   3,     0, 0, 0 },
    [30]  = {  15,     0, 0, 0 },
    [31]  = {  14,   142, 0, 0 },
    [32]  = {   1,   512, 2, 0 },
    [33]  = {   1,   512, 2, 0 },
    [34]  = {   1,   512, 2, 0 },
    [35]  = {   1,   512, 2, 0 },
    [36]  = {   1,   512, 2, 0 },
    [40]  = {   0,    16, 2, 0 },
    [41]  = {   2,     0, 4, 0 },
    [42]  = {  18,    54, 0, 0 },
    [43]  = {   1,   512, 2, 0 },
    [46]  = {   2,     0, 4, 0 },
    [47]  = {   2,     0, 4, 0 },
    [48]  = {   2,     0, 4, 0 },
    [49]  = {   4,     0, 7, 0 },
    [50]  = {   0,    64, 1, 0 },
    [51]  = {  74,   142, 0, 0 },
    [52]  = {   8,     0, 2, 0 },
    [53]  = {   4,     0, 7, 0 },
    [54]  = {  42,   324, 0, 0 },
    [55]  = {  14,   270, 0, 0 },
    [56]  = {  14,   270, 0, 0 },
    [57]  = {  12,    76, 0, 0 },
    [58]  = {   1,   512, 2, 0 },
    [61]  = {   2,     0, 4, 0 },
    [62]  = {   2,     0, 4, 0 },
    [63]  = {   2,     0, 4, 0 },
    [64]  = {   4,     0, 7, 0 },
    [65]  = {  32,     0, 7, 0 },
    [66]  = {   3,    16, 1, 0 },
    [67]  = {   3,    16, 1, 0 },
    [68]  = {  16,     0, 7, 0 },
    [69]  = {  72,   947, 0, 0 },
    [70]  = {   0,     8, 2, 0 },
    [71]  = {  10,     0, 0, 0 },
    [72]  = {   8,     0, 2, 0 },
    [73]  = {   4,     0, 7, 0 },
    [74]  = {  37,   220, 0, 0 },
    [75]  = {  44,   307, 0, 0 },
    [76]  = {   4,     0, 5, 0 },
    [77]  = {   4,     0, 5, 0 },
    [78]  = {   4,     0, 5, 0 },
    [79]  = {   4,     0, 5, 0 },
    [80]  = {   4,     0, 5, 0 },
    [81]  = {   4,     0, 5, 0 },
    [82]  = {   4,     0, 5, 0 },
    [83]  = {   4,     0, 5, 0 },
    [84]  = {   0,    64, 2, 0 },
    [85]  = {   0,   256, 2, 0 },
    [86]  = {   0,   256, 2, 0 },
    [87]  = {  14,   270, 0, 0 },
    [88]  = {  14,   270, 0, 0 },
    [89]  = {  12,    76, 0, 0 },
    [90]  = {   1,   512, 2, 0 },
    [92]  = {   2,     0, 4, 0 },
    [93]  = {   1,   512, 2, 0 },
    [94]  = {  28, 65535, 0, 0 },
    [95]  = {   0,   128, 2, 0 },
    [96]  = {  64,     0, 7, 0 },
    [97]  = {   8,     0, 6, 0 },
    [98]  = {   1,     0, 3, 0 },
    [100] = {   0,  1024, 0, 0 },
    [105] = {   1,     0, 3, 0 },
    [106] = {   8,     0, 7, 0 },
    [108] = {   0,     8, 2, 0 },
    [109] = {   0,     8, 2, 0 },
    [110] = {   0,     8, 2, 0 },
    [111] = {  16,     0, 2, 0 },
    [112] = {   8,     0, 6, 0 },
    [113] = {  68,   846, 0, 0 },
    [114] = {  72,   620, 0, 0 },
    [115] = {   2,     0, 4, 0 },
    [116] = {   2,     0, 4, 0 },
    [117] = {   1,     0, 3, 0 },
};

/* TLV specification tables (matches .rodata @ 0x2de08..0x2e257) */
static const NpTlvSpec g_specHostName[1] = {
    { 9, 0, 1 },
};
static const NpTlvSpec g_specRedirectHost[1] = {
    { 10, 0, 1 },
};
static const NpTlvSpec g_specRoomFlag[1] = {
    { 64, 0, 1 },
};
static const NpTlvSpec g_specMemberFlag[1] = {
    { 73, 0, 1 },
};
static const NpTlvSpec g_specWrappedBlob[1] = {
    { 1, 0, 1 },
};
static const NpTlvSpec g_specMsgOptData[4] = {
    { 1, 0, 1 }, { 11, 0, 0 }, { 12, 0, 0 }, { 13, 0, 0 },
};
static const NpTlvSpec g_specServerInfo[1] = {
    { 29, 0, 1 },
};
static const NpTlvSpec g_specAuthResp[5] = {
    { 20, 0, 1 }, { 35, 0, 1 }, { 34, 0, 1 }, { 93, 0, 1 }, { 116, 0, 1 },
};
static const NpTlvSpec g_specKeepaliveResp[1] = {
    { 93, 0, 1 },
};
static const NpTlvSpec g_specUserInfoList[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specUserInfoFields[5] = {
    { 11, 0, 1 }, { 12, 0, 0 }, { 13, 0, 0 }, { 95, 0, 0 }, { 2, 0, 0 },
};
static const NpTlvSpec g_specWorldInfoList[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specWorldAttr[1] = {
    { 18, 0, 0 },
};
static const NpTlvSpec g_specSearchRoomList[1] = {
    { 3, 0, 1 },
};
static const NpTlvSpec g_specRoomDataExtFields[16] = {
    { 65, 0, 1 }, { 76, 0, 0 }, { 77, 0, 0 }, { 78, 0, 0 },
    { 79, 0, 0 }, { 80, 0, 0 }, { 81, 0, 0 }, { 82, 0, 0 },
    { 83, 0, 0 }, { 84, 0, 0 }, { 85, 0, 0 }, { 86, 0, 0 },
    { 11, 0, 0 }, { 12, 0, 0 }, { 13, 0, 0 }, { 108, 0, 0 },
};
static const NpTlvSpec g_specRedirectResp[1] = {
    { 31, 0, 0 },
};
static const NpTlvSpec g_specRoomMemberExtList[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specRoomMemberExtFields[3] = {
    { 11, 0, 1 }, { 12, 0, 0 }, { 13, 0, 0 },
};
static const NpTlvSpec g_specRoomDataExtList[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specReserveRoomResp[5] = {
    { 61, 0, 0 }, { 31, 0, 0 }, { 32, 0, 1 }, { 2, 0, 0 }, { 18, 0, 0 },
};
static const NpTlvSpec g_specRoomKeepaliveResp[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specCreateJoinRoomResp[5] = {
    { 36, 0, 1 }, { 58, 0, 1 }, { 33, 0, 1 }, { 113, 0, 1 }, { 75, 0, 1 },
};
static const NpTlvSpec g_specRoomDataIntOpt[5] = {
    { 65, 0, 1 }, { 87, 0, 0 }, { 88, 0, 0 }, { 109, 0, 0 }, { 2, 0, 0 },
};
static const NpTlvSpec g_specRoomDataIntReq[5] = {
    { 65, 0, 1 }, { 87, 0, 1 }, { 88, 0, 1 }, { 109, 0, 1 }, { 2, 0, 1 },
};
static const NpTlvSpec g_specRoomMemberIntOpt[5] = {
    { 11, 0, 1 }, { 12, 0, 0 }, { 13, 0, 0 }, { 89, 0, 0 }, { 110, 0, 0 },
};
static const NpTlvSpec g_specRoomMemberIntReq[5] = {
    { 11, 0, 1 }, { 12, 0, 1 }, { 13, 0, 1 }, { 89, 0, 1 }, { 110, 0, 1 },
};
static const NpTlvSpec g_specJoinRoomResp[6] = {
    { 22, 0, 1 }, { 36, 0, 1 }, { 58, 0, 1 }, { 33, 0, 1 }, { 113, 0, 1 }, { 2, 0, 1 },
};
static const NpTlvSpec g_specLeaveRoomResp[1] = {
    { 58, 0, 0 },
};
static const NpTlvSpec g_specRoomMemberIntList[1] = {
    { 2, 0, 1 },
};
static const NpTlvSpec g_specGetRoomDataIntResp[1] = {
    { 113, 0, 1 },
};
static const NpTlvSpec g_specSetRoomDataIntResp[2] = {
    { 58, 0, 0 }, { 112, 0, 1 },
};
static const NpTlvSpec g_specSetRoomMemberDataIntResp[1] = {
    { 112, 0, 1 },
};
static const NpTlvSpec g_specGetRoomMemberDataIntResp[1] = {
    { 75, 0, 1 },
};
static const NpTlvSpec g_specSendRoomChatResp[2] = {
    { 68, 0, 1 }, { 117, 0, 1 },
};
static const NpTlvSpec g_specRoomChatNotify[5] = {
    { 22, 0, 1 }, { 68, 0, 0 }, { 26, 0, 1 }, { 100, 0, 0 }, { 117, 0, 1 },
};
static const NpTlvSpec g_specSendRoomMsgResp[1] = {
    { 68, 0, 1 },
};
static const NpTlvSpec g_specRoomMsgNotify[4] = {
    { 22, 0, 1 }, { 68, 0, 0 }, { 27, 0, 1 }, { 100, 0, 0 },
};
static const NpTlvSpec g_specMemberJoinedNotify[4] = {
    { 75, 0, 1 }, { 58, 0, 0 }, { 71, 0, 0 }, { 40, 0, 0 },
};
static const NpTlvSpec g_specMemberUpdateNotify[6] = {
    { 22, 0, 1 }, { 6, 0, 0 }, { 28, 0, 0 }, { 19, 0, 0 }, { 89, 0, 0 }, { 110, 0, 0 },
};
static const NpTlvSpec g_specRoomDataUpdateNotify[6] = {
    { 58, 0, 0 }, { 6, 0, 0 }, { 87, 0, 0 }, { 88, 0, 0 }, { 109, 0, 0 }, { 106, 0, 0 },
};
static const NpTlvSpec g_specMemberLeftNotify[6] = {
    { 22, 0, 1 }, { 11, 0, 1 }, { 98, 0, 1 }, { 58, 0, 0 }, { 40, 0, 0 }, { 71, 0, 0 },
};
static const NpTlvSpec g_specRoomOwnerChangedNotify[6] = {
    { 22, 0, 1 }, { 11, 0, 1 }, { 98, 0, 1 }, { 58, 0, 0 }, { 70, 0, 0 }, { 40, 0, 0 },
};
static const NpTlvSpec g_specRoomDestroyedNotify[3] = {
    { 98, 0, 1 }, { 58, 0, 0 }, { 40, 0, 0 },
};

/* Protocol callback context structs */
typedef struct NpProtoFlagDecodeCtx {
    u16 targetTag;
    u16 pad;
    u32 *valueOut;
} NpProtoFlagDecodeCtx;

typedef struct NpProtoBlobDecodeCtx {
    void **ptrOut;
    u32 *lenOut;
} NpProtoBlobDecodeCtx;

typedef struct NpProtoMsgOptDecodeCtx {
    NpProtoMsgOptData *optOut;
} NpProtoMsgOptDecodeCtx;

typedef struct NpProtoServerInfoCtx {
    u16 *statusOut;
    u8 *versionOut;
} NpProtoServerInfoCtx;

typedef struct NpProtoAuthRespCtx {
    u32 *sessionIdOut;
    NpMatching2BlobBuf *sessionTokenOut;
    NpMatching2BlobBuf *sessionKeyOut;
    NpMatching2BlobBuf *keepaliveCookieOut;
    u16 *keepaliveIntervalOut;
} NpProtoAuthRespCtx;

typedef struct NpProtoUserInfoListCtx {
    u16 *countOut;
    NpProtoUserInfo **listOut;
    NpProtoUserInfo *current;
    SceNpMatching2ServerId serverId;
    u16 pad;
} NpProtoUserInfoListCtx;

typedef struct NpProtoWorldInfoListCtx {
    u32 *countOut;
    NpProtoWorldInfo **listOut;
    NpProtoWorldInfo *prevNode;
    SceNpMatching2ServerId serverId;
    u16 pad;
} NpProtoWorldInfoListCtx;

typedef struct NpProtoSearchRoomCtx {
    u32 *countOut;
    u32 *startIndexOut;
    u32 *totalOut;
    NpProtoRoomDataExternal **listOut;
    SceNpMatching2ServerId serverId;
    u16 pad;
} NpProtoSearchRoomCtx;

typedef struct NpProtoRedirectCtx {
    NpProtoRedirectInfo *redirectOut;
    s32 *foundOut;
} NpProtoRedirectCtx;

typedef struct NpProtoRoomMemberExtListCtx {
    u16 *countOut;
    NpProtoRoomMemberExternal **listOut;
    NpProtoRoomMemberExternal *prevNode;
} NpProtoRoomMemberExtListCtx;

typedef struct NpProtoRoomDataExtListCtx {
    u16 *countOut;
    NpProtoRoomDataExternal **listOut;
    SceNpMatching2ServerId serverId;
    u16 pad;
} NpProtoRoomDataExtListCtx;

typedef struct NpProtoReserveRoomCtx {
    u16 *roomHandleOut;
    NpProtoRedirectInfo *redirectOut;
    NpMatching2BlobBuf *ticketOut;
    NpMatching2HostList *hostListOut;
    void *sigOptOut;
    s32 *hasSigOptOut;
} NpProtoReserveRoomCtx;

typedef struct NpProtoCreateJoinRespCtx {
    NpMatching2BlobBuf *roomKeyOut;
    NpMatching2BlobBuf *roomCookieOut;
    NpMatching2BlobBuf *roomTokenOut;
    NpProtoRoomDataInternalWire *roomDataOut;
    NpProtoRoomMemberInternalWire *memberDataOut;
} NpProtoCreateJoinRespCtx;

typedef struct NpProtoJoinRoomRespCtx {
    SceNpMatching2RoomMemberId *meMemberIdOut;
    NpMatching2BlobBuf *roomKeyOut;
    NpMatching2BlobBuf *roomCookieOut;
    NpMatching2BlobBuf *roomTokenOut;
    NpProtoRoomDataInternalWire *roomDataOut;
    u16 *memberCountOut;
    NpProtoRoomMemberInternalWire **memberListOut;
} NpProtoJoinRoomRespCtx;

typedef struct NpProtoMemberIntListCtx {
    u16 *countOut;
    NpProtoRoomMemberInternalWire **listOut;
} NpProtoMemberIntListCtx;

typedef struct NpProtoSetRoomDataIntCtx {
    NpMatching2BlobBuf *cookieOut;
    u64 *updateDateOut;
} NpProtoSetRoomDataIntCtx;

typedef struct NpProtoChatRespCtx {
    void *msgIdOut;
    void *filteredOut;
} NpProtoChatRespCtx;

typedef struct NpProtoChatNotifyCtx {
    SceNpMatching2RoomMemberId *srcMemberIdOut;
    void *msgIdOut;
    s32 *hasMsgIdOut;
    void **msgOut;
    u32 *msgLenOut;
    NpProtoMsgOptData *optDataOut;
    void *filteredOut;
} NpProtoChatNotifyCtx;

typedef struct NpProtoMsgNotifyCtx {
    SceNpMatching2RoomMemberId *srcMemberIdOut;
    void *msgIdOut;
    s32 *hasMsgIdOut;
    void **msgOut;
    u32 *msgLenOut;
    NpProtoMsgOptData *optDataOut;
} NpProtoMsgNotifyCtx;

typedef struct NpProtoMemberJoinedCtx {
    NpProtoRoomMemberInternalWire *memberOut;
    NpMatching2BlobBuf *cookieOut;
    NpProtoGroupConfigWire *groupOut;
    s32 *hasGroupOut;
    NpProtoOptDataBlock *optDataOut;
} NpProtoMemberJoinedCtx;

typedef struct NpProtoMemberUpdateCtx {
    SceNpMatching2RoomMemberId *memberIdOut;
    u32 *flagFilterOut;
    u32 *flagAttrOut;
    u8 *natTypeOut;
    u8 *teamIdOut;
    SceNpMatching2MemberBinAttrInternal *binAttrOut;
    NpMatching2BlobBuf *extraAttrOut;
} NpProtoMemberUpdateCtx;

typedef struct NpProtoRoomDataUpdateCtx {
    NpMatching2BlobBuf *cookieOut;
    u32 *flagFilterOut;
    u32 *flagAttrOut;
    u16 maxBinAttrs;
    u16 pad;
    SceNpMatching2BinAttrInternal *binAttrOut;
    NpMatching2BlobBuf *roomAttrOut;
    u64 *passwordSlotMaskOut;
    s32 *hasPasswordMaskOut;
} NpProtoRoomDataUpdateCtx;

typedef struct NpProtoMemberLeftCtx {
    SceNpMatching2RoomMemberId *memberIdOut;
    SceNpId *npIdOut;
    u8 *reasonOut;
    NpMatching2BlobBuf *cookieOut;
    NpProtoOptDataBlock *optDataOut;
    NpProtoGroupConfigWire *groupOut;
    s32 *hasGroupOut;
} NpProtoMemberLeftCtx;

typedef struct NpProtoOwnerChangedCtx {
    SceNpMatching2RoomMemberId *newOwnerIdOut;
    SceNpId *npIdOut;
    u8 *reasonOut;
    NpMatching2BlobBuf *cookieOut;
    SceNpMatching2SessionPassword *passwordOut;
    s32 *hasPasswordOut;
    NpProtoOptDataBlock *optDataOut;
} NpProtoOwnerChangedCtx;

typedef struct NpProtoRoomDestroyedCtx {
    u8 *reasonOut;
    NpMatching2BlobBuf *cookieOut;
    NpProtoOptDataBlock *optDataOut;
} NpProtoRoomDestroyedCtx;

/* Forward declarations for internal callbacks */
static s32 npProtoOnHostNameTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnHostEntryItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnRedirectHostTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnFlagValueTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnWrappedBlobTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnJoinedSessionItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnMsgOptDataTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnServerInfoTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnAuthRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnKeepaliveTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnUserInfoListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnWorldInfoListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnUserInfoItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnWorldInfoItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnUserInfoFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnWorldAttrTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnSearchRoomListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRedirectTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomMemberExtListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomDataExtListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnReserveRoomTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomDataExtItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnRoomMemberExtItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnRoomDataExtFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomMemberExtFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomKeepaliveHostTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnCreateJoinRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnJoinRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnLeaveRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomMemberIntListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnGetRoomDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnSetRoomDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnSetRoomMemberDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnGetRoomMemberDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnSendRoomChatRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomChatNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnSendRoomMsgRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomMsgNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnMemberJoinedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnMemberUpdateNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomDataUpdateNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnMemberLeftNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomOwnerChangedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoOnRoomDestroyedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoDecodeRoomBinAttrInt(u16 tag, const u8 *val, u16 len, u16 maxAttrs, SceNpMatching2BinAttrInternal *attrs);
static s32 npProtoOnRoomDataIntFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoDecodeRoomDataInt(const u8 *buf, u16 bufLen, NpProtoRoomDataInternalWire *outRoom, s32 requireAll);
static s32 npProtoOnRoomMemberIntFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx);
static s32 npProtoDecodeRoomMemberInt(const u8 *buf, u16 bufLen, NpProtoRoomMemberInternalWire *outMember, s32 requireAll);
static s32 npProtoOnRoomMemberIntItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);
static s32 npProtoOnGroupConfigItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx);

/* 0x006e9c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildHeader(u8 *outBuf, u32 outBufSize, u16 cmd, u8 flags, u32 txId,
                       void *routeId, u16 totalLen, const void *hmacKey, u32 hmacKeyLen)
{
    u8 digest[32];
    NpProtoBodyHeader bodyHdr;
    NpProtoFrameHeader frameHdr;
    u8 *bodyDst;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_HEADER;
    }

    sceKernelMemset(&bodyHdr, 0, sizeof(bodyHdr));
    bodyHdr.cmd = npSwap16(cmd);
    bodyHdr.flags = flags;
    bodyHdr.txId = txId;

    bodyDst = outBuf + 4;
    if ((cmd >> 8) == 0x31 || (cmd >> 8) == 0x32 || (cmd >> 8) == 0x33) {
        if (cmd != 0x3100 && cmd != 0x3200) {
            if (routeId != NULL) {
                NpProtoRoutePair *pair = (NpProtoRoutePair *)routeId;
                pair->secondWord = npSwap16(pair->secondWord);
                sceKernelMemcpy(bodyHdr.route, routeId, 4);
            }
        } else {
            if (routeId != NULL) {
                sceKernelMemcpy(bodyHdr.route, routeId, 4);
            }
        }
    } else {
        if (routeId != NULL) {
            sceKernelMemcpy(bodyHdr.route, routeId, 4);
        }
    }

    sceKernelMemcpy(bodyDst, &bodyHdr, sizeof(bodyHdr));
    sceKernelMemset(&frameHdr, 0, sizeof(frameHdr));
    frameHdr.versionWord = 0x0112;
    frameHdr.totalLen = npSwap16(totalLen);
    sceKernelMemcpy(outBuf, &frameHdr, sizeof(frameHdr));

    if (hmacKey == NULL || hmacKeyLen == 0) {
        return 32;
    }

    sceNet_lib_384EFE14(outBuf, totalLen, hmacKey, hmacKeyLen, digest);
    sceKernelMemcpy(bodyHdr.hmac, digest, 16);
    sceKernelMemcpy(bodyDst, &bodyHdr, sizeof(bodyHdr));
    return 32;
}

/* 0x007068 */
NP_MATCHING2_NOINLINE
s32 npProtoParseTlvStream(const u8 *buf, u32 bufLen, const NpTlvSpec *specs, u32 specCount,
                          NpTlvCallback callback, void *userCtx)
{
    u32 seen[32];
    u32 idx;
    const u8 *cursor;

    for (idx = 0; idx < specCount; idx++) {
        seen[idx] = 0;
    }

    cursor = buf;
    while (1) {
        u32 consumed = (u32)(cursor - buf);
        u16 wireTag;
        u16 wireLen;
        const u8 *valPtr;

        if (bufLen < consumed || (bufLen - consumed) < 4) {
            break;
        }
        sceKernelMemcpy(&wireTag, cursor, 2);
        wireTag = (u16)(npSwap16(wireTag) & 0x0FFF);
        sceKernelMemcpy(&wireLen, cursor + 2, 2);
        wireLen = npSwap16(wireLen);
        valPtr = cursor + 4;

        for (idx = 0; idx < specCount; idx++) {
            if (specs[idx].tag == wireTag) {
                const NpTlvAttrSchema *schema = &g_tlvAttrTable[wireTag];
                u32 valOffset = (u32)(valPtr - buf);
                if (bufLen >= valOffset && (bufLen - valOffset) >= schema->minLen) {
                    u32 passLen;
                    s32 cbRet;
                    if (wireLen < schema->minLen) {
                        return SCE_NP_MATCHING2_ERROR_TLV_LENGTH;
                    }
                    passLen = schema->minLen;
                    if (schema->maxLen != 0) {
                        passLen = (wireLen < schema->maxLen) ? wireLen : schema->maxLen;
                    }
                    cbRet = callback(&specs[idx], valPtr, (u16)passLen, userCtx);
                    if (cbRet < 0) {
                        return cbRet;
                    }
                    seen[idx] = 1;
                    break;
                }
            }
        }
        cursor = valPtr + wireLen;
    }

    for (idx = 0; idx < specCount; idx++) {
        if (specs[idx].required == 1 && seen[idx] == 0) {
            return SCE_NP_MATCHING2_ERROR_TLV_MISSING_REQUIRED;
        }
    }
    return (s32)(cursor - buf);
}

/* 0x0072b4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseTlvArray(u16 targetTag, const u8 *buf, u16 bufLen, u16 itemCount,
                         void **outArray, u32 allocSize, NpTlvItemCallback callback, void *userCtx)
{
    u8 *allocBuf;
    u8 *outCursor;
    const u8 *cursor;
    s32 parsed;

    if (allocSize == 0) {
        return 0;
    }

    allocBuf = (u8 *)npMatching2Alloc(allocSize);
    *outArray = allocBuf;
    if (allocBuf == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(allocBuf, 0, allocSize);

    outCursor = allocBuf;
    cursor = buf;
    parsed = 0;

    while (parsed < (s32)itemCount) {
        u16 wireTag;
        u16 wireLen;
        const u8 *valPtr;
        const u8 *nextPtr;

        if ((s32)bufLen < (s32)(cursor - buf) || (s32)bufLen - (s32)(cursor - buf) < 4) {
            return parsed;
        }
        sceKernelMemcpy(&wireTag, cursor, 2);
        wireTag = (u16)(npSwap16(wireTag) & 0x0FFF);
        sceKernelMemcpy(&wireLen, cursor + 2, 2);
        wireLen = npSwap16(wireLen);
        valPtr = cursor + 4;
        nextPtr = valPtr + wireLen;

        if (wireTag == targetTag) {
            const NpTlvAttrSchema *schema = &g_tlvAttrTable[targetTag];
            if ((s32)bufLen >= (s32)(valPtr - buf) &&
                (s32)bufLen - (s32)(valPtr - buf) >= (s32)schema->minLen) {
                u32 passLen;
                s32 step;
                if (wireLen < schema->minLen) {
                    return SCE_NP_MATCHING2_ERROR_TLV_LENGTH;
                }
                passLen = schema->minLen;
                if (schema->maxLen != 0) {
                    passLen = (wireLen < schema->maxLen) ? wireLen : schema->maxLen;
                }
                step = callback(targetTag, valPtr, (u16)passLen, outCursor, userCtx);
                if (step < 0) {
                    return step;
                }
                outCursor += step;
            }
        }
        parsed++;
        cursor = nextPtr;
    }
    return parsed;
}

/* 0x0074a0 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeIntFilterList(u8 *outBuf, u32 outBufSize, const NpProtoIntFilterWire *filters, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 attrTag = (u16)(filters[idx].attrId & 0x0FFF);
        u32 valSize = g_tlvAttrTable[attrTag].minLen;
        u16 outerLen = (u16)(valSize + 5);
        u16 outerTagWire;
        u16 outerLenWire;
        u8 *innerPos;
        s32 innerWritten;

        if (outBufSize - (u32)(cursor - outBuf) < (u32)outerLen + 4) {
            return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        }
        outerTagWire = npSwap16((u16)(((u16)g_tlvAttrTable[7].wireType << 12) | 7));
        sceKernelMemcpy(cursor, &outerTagWire, 2);
        outerLenWire = npSwap16(outerLen);
        sceKernelMemcpy(cursor + 2, &outerLenWire, 2);
        sceKernelMemcpy(cursor + 4, &filters[idx].op, 1);
        innerPos = cursor + 5;

        if (valSize == 1) {
            u8 valByte = (u8)filters[idx].value;
            innerWritten = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
            if (outBufSize - (u32)(innerPos - outBuf) >= 5) {
                u16 tagWire = npSwap16((u16)(attrTag | ((u16)g_tlvAttrTable[attrTag].wireType << 12)));
                u16 lenWire = npSwap16(1);
                sceKernelMemcpy(innerPos, &tagWire, 2);
                sceKernelMemcpy(innerPos + 2, &lenWire, 2);
                sceKernelMemcpy(innerPos + 4, &valByte, 1);
                innerWritten = 5;
            }
        } else if (valSize == 2) {
            u16 valHalf = npSwap16((u16)filters[idx].value);
            innerWritten = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
            if (outBufSize - (u32)(innerPos - outBuf) >= 6) {
                u16 tagWire = npSwap16((u16)(attrTag | ((u16)g_tlvAttrTable[attrTag].wireType << 12)));
                u16 lenWire = npSwap16(2);
                sceKernelMemcpy(innerPos, &tagWire, 2);
                sceKernelMemcpy(innerPos + 2, &lenWire, 2);
                sceKernelMemcpy(innerPos + 4, &valHalf, 2);
                innerWritten = 6;
            }
        } else if (valSize == 4) {
            u32 valWord = npSwap32(filters[idx].value);
            innerWritten = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
            if (outBufSize - (u32)(innerPos - outBuf) >= 8) {
                u16 tagWire = npSwap16((u16)(attrTag | ((u16)g_tlvAttrTable[attrTag].wireType << 12)));
                u16 lenWire = npSwap16(4);
                sceKernelMemcpy(innerPos, &tagWire, 2);
                sceKernelMemcpy(innerPos + 2, &lenWire, 2);
                sceKernelMemcpy(innerPos + 4, &valWord, 4);
                innerWritten = 8;
            }
        } else {
            return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_SIZE;
        }

        if (innerWritten < 0) {
            return innerWritten;
        }
        cursor = innerPos + innerWritten;
    }
    return (s32)(cursor - outBuf);
}

/* 0x007828 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeMsgOptData(u8 *outBuf, u32 outBufSize, const NpProtoMsgOptData *optData)
{
    u16 payloadLen = 5;
    u16 tagWire;
    u16 lenWire;
    s32 step;
    u8 *cursor;

    if (optData->hasProfile == 1) {
        payloadLen = 33;
        if (optData->onlineName != NULL) {
            payloadLen = 85;
        }
        if (optData->avatarUrl != NULL) {
            payloadLen = (u16)(payloadLen + (u16)sceNet_lib_D8722983(optData->avatarUrl) + 4);
        }
    }

    if (outBufSize < (u32)payloadLen + 4) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }

    tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[100].wireType << 12) | 100));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(payloadLen);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);

    step = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    if (outBufSize - 4 >= 5) {
        tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[1].wireType << 12) | 1));
        lenWire = npSwap16(1);
        sceKernelMemcpy(outBuf + 4, &tagWire, 2);
        sceKernelMemcpy(outBuf + 6, &lenWire, 2);
        sceKernelMemcpy(outBuf + 8, &optData->flag, 1);
        step = 5;
    }
    if (step < 0) {
        return step;
    }

    cursor = outBuf + 4 + step;
    if (optData->hasProfile != 1) {
        return (s32)(cursor - outBuf);
    }

    step = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    if (outBufSize - (u32)(cursor - outBuf) >= 28) {
        tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[11].wireType << 12) | 11));
        lenWire = npSwap16(24);
        sceKernelMemcpy(cursor, &tagWire, 2);
        sceKernelMemcpy(cursor + 2, &lenWire, 2);
        sceKernelMemcpy(cursor + 4, optData->npId.handle, 16);
        sceKernelMemcpy(cursor + 20, &optData->npId.opt[0], 2);
        sceKernelMemcpy(cursor + 22, &optData->npId.opt[2], 2);
        sceKernelMemcpy(cursor + 24, &optData->npId.opt[4], 4);
        step = 28;
    }
    cursor += step;

    if (optData->onlineName != NULL) {
        step = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        if (outBufSize - (u32)(cursor - outBuf) >= 52) {
            tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[12].wireType << 12) | 12));
            lenWire = npSwap16(48);
            sceKernelMemcpy(cursor, &tagWire, 2);
            sceKernelMemcpy(cursor + 2, &lenWire, 2);
            sceKernelMemcpy(cursor + 4, optData->onlineName, 48);
            step = 52;
        }
        cursor += step;
        if (step < 0) {
            return step;
        }
    }

    if (optData->avatarUrl != NULL) {
        u32 remain = outBufSize - (u32)(cursor - outBuf);
        u16 urlLen = (u16)sceNet_lib_D8722983(optData->avatarUrl);
        step = SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        if (remain >= (u32)urlLen + 4) {
            tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[13].wireType << 12) | 13));
            lenWire = npSwap16(urlLen);
            sceKernelMemcpy(cursor, &tagWire, 2);
            sceKernelMemcpy(cursor + 2, &lenWire, 2);
            sceKernelMemcpy(cursor + 4, optData->avatarUrl, urlLen);
            step = (s32)urlLen + 4;
        }
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    return (s32)(cursor - outBuf);
}

/* 0x007c10 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeFlagPair(const u8 *buf, u32 bufLen, u16 targetTag, u32 *filterOut, u32 *valueOut)
{
    const NpTlvSpec *spec;
    NpProtoFlagDecodeCtx ctx;
    s32 ret;

    sceKernelMemcpy(filterOut, buf, 4);
    *filterOut = npSwap32(*filterOut);

    if (targetTag == 64) {
        spec = g_specRoomFlag;
    } else if (targetTag == 73) {
        spec = g_specMemberFlag;
    } else {
        return (s32)0x80550cae;
    }

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.targetTag = targetTag;
    ctx.valueOut = valueOut;
    ret = npProtoParseTlvStream(buf + 4, bufLen - 4, spec, 1, npProtoOnFlagValueTlv, &ctx);
    if (ret >= 0) {
        ret += 4;
    }
    return ret;
}

/* 0x007d0c */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeJoinedSession(const u8 *buf, u32 bufLen, SceNpMatching2ServerId serverId,
                               SceNpMatching2JoinedSessionInfo *outInfo)
{
    u16 worldNum = 0;
    u16 lobbyNum = 0;
    u16 roomNum = 0;
    u32 hiWord;
    u32 loWord;
    u32 rawWords[2];
    (void)bufLen;

    sceKernelMemset(outInfo, 0, sizeof(*outInfo));
    sceKernelMemcpy(&outInfo->sessionType, buf, 1);
    sceKernelMemcpy(&worldNum, buf + 1, 2);
    worldNum = npSwap16(worldNum);
    sceKernelMemcpy(&lobbyNum, buf + 3, 2);
    lobbyNum = npSwap16(lobbyNum);
    sceKernelMemcpy(&roomNum, buf + 5, 2);
    roomNum = npSwap16(roomNum);

    if (worldNum != 0) {
        outInfo->serverId = serverId;
        outInfo->worldId = ((u32)serverId << 16) | (u32)worldNum;
    }
    if (lobbyNum != 0) {
        outInfo->lobbyId = (((u64)(((u32)serverId << 16) | (u32)worldNum)) << 32) | ((u64)lobbyNum << 16);
    }
    if (roomNum != 0) {
        outInfo->roomId = (((u64)(((u32)serverId << 16) | (u32)worldNum)) << 32) |
                          (((u64)lobbyNum << 16) | (u64)roomNum);
    }

    sceKernelMemcpy(rawWords, buf + 7, 8);
    hiWord = npSwap32(rawWords[0]);
    loWord = npSwap32(rawWords[1]);
    if ((loWord | hiWord) != 0) {
        u64 epochMs = ((u64)hiWord << 32) | (u64)loWord;
        outInfo->joinDate = epochMs * 1000ULL + NP_EPOCH_TO_RTC_OFFSET;
    } else {
        sceKernelMemset(&outInfo->joinDate, 0, 8);
    }
    return 15;
}

/* 0x007fc0 */
NP_MATCHING2_NOINLINE
s32 npProtoParseFrameHeader(const u8 *buf, u32 bufLen, NpProtoFrameHeader *outHdr)
{
    if (bufLen < 4) {
        return SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT;
    }
    sceKernelMemcpy(outHdr, buf, 4);
    outHdr->totalLen = npSwap16(outHdr->totalLen);
    if ((outHdr->versionWord & 0x00F0) == 0x0010) {
        return 4;
    }
    return SCE_NP_MATCHING2_ERROR_PACKET_VERSION;
}

/* 0x008030 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRespHeader(const u8 *buf, u32 bufLen, NpProtoBodyHeader *outHdr)
{
    if (bufLen < 28) {
        return (s32)0x80550c87;
    }
    sceKernelMemcpy(outHdr, buf, 28);
    outHdr->cmd = npSwap16(outHdr->cmd);
    return 28;
}

/* 0x008088 */
NP_MATCHING2_NOINLINE
s32 npProtoParseNotifyHeader(const u8 *buf, u32 bufLen, NpProtoBodyHeader *outHdr)
{
    if (bufLen < 28) {
        return (s32)0x80550c88;
    }
    sceKernelMemcpy(outHdr, buf, 28);
    outHdr->cmd = npSwap16(outHdr->cmd);
    return 28;
}

/* 0x0080e0 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeTlv(u8 *outBuf, u32 outBufSize, u16 tag, u16 valLen, const void *valPtr)
{
    u16 tagWire;
    u16 lenWire;

    if (outBufSize < (u32)valLen + 4) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)((tag & 0x0FFF) | ((u16)g_tlvAttrTable[tag & 0x0FFF].wireType << 12)));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(valLen);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    sceKernelMemcpy(outBuf + 4, valPtr, valLen);
    return (s32)valLen + 4;
}

/* 0x0081c8 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeCommId(u8 *outBuf, u32 outBufSize, const SceNpCommId *commId)
{
    char commStr[16];
    u16 tagWire;
    u16 lenWire;
    s32 ret;

    if (outBufSize < 16) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[16].wireType << 12) | 16));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(12);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    ret = sceNp_E90DFBC4(commId, commStr, 13);
    if (ret >= 0) {
        sceKernelMemcpy(outBuf + 4, commStr, 12);
        return 16;
    }
    return ret;
}

/* 0x0082a0 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeNpId(u8 *outBuf, u32 outBufSize, const SceNpId *npId)
{
    u16 tagWire;
    u16 lenWire;

    if (outBufSize < 28) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[11].wireType << 12) | 11));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(24);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    sceKernelMemcpy(outBuf + 4, npId->handle, 16);
    sceKernelMemcpy(outBuf + 20, &npId->opt[0], 2);
    sceKernelMemcpy(outBuf + 22, &npId->opt[2], 2);
    sceKernelMemcpy(outBuf + 24, &npId->opt[4], 4);
    return 28;
}

/* 0x0083a4 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeIntAttrList(u8 *outBuf, u32 outBufSize, const SceNpMatching2IntAttr *attrs, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u32 valBe = npSwap32(attrs[idx].num);
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf),
                                    attrs[idx].id, 4, &valBe);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x00844c */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeBinAttrList(u8 *outBuf, u32 outBufSize, const SceNpMatching2BinAttr *attrs, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf),
                                    attrs[idx].id, (u16)attrs[idx].size, attrs[idx].ptr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x0084e8 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeNpHandleList(u8 *outBuf, u32 outBufSize, u16 tag, const char (*handles)[20], u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 handleLen = (u16)sceNet_lib_D8722983(handles[idx]);
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf),
                                    tag, handleLen, handles[idx]);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x00859c */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeRange(u8 *outBuf, u32 outBufSize, const NpProtoRangeWire *range)
{
    u16 tagWire;
    u16 lenWire;
    u16 valWire;

    if (outBufSize < 8) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[4].wireType << 12) | 4));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(4);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    valWire = npSwap16((u16)range->startIndex);
    sceKernelMemcpy(outBuf + 4, &valWire, 2);
    valWire = npSwap16((u16)range->maxNum);
    sceKernelMemcpy(outBuf + 6, &valWire, 2);
    return 8;
}

/* 0x008688 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeFlagPair(u8 *outBuf, u32 outBufSize, u32 flagFilter, u16 flagTag, u32 flagAttr)
{
    u16 tagWire;
    u16 lenWire;
    u32 wordWire;
    s32 innerLen;

    if (outBufSize < 16) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[6].wireType << 12) | 6));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(12);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    wordWire = npSwap32(flagFilter);
    sceKernelMemcpy(outBuf + 4, &wordWire, 4);
    wordWire = npSwap32(flagAttr);
    innerLen = npProtoEncodeTlv(outBuf + 8, outBufSize - 8, flagTag, 4, &wordWire);
    if (innerLen >= 0) {
        return innerLen + 8;
    }
    return innerLen;
}

/* 0x00879c */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeBinFilterList(u8 *outBuf, u32 outBufSize, const NpProtoBinFilterWire *filters, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 outerLen = (u16)(filters[idx].size + 5);
        u16 tagWire;
        u16 lenWire;
        s32 step;

        if (outBufSize - (u32)(cursor - outBuf) < (u32)outerLen + 4) {
            return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        }
        tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[8].wireType << 12) | 8));
        sceKernelMemcpy(cursor, &tagWire, 2);
        lenWire = npSwap16(outerLen);
        sceKernelMemcpy(cursor + 2, &lenWire, 2);
        sceKernelMemcpy(cursor + 4, &filters[idx].op, 1);
        step = npProtoEncodeTlv(cursor + 5, outBufSize - (u32)(cursor + 5 - outBuf),
                                filters[idx].attrId, (u16)filters[idx].size, filters[idx].ptr);
        if (step < 0) {
            return step;
        }
        cursor = cursor + 5 + step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x0088ec */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeAttrIdList(u8 *outBuf, u32 outBufSize, const SceNpMatching2AttributeId *attrIds, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 tag = (u16)(attrIds[idx] & 0x0FFF);
        u16 wireVal = npSwap16((u16)(tag | ((u16)g_tlvAttrTable[tag].wireType << 12)));
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 5, 2, &wireVal);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x0089cc */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeU32Tlv(u8 *outBuf, u32 outBufSize, u16 tag, u32 value)
{
    u32 valBe = npSwap32(value);
    return npProtoEncodeTlv(outBuf, outBufSize, tag, 4, &valBe);
}

/* 0x0089f8 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeGroupConfigList(u8 *outBuf, u32 outBufSize, const NpProtoGroupConfigWire *groups, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 tagWire;
        u16 lenWire;

        if (outBufSize - (u32)(cursor - outBuf) < 14) {
            return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        }
        tagWire = npSwap16((u16)(((u16)g_tlvAttrTable[71].wireType << 12) | 71));
        sceKernelMemcpy(cursor, &tagWire, 2);
        lenWire = npSwap16(10);
        sceKernelMemcpy(cursor + 2, &lenWire, 2);
        sceKernelMemcpy(cursor + 4, &groups[idx].groupId, 1);
        sceKernelMemcpy(cursor + 5, &groups[idx].withPassword, 1);
        sceKernelMemcpy(cursor + 6, groups[idx].label, 8);
        cursor += 14;
    }
    return (s32)(cursor - outBuf);
}

/* 0x008b48 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeMemberBinAttr(u8 *outBuf, u32 outBufSize, const SceNpMatching2BinAttr *attr)
{
    u16 outerLen = (u16)(attr->size + 12);
    u16 tag = (u16)(attr->id & 0x0FFF);
    u16 tagWire;
    u16 lenWire;
    s32 step;

    if (outBufSize < (u32)outerLen + 4) {
        return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
    }
    tagWire = npSwap16((u16)(tag | ((u16)g_tlvAttrTable[tag].wireType << 12)));
    sceKernelMemcpy(outBuf, &tagWire, 2);
    lenWire = npSwap16(outerLen);
    sceKernelMemcpy(outBuf + 2, &lenWire, 2);
    sceKernelMemset(outBuf + 4, 0, 8);
    step = npProtoEncodeTlv(outBuf + 12, outBufSize - 12, 1, (u16)attr->size, attr->ptr);
    if (step >= 0) {
        return step + 12;
    }
    return step;
}

/* 0x008c68 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeRoomBinAttrList(u8 *outBuf, u32 outBufSize, const SceNpMatching2BinAttr *attrs, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 outerLen = (u16)(attrs[idx].size + 14);
        u16 tag = (u16)(attrs[idx].id & 0x0FFF);
        u16 tagWire;
        u16 lenWire;
        s32 step;

        if (outBufSize - (u32)(cursor - outBuf) < (u32)outerLen + 4) {
            return SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL;
        }
        tagWire = npSwap16((u16)(tag | ((u16)g_tlvAttrTable[tag].wireType << 12)));
        sceKernelMemcpy(cursor, &tagWire, 2);
        lenWire = npSwap16(outerLen);
        sceKernelMemcpy(cursor + 2, &lenWire, 2);
        sceKernelMemset(cursor + 4, 0, 2);
        sceKernelMemset(cursor + 6, 0, 8);
        step = npProtoEncodeTlv(cursor + 14, outBufSize - (u32)(cursor + 14 - outBuf),
                                1, (u16)attrs[idx].size, attrs[idx].ptr);
        if (step < 0) {
            return step;
        }
        cursor = cursor + 14 + step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x008ddc */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeMemberIdList(u8 *outBuf, u32 outBufSize, const SceNpMatching2RoomMemberId *memberIds, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 idBe = npSwap16(memberIds[idx]);
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 22, 2, &idBe);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x008e84 */
NP_MATCHING2_NOINLINE
s32 npProtoEncodeWorldIdList(u8 *outBuf, u32 outBufSize, const u16 *worldIds, u32 count)
{
    u32 idx;
    u8 *cursor = outBuf;

    for (idx = 0; idx < count; idx++) {
        u16 idBe = npSwap16(worldIds[idx]);
        s32 step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 41, 2, &idBe);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }
    return (s32)(cursor - outBuf);
}

/* 0x008f2c */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeNpId(const u8 *buf, u32 bufLen, SceNpId *npIdOut)
{
    (void)bufLen;
    sceKernelMemcpy(npIdOut->handle, buf, 16);
    sceKernelMemcpy(&npIdOut->opt[0], buf + 16, 2);
    sceKernelMemcpy(&npIdOut->opt[2], buf + 18, 2);
    sceKernelMemcpy(&npIdOut->opt[4], buf + 20, 4);
    npIdOut->reserved[0] = 1;
    return 24;
}

/* 0x008fa4 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeHostList(const u8 *buf, u16 bufLen, u32 *countOut, NpMatching2HostEntry **entriesOut)
{
    u16 innerTag;
    u16 innerCount;
    s32 parsed;

    sceKernelMemcpy(&innerTag, buf, 2);
    innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
    sceKernelMemcpy(&innerCount, buf + 2, 2);
    innerCount = npSwap16(innerCount);

    if (innerTag != 10) {
        return 0;
    }
    parsed = npProtoParseTlvArray(10, buf + 4, (u16)(bufLen - 4), innerCount,
                                  (void **)entriesOut, (u32)innerCount * sizeof(NpMatching2HostEntry),
                                  npProtoOnHostEntryItem, NULL);
    if (parsed >= 0) {
        *countOut = (u32)parsed;
        return 0;
    }
    return parsed;
}

/* 0x009088 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeRedirectInfo(const u8 *buf, u32 bufLen, NpProtoRedirectInfo *outInfo)
{
    s32 ret;

    sceKernelMemcpy(&outInfo->kind, buf, 1);
    sceKernelMemcpy(&outInfo->extraWord, buf + 1, 2);
    sceKernelMemcpy(&outInfo->statusByte, buf + 3, 1);
    ret = npProtoParseTlvStream(buf + 4, bufLen - 4, g_specRedirectHost, 1,
                                npProtoOnRedirectHostTlv, outInfo);
    if (ret >= 0) {
        ret += 4;
    }
    return ret;
}

/* 0x009138 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeTimestamp(const u8 *buf, u32 bufLen, u64 *tickOut)
{
    u32 rawWords[2];
    u32 hiWord;
    u32 loWord;
    (void)bufLen;

    sceKernelMemcpy(rawWords, buf, 8);
    hiWord = npSwap32(rawWords[0]);
    loWord = npSwap32(rawWords[1]);
    if ((loWord | hiWord) != 0) {
        u64 epochMs = ((u64)hiWord << 32) | (u64)loWord;
        *tickOut = epochMs * 1000ULL + NP_EPOCH_TO_RTC_OFFSET;
    } else {
        sceKernelMemset(tickOut, 0, 8);
    }
    return 8;
}

/* 0x009218 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeBinAttr(const u8 *buf, u32 bufLen, u16 attrId, SceNpMatching2BinAttr *attrOut)
{
    if (bufLen != 0) {
        void *copy = npMatching2Alloc(bufLen);
        attrOut->ptr = copy;
        if (copy == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(copy, buf, bufLen);
        attrOut->size = bufLen;
    } else {
        attrOut->size = 0;
    }
    attrOut->id = attrId;
    return 0;
}

/* 0x0092b4 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeGroupConfig(const u8 *buf, u32 bufLen, NpProtoGroupConfigWire *outGroup)
{
    (void)bufLen;
    sceKernelMemcpy(&outGroup->groupId, buf, 1);
    sceKernelMemcpy(&outGroup->withPassword, buf + 1, 1);
    sceKernelMemcpy(outGroup->label, buf + 2, 8);
    return 10;
}

/* 0x009318 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeWrappedBlob(const u8 *buf, u16 bufLen, void **ptrOut, u32 *lenOut)
{
    NpProtoBlobDecodeCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.ptrOut = ptrOut;
    ctx.lenOut = lenOut;
    return npProtoParseTlvStream(buf, bufLen, g_specWrappedBlob, 1, npProtoOnWrappedBlobTlv, &ctx);
}

/* 0x00939c */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeJoinedSessionList(const u8 *buf, u16 bufLen, u32 *countOut,
                                   SceNpMatching2ServerId serverId,
                                   SceNpMatching2JoinedSessionInfo **listOut)
{
    u16 innerTag;
    u16 innerCount;
    SceNpMatching2ServerId ctxServerId;
    s32 parsed;

    sceKernelMemcpy(&innerTag, buf, 2);
    innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
    sceKernelMemcpy(&innerCount, buf + 2, 2);
    innerCount = npSwap16(innerCount);

    if (innerTag != 30) {
        return 0;
    }
    ctxServerId = serverId;
    parsed = npProtoParseTlvArray(30, buf + 4, (u16)(bufLen - 4), innerCount,
                                  (void **)listOut, (u32)innerCount * sizeof(SceNpMatching2JoinedSessionInfo),
                                  npProtoOnJoinedSessionItem, &ctxServerId);
    if (parsed >= 0) {
        *countOut = (u32)parsed;
        return 0;
    }
    return parsed;
}

/* 0x009484 */
NP_MATCHING2_NOINLINE
s32 npProtoDecodeMsgOptData(const u8 *buf, u16 bufLen, NpProtoMsgOptData *outOpt)
{
    NpProtoMsgOptDecodeCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.optOut = outOpt;
    return npProtoParseTlvStream(buf, bufLen, g_specMsgOptData, 4, npProtoOnMsgOptDataTlv, &ctx);
}

/* 0x0094f8 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnHostNameTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    char *hostBuf = (char *)ctx;
    if (spec->tag == 9) {
        sceKernelMemcpy(hostBuf, val, len);
        hostBuf[len] = '\0';
    }
    return 0;
}

/* 0x009550 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnHostEntryItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    NpMatching2HostEntry *entry = (NpMatching2HostEntry *)outItem;
    s32 ret;
    (void)tag;
    (void)ctx;

    sceKernelMemcpy(&entry->port, val, 2);
    entry->port = npSwap16(entry->port);
    ret = npProtoParseTlvStream(val + 2, (u32)(len - 2), g_specHostName, 1,
                                npProtoOnHostNameTlv, entry->host);
    if (ret >= 0) {
        return (s32)sizeof(NpMatching2HostEntry);
    }
    return ret;
}

/* 0x0095f0 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRedirectHostTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRedirectInfo *info = (NpProtoRedirectInfo *)ctx;
    if (spec->tag == 10) {
        s32 ret;
        sceKernelMemcpy(&info->port, val, 2);
        info->port = npSwap16(info->port);
        ret = npProtoParseTlvStream(val + 2, (u32)(len - 2), g_specHostName, 1,
                                    npProtoOnHostNameTlv, info->host);
        if (ret >= 0) {
            return ret + 2;
        }
        return ret;
    }
    return 0;
}

/* 0x0096ac */
NP_MATCHING2_NOINLINE
static s32 npProtoOnFlagValueTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoFlagDecodeCtx *flagCtx = (NpProtoFlagDecodeCtx *)ctx;
    (void)len;
    if (spec->tag == flagCtx->targetTag) {
        sceKernelMemcpy(flagCtx->valueOut, val, 4);
        *flagCtx->valueOut = npSwap32(*flagCtx->valueOut);
    }
    return 0;
}

/* 0x0096fc */
NP_MATCHING2_NOINLINE
static s32 npProtoOnWrappedBlobTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoBlobDecodeCtx *blobCtx = (NpProtoBlobDecodeCtx *)ctx;
    if (spec->tag == 1) {
        if (len != 0) {
            void *copy = npMatching2Alloc(len);
            *blobCtx->ptrOut = copy;
            if (copy == NULL) {
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(copy, val, len);
            *blobCtx->lenOut = len;
        } else {
            *blobCtx->lenOut = 0;
            *blobCtx->ptrOut = NULL;
        }
    }
    return 0;
}

/* 0x0097ac */
NP_MATCHING2_NOINLINE
static s32 npProtoOnJoinedSessionItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    const SceNpMatching2ServerId *serverId = (const SceNpMatching2ServerId *)ctx;
    s32 ret;
    (void)tag;

    ret = npProtoDecodeJoinedSession(val, len, *serverId, (SceNpMatching2JoinedSessionInfo *)outItem);
    if (ret >= 0) {
        return (s32)sizeof(SceNpMatching2JoinedSessionInfo);
    }
    return ret;
}

/* 0x0097e4 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnMsgOptDataTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMsgOptDecodeCtx *optCtx = (NpProtoMsgOptDecodeCtx *)ctx;
    NpProtoMsgOptData *outOpt = optCtx->optOut;

    if (spec->tag == 11) {
        sceKernelMemcpy(outOpt->npId.handle, val, 16);
        sceKernelMemcpy(&outOpt->npId.opt[0], val + 16, 2);
        sceKernelMemcpy(&outOpt->npId.opt[2], val + 18, 2);
        sceKernelMemcpy(&outOpt->npId.opt[4], val + 20, 4);
        outOpt->npId.reserved[0] = 1;
        optCtx->optOut->hasProfile = 1;
        return 24;
    }
    if (spec->tag == 1) {
        if (len != 0) {
            sceKernelMemcpy(&outOpt->flag, val, 1);
        }
        return 0;
    }
    if (spec->tag == 12) {
        if (len == 0) {
            optCtx->optOut->onlineName = NULL;
            return 0;
        }
        optCtx->optOut->onlineName = (char *)npMatching2Alloc(52);
        if (optCtx->optOut->onlineName == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(optCtx->optOut->onlineName, 0, 52);
        sceKernelMemcpy(optCtx->optOut->onlineName, val, len);
        return 0;
    }
    if (spec->tag == 13) {
        if (len == 0) {
            optCtx->optOut->avatarUrl = NULL;
            return 0;
        }
        optCtx->optOut->avatarUrl = (char *)npMatching2Alloc(128);
        if (optCtx->optOut->avatarUrl == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(optCtx->optOut->avatarUrl, 0, 128);
        sceKernelMemcpy(optCtx->optOut->avatarUrl, val, len);
        return 0;
    }
    return 0;
}

/* 0x00999c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildAuthReq(u8 *outBuf, u32 outBufSize, u32 txId, const char *clientVer,
                        const SceNpCommId *commId, u16 serverId, const SceNpId *npId,
                        const char *onlineName, const char *avatarUrl,
                        const SceNpCommPassphrase *passPhrase, const void *ticket, u16 ticketLen,
                        const void *psid)
{
    u8 *cursor;
    s32 step;
    u16 serverIdBe;
    u32 totalLen;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 15,
                            (u16)sceNet_lib_D8722983(clientVer), clientVer);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeCommId(cursor, outBufSize - (u32)(cursor - outBuf), commId);
    if (step < 0) {
        return step;
    }
    cursor += step;

    serverIdBe = npSwap16(serverId);
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 115, 2, &serverIdBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeNpId(cursor, outBufSize - (u32)(cursor - outBuf), npId);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (onlineName != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 12, 48, onlineName);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (avatarUrl != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 13,
                                (u16)sceNet_lib_D8722983(avatarUrl), avatarUrl);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 21, 128, passPhrase);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 17, ticketLen, ticket);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 111, 16, psid);
    if (step < 0) {
        return step;
    }
    cursor += step;

    totalLen = (u32)(cursor - outBuf);
    step = npProtoBuildHeader(outBuf, 32, 0x1002, 0, txId, NULL, (u16)totalLen, NULL, 0);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x009bf0 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetUserInfoListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                   const void *hmacKey, const SceNpCommId *commId, u16 serverId,
                                   const char (*handles)[20], u32 handleCount,
                                   const SceNpMatching2AttributeId *attrIds, u32 attrIdCount)
{
    u8 *cursor;
    s32 step;
    u16 serverIdBe;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeCommId(cursor, outBufSize - 32, commId);
    if (step < 0) {
        return step;
    }
    cursor += step;

    serverIdBe = npSwap16(serverId);
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 115, 2, &serverIdBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeNpHandleList(cursor, outBufSize - (u32)(cursor - outBuf), 14, handles, handleCount);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeAttrIdList(cursor, outBufSize - (u32)(cursor - outBuf), attrIds, attrIdCount);
    if (step < 0) {
        return step;
    }
    cursor += step;

    totalLen = (u32)(cursor - outBuf);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1007, 0, txId, &routeWord, (u16)totalLen, hmacKey, 16);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x009d70 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetServerInfoReq(u8 *outBuf, u32 outBufSize, u32 txId, const void *hmacKey,
                                 const SceNpCommId *commId, u16 serverId)
{
    u8 *cursor;
    s32 step;
    u16 serverIdBe;
    u32 totalLen;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeCommId(cursor, outBufSize - 32, commId);
    if (step < 0) {
        return step;
    }
    cursor += step;

    serverIdBe = npSwap16(serverId);
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 115, 2, &serverIdBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    totalLen = (u32)(cursor - outBuf);
    step = npProtoBuildHeader(outBuf, 32, 0x1001, 0, txId, NULL, (u16)totalLen, hmacKey, 16);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x009e6c */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetServerInfoResp(const u8 *buf, u32 bufLen, u16 *statusOut, u8 *versionOut)
{
    NpProtoServerInfoCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.statusOut = statusOut;
    ctx.versionOut = versionOut;
    return npProtoParseTlvStream(buf, bufLen, g_specServerInfo, 1, npProtoOnServerInfoTlv, &ctx);
}

/* 0x009eec */
NP_MATCHING2_NOINLINE
s32 npProtoParseAuthResp(const u8 *buf, u32 bufLen, u32 *sessionIdOut,
                         NpMatching2BlobBuf *sessionTokenOut, NpMatching2BlobBuf *sessionKeyOut,
                         NpMatching2BlobBuf *keepaliveCookieOut, u16 *keepaliveIntervalOut)
{
    NpProtoAuthRespCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    sessionTokenOut->len = 0;
    sessionKeyOut->len = 0;
    keepaliveCookieOut->len = 0;
    ctx.sessionIdOut = sessionIdOut;
    ctx.sessionTokenOut = sessionTokenOut;
    ctx.sessionKeyOut = sessionKeyOut;
    ctx.keepaliveCookieOut = keepaliveCookieOut;
    ctx.keepaliveIntervalOut = keepaliveIntervalOut;
    return npProtoParseTlvStream(buf, bufLen, g_specAuthResp, 5, npProtoOnAuthRespTlv, &ctx);
}

/* 0x009fa8 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildKeepaliveReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                             const NpMatching2BlobBuf *sessionKey, const u16 *worldIds, u32 worldIdCount)
{
    u8 *cursor;
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (worldIds != NULL && worldIdCount != 0) {
        step = npProtoEncodeWorldIdList(cursor, outBufSize - 32, worldIds, worldIdCount);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    totalLen = (u32)(cursor - outBuf);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1003, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00a098 */
NP_MATCHING2_NOINLINE
s32 npProtoParseKeepaliveResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut)
{
    NpMatching2BlobBuf *ctxPtr;
    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    cookieOut->len = 0;
    ctxPtr = cookieOut;
    return npProtoParseTlvStream(buf, bufLen, g_specKeepaliveResp, 1, npProtoOnKeepaliveTlv, &ctxPtr);
}

/* 0x00a10c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSetUserInfoReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                               const NpMatching2BlobBuf *sessionKey, const SceNpMatching2BinAttr *userAttr)
{
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    step = npProtoEncodeTlv(outBuf + 32, outBufSize - 32, 95, (u16)userAttr->size, userAttr->ptr);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1006, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00a1ec */
NP_MATCHING2_NOINLINE
s32 npProtoParseSetUserInfoResp(void)
{
    return 0;
}

/* 0x00a1f4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetUserInfoListResp(const u8 *buf, u32 bufLen, u16 *countOut,
                                    SceNpMatching2ServerId serverId, NpProtoUserInfo **listOut)
{
    NpProtoUserInfoListCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.listOut = listOut;
    ctx.serverId = serverId;

    ret = npProtoParseTlvStream(buf, bufLen, g_specUserInfoList, 1, npProtoOnUserInfoListTlv, &ctx);
    if (ret < 0) {
        NpProtoUserInfo *list = *listOut;
        if (list != NULL) {
            u32 idx;
            for (idx = 0; idx < *countOut; idx++) {
                if (list[idx].onlineName != NULL) {
                    npMatching2Free(list[idx].onlineName);
                }
                list[idx].onlineName = NULL;
                if (list[idx].avatarUrl != NULL) {
                    npMatching2Free(list[idx].avatarUrl);
                }
                list[idx].avatarUrl = NULL;
                if (list[idx].userBinAttr.ptr != NULL) {
                    npMatching2Free(list[idx].userBinAttr.ptr);
                }
                list[idx].userBinAttr.ptr = NULL;
                if (list[idx].joinedSessionList != NULL) {
                    npMatching2Free(list[idx].joinedSessionList);
                }
                list[idx].joinedSessionList = NULL;
            }
            npMatching2Free(list);
        }
        *listOut = NULL;
        *countOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00a36c */
NP_MATCHING2_NOINLINE
void npProtoFreeUserInfoList(NpProtoUserInfo *list, u16 count)
{
    if (list != NULL) {
        u32 idx;
        for (idx = 0; idx < count; idx++) {
            if (list[idx].onlineName != NULL) {
                npMatching2Free(list[idx].onlineName);
            }
            list[idx].onlineName = NULL;
            if (list[idx].avatarUrl != NULL) {
                npMatching2Free(list[idx].avatarUrl);
            }
            list[idx].avatarUrl = NULL;
            if (list[idx].userBinAttr.ptr != NULL) {
                npMatching2Free(list[idx].userBinAttr.ptr);
            }
            list[idx].userBinAttr.ptr = NULL;
            if (list[idx].joinedSessionList != NULL) {
                npMatching2Free(list[idx].joinedSessionList);
            }
            list[idx].joinedSessionList = NULL;
        }
        npMatching2Free(list);
    }
}

/* 0x00a450 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetWorldInfoListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                    const NpMatching2BlobBuf *sessionKey)
{
    u32 routeWord;
    s32 ret;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    ret = npProtoBuildHeader(outBuf, 32, 0x1008, 0, txId, &routeWord, 32,
                             sessionKey->bytes, sessionKey->len);
    if (ret >= 0) {
        return 32;
    }
    return ret;
}

/* 0x00a50c */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetWorldInfoListResp(const u8 *buf, u32 bufLen, u32 *countOut,
                                     NpProtoWorldInfo **listOut, SceNpMatching2ServerId serverId)
{
    NpProtoWorldInfoListCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.listOut = listOut;
    ctx.serverId = serverId;

    ret = npProtoParseTlvStream(buf, bufLen, g_specWorldInfoList, 1, npProtoOnWorldInfoListTlv, &ctx);
    if (ret < 0) {
        if (*listOut != NULL) {
            npMatching2Free(*listOut);
        }
        *listOut = NULL;
        *countOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00a5dc */
NP_MATCHING2_NOINLINE
static s32 npProtoOnServerInfoTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoServerInfoCtx *srvCtx = (NpProtoServerInfoCtx *)ctx;
    (void)len;
    if (spec->tag == 29) {
        sceKernelMemcpy(srvCtx->statusOut, val, 2);
        *srvCtx->statusOut = npSwap16(*srvCtx->statusOut);
        sceKernelMemcpy(srvCtx->versionOut, val + 2, 1);
    }
    return 0;
}

/* 0x00a64c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnAuthRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoAuthRespCtx *authCtx = (NpProtoAuthRespCtx *)ctx;
    if (spec->tag == 20) {
        sceKernelMemcpy(authCtx->sessionIdOut, val, len);
    } else if (spec->tag == 35) {
        sceKernelMemcpy(authCtx->sessionTokenOut->bytes, val, len);
        authCtx->sessionTokenOut->len = len;
    } else if (spec->tag == 34) {
        sceKernelMemcpy(authCtx->sessionKeyOut->bytes, val, len);
        authCtx->sessionKeyOut->len = len;
    } else if (spec->tag == 93) {
        sceKernelMemcpy(authCtx->keepaliveCookieOut->bytes, val, len);
        authCtx->keepaliveCookieOut->len = len;
    } else if (spec->tag == 116) {
        sceKernelMemcpy(authCtx->keepaliveIntervalOut, val, len);
        *authCtx->keepaliveIntervalOut = npSwap16(*authCtx->keepaliveIntervalOut);
    }
    return 0;
}

/* 0x00a734 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnKeepaliveTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpMatching2BlobBuf **cookiePtr = (NpMatching2BlobBuf **)ctx;
    if (spec->tag == 93) {
        sceKernelMemcpy((*cookiePtr)->bytes, val, len);
        (*cookiePtr)->len = len;
    }
    return 0;
}

/* 0x00a78c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnUserInfoListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoUserInfoListCtx *listCtx = (NpProtoUserInfoListCtx *)ctx;
    if (spec->tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 94) {
            s32 parsed = npProtoParseTlvArray(94, val + 4, (u16)(len - 4), innerCount,
                                              (void **)listCtx->listOut,
                                              (u32)innerCount * sizeof(NpProtoUserInfo),
                                              npProtoOnUserInfoItem, listCtx);
            if (parsed >= 0) {
                *listCtx->countOut = (u16)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x00a878 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnWorldInfoListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoWorldInfoListCtx *listCtx = (NpProtoWorldInfoListCtx *)ctx;
    if (spec->tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 42) {
            s32 parsed = npProtoParseTlvArray(42, val + 4, (u16)(len - 4), innerCount,
                                              (void **)listCtx->listOut,
                                              (u32)innerCount * sizeof(NpProtoWorldInfo),
                                              npProtoOnWorldInfoItem, listCtx);
            if (parsed >= 0) {
                *listCtx->countOut = (u32)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x00a964 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnUserInfoItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    NpProtoUserInfoListCtx *listCtx = (NpProtoUserInfoListCtx *)ctx;
    s32 ret;
    (void)tag;

    listCtx->current = (NpProtoUserInfo *)outItem;
    ret = npProtoParseTlvStream(val, len, g_specUserInfoFields, 5, npProtoOnUserInfoFieldTlv, listCtx);
    if (ret >= 0) {
        return (s32)sizeof(NpProtoUserInfo);
    }
    return ret;
}

/* 0x00a9b0 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnWorldInfoItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    NpProtoWorldInfoListCtx *listCtx = (NpProtoWorldInfoListCtx *)ctx;
    NpProtoWorldInfo *world = (NpProtoWorldInfo *)outItem;
    u16 worldNum = 0;
    u16 tmpHalf;
    u32 tmpWord;
    s32 ret;
    (void)tag;

    sceKernelMemcpy(&worldNum, val, 2);
    worldNum = npSwap16(worldNum);
    world->worldId = ((u32)listCtx->serverId << 16) | (u32)worldNum;

    sceKernelMemcpy(&tmpHalf, val + 2, 2);
    world->numOfLobby = npSwap16(tmpHalf);

    sceKernelMemcpy(&tmpWord, val + 4, 4);
    world->maxNumOfLobby = npSwap32(tmpWord);

    sceKernelMemcpy(&tmpWord, val + 8, 4);
    world->curNumOfLobbyMember = npSwap32(tmpWord);

    sceKernelMemcpy(&tmpHalf, val + 12, 2);
    world->curNumOfRoom = npSwap16(tmpHalf);

    sceKernelMemcpy(&tmpWord, val + 14, 4);
    world->curNumOfRoomMember = npSwap32(tmpWord);

    ret = npProtoParseTlvStream(val + 18, (u32)(len - 18), g_specWorldAttr, 1,
                                npProtoOnWorldAttrTlv, world);
    if (ret >= 0) {
        if (listCtx->prevNode != NULL) {
            listCtx->prevNode->next = world;
        }
        listCtx->prevNode = world;
        return (s32)sizeof(NpProtoWorldInfo);
    }
    return ret;
}

/* 0x00ab20 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnUserInfoFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoUserInfoListCtx *listCtx = (NpProtoUserInfoListCtx *)ctx;
    NpProtoUserInfo *user = listCtx->current;

    if (spec->tag == 11) {
        return npProtoDecodeNpId(val, len, &user->npId);
    }
    if (spec->tag == 12) {
        if (len == 0) {
            user->onlineName = NULL;
            return 0;
        }
        user->onlineName = (char *)npMatching2Alloc(52);
        if (user->onlineName == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(user->onlineName, 0, 52);
        sceKernelMemcpy(user->onlineName, val, len);
        return 0;
    }
    if (spec->tag == 13) {
        if (len == 0) {
            user->avatarUrl = NULL;
            return 0;
        }
        user->avatarUrl = (char *)npMatching2Alloc(128);
        if (user->avatarUrl == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(user->avatarUrl, 0, 128);
        sceKernelMemcpy(user->avatarUrl, val, len);
        return 0;
    }
    if (spec->tag == 95) {
        if (user->userBinAttr.ptr == NULL) {
            return npProtoDecodeBinAttr(val, len, 95, &user->userBinAttr);
        }
        return 0;
    }
    if (spec->tag == 2) {
        return npProtoDecodeJoinedSessionList(val, len, &user->joinedSessionNum,
                                              listCtx->serverId, &user->joinedSessionList);
    }
    return 0;
}

/* 0x00acc4 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnWorldAttrTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoWorldInfo *world = (NpProtoWorldInfo *)ctx;
    if (spec->tag == 18) {
        sceKernelMemcpy(world->attrData, val, len);
        world->withAttr = 1;
    }
    return 0;
}

/* 0x00ad10 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSearchRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                              const NpMatching2BlobBuf *sessionKey, u16 protoCmd, u16 worldNum,
                              u32 unusedPad, u64 lobbyId, const NpProtoRangeWire *range,
                              u32 flagFilter, u32 flagAttr,
                              const NpProtoIntFilterWire *intFilters, u32 intFilterNum,
                              const NpProtoBinFilterWire *binFilters, u32 binFilterNum,
                              const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u16 lobbyWire;
    u32 routeWord;
    (void)unusedPad;

    if (range->startIndex > 0xFFFF || range->maxNum > 0xFFFF) {
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    }
    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    worldWire = (lobbyId != 0) ? (u16)(lobbyId >> 32) : worldNum;
    worldWire = npSwap16(worldWire);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (lobbyId != 0) {
        lobbyWire = npSwap16((u16)(lobbyId >> 16));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 46, 2, &lobbyWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeRange(cursor, outBufSize - (u32)(cursor - outBuf), range);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeFlagPair(cursor, outBufSize - (u32)(cursor - outBuf), flagFilter, 64, flagAttr);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (intFilters != NULL && intFilterNum != 0) {
        step = npProtoEncodeIntFilterList(cursor, outBufSize - (u32)(cursor - outBuf),
                                          intFilters, intFilterNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (binFilters != NULL && binFilterNum != 0) {
        step = npProtoEncodeBinFilterList(cursor, outBufSize - (u32)(cursor - outBuf),
                                          binFilters, binFilterNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeAttrIdList(cursor, outBufSize - (u32)(cursor - outBuf), attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    cursor += step;

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, protoCmd, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00afa4 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomDataExternalList(NpProtoRoomDataExternal *list, u16 count)
{
    if (list != NULL) {
        u32 idx;
        for (idx = 0; idx < count; idx++) {
            u32 attrIdx;
            if (list[idx].searchableBinAttr.ptr != NULL) {
                npMatching2Free(list[idx].searchableBinAttr.ptr);
            }
            list[idx].searchableBinAttr.ptr = NULL;
            for (attrIdx = 0; attrIdx < 2; attrIdx++) {
                if (list[idx].binAttrExternal[attrIdx].ptr != NULL) {
                    npMatching2Free(list[idx].binAttrExternal[attrIdx].ptr);
                }
                list[idx].binAttrExternal[attrIdx].ptr = NULL;
            }
            if (list[idx].ownerNpId != NULL) {
                npMatching2Free(list[idx].ownerNpId);
            }
            list[idx].ownerNpId = NULL;
            if (list[idx].ownerOnlineName != NULL) {
                npMatching2Free(list[idx].ownerOnlineName);
            }
            list[idx].ownerOnlineName = NULL;
            if (list[idx].ownerAvatarUrl != NULL) {
                npMatching2Free(list[idx].ownerAvatarUrl);
            }
            list[idx].ownerAvatarUrl = NULL;
        }
        npMatching2Free(list);
    }
}

/* 0x00b0d8 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildCreateRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                 const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie,
                                 const SceNpMatching2IntAttr *intAttrs, u32 intAttrNum,
                                 const SceNpMatching2BinAttr *binAttrs, u32 binAttrNum,
                                 const NpMatching2BlobBuf *extMeta)
{
    u8 *cursor;
    s32 step;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 58, (u16)roomCookie->len, roomCookie->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeIntAttrList(cursor, outBufSize - (u32)(cursor - outBuf), intAttrs, intAttrNum);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (binAttrs != NULL && binAttrNum != 0) {
        step = npProtoEncodeBinAttrList(cursor, outBufSize - (u32)(cursor - outBuf), binAttrs, binAttrNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (extMeta != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 108,
                                (u16)extMeta->len, extMeta->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1202, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00b274 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetRoomMemberExtListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                        const NpMatching2BlobBuf *sessionKey, u32 unusedPad,
                                        u16 roomNum, u16 worldNum,
                                        const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u16 roomWire;
    u32 routeWord;
    (void)unusedPad;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    worldWire = npSwap16(worldNum);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    roomWire = npSwap16(roomNum);
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 61, 2, &roomWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeAttrIdList(cursor, outBufSize - (u32)(cursor - outBuf), attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    cursor += step;

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1206, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00b3d8 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSetRoomDataExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                  const NpMatching2BlobBuf *sessionKey, u32 unusedPad,
                                  u16 roomNum, u16 worldNum,
                                  const SceNpMatching2IntAttr *intAttrs, u32 intAttrNum,
                                  const SceNpMatching2BinAttr *binAttrs, u32 binAttrNum,
                                  const NpMatching2BlobBuf *extMeta)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u16 roomWire;
    u32 routeWord;
    (void)unusedPad;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    worldWire = npSwap16(worldNum);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    roomWire = npSwap16(roomNum);
    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 61, 2, &roomWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (intAttrs != NULL && intAttrNum != 0) {
        step = npProtoEncodeIntAttrList(cursor, outBufSize - (u32)(cursor - outBuf), intAttrs, intAttrNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (binAttrs != NULL && binAttrNum != 0) {
        step = npProtoEncodeBinAttrList(cursor, outBufSize - (u32)(cursor - outBuf), binAttrs, binAttrNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (extMeta != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 108,
                                (u16)extMeta->len, extMeta->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1207, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00b5cc */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetRoomDataExtListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                      const NpMatching2BlobBuf *sessionKey, const SceNpMatching2RoomId *roomIds,
                                      u32 roomIdNum, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u32 idx;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    worldWire = npSwap16((u16)(roomIds[0] >> 32));
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    for (idx = 0; idx < roomIdNum; idx++) {
        u16 roomWire = npSwap16((u16)(roomIds[idx] & 0xFFFF));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 61, 2, &roomWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeAttrIdList(cursor, outBufSize - (u32)(cursor - outBuf), attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    cursor += step;

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1208, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00b75c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildReserveRoomSlotReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                   const NpMatching2BlobBuf *sessionKey, u16 worldNum,
                                   u64 lobbyId, SceNpMatching2RoomId roomId)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u16 lobbyWire;
    u16 roomWire;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    worldWire = (lobbyId != 0) ? (u16)(lobbyId >> 32) : worldNum;
    worldWire = npSwap16(worldWire);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (lobbyId != 0) {
        lobbyWire = npSwap16((u16)(lobbyId >> 16));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 46, 2, &lobbyWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomId != 0) {
        roomWire = npSwap16((u16)(roomId & 0xFFFF));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 61, 2, &roomWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1209, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00b920 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildReserveJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                   const NpMatching2BlobBuf *sessionKey, SceNpMatching2WorldId worldId,
                                   SceNpMatching2RoomId roomId)
{
    u8 *cursor;
    s32 step;
    u16 worldWire;
    u16 roomWire;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (worldId != 0) {
        worldWire = npSwap16((u16)(worldId & 0xFFFF));
        step = npProtoEncodeTlv(cursor, outBufSize - 32, 41, 2, &worldWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomId != 0) {
        roomWire = npSwap16((u16)(roomId & 0xFFFF));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 61, 2, &roomWire);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x120B, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00ba70 */
NP_MATCHING2_NOINLINE
s32 npProtoParseSearchRoomResp(const u8 *buf, u32 bufLen, u32 *startIndexOut, u32 *totalOut,
                               u32 *countOut, NpProtoRoomDataExternal **listOut,
                               SceNpMatching2ServerId serverId)
{
    NpProtoSearchRoomCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.startIndexOut = startIndexOut;
    ctx.totalOut = totalOut;
    ctx.listOut = listOut;
    ctx.serverId = serverId;

    ret = npProtoParseTlvStream(buf, bufLen, g_specSearchRoomList, 1, npProtoOnSearchRoomListTlv, &ctx);
    if (ret < 0) {
        if (*listOut != NULL) {
            npProtoFreeRoomDataExternalList(*listOut, (u16)*countOut);
        }
        *listOut = NULL;
        *countOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00bb60 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRedirectResp(const u8 *buf, u32 bufLen, NpProtoRedirectInfo *redirectOut, s32 *foundOut)
{
    NpProtoRedirectCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *foundOut = 0;
    ctx.redirectOut = redirectOut;
    ctx.foundOut = foundOut;
    return npProtoParseTlvStream(buf, bufLen, g_specRedirectResp, 1, npProtoOnRedirectTlv, &ctx);
}

/* 0x00bbe4 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildJoinRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                               const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie)
{
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeTlv(outBuf + 32, outBufSize - 32, 58, (u16)roomCookie->len, roomCookie->bytes);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1203, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00bcc8 */
NP_MATCHING2_NOINLINE
s32 npProtoParseJoinRoomExtResp(void)
{
    return 0;
}

/* 0x00bcd0 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildLeaveRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie)
{
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeTlv(outBuf + 32, outBufSize - 32, 58, (u16)roomCookie->len, roomCookie->bytes);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1204, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00bdb4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseLeaveRoomExtResp(void)
{
    return 0;
}

/* 0x00bdbc */
NP_MATCHING2_NOINLINE
s32 npProtoBuildUpdateRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                 const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie)
{
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeTlv(outBuf + 32, outBufSize - 32, 58, (u16)roomCookie->len, roomCookie->bytes);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x1205, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00bea0 */
NP_MATCHING2_NOINLINE
s32 npProtoParseUpdateRoomExtResp(void)
{
    return 0;
}

/* 0x00bea8 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetRoomMemberExtListResp(const u8 *buf, u32 bufLen, u16 *countOut,
                                         NpProtoRoomMemberExternal **listOut)
{
    NpProtoRoomMemberExtListCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.listOut = listOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomMemberExtList, 1,
                                npProtoOnRoomMemberExtListTlv, &ctx);
    if (ret < 0) {
        NpProtoRoomMemberExternal *list = *listOut;
        if (list != NULL) {
            u32 idx;
            for (idx = 0; idx < *countOut; idx++) {
                if (list[idx].onlineName != NULL) {
                    npMatching2Free(list[idx].onlineName);
                }
                list[idx].onlineName = NULL;
                if (list[idx].avatarUrl != NULL) {
                    npMatching2Free(list[idx].avatarUrl);
                }
                list[idx].avatarUrl = NULL;
            }
            npMatching2Free(list);
        }
        *listOut = NULL;
        *countOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00bfd8 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomMemberExtList(NpProtoRoomMemberExternal *list, u16 count)
{
    if (list != NULL) {
        u32 idx;
        for (idx = 0; idx < count; idx++) {
            if (list[idx].onlineName != NULL) {
                npMatching2Free(list[idx].onlineName);
            }
            list[idx].onlineName = NULL;
            if (list[idx].avatarUrl != NULL) {
                npMatching2Free(list[idx].avatarUrl);
            }
            list[idx].avatarUrl = NULL;
        }
        npMatching2Free(list);
    }
}

/* 0x00c07c */
NP_MATCHING2_NOINLINE
s32 npProtoParseSetRoomDataExtResp(void)
{
    return 0;
}

/* 0x00c084 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetRoomDataExtListResp(const u8 *buf, u32 bufLen, u16 *countOut,
                                       NpProtoRoomDataExternal **listOut,
                                       SceNpMatching2ServerId serverId)
{
    NpProtoRoomDataExtListCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.listOut = listOut;
    ctx.serverId = serverId;

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomDataExtList, 1,
                                npProtoOnRoomDataExtListTlv, &ctx);
    if (ret < 0) {
        if (*listOut != NULL) {
            npProtoFreeRoomDataExternalList(*listOut, *countOut);
        }
        *listOut = NULL;
        *countOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00c154 */
NP_MATCHING2_NOINLINE
s32 npProtoParseReserveRoomResp(const u8 *buf, u32 bufLen, u16 *roomHandleOut,
                                NpProtoRedirectInfo *redirectOut, NpMatching2BlobBuf *ticketOut,
                                NpMatching2HostList *hostListOut, void *sigOptOut, s32 *hasSigOptOut)
{
    NpProtoReserveRoomCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.roomHandleOut = roomHandleOut;
    if (roomHandleOut != NULL) {
        *roomHandleOut = 0;
    }
    ticketOut->len = 0;
    ctx.redirectOut = redirectOut;
    ctx.ticketOut = ticketOut;
    ctx.hostListOut = hostListOut;
    if (hostListOut != NULL) {
        sceKernelMemset(hostListOut, 0, sizeof(*hostListOut));
    }
    *hasSigOptOut = 0;
    ctx.sigOptOut = sigOptOut;
    ctx.hasSigOptOut = hasSigOptOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specReserveRoomResp, 5, npProtoOnReserveRoomTlv, &ctx);
    if (ret < 0) {
        if (ctx.hostListOut->entries != NULL) {
            npMatching2Free(ctx.hostListOut->entries);
        }
        ctx.hostListOut->entries = NULL;
        return ret;
    }
    return 0;
}

/* 0x00c274 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildReleaseRoomSlotReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                   const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie)
{
    s32 step;
    u32 totalLen;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeTlv(outBuf + 32, outBufSize - 32, 58, (u16)roomCookie->len, roomCookie->bytes);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x120A, 0, txId, &routeWord, (u16)totalLen,
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00c358 */
NP_MATCHING2_NOINLINE
s32 npProtoParseReleaseRoomSlotResp(void)
{
    return 0;
}

/* 0x00c360 */
NP_MATCHING2_NOINLINE
s32 npProtoParseEmptyResp(void)
{
    return 0;
}

/* 0x00c368 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnSearchRoomListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoSearchRoomCtx *searchCtx = (NpProtoSearchRoomCtx *)ctx;
    if (spec->tag == 3) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 69) {
            u16 tmpHalf;
            s32 parsed;
            sceKernelMemcpy(&tmpHalf, val + 4, 2);
            *searchCtx->startIndexOut = npSwap16(tmpHalf);
            sceKernelMemcpy(&tmpHalf, val + 6, 2);
            *searchCtx->totalOut = npSwap16(tmpHalf);
            parsed = npProtoParseTlvArray(69, val + 8, (u16)(len - 8), innerCount,
                                          (void **)searchCtx->listOut,
                                          (u32)innerCount * sizeof(NpProtoRoomDataExternal),
                                          npProtoOnRoomDataExtItem, &searchCtx->serverId);
            if (parsed >= 0) {
                *searchCtx->countOut = (u32)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x00c4ac */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRedirectTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRedirectCtx *redirCtx = (NpProtoRedirectCtx *)ctx;
    if (spec->tag == 31) {
        s32 ret = npProtoDecodeRedirectInfo(val, len, redirCtx->redirectOut);
        if (ret >= 0) {
            *redirCtx->foundOut = 1;
        }
        return ret;
    }
    return 0;
}

/* 0x00c50c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberExtListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomMemberExtListCtx *listCtx = (NpProtoRoomMemberExtListCtx *)ctx;
    if (spec->tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 74) {
            s32 parsed;
            listCtx->prevNode = NULL;
            parsed = npProtoParseTlvArray(74, val + 4, (u16)(len - 4), innerCount,
                                          (void **)listCtx->listOut,
                                          (u32)innerCount * sizeof(NpProtoRoomMemberExternal),
                                          npProtoOnRoomMemberExtItem, listCtx);
            if (parsed >= 0) {
                *listCtx->countOut = (u16)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x00c5fc */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDataExtListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDataExtListCtx *listCtx = (NpProtoRoomDataExtListCtx *)ctx;
    if (spec->tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 69) {
            s32 parsed = npProtoParseTlvArray(69, val + 4, (u16)(len - 4), innerCount,
                                              (void **)listCtx->listOut,
                                              (u32)innerCount * sizeof(NpProtoRoomDataExternal),
                                              npProtoOnRoomDataExtItem, &listCtx->serverId);
            if (parsed >= 0) {
                *listCtx->countOut = (u16)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x00c6f4 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnReserveRoomTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoReserveRoomCtx *resCtx = (NpProtoReserveRoomCtx *)ctx;
    if (spec->tag == 31) {
        if (resCtx->redirectOut != NULL) {
            return npProtoDecodeRedirectInfo(val, len, resCtx->redirectOut);
        }
        return 0;
    }
    if (spec->tag == 32) {
        sceKernelMemcpy(resCtx->ticketOut->bytes, val, len);
        resCtx->ticketOut->len = len;
        return 0;
    }
    if (spec->tag == 61) {
        if (resCtx->roomHandleOut != NULL) {
            sceKernelMemcpy(resCtx->roomHandleOut, val, len);
            *resCtx->roomHandleOut = npSwap16(*resCtx->roomHandleOut);
        }
        return 0;
    }
    if (spec->tag == 2) {
        if (resCtx->hostListOut != NULL) {
            return npProtoDecodeHostList(val, len, &resCtx->hostListOut->count,
                                         &resCtx->hostListOut->entries);
        }
        return 0;
    }
    if (spec->tag == 18) {
        sceKernelMemcpy(resCtx->sigOptOut, val, len);
        *resCtx->hasSigOptOut = 1;
    }
    return 0;
}

/* 0x00c808 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDataExtItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    const SceNpMatching2ServerId *serverId = (const SceNpMatching2ServerId *)ctx;
    NpProtoRoomDataExternal *room = (NpProtoRoomDataExternal *)outItem;
    u16 worldNum = 0;
    u16 lobbyNum = 0;
    u16 roomNum = 0;
    u32 rawWords[2];
    s32 ret;
    (void)tag;

    sceKernelMemcpy(&worldNum, val, 2);
    worldNum = npSwap16(worldNum);
    room->worldId = ((u32)(*serverId) << 16) | (u32)worldNum;

    sceKernelMemcpy(&lobbyNum, val + 2, 2);
    lobbyNum = npSwap16(lobbyNum);
    if (lobbyNum != 0) {
        room->lobbyId = (((u64)(((u32)(*serverId) << 16) | (u32)worldNum)) << 32) | ((u64)lobbyNum << 16);
    }

    sceKernelMemcpy(&roomNum, val + 4, 2);
    roomNum = npSwap16(roomNum);
    room->roomId = (((u64)(((u32)(*serverId) << 16) | (u32)worldNum)) << 32) | (((u64)lobbyNum << 16) | (u64)roomNum);

    sceKernelMemcpy(&room->flagAttr, val + 6, 4);
    room->flagAttr = npSwap32(room->flagAttr);

    sceKernelMemcpy(&room->maxSlot, val + 10, 2);
    room->maxSlot = npSwap16(room->maxSlot);

    sceKernelMemcpy(room->reservedMeta, val + 12, 16);

    sceKernelMemcpy(&room->openDate, val + 28, 8);
    room->openDate = npSwap64(room->openDate);

    ret = npProtoParseTlvStream(val + 36, (u32)(len - 36), g_specRoomDataExtFields, 16,
                                npProtoOnRoomDataExtFieldTlv, room);
    if (ret >= 0) {
        return (s32)sizeof(NpProtoRoomDataExternal);
    }
    return ret;
}

/* 0x00cab4 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberExtItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    NpProtoRoomMemberExtListCtx *listCtx = (NpProtoRoomMemberExtListCtx *)ctx;
    NpProtoRoomMemberExternal *member = (NpProtoRoomMemberExternal *)outItem;
    u32 rawWords[2];
    s32 ret;
    (void)tag;

    sceKernelMemcpy(&member->joinDate, val, 8);
    member->joinDate = npSwap64(member->joinDate);
    sceKernelMemcpy(&member->natType, val + 8, 1);

    ret = npProtoParseTlvStream(val + 9, (u32)(len - 9), g_specRoomMemberExtFields, 3,
                                npProtoOnRoomMemberExtFieldTlv, member);
    if (ret >= 0) {
        if (listCtx->prevNode != NULL) {
            listCtx->prevNode->next = member;
        }
        listCtx->prevNode = member;
        return (s32)sizeof(NpProtoRoomMemberExternal);
    }
    return ret;
}

/* 0x00cb9c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDataExtFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDataExternal *room = (NpProtoRoomDataExternal *)ctx;
    u16 tag = spec->tag;

    if (tag >= 76 && tag <= 83) {
        u32 slot = (u32)(tag - 76);
        if (room->searchableIntAttr[slot].id != 0) {
            return 0;
        }
        room->searchableIntAttr[slot].id = tag;
        sceKernelMemcpy(&room->searchableIntAttr[slot].num, val, 4);
        room->searchableIntAttr[slot].num = npSwap32(room->searchableIntAttr[slot].num);
        return 0;
    }
    if (tag == 11) {
        room->ownerNpId = (SceNpId *)npMatching2Alloc(sizeof(SceNpId));
        if (room->ownerNpId == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(room->ownerNpId, 0, sizeof(SceNpId));
        return npProtoDecodeNpId(val, len, room->ownerNpId);
    }
    if (tag == 12) {
        if (len == 0) {
            room->ownerOnlineName = NULL;
            return 0;
        }
        room->ownerOnlineName = (char *)npMatching2Alloc(52);
        if (room->ownerOnlineName == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(room->ownerOnlineName, 0, 52);
        sceKernelMemcpy(room->ownerOnlineName, val, len);
        return 0;
    }
    if (tag == 13) {
        if (len == 0) {
            room->ownerAvatarUrl = NULL;
            return 0;
        }
        room->ownerAvatarUrl = (char *)npMatching2Alloc(128);
        if (room->ownerAvatarUrl == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(room->ownerAvatarUrl, 0, 128);
        sceKernelMemcpy(room->ownerAvatarUrl, val, len);
        return 0;
    }
    if (tag == 65) {
        sceKernelMemcpy(room->publicSlotMeta, val, len);
        return 0;
    }
    if (tag == 84) {
        if (room->searchableBinAttr.ptr != NULL) {
            return 0;
        }
        return npProtoDecodeBinAttr(val, len, tag, &room->searchableBinAttr);
    }
    if (tag == 85) {
        if (room->binAttrExternal[0].ptr != NULL) {
            return 0;
        }
        return npProtoDecodeBinAttr(val, len, tag, &room->binAttrExternal[0]);
    }
    if (tag == 86) {
        if (room->binAttrExternal[1].ptr != NULL) {
            return 0;
        }
        return npProtoDecodeBinAttr(val, len, tag, &room->binAttrExternal[1]);
    }
    if (tag == 108) {
        sceKernelMemcpy(room->extMeta, val, len);
        room->extMetaLen = len;
    }
    return 0;
}

/* 0x00ce40 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberExtFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomMemberExternal *member = (NpProtoRoomMemberExternal *)ctx;

    if (spec->tag == 11) {
        return npProtoDecodeNpId(val, len, &member->npId);
    }
    if (spec->tag == 12) {
        if (len == 0) {
            member->onlineName = NULL;
            return 0;
        }
        member->onlineName = (char *)npMatching2Alloc(52);
        if (member->onlineName == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(member->onlineName, 0, 52);
        sceKernelMemcpy(member->onlineName, val, len);
        return 0;
    }
    if (spec->tag == 13) {
        if (len == 0) {
            member->avatarUrl = NULL;
            return 0;
        }
        member->avatarUrl = (char *)npMatching2Alloc(128);
        if (member->avatarUrl == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(member->avatarUrl, 0, 128);
        sceKernelMemcpy(member->avatarUrl, val, len);
        return 0;
    }
    return 0;
}

/* 0x00cf54 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildRoomKeepaliveReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                 const NpMatching2BlobBuf *sessionKey,
                                 const NpMatching2BlobBuf *sessionToken,
                                 const NpMatching2BlobBuf *keepaliveCookie, u8 flags)
{
    u8 *cursor;
    s32 step;
    u8 flagByte = flags;
    u32 routeWord;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 35, (u16)sessionToken->len, sessionToken->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 93,
                            (u16)keepaliveCookie->len, keepaliveCookie->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 25, 1, &flagByte);
    if (step < 0) {
        return step;
    }
    cursor += step;

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x2001, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00d0a4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomKeepaliveResp(const u8 *buf, u32 bufLen, NpMatching2HostList *hostListOut)
{
    NpMatching2HostList *ctxPtr;
    s32 ret;

    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    ctxPtr = hostListOut;
    sceKernelMemset(hostListOut, 0, sizeof(*hostListOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomKeepaliveResp, 1,
                                npProtoOnRoomKeepaliveHostTlv, &ctxPtr);
    if (ret < 0) {
        if (ctxPtr->entries != NULL) {
            npMatching2Free(ctxPtr->entries);
        }
        ctxPtr->entries = NULL;
        return ret;
    }
    return 0;
}

/* 0x00d15c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomKeepaliveHostTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpMatching2HostList **hostListPtr = (NpMatching2HostList **)ctx;
    if (spec->tag == 2) {
        return npProtoDecodeHostList(val, len, &(*hostListPtr)->count, &(*hostListPtr)->entries);
    }
    return 0;
}

/* 0x00d1a4 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildCreateJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId,
                                  const NpMatching2BlobBuf *sessionKey,
                                  const NpMatching2BlobBuf *sessionToken, const SceNpId *npId,
                                  const NpMatching2BlobBuf *keepaliveCookie,
                                  const NpMatching2BlobBuf *roomTicket,
                                  u32 flagAttr, s32 hasFlagAttr, u32 maxSlot,
                                  const void *publicMeta,
                                  const SceNpMatching2BinAttr *roomBinAttrs, u32 roomBinAttrNum,
                                  const SceNpMatching2SessionPassword *password,
                                  const u64 *passwordSlotMask,
                                  const NpProtoGroupConfigWire *groups, u32 groupNum,
                                  const char (*allowedUsers)[20], u32 allowedUserNum,
                                  const char (*blockedUsers)[20], u32 blockedUserNum,
                                  const NpMatching2BlobBuf *roomAttr,
                                  u8 natType, u8 teamId,
                                  const SceNpMatching2BinAttr *memberBinAttr,
                                  const NpMatching2BlobBuf *memberExtraAttr,
                                  const void *joinGroupLabel)
{
    u8 *cursor;
    s32 step;
    u8 natByte = natType;
    u8 teamByte = teamId;
    u32 routeWord;

    if (maxSlot >= 65) {
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    }
    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 35, (u16)sessionToken->len, sessionToken->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeNpId(cursor, outBufSize - (u32)(cursor - outBuf), npId);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 93,
                            (u16)keepaliveCookie->len, keepaliveCookie->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 32,
                            (u16)roomTicket->len, roomTicket->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (hasFlagAttr == 1) {
        step = npProtoEncodeU32Tlv(cursor, outBufSize - (u32)(cursor - outBuf), 64, flagAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (maxSlot != 0) {
        u16 maxSlotBe = npSwap16((u16)maxSlot);
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 63, 2, &maxSlotBe);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (publicMeta != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 65, 32, publicMeta);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomBinAttrs != NULL && roomBinAttrNum != 0) {
        step = npProtoEncodeRoomBinAttrList(cursor, outBufSize - (u32)(cursor - outBuf),
                                            roomBinAttrs, roomBinAttrNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (password != NULL) {
        u32 maskWords[2];
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 70, 8, password);
        if (step < 0) {
            return step;
        }
        cursor += step;
        maskWords[0] = npSwap32((u32)(*passwordSlotMask >> 32));
        maskWords[1] = npSwap32((u32)(*passwordSlotMask & 0xFFFFFFFFu));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 106, 8, maskWords);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (groups != NULL && groupNum != 0) {
        step = npProtoEncodeGroupConfigList(cursor, outBufSize - (u32)(cursor - outBuf), groups, groupNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (allowedUsers != NULL && allowedUserNum != 0) {
        step = npProtoEncodeNpHandleList(cursor, outBufSize - (u32)(cursor - outBuf), 67,
                                         allowedUsers, allowedUserNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (blockedUsers != NULL && blockedUserNum != 0) {
        step = npProtoEncodeNpHandleList(cursor, outBufSize - (u32)(cursor - outBuf), 66,
                                         blockedUsers, blockedUserNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomAttr != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 109,
                                (u16)roomAttr->len, roomAttr->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (natByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 28, 1, &natByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 19, 1, &teamByte);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (memberBinAttr != NULL) {
        step = npProtoEncodeMemberBinAttr(cursor, outBufSize - (u32)(cursor - outBuf), memberBinAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (memberExtraAttr != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 110,
                                (u16)memberExtraAttr->len, memberExtraAttr->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (joinGroupLabel != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 72, 8, joinGroupLabel);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&routeWord, 0, 4);
    routeWord = sessionId;
    step = npProtoBuildHeader(outBuf, 32, 0x3200, 0, txId, &routeWord, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00d69c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                            const NpMatching2BlobBuf *sessionKey,
                            const NpMatching2BlobBuf *sessionToken,
                            const NpMatching2BlobBuf *keepaliveCookie,
                            const NpMatching2BlobBuf *roomToken,
                            u8 natType, u8 teamId,
                            const SceNpMatching2BinAttr *memberBinAttr,
                            const NpMatching2BlobBuf *memberExtraAttr,
                            const SceNpMatching2SessionPassword *password,
                            const void *joinGroupLabel,
                            const NpMatching2BlobBuf *roomTicket,
                            const NpProtoOptDataBlock *optData)
{
    u8 *cursor;
    s32 step;
    u16 roomHandleVal = roomHandle;
    u8 natByte = natType;
    u8 teamByte = teamId;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 35, (u16)sessionToken->len, sessionToken->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 93,
                            (u16)keepaliveCookie->len, keepaliveCookie->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (roomToken != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 33,
                                (u16)roomToken->len, roomToken->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 62, 2, &roomHandleVal);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (natByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 28, 1, &natByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 19, 1, &teamByte);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (memberBinAttr != NULL) {
        step = npProtoEncodeMemberBinAttr(cursor, outBufSize - (u32)(cursor - outBuf), memberBinAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (memberExtraAttr != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 110,
                                (u16)memberExtraAttr->len, memberExtraAttr->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (password != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 70, 8, password);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (joinGroupLabel != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 72, 8, joinGroupLabel);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 32,
                            (u16)roomTicket->len, roomTicket->bytes);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (optData != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 40,
                                (u16)optData->size, optData->data);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandleVal;
    step = npProtoBuildHeader(outBuf, 32, 0x3201, 0, txId, &route, (u16)(cursor - outBuf),
                              sessionKey->bytes, sessionKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00d99c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSetRoomDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                  SceNpMatching2RoomMemberId memberId,
                                  const NpMatching2BlobBuf *roomKey,
                                  u32 flagFilter, u32 flagAttr,
                                  const SceNpMatching2BinAttr *roomBinAttrs, u32 roomBinAttrNum,
                                  const u64 *passwordSlotMask,
                                  const NpMatching2BlobBuf *roomAttr, u8 ownerPrivFlag,
                                  const SceNpMatching2RoomMemberId *ownerPrivRank, u32 ownerPrivRankNum)
{
    u8 *cursor;
    s32 step;
    u8 privByte = ownerPrivFlag;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (flagFilter != 0) {
        step = npProtoEncodeFlagPair(cursor, outBufSize - 32, flagFilter, 64, flagAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomBinAttrs != NULL && roomBinAttrNum != 0) {
        step = npProtoEncodeRoomBinAttrList(cursor, outBufSize - (u32)(cursor - outBuf),
                                            roomBinAttrs, roomBinAttrNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (passwordSlotMask != NULL) {
        u32 maskWords[2];
        maskWords[0] = npSwap32((u32)(*passwordSlotMask >> 32));
        maskWords[1] = npSwap32((u32)(*passwordSlotMask & 0xFFFFFFFFu));
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 106, 8, maskWords);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (roomAttr != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 109,
                                (u16)roomAttr->len, roomAttr->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (privByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 105, 1, &privByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (ownerPrivRank != NULL && ownerPrivRankNum != 0) {
        step = npProtoEncodeMemberIdList(cursor, outBufSize - (u32)(cursor - outBuf),
                                         ownerPrivRank, ownerPrivRankNum);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3204, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00dc14 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSetRoomMemberDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                        SceNpMatching2RoomMemberId senderMemberId,
                                        const NpMatching2BlobBuf *roomKey,
                                        SceNpMatching2RoomMemberId targetMemberId,
                                        u32 flagFilter, u32 flagAttr, u8 natType, u8 teamId,
                                        const SceNpMatching2BinAttr *memberBinAttr,
                                        const NpMatching2BlobBuf *memberExtraAttr, u8 notifyFlag)
{
    u8 *cursor;
    s32 step;
    u16 targetBe;
    u8 natByte = natType;
    u8 teamByte = teamId;
    u8 notifyByte = notifyFlag;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    targetBe = npSwap16(targetMemberId);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 22, 2, &targetBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (flagFilter != 0) {
        step = npProtoEncodeFlagPair(cursor, outBufSize - (u32)(cursor - outBuf), flagFilter, 73, flagAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (natByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 28, 1, &natByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (teamByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 19, 1, &teamByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (memberBinAttr != NULL) {
        step = npProtoEncodeMemberBinAttr(cursor, outBufSize - (u32)(cursor - outBuf), memberBinAttr);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (memberExtraAttr != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 110,
                                (u16)memberExtraAttr->len, memberExtraAttr->bytes);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (notifyByte != 0) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 105, 1, &notifyByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = senderMemberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3206, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00de6c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetRoomMemberDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                        SceNpMatching2RoomMemberId senderMemberId,
                                        const NpMatching2BlobBuf *roomKey,
                                        SceNpMatching2RoomMemberId targetMemberId,
                                        const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    u8 *cursor;
    s32 step;
    u16 targetBe;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    targetBe = npSwap16(targetMemberId);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 22, 2, &targetBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    step = npProtoEncodeAttrIdList(cursor, outBufSize - (u32)(cursor - outBuf), attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    cursor += step;

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = senderMemberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3207, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00dfac */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSendRoomChatReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                SceNpMatching2RoomMemberId memberId,
                                const NpMatching2BlobBuf *roomKey,
                                const void *castMask, const void *msg, u16 msgLen,
                                const NpProtoMsgOptData *optData)
{
    u8 *cursor;
    s32 step;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (castMask != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - 32, 68, 16, castMask);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 26, msgLen, msg);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (optData != NULL) {
        step = npProtoEncodeMsgOptData(cursor, outBufSize - (u32)(cursor - outBuf), optData);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3208, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00e114 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomChatNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *srcMemberIdOut,
                               void *msgIdOut, s32 *hasMsgIdOut, void **msgOut, u32 *msgLenOut,
                               NpProtoMsgOptData *optDataOut, void *filteredOut)
{
    NpProtoChatNotifyCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *hasMsgIdOut = 0;
    *msgOut = NULL;
    *msgLenOut = 0;
    ctx.srcMemberIdOut = srcMemberIdOut;
    ctx.msgIdOut = msgIdOut;
    ctx.hasMsgIdOut = hasMsgIdOut;
    ctx.msgOut = msgOut;
    ctx.msgLenOut = msgLenOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));
    ctx.filteredOut = filteredOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomChatNotify, 5, npProtoOnRoomChatNotifyTlv, &ctx);
    if (ret < 0) {
        if (*msgOut != NULL) {
            npMatching2Free(*msgOut);
        }
        *msgOut = NULL;
        *msgLenOut = 0;
        if (optDataOut->onlineName != NULL) {
            npMatching2Free(optDataOut->onlineName);
        }
        optDataOut->onlineName = NULL;
        if (optDataOut->avatarUrl != NULL) {
            npMatching2Free(optDataOut->avatarUrl);
        }
        optDataOut->avatarUrl = NULL;
        return ret;
    }
    return 0;
}

/* 0x00e270 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildSendRoomMsgReq(u8 *outBuf, u32 outBufSize, u8 hdrFlags, u32 txId, u16 roomHandle,
                               SceNpMatching2RoomMemberId memberId,
                               const NpMatching2BlobBuf *roomKey,
                               const void *castMask, const void *msg, u16 msgLen,
                               const NpProtoMsgOptData *optData)
{
    u8 *cursor;
    s32 step;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (castMask != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - 32, 68, 16, castMask);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 27, msgLen, msg);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (optData != NULL) {
        step = npProtoEncodeMsgOptData(cursor, outBufSize - (u32)(cursor - outBuf), optData);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x320A, hdrFlags, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00e3d8 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomMsgNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *srcMemberIdOut,
                              void *msgIdOut, s32 *hasMsgIdOut, void **msgOut, u32 *msgLenOut,
                              NpProtoMsgOptData *optDataOut)
{
    NpProtoMsgNotifyCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *hasMsgIdOut = 0;
    *msgOut = NULL;
    *msgLenOut = 0;
    ctx.srcMemberIdOut = srcMemberIdOut;
    ctx.msgIdOut = msgIdOut;
    ctx.hasMsgIdOut = hasMsgIdOut;
    ctx.msgOut = msgOut;
    ctx.msgLenOut = msgLenOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomMsgNotify, 4, npProtoOnRoomMsgNotifyTlv, &ctx);
    if (ret < 0) {
        if (*msgOut != NULL) {
            npMatching2Free(*msgOut);
        }
        *msgOut = NULL;
        *msgLenOut = 0;
        if (optDataOut->onlineName != NULL) {
            npMatching2Free(optDataOut->onlineName);
        }
        optDataOut->onlineName = NULL;
        if (optDataOut->avatarUrl != NULL) {
            npMatching2Free(optDataOut->avatarUrl);
        }
        optDataOut->avatarUrl = NULL;
        return ret;
    }
    return 0;
}

/* 0x00e52c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGrantRoomOwnerReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                  SceNpMatching2RoomMemberId memberId,
                                  const NpMatching2BlobBuf *roomKey,
                                  SceNpMatching2RoomMemberId newOwnerId,
                                  const NpProtoOptDataBlock *optData)
{
    u8 *cursor;
    s32 step;
    u16 ownerBe;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    ownerBe = npSwap16(newOwnerId);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 22, 2, &ownerBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (optData != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 40,
                                (u16)optData->size, optData->data);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3210, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00e66c */
NP_MATCHING2_NOINLINE
s32 npProtoBuildKickoutRoomMemberReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                     SceNpMatching2RoomMemberId memberId,
                                     const NpMatching2BlobBuf *roomKey,
                                     SceNpMatching2RoomMemberId targetMemberId,
                                     u8 blockFlag, s32 hasBlockFlag,
                                     const NpProtoOptDataBlock *optData)
{
    u8 *cursor;
    s32 step;
    u16 targetBe;
    u8 blockByte = blockFlag;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    targetBe = npSwap16(targetMemberId);
    step = npProtoEncodeTlv(cursor, outBufSize - 32, 22, 2, &targetBe);
    if (step < 0) {
        return step;
    }
    cursor += step;

    if (hasBlockFlag == 1) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 23, 1, &blockByte);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    if (optData != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - (u32)(cursor - outBuf), 40,
                                (u16)optData->size, optData->data);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3211, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00e7ec */
NP_MATCHING2_NOINLINE
s32 npProtoParseCreateJoinRoomResp(const u8 *buf, u32 bufLen,
                                   NpMatching2BlobBuf *roomKeyOut,
                                   NpMatching2BlobBuf *roomCookieOut,
                                   NpMatching2BlobBuf *roomTokenOut,
                                   NpProtoRoomDataInternalWire *roomDataOut,
                                   NpProtoRoomMemberInternalWire *memberDataOut)
{
    NpProtoCreateJoinRespCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    roomKeyOut->len = 0;
    roomCookieOut->len = 0;
    roomTokenOut->len = 0;
    ctx.roomKeyOut = roomKeyOut;
    ctx.roomCookieOut = roomCookieOut;
    ctx.roomTokenOut = roomTokenOut;
    ctx.roomDataOut = roomDataOut;
    sceKernelMemset(roomDataOut, 0, sizeof(*roomDataOut));
    ctx.memberDataOut = memberDataOut;
    sceKernelMemset(memberDataOut, 0, sizeof(*memberDataOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specCreateJoinRoomResp, 5,
                                npProtoOnCreateJoinRoomRespTlv, &ctx);
    if (ret < 0) {
        u32 idx;
        for (idx = 0; idx < 2; idx++) {
            if (roomDataOut->binAttrInternal[idx].attr.ptr != NULL) {
                npMatching2Free(roomDataOut->binAttrInternal[idx].attr.ptr);
            }
            roomDataOut->binAttrInternal[idx].attr.ptr = NULL;
            roomDataOut->binAttrInternal[idx].updateDate = 0;
            roomDataOut->binAttrInternal[idx].attr.size = 0;
            roomDataOut->binAttrInternal[idx].updateMemberId = 0;
        }
        if (roomDataOut->groupList != NULL) {
            npMatching2Free(roomDataOut->groupList);
        }
        roomDataOut->groupList = NULL;
        if (memberDataOut->onlineName != NULL) {
            npMatching2Free(memberDataOut->onlineName);
        }
        memberDataOut->onlineName = NULL;
        if (memberDataOut->avatarUrl != NULL) {
            npMatching2Free(memberDataOut->avatarUrl);
        }
        memberDataOut->avatarUrl = NULL;
        if (memberDataOut->binAttrInternal.attr.ptr != NULL) {
            npMatching2Free(memberDataOut->binAttrInternal.attr.ptr);
        }
        memberDataOut->binAttrInternal.attr.ptr = NULL;
        memberDataOut->binAttrInternal.updateDate = 0;
        memberDataOut->binAttrInternal.attr.size = 0;
        return ret;
    }
    return 0;
}

/* 0x00e9c0 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomBinAttrIntArray(SceNpMatching2BinAttrInternal *attrs, u16 count)
{
    u32 idx;
    for (idx = 0; idx < count; idx++) {
        if (attrs[idx].attr.ptr != NULL) {
            npMatching2Free(attrs[idx].attr.ptr);
        }
        attrs[idx].attr.ptr = NULL;
        attrs[idx].updateDate = 0;
        attrs[idx].attr.size = 0;
        attrs[idx].updateMemberId = 0;
    }
}

/* 0x00ea44 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomDataInt(NpProtoRoomDataInternalWire *roomData)
{
    u32 idx;
    for (idx = 0; idx < 2; idx++) {
        if (roomData->binAttrInternal[idx].attr.ptr != NULL) {
            npMatching2Free(roomData->binAttrInternal[idx].attr.ptr);
        }
        roomData->binAttrInternal[idx].attr.ptr = NULL;
        roomData->binAttrInternal[idx].updateDate = 0;
        roomData->binAttrInternal[idx].attr.size = 0;
        roomData->binAttrInternal[idx].updateMemberId = 0;
    }
    if (roomData->groupList != NULL) {
        npMatching2Free(roomData->groupList);
    }
    roomData->groupList = NULL;
}

/* 0x00eae4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseJoinRoomResp(const u8 *buf, u32 bufLen,
                             SceNpMatching2RoomMemberId *meMemberIdOut,
                             NpMatching2BlobBuf *roomKeyOut,
                             NpMatching2BlobBuf *roomCookieOut,
                             NpMatching2BlobBuf *roomTokenOut,
                             NpProtoRoomDataInternalWire *roomDataOut,
                             u16 *memberCountOut,
                             NpProtoRoomMemberInternalWire **memberListOut)
{
    NpProtoJoinRoomRespCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    roomKeyOut->len = 0;
    roomCookieOut->len = 0;
    roomTokenOut->len = 0;
    ctx.meMemberIdOut = meMemberIdOut;
    ctx.roomKeyOut = roomKeyOut;
    ctx.roomCookieOut = roomCookieOut;
    ctx.roomTokenOut = roomTokenOut;
    ctx.roomDataOut = roomDataOut;
    sceKernelMemset(roomDataOut, 0, sizeof(*roomDataOut));
    *memberCountOut = 0;
    *memberListOut = NULL;
    ctx.memberCountOut = memberCountOut;
    ctx.memberListOut = memberListOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specJoinRoomResp, 6, npProtoOnJoinRoomRespTlv, &ctx);
    if (ret < 0) {
        u32 idx;
        for (idx = 0; idx < 2; idx++) {
            if (roomDataOut->binAttrInternal[idx].attr.ptr != NULL) {
                npMatching2Free(roomDataOut->binAttrInternal[idx].attr.ptr);
            }
            roomDataOut->binAttrInternal[idx].attr.ptr = NULL;
            roomDataOut->binAttrInternal[idx].updateDate = 0;
            roomDataOut->binAttrInternal[idx].attr.size = 0;
            roomDataOut->binAttrInternal[idx].updateMemberId = 0;
        }
        if (roomDataOut->groupList != NULL) {
            npMatching2Free(roomDataOut->groupList);
        }
        roomDataOut->groupList = NULL;
        if (*memberListOut != NULL) {
            NpProtoRoomMemberInternalWire *list = *memberListOut;
            for (idx = 0; idx < *memberCountOut; idx++) {
                if (list[idx].onlineName != NULL) {
                    npMatching2Free(list[idx].onlineName);
                }
                list[idx].onlineName = NULL;
                if (list[idx].avatarUrl != NULL) {
                    npMatching2Free(list[idx].avatarUrl);
                }
                list[idx].avatarUrl = NULL;
                if (list[idx].binAttrInternal.attr.ptr != NULL) {
                    npMatching2Free(list[idx].binAttrInternal.attr.ptr);
                }
                list[idx].binAttrInternal.attr.ptr = NULL;
                list[idx].binAttrInternal.updateDate = 0;
                list[idx].binAttrInternal.attr.size = 0;
            }
            npMatching2Free(list);
        }
        *memberListOut = NULL;
        *memberCountOut = 0;
        return ret;
    }
    return 0;
}

/* 0x00ed18 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomMemberIntList(NpProtoRoomMemberInternalWire *list, u16 count)
{
    if (list != NULL) {
        u32 idx;
        for (idx = 0; idx < count; idx++) {
            if (list[idx].onlineName != NULL) {
                npMatching2Free(list[idx].onlineName);
            }
            list[idx].onlineName = NULL;
            if (list[idx].avatarUrl != NULL) {
                npMatching2Free(list[idx].avatarUrl);
            }
            list[idx].avatarUrl = NULL;
            if (list[idx].binAttrInternal.attr.ptr != NULL) {
                npMatching2Free(list[idx].binAttrInternal.attr.ptr);
            }
            list[idx].binAttrInternal.attr.ptr = NULL;
            list[idx].binAttrInternal.updateDate = 0;
            list[idx].binAttrInternal.attr.size = 0;
        }
        npMatching2Free(list);
    }
}

/* 0x00ee08 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildLeaveRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                             SceNpMatching2RoomMemberId memberId,
                             const NpMatching2BlobBuf *roomKey,
                             const NpProtoOptDataBlock *optData)
{
    u8 *cursor;
    s32 step;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }

    cursor = outBuf + 32;
    if (optData != NULL) {
        step = npProtoEncodeTlv(cursor, outBufSize - 32, 40, (u16)optData->size, optData->data);
        if (step < 0) {
            return step;
        }
        cursor += step;
    }

    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3202, 0, txId, &route, (u16)(cursor - outBuf),
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)(cursor - outBuf);
    }
    return step;
}

/* 0x00eefc */
NP_MATCHING2_NOINLINE
s32 npProtoParseLeaveRoomResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut)
{
    NpMatching2BlobBuf *ctxPtr;
    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    cookieOut->len = 0;
    ctxPtr = cookieOut;
    return npProtoParseTlvStream(buf, bufLen, g_specLeaveRoomResp, 1, npProtoOnLeaveRoomRespTlv, &ctxPtr);
}

/* 0x00ef70 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetRoomMemberIntListReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                        SceNpMatching2RoomMemberId memberId,
                                        const NpMatching2BlobBuf *roomKey,
                                        const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    s32 step;
    u32 totalLen;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeAttrIdList(outBuf + 32, outBufSize - 32, attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3203, 0, txId, &route, (u16)totalLen,
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00f064 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetRoomMemberIntListResp(const u8 *buf, u32 bufLen, u16 *countOut,
                                         NpProtoRoomMemberInternalWire **listOut)
{
    NpProtoMemberIntListCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *countOut = 0;
    *listOut = NULL;
    ctx.countOut = countOut;
    ctx.listOut = listOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomMemberIntList, 1,
                                npProtoOnRoomMemberIntListTlv, &ctx);
    if (ret < 0) {
        if (*listOut != NULL) {
            npProtoFreeRoomMemberIntList(*listOut, *countOut);
        }
        *countOut = 0;
        *listOut = NULL;
        return ret;
    }
    return 0;
}

/* 0x00f124 */
NP_MATCHING2_NOINLINE
s32 npProtoBuildGetRoomDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle,
                                  SceNpMatching2RoomMemberId memberId,
                                  const NpMatching2BlobBuf *roomKey,
                                  const SceNpMatching2AttributeId *attrIds, u32 attrIdNum)
{
    s32 step;
    u32 totalLen;
    NpProtoRoutePair route;

    if (outBufSize < 32) {
        return SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL;
    }
    step = npProtoEncodeAttrIdList(outBuf + 32, outBufSize - 32, attrIds, attrIdNum);
    if (step < 0) {
        return step;
    }
    totalLen = (u32)(32 + step);
    sceKernelMemset(&route, 0, sizeof(route));
    route.firstWord = roomHandle;
    route.secondWord = memberId;
    step = npProtoBuildHeader(outBuf, 32, 0x3205, 0, txId, &route, (u16)totalLen,
                              roomKey->bytes, roomKey->len);
    if (step >= 0) {
        return (s32)totalLen;
    }
    return step;
}

/* 0x00f218 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetRoomDataIntResp(const u8 *buf, u32 bufLen, NpProtoRoomDataInternalWire *roomDataOut)
{
    NpProtoRoomDataInternalWire *ctxPtr;
    s32 ret;

    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    ctxPtr = roomDataOut;
    sceKernelMemset(roomDataOut, 0, sizeof(*roomDataOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specGetRoomDataIntResp, 1,
                                npProtoOnGetRoomDataIntTlv, &ctxPtr);
    if (ret < 0) {
        u32 idx;
        for (idx = 0; idx < 2; idx++) {
            if (roomDataOut->binAttrInternal[idx].attr.ptr != NULL) {
                npMatching2Free(roomDataOut->binAttrInternal[idx].attr.ptr);
            }
            roomDataOut->binAttrInternal[idx].attr.ptr = NULL;
            roomDataOut->binAttrInternal[idx].updateDate = 0;
            roomDataOut->binAttrInternal[idx].attr.size = 0;
            roomDataOut->binAttrInternal[idx].updateMemberId = 0;
        }
        if (roomDataOut->groupList != NULL) {
            npMatching2Free(roomDataOut->groupList);
        }
        roomDataOut->groupList = NULL;
        return ret;
    }
    return 0;
}

/* 0x00f324 */
NP_MATCHING2_NOINLINE
s32 npProtoParseSetRoomDataIntResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut,
                                   u64 *updateDateOut)
{
    NpProtoSetRoomDataIntCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    cookieOut->len = 0;
    ctx.cookieOut = cookieOut;
    ctx.updateDateOut = updateDateOut;
    return npProtoParseTlvStream(buf, bufLen, g_specSetRoomDataIntResp, 2,
                                 npProtoOnSetRoomDataIntTlv, &ctx);
}

/* 0x00f3a8 */
NP_MATCHING2_NOINLINE
s32 npProtoParseSetRoomMemberDataIntResp(const u8 *buf, u32 bufLen, u64 *updateDateOut)
{
    u64 *ctxPtr;
    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    ctxPtr = updateDateOut;
    return npProtoParseTlvStream(buf, bufLen, g_specSetRoomMemberDataIntResp, 1,
                                 npProtoOnSetRoomMemberDataIntTlv, &ctxPtr);
}

/* 0x00f418 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGetRoomMemberDataIntResp(const u8 *buf, u32 bufLen,
                                         NpProtoRoomMemberInternalWire *memberDataOut)
{
    NpProtoRoomMemberInternalWire *ctxPtr;
    s32 ret;

    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    ctxPtr = memberDataOut;
    sceKernelMemset(memberDataOut, 0, sizeof(*memberDataOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specGetRoomMemberDataIntResp, 1,
                                npProtoOnGetRoomMemberDataIntTlv, &ctxPtr);
    if (ret < 0) {
        if (memberDataOut->onlineName != NULL) {
            npMatching2Free(memberDataOut->onlineName);
        }
        memberDataOut->onlineName = NULL;
        if (memberDataOut->avatarUrl != NULL) {
            npMatching2Free(memberDataOut->avatarUrl);
        }
        memberDataOut->avatarUrl = NULL;
        if (memberDataOut->binAttrInternal.attr.ptr != NULL) {
            npMatching2Free(memberDataOut->binAttrInternal.attr.ptr);
        }
        memberDataOut->binAttrInternal.attr.ptr = NULL;
        memberDataOut->binAttrInternal.updateDate = 0;
        memberDataOut->binAttrInternal.attr.size = 0;
        return ret;
    }
    return 0;
}

/* 0x00f524 */
NP_MATCHING2_NOINLINE
void npProtoFreeRoomMemberInt(NpProtoRoomMemberInternalWire *memberData)
{
    if (memberData->onlineName != NULL) {
        npMatching2Free(memberData->onlineName);
    }
    memberData->onlineName = NULL;
    if (memberData->avatarUrl != NULL) {
        npMatching2Free(memberData->avatarUrl);
    }
    memberData->avatarUrl = NULL;
    if (memberData->binAttrInternal.attr.ptr != NULL) {
        npMatching2Free(memberData->binAttrInternal.attr.ptr);
    }
    memberData->binAttrInternal.attr.ptr = NULL;
    memberData->binAttrInternal.updateDate = 0;
    memberData->binAttrInternal.attr.size = 0;
}

/* 0x00f5cc */
NP_MATCHING2_NOINLINE
s32 npProtoParseSendRoomChatResp(const u8 *buf, u32 bufLen, void *msgIdOut, void *filteredOut)
{
    NpProtoChatRespCtx ctx;
    s32 ret;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.msgIdOut = msgIdOut;
    ctx.filteredOut = filteredOut;
    ret = npProtoParseTlvStream(buf, bufLen, g_specSendRoomChatResp, 2,
                                npProtoOnSendRoomChatRespTlv, &ctx);
    return (ret < 0) ? ret : 0;
}

/* 0x00f654 */
NP_MATCHING2_NOINLINE
s32 npProtoParseSendRoomMsgResp(const u8 *buf, u32 bufLen, void *msgIdOut)
{
    void *ctxPtr;
    s32 ret;
    sceKernelMemset(&ctxPtr, 0, sizeof(ctxPtr));
    ctxPtr = msgIdOut;
    ret = npProtoParseTlvStream(buf, bufLen, g_specSendRoomMsgResp, 1,
                                npProtoOnSendRoomMsgRespTlv, &ctxPtr);
    return (ret < 0) ? ret : 0;
}

/* 0x00f6cc */
NP_MATCHING2_NOINLINE
s32 npProtoParseMemberJoinedNotify(const u8 *buf, u32 bufLen,
                                   NpProtoRoomMemberInternalWire *memberOut,
                                   NpMatching2BlobBuf *cookieOut,
                                   NpProtoGroupConfigWire *groupOut, s32 *hasGroupOut,
                                   NpProtoOptDataBlock *optDataOut)
{
    NpProtoMemberJoinedCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    ctx.memberOut = memberOut;
    sceKernelMemset(memberOut, 0, sizeof(*memberOut));
    cookieOut->len = 0;
    *hasGroupOut = 0;
    ctx.cookieOut = cookieOut;
    ctx.groupOut = groupOut;
    ctx.hasGroupOut = hasGroupOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));

    ret = npProtoParseTlvStream(buf, bufLen, g_specMemberJoinedNotify, 4,
                                npProtoOnMemberJoinedNotifyTlv, &ctx);
    if (ret < 0) {
        if (memberOut->onlineName != NULL) {
            npMatching2Free(memberOut->onlineName);
        }
        memberOut->onlineName = NULL;
        if (memberOut->avatarUrl != NULL) {
            npMatching2Free(memberOut->avatarUrl);
        }
        memberOut->avatarUrl = NULL;
        if (memberOut->binAttrInternal.attr.ptr != NULL) {
            npMatching2Free(memberOut->binAttrInternal.attr.ptr);
        }
        memberOut->binAttrInternal.attr.ptr = NULL;
        memberOut->binAttrInternal.updateDate = 0;
        memberOut->binAttrInternal.attr.size = 0;
        return ret;
    }
    return 0;
}

/* 0x00f830 */
NP_MATCHING2_NOINLINE
s32 npProtoParseMemberUpdateNotify(const u8 *buf, u32 bufLen,
                                   SceNpMatching2RoomMemberId *memberIdOut,
                                   u32 *flagFilterOut, u32 *flagAttrOut,
                                   u8 *natTypeOut, u8 *teamIdOut,
                                   SceNpMatching2MemberBinAttrInternal *binAttrOut,
                                   NpMatching2BlobBuf *extraAttrOut)
{
    NpProtoMemberUpdateCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    *flagFilterOut = 0;
    *natTypeOut = 0;
    *teamIdOut = 0;
    ctx.memberIdOut = memberIdOut;
    ctx.flagFilterOut = flagFilterOut;
    ctx.flagAttrOut = flagAttrOut;
    ctx.natTypeOut = natTypeOut;
    ctx.teamIdOut = teamIdOut;
    ctx.binAttrOut = binAttrOut;
    sceKernelMemset(binAttrOut, 0, sizeof(*binAttrOut));
    extraAttrOut->len = 0;
    ctx.extraAttrOut = extraAttrOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specMemberUpdateNotify, 6,
                                npProtoOnMemberUpdateNotifyTlv, &ctx);
    if (ret < 0) {
        if (binAttrOut != NULL) {
            if (binAttrOut->attr.ptr != NULL) {
                npMatching2Free(binAttrOut->attr.ptr);
            }
            binAttrOut->attr.ptr = NULL;
            binAttrOut->updateDate = 0;
            binAttrOut->attr.size = 0;
        }
        return ret;
    }
    return 0;
}

/* 0x00f970 */
NP_MATCHING2_NOINLINE
void npProtoFreeMemberBinAttrIntArray(SceNpMatching2MemberBinAttrInternal *attrs, u32 count)
{
    if (attrs != NULL) {
        u32 idx;
        for (idx = 0; idx < count; idx++) {
            if (attrs[idx].attr.ptr != NULL) {
                npMatching2Free(attrs[idx].attr.ptr);
            }
            attrs[idx].attr.ptr = NULL;
            attrs[idx].updateDate = 0;
            attrs[idx].attr.size = 0;
        }
    }
}

/* 0x00f9f4 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomDataUpdateNotify(const u8 *buf, u32 bufLen,
                                     NpMatching2BlobBuf *cookieOut,
                                     u32 *flagFilterOut, u32 *flagAttrOut,
                                     u16 maxBinAttrs, SceNpMatching2BinAttrInternal *binAttrOut,
                                     NpMatching2BlobBuf *roomAttrOut,
                                     u64 *passwordSlotMaskOut, s32 *hasPasswordMaskOut)
{
    NpProtoRoomDataUpdateCtx ctx;
    s32 ret;

    sceKernelMemset(&ctx, 0, sizeof(ctx));
    cookieOut->len = 0;
    *flagFilterOut = 0;
    ctx.cookieOut = cookieOut;
    ctx.flagFilterOut = flagFilterOut;
    ctx.flagAttrOut = flagAttrOut;
    ctx.maxBinAttrs = maxBinAttrs;
    ctx.binAttrOut = binAttrOut;
    sceKernelMemset(binAttrOut, 0, sizeof(*binAttrOut) * maxBinAttrs);
    roomAttrOut->len = 0;
    *hasPasswordMaskOut = 0;
    ctx.roomAttrOut = roomAttrOut;
    ctx.passwordSlotMaskOut = passwordSlotMaskOut;
    ctx.hasPasswordMaskOut = hasPasswordMaskOut;

    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomDataUpdateNotify, 6,
                                npProtoOnRoomDataUpdateNotifyTlv, &ctx);
    if (ret < 0) {
        u32 idx;
        for (idx = 0; idx < maxBinAttrs; idx++) {
            if (binAttrOut[idx].attr.ptr != NULL) {
                npMatching2Free(binAttrOut[idx].attr.ptr);
            }
            binAttrOut[idx].attr.ptr = NULL;
            binAttrOut[idx].updateDate = 0;
            binAttrOut[idx].attr.size = 0;
            binAttrOut[idx].updateMemberId = 0;
        }
        return ret;
    }
    return 0;
}

/* 0x00fb5c */
NP_MATCHING2_NOINLINE
s32 npProtoParseMemberLeftNotify(const u8 *buf, u32 bufLen,
                                 SceNpMatching2RoomMemberId *memberIdOut, SceNpId *npIdOut,
                                 u8 *reasonOut, NpMatching2BlobBuf *cookieOut,
                                 NpProtoOptDataBlock *optDataOut,
                                 NpProtoGroupConfigWire *groupOut, s32 *hasGroupOut)
{
    NpProtoMemberLeftCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    cookieOut->len = 0;
    ctx.memberIdOut = memberIdOut;
    ctx.npIdOut = npIdOut;
    ctx.reasonOut = reasonOut;
    ctx.cookieOut = cookieOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));
    *hasGroupOut = 0;
    ctx.groupOut = groupOut;
    ctx.hasGroupOut = hasGroupOut;
    return npProtoParseTlvStream(buf, bufLen, g_specMemberLeftNotify, 6,
                                 npProtoOnMemberLeftNotifyTlv, &ctx);
}

/* 0x00fc44 */
NP_MATCHING2_NOINLINE
s32 npProtoParseGrantRoomOwnerResp(void)
{
    return 0;
}

/* 0x00fc4c */
NP_MATCHING2_NOINLINE
s32 npProtoParseKickoutRoomMemberResp(void)
{
    return 0;
}

/* 0x00fc54 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomOwnerChangedNotify(const u8 *buf, u32 bufLen,
                                       SceNpMatching2RoomMemberId *newOwnerIdOut, SceNpId *npIdOut,
                                       u8 *reasonOut, NpMatching2BlobBuf *cookieOut,
                                       SceNpMatching2SessionPassword *passwordOut, s32 *hasPasswordOut,
                                       NpProtoOptDataBlock *optDataOut)
{
    NpProtoOwnerChangedCtx ctx;
    s32 ret;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    cookieOut->len = 0;
    *hasPasswordOut = 0;
    ctx.newOwnerIdOut = newOwnerIdOut;
    ctx.npIdOut = npIdOut;
    ctx.reasonOut = reasonOut;
    ctx.cookieOut = cookieOut;
    ctx.passwordOut = passwordOut;
    ctx.hasPasswordOut = hasPasswordOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));
    ret = npProtoParseTlvStream(buf, bufLen, g_specRoomOwnerChangedNotify, 6,
                                npProtoOnRoomOwnerChangedNotifyTlv, &ctx);
    return (ret < 0) ? ret : 0;
}

/* 0x00fd44 */
NP_MATCHING2_NOINLINE
s32 npProtoParseRoomDestroyedNotify(const u8 *buf, u32 bufLen, u8 *reasonOut,
                                    NpMatching2BlobBuf *cookieOut, NpProtoOptDataBlock *optDataOut)
{
    NpProtoRoomDestroyedCtx ctx;
    sceKernelMemset(&ctx, 0, sizeof(ctx));
    cookieOut->len = 0;
    ctx.reasonOut = reasonOut;
    ctx.cookieOut = cookieOut;
    ctx.optDataOut = optDataOut;
    sceKernelMemset(optDataOut, 0, sizeof(*optDataOut));
    return npProtoParseTlvStream(buf, bufLen, g_specRoomDestroyedNotify, 3,
                                 npProtoOnRoomDestroyedNotifyTlv, &ctx);
}

/* 0x00fde8 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnCreateJoinRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoCreateJoinRespCtx *cjCtx = (NpProtoCreateJoinRespCtx *)ctx;
    u16 tag = spec->tag;

    if (tag == 75) {
        NpProtoRoomMemberInternalWire *member = cjCtx->memberDataOut;
        const u8 *cursor = val + 2;
        s32 step;
        sceKernelMemcpy(&member->memberId, val, 2);
        member->memberId = npSwap16(member->memberId);
        step = npProtoDecodeTimestamp(cursor, (u32)(len - 2), &member->joinDate);
        if (step < 0) {
            return step;
        }
        cursor += step;
        sceKernelMemcpy(&member->flagAttr, cursor, 4);
        member->flagAttr = npSwap32(member->flagAttr);
        cursor += 4;
        sceKernelMemcpy(&member->natType, cursor, 1);
        sceKernelMemcpy(&member->teamId, cursor + 1, 1);
        cursor += 2;
        step = npProtoParseTlvStream(cursor, (u32)(len - (u32)(cursor - val)),
                                     g_specRoomMemberIntReq, 5,
                                     npProtoOnRoomMemberIntFieldTlv, member);
        if (step < 0) {
            return step;
        }
        return (s32)((cursor - val) + step);
    }

    if (tag == 113) {
        NpProtoRoomDataInternalWire *room = cjCtx->roomDataOut;
        u32 rawWords[2];
        s32 ret;
        sceKernelMemcpy(&room->roomHandle, val, 2);
        sceKernelMemcpy(&room->flagAttr, val + 2, 4);
        room->flagAttr = npSwap32(room->flagAttr);
        sceKernelMemcpy(&room->maxSlot, val + 6, 2);
        room->maxSlot = npSwap16(room->maxSlot);
        sceKernelMemcpy(room->roomMeta, val + 8, 16);
        sceKernelMemcpy(rawWords, val + 24, 8);
        room->passwordSlotMask = ((u64)npSwap32(rawWords[0]) << 32) | (u64)npSwap32(rawWords[1]);
        ret = npProtoParseTlvStream(val + 32, (u32)(len - 32), g_specRoomDataIntReq, 5,
                                    npProtoOnRoomDataIntFieldTlv, room);
        if (ret >= 0) {
            ret += 32;
        }
        return ret;
    }

    if (tag == 36) {
        sceKernelMemcpy(cjCtx->roomKeyOut->bytes, val, len);
        cjCtx->roomKeyOut->len = len;
        return 0;
    }
    if (tag == 33) {
        sceKernelMemcpy(cjCtx->roomTokenOut->bytes, val, len);
        cjCtx->roomTokenOut->len = len;
        return 0;
    }
    if (tag == 58) {
        sceKernelMemcpy(cjCtx->roomCookieOut->bytes, val, len);
        cjCtx->roomCookieOut->len = len;
        return 0;
    }
    return 0;
}

/* 0x01006c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnJoinRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoJoinRoomRespCtx *joinCtx = (NpProtoJoinRoomRespCtx *)ctx;
    u16 tag = spec->tag;
    s32 requireAll = 1;

    if (tag == 33) {
        sceKernelMemcpy(joinCtx->roomTokenOut->bytes, val, len);
        joinCtx->roomTokenOut->len = len;
        return 0;
    }
    if (tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 75) {
            s32 parsed = npProtoParseTlvArray(75, val + 4, (u16)(len - 4), innerCount,
                                              (void **)joinCtx->memberListOut,
                                              (u32)innerCount * sizeof(NpProtoRoomMemberInternalWire),
                                              npProtoOnRoomMemberIntItem, &requireAll);
            if (parsed >= 0) {
                *joinCtx->memberCountOut = (u16)parsed;
            }
            return parsed;
        }
        return 0;
    }
    if (tag == 22) {
        sceKernelMemcpy(joinCtx->meMemberIdOut, val, len);
        *joinCtx->meMemberIdOut = npSwap16(*joinCtx->meMemberIdOut);
        return 0;
    }
    if (tag == 58) {
        sceKernelMemcpy(joinCtx->roomCookieOut->bytes, val, len);
        joinCtx->roomCookieOut->len = len;
        return 0;
    }
    if (tag == 36) {
        sceKernelMemcpy(joinCtx->roomKeyOut->bytes, val, len);
        joinCtx->roomKeyOut->len = len;
        return 0;
    }
    if (tag == 113) {
        NpProtoRoomDataInternalWire *room = joinCtx->roomDataOut;
        u32 rawWords[2];
        s32 ret;
        sceKernelMemcpy(&room->roomHandle, val, 2);
        sceKernelMemcpy(&room->flagAttr, val + 2, 4);
        room->flagAttr = npSwap32(room->flagAttr);
        sceKernelMemcpy(&room->maxSlot, val + 6, 2);
        room->maxSlot = npSwap16(room->maxSlot);
        sceKernelMemcpy(room->roomMeta, val + 8, 16);
        sceKernelMemcpy(rawWords, val + 24, 8);
        room->passwordSlotMask = ((u64)npSwap32(rawWords[0]) << 32) | (u64)npSwap32(rawWords[1]);
        ret = npProtoParseTlvStream(val + 32, (u32)(len - 32), g_specRoomDataIntReq, 5,
                                    npProtoOnRoomDataIntFieldTlv, room);
        if (ret >= 0) {
            ret += 32;
        }
        return ret;
    }
    return 0;
}

/* 0x010308 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnLeaveRoomRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpMatching2BlobBuf **cookiePtr = (NpMatching2BlobBuf **)ctx;
    if (spec->tag == 58) {
        sceKernelMemcpy((*cookiePtr)->bytes, val, len);
        (*cookiePtr)->len = len;
    }
    return 0;
}

/* 0x010360 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberIntListTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMemberIntListCtx *listCtx = (NpProtoMemberIntListCtx *)ctx;
    s32 requireAll = 0;

    if (spec->tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 75) {
            s32 parsed = npProtoParseTlvArray(75, val + 4, (u16)(len - 4), innerCount,
                                              (void **)listCtx->listOut,
                                              (u32)innerCount * sizeof(NpProtoRoomMemberInternalWire),
                                              npProtoOnRoomMemberIntItem, &requireAll);
            if (parsed >= 0) {
                *listCtx->countOut = (u16)parsed;
            }
            return parsed;
        }
    }
    return 0;
}

/* 0x010458 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnGetRoomDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDataInternalWire **roomPtr = (NpProtoRoomDataInternalWire **)ctx;
    if (spec->tag == 113) {
        return npProtoDecodeRoomDataInt(val, len, *roomPtr, 0);
    }
    return 0;
}

/* 0x0104a4 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnSetRoomDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoSetRoomDataIntCtx *setCtx = (NpProtoSetRoomDataIntCtx *)ctx;
    if (spec->tag == 58) {
        sceKernelMemcpy(setCtx->cookieOut->bytes, val, len);
        setCtx->cookieOut->len = len;
        return 0;
    }
    if (spec->tag == 112) {
        return npProtoDecodeTimestamp(val, len, setCtx->updateDateOut);
    }
    return 0;
}

/* 0x010528 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnSetRoomMemberDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    u64 **datePtr = (u64 **)ctx;
    if (spec->tag == 112) {
        return npProtoDecodeTimestamp(val, len, *datePtr);
    }
    return 0;
}

/* 0x01056c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnGetRoomMemberDataIntTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomMemberInternalWire **memberPtr = (NpProtoRoomMemberInternalWire **)ctx;
    if (spec->tag == 75) {
        sceKernelMemset(*memberPtr, 0, sizeof(**memberPtr));
        return npProtoDecodeRoomMemberInt(val, len, *memberPtr, 0);
    }
    return 0;
}

/* 0x0105e8 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnSendRoomChatRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoChatRespCtx *respCtx = (NpProtoChatRespCtx *)ctx;
    if (spec->tag == 68) {
        sceKernelMemcpy(respCtx->msgIdOut, val, len);
    } else if (spec->tag == 117) {
        sceKernelMemcpy(respCtx->filteredOut, val, len);
    }
    return 0;
}

/* 0x010638 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomChatNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoChatNotifyCtx *chatCtx = (NpProtoChatNotifyCtx *)ctx;
    if (spec->tag == 68) {
        sceKernelMemcpy(chatCtx->msgIdOut, val, len);
        *chatCtx->hasMsgIdOut = 1;
        return 0;
    }
    if (spec->tag == 22) {
        sceKernelMemcpy(chatCtx->srcMemberIdOut, val, len);
        *chatCtx->srcMemberIdOut = npSwap16(*chatCtx->srcMemberIdOut);
        return 0;
    }
    if (spec->tag == 26) {
        if (len != 0) {
            void *copy = npMatching2Alloc(len);
            *chatCtx->msgOut = copy;
            if (copy == NULL) {
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(copy, val, len);
            *chatCtx->msgLenOut = len;
        } else {
            *chatCtx->msgLenOut = 0;
            *chatCtx->msgOut = NULL;
        }
        return 0;
    }
    if (spec->tag == 100) {
        return npProtoDecodeMsgOptData(val, len, chatCtx->optDataOut);
    }
    if (spec->tag == 117) {
        sceKernelMemcpy(chatCtx->filteredOut, val, len);
    }
    return 0;
}

/* 0x010784 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnSendRoomMsgRespTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    void **msgIdPtr = (void **)ctx;
    if (spec->tag == 68) {
        sceKernelMemcpy(*msgIdPtr, val, len);
    }
    return 0;
}

/* 0x0107bc */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMsgNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMsgNotifyCtx *msgCtx = (NpProtoMsgNotifyCtx *)ctx;
    if (spec->tag == 27) {
        if (len != 0) {
            void *copy = npMatching2Alloc(len);
            *msgCtx->msgOut = copy;
            if (copy == NULL) {
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(copy, val, len);
            *msgCtx->msgLenOut = len;
        } else {
            *msgCtx->msgLenOut = 0;
            *msgCtx->msgOut = NULL;
        }
        return 0;
    }
    if (spec->tag == 22) {
        sceKernelMemcpy(msgCtx->srcMemberIdOut, val, len);
        *msgCtx->srcMemberIdOut = npSwap16(*msgCtx->srcMemberIdOut);
        return 0;
    }
    if (spec->tag == 68) {
        sceKernelMemcpy(msgCtx->msgIdOut, val, len);
        *msgCtx->hasMsgIdOut = 1;
        return 0;
    }
    if (spec->tag == 100) {
        return npProtoDecodeMsgOptData(val, len, msgCtx->optDataOut);
    }
    return 0;
}

/* 0x0108e8 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnMemberJoinedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMemberJoinedCtx *joinedCtx = (NpProtoMemberJoinedCtx *)ctx;
    if (spec->tag == 71) {
        s32 ret = npProtoDecodeGroupConfig(val, len, joinedCtx->groupOut);
        if (ret >= 0) {
            *joinedCtx->hasGroupOut = 1;
        }
        return ret;
    }
    if (spec->tag == 40) {
        sceKernelMemcpy(joinedCtx->optDataOut->data, val, len);
        joinedCtx->optDataOut->size = len;
        return 0;
    }
    if (spec->tag == 58) {
        sceKernelMemcpy(joinedCtx->cookieOut->bytes, val, len);
        joinedCtx->cookieOut->len = len;
        return 0;
    }
    if (spec->tag == 75) {
        NpProtoRoomMemberInternalWire *member = joinedCtx->memberOut;
        const u8 *cursor = val + 2;
        s32 step;
        sceKernelMemset(member, 0, sizeof(*member));
        sceKernelMemcpy(&member->memberId, val, 2);
        member->memberId = npSwap16(member->memberId);
        step = npProtoDecodeTimestamp(cursor, (u32)(len - 2), &member->joinDate);
        if (step < 0) {
            return step;
        }
        cursor += step;
        sceKernelMemcpy(&member->flagAttr, cursor, 4);
        member->flagAttr = npSwap32(member->flagAttr);
        cursor += 4;
        sceKernelMemcpy(&member->natType, cursor, 1);
        sceKernelMemcpy(&member->teamId, cursor + 1, 1);
        cursor += 2;
        step = npProtoParseTlvStream(cursor, (u32)(len - (u32)(cursor - val)),
                                     g_specRoomMemberIntReq, 5,
                                     npProtoOnRoomMemberIntFieldTlv, member);
        if (step >= 0) {
            return (s32)((cursor - val) + step);
        }
        return step;
    }
    return 0;
}

/* 0x010a8c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnMemberUpdateNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMemberUpdateCtx *updCtx = (NpProtoMemberUpdateCtx *)ctx;
    if (spec->tag == 22) {
        sceKernelMemcpy(updCtx->memberIdOut, val, len);
        *updCtx->memberIdOut = npSwap16(*updCtx->memberIdOut);
        return 0;
    }
    if (spec->tag == 6) {
        return npProtoDecodeFlagPair(val, len, 73, updCtx->flagFilterOut, updCtx->flagAttrOut);
    }
    if (spec->tag == 19) {
        sceKernelMemcpy(updCtx->teamIdOut, val, len);
        return 0;
    }
    if (spec->tag == 28) {
        sceKernelMemcpy(updCtx->natTypeOut, val, len);
        return 0;
    }
    if (spec->tag == 89) {
        SceNpMatching2MemberBinAttrInternal *attr = updCtx->binAttrOut;
        if (attr->attr.ptr == NULL) {
            s32 step;
            attr->attr.id = 89;
            step = npProtoDecodeTimestamp(val, len, &attr->updateDate);
            if (step < 0) {
                return step;
            }
            return npProtoDecodeWrappedBlob(val + step, (u16)(len - step),
                                            &attr->attr.ptr, &attr->attr.size);
        }
        return 0;
    }
    if (spec->tag == 110) {
        sceKernelMemcpy(updCtx->extraAttrOut->bytes, val, len);
        updCtx->extraAttrOut->len = len;
    }
    return 0;
}

/* 0x010be8 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDataUpdateNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDataUpdateCtx *updCtx = (NpProtoRoomDataUpdateCtx *)ctx;
    u16 tag = spec->tag;

    if (tag == 87 || tag == 88) {
        return npProtoDecodeRoomBinAttrInt(tag, val, len, updCtx->maxBinAttrs, updCtx->binAttrOut);
    }
    if (tag == 6) {
        return npProtoDecodeFlagPair(val, len, 64, updCtx->flagFilterOut, updCtx->flagAttrOut);
    }
    if (tag == 58) {
        sceKernelMemcpy(updCtx->cookieOut->bytes, val, len);
        updCtx->cookieOut->len = len;
        return 0;
    }
    if (tag == 106) {
        u32 rawWords[2];
        sceKernelMemcpy(rawWords, val, len);
        *updCtx->passwordSlotMaskOut = ((u64)npSwap32(rawWords[0]) << 32) | (u64)npSwap32(rawWords[1]);
        *updCtx->hasPasswordMaskOut = 1;
        return 0;
    }
    if (tag == 109) {
        sceKernelMemcpy(updCtx->roomAttrOut->bytes, val, len);
        updCtx->roomAttrOut->len = len;
    }
    return 0;
}

/* 0x010d24 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnMemberLeftNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoMemberLeftCtx *leftCtx = (NpProtoMemberLeftCtx *)ctx;
    if (spec->tag == 40) {
        sceKernelMemcpy(leftCtx->optDataOut->data, val, len);
        leftCtx->optDataOut->size = len;
        return 0;
    }
    if (spec->tag == 11) {
        return npProtoDecodeNpId(val, len, leftCtx->npIdOut);
    }
    if (spec->tag == 22) {
        sceKernelMemcpy(leftCtx->memberIdOut, val, len);
        *leftCtx->memberIdOut = npSwap16(*leftCtx->memberIdOut);
        return 0;
    }
    if (spec->tag == 71) {
        s32 ret = npProtoDecodeGroupConfig(val, len, leftCtx->groupOut);
        if (ret >= 0) {
            *leftCtx->hasGroupOut = 1;
        }
        return ret;
    }
    if (spec->tag == 98) {
        sceKernelMemcpy(leftCtx->reasonOut, val, len);
        return 0;
    }
    if (spec->tag == 58) {
        sceKernelMemcpy(leftCtx->cookieOut->bytes, val, len);
        leftCtx->cookieOut->len = len;
    }
    return 0;
}

/* 0x010e50 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomOwnerChangedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoOwnerChangedCtx *ownCtx = (NpProtoOwnerChangedCtx *)ctx;
    if (spec->tag == 40) {
        sceKernelMemcpy(ownCtx->optDataOut->data, val, len);
        ownCtx->optDataOut->size = len;
        return 0;
    }
    if (spec->tag == 11) {
        return npProtoDecodeNpId(val, len, ownCtx->npIdOut);
    }
    if (spec->tag == 22) {
        sceKernelMemcpy(ownCtx->newOwnerIdOut, val, len);
        *ownCtx->newOwnerIdOut = npSwap16(*ownCtx->newOwnerIdOut);
        return 0;
    }
    if (spec->tag == 70) {
        if (len != 0) {
            sceKernelMemcpy(ownCtx->passwordOut, val, len);
            *ownCtx->hasPasswordOut = 1;
        }
        return 0;
    }
    if (spec->tag == 98) {
        sceKernelMemcpy(ownCtx->reasonOut, val, len);
        return 0;
    }
    if (spec->tag == 58) {
        sceKernelMemcpy(ownCtx->cookieOut->bytes, val, len);
        ownCtx->cookieOut->len = len;
    }
    return 0;
}

/* 0x010f7c */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDestroyedNotifyTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDestroyedCtx *destCtx = (NpProtoRoomDestroyedCtx *)ctx;
    if (spec->tag == 58) {
        sceKernelMemcpy(destCtx->cookieOut->bytes, val, len);
        destCtx->cookieOut->len = len;
    } else if (spec->tag == 98) {
        sceKernelMemcpy(destCtx->reasonOut, val, len);
    } else if (spec->tag == 40) {
        sceKernelMemcpy(destCtx->optDataOut->data, val, len);
        destCtx->optDataOut->size = len;
    }
    return 0;
}

/* 0x011018 */
NP_MATCHING2_NOINLINE
static s32 npProtoDecodeRoomBinAttrInt(u16 tag, const u8 *val, u16 len, u16 maxAttrs,
                                       SceNpMatching2BinAttrInternal *attrs)
{
    u32 idx = 0;

    while (idx < maxAttrs) {
        if (attrs[idx].attr.id == 0) {
            break;
        }
        idx++;
    }
    if (idx != maxAttrs) {
        SceNpMatching2BinAttrInternal *slot = &attrs[idx];
        s32 step;
        s32 blobRet;
        slot->attr.id = tag;
        sceKernelMemcpy(&slot->updateMemberId, val, 2);
        slot->updateMemberId = npSwap16(slot->updateMemberId);
        step = npProtoDecodeTimestamp(val + 2, (u32)(len - 2), &slot->updateDate);
        if (step < 0) {
            return step;
        }
        blobRet = npProtoDecodeWrappedBlob(val + 2 + step, (u16)(len - (step + 2)),
                                           &slot->attr.ptr, &slot->attr.size);
        if (blobRet < 0) {
            return blobRet;
        }
        return step + 2 + blobRet;
    }
    return 0;
}

/* 0x011110 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomDataIntFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomDataInternalWire *room = (NpProtoRoomDataInternalWire *)ctx;
    u16 tag = spec->tag;

    if (tag == 87 || tag == 88) {
        return npProtoDecodeRoomBinAttrInt(tag, val, len, 2, room->binAttrInternal);
    }
    if (tag == 2) {
        u16 innerTag;
        u16 innerCount;
        sceKernelMemcpy(&innerTag, val, 2);
        innerTag = (u16)(npSwap16(innerTag) & 0x0FFF);
        sceKernelMemcpy(&innerCount, val + 2, 2);
        innerCount = npSwap16(innerCount);
        if (innerTag == 71) {
            void *groupsPtr = NULL;
            s32 parsed = npProtoParseTlvArray(71, val + 4, (u16)(len - 4), innerCount,
                                              &groupsPtr, (u32)innerCount * sizeof(NpProtoGroupConfigWire),
                                              npProtoOnGroupConfigItem, NULL);
            room->groupList = (NpProtoGroupConfigWire *)groupsPtr;
            if (parsed >= 0) {
                room->groupCount = (u32)parsed;
            }
            return parsed;
        }
        return 0;
    }
    if (tag == 65) {
        sceKernelMemcpy(room->publicMeta, val, len);
        return 0;
    }
    if (tag == 109) {
        sceKernelMemcpy(room->roomAttrBytes, val, len);
        room->roomAttrLen = len;
    }
    return 0;
}

/* 0x011260 */
NP_MATCHING2_NOINLINE
static s32 npProtoDecodeRoomDataInt(const u8 *buf, u16 bufLen,
                                    NpProtoRoomDataInternalWire *outRoom, s32 requireAll)
{
    u32 rawWords[2];
    const NpTlvSpec *specs;
    s32 ret;

    sceKernelMemcpy(&outRoom->roomHandle, buf, 2);
    sceKernelMemcpy(&outRoom->flagAttr, buf + 2, 4);
    outRoom->flagAttr = npSwap32(outRoom->flagAttr);
    sceKernelMemcpy(&outRoom->maxSlot, buf + 6, 2);
    outRoom->maxSlot = npSwap16(outRoom->maxSlot);
    sceKernelMemcpy(outRoom->roomMeta, buf + 8, 16);
    sceKernelMemcpy(rawWords, buf + 24, 8);
    outRoom->passwordSlotMask = ((u64)npSwap32(rawWords[0]) << 32) | (u64)npSwap32(rawWords[1]);

    specs = (requireAll == 1) ? g_specRoomDataIntReq : g_specRoomDataIntOpt;
    ret = npProtoParseTlvStream(buf + 32, (u32)(bufLen - 32), specs, 5,
                                npProtoOnRoomDataIntFieldTlv, outRoom);
    if (ret >= 0) {
        ret += 32;
    }
    return ret;
}

/* 0x0113b0 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberIntFieldTlv(const NpTlvSpec *spec, const u8 *val, u16 len, void *ctx)
{
    NpProtoRoomMemberInternalWire *member = (NpProtoRoomMemberInternalWire *)ctx;
    u16 tag = spec->tag;

    if (tag == 11) {
        return npProtoDecodeNpId(val, len, &member->npId);
    }
    if (tag == 12) {
        if (len == 0) {
            member->onlineName = NULL;
            return 0;
        }
        member->onlineName = (char *)npMatching2Alloc(52);
        if (member->onlineName == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(member->onlineName, 0, 52);
        sceKernelMemcpy(member->onlineName, val, len);
        return 0;
    }
    if (tag == 13) {
        if (len == 0) {
            member->avatarUrl = NULL;
            return 0;
        }
        member->avatarUrl = (char *)npMatching2Alloc(128);
        if (member->avatarUrl == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(member->avatarUrl, 0, 128);
        sceKernelMemcpy(member->avatarUrl, val, len);
        return 0;
    }
    if (tag == 89) {
        if (member->binAttrInternal.attr.ptr == NULL) {
            s32 step;
            member->binAttrInternal.attr.id = 89;
            step = npProtoDecodeTimestamp(val, len, &member->binAttrInternal.updateDate);
            if (step < 0) {
                return step;
            }
            return npProtoDecodeWrappedBlob(val + step, (u16)(len - step),
                                            &member->binAttrInternal.attr.ptr,
                                            &member->binAttrInternal.attr.size);
        }
        return 0;
    }
    if (tag == 110) {
        sceKernelMemcpy(member->extraAttrBytes, val, len);
        member->extraAttrLen = len;
    }
    return 0;
}

/* 0x011538 */
NP_MATCHING2_NOINLINE
static s32 npProtoDecodeRoomMemberInt(const u8 *buf, u16 bufLen,
                                      NpProtoRoomMemberInternalWire *outMember, s32 requireAll)
{
    const u8 *cursor = buf + 2;
    const NpTlvSpec *specs;
    s32 step;

    sceKernelMemcpy(&outMember->memberId, buf, 2);
    outMember->memberId = npSwap16(outMember->memberId);
    step = npProtoDecodeTimestamp(cursor, (u32)(bufLen - 2), &outMember->joinDate);
    if (step < 0) {
        return step;
    }
    cursor += step;
    sceKernelMemcpy(&outMember->flagAttr, cursor, 4);
    outMember->flagAttr = npSwap32(outMember->flagAttr);
    cursor += 4;
    sceKernelMemcpy(&outMember->natType, cursor, 1);
    sceKernelMemcpy(&outMember->teamId, cursor + 1, 1);
    cursor += 2;

    specs = (requireAll == 1) ? g_specRoomMemberIntReq : g_specRoomMemberIntOpt;
    step = npProtoParseTlvStream(cursor, (u32)(bufLen - (u32)(cursor - buf)), specs, 5,
                                 npProtoOnRoomMemberIntFieldTlv, outMember);
    if (step >= 0) {
        return (s32)((cursor - buf) + step);
    }
    return step;
}

/* 0x011654 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnRoomMemberIntItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    const s32 *requireAll = (const s32 *)ctx;
    s32 ret;
    (void)tag;

    ret = npProtoDecodeRoomMemberInt(val, len, (NpProtoRoomMemberInternalWire *)outItem, *requireAll);
    if (ret >= 0) {
        return (s32)sizeof(NpProtoRoomMemberInternalWire);
    }
    return ret;
}

/* 0x011690 */
NP_MATCHING2_NOINLINE
static s32 npProtoOnGroupConfigItem(u16 tag, const u8 *val, u16 len, void *outItem, void *ctx)
{
    NpProtoGroupConfigWire *group = (NpProtoGroupConfigWire *)outItem;
    (void)tag;
    (void)len;
    (void)ctx;

    sceKernelMemcpy(&group->groupId, val, 1);
    sceKernelMemcpy(&group->withPassword, val + 1, 1);
    sceKernelMemcpy(group->label, val + 2, 8);
    return (s32)sizeof(NpProtoGroupConfigWire);
}
