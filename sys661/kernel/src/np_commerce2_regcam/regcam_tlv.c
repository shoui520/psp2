#include "regcam_internal.h"

#define REGCAM_ERR_BAD_RESP       ((int)0x80550F14)
#define REGCAM_ERR_OUT_OF_RANGE   ((int)0x80550F06)

static void regcam_tlv_reset_header(u32 *out, u32 total_struct_size, u32 ver,
                                    const u8 *base, const u8 *data, u32 data_size)
{
    sceKernelMemset(out, 0, total_struct_size);
    out[0] = ver;
    out[1] = (u32)base;
    out[3] = (u32)data;
    out[4] = data_size;
}

static int regcam_tlv_read_str(const u8 *scan, u32 scan_len, u32 tag,
                               const u8 *empty_base, u32 *out_ptr, int optional)
{
    u16 tag_len = 0;
    int off;

    *out_ptr = (u32)(empty_base + 4);
    off = sceNpCommerce2_E3455E27(scan, scan_len, tag, 0, &tag_len);
    if (off >= 0 && tag_len != 0)
        *out_ptr = (u32)(scan + off);
    if (off < 0) {
        if (optional && off == REGCAM_ERR_TAG_NOT_FOUND)
            return 0;
        return off;
    }
    return 0;
}

static void regcam_tlv_store_rtc(u32 *dst_pair, const u8 *be64_ptr)
{
    u64 ms = regcam_read_be64(be64_ptr);
    if (ms != 0) {
        u64 tick = regcam_unix_ms_to_rtc(ms);
        dst_pair[0] = (u32)tick;
        dst_pair[1] = (u32)(tick >> 32);
    } else {
        dst_pair[0] = 0;
        dst_pair[1] = 0;
    }
}

int regcam_tlv_parse_root(const u8 *buf, u32 buf_size, u32 *ver_out, u32 *body_size_out, u32 *extra_out)
{
    u32 hdr[14];
    u32 body_len;

    if (buf_size < 60U)
        return REGCAM_ERR_BAD_RESP;
    sceKernelMemcpy(hdr, buf, sizeof(hdr));
    body_len = hdr[2];
    if (buf_size < body_len + 56U)
        return REGCAM_ERR_BAD_RESP;
    *body_size_out = body_len - 4U;
    if (ver_out != NULL)
        *ver_out = hdr[0];
    if (extra_out != NULL)
        *extra_out = hdr[3];
    return 60;
}

int regcam_tlv_find_nth(const u8 *buf, u32 buf_size, u16 tag, u16 min_len, u32 index, u16 *len_out)
{
    u16 total_len = 0;
    u16 item_len = 0;
    u32 count = 0;
    const u8 *group_start;
    const u8 *cur;
    u32 idx;
    int off;

    off = ((int (*)(const u8 *, u32, u32, u16 *, u32 *))sceNpCommerce2_3F6D413C)(
              buf, buf_size, tag, &total_len, &count);
    if (off < 0)
        return off;
    if (index >= count)
        return REGCAM_ERR_OUT_OF_RANGE;

    group_start = buf + off;
    cur = group_start;
    for (idx = 0; idx <= index; idx++) {
        if (idx > 0)
            cur += item_len;
        off = sceNpCommerce2_E3455E27(cur, (u32)(total_len - (u32)(cur - group_start)), tag, min_len, &item_len);
        if (off < 0)
            return off;
        cur += off;
    }
    *len_out = item_len;
    return (int)(cur - buf);
}

int regcam_tlv_find_nth_ex(const u8 *buf, u32 buf_size, u16 tag, u16 min_len,
                           u32 index, u16 *len_out, int extra_mode)
{
    u32 range_info[4];
    u16 item_len = 0;
    const u8 *cur;
    u32 idx;
    int off;

    off = ((int (*)(const u8 *, u32, u32, u32, u32 *, int))sceNpCommerce2_69FEA1C8)(
              buf, buf_size, tag, 0, range_info, extra_mode);
    if (off < 0)
        return off;
    if (index >= range_info[1])
        return REGCAM_ERR_OUT_OF_RANGE;

    cur = buf + off;
    for (idx = 0; idx <= index; idx++) {
        if (idx > 0)
            cur += item_len;
        off = sceNpCommerce2_E3455E27(cur, buf_size - (u32)(cur - buf), tag, min_len, &item_len);
        if (off < 0)
            return off;
        cur += off;
    }
    *len_out = item_len;
    return (int)(cur - buf);
}

