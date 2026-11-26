#include "mp4msv_internal.h"

/* 0x0060dc */
MP4_NOINLINE int32_t mp4MfraFindPrevMoof(Mp4MfraBox *mfra, uint32_t track_id,
                                         uint32_t cur_time, uint64_t cur_offset,
                                         uint32_t *out_found, uint64_t *out_prev_offset)
{
    (void)cur_offset;

    uint32_t tfra_count = 0;
    int32_t status = mp4DynArrayCount(mfra->tfra_list, &tfra_count);
    if (status != MP4_OK) {
        return status;
    }

    for (uint32_t idx = 0; idx < tfra_count; idx++) {
        Mp4TfraBox *tfra = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &tfra);
        if (status != MP4_OK) {
            return status;
        }
        uint32_t tid = 0;
        status = mp4TfraGetTrackId(tfra, &tid);
        if (status != MP4_OK) {
            return status;
        }
        if (track_id < tid) {
            *out_found = 0;
            return MP4_OK;
        }
        if (track_id == tid) {
            uint32_t entry_count = 0;
            status = mp4TfraGetEntryCount(tfra, &entry_count);
            if (status != MP4_OK) {
                return status;
            }
            if (entry_count == 0) {
                *out_found = 0;
                return MP4_OK;
            }
            Mp4TfraEntry cur;
            status = mp4TfraGetEntry(tfra, 1, &cur);
            if (status != MP4_OK) {
                return status;
            }
            if (cur.time_lo >= cur_time) {
                *out_found = 0;
                return MP4_OK;
            }
            for (uint32_t eidx = 2; eidx <= entry_count; eidx++) {
                uint64_t prev_off = cur.moof_offset;
                status = mp4TfraGetEntry(tfra, eidx, &cur);
                if (status != MP4_OK) {
                    return status;
                }
                if ((cur.time_lo == cur_time && cur.time_hi == 0) || cur_time < cur.time_lo) {
                    *out_found = 1;
                    *out_prev_offset = prev_off;
                    return MP4_OK;
                }
            }
            *out_found = 1;
            *out_prev_offset = cur.moof_offset;
            return MP4_OK;
        }
    }

    *out_found = 0;
    return MP4_OK;
}

/* 0x0062c0 */
MP4_NOINLINE int32_t mp4MfraLookupLast(Mp4MfraBox *mfra, uint32_t track_id,
                                       uint32_t *out_found, Mp4TfraEntry *out_entry)
{
    uint32_t tfra_count = 0;
    int32_t status = mp4DynArrayCount(mfra->tfra_list, &tfra_count);
    if (status != MP4_OK) {
        return status;
    }

    for (uint32_t idx = 0; idx < tfra_count; idx++) {
        Mp4TfraBox *tfra = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &tfra);
        if (status != MP4_OK) {
            return status;
        }
        uint32_t tid = 0;
        status = mp4TfraGetTrackId(tfra, &tid);
        if (status != MP4_OK) {
            return status;
        }
        if (track_id < tid) {
            *out_found = 0;
            return MP4_OK;
        }
        if (track_id == tid) {
            uint32_t entry_count = 0;
            status = mp4TfraGetEntryCount(tfra, &entry_count);
            if (status != MP4_OK) {
                return status;
            }
            if (entry_count == 0) {
                *out_found = 0;
                return MP4_OK;
            }
            Mp4TfraEntry last;
            status = mp4TfraGetEntry(tfra, entry_count, &last);
            if (status != MP4_OK) {
                return status;
            }
            *out_found = 1;
            *out_entry = last;
            return MP4_OK;
        }
    }

    *out_found = 0;
    return MP4_OK;
}

/* 0x006410 */
MP4_NOINLINE int32_t mp4MfraRemoveOther(Mp4MfraBox *mfra, uint32_t seq_id)
{
    uint32_t count = 0;
    int32_t status = mp4DynArrayCount(mfra->other_list, &count);
    if (status != MP4_OK) {
        return status;
    }

    Mp4MfraOther *found = NULL;
    uint32_t idx = 0;
    while (idx < count) {
        status = mp4DynArrayGet(mfra->other_list, idx, &found);
        if (status != MP4_OK) {
            return status;
        }
        uint32_t item_id = 0;
        status = mp4MfraOtherGetId(found, &item_id);
        if (status != MP4_OK) {
            return status;
        }
        if (item_id == seq_id) {
            break;
        }
        idx++;
    }

    if (idx == count) {
        return MP4_OK;
    }

    uint32_t complete = 0;
    status = mp4MfraOtherCheckComplete(found, &complete);
    if (status != MP4_OK) {
        return status;
    }
    if (complete == 0) {
        mfra->incomplete_flag = 1;
    }
    mp4MfraOtherDestroy(found);
    return mp4DynArrayErase(mfra->other_list, idx);
}

/* 0x006504 */
MP4_NOINLINE int32_t mp4MfraParse(Mp4MfraBox *mfra, void *fh, uint32_t body_size,
                                  uint32_t *out_err)
{
    *out_err = 0;
    int64_t start_pos = mp4CbTell(fh);
    if (start_pos == -1) {
        return MP4_ERR_IO;
    }
    uint64_t end_pos = (uint64_t)start_pos + (uint64_t)body_size;

    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = 0;
        if (mp4CbReadBe(&box_size, 4, fh) != 4) {
            return MP4_ERR_IO;
        }
        if (box_size < 8U) {
            *out_err |= MP4_PARSE_FLAG_SIZE_MISMATCH;
            if (mp4SeekAbs(fh, end_pos) == 0) {
                return MP4_ERR_IO;
            }
            return MP4_ERR_CORRUPT;
        }

        uint32_t box_tag = 0;
        if (mp4CbReadBe(&box_tag, 4, fh) != 4) {
            return MP4_ERR_IO;
        }
        if (rem < box_size) {
            *out_err |= MP4_PARSE_FLAG_SIZE_MISMATCH;
            if (mp4SeekAbs(fh, end_pos) == 0) {
                return MP4_ERR_IO;
            }
            return MP4_ERR_CORRUPT;
        }

        uint32_t child_err = 0;
        int32_t status = MP4_OK;
        if (box_tag == MP4_TAG_MFRO) {
            status = mp4MfraParseMfro(mfra, fh, box_size - 8U, &child_err);
            *out_err |= child_err;
            if (status != MP4_OK) {
                return status;
            }
        } else if (box_tag == MP4_TAG_TFRA) {
            status = mp4MfraParseTfra(mfra, fh, box_size - 8U, &child_err);
            *out_err |= child_err;
            if (status != MP4_OK) {
                return status;
            }
        } else {
            *out_err |= MP4_PARSE_FLAG_UNREC_BOX;
            if (mp4CbSeek(fh, (int64_t)(uint64_t)box_size, 1) == 0) {
                return MP4_ERR_IO;
            }
        }
        rem -= box_size;
    }

    uint32_t tfra_count = 0;
    int32_t status = mp4DynArrayCount(mfra->tfra_list, &tfra_count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < tfra_count; idx++) {
        Mp4TfraBox *tfra = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &tfra);
        if (status != MP4_OK) {
            return status;
        }
        status = mp4TfraValidate(tfra);
        if (status != MP4_OK) {
            return status;
        }
    }
    *out_err |= 0x00080000U;
    return MP4_OK;
}

/* 0x00675c */
MP4_NOINLINE int32_t mp4MfraParseTfra(Mp4MfraBox *mfra, void *fh, uint32_t body_size,
                                      uint32_t *out_err)
{
    *out_err = 0;
    int64_t start_pos = mp4CbTell(fh);
    if ((uint32_t)start_pos == 0xFFFFFFFFU && ((uint64_t)start_pos >> 32) == 0U) {
        return MP4_ERR_IO;
    }
    uint64_t end_pos = (uint64_t)start_pos + (uint64_t)body_size;

    Mp4TfraBox *tfra = NULL;
    int32_t status = mp4TfraCreate(&tfra);
    if (status == MP4_ERR_NOMEM) {
        if (mp4SeekAbs(fh, end_pos) == 0) {
            return MP4_ERR_IO;
        }
        return MP4_ERR_NOMEM;
    }
    if (status != MP4_OK) {
        return status;
    }

    status = mp4TfraParseBody(tfra, fh, body_size, out_err);
    uint32_t track_id = 0;
    uint32_t list_count = 0;
    if (status == MP4_OK) {
        status = mp4TfraGetTrackId(tfra, &track_id);
    }
    if (status == MP4_OK) {
        status = mp4DynArrayCount(mfra->tfra_list, &list_count);
    }
    if (status != MP4_OK) {
        mp4TfraDestroy(tfra);
        return status;
    }

    for (uint32_t idx = 0; idx < list_count; idx++) {
        Mp4TfraBox *existing = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &existing);
        if (status != MP4_OK) {
            mp4TfraDestroy(tfra);
            return status;
        }
        uint32_t exist_id = 0;
        status = mp4TfraGetTrackId(existing, &exist_id);
        if (status != MP4_OK) {
            mp4TfraDestroy(tfra);
            return status;
        }
        if (track_id < exist_id) {
            status = mp4DynArraySet(mfra->tfra_list, idx, &tfra);
            if (status == MP4_ERR_NOMEM) {
                if (mp4SeekAbs(fh, end_pos) == 0) {
                    return MP4_ERR_IO;
                }
            }
            if (status == MP4_OK) {
                return MP4_OK;
            }
            mp4TfraDestroy(tfra);
            return status;
        }
        if (track_id == exist_id) {
            status = mp4TfraMerge(existing, tfra);
            if (status == MP4_ERR_NOMEM) {
                if (mp4SeekAbs(fh, end_pos) == 0) {
                    return MP4_ERR_IO;
                }
            }
            if (status != MP4_OK) {
                mp4TfraDestroy(tfra);
                return status;
            }
            mp4TfraDestroy(tfra);
            return MP4_OK;
        }
    }

    status = mp4DynArrayPush(mfra->tfra_list, &tfra);
    if (status == MP4_ERR_NOMEM) {
        if (mp4SeekAbs(fh, end_pos) == 0) {
            return MP4_ERR_IO;
        }
        return MP4_ERR_NOMEM;
    }
    return status;
}

/* 0x0069b4 */
MP4_NOINLINE int32_t mp4MfraParseMfro(Mp4MfraBox *mfra, void *fh, uint32_t body_size,
                                      uint32_t *out_err)
{
    *out_err = 0;
    int64_t start_pos = mp4CbTell(fh);
    if ((uint32_t)start_pos == 0xFFFFFFFFU && ((uint64_t)start_pos >> 32) == 0U) {
        return MP4_ERR_IO;
    }
    uint64_t end_pos = (uint64_t)start_pos + (uint64_t)body_size;

    uint32_t existing_size;
    int32_t status = mp4MfroGetSize(mfra->mfro, &existing_size);
    if (status != MP4_OK) {
        return status;
    }

    if (existing_size != 0) {
        *out_err |= MP4_PARSE_FLAG_DUPLICATE_BOX;
        mp4SeekAbs(fh, end_pos);
        return MP4_OK;
    }

    return mp4MfroParseBody(mfra->mfro, fh, body_size, out_err);
}

/* 0x006ab4 */
MP4_NOINLINE int32_t mp4TfraCreate(Mp4TfraBox **out_tfra)
{
    Mp4TfraBox *tfra = (Mp4TfraBox *)mp4CbMalloc(sizeof(Mp4TfraBox));
    if (tfra == NULL) {
        return MP4_ERR_NOMEM;
    }

    tfra->tag = MP4_TAG_TFRA;
    tfra->size = 24;
    tfra->version = 1;
    tfra->flags[0] = 0;
    tfra->flags[1] = 0;
    tfra->flags[2] = 0;
    tfra->track_id = 0;
    tfra->traf_len_size = 0;
    tfra->trun_len_size = 0;
    tfra->sample_len_size = 0;
    tfra->entry_count = 0;

    Mp4DynArray *entries;
    int32_t status = mp4DynArrayCreate(sizeof(Mp4TfraEntry), 100, &entries);
    if (status != MP4_OK) {
        mp4CbFree(tfra);
        return status;
    }

    *out_tfra = tfra;
    tfra->reserved_words[0] = 0;
    tfra->reserved_words[1] = 0;
    tfra->reserved_words[2] = 0;
    tfra->reserved_words[3] = 0;
    tfra->entries = entries;
    return MP4_OK;
}

