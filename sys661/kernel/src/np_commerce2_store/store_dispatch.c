#include "store_internal.h"

/* =========================================================================
 * Core manager lifecycle, country capabilities, and HTTP response receive
 * ========================================================================= */

int store_init_mgr(void)
{
    sceKernelMemset(&g_store_mgr, 0, sizeof(g_store_mgr));
    return 0;
}

int store_get_country_caps(u32 ctx_id, const char *country, u32 *out_info)
{
    StoreContext *ctx = NULL;
    char default_cc[16];
    const char * const *entry;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_store_mgr, ctx_id, &ctx, 1);
    if (ret >= 0) {
        sceKernelMemset(out_info, 0, 8);
        if (!country) {
            ret = sceNpCommerce2_D5932A6C(ctx, default_cc, 3, 0);
            if (ret < 0)
                goto done;
            country = default_cc;
        }

        for (entry = &g_country_table[0]; sceNet_lib_D8722983(*entry) != 0; entry++) {
            if (sceNet_lib_A0F16ABD(*entry, country) == 0) {
                out_info[1] = 7;
                sceNet_lib_B5CE388A((char *)out_info, *entry, 3);
                goto done;
            }
        }
        for (entry = &g_country_table[2]; sceNet_lib_D8722983(*entry) != 0; entry++) {
            if (sceNet_lib_A0F16ABD(*entry, country) == 0) {
                out_info[1] = 3;
                sceNet_lib_B5CE388A((char *)out_info, *entry, 3);
                goto done;
            }
        }
        for (entry = &g_country_table[35]; sceNet_lib_D8722983(*entry) != 0; entry++) {
            if (sceNet_lib_A0F16ABD(*entry, country) == 0) {
                out_info[1] = 1;
                sceNet_lib_B5CE388A((char *)out_info, *entry, 3);
                goto done;
            }
        }
        ret = STORE_ERR_COUNTRY_NOT_FOUND;
    }

done:
    if (ctx)
        sceNpCommerce2_77BE411B(&g_store_mgr);
    return ret;
}

int store_recv_response(u32 req_id, u8 *buf, u32 buf_size, u32 *recv_size_out)
{
    StoreContext *ctx = NULL;
    StoreRequest *req = NULL;
    u8 srv_status = 0;
    int truncated = 0;
    u32 err_flag = 0;
    int hdr_len, body_len;
    int ret;

    if (!buf || buf_size == 0 || !recv_size_out)
        return STORE_ERR_INVALID_ARG;

    ret = store_lock_ctx_req(req_id, &ctx, &req);
    if (ret < 0)
        return store_unlock_ctx_req(ctx, req, ret);

    if ((req->cmd_id == STORE_CMD_GET_CAT || req->cmd_id == STORE_CMD_GET_PROD) &&
        ctx->no_size_check != 1 && buf_size < 0x40000U) {
        return store_unlock_ctx_req(ctx, req, STORE_ERR_RESP_BUF_LIMIT);
    }

    ret = sceNpCommerce2_4D43B84D(req->http_req_id, &ctx->http_status);
    if (ret < 0)
        return store_unlock_ctx_req(ctx, req, ret);

    hdr_len = sceNpCommerce2_C27FEAB1(ctx, req->http_req_id, req->cmd_id, &srv_status, buf, buf_size);
    if (hdr_len < 0)
        return store_unlock_ctx_req(ctx, req, hdr_len);

    body_len = sceNpCommerce2_D4C319C6(req->http_req_id, buf, buf + hdr_len, buf_size, &truncated);
    if (body_len < 0)
        return store_unlock_ctx_req(ctx, req, body_len);

    if (truncated == 1) {
        u16 cmd = req->cmd_id;
        if (cmd != STORE_CMD_GET_CAT && cmd != STORE_CMD_GET_DRM &&
            cmd != STORE_CMD_SRCH_PROD && cmd != 53 && cmd != 54 && cmd != 55 &&
            cmd != STORE_CMD_RECOM_PRODS) {
            return store_unlock_ctx_req(ctx, req, STORE_ERR_RESP_TOO_SMALL);
        }
    }

    ret = sceNpCommerce2_ADAA054C(buf, buf_size, (u32)body_len);
    if (ret < 0)
        return store_unlock_ctx_req(ctx, req, ret);

    *recv_size_out = (u32)(hdr_len + body_len);
    if (srv_status == 0) {
        store_post_process_response(ctx, req, buf, buf_size, *recv_size_out);
        return store_unlock_ctx_req(ctx, req, 0);
    }

    ret = (int)(STORE_ERR_SERVER_BASE | (u32)srv_status);
    if (req->cmd_id == STORE_CMD_SET_RES_ADDR && ret == STORE_ERR_SERVER_GENERIC)
        return store_unlock_ctx_req(ctx, req, STORE_ERR_SERVER_ADDR_INVALID);
    if ((req->cmd_id == STORE_CMD_REG_CC || req->cmd_id == 64) && ret == STORE_ERR_SERVER_GENERIC)
        return store_unlock_ctx_req(ctx, req, STORE_ERR_SERVER_CARD_INVALID);

    store_tlv_parse_err_flag(buf, *recv_size_out, &err_flag);
    if (err_flag == 1)
        ret = STORE_ERR_MAINTENANCE;

    return store_unlock_ctx_req(ctx, req, ret);
}