static int regcam_tlv_clamp_count(const u32 *root, u16 tag, u16 min_len, u32 *range_info)
{
    u16 item_len = 0;
    u32 idx;
    int ret = 0;

    for (idx = 0; idx < range_info[1]; idx++) {
        ret = regcam_tlv_find_nth_ex((const u8 *)root[3], root[4], tag, min_len, idx, &item_len, (int)root[6]);
        if (ret < 0) {
            if (idx != 0) {
                range_info[1] = idx;
                return 0;
            }
            return ret;
        }
    }
    return ret;
}

static int regcam_tlv_parse_expiry(const u32 *parent, u32 *has_out, u32 *y_out, u32 *m_out, u32 *d_out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    u16 len = 0;
    int off;

    off = sceNpCommerce2_E3455E27(body, remain, 74, 12, &len);
    if (off == REGCAM_ERR_TAG_NOT_FOUND) {
        *has_out = 0;
        return 0;
    }
    if (off < 0)
        return off;
    *y_out = regcam_read_be32(body + off);
    *m_out = regcam_read_be32(body + off + 4);
    *d_out = regcam_read_be32(body + off + 8);
    *has_out = 1;
    return 0;
}

int regcam_tlv_parse_err_flag(const u8 *buf, u32 buf_size, u32 *flag_out)
{
    u32 ver = 0, body_len = 0, cnt = 0;
    int off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    *flag_out = (sceNpCommerce2_3F6D413C(buf + off, body_len, 120, 0, &cnt) >= 0) ? 1U : 0U;
    return 0;
}

int regcam_tlv_parse_simple_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_len = 0;
    int off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 40, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    return 0;
}

int regcam_tlv_parse_counted_root(u32 *out, const u8 *buf, u32 buf_size, u32 tag)
{
    u32 ver = 0, body_len = 0;
    int off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 44, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    return regcam_min_s32(sceNpCommerce2_3F6D413C(buf + off, body_len, tag, 0, &out[10]), 0);
}

int regcam_tlv_parse_paged_root(u32 *out, const u8 *buf, u32 buf_size, u16 tag, u16 min_len)
{
    u32 ver = 0, body_len = 0, extra = 0;
    int off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, &extra);
    int ret;

    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 52, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    out[6] = extra;
    ret = ((int (*)(const u8 *, u32, u32, u32, u32 *, u32))sceNpCommerce2_69FEA1C8)(
              buf + off, body_len, tag, 0, &out[10], extra);
    if (ret < 0)
        return ret;
    return regcam_min_s32(regcam_tlv_clamp_count(out, tag, min_len, &out[10]), 0);
}

int regcam_tlv_parse_sess_root(u32 *out, const u8 *buf, u32 buf_size)
{
    return regcam_tlv_parse_counted_root(out, buf, buf_size, 22);
}

int regcam_tlv_parse_auth_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_len = 0;
    u16 url_len = 0;
    int off, ret;

    off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 56, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    out[13] = (u32)(buf + 4);
    ret = sceNpCommerce2_E3455E27(buf + off, body_len, 164, 0, &url_len);
    if (ret >= 0 && url_len != 0)
        out[13] = (u32)(buf + off + ret);
    if (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND)
        return ret;
    if (ret != REGCAM_ERR_TAG_NOT_FOUND)
        return 0;

    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 22, 0, &out[10]);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 100, 0, &out[11]);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 88, 0, &out[12]);
    return (ret < 0) ? ret : 0;
}