/* 0x006b84 */
MP4_NOINLINE int32_t mp4TfraDestroy(Mp4TfraBox *tfra)
{
    if (tfra->entries != NULL) {
        mp4DynArrayDestroy(tfra->entries);
    }
    if (tfra != NULL) {
        mp4CbFree(tfra);
    }
    return MP4_OK;
}

/* 0x006bd0 */
MP4_NOINLINE int32_t mp4TfraGetTrackId(Mp4TfraBox *tfra, uint32_t *out_id)
{
    *out_id = tfra->track_id;
    return MP4_OK;
}

/* 0x006be0 */
MP4_NOINLINE int32_t mp4TfraGetEntryCount(Mp4TfraBox *tfra, uint32_t *out_count)
{
    return mp4DynArrayCount(tfra->entries, out_count);
}

/* 0x006bfc */
MP4_NOINLINE int32_t mp4TfraGetEntry(Mp4TfraBox *tfra, uint32_t index_one_based,
                                     Mp4TfraEntry *out_entry)
{
    return mp4DynArrayGet(tfra->entries, index_one_based - 1U, out_entry);
}

/* 0x006c1c */
MP4_NOINLINE int32_t mp4TfraMerge(Mp4TfraBox *dst, Mp4TfraBox *src)
{
    uint32_t count = 0;
    int32_t status = mp4DynArrayCount(src->entries, &count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < count; idx++) {
        Mp4TfraEntry item;
        status = mp4DynArrayGet(src->entries, idx, &item);
        if (status != MP4_OK) {
            return status;
        }
        status = mp4DynArrayPush(dst->entries, &item);
        if (status != MP4_OK) {
            return status;
        }
    }

    dst->entry_count += src->entry_count;
    if (dst->traf_len_size < src->traf_len_size) {
        dst->traf_len_size = src->traf_len_size;
    }
    if (dst->trun_len_size < src->trun_len_size) {
        dst->trun_len_size = src->trun_len_size;
    }
    if (dst->sample_len_size < src->sample_len_size) {
        dst->sample_len_size = src->sample_len_size;
    }
    return MP4_OK;
}

/* 0x006d18 */
MP4_NOINLINE int32_t mp4TfraValidate(Mp4TfraBox *tfra)
{
    uint32_t count = 0;
    int32_t status = mp4DynArrayCount(tfra->entries, &count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < count; idx++) {
        Mp4TfraEntry item;
        status = mp4DynArrayGet(tfra->entries, idx, &item);
        if (status != MP4_OK) {
            return status;
        }
    }
    return MP4_OK;
}