/* =========================================================================
 * Low-level TLV scanning & cursor helpers
 * ========================================================================= */

static void store_tlv_reset_hdr(u32 *out, u32 total_struct_size, u32 ver, const u8 *base,
                                const u8 *data, u32 data_size, const u8 *body)
{
    sceKernelMemset(out, 0, total_struct_size);
    out[0] = ver;
    out[1] = (u32)(size_t)base;
    out[3] = (u32)(size_t)data;
    out[4] = data_size;
    out[5] = (u32)(size_t)body;
}

static int store_tlv_read_str(const u8 *scan, u32 scan_len, u16 tag, const u8 *base, u32 *out_ptr)
{
    u16 tlen = 0;
    int off;

    *out_ptr = (u32)(size_t)(base + 4);
    off = sceNpCommerce2_E3455E27(scan, scan_len, tag, 0, &tlen);
    if (off >= 0 && tlen != 0)
        *out_ptr = (u32)(size_t)(scan + off);
    return off;
}

static void store_tlv_store_rtc(const u8 *ptr, u32 *out_pair)
{
    u64 rtc = store_unix_ms_to_rtc(store_read_be64(ptr));
    if (rtc != 0) {
        out_pair[0] = (u32)rtc;
        out_pair[1] = (u32)(rtc >> 32);
    } else {
        out_pair[0] = 0;
        out_pair[1] = 0;
    }
}

int store_tlv_parse_root(const u8 *buf, u32 buf_size, u32 *ver_out, u32 *body_size_out, u32 *extra_out)
{
    u32 hdr[14];
    u8 magic[4];

    if (buf_size < 60)
        return (int)0x80550F14;

    sceKernelMemcpy(hdr, buf, 56);
    sceKernelMemcpy(magic, buf + 56, 4);
    if (buf_size < hdr[1] + 56)
        return (int)0x80550F14;

    *body_size_out = hdr[1] - 4;
    if (ver_out)
        *ver_out = hdr[0];
    if (extra_out)
        *extra_out = hdr[2];
    return 60;
}

int store_tlv_find_nth(const u8 *buf, u32 buf_size, u16 tag, u16 min_len, u32 index, u16 *len_out)
{
    u16 span_len = 0;
    u32 count = 0;
    int base_off = sceNpCommerce2_3F6D413C(buf, buf_size, tag, (u32)(size_t)&span_len, &count);
    const u8 *start, *cur;
    u32 idx;
    u16 item_len = 0;

    if (base_off < 0)
        return base_off;
    if (index >= count)
        return (int)0x80550F06;

    start = buf + base_off;
    cur = start;
    for (idx = 0; idx <= index; idx++) {
        int rel;
        if (idx != 0)
            cur += item_len;
        rel = sceNpCommerce2_E3455E27(cur, span_len - (u32)(cur - start), tag, min_len, &item_len);
        if (rel < 0)
            return rel;
        cur += rel;
    }
    *len_out = item_len;
    return (int)(cur - buf);
}

