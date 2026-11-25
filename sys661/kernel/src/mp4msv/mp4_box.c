#include "mp4msv_internal.h"

Mp4Config g_mp4_config = { 100, 1, 1, 3, 0 };
Mp4OsCallbacks g_mp4_os_cbs;

const u32 g_mp4_state_table[360] = {
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0x00000004U, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000007U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000006U, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000005U, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000002U, 0x00000001U, 0x00000002U,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000008U, 0x00000001U, 0x00000008U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000001U, 0x00000001U, 0x00000001U,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000001U, 0x00000001U, 0x00000001U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
};

const u32 g_mp4_err_table[28] = {
    0xFFFFFFFFU, 0xFFFFFFFFU, 0x00000003U, 0x00000003U, 0x00000007U, 0x00000007U, 0x00000007U, 0x00000007U,
    0x00000009U, 0x00000009U, 0x00021798U, 0x00021798U, 0x00021778U, 0x000217CCU, 0x00021798U, 0x00021798U,
    0x00021798U, 0x00021798U, 0x00021798U, 0x00021798U, 0x00021798U, 0x000217F0U, 0x00021798U, 0x00021798U,
    0x00021798U, 0x00021798U, 0x00021778U, 0x000217F8U,
};

int mp4StateCreate(Mp4StateMachine **out_fsm)
{
    Mp4StateMachine *fsm = (Mp4StateMachine *)mp4Alloc(sizeof(Mp4StateMachine));
    if (fsm == NULL)
        return MP4_ERR_NOMEM;
    fsm->cur_state = 0;
    fsm->pending_state = 0xFFFFFFFFU;
    *out_fsm = fsm;
    return MP4_OK;
}

void mp4StateDestroy(Mp4StateMachine *fsm)
{
    mp4Free(fsm);
}

int mp4StateCheck(Mp4StateMachine *fsm, u32 action)
{
    if (fsm == NULL || action >= 18U || fsm->cur_state >= 10U)
        return MP4_ERR_BAD_STATE;
    u32 idx = (action * 10U + fsm->cur_state) * 2U;
    if (g_mp4_state_table[idx] == 0)
        return MP4_ERR_BAD_STATE;
    fsm->pending_state = g_mp4_state_table[idx + 1U];
    return MP4_OK;
}

void mp4StateCommit(Mp4StateMachine *fsm)
{
    if (fsm != NULL && fsm->pending_state != 0xFFFFFFFFU) {
        fsm->cur_state = fsm->pending_state;
        fsm->pending_state = 0xFFFFFFFFU;
    }
}

static int mp4ParseFtypBox(Mp4Movie *movie, Mp4IoStream *stream, u64 end_pos)
{
    s64 cur = mp4IoTell(stream);
    if (cur < 0 || (u64)cur + 8ULL > end_pos)
        return MP4_ERR_PARSE;

    Mp4FtypBox *ftyp = movie->ftyp;
    if (ftyp == NULL) {
        ftyp = (Mp4FtypBox *)mp4Alloc(sizeof(Mp4FtypBox));
        if (ftyp == NULL)
            return MP4_ERR_NOMEM;
        mp4MemSet(ftyp, 0, sizeof(*ftyp));
        movie->ftyp = ftyp;
    }

    ftyp->major_brand = mp4IoReadU32(stream);
    ftyp->minor_version = mp4IoReadU32(stream);
    ftyp->compat_count = 0;

    while ((u64)mp4IoTell(stream) + 4ULL <= end_pos) {
        u32 brand = mp4IoReadU32(stream);
        if (ftyp->compat_count < 16U)
            ftyp->compat_brands[ftyp->compat_count++] = brand;
    }
    return MP4_OK;
}

static int mp4ParseMvhdBox(Mp4Movie *movie, Mp4IoStream *stream, u64 end_pos)
{
    u32 ver_flags = mp4IoReadU32(stream);
    u8 version = (u8)(ver_flags >> 24);
    if (version == 1) {
        movie->creation_time = (u32)mp4IoReadU64(stream);
        movie->modification_time = (u32)mp4IoReadU64(stream);
        movie->timescale = mp4IoReadU32(stream);
        movie->duration = mp4IoReadU64(stream);
    } else {
        movie->creation_time = mp4IoReadU32(stream);
        movie->modification_time = mp4IoReadU32(stream);
        movie->timescale = mp4IoReadU32(stream);
        movie->duration = mp4IoReadU32(stream);
    }
    movie->rate_fixed = mp4IoReadU32(stream);
    movie->volume_fixed = mp4IoReadU16(stream);
    if (end_pos >= 4ULL) {
        mp4IoSeek(stream, (s64)(end_pos - 4ULL), 0);
        movie->next_track_id = mp4IoReadU32(stream);
    }
    return MP4_OK;
}

