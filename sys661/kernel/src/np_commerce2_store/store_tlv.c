#include "store_internal.h"

/* Forward declarations for low-level TLV helpers in exact Sony order */
static int store_tlv_parse_root(const u8 *resp, u32 resp_size, u32 *ver_out,
                                u32 *body_len_out, u32 *extra_out) __attribute__((noinline));
static int store_tlv_find_nth(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                              u32 index, u16 *len_out) __attribute__((noinline));
static int store_tlv_find_nth_ex(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                                 u32 index, u16 *len_out, u32 extra) __attribute__((noinline));
static int store_tlv_find_nth_paged(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                                    u32 index, u16 *len_out, u32 extra) __attribute__((noinline));
static int store_tlv_clamp_paged(const u32 *cursor, u16 tag, u16 min_len,
                                 u32 *range_out) __attribute__((noinline));
static int store_tlv_parse_currency(const u32 *parent, u32 index,
                                    u32 *out) __attribute__((noinline));
static int store_tlv_parse_content_hdr(const u8 *base, const u8 *item, u32 *id_out,
                                       u32 *type_out, u32 *date_out, u32 *cat_cnt_out,
                                       u32 *prod_cnt_out, u32 *has_flag_out, u32 *raw_flags_out,
                                       u32 *sp_flags_out) __attribute__((noinline));
static int store_tlv_parse_content_item(const u8 *item, u32 item_len, const u8 *base, u32 ver,
                                        u32 kind, u32 *out) __attribute__((noinline));
static int store_tlv_parse_rating_system(const u8 *item, u32 item_len, const u8 *base, u32 ver,
                                         u32 *out) __attribute__((noinline));
static int store_tlv_parse_sku_fields(const u8 *item, u32 **desc,
                                      int skip_extra) __attribute__((noinline));
static int store_tlv_parse_sku_full(const u8 *item, u32 **desc) __attribute__((noinline));
static int store_tlv_parse_reward_std(u32 ver, const u8 *base, const u8 *cur, u32 cur_len,
                                      const u8 *body, u32 *out) __attribute__((noinline));
static int store_tlv_parse_reward_ext(u32 ver, const u8 *base, const u8 *cur, u32 cur_len,
                                      const u8 *body, u32 *out) __attribute__((noinline));
static int store_tlv_parse_wallet_info(const u32 *parent, u32 *out) __attribute__((noinline));
static int store_tlv_parse_cart_summary(const u32 *parent, u32 *out) __attribute__((noinline));
static int store_tlv_parse_tax_info(const u32 *parent, const u8 *scan, u32 scan_len,
                                    u32 *out) __attribute__((noinline));
static int store_tlv_parse_province_info(const u32 *parent, u32 index,
                                         u32 *out) __attribute__((noinline));
static int store_tlv_parse_pay_method(const u32 *parent, u32 index,
                                      u32 *out) __attribute__((noinline));
static int store_tlv_parse_voucher_summary(const u32 *parent, u32 *out) __attribute__((noinline));
static int store_tlv_parse_discount_info(const u32 *cursor, u32 *has_disc, u32 *disc_type,
                                         u32 *disc_val, u32 *disc_price) __attribute__((noinline));

/* Inline TLV primitives matching Sony's exact memcpy/memset idioms */
static inline __attribute__((always_inline)) u32
store_tlv_be32(const u8 *src)
{
    u32 raw;
    sceKernelMemcpy(&raw, src, 4U);
    return store_bswap32(raw);
}

static inline __attribute__((always_inline)) u16
store_tlv_be16(const u8 *src)
{
    u16 raw;
    sceKernelMemcpy(&raw, src, 2U);
    return store_bswap16(raw);
}

static inline __attribute__((always_inline)) void
store_tlv_be64_words(const u8 *src, u32 *dst_pair)
{
    u32 raw_pair[2];
    sceKernelMemcpy(raw_pair, src, 8U);
    dst_pair[0] = store_bswap32(raw_pair[1]);
    dst_pair[1] = store_bswap32(raw_pair[0]);
}

static inline __attribute__((always_inline)) void
store_tlv_store_rtc(const u8 *src, u32 *dst_pair)
{
    u32 raw_pair[2];
    u64 ms;
    sceKernelMemcpy(raw_pair, src, 8U);
    ms = ((u64)store_bswap32(raw_pair[0]) << 32) | (u64)store_bswap32(raw_pair[1]);
    if (ms != 0ULL) {
        u64 rtc = ms * 1000ULL + STORE_UNIX_EPOCH_US;
        dst_pair[0] = (u32)(rtc & 0xFFFFFFFFULL);
        dst_pair[1] = (u32)(rtc >> 32);
    } else {
        sceKernelMemset(dst_pair, 0, 8U);
    }
}

static inline __attribute__((always_inline)) int
store_tlv_read_str(u32 *out, u32 field_idx, const u8 *scan, u32 scan_len, u32 tag, int optional)
{
    u16 plen;
    int off;

    out[field_idx] = out[1] + 4U;
    off = sceNpCommerce2_E3455E27(scan, scan_len, tag, 0U, &plen);
    if (off >= 0 && plen != 0U)
        out[field_idx] = (u32)(scan + off);
    if (optional) {
        if (((off != STORE_ERR_TAG_NOT_FOUND) & ((u32)off >> 31)) == 0)
            return 0;
        return off;
    }
    return off;
}

static inline __attribute__((always_inline)) int
store_tlv_opt_count(const u8 *scan, u32 scan_len, u32 tag, u32 *count_out)
{
    int ret = sceNpCommerce2_3F6D413C(scan, scan_len, tag, NULL, count_out);
    if (((ret != STORE_ERR_TAG_NOT_FOUND) & ((u32)ret >> 31)) == 0)
        return 0;
    return ret;
}

static inline __attribute__((always_inline)) int
store_tlv_init_counted_root(u32 *out, const u8 *resp, u32 resp_size, u32 tag)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        sceKernelMemset(out, 0, 44U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        return store_min_s32(sceNpCommerce2_3F6D413C(body, body_len, tag, NULL, &out[10]), 0);
    }
    return off;
}

static inline __attribute__((always_inline)) int
store_tlv_init_simple_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        sceKernelMemset(out, 0, 40U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        return 0;
    }
    return off;
}

static inline __attribute__((always_inline)) int
store_tlv_init_cart_root_std(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    u32 extra;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, &extra);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 48U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[6] = extra;

        ret = sceNpCommerce2_E3455E27(body, body_len, 45U, 4U, NULL);
        if (ret < 0)
            return ret;
        {
            u32 raw;
            sceKernelMemcpy(&raw, body + ret, 4U);
            raw = store_bswap32(raw);
            sceKernelMemcpy(&out[10], &raw, 4U);
        }
        ret = sceNpCommerce2_E3455E27(body, body_len, 21U, 4U, NULL);
        if (ret >= 0) {
            out[11] = store_tlv_be32(body + ret);
            return 0;
        }
        if (((ret != STORE_ERR_TAG_NOT_FOUND) & ((u32)ret >> 31)) == 0)
            return 0;
        return ret;
    }
    return off;
}

/* =========================================================================
 * Main TLV parsers (0x00AD64 .. 0x010378)
 * ========================================================================= */

int store_tlv_parse_category_info(const u32 *parent, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 tag = (api_ver == 3) ? 139U : 38U;
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 104U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  tag, 101U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *base = (const u8 *)parent[1];
        const u8 *item = scan + off;
        int hdr_len;
        const u8 *body;
        u32 rem;
        int ret;

        sceKernelMemset(out, 0, 40U);
        out[0] = parent[0];
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        hdr_len = store_tlv_parse_content_hdr(base, item, &out[10], &out[11], &out[12],
                                              &out[14], &out[15], &out[16], &out[17], &out[18]);
        if (hdr_len < 0)
            return hdr_len;

        body = item + hdr_len;
        rem = (u32)(u16)((u32)tlen - (u32)hdr_len);
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 19U, body, rem, 7U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 20U, body, rem, 12U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 21U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 22U, body, rem, 4U, 0);
        if (ret < 0)
            return ret;

        rem = (u32)tlen - (u32)hdr_len;
        if (api_ver == 3) {
            ret = store_tlv_opt_count(body, rem, 17U, &out[25]);
            if (ret < 0)
                return ret;
        } else {
            ret = store_tlv_read_str(out, 25U, body, rem, 17U, 0);
            if (ret < 0)
                return ret;
        }

        ret = sceNpCommerce2_3F6D413C(body, rem, 24U, NULL, &out[23]);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 24U, body, rem, 68U, 1);
    }
}