int store_tlv_find_nth_ex(const u8 *buf, u32 buf_size, u16 tag, u16 min_len,
                          u32 index, u16 *len_out, int extra_mode)
{
    u32 count = 0;
    int base_off = sceNpCommerce2_3546C737(buf, buf_size, tag, 0, &count);
    const u8 *cur;
    u32 idx;
    u16 item_len = 0;

    (void)extra_mode;
    if (base_off < 0)
        return base_off;
    if (index >= count)
        return (int)0x80550F06;

    cur = buf + base_off;
    for (idx = 0; idx <= index; idx++) {
        int rel;
        if (idx != 0)
            cur += item_len;
        rel = sceNpCommerce2_E3455E27(cur, buf_size - (u32)(cur - buf), tag, min_len, &item_len);
        if (rel < 0)
            return rel;
        cur += rel;
    }
    *len_out = item_len;
    return (int)(cur - buf);
}

int store_tlv_find_nth_paged(const u8 *buf, u32 buf_size, u16 tag, u16 min_len,
                             u32 index, u16 *len_out, u32 extra)
{
    u32 range[4];
    int base_off = sceNpCommerce2_69FEA1C8(buf, buf_size, tag, 0, range, extra);
    const u8 *cur;
    u32 idx;
    u16 item_len = 0;

    if (base_off < 0)
        return base_off;
    if (index >= range[1])
        return (int)0x80550F06;

    cur = buf + base_off;
    for (idx = 0; idx <= index; idx++) {
        int rel;
        if (idx != 0)
            cur += item_len;
        rel = sceNpCommerce2_E3455E27(cur, buf_size - (u32)(cur - buf), tag, min_len, &item_len);
        if (rel < 0)
            return rel;
        cur += rel;
    }
    *len_out = item_len;
    return (int)(cur - buf);
}

int store_tlv_clamp_paged(const u32 *cursor, u16 tag, u16 min_len, u32 *range_out)
{
    u32 idx;
    u16 item_len = 0;
    int ret = 0;

    for (idx = 0; idx < range_out[1]; idx++) {
        ret = store_tlv_find_nth_paged((const u8 *)(size_t)cursor[3], cursor[4],
                                       tag, min_len, idx, &item_len, cursor[6]);
        if (ret < 0) {
            if (idx != 0) {
                range_out[1] = idx;
                return 0;
            }
            return ret;
        }
    }
    return ret;
}

/* =========================================================================
 * Store response root & sub-item TLV parsers
 * ========================================================================= */

int store_tlv_parse_simple_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 40, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    return 0;
}

int store_tlv_parse_counted_root(u32 *out, const u8 *buf, u32 buf_size, u32 tag)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    return store_min_s32(sceNpCommerce2_3F6D413C(buf + off, body_size, tag, 0, &out[10]), 0);
}

int store_tlv_parse_paged_root(u32 *out, const u8 *buf, u32 buf_size, u16 tag, u16 min_len)
{
    u32 ver = 0, body_size = 0, extra = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, &extra);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 52, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    out[6] = extra;
    ret = sceNpCommerce2_69FEA1C8(buf + off, body_size, tag, 0, &out[10], extra);
    if (ret < 0)
        return ret;
    return store_min_s32(store_tlv_clamp_paged(out, tag, min_len, &out[10]), 0);
}

int store_tlv_parse_cart_root(u32 *out, const u8 *buf, u32 buf_size, int optional_tags)
{
    u32 ver = 0, body_size = 0, extra = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, optional_tags ? NULL : &extra);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 48, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    if (!optional_tags)
        out[6] = extra;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 45, 4, NULL);
    if (ret >= 0)
        out[10] = store_read_be32(buf + off + ret);
    else if (!optional_tags || ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 21, 4, NULL);
    if (ret >= 0)
        out[11] = store_read_be32(buf + off + ret);
    else if (ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;
    return 0;
}

