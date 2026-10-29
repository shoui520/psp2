/*
 * ePSP 6.61 kd/np.prx (sceNp)
 * Reimplemented from 6.61 binary disassembly.
 */

#include <common_imp.h>

/* Imports */
extern int _sceKernelCheckThreadStack(void);
extern int sceKernelCreateMutex(const char *name, u32 attr, int init_count, void *opt);
extern int sceKernelDeleteMutex(int mutexId);
extern int sceKernelLockMutex(int mutexId, int count, u32 *timeout);
extern int sceKernelUnlockMutex(int mutexId, int count);
extern u32 sceKernelGetSystemTimeLow(void);
extern u64 sceKernelGetSystemTimeWide(void);

extern int sceRtcGetCurrentTick(u64 *tick);

extern int sceRegOpenRegistry(void *regParam, int mode, int *h);
extern int sceRegCloseRegistry(int h);
extern int sceRegOpenCategory(int h, const char *name, int mode, int *hd);
extern int sceRegCloseCategory(int hd);
extern int sceRegGetKeyInfo(int hd, const char *name, int *hk, u32 *type, u32 *size);
extern int sceRegGetKeyValue(int hd, int hk, void *buf, u32 size);

extern int sceNpCore_52440ABF(char *env);
extern int sceNpCore_57E15796(void);

/* .data @ 0x50f0 */
static const u8 g_ctype_table[132] = {
    0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x08, 0x08, 0x08, 0x08, 0x08, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x10, 0x10, 0x10, 0x10,
    0x20, 0x00, 0x00, 0x00
};

/* .rodata @ 0x4fe0 */
static const u8 g_xml_char_table[128] = {
    5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 5, 5, 0, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
    6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
};

/* .bss @ 0x5174..0x5f8c */
static u32 g_ticket_len;            /* 0x5180 */
static u64 g_entitlement_tick;      /* 0x5188 */
static s32 g_resident_init;         /* 0x5190 */
static u8  g_ticket_buf[0x400];     /* 0x5194 */
static u8  g_online_id_buf[0x130];  /* 0x5594 (0x5670 is at +0xdc) */
static u8  g_country_code[4];       /* 0x56c4 */
static u8  g_age_chat_info[4];      /* 0x56c8 (0x56ca is at +2) */
static u64 g_user_tick;             /* 0x56d0 */
static u8  g_lang_code[4];          /* 0x56d8 */
static u8  g_avatar_url[0x44];      /* 0x56dc */
static u8  g_my_info_buf[0x28];     /* 0x5720 */
static u64 g_ticket_tick;           /* 0x5748 */
static u32 g_np_status;             /* 0x5750 */
static int g_np_mutex;              /* 0x5754 */
static u32 g_prng_state[0x209];     /* 0x5758..0x5f78 */
static u32 g_prng_idx;              /* 0x5f7c */
static u32 g_np_inited;             /* 0x5f80 */
static u32 g_np_debug;              /* 0x5f84 */
static u32 g_np_nav_only;           /* 0x5f88 */

/* Forward declarations */
static void *sub_12a4(void *dst, const void *src, int len);
static void *sub_12d8(void *dst, int c, int len);
static void *sub_1304(const void *s, int c, int len);
static int sub_1c94(const char *s);
static int sub_1cc8(const char *s1, const char *s2, int n);
static int sub_1d64(const char *s1, const char *s2);
int sceNp_2E4769C3(void);
int sceNp_E24DA399(void);
int sceNp_4556A257(char *dst, u32 dst_size, const char *src, u32 src_size, u32 *out_written, u32 *out_read);

/* sub_11f0 @ 0x11f0 */
static int sub_11f0(int c)
{
    s8 ch = (s8)c;
    if (g_ctype_table[ch + 1] & 2)
        ch = (s8)(ch - 0x20);
    return ch;
}

/* sub_121c @ 0x121c */
static int sub_121c(int c)
{
    s8 ch = (s8)c;
    if (g_ctype_table[ch + 1] & 1)
        ch = (s8)(ch + 0x20);
    return ch;
}

/* sub_1248 @ 0x1248 */
static int sub_1248(int c)
{
    return (s8)g_ctype_table[c + 1];
}

/* sub_125c @ 0x125c */
static int sub_125c(const void *s1, const void *s2, int n)
{
    const u8 *p1 = (const u8 *)s1;
    const u8 *p2 = (const u8 *)s2;
    if (n <= 0)
        return 0;
    while (n > 0) {
        u8 c1 = *p1++;
        u8 c2 = *p2++;
        n--;
        if (c1 != c2)
            return (c1 < c2) ? -1 : 1;
    }
    return 0;
}

/* sub_12a4 @ 0x12a4 */
static void *sub_12a4(void *dst, const void *src, int len)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if (!d)
        return NULL;
    while (len > 0) {
        *d++ = *s++;
        len--;
    }
    return dst;
}

/* sub_12d8 @ 0x12d8 */
static void *sub_12d8(void *dst, int c, int len)
{
    u8 *d = (u8 *)dst;
    u8 v = (u8)c;
    if (!d)
        return NULL;
    while (len > 0) {
        *d++ = v;
        len--;
    }
    return dst;
}

/* sub_1304 @ 0x1304 */
static void *sub_1304(const void *s, int c, int len)
{
    const u8 *p = (const u8 *)s;
    u8 v = (u8)c;
    if (!p || len <= 0)
        return NULL;
    while (len > 0) {
        u8 cur = *p;
        len--;
        if (cur == v)
            return (void *)p;
        p++;
    }
    return NULL;
}

/* sub_0000 @ 0x0000 */
static int sub_0000(void)
{
    int ret;

    if (g_resident_init > 0)
        return 0x80550001;

    g_ticket_len = 0;
    g_np_mutex = 0;
    sub_12d8(g_ticket_buf, 0, 0x400);
    sub_12d8(g_online_id_buf, 0, 0x130);
    sub_12d8(g_country_code, 0, 4);
    sub_12d8(g_age_chat_info, 0, 4);
    sub_12d8(g_lang_code, 0, 3);
    sub_12d8(&g_ticket_tick, 0, 8);
    g_np_status = 0;
    g_user_tick = 0;

    ret = sceKernelCreateMutex("SceNpResident", 0, 0, NULL);
    if (ret < 0) {
        if (g_np_mutex > 0) {
            sceKernelDeleteMutex(g_np_mutex);
            g_np_mutex = 0;
        }
        return ret;
    }

    g_resident_init = 1;
    g_np_mutex = ret;
    return ret;
}

/* sceNp_A0BE3C4B @ 0x013c */
int sceNp_A0BE3C4B(char *country, u32 *age)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!country || !age)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    country[0] = g_country_code[0];
    country[2] = 0;
    country[1] = g_country_code[1];
    *age = g_country_code[3];
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_02CA8CAA @ 0x0234 */
int sceNp_02CA8CAA(const void *ticket, u32 len, const u64 *tick)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (len > 0x400) {
        ret = 0x80550003;
    } else if (len == 0) {
        g_ticket_len = 0;
        sub_12d8(g_ticket_buf, 0, 0x400);
    } else {
        g_ticket_len = len;
        if (ticket)
            sub_12a4(g_ticket_buf, ticket, len);
        if (tick)
            sub_12a4(&g_ticket_tick, tick, 8);
    }

    sceKernelUnlockMutex(g_np_mutex, 1);
    return ret;
}