int store_tlv_parse_cat_sku(const u32 *parent, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 tag = (api_ver == 3) ? 135U : 30U;
    u32 *desc[20];
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 144U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  tag, 97U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *base = (const u8 *)parent[1];
        const u8 *item = scan + off;
        int sku_len;
        const u8 *body;
        u32 rem;
        int ret;

        sceKernelMemset(out, 0, 40U);
        out[0] = parent[0];
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        sceKernelMemset(desc, 0, sizeof(desc));
        sceKernelMemset(desc, 0, 60U);
        desc[0]  = (u32 *)base;
        desc[1]  = (u32 *)7U;
        desc[2]  = &out[35];
        desc[3]  = &out[10];
        desc[4]  = &out[11];
        desc[5]  = &out[12];
        desc[6]  = &out[13];
        desc[7]  = &out[14];
        desc[8]  = &out[15];
        desc[9]  = &out[16];
        desc[10] = &out[17];
        desc[11] = &out[18];
        desc[12] = &out[19];
        desc[13] = &out[20];
        desc[14] = &out[21];
        desc[15] = &out[22];
        desc[16] = &out[24];
        desc[17] = &out[26];
        desc[18] = &out[28];
        desc[19] = &out[30];

        sku_len = store_tlv_parse_sku_full(item, desc);
        if (sku_len < 0)
            return sku_len;

        body = item + sku_len;
        rem = (u32)tlen - (u32)sku_len;
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 32U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_opt_count(body, rem, 128U, &out[33]);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem, 149U, &out[34]);
    }
}

int store_tlv_parse_product_info(const u32 *parent, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 tag = (api_ver == 3) ? 140U : 39U;
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 136U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  tag, 137U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *base = (const u8 *)parent[1];
        const u8 *item = scan + off;
        int hdr_len;
        const u8 *body;
        u32 rem;
        int ret;

        sceKernelMemset(out, 0, 40U);
        out[0] = parent[0];
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        hdr_len = store_tlv_parse_content_hdr(base, item, &out[10], &out[12], &out[14],
                                              &out[16], &out[11], &out[17], &out[18], &out[19]);
        if (hdr_len < 0)
            return hdr_len;

        body = item + hdr_len;
        rem = (u32)(u16)((u32)tlen - (u32)hdr_len);
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 20U, body, rem, 7U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 21U, body, rem, 12U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 22U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 23U, body, rem, 4U, 0);
        if (ret < 0)
            return ret;

        rem = (u32)tlen - (u32)hdr_len;
        if (api_ver == 3) {
            ret = store_tlv_opt_count(body, rem, 17U, &out[32]);
            if (ret < 0)
                return ret;
        } else {
            ret = store_tlv_read_str(out, 32U, body, rem, 17U, 0);
            if (ret < 0)
                return ret;
        }

        ret = sceNpCommerce2_3F6D413C(body, rem, 24U, NULL, &out[24]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 25U, body, rem, 13U, 0);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 35U, NULL, &out[26]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 36U, NULL, &out[27]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 37U, NULL, &out[28]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 29U, body, rem, 68U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 30U, body, rem, 69U, 1);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 31U, body, rem, 119U, 1);
    }
}

int store_tlv_parse_product_sku(const u32 *parent, u32 index, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tag = (api_ver == 3) ? 137U : 31U;
    u32 *desc[20];
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 168U);
    off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                             tag, 101U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *base = (const u8 *)parent[1];
        const u8 *item = scan + off;
        int sku_len;
        const u8 *body;
        u32 rem;
        u32 rem_full;
        u32 sku_kind;
        int ret;

        sceKernelMemset(out, 0, 40U);
        out[0] = parent[0];
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        sceKernelMemset(desc, 0, sizeof(desc));
        sceKernelMemset(desc, 0, 60U);
        desc[0]  = (u32 *)base;
        desc[1]  = (u32 *)7U;
        desc[2]  = &out[41];
        desc[3]  = &out[10];
        desc[4]  = &out[11];
        desc[5]  = &out[12];
        desc[6]  = &out[13];
        desc[7]  = &out[14];
        desc[8]  = &out[15];
        desc[9]  = &out[16];
        desc[10] = &out[17];
        desc[11] = &out[18];
        desc[12] = &out[19];
        desc[13] = &out[20];
        desc[14] = &out[21];
        desc[15] = &out[22];
        desc[16] = &out[24];
        desc[17] = &out[26];
        desc[18] = &out[28];
        desc[19] = &out[30];

        sku_len = store_tlv_parse_sku_full(item, desc);
        if (sku_len < 0)
            return sku_len;

        body = item + sku_len;
        rem_full = (u32)tlen - (u32)sku_len;
        rem = (u32)(u16)rem_full;
        out[5] = (u32)body;
        sku_kind = out[11];

        ret = store_tlv_read_str(out, 32U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 33U, body, rem, 56U, 0);
        if (ret < 0)
            return ret;

        out[34] = out[1] + 4U;
        if (sku_kind == 2U) {
            ret = store_tlv_read_str(out, 34U, body, rem, 18U, 0);
            if (ret < 0)
                return ret;
        }

        ret = store_tlv_read_str(out, 35U, body, rem_full, 60U, 1);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem_full, 61U, NULL, &out[36]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem_full, 62U, NULL, &out[37]);
        if (ret < 0)
            return ret;
        ret = store_tlv_opt_count(body, rem_full, 128U, &out[38]);
        if (ret < 0)
            return ret;
        ret = store_tlv_opt_count(body, rem_full, 133U, &out[39]);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem_full, 149U, &out[40]);
    }
}

int store_tlv_parse_link_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 37U, 0U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 4;
        u32 rem = (u32)tlen - 4U;
        int ret;

        sceKernelMemset(out, 0, 56U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 11U, body, rem, 63U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 12U, body, rem, 64U, 1);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 13U, body, rem, 65U, 1);
    }
}

int store_tlv_parse_entitlement(const u32 *parent, u32 index, u32 *out)
{
    u32 hdr_copy[16];
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off;

    sceKernelMemcpy(hdr_copy, (const void *)parent[1], 56U);
    off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                             128U, 0U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 248;
        u32 rem = (u32)tlen - 248U;
        int ret;

        sceKernelMemset(out, 0, 104U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = store_tlv_be32(item + 49);
        out[12] = (u32)(item + 53);
        out[13] = (u32)(item + 110);
        out[14] = store_tlv_be32(item + 159);
        out[15] = store_tlv_be32(item + 163);
        out[16] = store_tlv_be32(item + 167);
        out[17] = (u32)(item + 171);
        out[18] = store_tlv_be32(item + 228);
        store_tlv_store_rtc(item + 232, &out[20]);
        out[22] = store_tlv_be32(item + 240);
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 23U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 24U, body, rem, 132U, 1);
    }
}

int store_tlv_parse_subscr_sku(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 149U, 0U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 248;
        u32 rem = (u32)tlen - 248U;
        int nested_off;
        int ret;

        sceKernelMemset(out, 0, 120U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = store_tlv_be32(item + 49);
        out[12] = (u32)(item + 110);
        out[13] = store_tlv_be32(item + 159);
        out[14] = store_tlv_be32(item + 163);
        out[15] = store_tlv_be32(item + 167);
        out[16] = (u32)(item + 171);
        out[17] = store_tlv_be32(item + 228);
        store_tlv_store_rtc(item + 232, &out[18]);
        out[20] = store_tlv_be32(item + 240);
        out[21] = store_tlv_be32(item + 244);
        out[5] = (u32)body;

        nested_off = sceNpCommerce2_E3455E27(body, rem, 131U, 20U, NULL);
        if (nested_off < 0)
            return nested_off;
        {
            const u8 *plan = body + nested_off;
            out[24] = store_tlv_be32(plan);
            store_tlv_be64_words(plan + 4, &out[26]);
            out[28] = store_tlv_be32(plan + 12);
            out[29] = store_tlv_be32(plan + 16);
        }

        ret = store_tlv_read_str(out, 22U, body, rem, 151U, 1);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem, 150U, &out[23]);
    }
}

int store_tlv_parse_download_file(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 156U, 0U, index, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 72U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = store_tlv_be32(item + 49);
        out[12] = store_tlv_be32(item + 53);
        out[13] = store_tlv_be32(item + 57);
        store_tlv_be64_words(item + 61, &out[14]);
        store_tlv_be64_words(item + 69, &out[16]);
        out[5] = (u32)(item + 77);
    }
    return off;
}