int regcam_tlv_parse_bind_root(u32 *out, const u8 *buf, u32 buf_size)
{
    u32 ver = 0, body_len = 0;
    u16 url_len = 0;
    int off, ret;

    off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 48, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    out[11] = (u32)(buf + 4);
    ret = sceNpCommerce2_E3455E27(buf + off, body_len, 164, 0, &url_len);
    if (ret >= 0 && url_len != 0)
        out[11] = (u32)(buf + off + ret);
    if (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND)
        return ret;
    if (ret != REGCAM_ERR_TAG_NOT_FOUND)
        return 0;
    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 147, 0, &out[10]);
    return (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND) ? ret : 0;
}

int regcam_tlv_parse_address_root(u32 *out, const u8 *buf, u32 buf_size)
{
    static const u32 addr_tags[6] = { 79, 80, 81, 82, 54, 84 };
    u32 ver = 0, body_len = 0;
    int off, idx, ret;

    off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 64, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;
    for (idx = 0; idx < 6; idx++) {
        ret = regcam_tlv_read_str(buf + off, body_len, addr_tags[idx], buf, &out[10 + idx], 0);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int regcam_tlv_parse_billing_root(u32 *out, const u8 *buf, u32 buf_size)
{
    static const u32 addr_tags[6] = { 79, 80, 81, 82, 54, 84 };
    u32 ver = 0, body_len = 0;
    int off, idx, ret;

    off = regcam_tlv_parse_root(buf, buf_size, &ver, &body_len, NULL);
    if (off < 0)
        return off;
    regcam_tlv_reset_header(out, 76, ver, buf, buf + off, body_len);
    out[2] = (u32)off + body_len;

    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 55, 0, &out[10]);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(buf + off, body_len, 57, 0, &out[11]);
    if (ret < 0) return ret;
    for (idx = 0; idx < 6; idx++) {
        ret = regcam_tlv_read_str(buf + off, body_len, addr_tags[idx], buf, &out[12 + idx], 0);
        if (ret < 0) return ret;
    }
    ret = sceNpCommerce2_E3455E27(buf + off, body_len, 48, 25, NULL);
    if (ret == REGCAM_ERR_TAG_NOT_FOUND) {
        out[18] = 0;
        return 0;
    }
    if (ret < 0)
        return ret;
    out[18] = 1;
    return 0;
}

int regcam_tlv_parse_account_info(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(src, parent[4], 94, 50, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 88, parent[0], (const u8 *)parent[1], payload, plen);

    ((int (*)(void *, u32, u32, u32, int))sceNpCommerce2_22CEC5E2)(
        &out[10], 17, regcam_read_be32(payload + 4), regcam_read_be32(payload), 0);
    out[15] = (u32)(payload + 8);
    out[16] = (u32)(payload + 25);
    out[17] = (u32)(payload + 28);
    out[18] = regcam_read_be32(payload + 34);
    out[20] = (regcam_read_be32(payload + 38) != 0) ? 1U : 0U;
    out[21] = regcam_read_be32(payload + 42);
    tail = payload + 46;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 46U), 93, (const u8 *)parent[1], &out[19], 0);
    return ret;
}

int regcam_tlv_parse_country_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(src, parent[4], 91, 15, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 60, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = regcam_read_be32(payload + 3);
    out[13] = regcam_read_be32(payload + 7);
    out[5]  = (u32)(payload + 11);
    ret = sceNpCommerce2_3F6D413C(payload + 11, (u32)(plen - 11U), 92, 0, &out[12]);
    if (ret < 0) return ret;
    return sceNpCommerce2_3F6D413C(payload + 11, (u32)(plen - 11U), 155, 0, &out[14]);
}

int regcam_tlv_parse_reg_rules(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    u16 plen = 0;
    u32 mode;
    int off, idx;

    off = sceNpCommerce2_E3455E27(src, parent[4], 95, 44, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 104, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 0);
    out[11] = regcam_read_be32(payload + 4);
    out[12] = regcam_read_be32(payload + 8);
    mode = regcam_read_be32(payload + 12);
    out[13] = mode;
    if (mode == 0) out[14] = 0;
    else if (mode == 1) out[14] = 3;
    else if (mode == 4) out[14] = 2;
    else if (mode == 8) out[14] = 1;
    for (idx = 0; idx < 11; idx++)
        out[15 + idx] = regcam_read_be32(payload + 16 + idx * 4);
    out[5] = (u32)(payload + 60);
    return off;
}

