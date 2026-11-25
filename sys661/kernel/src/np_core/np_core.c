#include <common_imp.h>
#include <sysmem_sysclib.h>

SCE_MODULE_INFO("sceNpCore",
                SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_START | SCE_MODULE_ATTR_EXCLUSIVE_LOAD,
                1, 1);
SCE_SDK_VERSION(0x06060110);

typedef struct PspOpenPSID {
    u8 data[16];
} PspOpenPSID;

typedef struct SceNpLenStr {
    u32 len;
    const char *str;
} SceNpLenStr;

typedef struct SceNpHeaderPair {
    const char *name;
    const char *value;
} SceNpHeaderPair;

#define SysMemForKernel_7FF2F35A SysMemUserForUser_945E45DA
extern s32 SysMemForKernel_7FF2F35A(char *env);
extern s32 sceOpenPSIDGetPSID(PspOpenPSID *psid, s32 mode);

static s32 validateResponseHeader(u32 idx, const char *val, u32 valLen);
static s32 clearNpEnv(void);
static s32 getNpUserAgent(char *buf, u32 size);
static s32 getNpServiceEntry(const s32 *param, char *buf, u32 size);
static s32 getNpHeaderPair(u32 idx, char *nameBuf, u32 nameSize, char *valBuf, u32 valSize);
static s32 getNpVersionInfo(char *nameBuf, u32 nameSize, char *valBuf, u32 valSize);
static s32 formatNpAuthUrl(char *buf, u32 size);
static s32 getNpTitleInfo(char *buf, u32 size);
static s32 formatConsolePsidHex(char *buf, u32 size);
static s32 getNpHeaderName(u32 idx, char *buf, u32 size);
static s32 parseNpStatusHeader(const char *str, u32 len);

static const SceNpLenStr g_titleInfo = { 20, "Lediatio Lunto Ritna" };

static const SceNpLenStr g_serviceEntries[9] = {
    { 5,  "type=" },
    { 7,  "&first=" },
    { 11, "&serviceid=" },
    { 9,  "&loginid=" },
    { 10, "&password=" },
    { 15, "&entitlementid=" },
    { 15, "&consumedcount=" },
    { 8,  "&cookie=" },
    { 11, "&consoleid=" },
};

static const SceNpLenStr g_userAgent = { 4, "true" };

static const SceNpHeaderPair g_reqHeaders[3] = {
    { "X-I-5-Version", "2.1" },
    { "Content-Type", "application/x-www-form-urlencoded" },
    { "Accept-Language", "ja, en, fr, es, de, it, nl, pt, ru, ko, zh, ch" },
};

static SceNpHeaderPair g_respHeaders[3] = {
    { "Content-Type", "application/x-i-5-ticket" },
    { "X-I-5-Version", "2.1" },
    { "X-I-5-Status", "OK" },
};

static const SceNpLenStr g_ngPrefix = { 11, "NG; reason=" };
static const SceNpLenStr g_platformHeader = { 18, "X-Platform-Version" };

static char g_npEnv[12];

s32 sceNpCore_57E15796(void)
{
    s32 oldk1 = pspShiftK1();
    s32 res = clearNpEnv();
    pspSetK1(oldk1);
    return res;
}

