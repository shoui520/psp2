#include <common_imp.h>

SCE_MODULE_INFO("sceNetAdhocDiscover_Library", SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 2);
SCE_SDK_VERSION(0x06060110);

typedef struct SceNetAdhocDiscoverParam {
    s32 mode;           /* 0x00 */
    char groupName[8];  /* 0x04 */
    u32 timeout;        /* 0x0c */
    s32 result;         /* 0x10 */
} SceNetAdhocDiscoverParam;

typedef struct SceNetAdhocctlScanInfo {
    struct SceNetAdhocctlScanInfo *next; /* 0x00 */
    s32 channel;                         /* 0x04 */
    char name[8];                        /* 0x08 */
    u8 bssid[6];                         /* 0x10 */
    u8 pad[2];                           /* 0x16 */
    s32 mode;                            /* 0x18 */
} SceNetAdhocctlScanInfo;

s32 _sceKernelCheckThreadStack(void);
void *sceKernelMemcpy(void *dst, const void *src, u32 size);
s64 sceKernelGetSystemTimeWide(void);
s32 sceNetAdhocctlScan(void);
s32 sceNetAdhocctlDisconnect(void);
s32 sceNetAdhocctlJoin(SceNetAdhocctlScanInfo *scanInfo);
s32 sceNetAdhocctlGetScanInfo(s32 *buflen, void *buf);
s32 sceNetAdhocctlGetPeerList(s32 *buflen, void *buf);
s32 sceNetAdhocctlCreate(const char *groupName);
s32 sceNetAdhocctl_lib_1572422C(void);
s32 sceNetAdhocctl_lib_1A2B6D03(const char *groupName);
s32 sceNetAdhocctl_lib_1C679240(s32 handlerId);
s32 sceNetAdhocctl_lib_F6057CC4(const char *groupName);
s32 sceNetAdhocctl_lib_F8BABD85(void (*handler)(s32 event, s32 error, void *arg), void *arg);
void *sceNetIfhandle_lib_15CFE3C0(s32 size);
void sceNetIfhandle_lib_76BAD213(void *ptr);
u32 sceNet_lib_1858883D(void);
s32 sceNet_lib_E0A81C7C(const void *s1, const void *s2, u32 n);
s32 sceUtilityGetSystemParamInt(s32 id, s32 *value);

static s32 g_handlerId = -1;                 /* 0x10a0 */
static s32 g_state;                          /* 0x10b0 */
static s32 g_subState;                       /* 0x10b4 */
static u64 g_nextTime;                       /* 0x10b8 */
static u64 g_endTime;                        /* 0x10c0 */
static u32 g_eventFlags;                     /* 0x10c8 */
static s32 g_eventError;                     /* 0x10cc */
static s32 g_adhocChannel;                   /* 0x10d0 */
static s32 g_canSkip;                        /* 0x10d4 */
static SceNetAdhocDiscoverParam *g_param;    /* 0x10d8 */
static SceNetAdhocctlScanInfo g_targetScan;  /* 0x10dc */

static void sub_07f8(s32 event, s32 error, void *arg);
static s32 sub_0938(void);
static s32 sub_0a30(void);
static s32 sub_0af4(void);
static s32 sub_0cbc(void);

s32 sceNetAdhocDiscover_941B3877(SceNetAdhocDiscoverParam *param)
{
    if (_sceKernelCheckThreadStack() < 0xFE0)
        return 0x80410005;
    if (g_state != 0)
        return 0x80411301;
    g_eventFlags = 0;
    g_eventError = 0;
    s32 id = sceNetAdhocctl_lib_F8BABD85(sub_07f8, NULL);
    if (id < 0)
        return id;
    s32 mode = param->mode;
    g_handlerId = id;
    g_canSkip = 0;
    g_param = param;
    if (mode == 0) {
        g_state = 11;
        g_subState = 3;
    } else if (mode == 1) {
        g_state = 15;
        g_subState = 4;
    } else {
        if (g_handlerId >= 0) {
            sceNetAdhocctl_lib_1C679240(g_handlerId);
            g_handlerId = -1;
        }
        return 0x80411302;
    }
    if (g_param->timeout == 0) {
        if (g_handlerId >= 0) {
            sceNetAdhocctl_lib_1C679240(g_handlerId);
            g_handlerId = -1;
        }
        return 0x80411302;
    }
    g_nextTime = 0;
    g_endTime = 0;
    s32 ret = sceUtilityGetSystemParamInt(2, &g_adhocChannel);
    if (ret < 0) {
        if (g_handlerId >= 0) {
            sceNetAdhocctl_lib_1C679240(g_handlerId);
            g_handlerId = -1;
        }
        return ret;
    }
    return 0;
}