int store_tlv_parse_video_sku(const u32 *parent, u32 index, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 88U);
    if (api_ver == 3) {
        off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 144U, 0U, index, &tlen);
    } else {
        off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 133U, 0U, index, &tlen);
    }
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 77;
        u32 rem = (u32)tlen - 77U;
        int ret;

        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = store_tlv_be32(item + 49);
        out[12] = store_tlv_be32(item + 53);
        out[13] = store_tlv_be32(item + 57);
        out[14] = store_tlv_be32(item + 61);
        out[15] = store_tlv_be32(item + 65);
        store_tlv_be64_words(item + 69, &out[16]);
        out[5] = (u32)body;

        if (api_ver == 3) {
            ret = store_tlv_opt_count(body, rem, 17U, &out[21]);
            if (ret < 0)
                return ret;
        } else {
            ret = store_tlv_read_str(out, 21U, body, rem, 17U, 0);
            if (ret < 0)
                return ret;
        }

        ret = store_tlv_read_str(out, 18U, body, rem, 60U, 1);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 61U, NULL, &out[19]);
        if (ret < 0)
            return ret;
        return store_min_s32(sceNpCommerce2_3F6D413C(body, rem, 62U, NULL, &out[20]), 0);
    }
}

int store_tlv_parse_cart_entry(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth_ex(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                    46U, 24U, index, &tlen, parent[6]);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 28;
        int ret;

        sceKernelMemset(out, 0, 72U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[12] = store_tlv_be32(item + 8);
        out[13] = store_tlv_be32(item + 12);
        out[14] = store_tlv_be32(item + 16);
        out[15] = store_tlv_be32(item + 20);
        out[17] = store_tlv_be32(item + 24);
        out[5] = (u32)body;

        ret = sceNpCommerce2_E3455E27(body, out[4] - (u32)(body - (const u8 *)out[3]),
                                      24U, 12U, NULL);
        if (ret == STORE_ERR_TAG_NOT_FOUND) {
            out[16] = 0U;
            return 0;
        }
        if (ret >= 0) {
            out[16] = 1U;
            return 0;
        }
        return ret;
    }
}

int store_tlv_parse_cart_sku_brief(const u32 *parent, u32 *out)
{
    u32 *desc[16];
    u32 hdr_copy[16];
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off;

    sceKernelMemcpy(hdr_copy, (const void *)parent[1], 56U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  76U, 101U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        int sku_len;
        const u8 *body;
        u32 rem;
        u32 rem_full;
        u32 sku_kind;
        int ret;

        sceKernelMemset(out, 0, 104U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        sceKernelMemset(desc, 0, 60U);
        desc[0]  = hdr_copy;
        desc[1]  = (u32 *)2U;
        desc[2]  = &out[24];
        desc[3]  = &out[10];
        desc[4]  = &out[11];
        desc[5]  = &out[12];
        desc[6]  = &out[13];
        desc[9]  = &out[14];
        desc[13] = &out[15];
        desc[14] = &out[16];

        sku_len = store_tlv_parse_sku_fields(item, desc, 0);
        if (sku_len < 0)
            return sku_len;

        body = item + sku_len + 36;
        rem_full = (u32)tlen - (u32)(body - item);
        rem = (u32)(u16)rem_full;
        out[5] = (u32)body;
        sku_kind = out[11];

        ret = store_tlv_read_str(out, 17U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 18U, body, rem, 56U, 0);
        if (ret < 0)
            return ret;

        out[19] = out[1] + 4U;
        if (sku_kind == 2U) {
            ret = store_tlv_read_str(out, 19U, body, rem, 18U, 0);
            if (ret < 0)
                return ret;
        }

        ret = store_tlv_read_str(out, 20U, body, rem, 7U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 21U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 22U, body, rem, 4U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_opt_count(body, rem, 128U, &out[23]);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem_full, 156U, &out[25]);
    }
}

int store_tlv_parse_cart_sku_full(const u32 *parent, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 tag = (api_ver == 3) ? 138U : 77U;
    u32 *desc[20];
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 176U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  tag, 157U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *base = (const u8 *)parent[1];
        const u8 *item = scan + off;
        int sku_len;
        const u8 *body;
        u32 rem;
        u32 rem_full;
        u32 sku_kind;
        int ret;

        sceKernelMemset(out, 0, 40U);
        out[0] = parent[0];
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;

        sceKernelMemset(desc, 0, sizeof(desc));
        sceKernelMemset(desc, 0, 60U);
        desc[0]  = (u32 *)base;
        desc[1]  = (u32 *)2U;
        desc[2]  = &out[42];
        desc[3]  = &out[10];
        desc[4]  = &out[11];
        desc[5]  = &out[12];
        desc[6]  = &out[13];
        desc[9]  = &out[14];
        desc[11] = &out[15];
        desc[12] = &out[16];
        desc[13] = &out[17];
        desc[14] = &out[18];
        desc[15] = &out[20];
        desc[16] = &out[22];
        desc[17] = &out[24];
        desc[18] = &out[26];
        desc[19] = &out[28];

        sku_len = store_tlv_parse_sku_full(item, desc);
        if (sku_len < 0)
            return sku_len;

        body = item + sku_len;
        rem_full = (u32)tlen - (u32)sku_len;
        rem = (u32)(u16)rem_full;
        out[5] = (u32)body;
        sku_kind = out[11];

        ret = store_tlv_read_str(out, 30U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 31U, body, rem, 56U, 0);
        if (ret < 0)
            return ret;

        out[32] = out[1] + 4U;
        if (sku_kind == 2U) {
            ret = store_tlv_read_str(out, 32U, body, rem, 18U, 0);
            if (ret < 0)
                return ret;
        }

        ret = store_tlv_read_str(out, 36U, body, rem, 7U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 37U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 38U, body, rem, 4U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_opt_count(body, rem, 128U, &out[40]);
        if (ret < 0)
            return ret;

        ret = store_tlv_read_str(out, 33U, body, rem_full, 60U, 1);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem_full, 61U, NULL, &out[34]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem_full, 62U, NULL, &out[35]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 39U, body, rem_full, 68U, 1);
        if (ret < 0)
            return ret;

        if (api_ver == 2) {
            out[43] = (sceNpCommerce2_E3455E27(body, rem_full, 75U, 8U, NULL) < 0) ? 1U : 3U;
        }
        return store_tlv_opt_count(body, rem_full, 133U, &out[41]);
    }
}

int store_tlv_parse_audio_lang(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 61U, 0U, index, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        *code_out = (u32)item;
        if (flags_out) {
            if (tlen >= 7U)
                *flags_out = store_tlv_be32(item + 3);
            else
                *flags_out = 0U;
        }
    }
    return off;
}

int store_tlv_parse_card_info(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 48U, 25U, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 21;
        u32 rem = (u32)tlen - 21U;
        int ret;

        sceKernelMemset(out, 0, 68U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 11U, body, rem, 15U, 0);
        if (ret < 0)
            return ret;

        ret = sceNpCommerce2_E3455E27(body, rem, 99U, 20U, NULL);
        if (ret == STORE_ERR_TAG_NOT_FOUND)
            return 0;
        if (ret < 0)
            return ret;

        {
            const u8 *card = body + ret;
            const u8 *tail = card + 16;
            u32 tail_rem = (u32)tlen - (u32)(tail - item);
            out[12] = store_tlv_be32(card);
            out[13] = store_tlv_be32(card + 4);
            out[14] = store_tlv_be32(card + 8);
            out[15] = store_tlv_be32(card + 12);
            return store_tlv_read_str(out, 16U, tail, tail_rem, 98U, 0);
        }
    }
}

int store_tlv_parse_drm_item(const u32 *parent, u32 index, u32 *out)
{
    u32 hdr_copy[16];
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off;

    sceKernelMemcpy(hdr_copy, (const void *)parent[1], 56U);
    off = store_tlv_find_nth_paged(scan, parent[4], 43U, 125U, index, &tlen, parent[6]);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 125;
        u32 rem = (u32)tlen - 125U;
        u32 drm_kind;
        int ret;

        sceKernelMemset(out, 0, 184U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = store_tlv_be32(item + 49);
        out[12] = store_tlv_be32(item + 53);
        out[13] = store_tlv_be32(item + 57);
        store_tlv_be64_words(item + 61, &out[14]);
        store_tlv_store_rtc(item + 69, &out[16]);
        store_tlv_store_rtc(item + 77, &out[18]);
        out[20] = (store_tlv_be32(item + 85) != 0U) ? 1U : 0U;
        store_tlv_store_rtc(item + 89, &out[22]);
        store_tlv_be64_words(item + 97, &out[42]);
        out[45] = store_tlv_be32(item + 105);
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 24U, body, rem, 9U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 25U, body, rem, 10U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 26U, body, rem, 4U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 27U, body, rem, 5U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 28U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 29U, body, rem, 20U, 0);
        if (ret < 0)
            return ret;

        drm_kind = out[12];
        if (drm_kind == 1U) {
            int nested_off = sceNpCommerce2_E3455E27(body, rem, 152U, 8U, NULL);
            if (nested_off < 0)
                return nested_off;
            {
                const u8 *ent = body + nested_off;
                out[30] = store_tlv_be32(ent);
                out[31] = store_tlv_be32(ent + 4);
                store_tlv_be64_words(ent + 8, &out[32]);
            }
        } else if (drm_kind == 2U || drm_kind == 4U) {
            u32 nested_tag = (drm_kind == 4U) ? 141U : 86U;
            int nested_off = sceNpCommerce2_E3455E27(body, rem, nested_tag, 8U, NULL);
            if (nested_off < 0)
                return nested_off;
            {
                const u8 *ent = body + nested_off;
                out[30] = store_tlv_be32(ent);
                out[31] = store_tlv_be32(ent + 4);
                body = ent + 16;
                rem = (u32)tlen - (u32)(body - item);
            }
            ret = store_tlv_read_str(out, 35U, body, rem, 89U, 0);
            if (ret < 0)
                return ret;
            ret = store_tlv_read_str(out, 36U, body, rem, 85U, 1);
            if (ret < 0)
                return ret;
            ret = store_tlv_read_str(out, 37U, body, rem, 7U, 0);
            if (ret < 0)
                return ret;
            ret = store_tlv_read_str(out, 39U, body, rem, 68U, 1);
            if (ret < 0)
                return ret;
            out[38] = (sceNpCommerce2_E3455E27(body, rem, 75U, 8U, NULL) < 0) ? 1U : 3U;
            ret = store_tlv_opt_count(body, rem, 157U, &out[34]);
            if (ret < 0)
                return ret;
        }

        ret = store_tlv_read_str(out, 40U, body, rem, 56U, 1);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem, 128U, &out[44]);
    }
}