int regcam_tlv_parse_name_info(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(src, parent[4], 103, 78, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 60, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload);
    out[11] = (u32)(payload + 4);
    out[12] = (u32)(payload + 37);
    tail = payload + 70;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 70U), 101, (const u8 *)parent[1], &out[13], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 70U), 102, (const u8 *)parent[1], &out[14], 0);
}

int regcam_tlv_parse_opt_in(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    u16 plen = 0;
    int off;

    off = sceNpCommerce2_E3455E27(src, parent[4], 104, 8, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 48, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload);
    out[11] = regcam_read_be32(payload + 4);
    out[5]  = (u32)(payload + 8);
    return off;
}

int regcam_tlv_parse_wal_opt(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    u16 plen = 0;
    int off;

    off = sceNpCommerce2_E3455E27(src, parent[4], 105, 8, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 48, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (regcam_read_be32(payload) != 0) ? 2U : 1U;
    out[11] = (regcam_read_be32(payload + 4) != 0) ? 2U : 1U;
    out[5]  = (u32)(payload + 8);
    return off;
}

int regcam_tlv_parse_card_info(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(src, parent[4], 48, 25, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 68, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    tail = payload + 21;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 21U), 15, (const u8 *)parent[1], &out[11], 0);
    if (ret < 0)
        return ret;
    off = sceNpCommerce2_E3455E27(tail, (u32)(plen - 21U), 99, 20, NULL);
    if (off == REGCAM_ERR_TAG_NOT_FOUND)
        return 0;
    if (off < 0)
        return off;
    {
        const u8 *cc = tail + off;
        out[12] = regcam_read_be32(cc + 0);
        out[13] = regcam_read_be32(cc + 4);
        out[14] = regcam_read_be32(cc + 8);
        out[15] = regcam_read_be32(cc + 12);
        return regcam_tlv_read_str(cc + 16, (u32)(plen - (u32)((cc + 16) - payload)), 98,
                                   (const u8 *)parent[1], &out[16], 0);
    }
}

int regcam_tlv_parse_sec_q_item(const u32 *parent, const u8 *payload, u16 plen, u32 *out)
{
    const u8 *tail = payload + 4;
    regcam_tlv_reset_header(out, 48, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload);
    out[5]  = (u32)tail;
    return regcam_tlv_read_str(tail, (u32)(plen - 4U), 96, (const u8 *)parent[1], &out[11], 0);
}

int regcam_tlv_parse_sec_q_nth(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    u16 plen = 0;
    int off = regcam_tlv_find_nth(src, parent[4], 97, 8, index, &plen);
    if (off < 0)
        return off;
    return regcam_tlv_parse_sec_q_item(parent, src + off, plen, out);
}

int regcam_tlv_parse_sec_q_first(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    u16 plen = 0;
    int off = sceNpCommerce2_E3455E27(src, parent[4], 97, 8, &plen);
    if (off < 0)
        return off;
    return regcam_tlv_parse_sec_q_item(parent, src + off, plen, out);
}

int regcam_tlv_parse_province_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(src, parent[4], 55, 73, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 52, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    tail = payload + 65;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 65U), 54, (const u8 *)parent[1], &out[11], 0);
    if (ret < 0) return ret;
    return sceNpCommerce2_3F6D413C(tail, (u32)(plen - 65U), 59, 0, &out[12]);
}

int regcam_tlv_parse_city_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off;

    off = regcam_tlv_find_nth(body, remain, 59, 69, index, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 48, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    tail = payload + 65;
    out[5] = (u32)tail;
    return regcam_tlv_read_str(tail, (u32)(plen - 65U), 58, (const u8 *)parent[1], &out[11], 0);
}

int regcam_tlv_parse_pay_method(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(src, parent[4], 57, 25, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 52, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    tail = payload + 17;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 17U), 15, (const u8 *)parent[1], &out[11], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 17U), 16, (const u8 *)parent[1], &out[12], 0);
}