int store_tlv_parse_purchase_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 56, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 21, 4, NULL);
    if (ret >= 0)
        out[10] = store_read_be32(buf + off + ret);
    else if (ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 3, 8, NULL);
    if (ret >= 0) {
        u64 val = store_read_be64(buf + off + ret);
        out[12] = (u32)val;
        out[13] = (u32)(val >> 32);
    } else if (ret != STORE_ERR_TAG_NOT_FOUND) {
        return ret;
    }
    return 0;
}

int store_tlv_parse_topup_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 56, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 21, 4, NULL);
    if (ret < 0)
        return ret;
    out[10] = store_read_be32(buf + off + ret);

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 45, 4, NULL);
    if (ret < 0)
        return ret;
    out[11] = store_read_be32(buf + off + ret);

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 3, 8, NULL);
    if (ret >= 0) {
        u64 val = store_read_be64(buf + off + ret);
        out[12] = (u32)val;
        out[13] = (u32)(val >> 32);
    }
    return store_min_s32(ret, 0);
}

int store_tlv_parse_voucher_root(u32 *out, const u8 *buf, u32 buf_size, int with_tx_id)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, with_tx_id ? 56 : 44, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 49, 4, NULL);
    if (ret >= 0)
        out[10] = store_read_be32(buf + off + ret);
    if (!with_tx_id || ret < 0)
        return store_min_s32(ret, 0);

    ret = sceNpCommerce2_E3455E27(buf + off, body_size, 3, 8, NULL);
    if (ret >= 0) {
        u64 val = store_read_be64(buf + off + ret);
        out[12] = (u32)val;
        out[13] = (u32)(val >> 32);
    }
    return store_min_s32(ret, 0);
}

int store_tlv_parse_addr_lookup_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 52, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = store_tlv_read_str(buf + off, body_size, 79, buf, &out[10]);
    if (ret >= 0) ret = store_tlv_read_str(buf + off, body_size, 82, buf, &out[11]);
    if (ret >= 0) ret = store_tlv_read_str(buf + off, body_size, 54, buf, &out[12]);
    return store_min_s32(ret, 0);
}

int store_tlv_parse_epw_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    ret = store_tlv_read_str(buf + off, body_size, 118, buf, &out[10]);
    return store_min_s32(ret, 0);
}

int store_tlv_parse_prod_list_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0, extra = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, &extra);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    out[6] = extra;
    return store_min_s32(sceNpCommerce2_3546C737(buf + off, body_size, 40, 0, &out[10]), 0);
}

int store_tlv_parse_recom_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0, extra = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, &extra);
    int ret;
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 56, ver, buf, buf + off, body_size, NULL);
    out[2] = (u32)off + body_size;
    out[6] = extra;

    ret = store_tlv_read_str(buf + off, body_size, 148, buf, &out[13]);
    if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;
    ret = sceNpCommerce2_69FEA1C8(buf + off, body_size, 40, 0, &out[10], extra);
    if (ret < 0)
        return ret;
    return store_min_s32(store_tlv_clamp_paged(out, 40, 4, &out[10]), 0);
}

int store_tlv_parse_tag_ptr_item(const u32 *parent, u32 index, u16 tag, int from_body, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)(size_t)(from_body ? parent[5] : parent[3]);
    u32 scan_len = from_body ? (parent[4] - (u32)(scan - (const u8 *)(size_t)parent[3])) : parent[4];
    u16 tlen = 0;
    int off = store_tlv_find_nth(scan, scan_len, tag, 0, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(size_t)(scan + off);
    return off;
}

int store_tlv_parse_search_item(const u32 *parent, u32 index, int use_paged, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = use_paged
        ? store_tlv_find_nth_paged(data, parent[4], 40, 4, index, &tlen, parent[6])
        : store_tlv_find_nth_ex(data, parent[4], 40, 4, index, &tlen, (int)parent[6]);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, parent[0], (const u8 *)(size_t)parent[1],
                        data + off, tlen, data + off + 4);
    out[10] = store_read_be32(data + off);
    return off;
}

int store_tlv_parse_cart_item(const u32 *parent, int indexed, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = indexed
        ? store_tlv_find_nth(body, rem, 78, 105, index, &tlen)
        : sceNpCommerce2_E3455E27(body, rem, 78, 105, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, parent[0], (const u8 *)(size_t)parent[1],
                        body + off, tlen, body + off + 4);
    out[10] = store_read_be32(body + off);
    return off;
}