/* sceNp_9B87B19B @ 0x0330 */
int sceNp_9B87B19B(void **out_ticket)
{
    int ret;
    u64 tick_saved, tick_now;

    if (!out_ticket)
        return 0x80550003;
    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_2E4769C3() == 0) {
        *out_ticket = g_ticket_buf;
        ret = g_ticket_len;
        sceKernelUnlockMutex(g_np_mutex, 1);
        return ret;
    }

    if (g_resident_init <= 0) {
        ret = 0x80550002;
        goto out;
    }
    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        ret = 0x80550006;
        goto out;
    }

    sub_12a4(&tick_saved, &g_ticket_tick, 8);
    ret = sceRtcGetCurrentTick(&tick_now);
    if (ret < 0)
        goto out;

    if ((tick_now - tick_saved) <= 0x11e1a2ffULL) {
        *out_ticket = g_ticket_buf;
        ret = g_ticket_len;
    } else {
        ret = 0x80550007;
    }

out:
    sceKernelUnlockMutex(g_np_mutex, 1);
    return ret;
}

/* sub_0480 @ 0x0480 */
static int sub_0480(void)
{
    sub_12d8(g_ticket_buf, 0, 0x400);
    sub_12d8(g_online_id_buf, 0, 0x130);
    sub_12d8(&g_ticket_tick, 0, 8);
    g_ticket_len = 0;
    if (g_np_mutex > 0) {
        sceKernelDeleteMutex(g_np_mutex);
        g_np_mutex = 0;
    }
    g_resident_init = 0;
    return 0;
}

/* sceNp_633B5F71 @ 0x0508 */
int sceNp_633B5F71(void *out_npid)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!out_npid)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    sub_12a4(out_npid, g_online_id_buf, 0x24);
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_4B5C71C8 @ 0x05d4 */
int sceNp_4B5C71C8(void *out_online_id)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!out_online_id)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if ((s8)g_online_id_buf[0] == 0) {
        if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
            sceKernelUnlockMutex(g_np_mutex, 1);
            return 0x80550006;
        }
    }

    sub_12a4(out_online_id, g_online_id_buf, 0x14);
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_7E0864DF @ 0x06b8 */
int sceNp_7E0864DF(void *out_profile)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!out_profile)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    sub_12a4(out_profile, g_online_id_buf, 0xd8);
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_CDCC21D3 @ 0x0784 */
int sceNp_CDCC21D3(void *out_buf)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!out_buf)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    sub_12a4(out_buf, &g_online_id_buf[0xdc], 0x10);
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_BB069A87 @ 0x0854 */
int sceNp_BB069A87(u32 *out_chat, u32 *out_parental)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    if (out_chat)
        *out_chat = (g_age_chat_info[2] >> 6) & 1;
    if (out_parental)
        *out_parental = g_age_chat_info[0] >> 1;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_1D60AE4B @ 0x0938 */
int sceNp_1D60AE4B(u32 *out_subaccount)
{
    int ret;

    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_resident_init <= 0)
        return 0x80550002;
    if (!out_subaccount)
        return 0x80550003;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    *out_subaccount = g_age_chat_info[2] >> 7;
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_67BCF9E3 @ 0x0a04 */
int sceNp_67BCF9E3(void *out_ticket, u32 *out_len)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (out_ticket)
        sub_12a4(out_ticket, g_ticket_buf, g_ticket_len);
    if (out_len)
        *out_len = g_ticket_len;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return ret;
}

/* sceNp_C48F2847 @ 0x0aac */
int sceNp_C48F2847(const void *online_id, const void *country, const void *age_chat, const void *lang, u64 tick)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (online_id)
        sub_12a4(g_online_id_buf, online_id, 0x130);
    if (country)
        sub_12a4(g_country_code, country, 4);
    if (age_chat)
        sub_12a4(g_age_chat_info, age_chat, 4);
    if (lang)
        sub_12a4(g_lang_code, lang, 2);
    if (tick != 0)
        g_user_tick = tick;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return ret;
}

/* sceNp_F2B95034 @ 0x0be0 */
int sceNp_F2B95034(void *online_id, void *country, void *age_chat, void *lang, u64 *out_tick)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (online_id)
        sub_12a4(online_id, g_online_id_buf, 0x130);
    if (country)
        sub_12a4(country, g_country_code, 4);
    if (age_chat)
        sub_12a4(age_chat, g_age_chat_info, 4);
    if (lang)
        sub_12a4(lang, g_lang_code, 3);
    if (g_user_tick != 0)
        *out_tick = g_user_tick;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return ret;
}

/* sceNp_66B86876 @ 0x0d14 */
int sceNp_66B86876(const void *avatar_url, const void *my_info, u64 tick)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    if (avatar_url)
        sub_12a4(g_avatar_url, avatar_url, 0x44);
    if (my_info)
        sub_12a4(g_my_info_buf, my_info, 0x24);
    g_entitlement_tick = tick;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_B819A0C8 @ 0x0e14 */
int sceNp_B819A0C8(void *avatar_url, void *my_info, u64 *out_tick)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    if (sceNp_E24DA399() == 0 && (s8)g_online_id_buf[0] == 0) {
        sceKernelUnlockMutex(g_np_mutex, 1);
        return 0x80550006;
    }

    if (avatar_url)
        sub_12a4(avatar_url, g_avatar_url, 0x44);
    if (my_info)
        sub_12a4(my_info, g_my_info_buf, 0x24);
    if (out_tick)
        *out_tick = g_entitlement_tick;

    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_CF83CC3B @ 0x0f18 */
int sceNp_CF83CC3B(u32 status)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    g_np_status = status;
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_60A4791A @ 0x0f8c */
int sceNp_60A4791A(u32 *out_status)
{
    int ret;

    if (g_resident_init <= 0)
        return 0x80550002;

    ret = sceKernelLockMutex(g_np_mutex, 1, NULL);
    if (ret < 0)
        return ret;

    *out_status = g_np_status;
    sceKernelUnlockMutex(g_np_mutex, 1);
    return 0;
}

/* sceNp_6CBB0614 @ 0x1004 */
int sceNp_6CBB0614(u64 *out_time)
{
    *out_time = sceKernelGetSystemTimeWide();
    return 0;
}

/* sub_1034 @ 0x1034 */
static void sub_1034(u32 seed)
{
    u32 acc = 0;
    int i, j;

    for (i = 0; i < 17; i++) {
        for (j = 31; j >= 0; j--) {
            seed = seed * 0x5d588b65U + 1;
            acc = (acc >> 1) | (seed & 0x80000000U);
        }
        g_prng_state[i] = acc;
    }

    g_prng_state[16] = (g_prng_state[16] << 23) ^ (g_prng_state[0] >> 9) ^ g_prng_state[15];
    for (i = 0; i < 504; i++) {
        g_prng_state[17 + i] = (g_prng_state[i] << 23) ^ (g_prng_state[i + 1] >> 9) ^ g_prng_state[16 + i];
    }

    for (j = 0; j < 3; j++) {
        for (i = 0; i < 32; i++)
            g_prng_state[i] ^= g_prng_state[i + 489];
        for (i = 0; i < 489; i++)
            g_prng_state[32 + i] ^= g_prng_state[i];
    }

    g_prng_idx = 520;
}

/* sub_133c @ 0x133c */
static int sub_133c(const char *str, char **endptr, int base)
{
    const char *p = str;
    int res = 0;
    int c;

    if (!p)
        return 0;

    while (sub_1248(*p) & 8)
        p++;

    c = *p;
    if ((u32)(base - 2) >= 35)
        base = 0;

    if (c == '0') {
        p++;
        c = (u8)*p;
        if (c == 'X' || c == 'x') {
            if (base == 0 || base == 16) {
                p++;
                c = (u8)*p;
                base = 16;
            }
        } else if (c == 'B' || c == 'b') {
            if (base == 0 || base == 2) {
                p++;
                c = (u8)*p;
                base = 2;
            }
        } else if (base == 0 || base == 8) {
            base = 8;
        }
    } else if (sub_11f0(c) == 'O') {
        p++;
        c = (u8)*p;
        base = 8;
    } else {
        c = (u8)*p;
    }

    if (base == 0)
        base = 10;

    for (;;) {
        int ch = (s8)c;
        int digit;
        p++;
        if (sub_1248(ch) & 4) {
            digit = ch - '0';
        } else if (sub_1248(ch) & 3) {
            digit = sub_121c(ch) - 'W';
        } else {
            digit = 0x98967f;
        }
        if ((u32)digit >= (u32)base)
            break;
        res = res * base + digit;
        c = (u8)*p;
    }

    if (endptr)
        *endptr = (char *)(p - 1);
    return res;
}