s32 sceNetAdhocDiscover_52DE1B97(void)
{
    u32 delay = 0;
    if (_sceKernelCheckThreadStack() < 0xFF0)
        return 0x80410005;
    if ((u32)g_state >= 19)
        return 0;
    switch (g_state) {
    case 1: {
        s32 ret = sceNetAdhocctlCreate(g_param->groupName);
        if (ret < 0) {
            g_param->result = ret;
            g_state = 19;
        } else {
            g_state = 3;
        }
        break;
    }
    case 2: {
        s32 ret = sceNetAdhocctlJoin(&g_targetScan);
        if (ret >= 0) {
            g_state = 3;
        } else {
            g_param->result = ret;
            g_state = 19;
        }
        break;
    }
    case 3:
        if ((g_eventFlags & 2) != 0) {
            g_eventFlags &= ~2U;
            g_state = 4;
        } else if ((g_eventFlags & 1) != 0) {
            g_eventFlags &= ~1U;
            g_param->result = g_eventError;
            g_state = 19;
            g_eventError = 0;
        }
        break;
    case 4:
        sub_0a30();
        break;
    case 5: {
        s32 ret = sceNetAdhocctlDisconnect();
        if (ret < 0) {
            g_param->result = ret;
            g_state = 19;
        } else {
            g_state = 6;
        }
        break;
    }
    case 6:
        if ((g_eventFlags & 4) != 0) {
            g_eventFlags &= ~4U;
            g_state = 7;
        }
        break;
    case 7:
        sub_0938();
        break;
    case 8: {
        s32 ret = sceNetAdhocctlScan();
        if (ret < 0) {
            g_param->result = ret;
            g_state = 19;
        } else {
            g_state = 9;
        }
        break;
    }
    case 9:
        if ((g_eventFlags & 8) != 0) {
            g_eventFlags &= ~8U;
            g_state = 10;
        } else if ((g_eventFlags & 1) != 0) {
            g_eventFlags &= ~1U;
            g_param->result = g_eventError;
            g_state = 19;
            g_eventError = 0;
        }
        break;
    case 10:
        sub_0af4();
        break;
    case 11: {
        s32 ret = sceNetAdhocctl_lib_F6057CC4(g_param->groupName);
        if (ret < 0) {
            g_param->result = ret;
            g_state = 19;
        } else {
            g_state = 12;
        }
        break;
    }
    case 12:
        if ((g_eventFlags & 1) != 0) {
            g_eventFlags &= ~1U;
            g_param->result = g_eventError;
            g_state = 19;
            g_eventError = 0;
        } else if ((g_eventFlags & 0x200) != 0) {
            g_eventFlags &= ~0x200U;
            g_state = 13;
        }
        break;
    case 13:
        sub_0cbc();
        break;
    case 14:
        if (g_subState == 3) {
            g_state = 7;
            g_subState = 0;
        }
        break;
    case 15: {
        s32 ret = sceNetAdhocctl_lib_1A2B6D03(g_param->groupName);
        if (ret < 0) {
            g_param->result = ret;
            g_state = 19;
        } else {
            g_state = 16;
        }
        break;
    }
    case 16:
        if ((g_eventFlags & 1) != 0) {
            g_eventFlags &= ~1U;
            g_param->result = g_eventError;
            g_state = 19;
            g_eventError = 0;
        } else if ((g_eventFlags & 0x200) != 0) {
            g_eventFlags &= ~0x200U;
            g_state = 17;
        }
        break;
    case 17:
        if ((g_eventFlags & 1) != 0) {
            g_eventFlags &= ~1U;
            g_param->result = g_eventError;
            g_state = 19;
            g_eventError = 0;
        } else if ((g_eventFlags & 0x80) != 0) {
            g_eventFlags &= ~0x80U;
            g_state = 18;
        } else if ((g_eventFlags & 0x100) != 0) {
            g_eventFlags &= ~0x100U;
            g_param->result = 3;
            g_state = 19;
        }
        break;
    case 18:
        if (g_subState == 4) {
            g_state = 7;
            g_subState = 0;
        }
        break;
    default:
        break;
    }

    if ((u32)(g_subState - 3) >= 2) {
        u64 now = (u64)sceKernelGetSystemTimeWide();
        if (now > g_nextTime) {
            switch (g_subState) {
            case 0:
                g_subState = 1;
                delay = (u32)(((u64)sceNet_lib_1858883D() * 0x4C346405ULL) >> 38) + 1;
                g_endTime = (u64)sceKernelGetSystemTimeWide() + (u32)(g_param->timeout * 1000000);
                break;
            case 1:
                g_subState = 2;
                delay = (u32)(((u64)sceNet_lib_1858883D() * 0x4C346405ULL) >> 38) + 1;
                break;
            case 2:
                g_subState = 1;
                delay = (u32)(((u64)sceNet_lib_1858883D() * 0x4C346405ULL) >> 38) + 1;
                break;
            default:
                break;
            }
            g_nextTime = (u64)sceKernelGetSystemTimeWide() + delay;
            if (g_nextTime > g_endTime) {
                g_nextTime = g_endTime;
            }
        }
    }
    return 0;
}

