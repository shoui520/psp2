#include "store_internal.h"

static void tlv_reset_hdr(u32 *out, u32 total_size, u32 ver, const u8 *base,
                          const u8 *data, u32 data_size, const u8 *body)
{
    sceKernelMemset(out, 0, total_size);
    out[0] = ver;
    out[1] = (u32)(size_t)base;
    out[3] = (u32)(size_t)data;
    out[4] = data_size;
    out[5] = (u32)(size_t)body;
}

static int tlv_read_str(const u8 *scan, u32 scan_len, u16 tag, const u8 *base, u32 *out_ptr)
{
    u16 tlen = 0;
    int off;

    *out_ptr = (u32)(size_t)(base + 4);
    off = sceNpCommerce2_E3455E27(scan, scan_len, tag, 0, &tlen);
    if (off >= 0 && tlen != 0)
        *out_ptr = (u32)(size_t)(scan + off);
    return off;
}

static void tlv_store_rtc(const u8 *ptr, u32 *out_pair)
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

static int tlv_allow_missing(int ret)
{
    return (ret == STORE_ERR_TAG_NOT_FOUND) ? 0 : ret;
}

/* =========================================================================
 * Category, Product, and SKU TLV parsers
 * ========================================================================= */

int store_tlv_parse_category_info(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 139 : 38;
    u16 tlen = 0;
    int off, hdr_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemset(out, 0, 104);
    off = sceNpCommerce2_E3455E27(body, rem, tag, 101, &tlen);
    if (off < 0)
        return off;

    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, NULL);
    hdr_len = store_tlv_parse_content_hdr(item, &out[10], &out[11], &out[12],
                                          &out[14], &out[15], &out[16], &out[17], &out[18]);
    if (hdr_len < 0)
        return hdr_len;

    sub = item + hdr_len;
    sub_len = (u16)(tlen - hdr_len);
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 7, base, &out[19]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 12, base, &out[20]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 16, base, &out[21]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 4, base, &out[22]);
    if (ret < 0)
        return ret;

    if (is_alt == 3) {
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 17, 0, &out[25]));
        if (ret < 0)
            return ret;
    } else {
        ret = tlv_read_str(sub, sub_len, 17, base, &out[25]);
        if (ret < 0)
            return ret;
    }

    ret = sceNpCommerce2_3F6D413C(sub, sub_len, 24, 0, &out[23]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 68, base, &out[24]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_cat_sku(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 135 : 30;
    u16 tlen = 0;
    u32 *desc[20];
    int off, sku_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemset(out, 0, 144);
    off = sceNpCommerce2_E3455E27(body, rem, tag, 97, &tlen);
    if (off < 0)
        return off;

    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, NULL);

    sceKernelMemset(desc, 0, sizeof(desc));
    desc[0]  = (u32 *)(size_t)base;
    desc[1]  = (u32 *)(size_t)7;
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

    sub = item + sku_len;
    sub_len = tlen - (u32)sku_len;
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 8, base, &out[32]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 128, 0, &out[33]));
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 149, 0, &out[34]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_product_info(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 140 : 39;
    u16 tlen = 0;
    int off, hdr_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemset(out, 0, 136);
    off = sceNpCommerce2_E3455E27(body, rem, tag, 137, &tlen);
    if (off < 0)
        return off;

    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, NULL);
    hdr_len = store_tlv_parse_content_hdr(item, &out[10], &out[12], &out[14],
                                          &out[16], &out[11], &out[17], &out[18], &out[19]);
    if (hdr_len < 0)
        return hdr_len;

    sub = item + hdr_len;
    sub_len = (u16)(tlen - hdr_len);
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 7, base, &out[20]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 12, base, &out[21]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 16, base, &out[22]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 4, base, &out[23]);
    if (ret < 0)
        return ret;

    if (is_alt == 3) {
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 17, 0, &out[32]));
        if (ret < 0)
            return ret;
    } else {
        ret = tlv_read_str(sub, sub_len, 17, base, &out[32]);
        if (ret < 0)
            return ret;
    }

    ret = sceNpCommerce2_3F6D413C(sub, sub_len, 24, 0, &out[24]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 13, base, &out[25]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 35, 0, &out[26]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 36, 0, &out[27]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 37, 0, &out[28]);
    if (ret < 0)
        return ret;

    ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 68, base, &out[29]));
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 69, base, &out[30]));
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 119, base, &out[31]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_product_sku(const u32 *parent, u32 index, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 137 : 31;
    u16 tlen = 0;
    u32 *desc[20];
    int off, sku_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemset(out, 0, 168);
    off = store_tlv_find_nth(body, rem, tag, 101, index, &tlen);
    if (off < 0)
        return off;

    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, NULL);

    sceKernelMemset(desc, 0, sizeof(desc));
    desc[0]  = (u32 *)(size_t)base;
    desc[1]  = (u32 *)(size_t)7;
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

    sub = item + sku_len;
    sub_len = (u16)(tlen - sku_len);
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 8, base, &out[32]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 56, base, &out[33]);
    if (ret < 0)
        return ret;
    out[34] = (u32)(size_t)(base + 4);
    if (out[11] == 2) {
        ret = tlv_read_str(sub, sub_len, 18, base, &out[34]);
        if (ret < 0)
            return ret;
    }

    ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 60, base, &out[35]));
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(sub, sub_len, 61, 0, &out[36]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 62, 0, &out[37]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 128, 0, &out[38]));
    if (ret >= 0) ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 133, 0, &out[39]));
    if (ret >= 0) ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 149, 0, &out[40]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_link_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off, ret;
    const u8 *item, *sub, *base;

    off = store_tlv_find_nth(body, rem, 37, 0, index, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 4;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 56, parent[0], base, item, tlen, sub);
    out[10] = store_read_be32(item);

    ret = tlv_read_str(sub, tlen - 4, 63, base, &out[11]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(tlv_read_str(sub, tlen - 4, 64, base, &out[12]));
    if (ret >= 0)
        ret = tlv_allow_missing(tlv_read_str(sub, tlen - 4, 65, base, &out[13]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_entitlement(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off, ret;
    const u8 *item, *sub, *base;

    off = store_tlv_find_nth(body, rem, 128, 0, index, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 248;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 104, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = store_read_be32(item + 49);
    out[12] = (u32)(size_t)(item + 53);
    out[13] = (u32)(size_t)(item + 110);
    out[14] = store_read_be32(item + 159);
    out[15] = store_read_be32(item + 163);
    out[16] = store_read_be32(item + 167);
    out[17] = (u32)(size_t)(item + 171);
    out[18] = store_read_be32(item + 228);
    tlv_store_rtc(item + 232, &out[20]);
    out[22] = store_read_be32(item + 240);

    ret = tlv_read_str(sub, tlen - 248, 8, base, &out[23]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(tlv_read_str(sub, tlen - 248, 132, base, &out[24]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_subscr_sku(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off, sub_off, ret;
    const u8 *item, *sub, *base, *plan;

    off = store_tlv_find_nth(body, rem, 149, 0, index, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 248;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 120, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = store_read_be32(item + 49);
    out[12] = (u32)(size_t)(item + 110);
    out[13] = store_read_be32(item + 159);
    out[14] = store_read_be32(item + 163);
    out[15] = store_read_be32(item + 167);
    out[16] = (u32)(size_t)(item + 171);
    out[17] = store_read_be32(item + 228);
    tlv_store_rtc(item + 232, &out[18]);
    out[20] = store_read_be32(item + 240);
    out[21] = store_read_be32(item + 244);

    sub_off = sceNpCommerce2_E3455E27(sub, tlen - 248, 131, 20, NULL);
    if (sub_off < 0)
        return sub_off;
    plan = sub + sub_off;
    out[24] = store_read_be32(plan);
    {
        u64 val = store_read_be64(plan + 4);
        out[26] = (u32)val;
        out[27] = (u32)(val >> 32);
    }
    out[28] = store_read_be32(plan + 12);
    out[29] = store_read_be32(plan + 16);

    ret = tlv_allow_missing(tlv_read_str(sub, tlen - 248, 151, base, &out[22]));
    if (ret >= 0)
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, tlen - 248, 150, 0, &out[23]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_download_file(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off;
    const u8 *item;
    u64 file_size, dl_size;

    off = store_tlv_find_nth(body, rem, 156, 0, index, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    tlv_reset_hdr(out, 72, parent[0], (const u8 *)(size_t)parent[1], item, tlen, item + 77);
    out[10] = (u32)(size_t)item;
    out[11] = store_read_be32(item + 49);
    out[12] = store_read_be32(item + 53);
    out[13] = store_read_be32(item + 57);
    file_size = store_read_be64(item + 61);
    out[14] = (u32)file_size;
    out[15] = (u32)(file_size >> 32);
    dl_size = store_read_be64(item + 69);
    out[16] = (u32)dl_size;
    out[17] = (u32)(dl_size >> 32);
    return off;
}

int store_tlv_parse_video_sku(const u32 *parent, u32 index, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 144 : 133;
    u16 tlen = 0;
    int off, ret;
    const u8 *item, *sub, *base;
    u64 fsize;

    sceKernelMemset(out, 0, 88);
    off = store_tlv_find_nth(body, rem, tag, 0, index, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 77;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = store_read_be32(item + 49);
    out[12] = store_read_be32(item + 53);
    out[13] = store_read_be32(item + 57);
    out[14] = store_read_be32(item + 61);
    out[15] = store_read_be32(item + 65);
    fsize = store_read_be64(item + 69);
    out[16] = (u32)fsize;
    out[17] = (u32)(fsize >> 32);

    if (is_alt == 3) {
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, tlen - 77, 17, 0, &out[21]));
        if (ret < 0)
            return ret;
    } else {
        ret = tlv_read_str(sub, tlen - 77, 17, base, &out[21]);
        if (ret < 0)
            return ret;
    }

    ret = tlv_allow_missing(tlv_read_str(sub, tlen - 77, 60, base, &out[18]));
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(sub, tlen - 77, 61, 0, &out[19]);
    if (ret >= 0)
        ret = store_min_s32(sceNpCommerce2_3F6D413C(sub, tlen - 77, 62, 0, &out[20]), 0);
    return ret;
}

int store_tlv_parse_cart_entry(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off, rating_off;
    const u8 *item, *sub;

    off = store_tlv_find_nth_ex(body, rem, 46, 24, index, &tlen, (int)parent[6]);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 28;
    tlv_reset_hdr(out, 72, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);
    out[12] = store_read_be32(item + 8);
    out[13] = store_read_be32(item + 12);
    out[14] = store_read_be32(item + 16);
    out[15] = store_read_be32(item + 20);
    out[17] = store_read_be32(item + 24);

    rating_off = sceNpCommerce2_E3455E27(sub, tlen - 28, 24, 12, NULL);
    if (rating_off == STORE_ERR_TAG_NOT_FOUND) {
        out[16] = 0;
        return 0;
    }
    if (rating_off >= 0) {
        out[16] = 1;
        return 0;
    }
    return rating_off;
}

int store_tlv_parse_cart_sku_brief(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u8 hdr_copy[64];
    u32 *desc[16];
    u16 tlen = 0;
    int off, sku_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemcpy(hdr_copy, (const void *)(size_t)parent[1], 56);
    off = sceNpCommerce2_E3455E27(body, rem, 76, 101, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 104, parent[0], base, item, tlen, NULL);

    sceKernelMemset(desc, 0, sizeof(desc));
    desc[0]  = (u32 *)hdr_copy;
    desc[1]  = (u32 *)(size_t)2;
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

    sub = item + sku_len + 36;
    sub_len = (u16)(tlen - (u32)(sub - item));
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 8, base, &out[17]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 56, base, &out[18]);
    if (ret < 0)
        return ret;
    out[19] = (u32)(size_t)(base + 4);
    if (out[11] == 2) {
        ret = tlv_read_str(sub, sub_len, 18, base, &out[19]);
        if (ret < 0)
            return ret;
    }
    ret = tlv_read_str(sub, sub_len, 7, base, &out[20]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 16, base, &out[21]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 4, base, &out[22]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 128, 0, &out[23]));
    if (ret >= 0)
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 156, 0, &out[25]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_cart_sku_full(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = is_alt == 3 ? 138 : 77;
    u32 *desc[20];
    u16 tlen = 0;
    int off, sku_len, ret;
    const u8 *item, *sub, *base;
    u32 sub_len;

    sceKernelMemset(out, 0, 176);
    off = sceNpCommerce2_E3455E27(body, rem, tag, 157, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, NULL);

    sceKernelMemset(desc, 0, sizeof(desc));
    desc[0]  = (u32 *)(size_t)base;
    desc[1]  = (u32 *)(size_t)2;
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

    sub = item + sku_len;
    sub_len = (u16)(tlen - sku_len);
    out[5] = (u32)(size_t)sub;

    ret = tlv_read_str(sub, sub_len, 8, base, &out[30]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 56, base, &out[31]);
    if (ret < 0)
        return ret;
    out[32] = (u32)(size_t)(base + 4);
    if (out[11] == 2) {
        ret = tlv_read_str(sub, sub_len, 18, base, &out[32]);
        if (ret < 0)
            return ret;
    }
    ret = tlv_read_str(sub, sub_len, 7, base, &out[36]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 16, base, &out[37]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 4, base, &out[38]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 128, 0, &out[40]));
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 60, base, &out[33]));
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 61, 0, &out[34]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 62, 0, &out[35]);
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 68, base, &out[39]));
    if (ret < 0)
        return ret;
    if (is_alt == 2)
        out[43] = (sceNpCommerce2_E3455E27(sub, sub_len, 75, 8, NULL) >= 0) ? 3 : 1;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 133, 0, &out[41]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_audio_lang(const u32 *parent, u32 index, u32 *code_out, u32 *flags_out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 61, 0, index, &tlen);
    if (off < 0)
        return off;
    *code_out = (u32)(size_t)(body + off);
    if (flags_out)
        *flags_out = (tlen >= 7) ? store_read_be32(body + off + 3) : 0;
    return off;
}

int store_tlv_parse_card_info(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 48, 25, &tlen);
    const u8 *item, *sub;
    int ret, detail_off;

    if (off < 0)
        return off;
    item = data + off;
    sub = item + 21;
    tlv_reset_hdr(out, 68, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    ret = tlv_read_str(sub, tlen - 21, 15, base, &out[11]);
    if (ret < 0)
        return ret;

    detail_off = sceNpCommerce2_E3455E27(sub, tlen - 21, 99, 20, NULL);
    if (detail_off == STORE_ERR_TAG_NOT_FOUND)
        return 0;
    if (detail_off < 0)
        return detail_off;

    out[12] = store_read_be32(sub + detail_off);
    out[13] = store_read_be32(sub + detail_off + 4);
    out[14] = store_read_be32(sub + detail_off + 8);
    out[15] = store_read_be32(sub + detail_off + 12);
    return tlv_read_str(sub + detail_off + 16,
                        tlen - (u32)((sub + detail_off + 16) - item), 98, base, &out[16]);
}

int store_tlv_parse_drm_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = store_tlv_find_nth_paged(data, parent[4], 43, 125, index, &tlen, parent[6]);
    const u8 *item, *sub;
    u32 sub_len, drm_kind;
    u64 val64;
    int ret;

    if (off < 0)
        return off;
    item = data + off;
    sub = item + 121;
    sub_len = tlen - 121;
    tlv_reset_hdr(out, 184, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = store_read_be32(item + 49);
    out[12] = store_read_be32(item + 53);
    out[13] = store_read_be32(item + 57);
    val64 = store_read_be64(item + 61);
    out[14] = (u32)val64;
    out[15] = (u32)(val64 >> 32);
    tlv_store_rtc(item + 69, &out[16]);
    tlv_store_rtc(item + 77, &out[18]);
    out[20] = (store_read_be32(item + 85) != 0);
    tlv_store_rtc(item + 89, &out[22]);
    val64 = store_read_be64(item + 97);
    out[42] = (u32)val64;
    out[43] = (u32)(val64 >> 32);
    out[45] = store_read_be32(item + 105);

    ret = tlv_read_str(sub, sub_len, 9, base, &out[24]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 10, base, &out[25]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 4, base, &out[26]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 5, base, &out[27]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 16, base, &out[28]);
    if (ret >= 0) ret = tlv_read_str(sub, sub_len, 20, base, &out[29]);
    if (ret < 0)
        return ret;

    drm_kind = out[12];
    if (drm_kind == 1) {
        int sub_off = sceNpCommerce2_E3455E27(sub, sub_len, 152, 8, NULL);
        if (sub_off < 0)
            return sub_off;
        out[30] = store_read_be32(sub + sub_off);
        out[31] = store_read_be32(sub + sub_off + 4);
        val64 = store_read_be64(sub + sub_off + 8);
        out[32] = (u32)val64;
        out[33] = (u32)(val64 >> 32);
    } else if (drm_kind == 2 || drm_kind == 4) {
        u16 ext_tag = (drm_kind == 4) ? 141 : 86;
        int ext_off = sceNpCommerce2_E3455E27(sub, sub_len, ext_tag, 8, NULL);
        if (ext_off < 0)
            return ext_off;
        out[30] = store_read_be32(sub + ext_off);
        out[31] = store_read_be32(sub + ext_off + 4);
        sub = sub + ext_off + 16;
        sub_len = tlen - (u32)(sub - item);

        ret = tlv_read_str(sub, sub_len, 89, base, &out[35]);
        if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 85, base, &out[36]));
        if (ret >= 0) ret = tlv_read_str(sub, sub_len, 7, base, &out[37]);
        if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 68, base, &out[39]));
        if (ret < 0)
            return ret;
        out[38] = (sceNpCommerce2_E3455E27(sub, sub_len, 75, 8, NULL) >= 0) ? 3 : 1;
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 157, 0, &out[34]));
        if (ret < 0)
            return ret;
    }

    ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 56, base, &out[40]));
    if (ret >= 0)
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 128, 0, &out[44]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_drm_reward(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = (is_alt == 3) ? 141 : 86;
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(body, rem, tag, 8, &tlen);
    if (off < 0)
        return off;
    return store_tlv_parse_reward_info(parent[0], (const u8 *)(size_t)parent[1],
                                       body + off, tlen, body + off + 8, out, is_alt == 3);
}