int store_tlv_parse_wallet_info(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 44, 12, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 56, parent[0], (const u8 *)(size_t)parent[1],
                        data + off, tlen, data + off + 12);
    out[10] = store_read_be32(data + off);
    store_tlv_store_rtc(data + off + 4, &out[12]);
    return off;
}

int store_tlv_parse_cart_summary(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 47, 16, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 56, parent[0], (const u8 *)(size_t)parent[1],
                        data + off, tlen, data + off + 12);
    out[10] = store_read_be32(data + off);
    out[11] = store_read_be32(data + off + 4);
    out[12] = store_read_be32(data + off + 8);
    return sceNpCommerce2_3546C737(data + off + 12, tlen - 12, 46, 0, &out[13]);
}

int store_tlv_parse_tax_info(const u32 *parent, const u8 *scan, u32 scan_len, u32 *out)
{
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(scan, scan_len, 88, 8, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1],
                        scan + off, tlen, scan + off + 8);
    out[10] = store_read_be32(scan + off);
    out[11] = store_read_be32(scan + off + 4);
    return off;
}

int store_tlv_parse_voucher_summary(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 50, 8, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1],
                        data + off, tlen, data + off + 8);
    out[10] = store_read_be32(data + off);
    out[11] = store_read_be32(data + off + 4);
    return off;
}

int store_tlv_parse_voucher_items(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 51, 4, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 44, parent[0], (const u8 *)(size_t)parent[1],
                        data + off, tlen, data + off);
    return sceNpCommerce2_3F6D413C(data + off, tlen, 122, 0, &out[10]);
}

int store_tlv_parse_province_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = store_tlv_find_nth(data, parent[4], 55, 73, index, &tlen);
    const u8 *item, *sub;
    int ret;
    if (off < 0)
        return off;
    item = data + off;
    sub = item + 65;
    store_tlv_reset_hdr(out, 52, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = (u32)(size_t)item;
    ret = store_tlv_read_str(sub, tlen - 65, 54, (const u8 *)(size_t)parent[1], &out[11]);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_3F6D413C(sub, tlen - 65, 59, 0, &out[12]);
}

int store_tlv_parse_city_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 59, 69, index, &tlen);
    const u8 *item, *sub;
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 65;
    store_tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = (u32)(size_t)item;
    return store_tlv_read_str(sub, tlen - 65, 58, (const u8 *)(size_t)parent[1], &out[11]);
}

int store_tlv_parse_pay_method(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = store_tlv_find_nth(data, parent[4], 57, 25, index, &tlen);
    const u8 *item, *sub;
    int ret;
    if (off < 0)
        return off;
    item = data + off;
    sub = item + 17;
    store_tlv_reset_hdr(out, 52, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = (u32)(size_t)item;
    ret = store_tlv_read_str(sub, tlen - 17, 15, (const u8 *)(size_t)parent[1], &out[11]);
    if (ret >= 0)
        ret = store_tlv_read_str(sub, tlen - 17, 16, (const u8 *)(size_t)parent[1], &out[12]);
    return ret;
}

int store_tlv_parse_currency(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = store_tlv_find_nth(data, parent[4], 22, 42, index, &tlen);
    const u8 *item, *sub;
    int ret;
    if (off < 0)
        return off;
    item = data + off;
    sub = item + 30;
    store_tlv_reset_hdr(out, 84, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = (u32)(size_t)(item + 4);
    *(u16 *)&out[12] = store_bswap16((u16)((item[8] << 8) | item[9]));
    out[13] = store_read_be32(item + 10);
    out[14] = store_read_be32(item + 14);
    out[16] = store_read_be32(item + 18);
    out[17] = store_read_be32(item + 22);
    out[18] = store_read_be32(item + 26);

    ret = sceNpCommerce2_3F6D413C(sub, tlen - 30, 21, 0, &out[15]);
    if (ret < 0)
        return ret;
    ret = store_tlv_read_str(sub, tlen - 30, 125, (const u8 *)(size_t)parent[1], &out[19]);
    if (ret >= 0)
        ret = store_tlv_read_str(sub, tlen - 30, 126, (const u8 *)(size_t)parent[1], &out[20]);
    return ret;
}

int store_tlv_parse_rating_item(const u32 *parent, int indexed, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = indexed
        ? store_tlv_find_nth(body, rem, 155, 71, index, &tlen)
        : sceNpCommerce2_E3455E27(body, rem, 155, 71, &tlen);
    if (off < 0)
        return off;
    store_tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1],
                        body + off, tlen, body + off + 71);
    out[10] = (u32)(size_t)(body + off);
    out[11] = (u32)(size_t)(body + off + 6);
    return 0;
}