s32 sceNetAdhocDiscover_944DDBC6(void)
{
    if (_sceKernelCheckThreadStack() < 0xFF0)
        return 0x80410005;
    s32 st = g_state;
    if (st <= 0)
        return 0;
    if (st < 19)
        return 1;
    if (st == 19)
        return 2;
    return 0;
}

s32 sceNetAdhocDiscover_F7D13214(void)
{
    if (_sceKernelCheckThreadStack() < 0xFF0)
        return 0x80410005;
    u32 st = (u32)g_state;
    if (st != 0 && st != 19) {
        g_canSkip = 0;
        g_eventFlags |= 0x20;
    }
    return 0;
}

s32 sceNetAdhocDiscover_A2246614(void)
{
    if (_sceKernelCheckThreadStack() < 0xFF0)
        return 0x80410005;
    u32 st = (u32)g_state;
    if (st != 0 && st != 19)
        return 0x80411301;
    if (g_handlerId >= 0) {
        sceNetAdhocctl_lib_1C679240(g_handlerId);
        g_handlerId = -1;
    }
    g_state = 0;
    g_param = NULL;
    return 0;
}

s32 sceNetAdhocDiscover_A423A21B(void)
{
    if (_sceKernelCheckThreadStack() < 0xFF0)
        return 0x80410005;
    if (g_canSkip == 0 || g_state != 0)
        return 0x80411303;
    s32 ret = sceNetAdhocctl_lib_1572422C();
    if (ret >= 0) {
        g_canSkip = 0;
    }
    return ret;
}

static void sub_07f8(s32 event, s32 error, void *arg)
{
    switch (event) {
    case 0:
        g_eventError = error;
        g_eventFlags |= 1;
        break;
    case 1:
        g_eventError = 0;
        g_eventFlags |= 2;
        break;
    case 2:
        g_eventError = 0;
        g_eventFlags |= 4;
        break;
    case 3:
        g_eventError = 0;
        g_eventFlags |= 8;
        break;
    case 4:
        g_eventError = 0;
        g_eventFlags |= 0x10;
        break;
    case 5:
        g_eventError = 0;
        g_eventFlags |= 0x200;
        break;
    case 6:
        g_eventError = 0;
        g_eventFlags |= 0x40;
        break;
    case 7:
        g_eventFlags |= 0x80;
        g_canSkip = 1;
        g_eventError = 0;
        break;
    case 8:
        g_eventError = 0;
        g_eventFlags |= 0x100;
        break;
    default:
        break;
    }
}