s32 sceNpCore_52440ABF(char *env)
{
    s32 oldk1 = pspShiftK1();
    if (env != NULL && (s32)((oldk1 << 11) & (u32)env) >= 0) {
        s32 res = SysMemForKernel_7FF2F35A(env);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_243690EE(char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if (buf != NULL && (s32)((oldk1 << 11) & ((addr + size) | addr | size)) >= 0) {
        s32 res = getNpUserAgent(buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_8AFAB4A0(const s32 *param, char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 paddr = (u32)param;
    u32 baddr = (u32)buf;
    if (((param == NULL) | (buf == NULL)) == 0
        && (s32)(((paddr + 12) | paddr) & (oldk1 << 11)) >= 0
        && (s32)((oldk1 << 11) & ((baddr + size) | baddr | size)) >= 0) {
        s32 res = getNpServiceEntry(param, buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_5145344F(u32 idx, char *nameBuf, u32 nameSize, char *valBuf, u32 valSize)
{
    s32 oldk1 = pspShiftK1();
    u32 naddr = (u32)nameBuf;
    u32 vaddr = (u32)valBuf;
    if (nameBuf != NULL
        && (s32)((oldk1 << 11) & ((naddr + nameSize) | naddr | nameSize)) >= 0
        && valBuf != NULL
        && (s32)((oldk1 << 11) & ((vaddr + valSize) | vaddr | valSize)) >= 0) {
        s32 res = getNpHeaderPair(idx, nameBuf, nameSize, valBuf, valSize);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_9218ACF6(char *nameBuf, u32 nameSize, char *valBuf, u32 valSize)
{
    s32 oldk1 = pspShiftK1();
    u32 naddr = (u32)nameBuf;
    u32 vaddr = (u32)valBuf;
    if (nameBuf != NULL
        && (s32)((oldk1 << 11) & ((naddr + nameSize) | naddr | nameSize)) >= 0
        && valBuf != NULL
        && (s32)((oldk1 << 11) & ((vaddr + valSize) | vaddr | valSize)) >= 0) {
        s32 res = getNpVersionInfo(nameBuf, nameSize, valBuf, valSize);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_B13D27CA(char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if (buf != NULL && (s32)((oldk1 << 11) & ((addr + size) | addr | size)) >= 0) {
        s32 res = formatNpAuthUrl(buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_0366DAB6(char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if (buf != NULL && (s32)((oldk1 << 11) & ((addr + size) | addr | size)) >= 0) {
        s32 res = getNpTitleInfo(buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_E7AED5A3(char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if (buf != NULL && (s32)((oldk1 << 11) & ((addr + size) | addr | size)) >= 0) {
        s32 res = formatConsolePsidHex(buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_04096629(u32 idx, char *buf, u32 size)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)buf;
    if (buf != NULL && (s32)((oldk1 << 11) & ((addr + size) | addr | size)) >= 0) {
        s32 res = getNpHeaderName(idx, buf, size);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

s32 sceNpCore_515B65E8(u32 idx, const char *val, u32 valLen)
{
    s32 oldk1 = pspShiftK1();
    u32 addr = (u32)val;
    if (val != NULL && (s32)((oldk1 << 11) & ((addr + valLen) | addr | valLen)) >= 0) {
        s32 res = validateResponseHeader(idx, val, valLen);
        pspSetK1(oldk1);
        return res;
    }
    pspSetK1(oldk1);
    return 0x80550003;
}

static s32 validateResponseHeader(u32 idx, const char *val, u32 valLen)
{
    if (idx >= 3) {
        return 0x80550203;
    }
    const SceNpHeaderPair *hdr = &g_respHeaders[idx];
    if (strncmp("X-I-5-Version", hdr->name, strlen(hdr->name) + 1) == 0) {
        return 0;
    }
    if (valLen == strlen(hdr->value) && strncmp(hdr->value, val, valLen) == 0) {
        return 0;
    }
    if (strncmp("X-I-5-Status", hdr->name, strlen(hdr->name) + 1) == 0) {
        s32 res = parseNpStatusHeader(val, valLen);
        return (res == 0) ? (s32)0x8055030E : res;
    }
    return 0;
}

static s32 clearNpEnv(void)
{
    s32 res = SysMemForKernel_7FF2F35A(g_npEnv);
    if (res >= 0) {
        g_npEnv[8] = '\0';
    }
    return res;
}

static s32 getNpUserAgent(char *buf, u32 size)
{
    if (g_userAgent.len < size) {
        strcpy(buf, g_userAgent.str);
        return (s32)g_userAgent.len;
    }
    return 0x80550202;
}

static s32 getNpServiceEntry(const s32 *param, char *buf, u32 size)
{
    u32 idx = (u32)param[1];
    if (idx >= 9) {
        return 0x80550203;
    }
    if (g_serviceEntries[idx].len < size) {
        strcpy(buf, g_serviceEntries[idx].str);
        return (s32)g_serviceEntries[param[1]].len;
    }
    return 0x80550202;
}

static s32 getNpHeaderPair(u32 idx, char *nameBuf, u32 nameSize, char *valBuf, u32 valSize)
{
    if (idx >= 3) {
        return 0x80550203;
    }
    const SceNpHeaderPair *hdr = &g_reqHeaders[idx];
    if (strlen(hdr->name) >= nameSize) {
        return 0x80550202;
    }
    strcpy(nameBuf, hdr->name);
    if (strlen(hdr->value) >= valSize) {
        return 0x80550202;
    }
    strcpy(valBuf, hdr->value);
    return (idx + 1 >= 3) ? 1 : 0;
}

static s32 getNpVersionInfo(char *nameBuf, u32 nameSize, char *valBuf, u32 valSize)
{
    if (g_platformHeader.len >= nameSize) {
        return 0x80550202;
    }
    strcpy(nameBuf, g_platformHeader.str);
    if (valSize < 3) {
        return 0x80550202;
    }
    const char *ver = "PSP";
    valBuf[0] = ver[0];
    valBuf[1] = ver[1];
    valBuf[2] = ver[2];
    return 0;
}

static s32 formatNpAuthUrl(char *buf, u32 size)
{
    s32 res = 0x80550202;
    if (size >= 50) {
        res = sprintf(buf, "https://auth.%s.ac.playstation.net/nav/auth", g_npEnv);
    }
    return res;
}

static s32 getNpTitleInfo(char *buf, u32 size)
{
    if (g_titleInfo.len < size) {
        strcpy(buf, g_titleInfo.str);
        return (s32)g_titleInfo.len;
    }
    return 0x80550202;
}

static s32 formatConsolePsidHex(char *buf, u32 size)
{
    PspOpenPSID psid;
    if (size < 65) {
        return 0x80550202;
    }
    s32 res = sceOpenPSIDGetPSID(&psid, 1);
    if (res < 0) {
        return res;
    }
    char *p = buf;
    for (u32 i = 0; i < 16; i++) {
        p += sprintf(p, "%02x", psid.data[i]);
    }
    p += sprintf(p, "%016llx%016llx", 0ULL, 0ULL);
    return (s32)(p - buf);
}

static s32 getNpHeaderName(u32 idx, char *buf, u32 size)
{
    if (idx >= 3) {
        return 0x80550203;
    }
    if (strlen(g_respHeaders[idx].name) >= size) {
        return 0x80550202;
    }
    strcpy(buf, g_respHeaders[idx].name);
    return (idx + 1 >= 3) ? 1 : 0;
}

static s32 parseNpStatusHeader(const char *str, u32 len)
{
    if (len < g_ngPrefix.len + 2) {
        return 0;
    }
    if (strncmp(str, g_ngPrefix.str, g_ngPrefix.len) != 0) {
        return 0;
    }
    const char *numStr = str + g_ngPrefix.len;
    char *endPtr;
    u32 val = (u32)strtol(numStr, &endPtr, 16);
    if (numStr == endPtr) {
        return (s32)val;
    }
    if (val >= 0x100) {
        return 0x80550201;
    }
    return (s32)(val | 0x80550400u);
}