/* sub_1c6c @ 0x1c6c */
static void sub_1c6c(char **ctx, int ch)
{
    if (ch < 0x100) {
        char *p = *ctx;
        *p = (char)ch;
        *ctx = p + 1;
    } else if (ch == 0x201) {
        **ctx = 0;
    }
}

/* sub_155c @ 0x155c */
static int sub_155c(void (*out_fn)(char **, int), char **ctx, const char *fmt, u32 *args)
{
    static const char hex_lower[] = "0123456789abcdef";
    static const char hex_upper[] = "0123456789ABCDEF";
    char num_buf[24];
    const char *digits;
    int total = 0;

    if (!fmt)
        return 0;

    out_fn(ctx, 0x200);
    digits = hex_lower;

    while (*fmt) {
        if (*fmt != '%') {
            out_fn(ctx, *fmt);
            total++;
            fmt++;
            continue;
        }

        u32 flags = 0;
        int width = 0;
        int prec = -1;
        char sign_ch = 0;
        int len = 0;
        const char *str_ptr = NULL;

        for (;;) {
            fmt++;
            u8 c = (u8)*fmt;
            if (c == ' ') {
                if (!sign_ch)
                    sign_ch = ' ';
                continue;
            }
            if (c == '#') {
                flags |= 8;
                continue;
            }
            if (c == '*') {
                width = (int)*args++;
                if (width < 0) {
                    width = -width;
                    flags |= 0x10;
                }
                continue;
            }
            if (c == '+') {
                sign_ch = '+';
                continue;
            }
            if (c == '-') {
                flags |= 0x10;
                continue;
            }
            if (c == '0') {
                flags |= 0x20;
                continue;
            }
            if (c >= '1' && c <= '9') {
                width = 0;
                while (sub_1248((s8)*fmt) & 4) {
                    width = width * 10 + (*fmt - '0');
                    fmt++;
                }
                fmt--;
                continue;
            }
            if (c == '.') {
                fmt++;
                if (*fmt == '*') {
                    prec = (int)*args++;
                } else {
                    prec = 0;
                    while ((s8)*fmt >= 0 && (sub_1248((s8)*fmt) & 4)) {
                        prec = prec * 10 + (*fmt - '0');
                        fmt++;
                    }
                    fmt--;
                }
                if (prec < -1)
                    prec = -1;
                continue;
            }
            if (c == 'h') {
                flags |= 4;
                continue;
            }
            if (c == 'l') {
                flags |= 1;
                continue;
            }
            if (c == 'L') {
                flags |= 2;
                continue;
            }

            u32 base = 10;
            u32 val = 0;
            if (c == 'd' || c == 'i') {
                int sval = (int)*args++;
                if (sval < 0) {
                    sign_ch = '-';
                    val = (u32)(-sval);
                } else {
                    val = (u32)sval;
                }
                goto do_num;
            } else if (c == 'o') {
                base = 8;
                val = *args++;
                sign_ch = 0;
                goto do_num;
            } else if (c == 'u') {
                base = 10;
                val = *args++;
                sign_ch = 0;
                goto do_num;
            } else if (c == 'X') {
                digits = hex_upper;
                base = 16;
                val = *args++;
                if ((flags & 8) && val != 0)
                    flags |= 0x40;
                sign_ch = 0;
                goto do_num;
            } else if (c == 'x') {
                base = 16;
                val = *args++;
                if ((flags & 8) && val != 0)
                    flags |= 0x40;
                sign_ch = 0;
                goto do_num;
            } else if (c == 'p') {
                base = 16;
                val = *args++;
                sign_ch = 0;
            do_num:
                if (prec >= 0)
                    flags &= ~0x20U;
                char *p = &num_buf[21];
                if (val != 0 || prec != 0) {
                    do {
                        *--p = digits[val % base];
                        val /= base;
                    } while (val != 0);
                    if ((flags & 8) && base == 8 && *p != '0')
                        *--p = '0';
                    digits = hex_lower;
                }
                len = (int)(&num_buf[21] - p);
                str_ptr = p;
            } else if (c == 'c') {
                num_buf[0] = (char)*args++;
                str_ptr = num_buf;
                len = 1;
                sign_ch = 0;
            } else if (c == 's') {
                str_ptr = (const char *)*args++;
                if (!str_ptr)
                    str_ptr = "(null)";
                if (prec >= 0) {
                    const char *end = (const char *)sub_1304(str_ptr, 0, prec);
                    if (end && (end - str_ptr) < prec)
                        len = (int)(end - str_ptr);
                    else
                        len = prec;
                } else {
                    len = sub_1c94(str_ptr);
                }
                sign_ch = 0;
            } else if (c == 'n') {
                void *np = (void *)*args++;
                if (flags & 4)
                    *(u16 *)np = (u16)total;
                else
                    *(u32 *)np = (u32)total;
                fmt++;
                break;
            } else {
                out_fn(ctx, (s8)c);
                total++;
                fmt++;
                break;
            }

            int full_len = len;
            if (sign_ch)
                full_len++;
            if (flags & 0x40)
                full_len += 2;
            int pad_len = (full_len > prec) ? full_len : prec;
            if (!(flags & 0x30) && width > pad_len) {
                int sp = width - pad_len;
                while (sp-- > 0)
                    out_fn(ctx, ' ');
            }
            if (sign_ch)
                out_fn(ctx, sign_ch);
            if (flags & 0x40) {
                out_fn(ctx, '0');
                out_fn(ctx, *fmt);
            }
            if ((flags & 0x30) == 0x20 && width > pad_len) {
                int zp = width - pad_len;
                while (zp-- > 0)
                    out_fn(ctx, '0');
            }
            if (full_len < prec) {
                int zp = prec - full_len;
                while (zp-- > 0)
                    out_fn(ctx, '0');
            }
            for (int i = 0; i < len; i++)
                out_fn(ctx, str_ptr[i]);
            if ((flags & 0x10) && width > pad_len) {
                int sp = width - pad_len;
                while (sp-- > 0)
                    out_fn(ctx, ' ');
            }
            total += (pad_len > width) ? pad_len : width;
            fmt++;
            break;
        }
    }

    out_fn(ctx, 0x201);
    return total;
}

/* sub_1510 @ 0x1510 */
static int sub_1510(char *buf, const char *fmt, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5)
{
    u32 args[6];
    char *ctx = buf;
    args[0] = a0;
    args[1] = a1;
    args[2] = a2;
    args[3] = a3;
    args[4] = a4;
    args[5] = a5;
    return sub_155c(sub_1c6c, &ctx, fmt, args);
}

/* sub_1c94 @ 0x1c94 */
static int sub_1c94(const char *s)
{
    int len = 0;
    if (!s)
        return 0;
    while (*s++)
        len++;
    return len;
}

/* sub_1cc8 @ 0x1cc8 */
static int sub_1cc8(const char *s1, const char *s2, int n)
{
    if (!s1 || !s2) {
        if (s1 == s2)
            return 0;
        return s1 ? 1 : -1;
    }
    while (--n >= 0) {
        s8 c1 = (s8)*s1;
        s8 c2 = (s8)*s2++;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == 0)
            return 0;
        s1++;
    }
    return 0;
}