int store_tlv_parse_drm_subitem(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    int ext_off = sceNpCommerce2_E3455E27(body, rem, 86, 8, NULL);
    const u8 *sub_scan, *item, *sub, *base;
    u16 tlen = 0;
    int off, ret;

    if (ext_off < 0)
        return ext_off;
    sub_scan = body + ext_off + 16;
    off = store_tlv_find_nth(sub_scan, parent[4] - (u32)(sub_scan - (const u8 *)(size_t)parent[3]),
                             157, 0, index, &tlen);
    if (off < 0)
        return off;

    item = sub_scan + off;
    sub = item + 110;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 72, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    out[11] = (u32)(size_t)(item + 49);
    out[12] = store_read_be32(item + 98);
    out[13] = store_read_be32(item + 102);
    out[14] = store_read_be32(item + 106);

    ret = tlv_read_str(sub, tlen - 110, 20, base, &out[15]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 110, 89, base, &out[16]);
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, tlen - 110, 159, base, &out[17]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_address_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    const u8 *data;
    int ret;

    if (off < 0)
        return off;
    data = buf + off;
    tlv_reset_hdr(out, 64, ver, buf, data, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = tlv_read_str(data, body_size, 79, buf, &out[10]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 80, buf, &out[11]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 81, buf, &out[12]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 82, buf, &out[13]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 54, buf, &out[14]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 84, buf, &out[15]);
    return store_min_s32(ret, 0);
}

int store_tlv_parse_edy_info(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 52, 29, &tlen);
    const u8 *item, *sub;

    if (off < 0)
        return off;
    item = data + off;
    sub = item + 25;
    tlv_reset_hdr(out, 56, parent[0], base, item, tlen, sub);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);
    out[12] = (u32)(size_t)(item + 8);
    return tlv_read_str(sub, tlen - 25, 19, base, &out[13]);
}