int store_tlv_parse_drm_reward(const u32 *parent, u32 *out, int api_ver)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 tag = (api_ver == 3) ? 141U : 86U;
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                      tag, 8U, &tlen);
    if (off < 0)
        return off;
    if (api_ver == 3)
        return store_tlv_parse_reward_ext(parent[0], (const u8 *)parent[1], scan + off, tlen,
                                          scan + off + 8, out);
    return store_tlv_parse_reward_std(parent[0], (const u8 *)parent[1], scan + off, tlen,
                                      scan + off + 8, out);
}

int store_tlv_parse_drm_child(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                      86U, 8U, NULL);
    if (off < 0)
        return off;

    {
        const u8 *inner = scan + off + 16;
        int nested_off = store_tlv_find_nth(inner, parent[4] - (u32)(inner - (const u8 *)parent[3]),
                                            157U, 0U, index, &tlen);
        if (nested_off < 0)
            return nested_off;

        {
            const u8 *item = inner + nested_off;
            const u8 *body = item + 110;
            u32 rem = (u32)tlen - 110U;
            int ret;

            sceKernelMemset(out, 0, 72U);
            {
                u32 parent_ver = parent[0];
                u32 parent_base = parent[1];
                sceKernelMemset(out, 0, 40U);
                out[0] = parent_ver;
                out[1] = parent_base;
            }
            out[3] = (u32)item;
            out[4] = (u32)tlen;
            out[10] = (u32)item;
            out[11] = (u32)(item + 49);
            out[12] = store_tlv_be32(item + 98);
            out[13] = store_tlv_be32(item + 102);
            out[14] = store_tlv_be32(item + 106);
            out[5] = (u32)body;

            ret = store_tlv_read_str(out, 15U, body, rem, 20U, 0);
            if (ret < 0)
                return ret;
            ret = store_tlv_read_str(out, 16U, body, rem, 89U, 0);
            if (ret < 0)
                return ret;
            return store_tlv_read_str(out, 17U, body, rem, 159U, 1);
        }
    }
}

static inline __attribute__((always_inline)) int
store_tlv_parse_addr_body(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;

        sceKernelMemset(out, 0, 64U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = store_tlv_read_str(out, 10U, body, body_len, 79U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 11U, body, body_len, 80U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 12U, body, body_len, 81U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 13U, body, body_len, 82U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 14U, body, body_len, 54U, 0);
        if (ret < 0)
            return ret;
        return store_min_s32(store_tlv_read_str(out, 15U, body, body_len, 84U, 0), 0);
    }
    return off;
}

int store_tlv_parse_address_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_parse_addr_body(out, resp, resp_size);
}

int store_tlv_parse_edy_info(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 52U, 29U, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 25;
        u32 rem = (u32)tlen - 25U;

        sceKernelMemset(out, 0, 56U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[12] = (u32)(item + 8);
        out[5] = (u32)body;
        return store_tlv_read_str(out, 13U, body, rem, 19U, 0);
    }
}

int store_tlv_parse_voucher_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 122U, 93U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 77;
        u32 rem = (u32)tlen - 77U;
        u32 item_kind;
        int ret;

        sceKernelMemset(out, 0, 104U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        store_tlv_store_rtc(item + 57, &out[12]);
        out[14] = store_tlv_be32(item + 65);
        out[22] = store_tlv_be32(item + 69);
        item_kind = store_tlv_be32(item + 73);
        out[23] = item_kind;
        if (item_kind != 3U && item_kind != 5U)
            out[14] = 0xFFFFFFFFU;
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 15U, body, rem, 4U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 16U, body, rem, 7U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 17U, body, rem, 8U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 18U, body, rem, 16U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 19U, body, rem, 12U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 20U, body, rem, 13U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 21U, body, rem, 119U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem, 128U, &out[24]);
    }
}

int store_tlv_parse_download_list(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off;

    sceKernelMemset(out, 0, 52U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  153U, 72U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 72;

        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item + 28);
        out[11] = store_tlv_be32(item + 32);
        out[5] = (u32)body;
        return store_tlv_opt_count(body, (u32)tlen - 72U, 156U, &out[12]);
    }
}

int store_tlv_parse_video_info(const u32 *parent, u32 *out, int api_ver)
{
    u32 hdr_copy[16];
    const u8 *scan = (const u8 *)parent[5];
    u32 main_tag = (api_ver == 3) ? 143U : 127U;
    u32 sku_tag = (api_ver == 3) ? 144U : 133U;
    u16 tlen;
    int off;

    sceKernelMemcpy(hdr_copy, (const void *)parent[1], 56U);
    sceKernelMemset(out, 0, 160U);
    off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                  main_tag, 100U, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 72;
        u32 rem = (u32)tlen - 72U;
        u32 raw_flags;
        int ret;

        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[12] = (store_tlv_be32(item + 8) != 0U) ? 1U : 0U;
        out[13] = store_tlv_be32(item + 12);
        out[14] = store_tlv_be32(item + 16);
        out[15] = store_tlv_be32(item + 20);
        raw_flags = store_tlv_be32(item + 24);
        out[16] = raw_flags;
        out[12] = ((raw_flags & 0x0AU) != 0U) ? 1U : 0U;
        out[17] = store_tlv_be32(item + 28);
        out[18] = store_tlv_be32(item + 32);
        store_tlv_store_rtc(item + 36, &out[20]);
        store_tlv_store_rtc(item + 44, &out[22]);
        store_tlv_store_rtc(item + 52, &out[24]);
        out[26] = store_tlv_be32(item + 60);
        store_tlv_be64_words(item + 64, &out[28]);
        out[5] = (u32)body;

        if (api_ver == 3) {
            ret = store_tlv_opt_count(body, rem, 17U, &out[39]);
            if (ret < 0)
                return ret;
        } else {
            ret = store_tlv_read_str(out, 39U, body, rem, 17U, 0);
            if (ret < 0)
                return ret;
        }

        ret = sceNpCommerce2_3F6D413C(body, rem, 35U, NULL, &out[30]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 36U, NULL, &out[31]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 37U, NULL, &out[32]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 33U, body, rem, 68U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 34U, body, rem, 69U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 35U, body, rem, 60U, 1);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 61U, NULL, &out[36]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, rem, 62U, NULL, &out[37]);
        if (ret < 0)
            return ret;
        return store_tlv_opt_count(body, rem, sku_tag, &out[38]);
    }
}

int store_tlv_parse_res_addr_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_parse_addr_body(out, resp, resp_size);
}

int store_tlv_parse_account_info(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 94U, 50U, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 46;
        u32 raw_pair[2];

        sceKernelMemset(out, 0, 88U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        sceKernelMemcpy(raw_pair, item, 8U);
        sceNpCommerce2_22CEC5E2(&out[10], 17U, store_bswap32(raw_pair[1]),
                                store_bswap32(raw_pair[0]), 0U);
        out[15] = (u32)(item + 8);
        out[16] = (u32)(item + 25);
        out[17] = (u32)(item + 28);
        out[18] = store_tlv_be32(item + 34);
        out[20] = (store_tlv_be32(item + 38) != 0U) ? 1U : 0U;
        out[21] = store_tlv_be32(item + 42);
        out[5] = (u32)body;
        return store_tlv_read_str(out, 19U, body, (u32)tlen - 46U, 93U, 0);
    }
}