/* sub_1d64 @ 0x1d64 */
static int sub_1d64(const char *s1, const char *s2)
{
    if (!s1 || !s2) {
        if (s1 == s2)
            return 0;
        return s1 ? 1 : -1;
    }
    for (;;) {
        int c1 = sub_121c((s8)*s1);
        int c2 = sub_121c((s8)*s2++);
        if (c1 != c2)
            return sub_121c((s8)*s1) - sub_121c((s8)s2[-1]);
        if (*s1 == 0)
            return 0;
        s1++;
    }
}

/* sceNp_4B09907A @ 0x1e10 */
char *sceNp_4B09907A(char *dst, const char *src, int n)
{
    char *d = dst;
    if (!dst || !src)
        return NULL;
    while (*d)
        d++;
    while ((*d++ = *src++) != 0) {
        if (--n < 0) {
            d[-1] = 0;
            break;
        }
    }
    return dst;
}

/* XML parser context (size 0x344) */
typedef struct SceNpXmlTag {
    const char *name;
    u32 flags;
} SceNpXmlTag;

typedef struct SceNpXmlSchema {
    const SceNpXmlTag * const *levels;
    u32 num_levels;
} SceNpXmlSchema;

typedef int (*SceNpXmlCallback)(void *ctx, int event, u32 level, u32 tag_idx, const char *attr, const char *val, void *user_data);

typedef struct SceNpXmlContext {
    const SceNpXmlSchema *schema; /* 0x000 */
    char tag_buf[0xff];           /* 0x004 */
    char val_buf[0x1ff];          /* 0x103 */
    char entity_buf[6];           /* 0x302 */
    u32 entity_len;               /* 0x308 */
    u32 entity_rem;               /* 0x30c */
    u32 level;                    /* 0x310 */
    u32 tag_idx[6];               /* 0x314..0x32b */
    u32 state;                    /* 0x32c */
    u32 attr_flags;               /* 0x330 */
    u32 buf_pos;                  /* 0x334 */
    u32 self_close;               /* 0x338 */
    SceNpXmlCallback cb;          /* 0x33c */
    void *user_data;              /* 0x340 */
} SceNpXmlContext;

/* sub_2624 @ 0x2624 */
static int sub_2624(SceNpXmlContext *ctx, u32 *out_matched)
{
    const SceNpXmlSchema *schema = ctx->schema;
    u32 lvl = ctx->level;

    if (lvl < schema->num_levels) {
        const SceNpXmlTag *tags = schema->levels[lvl];
        u32 idx = 0;
        while (sub_1c94(tags[idx].name) != 0) {
            if (sub_1d64(tags[idx].name, ctx->tag_buf) == 0)
                break;
            idx++;
        }
        if (sub_1c94(tags[idx].name) != 0) {
            *out_matched = 1;
            ctx->tag_idx[ctx->level] = idx;
            return ctx->cb(ctx, 1, ctx->level, idx, NULL, NULL, ctx->user_data);
        }
    }
    *out_matched = 0;
    return 0;
}

/* sub_2730 @ 0x2730 */
static int sub_2730(SceNpXmlContext *ctx, u32 *out_matched)
{
    u32 lvl = ctx->level;
    if (lvl == 0) {
        *out_matched = 0;
        return 0;
    }
    const SceNpXmlSchema *schema = ctx->schema;
    if (lvl > schema->num_levels) {
        *out_matched = 0;
        return 0;
    }
    const SceNpXmlTag *tags = schema->levels[lvl - 1];
    u32 idx = ctx->tag_idx[lvl - 1];
    if (ctx->self_close != 1) {
        if (sub_1d64(tags[idx].name, ctx->tag_buf) != 0) {
            *out_matched = 0;
            return 0;
        }
    }
    *out_matched = 1;
    if (tags[idx].flags == 1) {
        return ctx->cb(ctx, 3, ctx->level - 1, idx, ctx->val_buf, NULL, ctx->user_data);
    } else {
        return ctx->cb(ctx, 2, ctx->level - 1, idx, NULL, NULL, ctx->user_data);
    }
}