int regcam_tlv_parse_bind_result(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    u16 plen = 0;
    int off;

    off = regcam_tlv_find_nth(src, parent[4], 147, 4, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 44, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload);
    out[5]  = (u32)(payload + 4);
    return off;
}

int regcam_tlv_parse_currency(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    u16 decimals = 0;
    int off, ret;

    off = regcam_tlv_find_nth(src, parent[4], 22, 42, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 84, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = (u32)(payload + 4);
    sceKernelMemcpy(&decimals, payload + 8, 2);
    *(u16 *)((u8 *)out + 48) = regcam_bswap16(decimals);
    out[13] = regcam_read_be32(payload + 10);
    out[14] = regcam_read_be32(payload + 14);
    out[16] = regcam_read_be32(payload + 18);
    out[17] = regcam_read_be32(payload + 22);
    out[18] = regcam_read_be32(payload + 26);
    tail = payload + 30;
    out[5] = (u32)tail;
    ret = sceNpCommerce2_3F6D413C(tail, (u32)(plen - 30U), 21, 0, &out[15]);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 30U), 125, (const u8 *)parent[1], &out[19], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 30U), 126, (const u8 *)parent[1], &out[20], 0);
}

int regcam_tlv_parse_sub_account(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0, lim_len = 0;
    int off;

    off = regcam_tlv_find_nth(src, parent[4], 100, 24, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 68, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 0);
    out[11] = (regcam_read_be32(payload + 4) != 0) ? 1U : 0U;
    out[12] = (regcam_read_be32(payload + 8) != 0) ? 1U : 0U;
    out[13] = (regcam_read_be32(payload + 12) != 0) ? 1U : 0U;
    out[14] = (regcam_read_be32(payload + 16) != 0) ? 1U : 0U;
    tail = payload + 20;
    out[5] = (u32)tail;
    off = sceNpCommerce2_E3455E27(tail, (u32)(plen - 20U), 88, 8, &lim_len);
    if (off < 0)
        return off;
    out[15] = regcam_read_be32(tail + off);
    return regcam_tlv_read_str(tail, (u32)(plen - 20U), 93, (const u8 *)parent[1], &out[16], 0);
}

int regcam_tlv_parse_spend_limit(const u32 *parent, u32 index, u32 *limit_out)
{
    const u8 *src = (const u8 *)parent[3];
    u16 plen = 0;
    int off = regcam_tlv_find_nth(src, parent[4], 88, 8, index, &plen);
    if (off < 0)
        return off;
    *limit_out = regcam_read_be32(src + off);
    return off;
}

int regcam_tlv_parse_tran_hist_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(src, parent[4], 107, 48, index, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 88, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 4);
    out[11] = regcam_read_be32(payload + 0);
    out[12] = regcam_read_be32(payload + 12);
    out[13] = regcam_read_be32(payload + 8);
    regcam_tlv_store_rtc(&out[14], payload + 16);
    out[16] = regcam_read_be32(payload + 24);
    out[17] = regcam_read_be32(payload + 28);
    out[18] = regcam_read_be32(payload + 32);
    out[19] = regcam_read_be32(payload + 36);
    tail = payload + 40;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 40U), 106, (const u8 *)parent[1], &out[20], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 40U), 93, (const u8 *)parent[1], &out[21], 0);
}

int regcam_tlv_parse_tran_detail(const u32 *parent, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(src, parent[4], 112, 52, &plen);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 96, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 4);
    out[11] = regcam_read_be32(payload + 0);
    out[12] = regcam_read_be32(payload + 12);
    out[13] = regcam_read_be32(payload + 8);
    regcam_tlv_store_rtc(&out[14], payload + 16);
    out[16] = regcam_read_be32(payload + 24);
    out[17] = regcam_read_be32(payload + 28);
    out[18] = regcam_read_be32(payload + 32);
    out[19] = regcam_read_be32(payload + 36);
    tail = payload + 40;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 40U), 106, (const u8 *)parent[1], &out[20], 0);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 40U), 93, (const u8 *)parent[1], &out[21], 0);
    if (ret < 0) return ret;
    return sceNpCommerce2_3F6D413C(tail, (u32)(plen - 40U), 111, 0, &out[22]);
}