int store_tlv_parse_reg_rules(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 95U, 44U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        u32 opt_val;

        sceKernelMemset(out, 0, 104U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[12] = store_tlv_be32(item + 8);
        opt_val = store_tlv_be32(item + 12);
        out[13] = opt_val;
        if (opt_val == 0U)
            out[14] = 0U;
        else if (opt_val == 1U)
            out[14] = 3U;
        else if (opt_val == 4U)
            out[14] = 2U;
        else if (opt_val == 8U)
            out[14] = 1U;
        out[15] = store_tlv_be32(item + 16);
        out[16] = store_tlv_be32(item + 20);
        out[17] = store_tlv_be32(item + 24);
        out[18] = store_tlv_be32(item + 28);
        out[19] = store_tlv_be32(item + 32);
        out[20] = store_tlv_be32(item + 36);
        out[21] = store_tlv_be32(item + 40);
        out[22] = store_tlv_be32(item + 44);
        out[23] = store_tlv_be32(item + 48);
        out[24] = store_tlv_be32(item + 52);
        out[25] = store_tlv_be32(item + 56);
        out[5] = (u32)(item + 60);
    }
    return off;
}

int store_tlv_parse_billing_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off < 0)
        return off;

    {
        const u8 *body = resp + off;
        int ret;

        sceKernelMemset(out, 0, 76U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_3F6D413C(body, body_len, 55U, NULL, &out[10]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, body_len, 57U, NULL, &out[11]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 12U, body, body_len, 79U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 13U, body, body_len, 80U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 14U, body, body_len, 81U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 15U, body, body_len, 82U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 16U, body, body_len, 54U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 17U, body, body_len, 84U, 0);
        if (ret < 0)
            return ret;

        ret = sceNpCommerce2_E3455E27(body, body_len, 48U, 25U, NULL);
        if (ret == STORE_ERR_TAG_NOT_FOUND) {
            out[18] = 0U;
            return 0;
        }
        if (ret >= 0) {
            out[18] = 1U;
            return 0;
        }
        return ret;
    }
}

int store_tlv_parse_auth_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off < 0)
        return off;

    {
        const u8 *body = resp + off;
        u16 plen;
        int ret;

        sceKernelMemset(out, 0, 56U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[13] = (u32)(resp + 4);

        ret = sceNpCommerce2_E3455E27(body, body_len, 164U, 0U, &plen);
        if (ret >= 0 && plen != 0U)
            out[13] = (u32)(body + ret);
        if (ret < 0 && ret != STORE_ERR_TAG_NOT_FOUND)
            return ret;
        if (ret != STORE_ERR_TAG_NOT_FOUND)
            return 0;

        ret = sceNpCommerce2_3F6D413C(body, body_len, 22U, NULL, &out[10]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, body_len, 100U, NULL, &out[11]);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_3F6D413C(body, body_len, 88U, NULL, &out[12]);
        if (ret < 0)
            return ret;
        return 0;
    }
}

int store_tlv_parse_console_act(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 116U, 12U, index, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        u32 act_type;

        sceKernelMemset(out, 0, 64U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        act_type = store_tlv_be32(item);
        out[10] = act_type;
        out[11] = act_type;
        if (act_type - 1U >= 2U)
            out[10] = 0U;
        out[12] = store_tlv_be32(item + 4);
        out[13] = (store_tlv_be32(item + 8) != 0U) ? 1U : 0U;
        out[14] = store_tlv_be32(item + 12);
        out[15] = store_tlv_be32(item + 16);
        out[5] = (u32)(item + 20);
    }
    return off;
}

/* =========================================================================
 * Per-command root initializers & wrapper thunks (0x0104F0 .. 0x0130E0)
 * ========================================================================= */

int store_tlv_parse_err_flag(const u8 *resp, u32 resp_size, u32 *flag_out)
{
    u32 ver;
    u32 body_len;
    u32 count = 0U;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        *flag_out = (sceNpCommerce2_3F6D413C(resp + off, body_len, 120U, NULL, &count) >= 0)
                    ? 1U : 0U;
        return 0;
    }
    return off;
}

int store_tlv_init_err_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 120U);
}

int store_tlv_parse_err_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 120U, 8U, index, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[5] = (u32)(item + 8);
    }
    return off;
}

int store_tlv_init_sess_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 22U);
}

int store_tlv_get_sess_currency(const u32 *parent, u32 index, u32 *out)
{
    return store_tlv_parse_currency(parent, index, out);
}

int store_tlv_parse_eula_flag(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 87U, 4U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 44U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (store_tlv_be32(item) != 0U) ? 1U : 0U;
        out[5] = (u32)(item + 4);
    }
    return off;
}

int store_tlv_get_cat_icon_url(const u32 *parent, u32 index, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 17U, 0U, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(scan + off);
    return off;
}

int store_tlv_parse_category_item(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                      33U, 81U, &tlen);
    if (off >= 0)
        return store_tlv_parse_content_item(scan + off, tlen, (const u8 *)parent[1],
                                            parent[0], 0U, out);
    return off;
}

int store_tlv_parse_rating_first(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                      24U, 12U, &tlen);
    if (off >= 0)
        return store_tlv_parse_rating_system(scan + off, tlen, (const u8 *)parent[1],
                                             parent[0], out);
    return off;
}

int store_tlv_parse_rating_nth_cat(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 24U, 12U, index, &tlen);
    if (off >= 0)
        return store_tlv_parse_rating_system(scan + off, tlen, (const u8 *)parent[1],
                                             parent[0], out);
    return off;
}

int store_tlv_get_reward_std(const u32 *parent, u32 *out)
{
    return store_tlv_parse_reward_std(parent[0], (const u8 *)parent[1], (const u8 *)parent[3],
                                      parent[4], (const u8 *)parent[5], out);
}

int store_tlv_get_reward_ext(const u32 *parent, u32 *out)
{
    return store_tlv_parse_reward_ext(parent[0], (const u8 *)parent[1], (const u8 *)parent[3],
                                      parent[4], (const u8 *)parent[5], out);
}

int store_tlv_parse_rating_nth_prod(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 24U, 12U, index, &tlen);
    if (off >= 0)
        return store_tlv_parse_rating_system(scan + off, tlen, (const u8 *)parent[1],
                                             parent[0], out);
    return off;
}

int store_tlv_get_prod_genre(const u32 *parent, u32 index, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 35U, 0U, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(scan + off);
    return off;
}

int store_tlv_parse_credit_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 36U, 0U, index, &tlen);
    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        int ret;

        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[5] = (u32)item;

        ret = store_tlv_read_str(out, 10U, item, tlen, 66U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 11U, item, tlen, 67U, 1);
    }
}

int store_tlv_parse_subscr_ent(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 150U, 0U, index, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = (u32)(item + 57);
    }
    return off;
}

int store_tlv_init_add_cart_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_cart_root_std(out, resp, resp_size);
}

int store_tlv_get_add_cart_wallet(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_get_add_cart_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_cart_summary(parent, out);
}

int store_tlv_parse_cart_item(const u32 *parent, int indexed, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u32 scan_len = parent[4] - (u32)(scan - (const u8 *)parent[3]);
    u16 tlen;
    int off;

    if (indexed == 1)
        off = store_tlv_find_nth(scan, scan_len, 78U, 105U, index, &tlen);
    else
        off = sceNpCommerce2_E3455E27(scan, scan_len, 78U, 105U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 44U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[5] = (u32)(item + 4);
    }
    return off;
}

int store_tlv_get_subtitle_lang(const u32 *parent, u32 index, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 62U, 0U, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(scan + off);
    return off;
}

int store_tlv_init_rem_cart_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_cart_root_std(out, resp, resp_size);
}

int store_tlv_get_rem_cart_wallet(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_get_rem_cart_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_cart_summary(parent, out);
}

int store_tlv_init_get_cart_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_cart_root_std(out, resp, resp_size);
}

int store_tlv_get_cart_wallet(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_get_cart_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_cart_summary(parent, out);
}

int store_tlv_init_pur_prev_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 48U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_E3455E27(body, body_len, 45U, 4U, NULL);
        if (ret >= 0) {
            u32 raw;
            sceKernelMemcpy(&raw, body + ret, 4U);
            raw = store_bswap32(raw);
            sceKernelMemcpy(&out[10], &raw, 4U);
        } else if (ret != STORE_ERR_TAG_NOT_FOUND) {
            return ret;
        }

        ret = sceNpCommerce2_E3455E27(body, body_len, 21U, 4U, NULL);
        if (ret >= 0) {
            out[11] = store_tlv_be32(body + ret);
            return 0;
        }
        if (((ret != STORE_ERR_TAG_NOT_FOUND) & ((u32)ret >> 31)) == 0)
            return 0;
        return ret;
    }
    return off;
}