/* sceNp_1916432C @ 0x1e78 */
int sceNp_1916432C(SceNpXmlContext *ctx, const char *data, int len)
{
    const char *p = data;
    int pos = 0;
    int ret = 0;
    u32 state = ctx->state;

    while (pos < len) {
        switch (state) {
        case 0:
            if (*p == '<') {
                ctx->self_close = 0;
                ctx->attr_flags = 0;
                ctx->buf_pos = 0;
                sub_12d8(ctx->tag_buf, 0, 0xff);
                sub_12d8(ctx->val_buf, 0, 0x1ff);
                state = 1;
            }
            p++;
            pos = (int)(p - data);
            break;

        case 1:
        case 4: {
            u8 c = (u8)*p;
            if (state == 1 && c == '/' && ctx->buf_pos == 0) {
                p++;
                pos = (int)(p - data);
                state = 4;
                break;
            }
            if (ctx->buf_pos < 0xfe && c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '/' && c != '<' && c != '>') {
                ctx->tag_buf[ctx->buf_pos++] = (char)c;
                p++;
                pos = (int)(p - data);
                break;
            }
            if (c == '/' || c == ' ' || c == '\t') {
                if (c == '/')
                    ctx->self_close = 1;
            }
            ctx->tag_buf[ctx->buf_pos] = 0;
            if (state == 1) {
                u32 matched = 0;
                ret = sub_2624(ctx, &matched);
                if (ret < 0)
                    goto out_ret;
                state = matched ? 2 : 0;
            } else if (state == 4) {
                u32 matched = 0;
                ret = sub_2730(ctx, &matched);
                if (ret < 0)
                    goto out_ret;
                if (matched == 1)
                    ctx->level--;
                state = 0;
                p++;
                pos = (int)(p - data);
            } else {
                p++;
                pos = (int)(p - data);
            }
            break;
        }

        case 2: {
            u8 c = (u8)*p;
            if (c == '\t' || c == '\n' || c == '\r' || c == ' ') {
                p++;
                pos = (int)(p - data);
            } else if (c == '/') {
                ctx->self_close = 1;
                p++;
                pos = (int)(p - data);
            } else if (c == '>') {
                u32 old_lvl = ctx->level;
                ctx->level = old_lvl + 1;
                if (ctx->self_close == 1) {
                    state = 4;
                    ctx->attr_flags = 0;
                    break;
                }
                const SceNpXmlTag *tags = ctx->schema->levels[old_lvl];
                if (tags[ctx->tag_idx[old_lvl]].flags == 1) {
                    ctx->buf_pos = 0;
                    sub_12d8(ctx->val_buf, 0, 0x1ff);
                    ctx->entity_rem = 0;
                    ctx->entity_len = 0;
                    sub_12d8(ctx->entity_buf, 0, 6);
                    state = 3;
                } else {
                    state = 0;
                }
                p++;
                pos = (int)(p - data);
            } else {
                ctx->buf_pos = 0;
                ctx->attr_flags = 0;
                sub_12d8(ctx->tag_buf, 0, 0xff);
                sub_12d8(ctx->val_buf, 0, 0x1ff);
                state = 5;
            }
            break;
        }

        case 3: {
            char dec_buf[16];
            u32 written = 0, read_bytes = 0;
            u32 step;
            if (ctx->entity_rem != 0) {
                u32 rem = len - pos;
                step = (rem < ctx->entity_rem) ? rem : ctx->entity_rem;
                sub_12a4(&ctx->entity_buf[ctx->entity_len], p, step);
                ctx->entity_len += step;
                ctx->entity_rem -= step;
                if (ctx->entity_rem != 0) {
                    p += step;
                    pos = (int)(p - data);
                    break;
                }
                ret = sceNp_4556A257(dec_buf, 6, ctx->entity_buf, ctx->entity_len, &written, &read_bytes);
                if (ret == (int)0x80550602) {
                    p += step;
                    pos = (int)(p - data);
                    ctx->entity_rem = read_bytes - ctx->entity_len;
                    break;
                }
                if (ret < 0) {
                    written = 1;
                    read_bytes = 1;
                    sub_12a4(dec_buf, ctx->entity_buf, 1);
                } else {
                    ctx->entity_rem = 0;
                    ctx->entity_len = 0;
                }
            } else {
                u32 rem = len - pos;
                u32 chunk = (rem < 6) ? rem : 6;
                ret = sceNp_4556A257(dec_buf, 6, p, chunk, &written, &read_bytes);
                if (ret == (int)0x80550602) {
                    sub_12a4(ctx->entity_buf, p, chunk);
                    p += chunk;
                    pos = (int)(p - data);
                    ctx->entity_rem = read_bytes - chunk;
                    ctx->entity_len = chunk;
                    break;
                }
                if (ret < 0) {
                    written = 1;
                    read_bytes = 1;
                    sub_12a4(dec_buf, p, 1);
                    step = 1;
                } else {
                    step = read_bytes;
                }
            }
            if (ctx->buf_pos + written < 0x1ff && !(written == 1 && read_bytes == 1 && (u8)dec_buf[0] == '<')) {
                sub_12a4(&ctx->val_buf[ctx->buf_pos], dec_buf, written);
                p += step;
                pos = (int)(p - data);
                ctx->buf_pos += written;
            } else {
                ctx->val_buf[ctx->buf_pos] = 0;
                ctx->buf_pos = 0;
                sub_12d8(ctx->tag_buf, 0, 0xff);
                p++;
                pos = (int)(p - data);
                ctx->self_close = 0;
                state = 1;
                ctx->attr_flags = 0;
            }
            break;
        }

        case 5: {
            if (ctx->buf_pos < 0xfe) {
                if (ctx->attr_flags != 1) {
                    u8 c = (u8)*p;
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '/' && c != '=' && c != '>') {
                        ctx->tag_buf[ctx->buf_pos++] = (char)c;
                        p++;
                        pos = (int)(p - data);
                        break;
                    }
                    ctx->tag_buf[ctx->buf_pos] = 0;
                    ctx->attr_flags = 1;
                }
                u8 c = (u8)*p;
                if (c == '\t' || c == '\n' || c == '\r' || c == ' ') {
                    p++;
                    pos = (int)(p - data);
                    break;
                } else if (c == '/') {
                    ctx->self_close = 1;
                    p++;
                    pos = (int)(p - data);
                    break;
                } else if (c == '=') {
                    p++;
                    pos = (int)(p - data);
                    state = 6;
                    ctx->buf_pos = 0;
                    ctx->attr_flags = 0;
                    break;
                }
            }
            ret = ctx->cb(ctx, 4, ctx->level, ctx->tag_idx[ctx->level], ctx->tag_buf, ctx->val_buf, ctx->user_data);
            if (ret < 0)
                goto out_ret;
            state = 2;
            break;
        }

        case 6: {
            u8 c = (u8)*p;
            if (ctx->buf_pos < 0x1fe) {
                if (ctx->attr_flags & 2) {
                    if (c != '"' && c != '\'') {
                        ctx->val_buf[ctx->buf_pos++] = (char)c;
                        p++;
                        pos = (int)(p - data);
                        break;
                    }
                }
                if (c == '\t' || c == '\n' || c == '\r' || c == ' ' || c == '/' || c == '>') {
                    if (!(ctx->attr_flags & 1)) {
                        p++;
                        pos = (int)(p - data);
                        break;
                    }
                    if (c == '/')
                        ctx->self_close = 1;
                    goto finish_attr;
                } else if (c == '"' || c == '\'') {
                    p++;
                    pos = (int)(p - data);
                    if (ctx->attr_flags & 2)
                        ctx->attr_flags &= ~2U;
                    else
                        ctx->attr_flags |= 3U;
                    break;
                } else {
                    ctx->attr_flags |= 1U;
                    ctx->val_buf[ctx->buf_pos++] = (char)c;
                    p++;
                    pos = (int)(p - data);
                    break;
                }
            }
        finish_attr:
            ctx->val_buf[ctx->buf_pos] = 0;
            ret = ctx->cb(ctx, 4, ctx->level, ctx->tag_idx[ctx->level], ctx->tag_buf, ctx->val_buf, ctx->user_data);
            if (ret < 0)
                goto out_ret;
            state = 2;
            break;
        }

        default:
            goto done;
        }
    }
    ret = 0;
done:
    ctx->state = state;
out_ret:
    return ret;
}

/* sceNp_6999EDC4 @ 0x25c4 */
int sceNp_6999EDC4(SceNpXmlContext *ctx, const SceNpXmlSchema *schema, SceNpXmlCallback cb, void *user_data)
{
    sub_12d8(ctx, 0, 0x344);
    ctx->user_data = user_data;
    ctx->schema = schema;
    ctx->cb = cb;
    return 0;
}

/* sub_2c68 @ 0x2c68 */
static int sub_2c68(char *dst, u32 dst_size, const char *src, u32 src_len, u32 *out_len, u32 *out_used, int copy_flag)
{
    u32 i = 0;
    while (i < src_len) {
        s8 c = (s8)src[i];
        int ct = sub_1248(c);
        if ((ct & 7) || c == '+' || c == '-' || c == '.' || c == '_') {
            i++;
        } else {
            break;
        }
    }

    if (i != 0 && src[i] == '@') {
        u32 used = i + 1;
        if (copy_flag) {
            if (dst_size < used)
                return 0x80550602;
            sub_12a4(dst, src, i);
            dst[i] = 0;
        }
        if (out_used)
            *out_used = used;
        if (out_len)
            *out_len = i;
        return 0;
    }

    if (out_used)
        *out_used = 1;
    if (copy_flag) {
        if (dst_size == 0)
            return 0x80550602;
        dst[0] = 0;
    }
    if (out_len)
        *out_len = 0;
    return 0;
}

/* sub_2dd4 @ 0x2dd4 */
static int sub_2dd4(char *dst, u32 dst_size, const char *src, u32 src_len, u32 *out_len, u32 *out_used, int copy_flag)
{
    u32 i = 0;
    while (i < src_len) {
        s8 c = (s8)src[i];
        int ct = sub_1248(c);
        if ((ct & 7) || c == '.' || c == '_' || c == '-') {
            i++;
        } else {
            break;
        }
    }

    while (i != 0 && src[i - 1] == '.')
        i--;

    if (i == 0) {
        if (out_used)
            *out_used = 1;
        if (copy_flag) {
            if (dst_size == 0)
                return 0x80550602;
            dst[0] = 0;
        }
        if (out_len)
            *out_len = 0;
        return 0;
    }

    u32 used = i + 1;
    if (copy_flag) {
        if (dst_size < used)
            return 0x80550602;
        sub_12a4(dst, src, i);
        dst[i] = 0;
    }
    if (out_used)
        *out_used = used;
    if (out_len)
        *out_len = i;
    return 0;
}

/* sub_2f40 @ 0x2f40 */
static int sub_2f40(const char *domain, const char **out_part1, const char **out_part2)
{
    if (!(sub_1248((u8)domain[0]) & 7))
        return 0x80550604;
    if (!(sub_1248((u8)domain[1]) & 7))
        return 0x80550604;
    if (domain[2] != '.')
        return 0x80550604;
    const char *p2 = domain + 3;
    if (!(sub_1248((u8)p2[0]) & 7))
        return 0x80550604;
    if (!(sub_1248((u8)p2[1]) & 7))
        return 0x80550604;
    if (p2[2] != '.')
        return 0x80550604;

    const char *p = domain + 6;
    if (*p != '.') {
        while (*p != '.') {
            if (*p == 0)
                return 0x80550604;
            p++;
        }
    }
    if (sub_1cc8(p + 1, "playstation.net", 16) != 0)
        return 0x80550604;

    *out_part1 = domain;
    *out_part2 = p2;
    return 0;
}

