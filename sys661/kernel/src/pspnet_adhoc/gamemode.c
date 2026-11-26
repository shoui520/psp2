#include "adhoc_int.h"

static s32 g_gm_master_id = -1;
static struct {
    GameModeNode *head;
    GameModeNode **tail;
} g_gm_list;

static s32 gameModeCreateBuffer(const u8 *mac, void *buf, u32 size);
static s32 gameModeDeleteBuffer(s32 id);
static s32 gameModeUpdateBuffer(s32 id, void *opt_buf, s32 mode);

/* 0x006c1c */
s32 sceNetAdhocGameModeCreateMaster(void *buf, u32 size)
{
    u8 self_mac[16];
    s32 intr;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 0x7d0)
        return ADHOC_ERR_STACK_SMALL;
    if (buf == NULL)
        return ADHOC_ERR_INVALID_ARG;
    if (size - 1 >= 0x400)
        return ADHOC_ERR_INVALID_BUFLEN;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else if (sceWlanDevIsGameMode() == 0) {
        ret = ADHOC_ERR_NOT_IN_GAMEMODE;
    } else {
        ret = sceNetGetLocalEtherAddr(self_mac);
        if (ret >= 0) {
            ret = gameModeCreateBuffer(self_mac, buf, size);
            if (ret >= 0) {
                g_gm_master_id = ret;
                ret = 0;
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x006d0c */
s32 sceNetAdhocGameModeCreateReplica(const u8 *mac, void *buf, u32 size)
{
    s32 intr;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 0x7e0)
        return ADHOC_ERR_STACK_SMALL;
    if (mac == NULL || buf == NULL)
        return ADHOC_ERR_INVALID_ARG;
    if (size - 1 >= 0x400)
        return ADHOC_ERR_INVALID_BUFLEN;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else if (sceWlanDevIsGameMode() == 0) {
        ret = ADHOC_ERR_NOT_IN_GAMEMODE;
    } else {
        ret = gameModeCreateBuffer(mac, buf, size);
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x006df4 */
s32 sceNetAdhocGameModeUpdateMaster(void)
{
    s32 intr;
    s32 ret;
    GameModeNode *gm;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else if (sceWlanDevIsGameMode() == 0) {
        ret = ADHOC_ERR_NOT_IN_GAMEMODE;
    } else if (g_gm_master_id < 0) {
        ret = ADHOC_ERR_NOT_CREATED;
    } else {
        for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
            if (gm->id == g_gm_master_id)
                break;
        }
        if (gm == NULL) {
            ret = ADHOC_ERR_NOT_CREATED;
        } else {
            sceKernelMemcpy(gm->work_buf, gm->user_buf, gm->user_size);
            ret = sceNetAdhocAuth_lib_AAB06250(gm->work_buf, gm->work_size);
            if (ret >= 0)
                ret = sceWlanGPSend(0x10, gm->work_buf, gm->work_size);
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x006f08 */
s32 sceNetAdhocGameModeDeleteMaster(void)
{
    s32 intr;
    s32 ret;
    GameModeNode *gm;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else if (g_gm_master_id < 0) {
        ret = ADHOC_ERR_NOT_CREATED;
    } else {
        for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
            if (gm->id == g_gm_master_id)
                break;
        }
        if (gm == NULL) {
            ret = ADHOC_ERR_NOT_CREATED;
        } else {
            if (gm->next != NULL)
                gm->next->prev = gm->prev;
            else
                g_gm_list.tail = gm->prev;
            *gm->prev = gm->next;
            sceNetIfhandle_lib_76BAD213(gm->work_buf);
            sceNetIfhandle_lib_76BAD213(gm);
            ret = 0;
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x007008 */
s32 __attribute__((noinline))
gameModeInit(void)
{
    g_gm_list.head = NULL;
    g_gm_list.tail = &g_gm_list.head;
    return 0;
}

/* 0x007020 */
void __attribute__((noinline))
gameModeTerm(void)
{
    GameModeNode *gm;

    while ((gm = g_gm_list.head) != NULL) {
        if (gm->next != NULL)
            gm->next->prev = gm->prev;
        else
            g_gm_list.tail = gm->prev;
        *gm->prev = gm->next;
        sceNetIfhandle_lib_76BAD213(gm);
    }
}

/* 0x007084 */
s32 sceNetAdhocGameModeUpdateReplica(s32 id, void *opt_buf)
{
    if (_sceKernelCheckThreadStack() < 0x7f0)
        return ADHOC_ERR_STACK_SMALL;
    return gameModeUpdateBuffer(id, opt_buf, 0);
}

/* 0x0070e0 */
s32 sceNetAdhoc_lib_AF6DFB15(s32 id, void *opt_buf)
{
    return gameModeUpdateBuffer(id, opt_buf, 1);
}

/* 0x0070fc */
s32 sceNetAdhocGameModeDeleteReplica(s32 id)
{
    s32 intr;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        ret = gameModeDeleteBuffer(id);
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x007188 */
static s32 __attribute__((noinline))
gameModeCreateBuffer(const u8 *mac, void *buf, u32 size)
{
    s32 id;
    GameModeNode *gm;
    u32 work_size;

    id = sceWlanGPPrevEstablishActive();
    if (id < 0)
        return id;

    for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
        if (gm->id == id)
            return ADHOC_ERR_ALREADY_CREATED;
    }

    gm = (GameModeNode *)sceNetIfhandle_lib_15CFE3C0(sizeof(GameModeNode));
    if (gm == NULL)
        return ADHOC_ERR_NO_MEM;

    gm->id = id;
    sceKernelMemcpy(gm->mac, mac, 6);
    work_size = (size + 0x0f) & ~0x0fU;
    gm->user_buf = buf;
    gm->user_size = size;
    gm->work_size = work_size;
    gm->work_buf = sceNetIfhandle_lib_15CFE3C0(work_size);
    if (gm->work_buf == NULL) {
        sceNetIfhandle_lib_76BAD213(gm);
        return ADHOC_ERR_NO_MEM;
    }

    gm->next = NULL;
    gm->prev = g_gm_list.tail;
    *g_gm_list.tail = gm;
    g_gm_list.tail = &gm->next;
    return id;
}

/* 0x0072a4 */
static s32 __attribute__((noinline))
gameModeDeleteBuffer(s32 id)
{
    GameModeNode *gm;

    for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
        if (gm->id == id)
            break;
    }
    if (gm == NULL)
        return ADHOC_ERR_NOT_CREATED;

    if (gm->next != NULL)
        gm->next->prev = gm->prev;
    else
        g_gm_list.tail = gm->prev;
    *gm->prev = gm->next;
    sceNetIfhandle_lib_76BAD213(gm->work_buf);
    sceNetIfhandle_lib_76BAD213(gm);
    return 0;
}

/* 0x007338 */
static s32 __attribute__((noinline))
gameModeUpdateBuffer(s32 id, void *opt_buf, s32 mode)
{
    u32 recv_info[6];
    s32 intr;
    s32 ret;
    GameModeNode *gm;

    if (id < 0)
        return ADHOC_ERR_SOCKET_ALERTED;

    if (mode == 0 && opt_buf != NULL) {
        GameModeReplicaOpt *opt = (GameModeReplicaOpt *)opt_buf;
        if (opt->size != sizeof(GameModeReplicaOpt))
            return ADHOC_ERR_INVALID_BUFLEN;
    }

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else if (sceWlanDevIsGameMode() == 0) {
        ret = ADHOC_ERR_NOT_IN_GAMEMODE;
    } else {
        for (gm = g_gm_list.head; gm != NULL; gm = gm->next) {
            if (gm->id == id)
                break;
        }
        if (gm == NULL) {
            ret = ADHOC_ERR_NOT_CREATED;
        } else {
            ret = sceWlanGPRecv(id, gm->work_buf, gm->work_size, recv_info);
            if (ret >= 0) {
                if (recv_info[0] != 0) {
                    ret = sceNetAdhocAuth_lib_015A8A64(gm->work_buf, gm->work_size);
                    if (ret >= 0) {
                        sceKernelMemcpy(gm->user_buf, gm->work_buf, gm->user_size);
                        ret = 0;
                    }
                }
                if (ret >= 0 && opt_buf != NULL) {
                    if (mode == 1) {
                        GameModeReplicaOptEx *opt_ex = (GameModeReplicaOptEx *)opt_buf;
                        opt_ex->flag = recv_info[0];
                        opt_ex->last_update_lo = recv_info[2];
                        opt_ex->last_update_hi = recv_info[3];
                        opt_ex->extra_status = recv_info[4];
                    } else if (mode == 0) {
                        GameModeReplicaOpt *opt = (GameModeReplicaOpt *)opt_buf;
                        opt->flag = recv_info[0];
                        opt->last_update_lo = recv_info[2];
                        opt->last_update_hi = recv_info[3];
                    }
                }
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}