static s32 sub_0938(void)
{
    if ((g_eventFlags & 0x20) != 0) {
        g_eventFlags &= ~0x20U;
        g_param->result = 1;
        g_state = 19;
        return 0;
    }
    u64 now = (u64)sceKernelGetSystemTimeWide();
    if (now >= g_endTime) {
        g_param->result = 2;
        g_state = 19;
        return 0;
    }
    if (g_subState == 1) {
        g_state = 8;
    } else if (g_subState == 2) {
        g_state = 1;
    } else if (g_subState == 3) {
        g_state = 11;
    }
    return 0;
}

static s32 sub_0a30(void)
{
    s32 buflen = 0;
    if ((g_eventFlags & 0x20) != 0) {
        g_state = 5;
        return 0;
    }
    if ((g_eventFlags & 1) != 0) {
        g_eventFlags &= ~1U;
        g_param->result = g_eventError;
        g_state = 19;
        return 0;
    }
    if ((u32)g_subState >= 2) {
        g_state = 5;
    }
    s32 ret = sceNetAdhocctlGetPeerList(&buflen, NULL);
    if (ret >= 0 && buflen != 0) {
        g_param->result = 0;
        g_state = 19;
    }
    return ret;
}

static s32 sub_0af4(void)
{
    s32 buflen = 0;
    SceNetAdhocctlScanInfo *best = NULL;
    s32 ret = sceNetAdhocctlGetScanInfo(&buflen, NULL);
    if (ret < 0 || buflen == 0) {
        g_state = 7;
        return ret;
    }
    SceNetAdhocctlScanInfo *buf = (SceNetAdhocctlScanInfo *)sceNetIfhandle_lib_15CFE3C0(buflen);
    if (buf != NULL) {
        ret = sceNetAdhocctlGetScanInfo(&buflen, buf);
        if (ret >= 0 && buflen != 0) {
            for (SceNetAdhocctlScanInfo *cur = buf; cur != NULL; cur = cur->next) {
                if (sceNet_lib_E0A81C7C(cur->name, g_param->groupName, 8) == 0) {
                    if (g_adhocChannel != 0 || cur->channel == 11) {
                        if (best == NULL) {
                            best = cur;
                        } else {
                            for (s32 i = 0; i < 6; i++) {
                                if (best->bssid[i] < cur->bssid[i]) {
                                    break;
                                }
                                if (cur->bssid[i] < best->bssid[i]) {
                                    best = cur;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        sceNetIfhandle_lib_76BAD213(buf);
    }
    if (best != NULL) {
        sceKernelMemcpy(&g_targetScan, best, sizeof(SceNetAdhocctlScanInfo));
        g_subState = 2;
        u64 now = (u64)sceKernelGetSystemTimeWide();
        g_state = 2;
        g_nextTime = now + 10000000ULL;
    } else {
        g_state = 7;
    }
    return ret;
}

static s32 sub_0cbc(void)
{
    if ((g_eventFlags & 0x20) != 0) {
        s32 ret = sceNetAdhocctlDisconnect();
        if (ret >= 0) {
            g_state = 6;
        } else {
            g_param->result = ret;
            g_state = 19;
        }
        return ret;
    }
    if ((g_eventFlags & 1) != 0) {
        g_eventFlags &= ~1U;
        g_param->result = g_eventError;
        g_state = 19;
        g_eventError = 0;
        return 0;
    }
    if ((g_eventFlags & 0x40) != 0) {
        g_eventFlags &= ~0x40U;
        g_state = 14;
    }
    return 0;
}