/* sub_3078 @ 0x3078 */
static int sub_3078(char *dst, u32 dst_size, const char *src, u32 src_len, u32 *out_len, u32 *out_used, int copy_flag)
{
    u32 i = 0;
    if (src && src_len > 0 && src[0] != 0) {
        i = 1;
        while (i < src_len && src[i] != 0)
            i++;
    }
    if (i == 0) {
        if (out_used)
            *out_used = 1;
        if (copy_flag) {
            if (dst_size == 0)
                return 0x80550602;
            dst[0] = 0;
        }
        if (out_len)
            *out_len = 0;
        return 0;
    }

    u32 used = i + 1;
    if (copy_flag) {
        if (dst_size < used)
            return 0x80550602;
        sub_12a4(dst, src, i);
        dst[i] = 0;
    }
    if (out_used)
        *out_used = used;
    if (out_len)
        *out_len = i;
    return 0;
}

typedef struct SceNpJid {
    char *node;
    char *domain;
    char *resource;
    u32 pad;
} SceNpJid;

/* sceNp_5FA879D8 @ 0x2854 */
int sceNp_5FA879D8(SceNpJid *jid, const char *str, char *buf, u32 buf_size, u32 *out_req_size)
{
    u32 len = 0, used = 0;
    int copy_flag = 1;

    if (!str)
        return 0x80550601;

    if (jid && buf) {
        sub_12d8(jid, 0, 0x10);
    } else {
        copy_flag = 0;
        if (!out_req_size)
            return 0x80550601;
    }

    u32 str_len = sub_1c94(str);
    u32 pos = 0;
    while (pos < str_len && (str[pos] == '\t' || str[pos] == ' '))
        pos++;

    if (str[pos] == 0)
        return 0x80550604;

    int ret = sub_2c68(buf, buf_size, str + pos, str_len - pos, &len, &used, copy_flag);
    if (ret < 0)
        goto err;
    if (copy_flag)
        jid->node = buf;

    pos += len;
    if (pos < str_len && str[pos] == '@')
        pos++;

    u32 total_used = used;
    ret = sub_2dd4(buf + total_used, buf_size - total_used, str + pos, str_len - pos, &len, &used, copy_flag);
    if (ret < 0)
        goto err;
    if (copy_flag)
        jid->domain = buf + total_used;

    total_used += used;
    u32 res_pos = pos + len;
    if (res_pos < str_len)
        res_pos++;
    ret = sub_3078(buf + total_used, buf_size - total_used, str + res_pos, str_len - res_pos, &len, &used, copy_flag);
    if (ret < 0)
        goto err;
    if (copy_flag)
        jid->resource = buf + total_used;

    if (out_req_size)
        *out_req_size = total_used + used;
    return 0;

err:
    if (copy_flag)
        sub_12d8(jid, 0, 0x10);
    return ret;
}

/* sceNp_0B8BEE48 @ 0x2ab8 */
int sceNp_0B8BEE48(const SceNpJid *jid, char *buf, u32 buf_size, u32 *out_len)
{
    u32 node_len = 0, domain_len = 0, res_len = 0;

    if (!jid || !jid->domain || jid->domain[0] == 0)
        return 0x80550604;

    if (jid->node && jid->node[0] != 0)
        node_len = sub_1c94(jid->node);
    domain_len = sub_1c94(jid->domain);
    if (jid->resource)
        res_len = sub_1c94(jid->resource);

    u32 total = (node_len ? (node_len + 1) : 0) + domain_len + (res_len ? (res_len + 1) : 0) + 1;
    if (out_len)
        *out_len = total;

    if (buf) {
        if (buf_size < total)
            return 0x80550602;
        u32 pos = 0;
        if (node_len) {
            sub_12a4(buf, jid->node, node_len);
            buf[node_len] = '@';
            pos = node_len + 1;
        }
        sub_12a4(buf + pos, jid->domain, domain_len);
        pos += domain_len;
        if (res_len) {
            buf[pos++] = '/';
            sub_12a4(buf + pos, jid->resource, res_len);
            pos += res_len;
        }
        buf[pos] = 0;
        if (out_len)
            *out_len = pos + 1;
    }
    return 0;
}

/* sceNp_C0B3616C @ 0x3184 */
int sceNp_C0B3616C(u8 *npid, const SceNpJid *jid)
{
    const char *p1, *p2;
    u32 res_len = 0;

    if (!npid || !jid)
        return 0x80550601;
    if (!jid->node || !jid->domain)
        return 0x80550604;

    u32 node_len = sub_1c94(jid->node);
    if ((u32)(node_len - 1) >= 16)
        return 0x80550604;

    int ret = sub_2f40(jid->domain, &p1, &p2);
    if (ret < 0)
        return ret;

    if (jid->resource) {
        res_len = sub_1c94(jid->resource);
        if (res_len >= 5)
            return 0x80550604;
    }

    sub_12d8(npid, 0, 0x24);
    sub_12a4(npid, jid->node, node_len);
    npid[0x14] = (u8)p1[0];
    npid[0x15] = (u8)p1[1];
    npid[0x16] = (u8)p2[0];
    npid[0x17] = (u8)p2[1];
    if (res_len != 0)
        sub_12a4(npid + 0x18, jid->resource, res_len);
    npid[0x1c] = 1;
    return 0;
}

/* sceNp_B2BFADB2 @ 0x32d0 */
int sceNp_B2BFADB2(SceNpJid *jid, const u8 *npid, char *buf, u32 buf_size, u32 *out_req_size)
{
    char env[16];
    int copy_flag = 1;

    if (!npid)
        return 0x80550601;
    if (npid[0x1c] != 1)
        return 0x80550605;

    if (jid && buf) {
        sub_12d8(jid, 0, 0x10);
    } else {
        copy_flag = 0;
        if (!out_req_size)
            return 0x80550601;
    }

    u32 id_len = sub_1c94((const char *)npid);
    if (id_len == 0)
        return 0x80550605;
    if (!(sub_1248(npid[0x14]) & 7) || !(sub_1248(npid[0x15]) & 7) ||
        !(sub_1248(npid[0x16]) & 7) || !(sub_1248(npid[0x17]) & 7))
        return 0x80550605;

    int ret = sceNpCore_52440ABF(env);
    if (ret < 0)
        return ret;

    u32 res_len = 0;
    while (res_len < 4 && npid[0x18 + res_len] != 0)
        res_len++;

    u32 env_len = sub_1c94(env);
    u32 req_size = id_len + env_len + res_len + 0x19;
    if (out_req_size)
        *out_req_size = req_size;

    if (!copy_flag)
        return 0;
    if (buf_size < req_size)
        return 0x80550602;

    jid->node = buf;
    sub_12a4(buf, npid, id_len);
    buf[id_len] = 0;
    jid->domain = buf + id_len + 1;
    buf[id_len + 1] = npid[0x14];
    buf[id_len + 2] = npid[0x15];
    buf[id_len + 3] = '.';
    buf[id_len + 4] = npid[0x16];
    buf[id_len + 5] = npid[0x17];
    buf[id_len + 6] = '.';
    sub_12a4(buf + id_len + 7, env, env_len);
    buf[id_len + 7 + env_len] = '.';
    sub_12a4(buf + id_len + 8 + env_len, "playstation.net", 15);
    buf[id_len + 23 + env_len] = 0;

    char *res_ptr = buf + id_len + 24 + env_len;
    jid->resource = res_ptr;
    if (res_len != 0) {
        sub_12a4(res_ptr, npid + 0x18, res_len);
        res_ptr += res_len;
    }
    *res_ptr = 0;
    if (out_req_size)
        *out_req_size = (u32)(res_ptr - buf) + 1;
    return 0;
}

