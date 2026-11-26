#include "np_matching2_internal.h"

static const s8 g_base64DecodeTable[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

s32 g_npMatching2VplId = -1;
s32 g_npMatching2VplMaxUsed = -1;
s32 g_npMatching2Initialized = 0;

/* 0x000000 */
NP_MATCHING2_NOINLINE s32 npMatching2Base64Decode(char *outBuf, const u8 *inStr, s32 inLen)
{
    s32 decodedLen = 0;

    if (inLen != 0) {
        for (;;) {
            u8 thirdChar = inStr[2];
            u8 fourthChar = inStr[3];
            s32 digitFirst;
            s32 digitSecond;
            s32 digitThird;
            s32 digitFourth;
            u32 combined;

            if (thirdChar == '=')
                decodedLen--;
            if (fourthChar == '=')
                decodedLen--;

            digitFirst = g_base64DecodeTable[inStr[0]];
            digitSecond = g_base64DecodeTable[inStr[1]];
            digitThird = g_base64DecodeTable[thirdChar];
            digitFourth = g_base64DecodeTable[fourthChar];

            if (digitFirst < 0)
                return -1;
            if (digitSecond < 0)
                return -1;
            if (digitThird < 0)
                return -1;
            if (digitFourth < 0)
                return -1;

            combined = ((u32)digitFirst << 18) |
                       ((u32)digitSecond << 12) |
                       ((u32)digitThird << 6) |
                       (u32)digitFourth;

            outBuf[0] = (char)((combined >> 16) & 0xFFu);
            outBuf[1] = (char)((combined >> 8) & 0xFFu);
            outBuf[2] = (char)(combined & 0xFFu);

            inStr += 4;
            outBuf += 3;
            inLen -= 4;
            decodedLen += 3;

            if (inLen <= 0)
                break;
        }
    }

    *outBuf = '\0';
    return decodedLen;
}

/* 0x000634 */
NP_MATCHING2_NOINLINE s32 npMatching2VplCreate(s32 partId, u32 poolSize, const char *name)
{
    s32 vplId = sceKernelCreateVpl(name, partId, 0, poolSize, NULL);
    if (vplId >= 0) {
        g_npMatching2VplId = vplId;
        g_npMatching2VplMaxUsed = 0;
    }
    return vplId;
}

/* 0x000678 */
NP_MATCHING2_NOINLINE s32 npMatching2VplDelete(void)
{
    if (g_npMatching2VplId > 0) {
        sceKernelDeleteVpl(g_npMatching2VplId);
        g_npMatching2VplId = -1;
        return -1;
    }
    return 0;
}

/* 0x0006b4 */
NP_MATCHING2_NOINLINE s32 npMatching2VplGetStat(SceNpMatching2MemoryStat *statOut)
{
    SceKernelVplInfo vplInfo;
    s32 ret;

    if (statOut == NULL)
        return SCE_NP_ERROR_INVALID_PARAM_COMMON;

    vplInfo.size = sizeof(SceKernelVplInfo);
    ret = sceKernelReferVplStatus(g_npMatching2VplId, &vplInfo);
    if (ret >= 0) {
        sceKernelMemset(statOut, 0, sizeof(SceNpMatching2MemoryStat));
        statOut->totalSize = (u32)vplInfo.poolSize;
        statOut->maxUsedSize = (u32)g_npMatching2VplMaxUsed;
        statOut->currentFreeSize = (u32)vplInfo.freeSize;
        ret = 0;
    }
    return ret;
}

/* 0x000738 */
NP_MATCHING2_NOINLINE void *npMatching2Alloc(u32 size)
{
    SceKernelVplInfo vplInfo;
    void *allocated;

    if (sceKernelTryAllocateVpl(g_npMatching2VplId, size, &allocated) >= 0) {
        vplInfo.size = sizeof(SceKernelVplInfo);
        if (sceKernelReferVplStatus(g_npMatching2VplId, &vplInfo) == 0) {
            s32 usedSize = vplInfo.poolSize - vplInfo.freeSize;
            if (usedSize > g_npMatching2VplMaxUsed)
                g_npMatching2VplMaxUsed = usedSize;
        }
        return allocated;
    }
    return NULL;
}

/* 0x0007ac */
NP_MATCHING2_NOINLINE s32 npMatching2Free(void *ptr)
{
    if (ptr != NULL)
        return sceKernelFreeVpl(g_npMatching2VplId, ptr);
    return 0;
}

/* 0x0007d4 */
s32 sceNpMatching2Init(u32 poolSize, s32 threadPriority, u32 threadStackSize, s32 sigThreadPriority)
{
    s32 ret;

    if (threadStackSize < 0x3800u)
        return SCE_NP_MATCHING2_ERROR_POOL_SIZE_TOO_SMALL;
    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 1)
        return SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED;

    ret = npCoreInit(poolSize, (s32)threadStackSize, (u32)threadPriority);
    if (ret >= 0) {
        ret = npSignalingBridgeInit(0x2000u, sigThreadPriority);
        if (ret >= 0) {
            g_npMatching2Initialized = 1;
            return 0;
        }
        npCoreTerm();
    }
    return ret;
}