int regcam_tlv_parse_tran_order(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(body, remain, 111, 28, index, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 72, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 4);
    out[11] = regcam_read_be32(payload + 0);
    regcam_tlv_store_rtc(&out[12], payload + 8);
    out[14] = regcam_read_be32(payload + 16);
    out[15] = regcam_read_be32(payload + 20);
    tail = payload + 24;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 24U), 110, (const u8 *)parent[1], &out[16], 0);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(tail, (u32)(plen - 24U), 109, 0, &out[17]);
    return (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND) ? ret : 0;
}

int regcam_tlv_parse_tran_item(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(body, remain, 109, 93, index, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 96, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[12] = regcam_read_be32(payload + 61);
    out[13] = regcam_read_be32(payload + 57);
    out[14] = regcam_read_be32(payload + 69);
    out[15] = regcam_read_be32(payload + 65);
    out[16] = (u32)(-(s32)regcam_read_be32(payload + 73));
    out[17] = (u32)(-(s32)regcam_read_be32(payload + 77));
    out[18] = (u32)(-(s32)regcam_read_be32(payload + 81));
    out[20] = regcam_read_be32(payload + 85);
    tail = payload + 89;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 89U), 108, (const u8 *)parent[1], &out[19], 0);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 89U), 4, (const u8 *)parent[1], &out[21], 0);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 89U), 7, (const u8 *)parent[1], &out[22], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 89U), 8, (const u8 *)parent[1], &out[23], 0);
}

int regcam_tlv_parse_price_info(const u32 *parent, u32 *out, int is_alt)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    u32 tag = (is_alt == 3) ? 142U : 124U;
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off;

    off = sceNpCommerce2_E3455E27(body, remain, tag, 12, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 56, parent[0], (const u8 *)parent[1], payload, plen);
    out[13] = regcam_read_be32(payload + 0);
    out[12] = regcam_read_be32(payload + 4);
    out[10] = regcam_read_be32(payload + 8);
    tail = payload + 12;
    out[5] = (u32)tail;
    return regcam_tlv_read_str(tail, (u32)(plen - 12U), 68, (const u8 *)parent[1], &out[11], 1);
}

int regcam_tlv_parse_service_info(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth_ex(src, parent[4], 113, 49, index, &plen, (int)parent[6]);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 80, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    regcam_tlv_store_rtc(&out[12], payload + 33);
    out[14] = regcam_read_be32(payload + 41);
    out[15] = regcam_read_be32(payload + 45);
    tail = payload + 49;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 49U), 4, (const u8 *)parent[1], &out[16], 0);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 49U), 5, (const u8 *)parent[1], &out[17], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 49U), 16, (const u8 *)parent[1], &out[18], 0);
}

int regcam_tlv_parse_entitlement(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth_ex(src, parent[4], 114, 81, index, &plen, (int)parent[6]);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 88, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = (u32)(payload + 57);
    out[12] = (regcam_read_be32(payload + 106) != 0) ? 1U : 0U;
    regcam_tlv_store_rtc(&out[14], payload + 110);
    regcam_tlv_store_rtc(&out[16], payload + 118);
    out[18] = regcam_read_be32(payload + 126);
    out[19] = regcam_read_be32(payload + 130);
    tail = payload + 134;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 134U), 121, (const u8 *)parent[1], &out[20], 0);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(tail, (u32)(plen - 134U), 128, 0, &out[21]);
    return (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND) ? ret : 0;
}