/* sceNp_E41288E7 @ 0x3568 */
int sceNp_E41288E7(char *dst, u32 dst_size, const char *src, u32 src_size, u32 *out_written)
{
    int write_flag = 1;
    u32 count = 0;

    if (!src || !src_size)
        return 0x80550601;
    if (!dst) {
        if (!out_written)
            return 0x80550601;
        write_flag = 0;
    }

    const u8 *p = (const u8 *)src;
    const u8 *end = p + src_size;
    while (p < end) {
        u8 c = *p;
        if ((s8)c >= 0) {
            if (g_xml_char_table[c] != 0)
                return 0x80550608;
            if (c == '<') {
                count += 4;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    dst[0] = '&'; dst[1] = 'l'; dst[2] = 't'; dst[3] = ';';
                    dst += 4;
                }
                p++;
            } else if (c == '>') {
                count += 4;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    dst[0] = '&'; dst[1] = 'g'; dst[2] = 't'; dst[3] = ';';
                    dst += 4;
                }
                p++;
            } else if (c == '&') {
                count += 5;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    dst[0] = '&'; dst[1] = 'a'; dst[2] = 'm'; dst[3] = 'p'; dst[4] = ';';
                    dst += 5;
                }
                p++;
            } else if (c == '\'') {
                count += 6;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    dst[0] = '&'; dst[1] = 'a'; dst[2] = 'p'; dst[3] = 'o'; dst[4] = 's'; dst[5] = ';';
                    dst += 6;
                }
                p++;
            } else if (c == '"') {
                count += 6;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    dst[0] = '&'; dst[1] = 'q'; dst[2] = 'u'; dst[3] = 'o'; dst[4] = 't'; dst[5] = ';';
                    dst += 6;
                }
                p++;
            } else {
                count++;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag)
                    *dst++ = (char)c;
                p++;
            }
        } else {
            if ((c & 0xe0) == 0xc0 && c >= 0xc2 && (p[1] & 0xc0) == 0x80) {
                count += 2;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    *dst++ = (char)p[0];
                    *dst++ = (char)p[1];
                }
                p += 2;
            } else if ((c & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
                count += 3;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    *dst++ = (char)p[0];
                    *dst++ = (char)p[1];
                    *dst++ = (char)p[2];
                }
                p += 3;
            } else if ((c & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
                count += 4;
                if (write_flag && dst_size < count)
                    return 0x80550602;
                if (write_flag) {
                    *dst++ = (char)p[0];
                    *dst++ = (char)p[1];
                    *dst++ = (char)p[2];
                    *dst++ = (char)p[3];
                }
                p += 4;
            } else {
                return 0x80550608;
            }
        }
    }

    count++;
    if (write_flag && dst_size < count)
        return 0x80550602;
    if (write_flag)
        *dst = 0;
    if (out_written)
        *out_written = count;
    return 0;
}

/* sceNp_C250A650 @ 0x3978 */
int sceNp_C250A650(char *dst, u32 dst_size, const char *src, u32 src_size, u32 *out_written)
{
    int write_flag = 1;
    u32 count = 0;

    if (!src || !src_size)
        return 0x80550601;
    if (!dst) {
        if (!out_written)
            return 0x80550601;
        write_flag = 0;
    }

    const u8 *p = (const u8 *)src;
    const u8 *end = p + src_size;
    while (p < end) {
        count++;
        if (write_flag && dst_size < count)
            return 0x80550602;
        u8 c = *p;
        if (c == '&') {
            if (p[1] == 'l' && p[2] == 't' && p[3] == ';') {
                if (write_flag)
                    *dst++ = '<';
                p += 4;
                continue;
            }
            if (p[1] == 'g' && p[2] == 't' && p[3] == ';') {
                if (write_flag)
                    *dst++ = '>';
                p += 4;
                continue;
            }
            if (p[1] == 'a' && p[2] == 'm' && p[3] == 'p' && p[4] == ';') {
                if (write_flag)
                    *dst++ = '&';
                p += 5;
                continue;
            }
            if (p[1] == 'a' && p[2] == 'p' && p[3] == 'o' && p[4] == 's' && p[5] == ';') {
                if (write_flag)
                    *dst++ = '\'';
                p += 6;
                continue;
            }
            if (p[1] == 'q' && p[2] == 'u' && p[3] == 'o' && p[4] == 't' && p[5] == ';') {
                if (write_flag)
                    *dst++ = '"';
                p += 6;
                continue;
            }
        }
        if (write_flag)
            *dst++ = (char)c;
        p++;
    }

    count++;
    if (write_flag && dst_size < count)
        return 0x80550602;
    if (write_flag)
        *dst = 0;
    if (out_written)
        *out_written = count;
    return 0;
}

/* sceNp_4556A257 @ 0x3c10 */
int sceNp_4556A257(char *dst, u32 dst_size, const char *src, u32 src_size, u32 *out_written, u32 *out_read)
{
    char tmp[16];
    int write_flag = 1;
    u32 written = 0, read_len = 0;

    if (!src || !src_size)
        return 0x80550601;
    if (!dst) {
        write_flag = 0;
        if (!out_written)
            return 0x80550601;
    }

    u8 c = (u8)src[0];
    if ((s8)c >= 0) {
        written = 1;
        read_len = 1;
        if (c == '&') {
            if (src_size < 4) {
                read_len = 4;
                goto err_short;
            }
            if ((u8)src[1] == 'l' && (u8)src[2] == 't' && (u8)src[3] == ';') {
                read_len = 4;
                goto check_copy;
            }
            if ((u8)src[1] == 'g' && (u8)src[2] == 't' && (u8)src[3] == ';') {
                read_len = 4;
                goto check_copy;
            }
            if (src_size < 5) {
                read_len = 5;
                goto err_short;
            }
            if ((u8)src[1] == 'a' && (u8)src[2] == 'm' && (u8)src[3] == 'p' && (u8)src[4] == ';') {
                read_len = 5;
                goto check_copy;
            }
            if (src_size < 6) {
                read_len = 6;
                goto err_short;
            }
            if ((u8)src[1] == 'a' && (u8)src[2] == 'p' && (u8)src[3] == 'o' && (u8)src[4] == 's' && (u8)src[5] == ';') {
                read_len = 6;
                goto check_copy;
            }
            if ((u8)src[1] == 'q' && (u8)src[2] == 'u' && (u8)src[3] == 'o' && (u8)src[4] == 't' && (u8)src[5] == ';') {
                read_len = 6;
                goto check_copy;
            }
        }
    } else {
        if ((c & 0xe0) == 0xc0 && c >= 0xc2) {
            if (src_size < 2) {
                read_len = 2;
                goto err_short;
            }
            if ((src[1] & 0xc0) == 0x80) {
                written = 2;
                read_len = 2;
                goto check_copy;
            }
        }
        if ((c & 0xf0) == 0xe0) {
            if (src_size < 3) {
                read_len = 3;
                goto err_short;
            }
            if ((src[1] & 0xc0) == 0x80 && (src[2] & 0xc0) == 0x80) {
                written = 3;
                read_len = 3;
                goto check_copy;
            }
        }
        if ((c & 0xf8) == 0xf0) {
            if (src_size < 4) {
                read_len = 4;
                goto err_short;
            }
            if ((src[1] & 0xc0) == 0x80 && (src[2] & 0xc0) == 0x80 && (src[3] & 0xc0) == 0x80) {
                written = 4;
                read_len = 4;
                goto check_copy;
            }
        }
        written = 1;
        read_len = 1;
    }

check_copy:
    if (write_flag && dst_size < written) {
    err_short:
        if (out_written)
            *out_written = written;
        if (out_read)
            *out_read = read_len;
        return 0x80550602;
    }
    int ret = sceNp_C250A650(tmp, 7, src, read_len, NULL);
    if (ret < 0)
        return ret;
    if (write_flag)
        sub_12a4(dst, tmp, written);
    if (out_written)
        *out_written = written;
    if (out_read)
        *out_read = read_len;
    return ret;
}