/* 0x0008b8 */
s32 sceNpMatching2Term(void)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npCoreTerm();
    if (ret >= 0) {
        ret = npSignalingBridgeTerm();
        if (ret >= 0)
            g_npMatching2Initialized = 0;
    }
    return ret;
}

/* 0x000930 */
s32 sceNpMatching2GetMemoryStat(SceNpMatching2MemoryStat *statOut)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npMatching2VplGetStat(statOut);
}

/* 0x00098c */
s32 sceNpMatching2CreateContext(const SceNpCommId *commId, const SceNpCommPassphrase *passPhrase,
                                SceNpMatching2ContextId *ctxIdOut, u32 option)
{
    SceNpId npId;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4016)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (commId == NULL || passPhrase == NULL || ctxIdOut == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    ret = sceNpGetNpId(&npId);
    if (ret < 0)
        return ret;

    ret = npCoreCreateContext(commId, passPhrase, ctxIdOut, option, 0);
    if (ret >= 0)
        ret = npSignalingBridgeAddContext(&npId, *ctxIdOut);
    return ret;
}

/* 0x000a5c */
s32 sceNpMatching2DestroyContext(SceNpMatching2ContextId ctxId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npSignalingBridgeRemoveContext(ctxId);
    if (ret >= 0)
        ret = npCoreDestroyContext(ctxId);
    return ret;
}

/* 0x000acc */
NP_MATCHING2_NOINLINE s32 npMatching2RegisterInternalListener(u32 listenerId, SceNpMatching2ContextId ctxId,
                                                              const void *callbacks)
{
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (callbacks == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    return npCoreRegisterListener(listenerId, ctxId, callbacks);
}

/* 0x000b0c */
NP_MATCHING2_NOINLINE s32 npMatching2UnregisterInternalListener(u32 listenerId, SceNpMatching2ContextId ctxId)
{
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreUnregisterListener(listenerId, ctxId);
}

/* 0x000b40 */
s32 sceNpMatching2SetDefaultRequestOptParam(SceNpMatching2ContextId ctxId, const SceNpMatching2RequestOptParam *optParam)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (optParam == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    return npCoreSetDefaultReqOpt(ctxId, optParam);
}

/* 0x000bb8 */
s32 sceNpMatching2SetDefaultRoomEventOptParam(SceNpMatching2ContextId ctxId, const SceNpMatching2RoomEventOptParam *optParam)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (optParam == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    return npCoreSetDefaultRoomEvOpt(ctxId, optParam);
}

/* 0x000c30 */
s32 sceNpMatching2SetDefaultRoomMessageOptParam(SceNpMatching2ContextId ctxId, const SceNpMatching2RoomMessageOptParam *optParam)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (optParam == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    return npCoreSetDefaultRoomMsgOpt(ctxId, optParam);
}

/* 0x000ca8 */
s32 sceNpMatching2ContextStart(SceNpMatching2ContextId ctxId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npCoreStartContext(ctxId);
    if (ret >= 0)
        ret = npSignalingBridgeStartContext(ctxId);
    return ret;
}

/* 0x000d1c */
s32 sceNpMatching2ContextStop(SceNpMatching2ContextId ctxId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npSignalingBridgeStopContext(ctxId);
    if (ret >= 0)
        ret = npCoreStopContext(ctxId);
    return ret;
}

/* 0x000d8c */
s32 sceNpMatching2AbortRequest(SceNpMatching2RequestId reqId)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreAbortRequest(reqId);
}