int store_tlv_parse_discount_info(const u32 *cursor, u32 *has_disc, u32 *disc_type,
                                  u32 *disc_val, u32 *disc_price)
{
    const u8 *body = (const u8 *)(size_t)cursor[5];
    u32 rem = cursor[4] - (u32)(body - (const u8 *)(size_t)cursor[3]);
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(body, rem, 74, 12, &tlen);
    if (off == STORE_ERR_TAG_NOT_FOUND) {
        *has_disc = 0;
        return 0;
    }
    if (off < 0)
        return off;
    *disc_type  = store_read_be32(body + off);
    *disc_val   = store_read_be32(body + off + 4);
    *disc_price = store_read_be32(body + off + 8);
    *has_disc   = 1;
    return 0;
}

int store_tlv_parse_reward_info(u32 ver, const u8 *base, const u8 *data, u32 data_size,
                                const u8 *body, u32 *out, int is_alt)
{
    u32 rem = data_size - (u32)(body - data);
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(body, rem, is_alt ? 145 : 75, 8, &tlen);
    const u8 *item, *sub;
    int ret;
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 8;
    store_tlv_reset_hdr(out, is_alt ? 68 : 76, ver, base, item, tlen, sub);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);

    if (is_alt) {
        ret = store_tlv_read_str(sub, tlen - 8, 146, base, &out[12]);
        if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
            return ret;
        return store_min_s32(store_tlv_parse_discount_info(out, &out[13], &out[14], &out[15], &out[16]), 0);
    }

    ret = store_tlv_read_str(sub, tlen - 8, 90, base, &out[12]);
    if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;
    ret = store_tlv_read_str(sub, tlen - 8, 70, base, &out[13]);
    if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;
    ret = store_tlv_read_str(sub, tlen - 8, 71, base, &out[14]);
    if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;
    return store_min_s32(store_tlv_parse_discount_info(out, &out[15], &out[16], &out[17], &out[18]), 0);
}

int store_tlv_parse_content_hdr(const u8 *item, u32 *id_out, u32 *type_out, u32 *date_out,
                                u32 *count1_out, u32 *count2_out, u32 *has_flag_out,
                                u32 *raw_flags_out, u32 *sp_flags_out)
{
    u32 raw_flags, sp_bits;

    *id_out = (u32)(size_t)item;
    *type_out = store_read_be32(item + 49);
    store_tlv_store_rtc(item + 53, date_out);
    *count1_out = store_read_be32(item + 61);
    *count2_out = store_read_be32(item + 65);
    raw_flags = store_read_be32(item + 69);
    *raw_flags_out = raw_flags;
    *has_flag_out = (raw_flags & 0xAU) != 0;
    sp_bits = store_read_be32(item + 73);
    if (sp_bits & 0x80000000U) *sp_flags_out |= 1U;
    if (sp_bits & 0x40000000U) *sp_flags_out |= 2U;
    if (sp_bits & 0x20000000U) *sp_flags_out |= 4U;
    return 77;
}