int store_tlv_parse_voucher_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 122, 93, index, &tlen);
    const u8 *item, *sub;
    u32 item_type;
    int ret;

    if (off < 0)
        return off;
    item = body + off;
    sub = item + 77;
    tlv_reset_hdr(out, 104, parent[0], base, item, tlen, sub);
    out[10] = (u32)(size_t)item;
    tlv_store_rtc(item + 57, &out[12]);
    out[14] = store_read_be32(item + 65);
    out[22] = store_read_be32(item + 69);
    item_type = store_read_be32(item + 73);
    out[23] = item_type;
    if (item_type != 3 && item_type != 5)
        out[14] = (u32)-1;

    ret = tlv_allow_missing(tlv_read_str(sub, tlen - 77, 4, base, &out[15]));
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 7, base, &out[16]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 8, base, &out[17]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 16, base, &out[18]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 12, base, &out[19]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 13, base, &out[20]);
    if (ret >= 0) ret = tlv_read_str(sub, tlen - 77, 119, base, &out[21]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, tlen - 77, 128, 0, &out[24]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_download_list(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off, ret;
    const u8 *item, *sub;

    sceKernelMemset(out, 0, 52);
    off = sceNpCommerce2_E3455E27(body, rem, 153, 72, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 72;
    tlv_reset_hdr(out, 40, parent[0], (const u8 *)(size_t)parent[1], item, tlen, sub);
    out[10] = store_read_be32(item + 28);
    out[11] = store_read_be32(item + 32);
    ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, tlen - 72, 156, 0, &out[12]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_video_info(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tag = (is_alt == 3) ? 143 : 127;
    u16 sku_tag = (is_alt == 3) ? 144 : 133;
    u16 tlen = 0;
    int off, ret;
    const u8 *item, *sub, *base;
    u32 sub_len, flags;
    u64 fsize;

    sceKernelMemset(out, 0, 160);
    off = sceNpCommerce2_E3455E27(body, rem, tag, 100, &tlen);
    if (off < 0)
        return off;
    item = body + off;
    sub = item + 72;
    sub_len = tlen - 72;
    base = (const u8 *)(size_t)parent[1];
    tlv_reset_hdr(out, 40, parent[0], base, item, tlen, sub);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);
    out[13] = store_read_be32(item + 12);
    out[14] = store_read_be32(item + 16);
    out[15] = store_read_be32(item + 20);
    flags = store_read_be32(item + 24);
    out[16] = flags;
    out[12] = (flags & 0xAU) != 0;
    out[17] = store_read_be32(item + 28);
    out[18] = store_read_be32(item + 32);
    tlv_store_rtc(item + 36, &out[20]);
    tlv_store_rtc(item + 44, &out[22]);
    tlv_store_rtc(item + 52, &out[24]);
    out[26] = store_read_be32(item + 60);
    fsize = store_read_be64(item + 64);
    out[28] = (u32)fsize;
    out[29] = (u32)(fsize >> 32);

    if (is_alt == 3) {
        ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, 17, 0, &out[39]));
        if (ret < 0)
            return ret;
    } else {
        ret = tlv_read_str(sub, sub_len, 17, base, &out[39]);
        if (ret < 0)
            return ret;
    }

    ret = sceNpCommerce2_3F6D413C(sub, sub_len, 35, 0, &out[30]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 36, 0, &out[31]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 37, 0, &out[32]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 68, base, &out[33]));
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 69, base, &out[34]));
    if (ret >= 0) ret = tlv_allow_missing(tlv_read_str(sub, sub_len, 60, base, &out[35]));
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 61, 0, &out[36]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(sub, sub_len, 62, 0, &out[37]);
    if (ret >= 0) ret = tlv_allow_missing(sceNpCommerce2_3F6D413C(sub, sub_len, sku_tag, 0, &out[38]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_account_info(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 94, 50, &tlen);
    const u8 *item, *sub;

    if (off < 0)
        return off;
    item = data + off;
    sub = item + 46;
    tlv_reset_hdr(out, 88, parent[0], base, item, tlen, sub);
    {
        u32 hi = store_read_be32(item);
        u32 lo = store_read_be32(item + 4);
        sceNpCommerce2_22CEC5E2((int)(size_t)&out[10], 17);
        (void)hi;
        (void)lo;
    }
    out[15] = (u32)(size_t)(item + 8);
    out[16] = (u32)(size_t)(item + 25);
    out[17] = (u32)(size_t)(item + 28);
    out[18] = store_read_be32(item + 34);
    out[20] = (store_read_be32(item + 38) != 0);
    out[21] = store_read_be32(item + 42);
    return tlv_read_str(sub, tlen - 46, 93, base, &out[19]);
}

int store_tlv_parse_reg_rules(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 95, 44, &tlen);
    const u8 *item;
    u32 idx, acct_type;

    if (off < 0)
        return off;
    item = data + off;
    tlv_reset_hdr(out, 104, parent[0], (const u8 *)(size_t)parent[1], item, tlen, item + 60);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);
    out[12] = store_read_be32(item + 8);
    acct_type = store_read_be32(item + 12);
    out[13] = acct_type;
    if (acct_type == 0)      out[14] = 0;
    else if (acct_type == 1) out[14] = 3;
    else if (acct_type == 4) out[14] = 2;
    else if (acct_type == 8) out[14] = 1;
    for (idx = 0; idx < 11; idx++)
        out[15 + idx] = store_read_be32(item + 16 + idx * 4);
    return off;
}