/* 0x000de8 */
s32 sceNpMatching2GetServerIdListLocal(SceNpMatching2ContextId ctxId, SceNpMatching2ServerId *serverIdList, u32 listNum)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreGetServerIdListLocal(ctxId, serverIdList, listNum);
}

/* 0x000e64 */
s32 sceNpMatching2GetServerInfo(SceNpMatching2ContextId ctxId, const SceNpMatching2GetServerInfoRequest *reqParam,
                                const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetServerInfoReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetServerInfo(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x000f04 */
s32 sceNpMatching2GetWorldInfoList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetWorldInfoListRequest *reqParam,
                                   const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetWorldInfoListReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetWorldInfoList(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x000fa4 */
s32 sceNpMatching2SetUserInfo(SceNpMatching2ContextId ctxId, const SceNpMatching2SetUserInfoRequest *reqParam,
                              const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4032)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSetUserInfoReq(reqParam);
    if (ret >= 0)
        ret = npCoreSetUserInfo(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001044 */
s32 sceNpMatching2GetUserInfoList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetUserInfoListRequest *reqParam,
                                  const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4016)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetUserInfoListReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetUserInfoList(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x0010e4 */
s32 sceNpMatching2GetRoomMemberDataExternalList(SceNpMatching2ContextId ctxId,
                                                const SceNpMatching2GetRoomMemberDataExternalListRequest *reqParam,
                                                const SceNpMatching2RequestOptParam *optParam,
                                                SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomMemberDataExternalListReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetRoomMemberDataExternalList(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001184 */
s32 sceNpMatching2SetRoomDataExternal(SceNpMatching2ContextId ctxId, const SceNpMatching2SetRoomDataExternalRequest *reqParam,
                                      const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSetRoomDataExternalReq(reqParam);
    if (ret >= 0)
        ret = npCoreSetRoomDataExternal(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001224 */
s32 sceNpMatching2GetRoomDataExternalList(SceNpMatching2ContextId ctxId,
                                          const SceNpMatching2GetRoomDataExternalListRequest *reqParam,
                                          const SceNpMatching2RequestOptParam *optParam,
                                          SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomDataExternalListReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetRoomDataExternalList(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x0012c4 */
s32 sceNpMatching2SetSignalingOptParam(SceNpMatching2ContextId ctxId,
                                       const SceNpMatching2SetSignalingOptParamRequest *reqParam,
                                       const SceNpMatching2RequestOptParam *optParam,
                                       SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSetSignalingOptParamReq(reqParam);
    if (ret >= 0)
        ret = npCoreSetSignalingOptParam(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001364 */
s32 sceNpMatching2GetSignalingOptParamLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                            SceNpMatching2SignalingOptParam *optParamOut)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetSignalingOptParamLocal(roomId, optParamOut);
    if (ret >= 0)
        ret = npCoreGetSignalingOptParamLocal(ctxId, roomId, optParamOut);
    return ret;
}

/* 0x00140c */
s32 sceNpMatching2CreateJoinRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2CreateJoinRoomRequest *reqParam,
                                 const SceNpMatching2RequestOptParam *optParam,
                                 const SceNpMatching2RoomEventOptParam *roomEvOpt,
                                 const SceNpMatching2RoomMessageOptParam *roomMsgOpt,
                                 SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateCreateJoinRoomReq(reqParam);
    if (ret >= 0)
        ret = npCoreCreateJoinRoom(ctxId, reqParam, optParam, roomEvOpt, roomMsgOpt, assignedReqId);
    return ret;
}

/* 0x0014cc */
s32 sceNpMatching2JoinRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2JoinRoomRequest *reqParam,
                           const SceNpMatching2RequestOptParam *optParam,
                           const SceNpMatching2RoomEventOptParam *roomEvOpt,
                           const SceNpMatching2RoomMessageOptParam *roomMsgOpt,
                           SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateJoinRoomReq(reqParam);
    if (ret >= 0)
        ret = npCoreJoinRoom(ctxId, reqParam, optParam, roomEvOpt, roomMsgOpt, assignedReqId);
    return ret;
}

/* 0x00158c */
s32 sceNpMatching2LeaveRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2LeaveRoomRequest *reqParam,
                            const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateLeaveRoomReq(reqParam);
    if (ret >= 0)
        ret = npCoreLeaveRoom(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x00162c */
s32 sceNpMatching2GrantRoomOwner(SceNpMatching2ContextId ctxId, const SceNpMatching2GrantRoomOwnerRequest *reqParam,
                                 const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGrantRoomOwnerReq(reqParam);
    if (ret >= 0)
        ret = npCoreGrantRoomOwner(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x0016cc */
s32 sceNpMatching2KickoutRoomMember(SceNpMatching2ContextId ctxId, const SceNpMatching2KickoutRoomMemberRequest *reqParam,
                                    const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateKickoutRoomMemberReq(reqParam);
    if (ret >= 0)
        ret = npCoreKickoutRoomMember(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x00176c */
s32 sceNpMatching2SearchRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2SearchRoomRequest *reqParam,
                             const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSearchRoomReq(reqParam);
    if (ret >= 0)
        ret = npCoreSearchRoom(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x00180c */
s32 sceNpMatching2SendRoomChatMessage(SceNpMatching2ContextId ctxId,
                                      const SceNpMatching2SendRoomChatMessageRequest *reqParam,
                                      const SceNpMatching2RequestOptParam *optParam,
                                      SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSendRoomChatMessageReq(reqParam);
    if (ret >= 0)
        ret = npCoreSendRoomChatMessage(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x0018ac */
s32 sceNpMatching2SendRoomMessage(SceNpMatching2ContextId ctxId, const SceNpMatching2SendRoomMessageRequest *reqParam,
                                  const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSendRoomMessageReq(reqParam);
    if (ret >= 0)
        ret = npCoreSendRoomMessage(ctxId, reqParam, optParam, assignedReqId, 0);
    return ret;
}

/* 0x001954 */
NP_MATCHING2_NOINLINE s32 npMatching2SendRoomMessageInternal(SceNpMatching2ContextId ctxId,
                                                             const SceNpMatching2SendRoomMessageRequest *reqParam,
                                                             const SceNpMatching2RequestOptParam *optParam,
                                                             SceNpMatching2RequestId *assignedReqId,
                                                             s32 reqSource)
{
    s32 ret;

    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSendRoomMessageReq(reqParam);
    if (ret >= 0)
        ret = npCoreSendRoomMessage(ctxId, reqParam, optParam, assignedReqId, reqSource);
    return ret;
}

/* 0x0019f4 */
s32 sceNpMatching2SetRoomDataInternal(SceNpMatching2ContextId ctxId,
                                      const SceNpMatching2SetRoomDataInternalRequest *reqParam,
                                      const SceNpMatching2RequestOptParam *optParam,
                                      SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSetRoomDataInternalReq(reqParam);
    if (ret >= 0)
        ret = npCoreSetRoomDataInternal(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001a94 */
s32 sceNpMatching2GetRoomDataInternal(SceNpMatching2ContextId ctxId,
                                      const SceNpMatching2GetRoomDataInternalRequest *reqParam,
                                      const SceNpMatching2RequestOptParam *optParam,
                                      SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomDataInternalReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetRoomDataInternal(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001b34 */
s32 sceNpMatching2SetRoomMemberDataInternal(SceNpMatching2ContextId ctxId,
                                            const SceNpMatching2SetRoomMemberDataInternalRequest *reqParam,
                                            const SceNpMatching2RequestOptParam *optParam,
                                            SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateSetRoomMemberDataInternalReq(reqParam);
    if (ret >= 0)
        ret = npCoreSetRoomMemberDataInternal(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001bd4 */
s32 sceNpMatching2GetRoomMemberDataInternal(SceNpMatching2ContextId ctxId,
                                            const SceNpMatching2GetRoomMemberDataInternalRequest *reqParam,
                                            const SceNpMatching2RequestOptParam *optParam,
                                            SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomMemberDataInternalReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetRoomMemberDataInternal(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001c74 */
s32 sceNpMatching2GetRoomMemberDataInternalList(SceNpMatching2ContextId ctxId,
                                                const SceNpMatching2GetRoomMemberDataInternalListRequest *reqParam,
                                                const SceNpMatching2RequestOptParam *optParam,
                                                SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomMemberDataInternalListReq(reqParam);
    if (ret >= 0)
        ret = npCoreGetRoomMemberDataInternalList(ctxId, reqParam, optParam, assignedReqId);
    return ret;
}

/* 0x001d14 */
s32 sceNpMatching2GetRoomDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                           const SceNpMatching2AttributeId *attrId, u32 attrIdNum,
                                           void *roomDataOut, void *buf, u32 bufSize)
{
    if (_sceKernelCheckThreadStack() < 4048)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreGetRoomDataInternalLocal(ctxId, roomId, attrId, attrIdNum, roomDataOut, buf, bufSize);
}

/* 0x001dd4 */
s32 sceNpMatching2GetRoomMemberIdListLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                           u32 sortMethod, SceNpMatching2RoomMemberId *memberIdList, u32 listNum)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4064)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateRoomIdAndSortMethod(roomId, sortMethod);
    if (ret >= 0)
        ret = npCoreGetRoomMemberIdListLocal(ctxId, roomId, memberIdList, listNum);
    return ret;
}

/* 0x001ea0 */
s32 sceNpMatching2GetRoomMemberDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                 SceNpMatching2RoomMemberId memberId,
                                                 const SceNpMatching2AttributeId *attrId, u32 attrIdNum,
                                                 void *memberDataOut, void *buf, u32 bufSize)
{
    s32 ret;

    if (_sceKernelCheckThreadStack() < 4048)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    ret = npValidateGetRoomMemberDataInternalLocal(roomId, memberId, attrId, (s32)attrIdNum);
    if (ret >= 0)
        ret = npCoreGetRoomMemberDataInternalLocal(ctxId, roomId, memberId, attrId, attrIdNum, memberDataOut, buf, bufSize);
    return ret;
}

/* 0x001fa8 */
s32 sceNpMatching2GetRoomPasswordLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                       s32 *withPasswordOut, SceNpMatching2SessionPassword *passwordOut)
{
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreGetRoomPasswordLocal(ctxId, roomId, withPasswordOut, passwordOut);
}

/* 0x001fdc */
s32 sceNpMatching2RegisterSignalingCallback(SceNpMatching2ContextId ctxId, void *cbFunc, void *cbArg)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_STACK_OVERFLOW;
    if (g_npMatching2Initialized == 0)
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    return npCoreRegisterSignalingCb(ctxId, cbFunc, cbArg);
}
