#include "np_service_internal.h"

/* =========================================================================
 * XML Schema Definitions (used by sceNp_6999EDC4)
 * ========================================================================= */

typedef struct XmlNodeDef {
    const char *name;
    int is_leaf;
} XmlNodeDef;

typedef struct XmlSchema {
    const XmlNodeDef *const *levels;
    int depth;
} XmlSchema;

static const XmlNodeDef g_xml_profile_l0[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_profile_l1[] = {
    { "jid", 1 },
    { "onlinename", 1 },
    { "country", 1 },
    { "language1", 1 },
    { "language2", 1 },
    { "language3", 1 },
    { "aboutme", 1 },
    { "avatarurl", 1 },
    { "ptlp", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_profile_levels[] = {
    g_xml_profile_l0, g_xml_profile_l1,
};
static const XmlSchema g_xml_profile_schema = { g_xml_profile_levels, 2 };

static const XmlNodeDef g_xml_searchjid_fqdn_l0[] = {
    { "searchjid", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_searchjid_fqdn_l1[] = {
    { "searchjid-fqdn", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_searchjid_fqdn_levels[] = {
    g_xml_searchjid_fqdn_l0, g_xml_searchjid_fqdn_l1,
};
static const XmlSchema g_xml_searchjid_fqdn_schema = { g_xml_searchjid_fqdn_levels, 2 };

static const XmlNodeDef g_xml_matching2_fqdn_l0[] = {
    { "matching2", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_matching2_fqdn_l1[] = {
    { "agent-fqdn", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_matching2_fqdn_levels[] = {
    g_xml_matching2_fqdn_l0, g_xml_matching2_fqdn_l1,
};
static const XmlSchema g_xml_matching2_fqdn_schema = { g_xml_matching2_fqdn_levels, 2 };

static const XmlNodeDef g_xml_searchjid_l0[] = {
    { "searchjid", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_searchjid_l1[] = {
    { "jid", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_searchjid_levels[] = {
    g_xml_searchjid_l0, g_xml_searchjid_l1,
};
static const XmlSchema g_xml_searchjid_schema = { g_xml_searchjid_levels, 2 };

static const XmlNodeDef g_xml_uploader_list_l0[] = {
    { "uploader", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_l1[] = {
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_l2[] = {
    { "data-info", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_l3[] = {
    { "reference", 1 },
    { "application", 1 },
    { "size", 1 },
    { "date", 1 },
    { "keep", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_uploader_list_levels[] = {
    g_xml_uploader_list_l0, g_xml_uploader_list_l1,
    g_xml_uploader_list_l2, g_xml_uploader_list_l3,
};
static const XmlSchema g_xml_uploader_list_schema = { g_xml_uploader_list_levels, 4 };

static const XmlNodeDef g_xml_uploader_dl_l0[] = {
    { "uploader", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_dl_l1[] = {
    { "jid", 1 },
    { "quota", 1 },
    { "application", 1 },
    { "size", 1 },
    { "date", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_uploader_dl_levels[] = {
    g_xml_uploader_dl_l0, g_xml_uploader_dl_l1,
};
static const XmlSchema g_xml_uploader_dl_schema = { g_xml_uploader_dl_levels, 2 };

static const XmlNodeDef g_xml_avatar_cat_l0[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_cat_l1[] = {
    { "categories", 1 },
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_cat_l2[] = {
    { "category", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_avatar_cat_levels[] = {
    g_xml_avatar_cat_l0, g_xml_avatar_cat_l1, g_xml_avatar_cat_l2,
};
static const XmlSchema g_xml_avatar_cat_schema = { g_xml_avatar_cat_levels, 3 };

static const XmlNodeDef g_xml_avatar_list_l0[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_list_l1[] = {
    { "avatars", 1 },
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_list_l2[] = {
    { "avatarurl", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_avatar_list_levels[] = {
    g_xml_avatar_list_l0, g_xml_avatar_list_l1, g_xml_avatar_list_l2,
};
static const XmlSchema g_xml_avatar_list_schema = { g_xml_avatar_list_levels, 3 };

/* =========================================================================
 * Common XML parsing helpers
 * ========================================================================= */

int np_xml_feed(void *parser, const void *data, u32 len)
{
    return sceNp_1916432C(parser, data, len);
}

static int xml_check_result_attr(int ev, int depth, int idx, const char *name, const char *val)
{
    if (ev != 4 || depth != 0 || idx != 0)
        return 0;
    if (sceNet_lib_A0F16ABD("result", name) != 0)
        return 0;
    if (val == NULL || sceNet_lib_D8722983(val) != 2)
        return (int)0x80550708;
    {
        u32 code = sceNet_lib_2A73ADDC(val, NULL, 16);
        if (code >= 0x100u)
            return (int)0x80550708;
        if (code != 0) {
            u32 err = code | 0x80550800u;
            if (err == 0x80550804u)
                return (int)0x80550007;
            return (int)err;
        }
    }
    return 0;
}

static int xml_check_status_attr(int mode, int ev, int depth, int idx, const char *name, const char *val)
{
    if (ev != 4 || depth != 0 || idx != 0)
        return 0;
    if (sceNet_lib_A0F16ABD("status", name) != 0)
        return 0;
    if (val == NULL)
        return (int)0x80550708;
    if (sceNet_lib_B9085A96(val, "alive", 6) == 0)
        return 0;
    if (sceNet_lib_B9085A96(val, "maintenance", 12) == 0)
        return (mode == 2) ? (int)0x805508A2 : (int)0x805508A5;
    if (sceNet_lib_B9085A96(val, "end", 4) == 0)
        return (mode == 2) ? (int)0x805508A1 : (int)0x805508A4;
    if (sceNet_lib_B9085A96(val, "before", 7) == 0)
        return (mode == 2) ? (int)0x805508A0 : (int)0x805508A3;
    return (int)0x80550708;
}

static int xml_parse_onlinename(int ev, const char *a2, const char *a3, char *dst, int *upd_out, u64 *date_out)
{
    if (ev == 3)
        sceNet_lib_B5CE388A(dst, a2, 48);
    if (ev != 4)
        return 0;
    if (sceNet_lib_A0F16ABD("upd", a2) == 0) {
        *upd_out = sceNet_lib_1FB2FDDD(a3);
        return 0;
    }
    if (sceNet_lib_A0F16ABD("date", a2) != 0)
        return 0;
    if (sceRtcParseDateTime(date_out, a3) < 0)
        return (int)0x80550708;
    return 0;
}

static int xml_parse_avatarurl(int ev, const char *a2, const char *a3, char *dst, int *id_out, u32 *flags, u32 *count)
{
    if (ev == 4) {
        if (sceNet_lib_A0F16ABD("id", a2) == 0 && id_out != NULL)
            *id_out = sceNet_lib_1FB2FDDD(a3);
        if (flags != NULL)
            *flags |= 8u;
    }
    if (ev == 3) {
        if ((u32)sceNet_lib_D8722983(a2) >= 128u)
            return (int)0x80550708;
        sceNet_lib_B5CE388A(dst, a2, 127);
        if (flags != NULL) {
            if ((*flags & 8u) == 0)
                return (int)0x80550708;
            *flags &= ~8u;
        }
        if (count != NULL)
            (*count)++;
    }
    return 0;
}

static int xml_parse_category(int ev, const char *a2, const char *a3, int *entry, u32 *flags, u32 *count)
{
    if (ev == 4) {
        if (sceNet_lib_A0F16ABD("id", a2) == 0) {
            entry[0] = sceNet_lib_1FB2FDDD(a3);
            *flags |= 8u;
        } else if (sceNet_lib_A0F16ABD("avatars", a2) == 0) {
            entry[1] = sceNet_lib_1FB2FDDD(a3);
            *flags |= 16u;
        }
    }
    if (ev == 3) {
        sceNet_lib_B5CE388A((char *)(entry + 2), a2, 385);
        if ((*flags & 0x18u) != 0x18u)
            return (int)0x80550708;
        (*count)++;
    }
    return 0;
}

/* --- Profile XML callback --- */

static int xml_profile_cb(void *parser, int ev, int depth, u32 idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;
    u8 *prof = (u8 *)ctx[1];

    (void)parser;

    ret = xml_check_result_attr(ev, depth, (int)idx, s1, s2);
    if (ret < 0)
        return ret;

    ctx[4] |= 1u;
    if (ev == 3 && depth == 1 && idx == 0) {
        ret = np_jid_str_to_npid(prof, s1);
        if (ret < 0)
            return ret;
        ctx[4] |= 2u;
    }
    if (depth == 1 && idx == 1) {
        ret = xml_parse_onlinename(ev, s1, s2, (char *)(prof + 36), (int *)(ctx + 2), (u64 *)(ctx + 6));
        if (ret < 0)
            return ret;
        if (ev == 3)
            ctx[4] |= 4u;
    }
    if (ev == 3 && depth == 1 && idx == 2) {
        sceKernelMemcpy(prof + 216, s1, 2);
        ctx[4] |= 8u;
    }
    if (ev == 3 && depth == 1 && idx >= 3u && idx < 6u) {
        int lang = -1;
        if (sceNet_lib_D8722983(s1) != 0) {
            lang = sceNet_lib_1FB2FDDD(s1);
            if (lang >= 17)
                lang = 1;
            if (lang >= 12) {
                u32 sdk = sceKernelGetCompiledSdkVersion();
                if (sdk <= 0x0401FFFFu)
                    lang = 1;
                else if (sdk <= 0x0601FFFFu && lang >= 16)
                    lang = 1;
            }
        }
        if (idx == 3u) {
            *(int *)(prof + 220) = lang;
            ctx[4] |= 0x10u;
        } else if (idx == 4u) {
            *(int *)(prof + 224) = (sceNet_lib_D8722983(s1) != 0) ? lang : -1;
            ctx[4] |= 0x20u;
        } else {
            *(int *)(prof + 228) = (sceNet_lib_D8722983(s1) != 0) ? lang : -1;
            ctx[4] |= 0x40u;
        }
    }
    if (ev == 3 && depth == 1 && idx == 6u) {
        if (s1[0] != '\0') {
            if ((u32)(sceNet_lib_D8722983(s1) + 1) >= 65u)
                return (int)0x80550708;
            sceNet_lib_B5CE388A((char *)(prof + 240), s1, 64);
        }
        ctx[4] |= 0x80u;
    }
    if (depth == 1 && idx == 7u) {
        ret = xml_parse_avatarurl(ev, s1, s2, (char *)(prof + 88), (int *)(prof + 236), NULL, NULL);
        if (ret < 0)
            return ret;
        if (ev == 3)
            ctx[4] |= 0x100u;
    }
    if (ev == 3 && depth == 1 && idx == 8u) {
        ctx[3] = (u32)sceNet_lib_1FB2FDDD(s1);
        ctx[4] |= 0x200u;
    }
    if (ev == 2 && depth == 0 && idx == 0u) {
        if ((ctx[4] & 0x3FDu) == 0x3FDu) {
            if (ctx[0] != 1u || (ctx[4] & 2u) != 0) {
                ctx[4] |= 0x400u;
                return 0;
            }
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_profile_init(void *parser, int mode, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    int *prof = (int *)ctx[1];

    ctx[0] = (u32)mode;
    ctx[4] = 0;
    sceKernelMemset(prof, 0, 304);
    prof[55] = -1;
    prof[56] = -1;
    prof[57] = -1;
    return sceNp_6999EDC4(parser, &g_xml_profile_schema, xml_profile_cb, ctx);
}

int np_xml_profile_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[4] & 0x400u) != 0) ? 0 : (int)0x80550708;
}

/* --- FQDN list XML callback --- */

static int xml_fqdn_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;
    char title_buf[16];

    (void)parser;

    ret = xml_check_status_attr((int)ctx[0], ev, depth, idx, s1, s2);
    if (ret < 0)
        goto fail;

    ctx[6] |= 1u;
    if (ctx[0] == 2u) {
        if (ev == 4 && depth == 0 && idx == 0) {
            if (sceNet_lib_A0F16ABD("titleid", s1) == 0) {
                if (s2 == NULL) {
                    ret = (int)0x8055070B;
                    goto fail;
                }
                sceKernelMemset(title_buf, 0, 13);
                ret = sceNp_E90DFBC4((const void *)ctx[1], title_buf, 13);
                if (ret < 0)
                    goto fail;
                if (sceNet_lib_B9085A96(s2, title_buf, sceNet_lib_D8722983(title_buf) + 1) != 0) {
                    ret = (int)0x8055070B;
                    goto fail;
                }
            }
        }
        ctx[6] |= 2u;
    }

    if (ev == 4 && depth == 1 && idx == 0) {
        if (sceNet_lib_A0F16ABD("id", s1) == 0) {
            if (s2 == NULL) { ret = (int)0x80550708; goto fail; }
            ((u16 *)ctx)[10] = (u16)sceNet_lib_1FB2FDDD(s2);
            ctx[6] |= 8u;
        } else if (sceNet_lib_A0F16ABD("status", s1) == 0) {
            if (s2 == NULL) { ret = (int)0x80550708; goto fail; }
            if (sceNet_lib_B9085A96(s2, "alive", 6) == 0)
                ctx[4] = 1;
            else if (sceNet_lib_B9085A96(s2, "maintenance", 12) == 0)
                ctx[4] = 2;
            else { ret = (int)0x80550708; goto fail; }
            ctx[6] |= 0x10u;
        } else if (sceNet_lib_A0F16ABD("port", s1) == 0) {
            if (s2 == NULL) { ret = (int)0x80550708; goto fail; }
            ((u16 *)ctx)[11] = (u16)sceNet_lib_1FB2FDDD(s2);
            ctx[6] |= 0x20u;
        }
    }

    if (ev == 3 && depth == 1 && idx == 0) {
        u8 *node;
        int slen;
        void **head_ptr;

        if (s1[0] == '\0' || (ctx[0] == 2u && (ctx[6] & 0x38u) != 0x38u)) {
            ret = (int)0x80550708;
            goto fail;
        }
        slen = sceNet_lib_D8722983(s1);
        node = (u8 *)np_alloc((SceSize)(slen + 17));
        if (node == NULL) {
            ret = (int)0x80550703;
            goto fail;
        }
        sceKernelMemset(node, 0, 16);
        sceNet_lib_B5CE388A((char *)(node + 16), s1, slen + 1);
        *(char **)node = (char *)(node + 16);
        *(u16 *)(node + 4) = ((u16 *)ctx)[11];
        *(u16 *)(node + 6) = ((u16 *)ctx)[10];
        *(u32 *)(node + 8) = ctx[4];
        ((u16 *)ctx)[10] = 0;
        ((u16 *)ctx)[11] = 0;
        ctx[4] = 0;
        ctx[6] &= ~0x38u;

        head_ptr = (void **)ctx[2];
        if (*head_ptr == NULL)
            *head_ptr = node;
        if (ctx[3] != 0)
            *(void **)((u8 *)ctx[3] + 12) = node;
        ctx[3] = (u32)node;
        ctx[6] |= 4u;
    }

    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[6] & 5u) == 5u) {
            if (ctx[0] != 2u || (ctx[6] & 2u) != 0) {
                ctx[6] |= 0x40u;
                return 0;
            }
            ret = (int)0x8055070B;
            goto fail;
        }
        ret = (int)0x80550708;
        goto fail;
    }
    return 0;

fail:
    np_free_fqdn_list(*(void **)ctx[2]);
    *(void **)ctx[2] = NULL;
    return ret;
}

int np_xml_fqdn_init(void *parser, int mode, void *title_id, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    *(void **)ctx[2] = NULL;
    ctx[1] = (u32)title_id;
    ctx[0] = (u32)mode;
    ctx[6] = 0;
    ctx[3] = 0;
    if (mode == 1)
        return sceNp_6999EDC4(parser, &g_xml_searchjid_fqdn_schema, xml_fqdn_cb, ctx);
    if (mode == 2)
        return sceNp_6999EDC4(parser, &g_xml_matching2_fqdn_schema, xml_fqdn_cb, ctx);
    return (int)0x80550782;
}

int np_xml_fqdn_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[6] & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Search JID XML callback --- */

static int xml_searchjid_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, idx, s1, s2);
    if (ret < 0)
        return ret;

    ctx[1] |= 1u;
    if (ev == 3 && depth == 1 && idx == 0) {
        ret = np_jid_str_to_npid((void *)ctx[0], s1);
        if (ret < 0)
            return ret;
        ctx[1] |= 2u;
    }
    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[1] & 3u) == 3u) {
            ctx[1] |= 4u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_searchjid_init(void *parser, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    ctx[1] = 0;
    return sceNp_6999EDC4(parser, &g_xml_searchjid_schema, xml_searchjid_cb, ctx);
}

int np_xml_searchjid_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[1] & 4u) != 0) ? 0 : (int)0x80550708;
}

/* --- Uploader list XML callback --- */

static int xml_uploader_list_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, idx, s1, s2);
    if (ret < 0)
        goto fail;

    ctx[5] |= 1u;
    if (ev == 4 && depth == 1 && idx == 0) {
        if (sceNet_lib_A0F16ABD("num", s1) == 0) {
            if (s2 == NULL) { ret = (int)0x80550708; goto fail; }
            ctx[1] = (u32)sceNet_lib_1FB2FDDD(s2);
            ctx[5] |= 2u;
        }
    }
    if (ev == 1 && depth == 2 && idx == 0) {
        if (ctx[2] < ctx[1]) {
            void *node = np_alloc(152);
            if (node == NULL) { ret = (int)0x80550703; goto fail; }
            sceKernelMemset(node, 0, 152);
            ctx[3] = (u32)node;
        }
    }
    if (ev == 3 && depth == 3) {
        u8 *node = (u8 *)ctx[3];
        if (node != NULL) {
            if (idx == 0) {
                if ((u32)sceNet_lib_D8722983(s1) >= 64u) { ret = (int)0x80550708; goto fail; }
                sceNet_lib_B5CE388A((char *)node, s1, 64);
            } else if (idx == 1) {
                ret = sceNp_64F72B22(node + 112, s1);
                if (ret < 0)
                    goto fail;
            } else if (idx == 2) {
                *(s64 *)(node + 64) = np_parse_s64(s1);
            } else if (idx == 3) {
                if (sceRtcParseDateTime((u64 *)(node + 128), s1) < 0) { ret = (int)0x80550708; goto fail; }
            } else if (idx == 4) {
                *(s64 *)(node + 136) = np_parse_s64(s1);
            }
        }
    }
    if (ev == 2 && depth == 2 && idx == 0) {
        u8 *node = (u8 *)ctx[3];
        if (node != NULL) {
            void **head_ptr = (void **)ctx[0];
            if (*head_ptr == NULL)
                *head_ptr = node;
            if (ctx[4] != 0)
                *(void **)((u8 *)ctx[4] + 144) = node;
            ctx[4] = (u32)node;
            ctx[2]++;
            ctx[3] = 0;
        }
    }
    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[5] & 3u) == 3u && ctx[1] == ctx[2]) {
            ctx[5] |= 4u;
            return 0;
        }
        ret = (int)0x80550708;
        goto fail;
    }
    return 0;

fail:
    {
        u8 *cur = *(u8 **)ctx[0];
        while (cur != NULL) {
            u8 *next = *(u8 **)(cur + 144);
            np_free(cur);
            cur = next;
        }
        *(void **)ctx[0] = NULL;
    }
    return ret;
}

int np_xml_uploader_list_init(void *parser, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    *(void **)ctx[0] = NULL;
    ctx[1] = 0;
    ctx[2] = 0;
    ctx[3] = 0;
    ctx[4] = 0;
    ctx[5] = 0;
    return sceNp_6999EDC4(parser, &g_xml_uploader_list_schema, xml_uploader_list_cb, ctx);
}

int np_xml_uploader_list_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[5] & 4u) != 0) ? 0 : (int)0x80550708;
}

/* --- Uploader download info XML callback --- */

static int xml_uploader_dl_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;
    u8 *info = (u8 *)ctx[0];

    (void)parser;

    ret = xml_check_result_attr(ev, depth, idx, s1, s2);
    if (ret < 0)
        return ret;

    ctx[1] |= 1u;
    if (ev == 3 && depth == 1) {
        if (idx == 0) {
            ret = np_jid_str_to_npid(info + 12, s1);
            if (ret < 0)
                return ret;
            ctx[1] |= 2u;
        } else if (idx == 1) {
            if (sceNet_lib_B9085A96(s1, "sys", 4) == 0)
                *(int *)(info + 8) = 1;
            else if (sceNet_lib_B9085A96(s1, "game", 5) == 0)
                *(int *)(info + 8) = 2;
            else if (sceNet_lib_B9085A96(s1, "premo", 6) == 0)
                *(int *)(info + 8) = 3;
            ctx[1] |= 4u;
        } else if (idx == 2) {
            ret = sceNp_64F72B22(info + 48, s1);
            if (ret < 0)
                return ret;
            ctx[1] |= 8u;
        } else if (idx == 3) {
            *(s64 *)info = np_parse_s64(s1);
            ctx[1] |= 0x10u;
        } else if (idx == 4) {
            if (sceRtcParseDateTime((u64 *)(info + 64), s1) < 0)
                return (int)0x80550708;
            ctx[1] |= 0x20u;
        }
    }
    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[1] & 0x3Fu) == 0x3Fu) {
            ctx[1] |= 0x40u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_uploader_dl_init(void *parser, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    ctx[1] = 0;
    return sceNp_6999EDC4(parser, &g_xml_uploader_dl_schema, xml_uploader_dl_cb, ctx);
}

int np_xml_uploader_dl_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[1] & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Avatar category XML callback --- */

static int xml_avatar_cat_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, idx, s1, s2);
    if (ret < 0)
        return ret;

    ctx[4] |= 1u;
    if (ev == 3 && depth == 1 && idx == 0) {
        ctx[1] = (u32)sceNet_lib_1FB2FDDD(s1);
        ctx[4] |= 2u;
    }
    if (ev == 2 && depth == 1 && idx == 1)
        ctx[4] |= 4u;
    if (depth == 2 && idx == 0) {
        if (ctx[2] < ctx[3]) {
            ret = xml_parse_category(ev, s1, s2, (int *)(ctx[0] + 408u * ctx[2]), &ctx[4], &ctx[2]);
            if (ret < 0)
                return ret;
        }
    }
    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[4] & 7u) == 7u) {
            ctx[4] |= 0x40u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_avatar_cat_init(void *parser, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    ctx[4] = 0;
    ctx[2] = 0;
    sceKernelMemset((void *)ctx[0], 0, 408u * ctx[3]);
    return sceNp_6999EDC4(parser, &g_xml_avatar_cat_schema, xml_avatar_cat_cb, ctx);
}

int np_xml_avatar_cat_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[4] & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Avatar list XML callback --- */

static int xml_avatar_list_cb(void *parser, int ev, int depth, int idx, const char *s1, const char *s2, u32 *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, idx, s1, s2);
    if (ret < 0)
        return ret;

    ctx[5] |= 1u;
    if (ev == 3 && depth == 1 && idx == 0) {
        ctx[2] = (u32)sceNet_lib_1FB2FDDD(s1);
        ctx[5] |= 2u;
    }
    if (ev == 2 && depth == 1 && idx == 1)
        ctx[5] |= 4u;
    if (depth == 2 && idx == 0) {
        if (ctx[3] < ctx[4]) {
            ret = xml_parse_avatarurl(ev, s1, s2, (char *)(ctx[0] + (ctx[3] << 7)),
                                      (int *)(ctx[1] + 4u * ctx[3]), &ctx[5], &ctx[3]);
            if (ret < 0)
                return ret;
        }
    }
    if (ev == 2 && depth == 0 && idx == 0) {
        if ((ctx[5] & 7u) == 7u) {
            ctx[5] |= 0x10u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_avatar_list_init(void *parser, void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    ctx[5] = 0;
    ctx[3] = 0;
    sceKernelMemset((void *)ctx[1], 0, 4u * ctx[4]);
    sceKernelMemset((void *)ctx[0], 0, ctx[4] << 7);
    return sceNp_6999EDC4(parser, &g_xml_avatar_list_schema, xml_avatar_list_cb, ctx);
}

int np_xml_avatar_list_check(void *ctx_ptr)
{
    u32 *ctx = (u32 *)ctx_ptr;
    return ((ctx[5] & 0x10u) != 0) ? 0 : (int)0x80550708;
}

/* =========================================================================
 * Validation, HMAC Signature, HTTP Helpers, JID Conversion, SSL Error Mapping
 * ========================================================================= */

int np_validate_online_id(const char *s)
{
    u32 len;
    u32 i;

    if (s == NULL)
        return 0;
    len = (u32)sceNet_lib_D8722983(s);
    if (len < 3u || len > 16u)
        return 0;
    if ((sceNet_lib_750F705D((unsigned char)s[0]) & 3u) == 0)
        return 0;
    for (i = 1; i < len; i++) {
        char c = s[i];
        if ((sceNet_lib_750F705D((unsigned char)c) & 7u) == 0 && c != '-' && c != '_')
            return 0;
    }
    return 1;
}

static const u8 g_hmac_key_mode3[20] = {
    0xAF, 0x15, 0xD8, 0x03, 0x00, 0xDE, 0x1B, 0x7D, 0x4C, 0xA9,
    0xC3, 0x1C, 0x05, 0x26, 0x5F, 0xE3, 0x21, 0x6E, 0x9E, 0xA6,
};

static const u8 g_hmac_key_mode4[20] = {
    0x46, 0x75, 0x39, 0x33, 0x63, 0x4D, 0x1A, 0x1D, 0x46, 0x41,
    0x42, 0x75, 0x21, 0x10, 0x11, 0x6F, 0x2C, 0x5E, 0x78, 0x36,
};

int np_verify_hmac_sig(const void *data, int len, const u8 *sig, int mode)
{
    const u8 *key_src;
    u8 key[20];
    u8 digest[20];
    int padded_len;
    void *buf;
    int i;
    int cmp;

    if (mode == 3)
        key_src = g_hmac_key_mode3;
    else if (mode == 4)
        key_src = g_hmac_key_mode4;
    else
        return (int)0x8055070D;

    sceKernelMemset(digest, 0, 20);
    padded_len = len + ((-len) & 0x3F);
    buf = np_alloc((SceSize)padded_len);
    if (buf == NULL)
        return (int)0x80550703;

    sceKernelMemset(buf, 0, (SceSize)padded_len);
    sceKernelMemcpy(buf, data, (SceSize)len);
    for (i = 0; i < 20; i++)
        key[i] = (u8)(key_src[i] ^ 0x7Fu);

    sceNet_lib_384EFE14(buf, padded_len, key, 20, digest);
    np_free(buf);
    cmp = sceNet_lib_E0A81C7C(sig, digest, 20);
    return (cmp != 0) ? (int)0x8055070B : 0;
}

int np_http_read_exact(void *trans_ptr, void *buf, u32 len, u32 *read_out)
{
    int *trans = (int *)trans_ptr;
    u32 rem = len;
    u32 done = 0;

    while (rem != 0) {
        int r;
        if (trans[6] == 1)
            return (int)0x80550707;
        r = sceHttpReadData(trans[4], (u8 *)buf + done, rem);
        if (r < 0)
            return np_map_ssl_error(trans[4], r);
        if (r == 0) {
            *read_out = done;
            return 0;
        }
        done += (u32)r;
        rem -= (u32)r;
    }
    *read_out = len;
    return 0;
}

static int np_http_read_comment_sig(void *trans_ptr, u8 *sig_out, u64 *clen)
{
    int *trans = (int *)trans_ptr;
    u8 buf[80];
    char hex[4];
    u32 got = 0;
    int ret;
    int prefix_len;
    int i;
    u32 consumed;

    ret = sceKernelLockLwMutex(trans + 15, 1, NULL);
    if (ret < 0)
        return ret;
    if (trans[6] == 1)
        ret = (int)0x80550707;
    sceKernelUnlockLwMutex(trans + 15, 1);
    if (ret < 0)
        return ret;

    ret = np_http_read_exact(trans, buf, 65, &got);
    if (ret < 0)
        return ret;
    if (got != 65u)
        return (int)0x80550708;

    prefix_len = sceNet_lib_D8722983("<!--")
               + sceNet_lib_D8722983("Sce-Np-Signature")
               + sceNet_lib_D8722983(": ");
    if (sceNet_lib_B9085A96((const char *)buf, "<!--Sce-Np-Signature: ", prefix_len) != 0 ||
        65 - prefix_len < 40)
        return (int)0x80550708;

    for (i = 0; i < 20; i++) {
        u32 val;
        sceKernelMemcpy(hex, buf + prefix_len + 2 * i, 2);
        hex[2] = '\0';
        val = sceNet_lib_2A73ADDC(hex, NULL, 16);
        if (val >= 0x100u)
            return (int)0x80550708;
        sig_out[i] = (u8)val;
    }

    ret = np_http_read_exact(trans, buf, 1, &got);
    if (ret < 0)
        return ret;
    if (got != 1u)
        return (int)0x80550708;
    consumed = 66;
    if (buf[0] == '\r') {
        ret = np_http_read_exact(trans, buf, 1, &got);
        if (ret < 0)
            return ret;
        if (got != 1u)
            return (int)0x80550708;
        consumed = 67;
    }
    if (buf[0] != '\n')
        return (int)0x80550708;
    *clen -= consumed;
    return 0;
}

int np_http_parse_headers(void *trans_ptr, int *status_out, u64 *clen_out,
                          u32 *info_len_out, u8 *sig_out, int mode)
{
    int *trans = (int *)trans_ptr;
    const char *all_hdr = NULL;
    u32 all_hdr_len = 0;
    const char *val = NULL;
    u32 val_len = 0;
    int ret;

    if (mode != 1 && sig_out == NULL)
        return (int)0x8055070C;

    ret = sceHttpGetStatusCode(trans[4], status_out);
    if (ret < 0)
        return ret;
    if (*status_out != 200) {
        if ((u32)(mode - 3) >= 2u)
            return (int)0x8055070A;
        return (int)0x805508A6;
    }

    ret = sceHttpGetContentLength(trans[4], clen_out);
    if (ret < 0)
        return ret;
    ret = sceHttpGetAllHeader(trans[4], &all_hdr, &all_hdr_len);
    if (ret < 0)
        return ret;

    if (sceParseHttpResponseHeader(all_hdr, all_hdr_len, "Sce-Np-Info-Length", &val, &val_len) >= 0 &&
        info_len_out != NULL) {
        *info_len_out = sceNet_lib_2A73ADDC(val, NULL, 10);
    }

    if (mode == 2) {
        if (sceParseHttpResponseHeader(all_hdr, all_hdr_len, "Sce-Np-Signature", &val, &val_len) >= 0 &&
            sig_out != NULL) {
            char hex[4];
            int i;
            if (val_len < 40u)
                return (int)0x80550708;
            for (i = 0; i < 20; i++) {
                u32 v;
                sceKernelMemcpy(hex, val + 2 * i, 2);
                hex[2] = '\0';
                v = sceNet_lib_2A73ADDC(hex, NULL, 16);
                if (v >= 0x100u)
                    return (int)0x80550708;
                sig_out[i] = (u8)v;
            }
        }
        return 0;
    }

    if ((u32)(mode - 3) < 2u) {
        ret = np_http_read_comment_sig(trans, sig_out, clen_out);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int np_jid_str_to_npid(void *npid, const char *jid_str)
{
    u8 jid[16];
    u32 req_len = 0;
    void *buf;
    int ret;

    ret = sceNp_5FA879D8(jid, jid_str, NULL, 0, &req_len);
    if (ret < 0)
        return ret;
    if (req_len == 0)
        return (int)0x80550708;
    buf = np_alloc(req_len);
    if (buf == NULL)
        return (int)0x80550703;
    ret = sceNp_5FA879D8(jid, jid_str, buf, req_len, NULL);
    if (ret >= 0) {
        ret = sceNp_C0B3616C(npid, jid);
        if (ret > 0)
            ret = 0;
    }
    np_free(buf);
    return ret;
}

int np_npid_to_jid_str(char **out_str, const void *npid)
{
    u32 jid[4];
    u32 req_len = 0;
    void *tmp_buf = NULL;
    char *str_buf = NULL;
    int ret;

    ret = sceNp_B2BFADB2(NULL, npid, NULL, 0, &req_len);
    if (ret < 0)
        return ret;
    tmp_buf = np_alloc(req_len);
    if (tmp_buf == NULL)
        return (int)0x80550703;
    ret = sceNp_B2BFADB2(jid, npid, tmp_buf, req_len, NULL);
    if (ret < 0)
        goto fail;
    jid[2] = 0;
    ret = sceNp_0B8BEE48(jid, NULL, 0, &req_len);
    if (ret < 0)
        goto fail;
    str_buf = (char *)np_alloc(req_len);
    if (str_buf == NULL) {
        ret = (int)0x80550703;
        goto fail;
    }
    ret = sceNp_0B8BEE48(jid, str_buf, req_len, NULL);
    if (ret < 0)
        goto fail;
    np_free(tmp_buf);
    *out_str = str_buf;
    return 0;

fail:
    if (tmp_buf != NULL)
        np_free(tmp_buf);
    if (str_buf != NULL)
        np_free(str_buf);
    return ret;
}

int np_xml_escape_str(char **out_str, const char *src)
{
    int slen = sceNet_lib_D8722983(src);
    char *buf;
    u32 req_len = 0;
    int ret;

    if (slen == 0) {
        buf = (char *)np_alloc(1);
        *buf = '\0';
        *out_str = buf;
        return 0;
    }

    ret = sceNp_E41288E7(NULL, 0, src, (u32)slen, &req_len);
    if (ret < 0) {
        *out_str = NULL;
        return ret;
    }
    buf = (char *)np_alloc(req_len);
    if (buf == NULL) {
        *out_str = NULL;
        return (int)0x80550703;
    }
    sceKernelMemset(buf, 0, req_len);
    ret = sceNp_E41288E7(buf, req_len, src, (u32)slen, NULL);
    if (ret < 0) {
        np_free(buf);
        *out_str = NULL;
        return ret;
    }
    *out_str = buf;
    return 0;
}

int np_map_ssl_error(int req_id, int err)
{
    if (err == (int)0x80431075) {
        int ssl_err = 0;
        u32 detail = 0;
        int r = sceHttpsGetSslError(req_id, &ssl_err, &detail);
        if (r < 0)
            return r;
        if (ssl_err == (int)0x80435060) {
            if ((detail & 4u) != 0)    return (int)0x80550783;
            if ((detail & 0x20u) != 0) return (int)0x80550784;
            if ((detail & 2u) != 0)    return (int)0x80550785;
            if ((detail & 1u) != 0)    return (int)0x80550786;
            return (int)0x80550787;
        }
        return ssl_err;
    }
    return err;
}

s64 np_parse_s64(const char *s)
{
    int neg = 0;
    u32 i = 0;
    u64 val = 0;

    if (s != NULL) {
        if (s[0] == '-') {
            neg = 1;
            i = 1;
        }
        while (s[i] != '\0') {
            if ((sceNet_lib_750F705D((unsigned char)s[i]) & 8u) != 0 || i >= 20u)
                break;
            val = val * 10ULL + (u64)(unsigned char)(s[i] - '0');
            i++;
        }
    }
    return neg ? -(s64)val : (s64)val;
}

int np_trans_close_http(void *trans_ptr)
{
    int *trans = (int *)trans_ptr;
    int ret = 0;

    if (trans[4] > 0) {
        ret = sceKernelLockLwMutex(trans + 15, 1, NULL);
        if (ret < 0)
            return ret;
        ret = sceHttpDeleteRequest(trans[4]);
        trans[4] = 0;
        trans[7] = 0;
        sceKernelUnlockLwMutex(trans + 15, 1);
    }
    if (trans[3] > 0) {
        ret = sceHttpDeleteConnection(trans[3]);
        trans[3] = 0;
    }
    return ret;
}

int np_free_fqdn_list(void *head)
{
    u8 *cur = (u8 *)head;
    while (cur != NULL) {
        u8 *next = *(u8 **)(cur + 12);
        np_free(cur);
        cur = next;
    }
    return 0;
}