/* sceNp_E90DFBC4 @ 0x3fd4 */
int sceNp_E90DFBC4(const u8 *comm_id, char *str, u32 str_size)
{
    if (!comm_id || !str)
        return 0x80550601;
    if (str_size != 13)
        return 0x80550602;
    if (comm_id[10] >= 100)
        return 0x80550607;

    sub_12a4(str, comm_id, 9);
    str[9] = '_';
    if (comm_id[10] < 10) {
        str[10] = '0';
        char *dst = str + 11;
        u32 val = comm_id[10];
        u32 digits = 0;
        u32 tmp = val;
        do {
            digits++;
            tmp /= 10;
        } while (tmp != 0);
        if (dst != NULL && digits <= 1 && digits != 0) {
            while (digits != 0) {
                digits--;
                dst[digits] = (char)('0' + (val % 10));
                val /= 10;
            }
        }
        str[12] = 0;
    } else {
        char *dst = str + 10;
        u32 val = comm_id[10];
        u32 digits = 0;
        u32 tmp = val;
        do {
            digits++;
            tmp /= 10;
        } while (tmp != 0);
        if (dst != NULL && digits <= 2 && digits != 0) {
            while (digits != 0) {
                digits--;
                dst[digits] = (char)('0' + (val % 10));
                val /= 10;
            }
        }
        str[12] = 0;
    }
    return 0;
}

/* sceNp_64F72B22 @ 0x41ac */
int sceNp_64F72B22(u8 *comm_id, const char *str)
{
    if (!str)
        return 0x80550601;
    if ((u32)sub_1c94(str) >= 13)
        return 0x80550607;
    if (str[9] != '_')
        return 0x80550607;

    sub_12d8(comm_id, 0, 12);
    sub_12a4(comm_id, str, 9);
    comm_id[10] = (u8)sub_133c(str + 10, NULL, 10);
    return 0;
}

/* sceNp_2D8E93D2 @ 0x4254 */
int sceNp_2D8E93D2(const u8 *npid1, const u8 *npid2, int *out_order)
{
    if (!npid1 || !npid2)
        return 0x80550601;
    if (npid1[0x1c] != 1 || npid2[0x1c] != 1)
        return 0x80550605;

    int cmp = sub_1cc8((const char *)npid1, (const char *)npid2, 16);
    if (cmp < 0) {
        *out_order = -1;
        return 0;
    }
    if (cmp > 0) {
        *out_order = 1;
        return 0;
    }

    cmp = sub_125c(npid1 + 0x14, npid2 + 0x14, 4);
    if (cmp < 0) {
        *out_order = -1;
        return 0;
    }
    if (cmp > 0) {
        *out_order = 1;
        return 0;
    }

    const u8 *r1 = (npid1[0x18] != 0) ? (npid1 + 0x18) : (const u8 *)"ps3";
    const u8 *r2 = (npid2[0x18] != 0) ? (npid2 + 0x18) : (const u8 *)"ps3";
    if (npid1[0x18] == 0 && npid2[0x18] == 0) {
        *out_order = 0;
        return 0;
    }
    cmp = sub_125c(r1, r2, 4);
    *out_order = (cmp < 0) ? -1 : ((cmp > 0) ? 1 : 0);
    return 0;
}

/* sceNp_9A264EF2 @ 0x437c */
int sceNp_9A264EF2(const u8 *npid1, const u8 *npid2)
{
    if (!npid1 || !npid2)
        return 0x80550601;
    if (npid1[0x1c] != 1 || npid2[0x1c] != 1)
        return 0x80550605;

    if (sub_1cc8((const char *)npid1, (const char *)npid2, 16) != 0)
        return 0x80550609;
    if (sub_125c(npid1 + 0x14, npid2 + 0x14, 4) != 0)
        return 0x80550609;
    if (sub_125c(npid1 + 0x18, npid2 + 0x18, 4) == 0)
        return 0;

    if (npid1[0x18] == 0) {
        if (sub_125c("ps3", npid2 + 0x18, 4) == 0)
            return 0;
    } else if (npid2[0x18] == 0) {
        if (sub_125c(npid1 + 0x18, "ps3", 4) == 0)
            return 0;
    }
    return 0x80550609;
}

/* sub_463c @ 0x463c */
static int sub_463c(const char *cat, const char *key, u32 *out_val)
{
    u32 reg_param[0x44];
    int h = 0, hd = 0, hk = 0;
    u32 type = 0, size = 0;

    sub_12d8(reg_param, 0, 0x110);
    reg_param[0] = 1;
    reg_param[0x42] = 1;
    reg_param[0x43] = 1;

    int ret = sceRegOpenRegistry(reg_param, 2, &h);
    if (ret != 0)
        return ret;

    ret = sceRegOpenCategory(h, cat, 2, &hd);
    if (ret != 0) {
        sceRegCloseRegistry(h);
        return ret;
    }

    ret = sceRegGetKeyInfo(hd, key, &hk, &type, &size);
    if (ret != 0) {
        sceRegCloseCategory(hd);
        sceRegCloseRegistry(h);
        return ret;
    }
    if (type != 2) {
        sceRegCloseCategory(hd);
        sceRegCloseRegistry(h);
        return 0x80550081;
    }
    if (size != 4) {
        sceRegCloseCategory(hd);
        sceRegCloseRegistry(h);
        return 0x80550082;
    }

    ret = sceRegGetKeyValue(hd, hk, out_val, 4);
    sceRegCloseCategory(hd);
    sceRegCloseRegistry(h);
    return ret;
}

/* sceNp_857B47D3 @ 0x44a0 */
int sceNp_857B47D3(void)
{
    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;

    int ret = sceNpCore_57E15796();
    if (ret < 0)
        return ret;

    sub_1034(sceKernelGetSystemTimeLow());

    if (sub_463c("/TOOL/CONFIG", "np_debug", &g_np_debug) < 0)
        g_np_debug = 0;
    if (sub_463c("/CONFIG/NP", "nav_only", &g_np_nav_only) < 0)
        g_np_nav_only = 0;

    ret = sub_0000();
    if (ret < 0) {
        sub_0480();
        return ret;
    }
    g_np_inited = 1;
    return 0;
}

/* sceNp_37E1E274 @ 0x4580 */
int sceNp_37E1E274(void)
{
    if (_sceKernelCheckThreadStack() < 0x7f0)
        return 0x80550004;
    if (g_np_inited != 0) {
        sub_0480();
        g_np_inited = 0;
    }
    return 0;
}

/* sceNp_B4063F7A @ 0x45cc */
int sceNp_B4063F7A(char *ver_str)
{
    if (!ver_str)
        return 0x80550003;
    sub_12d8(ver_str, 0, 8);
    sub_1510(ver_str, "%02d.%02d", 6, 60, 0, 0, 0, 0);
    return 0;
}

/* sceNp_2E4769C3 @ 0x4624 */
int sceNp_2E4769C3(void)
{
    return g_np_debug;
}

/* sceNp_E24DA399 @ 0x4630 */
int sceNp_E24DA399(void)
{
    return g_np_nav_only;
}