int store_tlv_parse_content_item(const u8 *item, u32 item_len, const u8 *base, u32 ver,
                                 u32 kind, u32 *out)
{
    int is_new_sdk = ((u32)sceKernelGetCompiledSdkVersion() > 0x0602FFFFU);
    const u8 *sub = item + 77;
    u32 sub_len = (u16)(item_len - 77);
    u32 sp_bits;
    int ret;

    store_tlv_reset_hdr(out, is_new_sdk ? 128 : 88, ver, base, item, item_len, sub);
    out[10] = kind;
    out[11] = (u32)(size_t)item;
    out[12] = store_read_be32(item + 49);
    store_tlv_store_rtc(item + 53, &out[14]);
    sp_bits = store_read_be32(item + 73);
    if (is_new_sdk) {
        if (sp_bits & 0x80000000U) out[22] |= 1U;
        if (sp_bits & 0x40000000U) out[22] |= 2U;
        if (sp_bits & 0x20000000U) out[22] |= 4U;
    }

    ret = store_tlv_read_str(sub, sub_len, 7, base, &out[16]);
    if (ret >= 0) ret = store_tlv_read_str(sub, sub_len, 12, base, &out[17]);
    if (ret >= 0) ret = store_tlv_read_str(sub, sub_len, 16, base, &out[18]);
    if (ret >= 0) ret = store_tlv_read_str(sub, sub_len, 4, base, &out[19]);
    if (ret >= 0 && kind == 1) {
        ret = store_tlv_read_str(sub, sub_len, 13, base, &out[20]);
        if (ret >= 0) {
            ret = store_tlv_read_str(sub, sub_len, 119, base, &out[21]);
            if (ret == STORE_ERR_TAG_NOT_FOUND)
                ret = 0;
        }
    }
    return ret;
}

int store_tlv_parse_rating_system(const u8 *item, u32 item_len, const u8 *base, u32 ver, u32 *out)
{
    const u8 *sub = item + 17;
    int ret;

    store_tlv_reset_hdr(out, 52, ver, base, item, item_len, sub);
    out[10] = (u32)(size_t)item;
    ret = store_tlv_read_str(sub, item_len - 17, 16, base, &out[11]);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_3F6D413C(sub, item_len - 17, 23, 0, &out[12]);
}

int store_tlv_parse_sku_fields(const u8 *item, u32 **desc, int skip_extra)
{
    const u8 *cur = item + 61;
    u32 val, extra_flag = 0;

    *(desc[3]) = (u32)(size_t)item;
    val = store_read_be32(item + 57);
    *(desc[4]) = (val == 2) ? 2 : 1;
    *(desc[5]) = store_read_be32(cur); cur += 4;
    *(desc[6]) = store_read_be32(cur); cur += 4;
    if (desc[7]) *(desc[7]) = store_read_be32(cur);
    cur += 4;
    if (desc[8]) *(desc[8]) = store_read_be32(cur);
    cur += 4;
    *(desc[9]) = (store_read_be32(cur) != 0); cur += 4;
    if (((u32)(size_t)desc[1]) & 4U)
        extra_flag = store_read_be32(cur);
    cur += 4;
    if (desc[10]) *(desc[10]) = store_read_be32(cur);
    cur += 4;
    if (desc[11]) *(desc[11]) = store_read_be32(cur);
    cur += 4;
    if (desc[12]) *(desc[12]) = store_read_be32(cur);
    cur += 4;
    if (!skip_extra && desc[13]) *(desc[13]) = store_read_be32(cur);
    cur += 4;
    if (!skip_extra && desc[14]) *(desc[14]) = store_read_be32(cur);
    cur += 4;

    if (!skip_extra) {
        if (desc[7] && *(desc[7]) != 0) *(desc[2]) |= 1U;
        if (desc[9] && *(desc[9]) != 0) *(desc[2]) |= 2U;
        if (extra_flag != 0)            *(desc[2]) |= 4U;
    }
    return (int)(cur - item);
}

int store_tlv_parse_sku_full(const u8 *item, u32 **desc)
{
    int off = store_tlv_parse_sku_fields(item, desc, 0);
    const u8 *cur;
    u64 size_val;

    if (off < 0)
        return off;
    cur = item + off;
    store_tlv_store_rtc(cur, desc[15]); cur += 8;
    store_tlv_store_rtc(cur, desc[16]); cur += 8;
    store_tlv_store_rtc(cur, desc[17]); cur += 8;
    *(desc[18]) = store_read_be32(cur); cur += 4;
    size_val = store_read_be64(cur); cur += 8;
    desc[19][0] = (u32)size_val;
    desc[19][1] = (u32)(size_val >> 32);
    return (int)(cur - item);
}