int store_tlv_get_pur_prev_wallet(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_get_pur_prev_tax(const u32 *parent, u32 *out)
{
    return store_tlv_parse_tax_info(parent, (const u8 *)parent[3], parent[4], out);
}

int store_tlv_get_pur_prev_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_cart_summary(parent, out);
}

int store_tlv_init_purch_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 56U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_E3455E27(body, body_len, 21U, 4U, NULL);
        if (ret >= 0) {
            out[10] = store_tlv_be32(body + ret);
        } else if (ret != STORE_ERR_TAG_NOT_FOUND) {
            return ret;
        }

        ret = sceNpCommerce2_E3455E27(body, body_len, 3U, 8U, NULL);
        if (ret >= 0) {
            store_tlv_be64_words(body + ret, &out[12]);
            return 0;
        }
        if (((ret != STORE_ERR_TAG_NOT_FOUND) & ((u32)ret >> 31)) == 0)
            return 0;
        return ret;
    }
    return off;
}

int store_tlv_get_purch_wallet(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_get_purch_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_cart_summary(parent, out);
}

int store_tlv_get_purch_tax(const u32 *parent, u32 *out)
{
    return store_tlv_parse_tax_info(parent, (const u8 *)parent[3], parent[4], out);
}

int store_tlv_init_get_cc_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_simple_root(out, resp, resp_size);
}

int store_tlv_get_cc_card(const u32 *parent, u32 *out)
{
    return store_tlv_parse_card_info(parent, out);
}

int store_tlv_init_get_prov_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 55U);
}

int store_tlv_get_prov_item(const u32 *parent, u32 index, u32 *out)
{
    return store_tlv_parse_province_info(parent, index, out);
}

int store_tlv_parse_city_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 59U, 69U, index, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        const u8 *body = item + 65;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[5] = (u32)body;
        return store_tlv_read_str(out, 11U, body, (u32)tlen - 65U, 58U, 0);
    }
    return off;
}

int store_tlv_init_get_pay_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 57U);
}

int store_tlv_get_pay_item(const u32 *parent, u32 index, u32 *out)
{
    return store_tlv_parse_pay_method(parent, index, out);
}

int store_tlv_init_get_drm_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    u32 extra;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, &extra);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 52U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[6] = extra;

        ret = sceNpCommerce2_69FEA1C8(body, body_len, 43U, 0U, &out[10], extra);
        if (ret >= 0)
            return store_min_s32(store_tlv_clamp_paged(out, 43U, 125U, &out[10]), 0);
        return ret;
    }
    return off;
}

int store_tlv_init_addr_lookup_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 52U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = store_tlv_read_str(out, 10U, body, body_len, 79U, 0);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 11U, body, body_len, 82U, 0);
        if (ret < 0)
            return ret;
        return store_min_s32(store_tlv_read_str(out, 12U, body, body_len, 54U, 0), 0);
    }
    return off;
}

int store_tlv_init_get_wal_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_simple_root(out, resp, resp_size);
}

int store_tlv_get_wal_info(const u32 *parent, u32 *out)
{
    return store_tlv_parse_wallet_info(parent, out);
}

int store_tlv_init_topup_cc_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 56U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_E3455E27(body, body_len, 21U, 4U, NULL);
        if (ret < 0)
            return ret;
        out[10] = store_tlv_be32(body + ret);

        ret = sceNpCommerce2_E3455E27(body, body_len, 45U, 4U, NULL);
        if (ret < 0)
            return ret;
        {
            u32 raw;
            sceKernelMemcpy(&raw, body + ret, 4U);
            raw = store_bswap32(raw);
            sceKernelMemcpy(&out[11], &raw, 4U);
        }

        ret = sceNpCommerce2_E3455E27(body, body_len, 3U, 8U, NULL);
        if (ret >= 0)
            store_tlv_be64_words(body + ret, &out[12]);
        return store_min_s32(ret, 0);
    }
    return off;
}

int store_tlv_get_topup_cc_card(const u32 *parent, u32 *out)
{
    return store_tlv_parse_card_info(parent, out);
}

int store_tlv_init_top_edy_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_simple_root(out, resp, resp_size);
}

int store_tlv_init_vchr_prev_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 44U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_E3455E27(body, body_len, 49U, 4U, NULL);
        if (ret >= 0)
            out[10] = store_tlv_be32(body + ret);
        return store_min_s32(ret, 0);
    }
    return off;
}

int store_tlv_get_vchr_prev_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_voucher_summary(parent, out);
}

static inline __attribute__((always_inline)) int
store_tlv_parse_vchr_items_body(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 51U, 4U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 44U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[5] = (u32)item;
        return sceNpCommerce2_3F6D413C(item, tlen, 122U, NULL, &out[10]);
    }
    return off;
}

int store_tlv_get_vchr_prev_items(const u32 *parent, u32 *out)
{
    return store_tlv_parse_vchr_items_body(parent, out);
}

int store_tlv_init_vchr_red_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 56U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;

        ret = sceNpCommerce2_E3455E27(body, body_len, 49U, 4U, NULL);
        if (ret < 0)
            return ret;
        out[10] = store_tlv_be32(body + ret);

        ret = sceNpCommerce2_E3455E27(body, body_len, 3U, 8U, NULL);
        if (ret >= 0)
            store_tlv_be64_words(body + ret, &out[12]);
        return store_min_s32(ret, 0);
    }
    return off;
}

int store_tlv_get_vchr_red_summary(const u32 *parent, u32 *out)
{
    return store_tlv_parse_voucher_summary(parent, out);
}

int store_tlv_get_vchr_red_items(const u32 *parent, u32 *out)
{
    return store_tlv_parse_vchr_items_body(parent, out);
}

int store_tlv_init_search_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    u32 extra;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, &extra);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 52U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[6] = extra;

        ret = sceNpCommerce2_69FEA1C8(body, body_len, 40U, 0U, &out[10], extra);
        if (ret >= 0)
            return store_min_s32(store_tlv_clamp_paged(out, 40U, 4U, &out[10]), 0);
        return ret;
    }
    return off;
}

int store_tlv_parse_search_item_paged(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth_paged(scan, parent[4], 40U, 4U, index, &tlen, parent[6]);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 44U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[5] = (u32)(item + 4);
    }
    return off;
}

int store_tlv_get_search_tag_ptr(const u32 *parent, u32 index, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 92U, 0U, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(scan + off);
    return off;
}

int store_tlv_parse_search_item_ext(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                 155U, 71U, index, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[5] = (u32)(item + 71);
        out[10] = (u32)item;
        out[11] = (u32)(item + 6);
        return 0;
    }
    return off;
}

int store_tlv_parse_search_item_single(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[5];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4] - (u32)(scan - (const u8 *)parent[3]),
                                      155U, 71U, &tlen);
    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[5] = (u32)(item + 71);
        out[10] = (u32)item;
        out[11] = (u32)(item + 6);
        return 0;
    }
    return off;
}

int store_tlv_get_billing_prov(const u32 *parent, u32 index, u32 *out)
{
    return store_tlv_parse_province_info(parent, index, out);
}

int store_tlv_get_billing_pay(const u32 *parent, u32 index, u32 *out)
{
    return store_tlv_parse_pay_method(parent, index, out);
}

int store_tlv_get_billing_card(const u32 *parent, u32 *out)
{
    return store_tlv_parse_card_info(parent, out);
}

int store_tlv_init_cnsl_act_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 116U);
}

int store_tlv_init_epw_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, NULL);

    if (off >= 0) {
        const u8 *body = resp + off;
        sceKernelMemset(out, 0, 44U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        return store_min_s32(store_tlv_read_str(out, 10U, body, body_len, 118U, 0), 0);
    }
    return off;
}

int store_tlv_init_prod_list_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    u32 extra;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, &extra);

    if (off >= 0) {
        const u8 *body = resp + off;
        sceKernelMemset(out, 0, 44U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[6] = extra;
        return store_min_s32(sceNpCommerce2_3546C737(body, body_len, 40U, 0U, &out[10], extra), 0);
    }
    return off;
}

int store_tlv_parse_prod_list_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth_ex(scan, parent[4], 40U, 4U, index, &tlen, parent[6]);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 44U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[5] = (u32)(item + 4);
    }
    return off;
}