int store_tlv_parse_billing_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    const u8 *data;
    int ret, card_off;

    if (off < 0)
        return off;
    data = buf + off;
    tlv_reset_hdr(out, 76, ver, buf, data, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = sceNpCommerce2_3F6D413C(data, body_size, 55, 0, &out[10]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(data, body_size, 57, 0, &out[11]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 79, buf, &out[12]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 80, buf, &out[13]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 81, buf, &out[14]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 82, buf, &out[15]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 54, buf, &out[16]);
    if (ret >= 0) ret = tlv_read_str(data, body_size, 84, buf, &out[17]);
    if (ret < 0)
        return ret;

    card_off = sceNpCommerce2_E3455E27(data, body_size, 48, 25, NULL);
    if (card_off == STORE_ERR_TAG_NOT_FOUND) {
        out[18] = 0;
        return 0;
    }
    if (card_off >= 0) {
        out[18] = 1;
        return 0;
    }
    return card_off;
}

int store_tlv_parse_auth_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_size = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    const u8 *data;
    int ret;

    if (off < 0)
        return off;
    data = buf + off;
    tlv_reset_hdr(out, 56, ver, buf, data, body_size, NULL);
    out[2] = (u32)off + body_size;

    ret = tlv_read_str(data, body_size, 164, buf, &out[13]);
    if (ret >= 0)
        return 0;
    if (ret != STORE_ERR_TAG_NOT_FOUND)
        return ret;

    ret = sceNpCommerce2_3F6D413C(data, body_size, 22, 0, &out[10]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(data, body_size, 100, 0, &out[11]);
    if (ret >= 0) ret = sceNpCommerce2_3F6D413C(data, body_size, 88, 0, &out[12]);
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_console_act(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = store_tlv_find_nth(data, parent[4], 116, 12, index, &tlen);
    const u8 *item;
    u32 dev_type;

    if (off < 0)
        return off;
    item = data + off;
    tlv_reset_hdr(out, 64, parent[0], (const u8 *)(size_t)parent[1], item, tlen, item + 20);
    dev_type = store_read_be32(item);
    out[10] = (dev_type == 1 || dev_type == 2) ? dev_type : 0;
    out[11] = dev_type;
    out[12] = store_read_be32(item + 4);
    out[13] = (store_read_be32(item + 8) != 0);
    out[14] = store_read_be32(item + 12);
    out[15] = store_read_be32(item + 16);
    return off;
}

int store_tlv_parse_err_flag(const u8 *buf, u32 buf_size, u32 *flag_out)
{
    u32 ver = 0, body_size = 0, count = 0;
    int off = store_tlv_parse_root(buf, buf_size, &ver, &body_size, NULL);
    if (off < 0)
        return off;
    *flag_out = (sceNpCommerce2_3F6D413C(buf + off, body_size, 120, 0, &count) >= 0) ? 1 : 0;
    return 0;
}

int store_tlv_parse_err_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = store_tlv_find_nth(data, parent[4], 120, 8, index, &tlen);
    const u8 *item;

    if (off < 0)
        return off;
    item = data + off;
    tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1], item, tlen, item + 8);
    out[10] = store_read_be32(item);
    out[11] = store_read_be32(item + 4);
    return off;
}