static int mp4ParseUuidBox(Mp4Movie *movie, Mp4IoStream *stream, u64 box_start, u32 box_size)
{
    Mp4UuidBox *node = (Mp4UuidBox *)mp4Alloc(sizeof(Mp4UuidBox));
    if (node == NULL)
        return MP4_ERR_NOMEM;
    mp4MemSet(node, 0, sizeof(*node));
    node->size = box_size;
    node->file_offset = box_start;
    int idx;
    for (idx = 0; idx < 4; idx++)
        node->uuid[idx] = mp4IoReadU32(stream);

    if (movie->uuid_list == NULL) {
        movie->uuid_list = node;
    } else {
        Mp4UuidBox *tail = movie->uuid_list;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = node;
    }
    return MP4_OK;
}

static int mp4ParseMoovBox(Mp4Movie *movie, Mp4IoStream *stream, u64 end_pos)
{
    while ((u64)mp4IoTell(stream) + 8ULL <= end_pos) {
        s64 pos = mp4IoTell(stream);
        if (pos < 0)
            return MP4_ERR_PARSE;
        u64 box_start = (u64)pos;
        u32 box_size = mp4IoReadU32(stream);
        u32 box_type = mp4IoReadU32(stream);
        u64 full_size = box_size;
        if (box_size == 1U)
            full_size = mp4IoReadU64(stream);
        else if (box_size == 0U)
            full_size = end_pos - box_start;

        if (full_size < 8ULL || box_start + full_size > end_pos)
            break;
        u64 box_end = box_start + full_size;

        if (box_type == MP4_BOX_MVHD)
            mp4ParseMvhdBox(movie, stream, box_end);
        else if (box_type == MP4_BOX_TRAK)
            mp4ParseTrakBox(movie, stream, box_end);
        else if (box_type == MP4_BOX_UUID && full_size >= 24ULL)
            mp4ParseUuidBox(movie, stream, box_start, (u32)full_size);

        mp4IoSeek(stream, (s64)box_end, 0);
    }
    return MP4_OK;
}

int mp4ParseTopLevel(Mp4Movie *movie, u32 parse_mask, Mp4IoStream *stream)
{
    s64 total_size = mp4IoGetSize(stream);
    if (total_size < 8)
        return MP4_OK;

    u64 file_end = (u64)total_size;
    while ((u64)mp4IoTell(stream) + 8ULL <= file_end) {
        s64 pos = mp4IoTell(stream);
        if (pos < 0)
            return MP4_ERR_PARSE;
        u64 box_start = (u64)pos;
        u32 box_size = mp4IoReadU32(stream);
        if (box_size == 0U)
            break;
        if (box_size < 8U && box_size != 1U)
            return MP4_ERR_PARSE;
        u32 box_type = mp4IoReadU32(stream);
        u64 full_size = box_size;
        if (box_size == 1U)
            full_size = mp4IoReadU64(stream);
        if (box_start + full_size > file_end)
            full_size = file_end - box_start;
        u64 box_end = box_start + full_size;

        if (box_type == MP4_BOX_FTYP && (parse_mask & 1U) != 0) {
            int ret = mp4ParseFtypBox(movie, stream, box_end);
            if (ret != MP4_OK)
                return ret;
            parse_mask &= ~1U;
        } else if (box_type == MP4_BOX_MOOV && (parse_mask & 2U) != 0) {
            int ret = mp4ParseMoovBox(movie, stream, box_end);
            if (ret != MP4_OK)
                return ret;
            parse_mask &= ~2U;
        } else if (box_type == MP4_BOX_MDAT) {
            movie->mdat_offset = box_start;
            movie->mdat_size = full_size;
        } else if (box_type == MP4_BOX_UUID && full_size >= 24ULL) {
            mp4ParseUuidBox(movie, stream, box_start, (u32)full_size);
        }

        if ((stream->error_flags & 0xE0000000U) != 0)
            return MP4_ERR_PARSE;
        mp4IoSeek(stream, (s64)box_end, 0);
        if (parse_mask == 0)
            break;
    }
    return MP4_OK;
}