int store_tlv_init_recom_root(u32 *out, const u8 *resp, u32 resp_size)
{
    u32 ver;
    u32 body_len;
    u32 extra;
    int off = store_tlv_parse_root(resp, resp_size, &ver, &body_len, &extra);

    if (off >= 0) {
        const u8 *body = resp + off;
        int ret;
        sceKernelMemset(out, 0, 56U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)resp;
        out[2] = (u32)off + body_len;
        out[3] = (u32)body;
        out[4] = body_len;
        out[6] = extra;

        ret = store_tlv_read_str(out, 13U, body, body_len, 148U, 1);
        if (ret < 0)
            return ret;
        ret = sceNpCommerce2_69FEA1C8(body, body_len, 40U, 0U, &out[10], extra);
        if (ret >= 0)
            return store_min_s32(store_tlv_clamp_paged(out, 40U, 4U, &out[10]), 0);
        return ret;
    }
    return off;
}

int store_tlv_init_prod_ids_root(u32 *out, const u8 *resp, u32 resp_size)
{
    return store_tlv_init_counted_root(out, resp, resp_size, 56U);
}

int store_tlv_get_prod_id_ptr(const u32 *parent, u32 index, u32 *out_ptr)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 56U, 0U, index, &tlen);
    if (off >= 0)
        *out_ptr = (u32)(scan + off);
    return off;
}

/* =========================================================================
 * Low-level TLV scanners & shared sub-parsers (0x013134 .. 0x015160)
 * ========================================================================= */

static int store_tlv_parse_root(const u8 *resp, u32 resp_size, u32 *ver_out,
                                u32 *body_len_out, u32 *extra_out)
{
    u32 hdr[16];
    u8 tail_hdr[16];

    if (resp_size < 60U)
        return STORE_ERR_RESP_TOO_SMALL;
    sceKernelMemcpy(hdr, resp, 56U);
    sceKernelMemcpy(tail_hdr, resp + 56, 4U);
    if (resp_size < hdr[2] + 56U)
        return STORE_ERR_RESP_TOO_SMALL;

    *body_len_out = hdr[2] - 4U;
    if (ver_out)
        *ver_out = hdr[0];
    if (extra_out)
        *extra_out = hdr[3];
    return 60;
}

static int store_tlv_find_nth(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                              u32 index, u16 *len_out)
{
    u16 item_len;
    u16 group_len;
    u32 count;
    int off = sceNpCommerce2_3F6D413C(scan, scan_len, tag, &group_len, &count);

    if (off < 0)
        return off;
    if (index >= count)
        return STORE_ERR_OUT_OF_RANGE;

    {
        const u8 *group_start = scan + off;
        const u8 *cur = group_start;
        u32 iter = 0U;

        while (1) {
            if (iter != 0U)
                cur += item_len;
            off = sceNpCommerce2_E3455E27(cur, (u32)group_len - (u32)(cur - group_start),
                                          tag, min_len, &item_len);
            iter++;
            cur += off;
            if (off < 0)
                return off;
            if (index < iter) {
                *len_out = item_len;
                return (int)(cur - scan);
            }
        }
    }
}

static int store_tlv_find_nth_ex(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                                 u32 index, u16 *len_out, u32 extra)
{
    u16 item_len;
    u32 count;
    int off = sceNpCommerce2_3546C737(scan, scan_len, tag, 0U, &count, extra);

    if (off < 0)
        return off;
    if (index >= count)
        return STORE_ERR_OUT_OF_RANGE;

    {
        const u8 *cur = scan + off;
        u32 iter = 0U;

        while (1) {
            if (iter != 0U)
                cur += item_len;
            off = sceNpCommerce2_E3455E27(cur, scan_len - (u32)(cur - scan),
                                          tag, min_len, &item_len);
            iter++;
            cur += off;
            if (off < 0)
                return off;
            if (index < iter) {
                *len_out = item_len;
                return (int)(cur - scan);
            }
        }
    }
}

static int store_tlv_find_nth_paged(const u8 *scan, u32 scan_len, u16 tag, u16 min_len,
                                    u32 index, u16 *len_out, u32 extra)
{
    u32 range[4];
    u16 item_len;
    int off = sceNpCommerce2_69FEA1C8(scan, scan_len, tag, 0U, range, extra);

    if (off < 0)
        return off;
    if (index >= range[1])
        return STORE_ERR_OUT_OF_RANGE;

    {
        const u8 *cur = scan + off;
        u32 iter = 0U;

        while (1) {
            if (iter != 0U)
                cur += item_len;
            off = sceNpCommerce2_E3455E27(cur, scan_len - (u32)(cur - scan),
                                          tag, min_len, &item_len);
            iter++;
            cur += off;
            if (off < 0)
                return off;
            if (index < iter) {
                *len_out = item_len;
                return (int)(cur - scan);
            }
        }
    }
}

static int store_tlv_clamp_paged(const u32 *cursor, u16 tag, u16 min_len, u32 *range_out)
{
    u16 item_len;
    u32 idx = 0U;
    int ret = 0;
    int clamped = 0;

    while (idx < range_out[1]) {
        ret = store_tlv_find_nth_paged((const u8 *)cursor[3], cursor[4], tag, min_len,
                                       idx, &item_len, cursor[6]);
        if (ret < 0) {
            if (idx != 0U) {
                ret = 0;
                clamped = 1;
            }
            break;
        }
        idx++;
    }
    if (clamped == 1)
        range_out[1] = idx;
    return ret;
}

static int store_tlv_parse_currency(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 22U, 42U, index, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = scan + off;
        const u8 *body = item + 30;
        u16 decimals;
        int ret;

        sceKernelMemset(out, 0, 84U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[11] = (u32)(item + 4);
        decimals = store_tlv_be16(item + 8);
        *((u16 *)&out[12]) = decimals;
        out[13] = store_tlv_be32(item + 10);
        out[14] = store_tlv_be32(item + 14);
        out[16] = store_tlv_be32(item + 18);
        out[17] = store_tlv_be32(item + 22);
        out[18] = store_tlv_be32(item + 26);
        out[5] = (u32)body;

        ret = sceNpCommerce2_3F6D413C(body, (u32)tlen - (u32)(body - (const u8 *)out[3]),
                                      21U, NULL, &out[15]);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 19U, body, (u32)tlen - (u32)(body - (const u8 *)out[3]),
                                 125U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 20U, body, (u32)tlen - (u32)(body - (const u8 *)out[3]),
                                  126U, 0);
    }
}

static int store_tlv_parse_content_hdr(const u8 *base, const u8 *item, u32 *id_out,
                                       u32 *type_out, u32 *date_out, u32 *cat_cnt_out,
                                       u32 *prod_cnt_out, u32 *has_flag_out, u32 *raw_flags_out,
                                       u32 *sp_flags_out)
{
    u32 raw_flags;
    s32 sp_bits;

    (void)base;
    *id_out = (u32)item;
    *type_out = store_tlv_be32(item + 49);
    store_tlv_store_rtc(item + 53, date_out);
    *cat_cnt_out = store_tlv_be32(item + 61);
    *prod_cnt_out = store_tlv_be32(item + 65);
    raw_flags = store_tlv_be32(item + 69);
    *raw_flags_out = raw_flags;
    *has_flag_out = ((raw_flags & 0x0AU) != 0U) ? 1U : 0U;
    sp_bits = (s32)store_tlv_be32(item + 73);
    if (sp_bits < 0)
        *sp_flags_out |= 1U;
    if ((sp_bits & 0x40000000) != 0)
        *sp_flags_out |= 2U;
    if ((sp_bits & 0x20000000) != 0)
        *sp_flags_out |= 4U;
    return 77;
}

static int store_tlv_parse_content_item(const u8 *item, u32 item_len, const u8 *base, u32 ver,
                                        u32 kind, u32 *out)
{
    u32 hdr_copy[16];
    int is_new_sdk;
    s32 sp_bits;
    const u8 *body;
    u32 rem;
    u32 rem_full;
    int ret;

    sceKernelMemcpy(hdr_copy, base, 56U);
    is_new_sdk = ((u32)sceKernelGetCompiledSdkVersion() > STORE_SDK_VER_620);
    sceKernelMemset(out, 0, is_new_sdk ? 128U : 88U);
    sceKernelMemset(out, 0, 40U);
    out[0] = ver;
    out[1] = (u32)base;
    out[3] = (u32)item;
    out[4] = item_len;
    out[10] = kind;
    out[11] = (u32)item;
    out[12] = store_tlv_be32(item + 49);
    store_tlv_store_rtc(item + 53, &out[14]);
    sp_bits = (s32)store_tlv_be32(item + 73);
    if (is_new_sdk) {
        if (sp_bits < 0)
            out[22] |= 1U;
        if ((sp_bits & 0x40000000) != 0)
            out[22] |= 2U;
        if ((sp_bits & 0x20000000) != 0)
            out[22] |= 4U;
    }

    body = item + 77;
    rem_full = item_len - 77U;
    rem = (u32)(u16)rem_full;
    out[5] = (u32)body;

    ret = store_tlv_read_str(out, 16U, body, rem, 7U, 0);
    if (ret < 0)
        return ret;
    ret = store_tlv_read_str(out, 17U, body, rem, 12U, 0);
    if (ret < 0)
        return ret;
    ret = store_tlv_read_str(out, 18U, body, rem, 16U, 0);
    if (ret < 0)
        return ret;
    ret = store_tlv_read_str(out, 19U, body, rem, 4U, 0);
    if (ret < 0)
        return ret;

    if (kind == 1U) {
        ret = store_tlv_read_str(out, 20U, body, rem_full, 13U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 21U, body, rem_full, 119U, 1);
    }
    return ret;
}