int store_tlv_parse_sess_root(u32 *out, const u8 *buf, u32 buf_size)
{
    return store_tlv_parse_counted_root(out, buf, buf_size, 22);
}

int store_tlv_parse_eula_flag(const u32 *parent, u32 *out)
{
    const u8 *data = (const u8 *)(size_t)parent[3];
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(data, parent[4], 87, 4, &tlen);
    if (off < 0)
        return off;
    tlv_reset_hdr(out, 44, parent[0], (const u8 *)(size_t)parent[1], data + off, tlen, data + off + 4);
    out[10] = (store_read_be32(data + off) != 0);
    return off;
}

int store_tlv_parse_category_item(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(body, rem, 33, 81, &tlen);
    if (off < 0)
        return off;
    return store_tlv_parse_content_item(body + off, tlen, (const u8 *)(size_t)parent[1], parent[0], 0, out);
}

int store_tlv_parse_rating_first(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = sceNpCommerce2_E3455E27(body, rem, 24, 12, &tlen);
    if (off < 0)
        return off;
    return store_tlv_parse_rating_system(body + off, tlen, (const u8 *)(size_t)parent[1], parent[0], out);
}

int store_tlv_parse_rating_nth(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 24, 12, index, &tlen);
    if (off < 0)
        return off;
    return store_tlv_parse_rating_system(body + off, tlen, (const u8 *)(size_t)parent[1], parent[0], out);
}

int store_tlv_parse_credit_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    const u8 *base = (const u8 *)(size_t)parent[1];
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 36, 0, index, &tlen);
    const u8 *item;
    int ret;

    if (off < 0)
        return off;
    item = body + off;
    tlv_reset_hdr(out, 48, parent[0], base, item, tlen, item);
    ret = tlv_read_str(item, tlen, 66, base, &out[10]);
    if (ret < 0)
        return ret;
    ret = tlv_allow_missing(tlv_read_str(item, tlen, 67, base, &out[11]));
    return ret < 0 ? ret : 0;
}

int store_tlv_parse_subscr_ent(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)(size_t)parent[5];
    u32 rem = parent[4] - (u32)(body - (const u8 *)(size_t)parent[3]);
    u16 tlen = 0;
    int off = store_tlv_find_nth(body, rem, 150, 0, index, &tlen);
    if (off < 0)
        return off;
    tlv_reset_hdr(out, 48, parent[0], (const u8 *)(size_t)parent[1], body + off, tlen, NULL);
    out[10] = (u32)(size_t)(body + off);
    out[11] = (u32)(size_t)(body + off + 57);
    return off;
}