/* 0x006d9c */
MP4_NOINLINE int32_t mp4TfraParseBody(Mp4TfraBox *tfra, void *fh, uint32_t body_size,
                                      uint32_t *out_err)
{
    *out_err = 0;
    if (body_size < 4U) {
        *out_err = MP4_PARSE_FLAG_BAD_BOX_SIZE;
        if (body_size != 0) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)body_size, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_ERR_CORRUPT;
    }

    uint8_t raw[32];
    if (mp4CbRead(raw, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    tfra->version = raw[0];
    tfra->flags[0] = raw[1];
    tfra->flags[1] = raw[2];
    tfra->flags[2] = raw[3];

    uint32_t rem = body_size - 4U;
    if (tfra->version != 1U) {
        return MP4_ERR_CORRUPT;
    }
    if (tfra->flags[0] != 0 || tfra->flags[1] != 0 || tfra->flags[2] != 0) {
        *out_err |= MP4_PARSE_FLAG_UNREC_BOX;
        if (rem != 0) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_OK;
    }

    if (rem < 12U) {
        *out_err |= MP4_PARSE_FLAG_BAD_BOX_SIZE;
        if (rem != 0) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_ERR_CORRUPT;
    }

    if (mp4CbReadBe(&tfra->track_id, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    rem -= 4U;
    if (mp4CbRead(raw, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    rem -= 4U;

    uint8_t traf_sz = (uint8_t)((raw[3] >> 4) & 3U);
    uint8_t trun_sz = (uint8_t)((raw[3] >> 2) & 3U);
    uint8_t smp_sz = (uint8_t)(raw[3] & 3U);
    tfra->traf_len_size = traf_sz;
    tfra->trun_len_size = trun_sz;
    tfra->sample_len_size = smp_sz;
    uint32_t entry_bytes = (uint32_t)traf_sz + (uint32_t)trun_sz + (uint32_t)smp_sz + 19U;

    if (mp4CbReadBe(&tfra->entry_count, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    rem -= 4U;

    if (rem < entry_bytes * tfra->entry_count) {
        *out_err |= MP4_PARSE_FLAG_COUNT_CLAMPED;
        if (rem != 0) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_ERR_CORRUPT;
    }

    for (uint32_t idx = 0; idx < tfra->entry_count; idx++) {
        if ((uint32_t)mp4CbRead(raw, entry_bytes, fh) != entry_bytes) {
            return MP4_ERR_IO;
        }
        Mp4TfraEntry item;
        item.time_hi = ((uint32_t)raw[0] << 24) + ((uint32_t)raw[1] << 16) +
                       ((uint32_t)raw[2] << 8) + (uint32_t)raw[3];
        item.time_lo = ((uint32_t)raw[4] << 24) + ((uint32_t)raw[5] << 16) +
                       ((uint32_t)raw[6] << 8) + (uint32_t)raw[7];
        uint32_t offset_hi = ((uint32_t)raw[8] << 24) + ((uint32_t)raw[9] << 16) +
                          ((uint32_t)raw[10] << 8) + (uint32_t)raw[11];
        uint32_t offset_lo = ((uint32_t)raw[12] << 24) + ((uint32_t)raw[13] << 16) +
                          ((uint32_t)raw[14] << 8) + (uint32_t)raw[15];
        item.moof_offset = ((uint64_t)offset_hi << 32) | (uint64_t)offset_lo;
        item.reserved = 0;

        uint8_t *cur = &raw[16];
        cur = mp4ReadVarUint(tfra, tfra->traf_len_size, cur, &item.traf_number);
        cur = mp4ReadVarUint(tfra, tfra->trun_len_size, cur, &item.trun_number);
        mp4ReadVarUint(tfra, tfra->sample_len_size, cur, &item.sample_number);

        rem -= entry_bytes;
        int32_t status = mp4DynArrayPush(tfra->entries, &item);
        if (status != MP4_OK) {
            return status;
        }
    }

    if (rem != 0) {
        tfra->size -= rem;
        if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
            return MP4_ERR_IO;
        }
    }
    return MP4_OK;
}

/* 0x00712c */
MP4_NOINLINE uint8_t *mp4ReadVarUint(Mp4TfraBox *tfra, uint8_t size_minus_one,
                                     uint8_t *src, uint32_t *out_val)
{
    (void)tfra;

    switch (size_minus_one) {
        case 0:
            *out_val = (uint32_t)src[0];
            return src + 1;
        case 1:
            *out_val = ((uint32_t)src[0] << 8) + (uint32_t)src[1];
            return src + 2;
        case 2:
            *out_val = ((uint32_t)src[0] << 16) + ((uint32_t)src[1] << 8) + (uint32_t)src[2];
            return src + 3;
        case 3:
            *out_val = ((uint32_t)src[0] << 24) + ((uint32_t)src[1] << 16) +
                       ((uint32_t)src[2] << 8) + (uint32_t)src[3];
            return src + 4;
        default:
            return src;
    }
}

/* 0x0071e0 */
MP4_NOINLINE int32_t mp4MfroCreate(Mp4MfroBox **out_mfro)
{
    Mp4MfroBox *mfro = (Mp4MfroBox *)mp4CbMalloc(sizeof(Mp4MfroBox));
    if (mfro == NULL) {
        return MP4_ERR_NOMEM;
    }
    mfro->version = 0;
    mfro->flags[0] = 0;
    mfro->flags[1] = 0;
    mfro->flags[2] = 0;
    mfro->size = 16;
    mfro->tag = MP4_TAG_MFRO;
    *out_mfro = mfro;
    mfro->parent_size = 0;
    return MP4_OK;
}

/* 0x007244 */
MP4_NOINLINE int32_t mp4MfroDestroy(Mp4MfroBox *mfro)
{
    if (mfro != NULL) {
        mp4CbFree(mfro);
    }
    return MP4_OK;
}

/* 0x007264 */
MP4_NOINLINE int32_t mp4MfroGetSize(Mp4MfroBox *mfro, uint32_t *out_size)
{
    *out_size = mfro->parent_size;
    return MP4_OK;
}

/* 0x007274 */
MP4_NOINLINE int32_t mp4MfroParseBody(Mp4MfroBox *mfro, void *fh, uint32_t body_size,
                                      uint32_t *out_err)
{
    *out_err = 0;
    if (body_size < 4U) {
        *out_err = MP4_PARSE_FLAG_BAD_BOX_SIZE;
        if (body_size != 0) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)body_size, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_ERR_CORRUPT;
    }

    uint8_t ver_flags[4];
    if (mp4CbRead(ver_flags, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    mfro->version = ver_flags[0];
    mfro->flags[0] = ver_flags[1];
    mfro->flags[1] = ver_flags[2];
    mfro->flags[2] = ver_flags[3];

    uint32_t rem = body_size - 4U;
    if (mfro->version != 0 || mfro->flags[0] != 0 || mfro->flags[1] != 0 || mfro->flags[2] != 0) {
        *out_err |= MP4_PARSE_FLAG_UNREC_BOX;
        if (rem != 0U) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_OK;
    }

    if (rem < 4U) {
        *out_err |= MP4_PARSE_FLAG_BAD_BOX_SIZE;
        if (rem != 0U) {
            if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
                return MP4_ERR_IO;
            }
        }
        return MP4_ERR_CORRUPT;
    }

    if (mp4CbReadBe(&mfro->parent_size, 4, fh) != 4) {
        return MP4_ERR_IO;
    }
    rem -= 4U;
    if (rem != 0U) {
        mfro->size -= rem;
        if (mp4CbSeek(fh, (int64_t)(uint64_t)rem, 1) != 0) {
            return MP4_ERR_IO;
        }
    }
    return MP4_OK;
}

/* 0x0073ec */
MP4_NOINLINE int32_t mp4MfraOtherDestroy(Mp4MfraOther *item)
{
    if (item->entries != NULL) {
        mp4DynArrayDestroy(item->entries);
    }
    if (item != NULL) {
        mp4CbFree(item);
    }
    return MP4_OK;
}

/* 0x007438 */
MP4_NOINLINE int32_t mp4MfraOtherGetId(Mp4MfraOther *item, uint32_t *out_id)
{
    *out_id = item->seq_id;
    return MP4_OK;
}

/* 0x007448 */
MP4_NOINLINE int32_t mp4MfraOtherCheckComplete(Mp4MfraOther *item, uint32_t *out_complete)
{
    uint32_t count = 0;
    int32_t status = mp4DynArrayCount(item->entries, &count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < count; idx++) {
        uint32_t rec[8];
        status = mp4DynArrayGet(item->entries, idx, rec);
        if (status != MP4_OK) {
            return status;
        }
        if (rec[3] == 0 || rec[4] == 0) {
            *out_complete = 0;
            return MP4_OK;
        }
    }
    *out_complete = 1;
    return MP4_OK;
}

/* 0x007500 */
MP4_NOINLINE int32_t mp4InitFragmentedTracks(Mp4Movie *movie, uint32_t *sample_counts,
                                             Mp4IoStream *stream)
{
    if (movie == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (movie->mvex == NULL) {
        if (mp4BuildDefaultMvex(movie) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    }

    Mp4Stbl **stbl_list = (Mp4Stbl **)mp4MemCalloc(sizeof(Mp4Stbl *) * movie->track_count);
    if (stbl_list == NULL) {
        return MP4_ERR_NOMEM;
    }

    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        Mp4Stbl *orig_stbl = &trk->mdia.minf.stbl;
        Mp4Stbl *clone_stbl = (Mp4Stbl *)mp4MemCalloc(sizeof(Mp4Stbl));
        if (clone_stbl == NULL) {
            for (uint32_t prev = 0; prev < idx; prev++) {
                Mp4Stbl *cur = stbl_list[prev];
                cur->stsd.head = NULL;
                void *box_ptr = cur;
                mp4BoxFree(&box_ptr, MP4_TAG_STBL);
                mp4MemFree(cur);
            }
            mp4MemFree(stbl_list);
            return MP4_ERR_NOMEM;
        }

        clone_stbl->size = 8;
        clone_stbl->tag = MP4_TAG_STBL;
        clone_stbl->track = mp4MovieGetTrackAt(movie, idx);

        uint32_t actual_samples = 0;
        mp4StblGetSampleCount(orig_stbl, &actual_samples, 0);
        if (actual_samples < sample_counts[idx]) {
            sample_counts[idx] = actual_samples;
        }

        mp4StblInitDynamic(clone_stbl, stream);
        clone_stbl->chunk_open = 1;

        int32_t copy_res = mp4CopyStblSamples(orig_stbl, clone_stbl, sample_counts[idx]);
        if (copy_res != MP4_OK) {
            for (uint32_t prev = 0; prev < idx; prev++) {
                Mp4Stbl *cur = stbl_list[prev];
                cur->stsd.head = NULL;
                void *box_ptr = cur;
                mp4BoxFree(&box_ptr, MP4_TAG_STBL);
                mp4MemFree(cur);
            }
            clone_stbl->stsd.head = NULL;
            void *box_ptr = clone_stbl;
            mp4BoxFree(&box_ptr, MP4_TAG_STBL);
            mp4MemFree(clone_stbl);
            mp4MemFree(stbl_list);
            return copy_res;
        }
        stbl_list[idx] = clone_stbl;
    }

    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        trk->mdia.minf.active_stbl = stbl_list[idx];
    }
    mp4MemFree(stbl_list);
    return MP4_OK;
}

/* 0x007778 */
MP4_NOINLINE int32_t mp4BuildDefaultMvex(Mp4Movie *movie)
{
    if (movie == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    Mp4MvexBox *mvex = (Mp4MvexBox *)mp4MemCalloc(sizeof(Mp4MvexBox));
    movie->mvex = mvex;
    if (mvex == NULL) {
        return MP4_ERR_NOMEM;
    }
    mvex->size = 8;
    mvex->tag = MP4_TAG_MVEX;

    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        Mp4Stbl *stbl = &trk->mdia.minf.stbl;
        Mp4TrexBox *trex = (Mp4TrexBox *)mp4MemCalloc(sizeof(Mp4TrexBox));
        if (trex == NULL) {
            void *box_ptr = movie->mvex;
            mp4BoxFree(&box_ptr, MP4_TAG_MVEX);
            movie->mvex = NULL;
            return MP4_ERR_NOMEM;
        }
        trex->size = 36;
        trex->tag = MP4_TAG_TREX;
        trex->track_id = trk->tkhd.track_id;
        trex->def_sample_desc = 1;

        uint32_t first_dur = 0;
        if (mp4StblGetSampleDur(stbl, 0, &first_dur, 0) != MP4_OK) {
            trex->def_sample_dur = 0;
        } else {
            trex->def_sample_dur = first_dur;
        }

        uint32_t first_size = 0;
        mp4StblLookupSampleSize(stbl, 0, &first_size);
        trex->def_sample_size = first_size;

        uint32_t loaded_start = 0;
        uint32_t loaded_cnt = 0;
        if (mp4StblGetFragRange(stbl, &loaded_start, &loaded_cnt) != MP4_OK) {
            trex->def_sample_flags = 0;
        } else {
            trex->def_sample_flags = (loaded_start != 0) ? 0x10000U : 0U;
        }

        if (movie->mvex->trex_list[idx] != NULL) {
            void *old_ptr = movie->mvex->trex_list[idx];
            mp4BoxFree(&old_ptr, MP4_TAG_TREX);
            movie->mvex->trex_list[idx] = NULL;
        }
        movie->mvex->trex_list[idx] = trex;
        movie->mvex->trex_count++;
    }
    return MP4_OK;
}

/* 0x007940 */
MP4_NOINLINE int32_t mp4CopyStblSamples(Mp4Stbl *src, Mp4Stbl *dst, uint32_t sample_count)
{
    dst->stsd = src->stsd;
    if (sample_count == 0) {
        return MP4_OK;
    }

    uint32_t sample_idx = 0;
    while (sample_idx < sample_count) {
        Mp4SampleInfo smp;
        if (mp4StblGetSampleInfo(src, sample_idx, 1, &smp, 0) != MP4_OK) {
            return MP4_ERR_EVICT_FAIL;
        }
        uint32_t chunk_samples = smp.chunk_rem_samples;
        for (uint32_t sub = 0; sub < chunk_samples; sub++) {
            if (sub != 0) {
                if (mp4StblGetSampleInfo(src, sample_idx, 0, &smp, 1) != MP4_OK) {
                    return MP4_ERR_EVICT_FAIL;
                }
            }
            sample_idx++;
            int32_t status = mp4StblAddRunAndCount(dst, 1, smp.duration, smp.offset, smp.size,
                                                   smp.is_sync != 1U ? 1 : 0,
                                                   smp.ctts_offset, smp.sample_desc);
            if (status != MP4_OK) {
                return status;
            }
        }
        mp4StblEndChunk(dst, 0);
    }
    return MP4_OK;
}

/* 0x007ab4 */
MP4_NOINLINE int32_t mp4MvexParse(Mp4MvexBox *mvex, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MVEX, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == 0x6D656864U) { /* 'mehd' */
            if (box_size < 12U) {
                mp4SetParseFlag(0x6D656864U, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(0x6D656864U, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (mvex->mehd.size != 0) {
                    mp4SetParseFlag(0x6D656864U, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    mvex->mehd.size = box_size;
                    mvex->mehd.tag = 0x6D656864U;
                    mp4MehdReadBody(&mvex->mehd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_TREX) {
            if (box_size < 32U) {
                mp4SetParseFlag(MP4_TAG_TREX, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                Mp4TrexBox *trex = (Mp4TrexBox *)mp4MemCalloc(sizeof(Mp4TrexBox));
                if (trex == NULL) {
                    return MP4_ERR_PARSE_FATAL;
                }
                trex->size = box_size;
                trex->tag = MP4_TAG_TREX;
                mp4ReadBytes(&trex->version, 1, stream);
                mp4ReadBytes(trex->flags, 3, stream);
                trex->track_id = mp4ReadU32(stream);
                trex->def_sample_desc = mp4ReadU32(stream);
                trex->def_sample_dur = mp4ReadU32(stream);
                trex->def_sample_size = mp4ReadU32(stream);
                trex->def_sample_flags = mp4ReadU32(stream);
                if (mvex->trex_count < 127U) {
                    mvex->trex_list[mvex->trex_count] = trex;
                    mvex->trex_count++;
                } else {
                    mp4SetParseFlag(MP4_TAG_TREX, MP4_PARSE_FLAG_TRACK_LIMIT, stream);
                    void *box_ptr = trex;
                    mp4BoxFree(&box_ptr, MP4_TAG_TREX);
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_MVEX, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_MVEX, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x007df0 */
MP4_NOINLINE int32_t mp4MoofReadNext(Mp4Movie *movie, Mp4IoStream *stream, uint64_t offset,
                                     Mp4MoofBox **out_moof, uint64_t *out_next_offset)
{
    Mp4MoofBox *parsed_moof = NULL;
    int64_t file_size = mp4CbGetSize(stream->file_handle);
    int32_t moof_seen = 0;

    mp4SeekAbs(stream->file_handle, offset);
    *out_next_offset = (uint64_t)file_size;
    if ((uint64_t)file_size < offset) {
        return MP4_ERR_IO;
    }
    stream->error_flags = 0;

    uint64_t rem = (uint64_t)file_size - offset;
    for (;;) {
        if (rem < 8U) {
            break;
        }
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            break;
        }
        if (rem < (uint64_t)box_size) {
            box_size = (uint32_t)rem;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if ((int32_t)stream->error_flags < 0) {
            mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            break;
        }

        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;
        if (box_tag == MP4_TAG_MOOF) {
            moof_seen++;
            if (moof_seen < 2) {
                if (movie->moof_count >= 2U) {
                    Mp4MoofBox *prev = NULL;
                    Mp4MoofBox *cur = movie->moof_head;
                    while (cur != NULL) {
                        if (cur->evictable != 0) {
                            Mp4MoofBox *next_m = cur->next;
                            if (cur == movie->moof_head) {
                                movie->moof_head = next_m;
                            }
                            cur->next = NULL;
                            if (prev != NULL) {
                                prev->next = next_m;
                            }
                            mp4MoofListFree(cur);
                            movie->moof_count--;
                            cur = next_m;
                        } else {
                            prev = cur;
                            cur = cur->next;
                        }
                    }
                    while (movie->moof_count >= 2U) {
                        Mp4MoofBox *head = movie->moof_head;
                        if (head != NULL) {
                            Mp4MoofBox *next_m = head->next;
                            head->next = NULL;
                            mp4MoofListFree(head);
                            movie->moof_head = next_m;
                            movie->moof_count--;
                        }
                    }
                }

                void *alloc_ptr = NULL;
                if (mp4AllocUniqueBox(&alloc_ptr, MP4_TAG_MOOF, sizeof(Mp4MoofBox),
                                      box_end, stream) != MP4_ERR_PARSE_FATAL) {
                    parsed_moof = (Mp4MoofBox *)alloc_ptr;
                    parsed_moof->next_moof_offset = *out_next_offset;
                    parsed_moof->file_offset = box_end - (uint64_t)box_size;
                    *out_moof = parsed_moof;
                    parsed_moof->size = box_size;
                    parsed_moof->tag = MP4_TAG_MOOF;

                    if (mp4MoofParse(parsed_moof, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                        for (uint32_t ti = 0; ti < parsed_moof->traf_count; ti++) {
                            mp4TrafFree(parsed_moof->traf_list[ti]);
                            parsed_moof->traf_list[ti] = NULL;
                        }
                        parsed_moof->traf_count = 0;
                    }
                    mp4MovieAppendMoof(movie, parsed_moof);

                    Mp4TrafBox *prev_traf = NULL;
                    for (uint32_t ti = 0; ti < parsed_moof->traf_count; ti++) {
                        mp4TrafResolveDataOffsets(prev_traf, parsed_moof, parsed_moof->traf_list[ti]);
                        prev_traf = parsed_moof->traf_list[ti];
                    }
                }
            } else {
                uint64_t next_pos = box_end - (uint64_t)box_size;
                parsed_moof->next_moof_offset = next_pos;
                *out_next_offset = next_pos;
            }
        } else if (box_tag == MP4_TAG_MDAT || box_tag == MP4_TAG_FREE ||
                   box_tag == MP4_TAG_FTYP || box_tag == MP4_TAG_MFRA ||
                   box_tag == MP4_TAG_MOOV) {
            mp4SeekAbs(stream->file_handle, box_end);
        } else {
            return MP4_ERR_EOF_FRAG;
        }

        if (moof_seen >= 2 || (stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            break;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= (uint64_t)box_size;
    }

    uint32_t err = stream->error_flags;
    if ((int32_t)err < 0) {
        return MP4_ERR_IO;
    }
    if ((err & MP4_PARSE_FLAG_ALLOC_FAIL) != 0) {
        return MP4_ERR_NOMEM;
    }
    if ((err & 0x2000003BU) != 0) {
        return MP4_ERR_CORRUPT;
    }
    if (moof_seen == 0) {
        return MP4_ERR_EOF_FRAG;
    }
    return MP4_OK;
}

/* 0x008330 */
MP4_NOINLINE int32_t mp4MoofParse(Mp4MoofBox *moof, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MOOF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MFHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_MFHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_MFHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_MFHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (moof->mfhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_MFHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    moof->mfhd.size = box_size;
                    moof->mfhd.tag = MP4_TAG_MFHD;
                    mp4MfhdReadBody(&moof->mfhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_TRAF) {
            Mp4TrafBox *traf = (Mp4TrafBox *)mp4MemCalloc(sizeof(Mp4TrafBox));
            if (traf == NULL) {
                return MP4_ERR_PARSE_FATAL;
            }
            traf->size = box_size;
            traf->tag = MP4_TAG_TRAF;
            if (mp4TrafParse(traf, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                mp4TrafFree(traf);
                return MP4_ERR_PARSE_FATAL;
            }
            if (moof->traf_count < 127U) {
                moof->traf_list[moof->traf_count] = traf;
                moof->traf_count++;
            } else {
                mp4SetParseFlag(MP4_TAG_TRAF, MP4_PARSE_FLAG_TRACK_LIMIT, stream);
                void *box_ptr = traf;
                mp4BoxFree(&box_ptr, MP4_TAG_TRAF);
            }
        } else {
            mp4SetParseFlag(MP4_TAG_MOOF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_MOOF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x008618 */
MP4_NOINLINE int32_t mp4MoofFindOrRead(Mp4Movie *movie, Mp4IoStream *stream, uint64_t offset,
                                       Mp4MoofBox **out_moof, uint64_t *out_next_offset)
{
    int32_t status = mp4MovieFindMoofByOffset(movie, stream, offset, out_moof, out_next_offset);
    if (status == MP4_OK) {
        return MP4_OK;
    }
    if (status == MP4_ERR_NO_ENTRY) {
        return mp4MoofReadNext(movie, stream, offset, out_moof, out_next_offset);
    }
    return MP4_ERR_INTERNAL;
}

/* 0x0086b4 */
MP4_NOINLINE int32_t mp4MoofGetTrackDuration(Mp4MoofBox *moof, uint32_t track_id,
                                             Mp4MvexBox *mvex, uint32_t *out_dur)
{
    (void)mvex;

    uint32_t def_dur = 0;
    if (mp4MvexGetDefSampleDur(moof->movie->mvex, track_id, &def_dur) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    uint32_t total_dur = 0;
    for (uint32_t idx = 0; idx < moof->traf_count; idx++) {
        Mp4TrafBox *traf = moof->traf_list[idx];
        if (traf->tfhd.track_id == track_id) {
            uint32_t traf_dur = 0;
            if (mp4TrafCalcDuration(traf, def_dur, &traf_dur) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }
            total_dur += traf_dur;
        }
    }
    *out_dur = total_dur;
    return MP4_OK;
}

/* 0x008780 */
MP4_NOINLINE int32_t mp4MoofGetTimeBeforeSample(Mp4MoofBox *moof, uint32_t track_id,
                                                Mp4MvexBox *mvex, uint32_t traf_idx,
                                                uint32_t trun_idx, uint32_t sample_idx,
                                                uint32_t *out_dur)
{
    (void)mvex;

    uint32_t def_dur = 0;
    if (mp4MvexGetDefSampleDur(moof->movie->mvex, track_id, &def_dur) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    if (traf_idx >= moof->traf_count) {
        return MP4_ERR_TRAF_RANGE;
    }

    uint32_t acc_dur = 0;
    for (uint32_t idx = 0; idx < traf_idx; idx++) {
        Mp4TrafBox *traf = moof->traf_list[idx];
        if (traf->tfhd.track_id == track_id) {
            uint32_t traf_dur = 0;
            if (mp4TrafCalcDuration(traf, def_dur, &traf_dur) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }
            acc_dur += traf_dur;
        }
    }

    uint32_t partial_dur = 0;
    int32_t status = mp4TrafCalcTimeBefore(moof->traf_list[traf_idx], def_dur,
                                           trun_idx, sample_idx, &partial_dur);
    if (status == MP4_OK) {
        *out_dur = acc_dur + partial_dur;
    }
    return status;
}

/* 0x0088b0 */
MP4_NOINLINE int32_t mp4MoofGetTimeFromSample(Mp4MoofBox *moof, uint32_t track_id,
                                              Mp4MvexBox *mvex, uint32_t traf_idx,
                                              uint32_t trun_idx, uint32_t sample_idx,
                                              uint32_t *out_dur)
{
    uint32_t def_dur = 0;
    if (mp4MvexGetDefSampleDur(moof->movie->mvex, track_id, &def_dur) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    if (traf_idx >= moof->traf_count) {
        return MP4_ERR_TRAF_RANGE;
    }

    uint32_t total_dur = 0;
    for (uint32_t idx = 0; idx < moof->traf_count; idx++) {
        Mp4TrafBox *traf = moof->traf_list[idx];
        if (traf->tfhd.track_id == track_id) {
            uint32_t traf_dur = 0;
            if (mp4TrafCalcDuration(traf, def_dur, &traf_dur) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }
            total_dur += traf_dur;
        }
    }

    uint32_t before_dur = 0;
    int32_t status = mp4MoofGetTimeBeforeSample(moof, track_id, mvex, traf_idx,
                                                trun_idx, sample_idx, &before_dur);
    if (status == MP4_OK) {
        *out_dur = total_dur - before_dur;
    }
    return status;
}

/* 0x0089e4 */
MP4_NOINLINE int32_t mp4TrafParse(Mp4TrafBox *traf, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_TRAF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_TFHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_TFHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_TFHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_TFHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (traf->tfhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_TFHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    traf->tfhd.size = box_size;
                    traf->tfhd.tag = MP4_TAG_TFHD;
                    mp4ReadBytes(traf->tfhd.flags, 3, stream);
                    traf->tfhd.track_id = mp4ReadU32(stream);
                    if (mp4GetBit(traf->tfhd.flags[2], 0) != 0) {
                        traf->tfhd.base_offset_hi = mp4ReadU32(stream);
                        traf->tfhd.base_offset_lo = mp4ReadU32(stream);
                    }
                    if (mp4GetBit(traf->tfhd.flags[2], 1) != 0) {
                        traf->tfhd.sample_desc_idx = mp4ReadU32(stream);
                    }
                    if (mp4GetBit(traf->tfhd.flags[2], 3) != 0) {
                        traf->tfhd.def_sample_dur = mp4ReadU32(stream);
                    }
                    if (mp4GetBit(traf->tfhd.flags[2], 4) != 0) {
                        traf->tfhd.def_sample_size = mp4ReadU32(stream);
                    }
                    if (mp4GetBit(traf->tfhd.flags[2], 5) != 0) {
                        traf->tfhd.def_sample_flags = mp4ReadU32(stream);
                    }
                    if ((uint64_t)mp4CbTell(stream->file_handle) < box_end) {
                        mp4SetParseFlag(MP4_TAG_TFHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                    }
                }
            }
        } else if (box_tag == MP4_TAG_TRUN) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_TRUN, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_TRUN, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_TRUN, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    Mp4TrunBox *trun = (Mp4TrunBox *)mp4MemCalloc(sizeof(Mp4TrunBox));
                    if (trun == NULL) {
                        return MP4_ERR_PARSE_FATAL;
                    }
                    trun->size = box_size;
                    trun->tag = MP4_TAG_TRUN;
                    trun->version = ver;
                    mp4ReadBytes(trun->flags, 3, stream);
                    trun->sample_count = mp4ReadU32(stream);
                    if (mp4GetBit(trun->flags[2], 0) != 0) {
                        trun->data_offset = (int32_t)mp4ReadU32(stream);
                    }
                    if (mp4GetBit(trun->flags[2], 2) != 0) {
                        trun->first_sample_flags = mp4ReadU32(stream);
                    }
                    if (box_end < (uint64_t)mp4CbTell(stream->file_handle)) {
                        mp4SetParseFlag(MP4_TAG_TRUN, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        void *box_ptr = trun;
                        mp4BoxFree(&box_ptr, MP4_TAG_TRUN);
                    } else {
                        if ((trun->flags[1] & 0x0FU) != 0) {
                            uint32_t declared = trun->sample_count;
                            trun->sample_count = 0;
                            for (uint32_t si = 0; si < declared; si++) {
                                uint32_t quad[4];
                                quad[0] = 0;
                                quad[1] = 0;
                                quad[2] = 0;
                                quad[3] = 0;
                                if (mp4GetBit(trun->flags[1], 0) != 0) {
                                    quad[0] = mp4ReadU32(stream);
                                }
                                if (mp4GetBit(trun->flags[1], 1) != 0) {
                                    quad[1] = mp4ReadU32(stream);
                                }
                                if (mp4GetBit(trun->flags[1], 2) != 0) {
                                    quad[2] = mp4ReadU32(stream);
                                }
                                if (mp4GetBit(trun->flags[1], 3) != 0) {
                                    quad[3] = mp4ReadU32(stream);
                                }
                                if (box_end < (uint64_t)mp4CbTell(stream->file_handle)) {
                                    mp4SeekAbs(stream->file_handle, box_end);
                                    mp4SetParseFlag(MP4_TAG_TRUN, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
                                    break;
                                }
                                if (mp4TrunAddSample(trun, quad) != MP4_OK) {
                                    return MP4_ERR_PARSE_FATAL;
                                }
                            }
                        }
                        if (mp4TrafAddTrun(traf, trun) != MP4_OK) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                        traf->total_samples += trun->sample_count;
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_TRAF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_TRAF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x008fd4 */
MP4_NOINLINE int32_t mp4MehdReadBody(Mp4MehdBox *mehd, Mp4IoStream *stream)
{
    mp4ReadBytes(mehd->flags, 3, stream);
    mehd->fragment_duration = mp4ReadU32(stream);
    return (int32_t)mehd->fragment_duration;
}

/* 0x00901c */
MP4_NOINLINE int32_t mp4MfhdReadBody(Mp4MfhdBox *mfhd, Mp4IoStream *stream)
{
    mp4ReadBytes(mfhd->flags, 3, stream);
    mfhd->seq_number = mp4ReadU32(stream);
    return (int32_t)mfhd->seq_number;
}

/* 0x009064 */
MP4_NOINLINE void mp4MoofListFree(Mp4MoofBox *head)
{
    Mp4MoofBox *cur = head;
    while (cur != NULL) {
        Mp4MoofBox *next = cur->next;
        for (uint32_t idx = 0; idx < 127U; idx++) {
            if (cur->traf_list[idx] != NULL) {
                mp4TrafFree(cur->traf_list[idx]);
            }
        }
        mp4MemFree(cur);
        cur = next;
    }
}

/* 0x0090ec */
MP4_NOINLINE void mp4TrafFree(Mp4TrafBox *traf)
{
    if (traf != NULL) {
        mp4TrunListFree(traf->trun_head);
        mp4MemFree(traf);
    }
}

/* 0x009120 */
MP4_NOINLINE void mp4TrunListFree(Mp4TrunBox *head)
{
    Mp4TrunBox *cur = head;
    while (cur != NULL) {
        Mp4TrunBox *next = cur->next;
        if (cur->entries != NULL) {
            mp4FreeChunkArray(cur->entries);
        }
        mp4MemFree(cur);
        cur = next;
    }
}

/* 0x00917c */
MP4_NOINLINE void mp4MvexCalcSize(Mp4MvexBox *mvex)
{
    mvex->size = 8;
    if (mvex->mehd.size != 0) {
        mvex->mehd.size = 16;
        mvex->size = 24;
    }
    for (uint32_t idx = 0; idx < mvex->trex_count; idx++) {
        Mp4TrexBox *trex = mvex->trex_list[idx];
        if (trex != NULL) {
            trex->size = 32;
            mvex->size += 32U;
        }
    }
}

/* 0x0091e8 */
MP4_NOINLINE int32_t mp4TrafAddTrun(Mp4TrafBox *traf, Mp4TrunBox *trun)
{
    if ((traf == NULL) | (trun == NULL)) {
        return MP4_ERR_NULL_OBJ;
    }
    if (traf->trun_count != 0) {
        Mp4TrunBox *tail = mp4TrafGetTrun(traf, traf->trun_count - 1U);
        tail->next = trun;
    } else {
        traf->trun_head = trun;
    }
    traf->trun_count++;
    return MP4_OK;
}

/* 0x00925c */
MP4_NOINLINE int32_t mp4TrunAddSample(Mp4TrunBox *trun, const uint32_t *quad)
{
    if ((trun == NULL) | (quad == NULL)) {
        return MP4_ERR_NULL_OBJ;
    }
    uint32_t vals[4];
    vals[0] = quad[0];
    vals[1] = quad[1];
    vals[2] = quad[2];
    vals[3] = quad[3];
    if (mp4ChunkArrayAppendU32(&trun->entries, 4, vals, 20) != MP4_OK) {
        return MP4_ERR_NOMEM;
    }
    trun->sample_count++;
    return MP4_OK;
}

/* 0x0092e4 */
MP4_NOINLINE int32_t mp4MovieAppendMoof(Mp4Movie *movie, Mp4MoofBox *moof)
{
    if ((movie == NULL) | (moof == NULL)) {
        return MP4_ERR_PARSE_FATAL;
    }
    if (movie->moof_count != 0) {
        Mp4MoofBox *tail = mp4MovieGetMoofAt(movie, movie->moof_count - 1U);
        if (tail->next != NULL) {
            return MP4_ERR_PARSE_FATAL;
        }
        tail = mp4MovieGetMoofAt(movie, movie->moof_count - 1U);
        tail->next = moof;
    } else {
        if (movie->moof_head != NULL) {
            return MP4_ERR_PARSE_FATAL;
        }
        movie->moof_head = moof;
    }
    moof->movie = movie;
    movie->moof_count++;
    return MP4_OK;
}

/* 0x009384 */
MP4_NOINLINE Mp4TrunBox *mp4TrafGetTrun(Mp4TrafBox *traf, uint32_t idx)
{
    if (idx >= traf->trun_count) {
        return NULL;
    }
    Mp4TrunBox *cur = traf->trun_head;
    for (uint32_t pos = 0; pos < idx; pos++) {
        cur = cur->next;
        if (cur == NULL) {
            return NULL;
        }
    }
    return cur;
}

/* 0x0093cc */
MP4_NOINLINE int32_t mp4TrunGetSampleQuad(Mp4TrunBox *trun, uint32_t idx, uint32_t *out_quad)
{
    if (mp4ChunkArrayGetU32(trun->entries, idx, 0, &out_quad[0]) != MP4_OK) {
        return MP4_ERR_PARSE_FATAL;
    }
    mp4ChunkArrayGetU32(trun->entries, idx, 1, &out_quad[1]);
    mp4ChunkArrayGetU32(trun->entries, idx, 2, &out_quad[2]);
    mp4ChunkArrayGetU32(trun->entries, idx, 3, &out_quad[3]);
    return MP4_OK;
}

/* 0x009460 */
MP4_NOINLINE Mp4MoofBox *mp4MovieGetMoofAt(Mp4Movie *movie, uint32_t idx)
{
    if (idx >= movie->moof_count) {
        return NULL;
    }
    Mp4MoofBox *cur = movie->moof_head;
    for (uint32_t pos = 0; pos < idx; pos++) {
        cur = cur->next;
        if (cur == NULL) {
            return NULL;
        }
    }
    return cur;
}

/* 0x0094a8 */
MP4_NOINLINE int32_t mp4TrunSumSizes(Mp4TrexBox *trex, Mp4TrafBox *traf, Mp4TrunBox *trun,
                                     uint32_t start_idx, uint32_t end_idx, uint32_t *out_bytes)
{
    *out_bytes = 0;
    if (start_idx >= trun->sample_count) {
        return MP4_ERR_CHUNK_EMPTY;
    }
    if (mp4GetBit(trun->flags[1], 1) == 0) {
        uint32_t unit_size = 0;
        if (mp4GetBit(traf->tfhd.flags[2], 4) != 0) {
            unit_size = traf->tfhd.def_sample_size;
        } else {
            if (trex == NULL) {
                return MP4_ERR_CORRUPT;
            }
            unit_size = trex->def_sample_size;
        }
        *out_bytes += (end_idx - start_idx + 1U) * unit_size;
        return MP4_OK;
    }

    for (uint32_t cur = start_idx; cur <= end_idx && cur < trun->sample_count; cur++) {
        uint32_t quad[4];
        if (mp4TrunGetSampleQuad(trun, cur, quad) != MP4_OK) {
            return MP4_ERR_CHUNK_EMPTY;
        }
        *out_bytes += quad[1];
    }
    return MP4_OK;
}

/* 0x0095dc */
MP4_NOINLINE int32_t mp4TrunGetSampleDur(Mp4TrunBox *trun, uint32_t idx,
                                         Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_dur)
{
    if (trun->sample_count < idx) {
        return MP4_ERR_CHUNK_EMPTY;
    }
    if ((trun->flags[1] & 1U) != 0) {
        uint32_t quad[4];
        mp4TrunGetSampleQuad(trun, idx, quad);
        *out_dur = quad[0];
        return MP4_OK;
    }
    if (tfhd == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if ((tfhd->flags[2] & 8U) != 0) {
        *out_dur = tfhd->def_sample_dur;
        return MP4_OK;
    }
    if (trex == NULL) {
        return MP4_ERR_CORRUPT;
    }
    *out_dur = trex->def_sample_dur;
    return MP4_OK;
}

/* 0x009674 */
MP4_NOINLINE int32_t mp4TrunGetSampleIsSync(Mp4TrunBox *trun, uint32_t idx,
                                            Mp4TfhdBox *tfhd, Mp4TrexBox *trex,
                                            uint32_t *out_is_sync)
{
    if (trun->sample_count < idx) {
        return MP4_ERR_CHUNK_EMPTY;
    }

    uint32_t smp_flags = 0;
    if (idx == 0 && (trun->flags[2] & 4U) != 0) {
        smp_flags = trun->first_sample_flags;
    } else if ((trun->flags[1] & 4U) != 0) {
        uint32_t quad[4];
        mp4TrunGetSampleQuad(trun, idx, quad);
        smp_flags = quad[2];
    } else {
        if (tfhd == NULL) {
            return MP4_ERR_CORRUPT;
        }
        if ((tfhd->flags[2] & 0x20U) != 0) {
            smp_flags = tfhd->def_sample_flags;
        } else {
            if (trex == NULL) {
                return MP4_ERR_CORRUPT;
            }
            smp_flags = trex->def_sample_flags;
        }
    }

    if ((smp_flags & 0x10000U) != 0U) {
        *out_is_sync = 0U;
    } else {
        *out_is_sync = 1U;
    }
    return MP4_OK;
}

/* 0x009730 */
MP4_NOINLINE int32_t mp4TrafResolveDataOffsets(Mp4TrafBox *prev_traf, Mp4MoofBox *moof,
                                               Mp4TrafBox *traf)
{
    uint32_t base_hi = 0;
    uint32_t base_lo = 0;
    Mp4Track *trk = NULL;
    Mp4TrexBox *trex = NULL;

    if ((traf->tfhd.flags[2] & 1U) != 0) {
        base_hi = traf->tfhd.base_offset_hi;
        base_lo = traf->tfhd.base_offset_lo;
    } else if (prev_traf == NULL) {
        base_hi = (uint32_t)(moof->file_offset >> 32);
        base_lo = (uint32_t)moof->file_offset;
    } else {
        if (mp4MovieFindTrackById(moof->movie, prev_traf->tfhd.track_id, &trk) != MP4_OK) {
            return MP4_ERR_NO_ENTRY;
        }
        mp4TrackGetTrex(trk, &trex);

        uint64_t max_end = 0;
        int32_t valid_cnt = 0;
        Mp4TrunBox *cur = prev_traf->trun_head;
        for (uint32_t ti = 0; ti < prev_traf->trun_count && cur != NULL; ti++) {
            if (cur->resolved_offset != ~(uint64_t)0) {
                uint64_t end_off = cur->resolved_offset;
                uint32_t bytes = 0;
                if (mp4TrunSumSizes(trex, prev_traf, cur, 0, cur->sample_count - 1U, &bytes) == MP4_OK) {
                    if (mp4CheckedAddU64(&end_off, bytes) == MP4_OK) {
                        if (end_off > max_end) {
                            max_end = end_off;
                        }
                        valid_cnt++;
                    }
                }
            }
            cur = cur->next;
        }
        if (valid_cnt == 0) {
            base_hi = 0xFFFFFFFFU;
            base_lo = 0xFFFFFFFFU;
        } else {
            base_hi = (uint32_t)(max_end >> 32);
            base_lo = (uint32_t)max_end;
        }
    }

    if (mp4MovieFindTrackById(moof->movie, traf->tfhd.track_id, &trk) != MP4_OK) {
        return MP4_ERR_NO_ENTRY;
    }
    mp4TrackGetTrex(trk, &trex);

    Mp4TrunBox *cur = traf->trun_head;
    Mp4TrunBox *prev = NULL;
    for (uint32_t ti = 0; ti < traf->trun_count; ti++) {
        uint32_t tmp_hi = base_hi;
        uint32_t tmp_lo = base_lo;
        if ((cur->flags[2] & 1U) != 0) {
            if (mp4AdjustU64(&tmp_hi, &tmp_lo, cur->data_offset) == MP4_OK) {
                cur->resolved_offset = ((uint64_t)tmp_hi << 32) | (uint64_t)tmp_lo;
            } else {
                cur->resolved_offset = ~(uint64_t)0;
            }
        } else if (ti == 0) {
            cur->resolved_offset = ((uint64_t)base_hi << 32) | (uint64_t)base_lo;
        } else if (prev->sample_count == 0) {
            cur->resolved_offset = prev->resolved_offset;
        } else {
            uint32_t bytes = 0;
            if (mp4TrunSumSizes(trex, traf, prev, 0, prev->sample_count, &bytes) != MP4_OK) {
                cur->resolved_offset = ~(uint64_t)0;
            } else {
                uint64_t offset_val = prev->resolved_offset;
                if (mp4AddS32ToU64(&offset_val, (int32_t)bytes) == MP4_OK) {
                    cur->resolved_offset = offset_val;
                }
            }
        }
        prev = cur;
        cur = cur->next;
    }
    return MP4_OK;
}

/* 0x009a5c */
MP4_NOINLINE int32_t mp4TrunGetSampleSize(Mp4TrunBox *trun, uint32_t idx,
                                          Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_size)
{
    if (trun == NULL || trun->sample_count < idx) {
        return MP4_ERR_CHUNK_EMPTY;
    }
    if ((trun->flags[1] & 2U) != 0) {
        uint32_t quad[4];
        mp4TrunGetSampleQuad(trun, idx, quad);
        *out_size = quad[1];
        return MP4_OK;
    }
    if (tfhd == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if ((tfhd->flags[2] & 0x10U) != 0) {
        *out_size = tfhd->def_sample_size;
        return MP4_OK;
    }
    if (trex == NULL) {
        return MP4_ERR_CORRUPT;
    }
    *out_size = trex->def_sample_size;
    return MP4_OK;
}

/* 0x009aec */
MP4_NOINLINE int32_t mp4TfhdGetSampleDescIdx(Mp4TfhdBox *tfhd, Mp4TrexBox *trex, uint32_t *out_idx)
{
    if ((tfhd->flags[2] & 2U) != 0) {
        *out_idx = tfhd->sample_desc_idx;
        return MP4_OK;
    }
    if (trex == NULL) {
        return MP4_ERR_CORRUPT;
    }
    *out_idx = trex->def_sample_desc;
    return MP4_OK;
}

/* 0x009b20 */
MP4_NOINLINE int32_t mp4TrafGetSampleDetail(Mp4TrafBox *traf, Mp4TrexBox *trex,
                                            uint32_t sample_idx, Mp4SampleInfo *out_info,
                                            uint32_t *out_desc_idx)
{
    Mp4TrunBox *cur = NULL;
    uint32_t rel_idx = 0;
    uint64_t sample_off = 0;
    int32_t use_cache = 0;

    if (traf->cached_trun != NULL) {
        int32_t diff = (int32_t)sample_idx - (int32_t)traf->cached_sample_idx + (int32_t)traf->cached_rel_idx;
        if (diff >= 0 && (uint32_t)diff < traf->cached_trun->sample_count) {
            cur = traf->cached_trun;
            rel_idx = (uint32_t)diff;
            if (cur->resolved_offset == ~(uint64_t)0) {
                return MP4_ERR_OVERFLOW;
            }
            if (rel_idx == traf->cached_rel_idx) {
                sample_off = traf->cached_offset;
            } else if (rel_idx < traf->cached_rel_idx) {
                uint32_t rewind_bytes = 0;
                int32_t sum_res = mp4TrunSumSizes(trex, traf, cur, rel_idx,
                                                  traf->cached_rel_idx - 1U, &rewind_bytes);
                if (sum_res != MP4_OK) {
                    return sum_res;
                }
                uint32_t hi = (uint32_t)(traf->cached_offset >> 32);
                uint32_t lo = (uint32_t)traf->cached_offset;
                if (mp4SubU64(&hi, &lo, 0, rewind_bytes) != MP4_OK) {
                    return MP4_ERR_CORRUPT;
                }
                sample_off = ((uint64_t)hi << 32) | (uint64_t)lo;
            } else {
                uint32_t add_bytes = 0;
                int32_t sum_res = mp4TrunSumSizes(trex, traf, cur, traf->cached_rel_idx,
                                                  rel_idx - 1U, &add_bytes);
                if (sum_res != MP4_OK) {
                    return sum_res;
                }
                sample_off = traf->cached_offset;
                if (mp4CheckedAddU64(&sample_off, add_bytes) != MP4_OK) {
                    return MP4_ERR_OVERFLOW;
                }
            }
            use_cache = 1;
        }
    }

    if (use_cache == 0) {
        cur = traf->trun_head;
        if (cur == NULL) {
            return MP4_ERR_CHUNK_EMPTY;
        }
        uint32_t rem = sample_idx;
        uint32_t ti = 0;
        while (rem >= cur->sample_count && ti < traf->trun_count) {
            rem -= cur->sample_count;
            cur = cur->next;
            ti++;
            if (cur == NULL) {
                return MP4_ERR_CHUNK_EMPTY;
            }
        }
        rel_idx = rem;
        if (cur->resolved_offset == ~(uint64_t)0) {
            return MP4_ERR_OVERFLOW;
        }
        sample_off = cur->resolved_offset;
        if (rel_idx != 0) {
            uint32_t pre_bytes = 0;
            if (mp4TrunSumSizes(trex, traf, cur, 0, rel_idx - 1U, &pre_bytes) != MP4_OK) {
                return MP4_ERR_OVERFLOW;
            }
            if (mp4CheckedAddU64(&sample_off, pre_bytes) != MP4_OK) {
                return MP4_ERR_OVERFLOW;
            }
        }
    }

    traf->cached_sample_idx = sample_idx;
    traf->cached_offset = sample_off;
    traf->cached_rel_idx = rel_idx;
    traf->cached_trun = cur;
    out_info->offset = sample_off;

    if ((cur->flags[1] & 8U) != 0) {
        uint32_t quad[4];
        quad[0] = 0;
        quad[1] = 0;
        quad[2] = 0;
        quad[3] = 0;
        if (mp4TrunGetSampleQuad(cur, rel_idx, quad) != MP4_OK) {
            return MP4_ERR_CHUNK_EMPTY;
        }
        out_info->ctts_offset = quad[3];
    } else {
        out_info->ctts_offset = 0;
    }

    int32_t status = mp4TrunGetSampleSize(cur, rel_idx, &traf->tfhd, trex, &out_info->size);
    if (status != MP4_OK) {
        return status;
    }
    status = mp4TrunGetSampleDur(cur, rel_idx, &traf->tfhd, trex, &out_info->duration);
    if (status != MP4_OK) {
        return status;
    }
    status = mp4TrunGetSampleIsSync(cur, rel_idx, &traf->tfhd, trex, &out_info->is_sync);
    if (status != MP4_OK) {
        return status;
    }
    return mp4TfhdGetSampleDescIdx(&traf->tfhd, trex, out_desc_idx);
}

/* 0x009eac */
MP4_NOINLINE int32_t mp4MovieFindMoofByOffset(Mp4Movie *movie, Mp4IoStream *stream,
                                              uint64_t offset, Mp4MoofBox **out_moof,
                                              uint64_t *out_next_offset)
{
    (void)stream;

    Mp4MoofBox *cur = movie->moof_head;
    uint32_t idx = 1;
    while (cur != NULL) {
        if (cur->file_offset == offset) {
            break;
        }
        if (movie->moof_count < idx) {
            break;
        }
        idx++;
        cur = cur->next;
    }

    *out_moof = cur;
    if (cur == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_next_offset = cur->next_moof_offset;
    return MP4_OK;
}

/* 0x009f18 */
MP4_NOINLINE int32_t mp4MoofCountTrafForTrack(Mp4MoofBox *moof, uint32_t track_id,
                                              uint32_t *out_count)
{
    if (moof == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    uint32_t count = 0;
    for (uint32_t idx = 0; idx < moof->traf_count; idx++) {
        if (moof->traf_list[idx]->tfhd.track_id == track_id) {
            count++;
        }
    }
    *out_count = count;
    return MP4_OK;
}

/* 0x009f68 */
MP4_NOINLINE int32_t mp4MoofGetTrafForTrack(Mp4MoofBox *moof, uint32_t track_id,
                                            uint32_t match_idx, Mp4TrafBox **out_traf)
{
    if (moof == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    uint32_t seen = 0;
    for (uint32_t idx = 0; idx < moof->traf_count; idx++) {
        Mp4TrafBox *traf = moof->traf_list[idx];
        if (traf->tfhd.track_id == track_id) {
            if (seen == match_idx) {
                *out_traf = traf;
                return MP4_OK;
            }
            seen++;
        }
    }
    return MP4_ERR_NO_ENTRY;
}

/* 0x009fc8 */
MP4_NOINLINE int32_t mp4MvexGetDefSampleDur(Mp4MvexBox *mvex, uint32_t track_id,
                                            uint32_t *out_dur)
{
    uint32_t def_dur = 0;
    if (mvex != NULL) {
        for (uint32_t idx = 0; idx < mvex->trex_count; idx++) {
            Mp4TrexBox *trex = mvex->trex_list[idx];
            if (trex->track_id == track_id) {
                def_dur = trex->def_sample_dur;
            }
        }
    }
    *out_dur = def_dur;
    return MP4_OK;
}

/* 0x00a014 */
MP4_NOINLINE int32_t mp4TrafCalcDuration(Mp4TrafBox *traf, uint32_t def_dur, uint32_t *out_dur)
{
    if ((traf->tfhd.flags[2] & 8U) != 0) {
        def_dur = traf->tfhd.def_sample_dur;
    }
    uint32_t total = 0;
    for (uint32_t ti = 0; ti < traf->trun_count; ti++) {
        Mp4TrunBox *trun = mp4TrafGetTrun(traf, ti);
        if ((trun->flags[1] & 1U) != 0) {
            for (uint32_t si = 0; si < trun->sample_count; si++) {
                uint32_t dur = 0;
                if (mp4ChunkArrayGetU32(trun->entries, si, 0, &dur) != MP4_OK) {
                    return MP4_ERR_INTERNAL;
                }
                total += dur;
            }
        } else {
            total += def_dur * trun->sample_count;
        }
    }
    *out_dur = total;
    return MP4_OK;
}

/* 0x00a12c */
MP4_NOINLINE int32_t mp4TrafCalcTimeBefore(Mp4TrafBox *traf, uint32_t def_dur,
                                           uint32_t trun_idx, uint32_t sample_idx,
                                           uint32_t *out_dur)
{
    if ((traf->tfhd.flags[2] & 8U) != 0) {
        def_dur = traf->tfhd.def_sample_dur;
    }
    if (trun_idx >= traf->trun_count) {
        return MP4_ERR_TRUN_RANGE;
    }

    uint32_t total = 0;
    for (uint32_t ti = 0; ti < trun_idx; ti++) {
        Mp4TrunBox *trun = mp4TrafGetTrun(traf, ti);
        if ((trun->flags[1] & 1U) != 0) {
            for (uint32_t si = 0; si < trun->sample_count; si++) {
                uint32_t dur = 0;
                if (mp4ChunkArrayGetU32(trun->entries, si, 0, &dur) != MP4_OK) {
                    return MP4_ERR_INTERNAL;
                }
                total += dur;
            }
        } else {
            total += def_dur * trun->sample_count;
        }
    }

    Mp4TrunBox *last_trun = mp4TrafGetTrun(traf, trun_idx);
    if (sample_idx >= last_trun->sample_count) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if ((last_trun->flags[1] & 1U) != 0) {
        for (uint32_t si = 0; si < sample_idx; si++) {
            uint32_t dur = 0;
            if (mp4ChunkArrayGetU32(last_trun->entries, si, 0, &dur) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }
            total += dur;
        }
    } else {
        total += def_dur * sample_idx;
    }

    *out_dur = total;
    return MP4_OK;
}

/* 0x00a2d4 */
MP4_NOINLINE int32_t mp4MvexFree(Mp4MvexBox *mvex)
{
    for (uint32_t idx = 0; idx < mvex->trex_count; idx++) {
        if (mvex->trex_list[idx] != NULL) {
            mp4MemFree(mvex->trex_list[idx]);
        }
    }
    return mp4MemFree(mvex);
}

/* 0x00a34c */
MP4_NOINLINE int32_t mp4TrafGetTrunSampleCount(Mp4TrafBox *traf, uint32_t trun_idx,
                                               uint32_t *out_count)
{
    if (traf == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4TrunBox *trun = mp4TrafGetTrun(traf, trun_idx);
    if (trun == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_count = trun->sample_count;
    return MP4_OK;
}

/* 0x00a398 */
MP4_NOINLINE int32_t mp4MoofGetOffset(Mp4MoofBox *moof, uint64_t *out_offset)
{
    if (moof == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    *out_offset = moof->file_offset;
    return MP4_OK;
}

/* 0x00a3bc */
MP4_NOINLINE int32_t mp4MoofGetSeqNum(Mp4MoofBox *moof, uint32_t *out_seq)
{
    if (moof == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    *out_seq = moof->mfhd.seq_number;
    return MP4_OK;
}

/* 0x00a3d8 */
MP4_NOINLINE int32_t mp4MoofGetMovie(Mp4MoofBox *moof, Mp4Movie **out_movie)
{
    if (moof == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    *out_movie = moof->movie;
    return MP4_OK;
}

/* 0x00a3f4 */
MP4_NOINLINE int32_t mp4BoxFree(void **box_ptr, uint32_t tag)
{
    void *obj = *box_ptr;
    if (obj == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    switch (tag) {
    case MP4_TAG_MVEX:
        mp4MvexFree((Mp4MvexBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_TRAK:
        mp4FreeTrack((Mp4Track *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_TRUN:
        mp4TrunListFree((Mp4TrunBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_VTBL:
        mp4FreeChunkArray((Mp4ChunkArray *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_TREF:
        mp4FreeTref((Mp4TrefBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_STBL:
        mp4FreeStblData((Mp4Stbl *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_SLDM:
        mp4FreeMovie((Mp4Movie *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_TRAF:
        mp4TrafFree((Mp4TrafBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_AVCC:
        mp4FreeExtraBoxAlt((Mp4ExtraBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case 0x6472656EU: /* 'dren' */
        mp4FreeTrefType((Mp4TrefTypeBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_ESDS:
        mp4FreeEsds((Mp4EsdsBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_MOOF:
        mp4MoofListFree((Mp4MoofBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_IODS:
        mp4FreeIods((Mp4IodsBox *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case 0x6D6F7476U: /* 'motv' */
        mp4FreeSampleEntry((Mp4SampleEntry *)obj);
        *box_ptr = NULL;
        return MP4_OK;
    case MP4_TAG_FTYP:
    case MP4_TAG_MPOD:
    case MP4_TAG_SYNC:
    case MP4_TAG_TREX:
        mp4MemFree(obj);
        *box_ptr = NULL;
        return MP4_OK;
    default:
        return MP4_ERR_PARSE_FATAL;
    }
}

/* 0x00a648 */
MP4_NOINLINE int32_t mp4MemFree(void *ptr)
{
    if (ptr == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    mp4CbFree(ptr);
    return MP4_OK;
}

/* 0x00a674 */
MP4_NOINLINE int32_t mp4MovieRemoveTrackById(Mp4Movie *movie, uint32_t track_id)
{
    if (movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (movie->mvhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }

    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        if (trk->tkhd.size == 0) {
            return MP4_ERR_CORRUPT;
        }
    }

    uint32_t match_idx = 0;
    while (match_idx < movie->track_count) {
        if (mp4MovieGetTrackAt(movie, match_idx)->tkhd.track_id == track_id) {
            break;
        }
        match_idx++;
    }
    if (match_idx >= movie->track_count) {
        return MP4_ERR_NO_ENTRY;
    }

    Mp4Track *victim = mp4MovieGetTrackAt(movie, match_idx);
    if (match_idx != 0) {
        Mp4Track *prev = mp4MovieGetTrackAt(movie, match_idx - 1U);
        prev->next = mp4MovieGetTrackAt(movie, match_idx)->next;
    } else {
        movie->tracks_head = mp4MovieGetTrackAt(movie, 0)->next;
    }
    mp4FreeTrack(victim);
    movie->track_count--;

    uint32_t next_id = 1;
    if (movie->track_count != 0) {
        uint32_t max_id = 0;
        for (uint32_t idx = 0; idx < movie->track_count; idx++) {
            Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
            if (trk->tkhd.size != 0 && max_id < trk->tkhd.track_id) {
                max_id = trk->tkhd.track_id;
            }
        }
        next_id = max_id + 1U;
    }
    movie->mvhd.next_track_id = next_id;
    mp4MovieUpdateDuration(movie);
    return MP4_OK;
}

/* 0x00a828 */
MP4_NOINLINE int32_t mp4MovieRemoveTrackByIndex(Mp4Movie *movie, uint32_t index)
{
    if (movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (index >= movie->track_count) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4Track *victim = mp4MovieGetTrackAt(movie, index);
    if (index != 0) {
        Mp4Track *prev = mp4MovieGetTrackAt(movie, index - 1U);
        prev->next = mp4MovieGetTrackAt(movie, index)->next;
    } else {
        movie->tracks_head = mp4MovieGetTrackAt(movie, 0)->next;
    }
    mp4FreeTrack(victim);
    movie->track_count--;
    return MP4_OK;
}

/* 0x00a8e4 */
MP4_NOINLINE int32_t mp4MovieAppendTrack(Mp4Movie *movie, Mp4Track *track)
{
    uint32_t wanted_id = track->tkhd.track_id;
    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *cur = mp4MovieGetTrackAt(movie, idx);
        if (cur != NULL && cur->tkhd.track_id == wanted_id) {
            wanted_id = 0;
            break;
        }
    }

    if (wanted_id == 0) {
        uint32_t cand = 1;
        while (cand <= movie->track_count + 1U) {
            int32_t taken = 0;
            for (uint32_t idx = 0; idx < movie->track_count; idx++) {
                Mp4Track *cur = mp4MovieGetTrackAt(movie, idx);
                if (cur != NULL && cur->tkhd.track_id == cand) {
                    taken = 1;
                }
            }
            if (taken == 0) {
                wanted_id = cand;
                break;
            }
            cand++;
        }
    }
    track->tkhd.track_id = wanted_id;

    if (movie->track_count != 0) {
        Mp4Track *tail = mp4MovieGetTrackAt(movie, movie->track_count - 1U);
        if (tail == NULL || tail->next != NULL) {
            return MP4_ERR_PARSE_FATAL;
        }
        tail->next = track;
    } else {
        if (movie->tracks_head != NULL) {
            return MP4_ERR_PARSE_FATAL;
        }
        movie->tracks_head = track;
    }

    track->next = NULL;
    movie->track_count++;

    uint32_t max_id = 0;
    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *cur = mp4MovieGetTrackAt(movie, idx);
        if (cur != NULL && max_id < cur->tkhd.track_id) {
            max_id = cur->tkhd.track_id;
        }
    }
    uint32_t next_id = max_id + 1U;
    movie->mvhd.next_track_id = (next_id == 0) ? 0xFFFFFFFFU : next_id;
    track->movie = movie;
    return MP4_OK;
}

/* 0x00ab14 */
MP4_NOINLINE Mp4Track *mp4MovieGetTrackAt(Mp4Movie *movie, uint32_t index)
{
    if (index >= movie->track_count) {
        return NULL;
    }
    Mp4Track *cur = movie->tracks_head;
    for (uint32_t pos = 0; pos < index; pos++) {
        cur = cur->next;
        if (cur == NULL) {
            return NULL;
        }
    }
    return cur;
}

/* 0x00ab54 */
MP4_NOINLINE int32_t mp4TrackGetTrex(Mp4Track *track, Mp4TrexBox **out_trex)
{
    uint32_t tid = track->tkhd.track_id;
    Mp4MvexBox *mvex = track->movie->mvex;
    Mp4TrexBox *found = NULL;

    if (mvex != NULL) {
        for (uint32_t idx = 0; idx < mvex->trex_count; idx++) {
            Mp4TrexBox *trex = mvex->trex_list[idx];
            if (trex == NULL) {
                break;
            }
            if (trex->track_id == tid) {
                found = trex;
                break;
            }
        }
    }

    *out_trex = found;
    return (found != NULL) ? MP4_OK : MP4_ERR_PARSE_FATAL;
}

/* 0x00abb0 */
MP4_NOINLINE int32_t mp4ElstFindSegment(Mp4ElstBox *elst, uint32_t time_val,
                                        uint32_t *out_seg_idx, uint32_t *out_offset_in_seg)
{
    if (elst == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    uint32_t acc = 0;
    for (uint32_t idx = 0; idx < elst->entry_count; idx++) {
        uint32_t seg_dur = 0;
        mp4ChunkArrayGetU32(elst->entries, idx, 0, &seg_dur);
        acc += seg_dur;
        if (time_val < acc) {
            *out_seg_idx = idx;
            *out_offset_in_seg = time_val - (acc - seg_dur);
            return MP4_OK;
        }
    }
    return MP4_ERR_PARSE_FATAL;
}

/* 0x00ac8c */
MP4_NOINLINE int32_t mp4DrefGetEntry(uint16_t index_one_based, Mp4DrefBox *dref,
                                     Mp4DrefEntry **out_entry)
{
    Mp4DrefEntry *cur = dref->head;
    if (cur == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    for (uint32_t pos = 1; pos < (uint32_t)index_one_based; pos++) {
        cur = cur->next;
        if (cur == NULL) {
            return MP4_ERR_NO_ENTRY;
        }
    }
    *out_entry = cur;
    return MP4_OK;
}

/* 0x00acd4 */
MP4_NOINLINE int32_t mp4StsdGetEntry(Mp4StsdBox *stsd, uint32_t index_one_based,
                                     Mp4SampleEntry **out_entry)
{
    if (stsd == NULL || stsd->size == 0) {
        return MP4_ERR_INVALID_HANDLE;
    }
    Mp4SampleEntry *cur = stsd->head;
    for (uint32_t pos = 1; pos < index_one_based; pos++) {
        cur = cur->next;
        if (cur == NULL) {
            return MP4_ERR_NO_ENTRY;
        }
    }
    *out_entry = cur;
    return MP4_OK;
}

/* 0x00ad2c */
MP4_NOINLINE int32_t mp4TrackMapMovieTime(Mp4Track *track, uint32_t movie_time,
                                          int32_t mode, uint32_t *out_media_time)
{
    Mp4Movie *movie = track->movie;
    if (movie == NULL || movie->mvhd.size == 0 || track->mdia.mdhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (movie_time == 0 && mode == 0) {
        return 8196;
    }

    uint32_t rel_movie_time = movie_time;
    uint32_t base_media_time = 0;

    if (movie->mvex != NULL) {
        if (track->edts.elst.size != 0) {
            uint32_t query_time = movie_time;
            if (mode == 0) {
                if (query_time == 0) {
                    return 8196;
                }
                query_time--;
            }
            uint32_t seg_idx = 0;
            uint32_t seg_off = 0;
            if (mp4ElstFindSegment(&track->edts.elst, query_time, &seg_idx, &seg_off) != MP4_OK) {
                rel_movie_time = movie_time - track->tkhd.duration;
                base_media_time = track->mdia.mdhd.duration;
            } else {
                if (seg_idx >= track->edts.elst.entry_count) {
                    return MP4_ERR_CORRUPT;
                }
                uint32_t media_start = 0;
                if (mp4ChunkArrayGetU32(track->edts.elst.entries, seg_idx, 1, &media_start) != MP4_OK) {
                    return MP4_ERR_CORRUPT;
                }
                if (media_start == 0xFFFFFFFFU) {
                    return 8199;
                }
                base_media_time = media_start;
                uint32_t pre_dur = 0;
                for (uint32_t idx = 0; idx < seg_idx; idx++) {
                    uint32_t seg_dur = 0;
                    if (idx >= track->edts.elst.entry_count ||
                        mp4ChunkArrayGetU32(track->edts.elst.entries, idx, 0, &seg_dur) != MP4_OK) {
                        return MP4_ERR_CORRUPT;
                    }
                    pre_dur += seg_dur;
                }
                rel_movie_time = movie_time - pre_dur;
            }
        }
    } else if (track->edts.elst.size != 0) {
        uint32_t query_time = movie_time;
        if (mode == 0) {
            if (query_time == 0) {
                return 8196;
            }
            query_time--;
        }
        uint32_t seg_idx = 0;
        uint32_t seg_off = 0;
        if (mp4ElstFindSegment(&track->edts.elst, query_time, &seg_idx, &seg_off) != MP4_OK) {
            return 8196;
        }
        if (seg_idx >= track->edts.elst.entry_count) {
            return MP4_ERR_CORRUPT;
        }
        uint32_t media_start = 0;
        if (mp4ChunkArrayGetU32(track->edts.elst.entries, seg_idx, 1, &media_start) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }
        if (media_start == 0xFFFFFFFFU) {
            return 8199;
        }
        base_media_time = media_start;
        uint32_t pre_dur = 0;
        for (uint32_t idx = 0; idx < seg_idx; idx++) {
            uint32_t seg_dur = 0;
            if (idx >= track->edts.elst.entry_count ||
                mp4ChunkArrayGetU32(track->edts.elst.entries, idx, 0, &seg_dur) != MP4_OK) {
                return MP4_ERR_CORRUPT;
            }
            pre_dur += seg_dur;
        }
        rel_movie_time = movie_time - pre_dur;
    } else {
        if (movie_time > track->tkhd.duration ||
            (movie_time == track->tkhd.duration && mode == 1)) {
            return 8196;
        }
    }

    uint32_t prod_hi = 0;
    uint32_t prod_lo = 0;
    uint32_t quot_hi = 0;
    uint32_t quot_lo = 0;
    uint32_t rem = 0;
    mp4MulU32ToU64(rel_movie_time, track->mdia.mdhd.timescale, &prod_hi, &prod_lo);
    mp4DivU64ByU32(prod_hi, prod_lo, track->movie_timescale, &quot_hi, &quot_lo, &rem);
    *out_media_time = base_media_time + quot_lo;
    return MP4_OK;
}

/* 0x00b004 */
MP4_NOINLINE int32_t mp4ChunkArrayEraseU32(Mp4ChunkArray **head_ptr, uint32_t first_idx,
                                           uint32_t last_idx, uint32_t cols)
{
    Mp4ChunkArray *head = *head_ptr;
    if (head == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    uint32_t start_row = first_idx;
    uint32_t end_row = last_idx;
    Mp4ChunkArray *first_blk = mp4ChunkArrayLocate(head, &start_row);
    if (first_blk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    Mp4ChunkArray *last_blk = mp4ChunkArrayLocate(head, &end_row);
    if (last_blk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    if (first_blk == last_blk) {
        uint32_t move_words = (first_blk->count - end_row - 1U) * cols;
        for (uint32_t idx = 0; idx < move_words; idx++) {
            first_blk->words[start_row * cols + idx] = first_blk->words[(end_row + 1U) * cols + idx];
        }
        first_blk->count = first_blk->count + start_row - end_row - 1U;
    } else {
        Mp4ChunkArray *mid = first_blk->next;
        while (mid != last_blk) {
            Mp4ChunkArray *next_mid = mid->next;
            if (mid->words != NULL) {
                mp4MemFree(mid->words);
            }
            if (mid->extra_a != NULL) {
                mp4MemFree(mid->extra_a);
            }
            if (mid->wide_words != NULL) {
                mp4MemFree(mid->wide_words);
            }
            mp4MemFree(mid);
            mid = next_mid;
        }
        first_blk->next = last_blk;
        first_blk->count = start_row;

        uint32_t move_words = (last_blk->count - end_row - 1U) * cols;
        for (uint32_t idx = 0; idx < move_words; idx++) {
            last_blk->words[idx] = last_blk->words[(end_row + 1U) * cols + idx];
        }
        last_blk->count = last_blk->count - end_row - 1U;
    }

    *head_ptr = mp4ChunkArrayPruneEmpty(*head_ptr);
    return MP4_OK;
}

/* 0x00b248 */
MP4_NOINLINE int32_t mp4ChunkArrayEraseU64(Mp4ChunkArray **head_ptr, uint32_t first_idx,
                                           uint32_t last_idx, uint32_t cols)
{
    Mp4ChunkArray *head = *head_ptr;
    if (head == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    uint32_t start_row = first_idx;
    uint32_t end_row = last_idx;
    Mp4ChunkArray *first_blk = mp4ChunkArrayLocate(head, &start_row);
    if (first_blk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    Mp4ChunkArray *last_blk = mp4ChunkArrayLocate(head, &end_row);
    if (last_blk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    if (first_blk == last_blk) {
        uint32_t move_cnt = (first_blk->count - end_row - 1U) * cols;
        for (uint32_t idx = 0; idx < move_cnt; idx++) {
            first_blk->wide_words[start_row * cols + idx] =
                first_blk->wide_words[(end_row + 1U) * cols + idx];
        }
        first_blk->count = first_blk->count + start_row - end_row - 1U;
    } else {
        Mp4ChunkArray *mid = first_blk->next;
        while (mid != last_blk) {
            Mp4ChunkArray *next_mid = mid->next;
            if (mid->words != NULL) {
                mp4MemFree(mid->words);
            }
            if (mid->extra_a != NULL) {
                mp4MemFree(mid->extra_a);
            }
            if (mid->wide_words != NULL) {
                mp4MemFree(mid->wide_words);
            }
            mp4MemFree(mid);
            mid = next_mid;
        }
        first_blk->next = last_blk;
        first_blk->count = start_row;

        uint32_t move_cnt = (last_blk->count - end_row - 1U) * cols;
        for (uint32_t idx = 0; idx < move_cnt; idx++) {
            last_blk->wide_words[idx] = last_blk->wide_words[(end_row + 1U) * cols + idx];
        }
        last_blk->count = last_blk->count - end_row - 1U;
    }

    *head_ptr = mp4ChunkArrayPruneEmpty(*head_ptr);
    return MP4_OK;
}

/* 0x00b470 */
MP4_NOINLINE int32_t mp4MovieUpdateDuration(Mp4Movie *movie)
{
    if (movie == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (movie->mvhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    uint32_t max_dur = 0;
    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        if (trk->tkhd.size != 0 && trk->mdia.hdlr.size != 0 &&
            trk->mdia.hdlr.handler_type != MP4_TAG_ODSM &&
            max_dur < trk->tkhd.duration) {
            max_dur = trk->tkhd.duration;
        }
    }
    movie->mvhd.duration = max_dur;
    return MP4_OK;
}

/* 0x00b56c */
MP4_NOINLINE int32_t mp4TrackCalcDuration(Mp4Track *track, uint32_t *out_dur, int32_t allow_io)
{
    if (track->mdia.mdhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    Mp4Movie *movie = track->movie;
    if (movie == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (movie->mvex == NULL) {
        *out_dur = track->mdia.mdhd.duration;
        return MP4_OK;
    }

    Mp4Movie *root = movie->self;
    if (root->mfra == NULL) {
        return mp4StblGetTotalDuration(&track->mdia.minf.stbl, out_dur, allow_io);
    }

    Mp4Handle *handle = root->handle;
    uint32_t tid = track->tkhd.track_id;
    uint32_t found = 0;
    Mp4TfraEntry last_entry;
    int32_t status = mp4MfraLookupLast(root->mfra, tid, &found, &last_entry);
    if (status != MP4_OK) {
        return status;
    }
    if (found == 0) {
        return MP4_ERR_IO;
    }

    Mp4MoofBox *moof = NULL;
    uint64_t next_off = 0;
    status = mp4MoofFindOrRead(handle->movie, handle->stream, last_entry.moof_offset,
                               &moof, &next_off);
    if (status != MP4_OK) {
        return status;
    }

    uint32_t rem_dur = 0;
    status = mp4MoofGetTimeFromSample(moof, tid, handle->movie->mvex,
                                      last_entry.traf_number - 1U,
                                      last_entry.trun_number - 1U,
                                      last_entry.sample_number - 1U,
                                      &rem_dur);
    if (status != MP4_OK) {
        return status;
    }

    uint32_t total_dur = last_entry.time_lo + rem_dur;
    int64_t file_size = mp4CbGetSize(handle->stream->file_handle);
    if (file_size == -1) {
        return MP4_ERR_IO;
    }

    while (next_off < (uint64_t)file_size) {
        status = mp4MoofFindOrRead(handle->movie, handle->stream, next_off, &moof, &next_off);
        if (status != MP4_OK) {
            return status;
        }
        status = mp4MoofGetTimeFromSample(moof, tid, handle->movie->mvex, 0, 0, 0, &rem_dur);
        if (status != MP4_OK) {
            return status;
        }
        total_dur += rem_dur;
    }

    *out_dur = total_dur;
    return MP4_OK;
}

/* 0x00b788 */
MP4_NOINLINE int32_t mp4MovieCalcDuration(Mp4Movie *movie, uint32_t *out_dur, int32_t allow_io)
{
    if (movie->mvhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (movie->mvex == NULL) {
        *out_dur = movie->mvhd.duration;
        return MP4_OK;
    }
    if (movie->mvex->mehd.size != 0) {
        *out_dur = movie->mvex->mehd.fragment_duration;
        return MP4_OK;
    }
    if (movie->track_count == 0) {
        *out_dur = 0;
        return MP4_OK;
    }

    uint32_t *track_durs = (uint32_t *)mp4CbCalloc(sizeof(uint32_t) * movie->track_count);
    if (track_durs == NULL) {
        return MP4_ERR_NOMEM;
    }
    uint32_t *pending_ids = (uint32_t *)mp4CbCalloc(sizeof(uint32_t) * movie->track_count);
    if (pending_ids == NULL) {
        mp4MemFree(track_durs);
        return MP4_ERR_NOMEM;
    }

    uint32_t pending_cnt = 0;
    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        if (trk != NULL) {
            uint32_t trk_dur = 0;
            int32_t status = mp4StblGetTotalDuration(&trk->mdia.minf.stbl, &trk_dur, 0);
            if (status != MP4_OK) {
                if (status != MP4_ERR_IO) {
                    mp4MemFree(track_durs);
                    mp4MemFree(pending_ids);
                    return status;
                }
                pending_ids[pending_cnt++] = idx;
            } else {
                uint32_t prod_hi = 0;
                uint32_t prod_lo = 0;
                uint32_t quot_hi = 0;
                uint32_t rem = 0;
                mp4MulU32ToU64(trk_dur, movie->mvhd.timescale, &prod_hi, &prod_lo);
                mp4DivU64ByU32(prod_hi, prod_lo, trk->mdia.mdhd.timescale,
                               &quot_hi, &track_durs[idx], &rem);
                if (quot_hi != 0) {
                    mp4MemFree(track_durs);
                    mp4MemFree(pending_ids);
                    return MP4_ERR_DUR_OVERFLOW;
                }
            }
        }
    }

    if (pending_cnt != 0 && allow_io == 0) {
        mp4MemFree(track_durs);
        mp4MemFree(pending_ids);
        return MP4_ERR_IO;
    }

    if (pending_cnt != 0) {
        for (;;) {
            int32_t all_eof = 1;
            for (uint32_t pi = 0; pi < pending_cnt; pi++) {
                Mp4Track *trk = mp4MovieGetTrackAt(movie, pending_ids[pi]);
                int32_t adv = mp4StblAdvanceOneFrag(&trk->mdia.minf.stbl);
                if (adv == MP4_OK) {
                    all_eof = 0;
                } else if (adv != MP4_ERR_EOF_FRAG) {
                    mp4MemFree(track_durs);
                    mp4MemFree(pending_ids);
                    return adv;
                }
            }
            if (all_eof != 0) {
                break;
            }
        }

        for (uint32_t pi = 0; pi < pending_cnt; pi++) {
            uint32_t trk_idx = pending_ids[pi];
            Mp4Track *trk = mp4MovieGetTrackAt(movie, trk_idx);
            uint32_t trk_dur = 0;
            int32_t status = mp4StblGetTotalDuration(&trk->mdia.minf.stbl, &trk_dur, 0);
            if (status != MP4_OK) {
                mp4MemFree(track_durs);
                mp4MemFree(pending_ids);
                return status;
            }
            uint32_t prod_hi = 0;
            uint32_t prod_lo = 0;
            uint32_t quot_hi = 0;
            uint32_t rem = 0;
            mp4MulU32ToU64(trk_dur, movie->mvhd.timescale, &prod_hi, &prod_lo);
            mp4DivU64ByU32(prod_hi, prod_lo, trk->mdia.mdhd.timescale,
                           &quot_hi, &track_durs[trk_idx], &rem);
            if (quot_hi != 0) {
                mp4MemFree(track_durs);
                mp4MemFree(pending_ids);
                return MP4_ERR_DUR_OVERFLOW;
            }
        }
    }

    uint32_t max_dur = track_durs[0];
    for (uint32_t idx = 1; idx < movie->track_count; idx++) {
        if (max_dur < track_durs[idx]) {
            max_dur = track_durs[idx];
        }
    }
    *out_dur = max_dur;
    mp4MemFree(track_durs);
    mp4MemFree(pending_ids);
    return MP4_OK;
}

/* 0x00bae8 */
MP4_NOINLINE int32_t mp4MulU32ToU64(uint32_t lhs, uint32_t rhs,
                                    uint32_t *out_hi, uint32_t *out_lo)
{
    uint32_t lhs_lo = lhs & 0xFFFFU;
    uint32_t lhs_hi = lhs >> 16;
    uint32_t rhs_lo = rhs & 0xFFFFU;
    uint32_t rhs_hi = rhs >> 16;

    uint32_t low_prod = lhs_lo * rhs_lo;
    uint32_t mid1 = lhs_hi * rhs_lo;
    uint32_t mid2 = lhs_lo * rhs_hi;
    uint32_t hi_prod = lhs_hi * rhs_hi;

    uint32_t mid_lo = (low_prod >> 16) + (mid1 & 0xFFFFU) + (mid2 & 0xFFFFU);
    uint32_t mid_hi = (mid1 >> 16) + (mid2 >> 16) + (hi_prod & 0xFFFFU);
    uint32_t carry = mid_hi + (mid_lo >> 16);

    *out_hi = (((hi_prod >> 16) + (carry >> 16)) << 16) + (carry & 0xFFFFU);
    *out_lo = (mid_lo << 16) + (low_prod & 0xFFFFU);
    return MP4_OK;
}