static int store_tlv_parse_rating_system(const u8 *item, u32 item_len, const u8 *base, u32 ver,
                                         u32 *out)
{
    const u8 *body = item + 17;
    u32 rem = item_len - 17U;
    int ret;

    sceKernelMemset(out, 0, 52U);
    sceKernelMemset(out, 0, 40U);
    out[0] = ver;
    out[1] = (u32)base;
    out[3] = (u32)item;
    out[4] = item_len;
    out[5] = (u32)body;
    out[10] = (u32)item;

    ret = store_tlv_read_str(out, 11U, body, rem, 16U, 0);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_3F6D413C(body, rem, 23U, NULL, &out[12]);
}

static int store_tlv_parse_sku_fields(const u8 *item, u32 **desc, int skip_extra)
{
    const u8 *cur = item;
    u32 extra_flag = 0U;
    u32 sku_kind;

    *desc[3] = (u32)cur;
    sku_kind = store_tlv_be32(cur + 57);
    *desc[4] = (sku_kind == 2U) ? 2U : 1U;
    *desc[5] = store_tlv_be32(cur + 61);
    *desc[6] = store_tlv_be32(cur + 65);
    cur += 69;

    if (desc[7])
        *desc[7] = store_tlv_be32(cur);
    cur += 4;
    if (desc[8])
        *desc[8] = store_tlv_be32(cur);
    cur += 4;
    *desc[9] = (store_tlv_be32(cur) != 0U) ? 1U : 0U;
    cur += 4;

    if (((u32)desc[1] & 4U) != 0U)
        extra_flag = store_tlv_be32(cur);
    cur += 4;

    if (desc[10])
        *desc[10] = store_tlv_be32(cur);
    cur += 4;
    if (desc[11])
        *desc[11] = store_tlv_be32(cur);
    cur += 4;
    if (desc[12])
        *desc[12] = store_tlv_be32(cur);
    cur += 4;

    if (!skip_extra && desc[13])
        *desc[13] = store_tlv_be32(cur);
    cur += 4;
    if (!skip_extra && desc[14])
        *desc[14] = store_tlv_be32(cur);
    cur += 4;

    if (!skip_extra) {
        if (desc[7] && *desc[7] != 0U)
            *desc[2] |= 1U;
        if (desc[9] && *desc[9] != 0U)
            *desc[2] |= 2U;
        if (extra_flag != 0U)
            *desc[2] |= 4U;
    }
    return (int)(cur - item);
}

static int store_tlv_parse_sku_full(const u8 *item, u32 **desc)
{
    int base_len = store_tlv_parse_sku_fields(item, desc, 0);
    const u8 *cur;

    if (base_len < 0)
        return base_len;

    cur = item + base_len;
    store_tlv_store_rtc(cur, desc[15]);
    cur += 8;
    store_tlv_store_rtc(cur, desc[16]);
    cur += 8;
    store_tlv_store_rtc(cur, desc[17]);
    cur += 8;
    *desc[18] = store_tlv_be32(cur);
    store_tlv_be64_words(cur + 4, desc[19]);
    return (int)((cur + 12) - item);
}

static int store_tlv_parse_reward_std(u32 ver, const u8 *base, const u8 *cur, u32 cur_len,
                                      const u8 *body, u32 *out)
{
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(body, cur_len - (u32)(body - cur), 75U, 8U, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = body + off;
        const u8 *tail = item + 8;
        u32 rem = (u32)tlen - 8U;
        int ret;

        sceKernelMemset(out, 0, 76U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[5] = (u32)tail;

        ret = store_tlv_read_str(out, 12U, tail, rem, 90U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 13U, tail, rem, 70U, 1);
        if (ret < 0)
            return ret;
        ret = store_tlv_read_str(out, 14U, tail, rem, 71U, 1);
        if (ret < 0)
            return ret;
        return store_min_s32(store_tlv_parse_discount_info(out, &out[15], &out[16],
                                                           &out[17], &out[18]), 0);
    }
}

static int store_tlv_parse_reward_ext(u32 ver, const u8 *base, const u8 *cur, u32 cur_len,
                                      const u8 *body, u32 *out)
{
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(body, cur_len - (u32)(body - cur), 145U, 8U, &tlen);

    if (off < 0)
        return off;

    {
        const u8 *item = body + off;
        const u8 *tail = item + 8;
        u32 rem = (u32)tlen - 8U;
        int ret;

        sceKernelMemset(out, 0, 68U);
        sceKernelMemset(out, 0, 40U);
        out[0] = ver;
        out[1] = (u32)base;
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[5] = (u32)tail;

        ret = store_tlv_read_str(out, 12U, tail, rem, 146U, 1);
        if (ret < 0)
            return ret;
        return store_min_s32(store_tlv_parse_discount_info(out, &out[13], &out[14],
                                                           &out[15], &out[16]), 0);
    }
}

static int store_tlv_parse_wallet_info(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 44U, 12U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 56U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        store_tlv_store_rtc(item + 4, &out[12]);
        out[5] = (u32)(item + 12);
    }
    return off;
}

static int store_tlv_parse_cart_summary(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 47U, 16U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        const u8 *body = item + 12;
        sceKernelMemset(out, 0, 56U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[12] = store_tlv_be32(item + 8);
        out[5] = (u32)body;
        return sceNpCommerce2_3546C737(body, (u32)tlen - 12U, 46U, 0U, &out[13], parent[6]);
    }
    return off;
}

static int store_tlv_parse_tax_info(const u32 *parent, const u8 *scan, u32 scan_len, u32 *out)
{
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, scan_len, 88U, 8U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[5] = (u32)(item + 8);
    }
    return off;
}

static int store_tlv_parse_province_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 55U, 73U, index, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        const u8 *body = item + 65;
        u32 rem = (u32)tlen - 65U;
        int ret;

        sceKernelMemset(out, 0, 52U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 11U, body, rem, 54U, 0);
        if (ret < 0)
            return ret;
        return sceNpCommerce2_3F6D413C(body, rem, 59U, NULL, &out[12]);
    }
    return off;
}

static int store_tlv_parse_pay_method(const u32 *parent, u32 index, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = store_tlv_find_nth(scan, parent[4], 57U, 25U, index, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        const u8 *body = item + 17;
        u32 rem = (u32)tlen - 17U;
        int ret;

        sceKernelMemset(out, 0, 52U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = (u32)item;
        out[5] = (u32)body;

        ret = store_tlv_read_str(out, 11U, body, rem, 15U, 0);
        if (ret < 0)
            return ret;
        return store_tlv_read_str(out, 12U, body, rem, 16U, 0);
    }
    return off;
}

static int store_tlv_parse_voucher_summary(const u32 *parent, u32 *out)
{
    const u8 *scan = (const u8 *)parent[3];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, parent[4], 50U, 8U, &tlen);

    if (off >= 0) {
        const u8 *item = scan + off;
        sceKernelMemset(out, 0, 48U);
        {
            u32 parent_ver = parent[0];
            u32 parent_base = parent[1];
            sceKernelMemset(out, 0, 40U);
            out[0] = parent_ver;
            out[1] = parent_base;
        }
        out[3] = (u32)item;
        out[4] = (u32)tlen;
        out[10] = store_tlv_be32(item);
        out[11] = store_tlv_be32(item + 4);
        out[5] = (u32)(item + 8);
    }
    return off;
}

static int store_tlv_parse_discount_info(const u32 *cursor, u32 *has_disc, u32 *disc_type,
                                         u32 *disc_val, u32 *disc_price)
{
    const u8 *scan = (const u8 *)cursor[5];
    u16 tlen;
    int off = sceNpCommerce2_E3455E27(scan, cursor[4] - (u32)(scan - (const u8 *)cursor[3]),
                                      74U, 12U, &tlen);
    if (off == STORE_ERR_TAG_NOT_FOUND) {
        *has_disc = 0U;
        return 0;
    }
    if (off >= 0) {
        const u8 *item = scan + off;
        *disc_type = store_tlv_be32(item);
        *disc_val = store_tlv_be32(item + 4);
        *disc_price = store_tlv_be32(item + 8);
        *has_disc = 1U;
        return 0;
    }
    return off;
}
