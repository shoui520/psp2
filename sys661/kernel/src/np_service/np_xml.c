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

static const XmlNodeDef g_xml_profile_root[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_profile_fields[] = {
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
    g_xml_profile_root, g_xml_profile_fields,
};
static const XmlSchema g_xml_profile_schema = { g_xml_profile_levels, 2 };

static const XmlNodeDef g_xml_searchjid_fqdn_root[] = {
    { "searchjid", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_searchjid_fqdn_fields[] = {
    { "searchjid-fqdn", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_searchjid_fqdn_levels[] = {
    g_xml_searchjid_fqdn_root, g_xml_searchjid_fqdn_fields,
};
static const XmlSchema g_xml_searchjid_fqdn_schema = { g_xml_searchjid_fqdn_levels, 2 };

static const XmlNodeDef g_xml_matching_fqdn_root[] = {
    { "matching2", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_matching_fqdn_fields[] = {
    { "agent-fqdn", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_matching_fqdn_levels[] = {
    g_xml_matching_fqdn_root, g_xml_matching_fqdn_fields,
};
static const XmlSchema g_xml_matching_fqdn_schema = { g_xml_matching_fqdn_levels, 2 };

static const XmlNodeDef g_xml_searchjid_root[] = {
    { "searchjid", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_searchjid_fields[] = {
    { "jid", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_searchjid_levels[] = {
    g_xml_searchjid_root, g_xml_searchjid_fields,
};
static const XmlSchema g_xml_searchjid_schema = { g_xml_searchjid_levels, 2 };

static const XmlNodeDef g_xml_uploader_list_root[] = {
    { "uploader", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_container[] = {
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_entry[] = {
    { "data-info", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_list_fields[] = {
    { "reference", 1 },
    { "application", 1 },
    { "size", 1 },
    { "date", 1 },
    { "keep", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_uploader_list_levels[] = {
    g_xml_uploader_list_root, g_xml_uploader_list_container,
    g_xml_uploader_list_entry, g_xml_uploader_list_fields,
};
static const XmlSchema g_xml_uploader_list_schema = { g_xml_uploader_list_levels, 4 };

static const XmlNodeDef g_xml_uploader_dl_root[] = {
    { "uploader", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_uploader_dl_fields[] = {
    { "jid", 1 },
    { "quota", 1 },
    { "application", 1 },
    { "size", 1 },
    { "date", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_uploader_dl_levels[] = {
    g_xml_uploader_dl_root, g_xml_uploader_dl_fields,
};
static const XmlSchema g_xml_uploader_dl_schema = { g_xml_uploader_dl_levels, 2 };

static const XmlNodeDef g_xml_avatar_cat_root[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_cat_branch[] = {
    { "categories", 1 },
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_cat_leaf[] = {
    { "category", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_avatar_cat_levels[] = {
    g_xml_avatar_cat_root, g_xml_avatar_cat_branch, g_xml_avatar_cat_leaf,
};
static const XmlSchema g_xml_avatar_cat_schema = { g_xml_avatar_cat_levels, 3 };

static const XmlNodeDef g_xml_avatar_list_root[] = {
    { "profile", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_list_branch[] = {
    { "avatars", 1 },
    { "list", 0 },
    { "", 1 },
};
static const XmlNodeDef g_xml_avatar_list_leaf[] = {
    { "avatarurl", 1 },
    { "", 1 },
};
static const XmlNodeDef *const g_xml_avatar_list_levels[] = {
    g_xml_avatar_list_root, g_xml_avatar_list_branch, g_xml_avatar_list_leaf,
};
static const XmlSchema g_xml_avatar_list_schema = { g_xml_avatar_list_levels, 3 };

/* =========================================================================
 * Common XML parsing helpers
 * ========================================================================= */

int np_xml_feed(void *parser, const void *xml_chunk, u32 len)
{
    return sceNp_1916432C(parser, xml_chunk, len);
}

static int xml_check_result_attr(int ev, int depth, int tag_index, const char *name, const char *val)
{
    if (ev != 4 || depth != 0 || tag_index != 0)
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

static int xml_check_status_attr(int parse_kind, int ev, int depth, int tag_index, const char *name, const char *val)
{
    if (ev != 4 || depth != 0 || tag_index != 0)
        return 0;
    if (sceNet_lib_A0F16ABD("status", name) != 0)
        return 0;
    if (val == NULL)
        return (int)0x80550708;
    if (sceNet_lib_B9085A96(val, "alive", 6) == 0)
        return 0;
    if (sceNet_lib_B9085A96(val, "maintenance", 12) == 0)
        return (parse_kind == 2) ? (int)0x805508A2 : (int)0x805508A5;
    if (sceNet_lib_B9085A96(val, "end", 4) == 0)
        return (parse_kind == 2) ? (int)0x805508A1 : (int)0x805508A4;
    if (sceNet_lib_B9085A96(val, "before", 7) == 0)
        return (parse_kind == 2) ? (int)0x805508A0 : (int)0x805508A3;
    return (int)0x80550708;
}

static int xml_parse_onlinename(int ev, const char *text_or_name, const char *attr_val,
                                char *dst, int *upd_out, u64 *date_out)
{
    if (ev == 3)
        sceNet_lib_B5CE388A(dst, text_or_name, 48);
    if (ev != 4)
        return 0;
    if (sceNet_lib_A0F16ABD("upd", text_or_name) == 0) {
        *upd_out = sceNet_lib_1FB2FDDD(attr_val);
        return 0;
    }
    if (sceNet_lib_A0F16ABD("date", text_or_name) != 0)
        return 0;
    if (sceRtcParseDateTime(date_out, attr_val) < 0)
        return (int)0x80550708;
    return 0;
}

static int xml_parse_avatarurl(int ev, const char *text_or_name, const char *attr_val,
                               char *dst, int *id_out, u32 *seen_mask, u32 *count)
{
    if (ev == 4) {
        if (sceNet_lib_A0F16ABD("id", text_or_name) == 0 && id_out != NULL)
            *id_out = sceNet_lib_1FB2FDDD(attr_val);
        if (seen_mask != NULL)
            *seen_mask |= 8u;
    }
    if (ev == 3) {
        if ((u32)sceNet_lib_D8722983(text_or_name) >= 128u)
            return (int)0x80550708;
        sceNet_lib_B5CE388A(dst, text_or_name, 127);
        if (seen_mask != NULL) {
            if ((*seen_mask & 8u) == 0)
                return (int)0x80550708;
            *seen_mask &= ~8u;
        }
        if (count != NULL)
            (*count)++;
    }
    return 0;
}

static int xml_parse_category(int ev, const char *text_or_name, const char *attr_val,
                              NpAvatarCategory *entry, u32 *seen_mask, u32 *count)
{
    if (ev == 4) {
        if (sceNet_lib_A0F16ABD("id", text_or_name) == 0) {
            entry->category_id = sceNet_lib_1FB2FDDD(attr_val);
            *seen_mask |= 8u;
        } else if (sceNet_lib_A0F16ABD("avatars", text_or_name) == 0) {
            entry->avatar_count = sceNet_lib_1FB2FDDD(attr_val);
            *seen_mask |= 16u;
        }
    }
    if (ev == 3) {
        sceNet_lib_B5CE388A(entry->name, text_or_name, 385);
        if ((*seen_mask & 0x18u) != 0x18u)
            return (int)0x80550708;
        (*count)++;
    }
    return 0;
}

/* --- Profile XML callback --- */

static int xml_profile_cb(void *parser, int ev, int depth, u32 tag_index,
                          const char *text_or_name, const char *attr_val,
                          NpXmlProfileCtx *ctx)
{
    int ret;
    NpProfileRecord *prof = ctx->profile;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, (int)tag_index, text_or_name, attr_val);
    if (ret < 0)
        return ret;

    ctx->seen_mask |= 1u;
    if (ev == 3 && depth == 1 && tag_index == 0) {
        ret = np_jid_str_to_npid(&prof->base.npid, text_or_name);
        if (ret < 0)
            return ret;
        ctx->seen_mask |= 2u;
    }
    if (depth == 1 && tag_index == 1) {
        ret = xml_parse_onlinename(ev, text_or_name, attr_val, prof->base.online_name,
                                   &ctx->update_flag, &ctx->updated_date);
        if (ret < 0)
            return ret;
        if (ev == 3)
            ctx->seen_mask |= 4u;
    }
    if (ev == 3 && depth == 1 && tag_index == 2) {
        sceKernelMemcpy(&prof->country_code, text_or_name, 2);
        ctx->seen_mask |= 8u;
    }
    if (ev == 3 && depth == 1 && tag_index >= 3u && tag_index < 6u) {
        int lang = -1;
        if (sceNet_lib_D8722983(text_or_name) != 0) {
            lang = sceNet_lib_1FB2FDDD(text_or_name);
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
        if (tag_index == 3u) {
            prof->languages.codes[0] = lang;
            ctx->seen_mask |= 0x10u;
        } else if (tag_index == 4u) {
            prof->languages.codes[1] = (sceNet_lib_D8722983(text_or_name) != 0) ? lang : -1;
            ctx->seen_mask |= 0x20u;
        } else {
            prof->languages.codes[2] = (sceNet_lib_D8722983(text_or_name) != 0) ? lang : -1;
            ctx->seen_mask |= 0x40u;
        }
    }
    if (ev == 3 && depth == 1 && tag_index == 6u) {
        if (text_or_name[0] != '\0') {
            if ((u32)(sceNet_lib_D8722983(text_or_name) + 1) >= 65u)
                return (int)0x80550708;
            sceNet_lib_B5CE388A(prof->about_me.text, text_or_name, 64);
        }
        ctx->seen_mask |= 0x80u;
    }
    if (depth == 1 && tag_index == 7u) {
        ret = xml_parse_avatarurl(ev, text_or_name, attr_val, prof->base.avatar_url,
                                  &prof->avatar_id, NULL, NULL);
        if (ret < 0)
            return ret;
        if (ev == 3)
            ctx->seen_mask |= 0x100u;
    }
    if (ev == 3 && depth == 1 && tag_index == 8u) {
        ctx->ptlp = sceNet_lib_1FB2FDDD(text_or_name);
        ctx->seen_mask |= 0x200u;
    }
    if (ev == 2 && depth == 0 && tag_index == 0u) {
        if ((ctx->seen_mask & 0x3FDu) == 0x3FDu) {
            if (ctx->parse_kind != 1 || (ctx->seen_mask & 2u) != 0) {
                ctx->seen_mask |= 0x400u;
                return 0;
            }
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_profile_init(void *parser, int parse_kind, NpXmlProfileCtx *ctx)
{
    NpProfileRecord *prof = ctx->profile;

    ctx->parse_kind = parse_kind;
    ctx->seen_mask = 0;
    sceKernelMemset(prof, 0, sizeof(NpProfileRecord));
    ctx->profile->languages.codes[2] = -1;
    ctx->profile->languages.codes[0] = -1;
    ctx->profile->languages.codes[1] = -1;
    return sceNp_6999EDC4(parser, &g_xml_profile_schema, xml_profile_cb, ctx);
}

int np_xml_profile_check(NpXmlProfileCtx *ctx)
{
    return ((ctx->seen_mask & 0x400u) != 0) ? 0 : (int)0x80550708;
}

/* --- FQDN list XML callback --- */

static int xml_fqdn_cb(void *parser, int ev, int depth, int tag_index,
                       const char *text_or_name, const char *attr_val,
                       NpXmlFqdnCtx *ctx)
{
    int ret;
    char title_buf[16];

    (void)parser;

    ret = xml_check_status_attr(ctx->parse_kind, ev, depth, tag_index, text_or_name, attr_val);
    if (ret >= 0) {
        ctx->seen_mask |= 1u;
        if (ctx->parse_kind == 2) {
            if (ev == 4 && depth == 0 && tag_index == 0 &&
                sceNet_lib_A0F16ABD("titleid", text_or_name) == 0) {
                if (attr_val == NULL) {
                    ret = (int)0x8055070B;
                } else {
                    sceKernelMemset(title_buf, 0, 13);
                    ret = sceNp_E90DFBC4(ctx->title_id, title_buf, 13);
                    if (ret >= 0 &&
                        sceNet_lib_B9085A96(attr_val, title_buf, sceNet_lib_D8722983(title_buf) + 1) != 0)
                        ret = (int)0x8055070B;
                }
            }
            if (ret >= 0)
                ctx->seen_mask |= 2u;
        }
    }

    if (ret >= 0 && ev == 4 && depth == 1 && tag_index == 0) {
        if (sceNet_lib_A0F16ABD("id", text_or_name) == 0) {
            if (attr_val == NULL) {
                ret = (int)0x80550708;
            } else {
                ctx->pending_id = (u16)sceNet_lib_1FB2FDDD(attr_val);
                ctx->seen_mask |= 8u;
            }
        } else if (sceNet_lib_A0F16ABD("status", text_or_name) == 0) {
            if (attr_val == NULL) {
                ret = (int)0x80550708;
            } else if (sceNet_lib_B9085A96(attr_val, "alive", 6) == 0) {
                ctx->pending_status = 1;
                ctx->seen_mask |= 0x10u;
            } else if (sceNet_lib_B9085A96(attr_val, "maintenance", 12) == 0) {
                ctx->pending_status = 2;
                ctx->seen_mask |= 0x10u;
            } else {
                ret = (int)0x80550708;
            }
        } else if (sceNet_lib_A0F16ABD("port", text_or_name) == 0) {
            if (attr_val == NULL) {
                ret = (int)0x80550708;
            } else {
                ctx->pending_port = (u16)sceNet_lib_1FB2FDDD(attr_val);
                ctx->seen_mask |= 0x20u;
            }
        }
    }

    if (ret >= 0 && ev == 3 && depth == 1 && tag_index == 0) {
        if (text_or_name[0] == '\0' || (ctx->parse_kind == 2 && (ctx->seen_mask & 0x38u) != 0x38u)) {
            ret = (int)0x80550708;
        } else {
            int slen = sceNet_lib_D8722983(text_or_name);
            NpFqdnEntry *entry = (NpFqdnEntry *)np_alloc((SceSize)(slen + sizeof(NpFqdnEntry) + 1));
            if (entry == NULL) {
                ret = (int)0x80550703;
            } else {
                char *host_str = (char *)(entry + 1);
                sceKernelMemset(entry, 0, sizeof(NpFqdnEntry));
                sceNet_lib_B5CE388A(host_str, text_or_name, slen + 1);
                entry->host = host_str;
                entry->port = ctx->pending_port;
                entry->server_id = ctx->pending_id;
                entry->status = ctx->pending_status;
                ctx->pending_id = 0;
                ctx->pending_port = 0;
                ctx->pending_status = 0;
                ctx->seen_mask &= ~0x38u;

                if (*ctx->list_head == NULL)
                    *ctx->list_head = entry;
                if (ctx->list_tail != NULL)
                    ctx->list_tail->next = entry;
                ctx->list_tail = entry;
                ctx->seen_mask |= 4u;
            }
        }
    }

    if (ret >= 0 && ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 5u) == 5u) {
            if (ctx->parse_kind != 2 || (ctx->seen_mask & 2u) != 0) {
                ctx->seen_mask |= 0x40u;
                return 0;
            }
            ret = (int)0x8055070B;
        } else {
            ret = (int)0x80550708;
        }
    }

    if (ret < 0) {
        np_free_fqdn_list(*ctx->list_head);
        *ctx->list_head = NULL;
    }
    return ret;
}

int np_xml_fqdn_init(void *parser, int parse_kind, const void *title_id, NpXmlFqdnCtx *ctx)
{
    *ctx->list_head = NULL;
    ctx->title_id = title_id;
    ctx->parse_kind = parse_kind;
    ctx->seen_mask = 0;
    ctx->list_tail = NULL;
    if (parse_kind == 1)
        return sceNp_6999EDC4(parser, &g_xml_searchjid_fqdn_schema, xml_fqdn_cb, ctx);
    if (parse_kind == 2)
        return sceNp_6999EDC4(parser, &g_xml_matching_fqdn_schema, xml_fqdn_cb, ctx);
    return (int)0x80550782;
}

int np_xml_fqdn_check(NpXmlFqdnCtx *ctx)
{
    return ((ctx->seen_mask & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Search JID XML callback --- */

static int xml_searchjid_cb(void *parser, int ev, int depth, int tag_index,
                            const char *text_or_name, const char *attr_val,
                            NpXmlSearchJidCtx *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, tag_index, text_or_name, attr_val);
    if (ret < 0)
        return ret;

    ctx->seen_mask |= 1u;
    if (ev == 3 && depth == 1 && tag_index == 0) {
        ret = np_jid_str_to_npid(ctx->npid_out, text_or_name);
        if (ret < 0)
            return ret;
        ctx->seen_mask |= 2u;
    }
    if (ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 3u) == 3u) {
            ctx->seen_mask |= 4u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_searchjid_init(void *parser, NpXmlSearchJidCtx *ctx)
{
    ctx->seen_mask = 0;
    return sceNp_6999EDC4(parser, &g_xml_searchjid_schema, xml_searchjid_cb, ctx);
}

int np_xml_searchjid_check(NpXmlSearchJidCtx *ctx)
{
    return ((ctx->seen_mask & 4u) != 0) ? 0 : (int)0x80550708;
}

/* --- Uploader list XML callback --- */

static int xml_uploader_list_cb(void *parser, int ev, int depth, int tag_index,
                                const char *text_or_name, const char *attr_val,
                                NpXmlUploaderListCtx *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, tag_index, text_or_name, attr_val);
    if (ret >= 0) {
        ctx->seen_mask |= 1u;
        if (ev == 4 && depth == 1 && tag_index == 0 &&
            sceNet_lib_A0F16ABD("num", text_or_name) == 0) {
            if (attr_val == NULL) {
                ret = (int)0x80550708;
            } else {
                ctx->expected_count = (u32)sceNet_lib_1FB2FDDD(attr_val);
                ctx->seen_mask |= 2u;
            }
        }
    }
    if (ret >= 0 && ev == 1 && depth == 2 && tag_index == 0) {
        if (ctx->parsed_count < ctx->expected_count) {
            NpUploaderEntry *entry = (NpUploaderEntry *)np_alloc(sizeof(NpUploaderEntry));
            if (entry == NULL) {
                ret = (int)0x80550703;
            } else {
                sceKernelMemset(entry, 0, sizeof(NpUploaderEntry));
                ctx->current = entry;
            }
        }
    }
    if (ret >= 0 && ev == 3 && depth == 3) {
        NpUploaderEntry *entry = ctx->current;
        if (entry != NULL) {
            if (tag_index == 0) {
                if ((u32)sceNet_lib_D8722983(text_or_name) >= 64u)
                    ret = (int)0x80550708;
                else
                    sceNet_lib_B5CE388A(entry->reference, text_or_name, 64);
            } else if (tag_index == 1) {
                ret = sceNp_64F72B22(entry->application, text_or_name);
            } else if (tag_index == 2) {
                entry->file_size = np_parse_s64(text_or_name);
            } else if (tag_index == 3) {
                if (sceRtcParseDateTime(&entry->created_date, text_or_name) < 0)
                    ret = (int)0x80550708;
            } else if (tag_index == 4) {
                entry->keep_days = np_parse_s64(text_or_name);
            }
        }
    }
    if (ret >= 0 && ev == 2 && depth == 2 && tag_index == 0) {
        NpUploaderEntry *entry = ctx->current;
        if (entry != NULL) {
            if (*ctx->list_head == NULL)
                *ctx->list_head = entry;
            if (ctx->list_tail != NULL)
                ctx->list_tail->next = entry;
            ctx->list_tail = entry;
            ctx->parsed_count++;
            ctx->current = NULL;
        }
    }
    if (ret >= 0 && ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 3u) == 3u && ctx->expected_count == ctx->parsed_count) {
            ctx->seen_mask |= 4u;
            return 0;
        }
        ret = (int)0x80550708;
    }

    if (ret < 0) {
        NpUploaderEntry *cur = *ctx->list_head;
        while (cur != NULL) {
            NpUploaderEntry *next = cur->next;
            np_free(cur);
            cur = next;
        }
        *ctx->list_head = NULL;
    }
    return ret;
}

int np_xml_uploader_list_init(void *parser, NpXmlUploaderListCtx *ctx)
{
    ctx->current = NULL;
    *ctx->list_head = NULL;
    ctx->seen_mask = 0;
    ctx->expected_count = 0;
    ctx->parsed_count = 0;
    ctx->list_tail = NULL;
    return sceNp_6999EDC4(parser, &g_xml_uploader_list_schema, xml_uploader_list_cb, ctx);
}

int np_xml_uploader_list_check(NpXmlUploaderListCtx *ctx)
{
    return ((ctx->seen_mask & 4u) != 0) ? 0 : (int)0x80550708;
}

/* --- Uploader download info XML callback --- */

static int xml_uploader_dl_cb(void *parser, int ev, int depth, int tag_index,
                              const char *text_or_name, const char *attr_val,
                              NpXmlUploaderDlCtx *ctx)
{
    int ret;
    NpUploaderDlInfo *dl_info = ctx->dl_info;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, tag_index, text_or_name, attr_val);
    if (ret < 0)
        return ret;

    ctx->seen_mask |= 1u;
    if (ev == 3 && depth == 1) {
        if (tag_index == 0) {
            ret = np_jid_str_to_npid(&dl_info->npid, text_or_name);
            if (ret < 0)
                return ret;
            ctx->seen_mask |= 2u;
        } else if (tag_index == 1) {
            if (sceNet_lib_B9085A96(text_or_name, "sys", 4) == 0)
                dl_info->quota_kind = 1;
            else if (sceNet_lib_B9085A96(text_or_name, "game", 5) == 0)
                dl_info->quota_kind = 2;
            else if (sceNet_lib_B9085A96(text_or_name, "premo", 6) == 0)
                dl_info->quota_kind = 3;
            ctx->seen_mask |= 4u;
        } else if (tag_index == 2) {
            ret = sceNp_64F72B22(dl_info->application, text_or_name);
            if (ret < 0)
                return ret;
            ctx->seen_mask |= 8u;
        } else if (tag_index == 3) {
            dl_info->file_size = np_parse_s64(text_or_name);
            ctx->seen_mask |= 0x10u;
        } else if (tag_index == 4) {
            if (sceRtcParseDateTime(&dl_info->created_date, text_or_name) < 0)
                return (int)0x80550708;
            ctx->seen_mask |= 0x20u;
        }
    }
    if (ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 0x3Fu) == 0x3Fu) {
            ctx->seen_mask |= 0x40u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_uploader_dl_init(void *parser, NpXmlUploaderDlCtx *ctx)
{
    ctx->seen_mask = 0;
    return sceNp_6999EDC4(parser, &g_xml_uploader_dl_schema, xml_uploader_dl_cb, ctx);
}

int np_xml_uploader_dl_check(NpXmlUploaderDlCtx *ctx)
{
    return ((ctx->seen_mask & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Avatar category XML callback --- */

static int xml_avatar_cat_cb(void *parser, int ev, int depth, int tag_index,
                             const char *text_or_name, const char *attr_val,
                             NpXmlAvatarCatCtx *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, tag_index, text_or_name, attr_val);
    if (ret < 0)
        return ret;

    ctx->seen_mask |= 1u;
    if (ev == 3 && depth == 1 && tag_index == 0) {
        ctx->total_count = (u32)sceNet_lib_1FB2FDDD(text_or_name);
        ctx->seen_mask |= 2u;
    }
    if (ev == 2 && depth == 1 && tag_index == 1)
        ctx->seen_mask |= 4u;
    if (depth == 2 && tag_index == 0) {
        if (ctx->parsed_count < ctx->max_count) {
            ret = xml_parse_category(ev, text_or_name, attr_val,
                                     &ctx->entries[ctx->parsed_count],
                                     &ctx->seen_mask, &ctx->parsed_count);
            if (ret < 0)
                return ret;
        }
    }
    if (ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 7u) == 7u) {
            ctx->seen_mask |= 0x40u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_avatar_cat_init(void *parser, NpXmlAvatarCatCtx *ctx)
{
    ctx->seen_mask = 0;
    ctx->parsed_count = 0;
    sceKernelMemset(ctx->entries, 0, sizeof(NpAvatarCategory) * ctx->max_count);
    return sceNp_6999EDC4(parser, &g_xml_avatar_cat_schema, xml_avatar_cat_cb, ctx);
}

int np_xml_avatar_cat_check(NpXmlAvatarCatCtx *ctx)
{
    return ((ctx->seen_mask & 0x40u) != 0) ? 0 : (int)0x80550708;
}

/* --- Avatar list XML callback --- */

static int xml_avatar_list_cb(void *parser, int ev, int depth, int tag_index,
                              const char *text_or_name, const char *attr_val,
                              NpXmlAvatarListCtx *ctx)
{
    int ret;

    (void)parser;

    ret = xml_check_result_attr(ev, depth, tag_index, text_or_name, attr_val);
    if (ret < 0)
        return ret;

    ctx->seen_mask |= 1u;
    if (ev == 3 && depth == 1 && tag_index == 0) {
        ctx->total_count = (u32)sceNet_lib_1FB2FDDD(text_or_name);
        ctx->seen_mask |= 2u;
    }
    if (ev == 2 && depth == 1 && tag_index == 1)
        ctx->seen_mask |= 4u;
    if (depth == 2 && tag_index == 0) {
        if (ctx->parsed_count < ctx->max_count) {
            ret = xml_parse_avatarurl(ev, text_or_name, attr_val,
                                      ctx->urls[ctx->parsed_count],
                                      &ctx->avatar_ids[ctx->parsed_count],
                                      &ctx->seen_mask, &ctx->parsed_count);
            if (ret < 0)
                return ret;
        }
    }
    if (ev == 2 && depth == 0 && tag_index == 0) {
        if ((ctx->seen_mask & 7u) == 7u) {
            ctx->seen_mask |= 0x10u;
            return 0;
        }
        return (int)0x80550708;
    }
    return 0;
}

int np_xml_avatar_list_init(void *parser, NpXmlAvatarListCtx *ctx)
{
    ctx->seen_mask = 0;
    ctx->parsed_count = 0;
    sceKernelMemset(ctx->avatar_ids, 0, sizeof(int) * ctx->max_count);
    sceKernelMemset(ctx->urls, 0, 128u * ctx->max_count);
    return sceNp_6999EDC4(parser, &g_xml_avatar_list_schema, xml_avatar_list_cb, ctx);
}

int np_xml_avatar_list_check(NpXmlAvatarListCtx *ctx)
{
    return ((ctx->seen_mask & 0x10u) != 0) ? 0 : (int)0x80550708;
}

/* =========================================================================
 * Validation, HMAC Signature, HTTP Helpers, JID Conversion, SSL Error Mapping
 * ========================================================================= */

int np_validate_online_id(const char *str)
{
    u32 len;
    u32 i;

    if (str == NULL)
        return 0;
    len = (u32)sceNet_lib_D8722983(str);
    if (len < 3u || len > 16u)
        return 0;
    if ((sceNet_lib_750F705D((unsigned char)str[0]) & 3u) == 0)
        return 0;
    for (i = 1; i < len; i++) {
        char ch = str[i];
        if ((sceNet_lib_750F705D((unsigned char)ch) & 7u) == 0 && ch != '-' && ch != '_')
            return 0;
    }
    return 1;
}

static const u8 g_hmac_key_title_resource[20] = {
    0xAF, 0x15, 0xD8, 0x03, 0x00, 0xDE, 0x1B, 0x7D, 0x4C, 0xA9,
    0xC3, 0x1C, 0x05, 0x26, 0x5F, 0xE3, 0x21, 0x6E, 0x9E, 0xA6,
};

static const u8 g_hmac_key_alt_resource[20] = {
    0x46, 0x75, 0x39, 0x33, 0x63, 0x4D, 0x1A, 0x1D, 0x46, 0x41,
    0x42, 0x75, 0x21, 0x10, 0x11, 0x6F, 0x2C, 0x5E, 0x78, 0x36,
};

int np_verify_hmac_sig(const void *payload, int len, const u8 *sig, int sig_kind)
{
    const u8 *key_src;
    u8 key[20];
    u8 digest[20];
    int padded_len;
    void *work_buf;
    int i;
    int cmp;

    if (sig_kind == 3)
        key_src = g_hmac_key_title_resource;
    else if (sig_kind == 4)
        key_src = g_hmac_key_alt_resource;
    else
        return (int)0x8055070D;

    sceKernelMemset(digest, 0, 20);
    padded_len = len + ((-len) & 0x3F);
    work_buf = np_alloc((SceSize)padded_len);
    if (work_buf == NULL)
        return (int)0x80550703;

    sceKernelMemset(work_buf, 0, (SceSize)padded_len);
    sceKernelMemcpy(work_buf, payload, (SceSize)len);
    for (i = 0; i < 20; i++)
        key[i] = (u8)(key_src[i] ^ 0x7Fu);

    sceNet_lib_384EFE14(work_buf, padded_len, key, 20, digest);
    np_free(work_buf);
    cmp = sceNet_lib_E0A81C7C(sig, digest, 20);
    return (cmp != 0) ? (int)0x8055070B : 0;
}

int np_http_read_exact(NpTransCtx *trans, void *out_buf, u32 len, u32 *read_out)
{
    u8 *dst = (u8 *)out_buf;
    u32 rem = len;
    u32 done = 0;

    while (rem != 0) {
        int got;
        if (trans->aborted == 1)
            return (int)0x80550707;
        got = sceHttpReadData(trans->req_id, dst + done, rem);
        if (got < 0)
            return np_map_ssl_error(trans->req_id, got);
        if (got == 0) {
            *read_out = done;
            return 0;
        }
        done += (u32)got;
        rem -= (u32)got;
    }
    *read_out = len;
    return 0;
}

static int np_http_read_comment_sig(NpTransCtx *trans, u8 *sig_out, u64 *clen)
{
    u8 comment_buf[80];
    char hex[4];
    u32 got = 0;
    int ret;
    int prefix_len;
    int i;
    u32 consumed;

    ret = sceKernelLockLwMutex(trans->lwmutex, 1, NULL);
    if (ret < 0)
        return ret;
    if (trans->aborted == 1)
        ret = (int)0x80550707;
    sceKernelUnlockLwMutex(trans->lwmutex, 1);
    if (ret < 0)
        return ret;

    ret = np_http_read_exact(trans, comment_buf, 65, &got);
    if (ret < 0)
        return ret;
    if (got != 65u)
        return (int)0x80550708;

    prefix_len = sceNet_lib_D8722983("<!--")
               + sceNet_lib_D8722983("Sce-Np-Signature")
               + sceNet_lib_D8722983(": ");
    if (sceNet_lib_B9085A96((const char *)comment_buf, "<!--Sce-Np-Signature: ", prefix_len) != 0 ||
        65 - prefix_len < 40)
        return (int)0x80550708;

    for (i = 0; i < 20; i++) {
        u32 val;
        sceKernelMemcpy(hex, comment_buf + prefix_len + 2 * i, 2);
        hex[2] = '\0';
        val = sceNet_lib_2A73ADDC(hex, NULL, 16);
        if (val >= 0x100u)
            return (int)0x80550708;
        sig_out[i] = (u8)val;
    }

    ret = np_http_read_exact(trans, comment_buf, 1, &got);
    if (ret < 0)
        return ret;
    if (got != 1u)
        return (int)0x80550708;
    consumed = 66;
    if (comment_buf[0] == '\r') {
        ret = np_http_read_exact(trans, comment_buf, 1, &got);
        if (ret < 0)
            return ret;
        if (got != 1u)
            return (int)0x80550708;
        consumed = 67;
    }
    if (comment_buf[0] != '\n')
        return (int)0x80550708;
    *clen -= consumed;
    return 0;
}

int np_http_parse_headers(NpTransCtx *trans, int *status_out, u64 *clen_out,
                          u32 *hdr_info_len_out, u8 *sig_out, int sig_kind)
{
    const char *all_hdr = NULL;
    u32 all_hdr_len = 0;
    const char *val = NULL;
    u32 val_len = 0;
    int ret;

    if (sig_kind != 1 && sig_out == NULL)
        return (int)0x8055070C;

    ret = sceHttpGetStatusCode(trans->req_id, status_out);
    if (ret < 0)
        return ret;
    if (*status_out != 200) {
        if ((u32)(sig_kind - 3) >= 2u)
            return (int)0x8055070A;
        return (int)0x805508A6;
    }

    ret = sceHttpGetContentLength(trans->req_id, clen_out);
    if (ret < 0)
        return ret;
    ret = sceHttpGetAllHeader(trans->req_id, &all_hdr, &all_hdr_len);
    if (ret < 0)
        return ret;

    if (sceParseHttpResponseHeader(all_hdr, all_hdr_len, "Sce-Np-Info-Length", &val, &val_len) >= 0 &&
        hdr_info_len_out != NULL) {
        *hdr_info_len_out = sceNet_lib_2A73ADDC(val, NULL, 10);
    }

    if (sig_kind == 2) {
        if (sceParseHttpResponseHeader(all_hdr, all_hdr_len, "Sce-Np-Signature", &val, &val_len) >= 0 &&
            sig_out != NULL) {
            char hex[4];
            int i;
            if (val_len < 40u)
                return (int)0x80550708;
            for (i = 0; i < 20; i++) {
                u32 octet;
                sceKernelMemcpy(hex, val + 2 * i, 2);
                hex[2] = '\0';
                octet = sceNet_lib_2A73ADDC(hex, NULL, 16);
                if (octet >= 0x100u)
                    return (int)0x80550708;
                sig_out[i] = (u8)octet;
            }
        }
        return 0;
    }

    if ((u32)(sig_kind - 3) < 2u) {
        ret = np_http_read_comment_sig(trans, sig_out, clen_out);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int np_jid_str_to_npid(NpId *npid, const char *jid_str)
{
    u8 jid[16];
    u32 req_len;
    void *work_buf;
    int ret;

    ret = sceNp_5FA879D8(jid, jid_str, NULL, 0, &req_len);
    if (ret < 0)
        return ret;
    if (req_len == 0)
        return (int)0x80550708;
    work_buf = np_alloc(req_len);
    if (work_buf == NULL)
        return (int)0x80550703;
    ret = sceNp_5FA879D8(jid, jid_str, work_buf, req_len, NULL);
    if (ret >= 0) {
        ret = sceNp_C0B3616C(npid, jid);
        if (ret > 0)
            ret = 0;
    }
    np_free(work_buf);
    return ret;
}

int np_npid_to_jid_str(char **out_str, const NpId *npid)
{
    u32 jid[4];
    u32 req_len;
    void *work_buf;
    char *str_buf = NULL;
    int ret;

    ret = sceNp_B2BFADB2(NULL, npid, NULL, 0, &req_len);
    if (ret < 0)
        return ret;
    work_buf = np_alloc(req_len);
    if (work_buf == NULL)
        return (int)0x80550703;

    ret = sceNp_B2BFADB2(jid, npid, work_buf, req_len, NULL);
    if (ret >= 0) {
        jid[2] = 0;
        ret = sceNp_0B8BEE48(jid, NULL, 0, &req_len);
        if (ret >= 0) {
            str_buf = (char *)np_alloc(req_len);
            if (str_buf == NULL) {
                ret = (int)0x80550703;
            } else {
                ret = sceNp_0B8BEE48(jid, str_buf, req_len, NULL);
            }
        }
    }

    np_free(work_buf);
    if (ret < 0) {
        if (str_buf != NULL)
            np_free(str_buf);
        return ret;
    }
    *out_str = str_buf;
    return 0;
}

int np_xml_escape_str(char **out_str, const char *src)
{
    int slen = sceNet_lib_D8722983(src);
    char *escaped;
    u32 req_len;
    int ret;

    if (slen == 0) {
        escaped = (char *)np_alloc(1);
        *escaped = '\0';
        *out_str = escaped;
        return 0;
    }

    ret = sceNp_E41288E7(NULL, 0, src, (u32)slen, &req_len);
    if (ret < 0) {
        *out_str = NULL;
        return ret;
    }
    escaped = (char *)np_alloc(req_len);
    if (escaped == NULL) {
        *out_str = NULL;
        return (int)0x80550703;
    }
    sceKernelMemset(escaped, 0, req_len);
    ret = sceNp_E41288E7(escaped, req_len, src, (u32)slen, NULL);
    if (ret < 0) {
        np_free(escaped);
        *out_str = NULL;
        return ret;
    }
    *out_str = escaped;
    return 0;
}

int np_map_ssl_error(int req_id, int err)
{
    if (err == (int)0x80431075) {
        int ssl_err = 0;
        u32 detail = 0;
        int ret = sceHttpsGetSslError(req_id, &ssl_err, &detail);
        if (ret < 0)
            return ret;
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

s64 np_parse_s64(const char *str)
{
    int neg = 0;
    u32 i = 0;
    u64 val = 0;

    if (str != NULL) {
        if (str[0] == '-') {
            neg = 1;
            i = 1;
        }
        while (str[i] != '\0') {
            if ((sceNet_lib_750F705D((unsigned char)str[i]) & 8u) != 0 || i >= 20u)
                break;
            val = val * 10ULL + (u64)(unsigned char)(str[i] - '0');
            i++;
        }
    }
    return neg ? -(s64)val : (s64)val;
}

int np_trans_close_http(NpTransCtx *trans)
{
    int ret = 0;

    if (trans->req_id > 0) {
        ret = sceKernelLockLwMutex(trans->lwmutex, 1, NULL);
        if (ret < 0)
            return ret;
        ret = sceHttpDeleteRequest(trans->req_id);
        trans->req_id = 0;
        trans->sending = 0;
        sceKernelUnlockLwMutex(trans->lwmutex, 1);
    }
    if (trans->conn_id > 0) {
        ret = sceHttpDeleteConnection(trans->conn_id);
        trans->conn_id = 0;
    }
    return ret;
}

int np_free_fqdn_list(NpFqdnEntry *head)
{
    NpFqdnEntry *cur = head;
    while (cur != NULL) {
        NpFqdnEntry *next = cur->next;
        np_free(cur);
        cur = next;
    }
    return 0;
}