int regcam_tlv_parse_subscription(const u32 *parent, u32 index, u32 *out)
{
    const u8 *src = (const u8 *)parent[3];
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth_ex(src, parent[4], 115, 85, index, &plen, (int)parent[6]);
    if (off < 0)
        return off;
    payload = src + off;
    regcam_tlv_reset_header(out, 112, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = (u32)(payload + 57);
    out[12] = regcam_read_be32(payload + 114);
    regcam_tlv_store_rtc(&out[14], payload + 118);
    out[16] = regcam_read_be32(payload + 126);
    out[17] = (regcam_read_be32(payload + 130) != 0) ? 1U : 0U;
    regcam_tlv_store_rtc(&out[18], payload + 134);
    regcam_tlv_store_rtc(&out[20], payload + 142);
    tail = payload + 150;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 150U), 7, (const u8 *)parent[1], &out[22], 0);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 150U), 130, (const u8 *)parent[1], &out[23], 1);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 150U), 8, (const u8 *)parent[1], &out[24], 0);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(tail, (u32)(plen - 150U), 123, 0, &out[25]);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_3F6D413C(tail, (u32)(plen - 150U), 128, 0, &out[26]);
    return (ret < 0 && ret != REGCAM_ERR_TAG_NOT_FOUND) ? ret : 0;
}

int regcam_tlv_parse_subscr_plan(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off;

    off = regcam_tlv_find_nth(body, remain, 123, 73, index, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 60, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = regcam_read_be32(payload + 57);
    out[12] = regcam_read_be32(payload + 61);
    out[13] = regcam_read_be32(payload + 65);
    tail = payload + 69;
    out[5] = (u32)tail;
    return regcam_tlv_read_str(tail, (u32)(plen - 69U), 8, (const u8 *)parent[1], &out[14], 0);
}

int regcam_tlv_parse_reward(const u32 *parent, u32 index, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = regcam_tlv_find_nth(body, remain, 128, 0, index, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 104, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = (u32)payload;
    out[11] = regcam_read_be32(payload + 49);
    out[12] = (u32)(payload + 53);
    out[13] = (u32)(payload + 110);
    out[14] = regcam_read_be32(payload + 159);
    out[15] = regcam_read_be32(payload + 163);
    out[16] = regcam_read_be32(payload + 167);
    out[17] = (u32)(payload + 171);
    out[18] = regcam_read_be32(payload + 228);
    regcam_tlv_store_rtc(&out[20], payload + 232);
    out[22] = regcam_read_be32(payload + 240);
    tail = payload + 248;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 248U), 8, (const u8 *)parent[1], &out[23], 0);
    if (ret < 0) return ret;
    return regcam_tlv_read_str(tail, (u32)(plen - 248U), 132, (const u8 *)parent[1], &out[24], 1);
}

int regcam_tlv_parse_eula_info(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(body, remain, 75, 8, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 76, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 0);
    out[11] = regcam_read_be32(payload + 4);
    tail = payload + 8;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 8U), 90, (const u8 *)parent[1], &out[12], 1);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 8U), 70, (const u8 *)parent[1], &out[13], 1);
    if (ret < 0) return ret;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 8U), 71, (const u8 *)parent[1], &out[14], 1);
    if (ret < 0) return ret;
    return regcam_min_s32(regcam_tlv_parse_expiry(out, &out[15], &out[16], &out[17], &out[18]), 0);
}

int regcam_tlv_parse_eula_ext(const u32 *parent, u32 *out)
{
    const u8 *body = (const u8 *)parent[5];
    u32 remain = parent[4] - (u32)(body - (const u8 *)parent[3]);
    const u8 *payload;
    const u8 *tail;
    u16 plen = 0;
    int off, ret;

    off = sceNpCommerce2_E3455E27(body, remain, 145, 8, &plen);
    if (off < 0)
        return off;
    payload = body + off;
    regcam_tlv_reset_header(out, 68, parent[0], (const u8 *)parent[1], payload, plen);
    out[10] = regcam_read_be32(payload + 0);
    out[11] = regcam_read_be32(payload + 4);
    tail = payload + 8;
    out[5] = (u32)tail;
    ret = regcam_tlv_read_str(tail, (u32)(plen - 8U), 146, (const u8 *)parent[1], &out[12], 1);
    if (ret < 0) return ret;
    return regcam_min_s32(regcam_tlv_parse_expiry(out, &out[13], &out[14], &out[15], &out[16]), 0);
}
