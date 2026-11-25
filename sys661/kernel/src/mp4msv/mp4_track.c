#include "mp4msv_internal.h"

Mp4Track *mp4TrackCreate(Mp4Movie *movie, u32 handler_type)
{
    Mp4Track *track = (Mp4Track *)mp4Alloc(sizeof(Mp4Track));
    if (track == NULL)
        return NULL;
    mp4MemSet(track, 0, sizeof(*track));
    track->movie = movie;
    track->handler_type = handler_type;
    track->track_id = (movie->next_track_id != 0) ? movie->next_track_id++ : (++movie->track_count);
    track->timescale = 90000U;
    track->samples.stsc.valid = 1;

    if (movie->tracks == NULL) {
        movie->tracks = track;
    } else {
        Mp4Track *tail = movie->tracks;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = track;
    }
    movie->track_count++;
    return track;
}

void mp4TrackDestroy(Mp4Track *track)
{
    if (track == NULL)
        return;
    mp4CodecConfigFree(&track->codec);
    mp4ChunkListFree(track->samples.stsc.chunks);
    mp4ChunkListFree(track->samples.stts_chunks);
    mp4ChunkListFree(track->samples.ctts_chunks);
    mp4ChunkListFree(track->samples.stsz_chunks);
    mp4ChunkListFree(track->samples.stco_chunks);
    mp4ChunkListFree(track->samples.stss_chunks);
    mp4Free(track->edit_list);
    mp4Free(track->ref_tracks);
    mp4Free(track->extra_data);
    mp4Free(track);
}

Mp4Track *mp4TrackGetByIndex(Mp4Movie *movie, u32 index)
{
    if (movie == NULL)
        return NULL;
    Mp4Track *cur = movie->tracks;
    while (cur != NULL && index > 0) {
        cur = cur->next;
        index--;
    }
    return cur;
}

Mp4Track *mp4TrackGetById(Mp4Movie *movie, u32 track_id)
{
    if (movie == NULL)
        return NULL;
    Mp4Track *cur = movie->tracks;
    while (cur != NULL) {
        if (cur->track_id == track_id)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

int mp4MovieCreate(Mp4Handle *handle)
{
    if (handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (handle->movie != NULL)
        mp4MovieDestroy(handle);

    Mp4Movie *movie = (Mp4Movie *)mp4Alloc(sizeof(Mp4Movie));
    if (movie == NULL)
        return MP4_ERR_NOMEM;
    mp4MemSet(movie, 0, sizeof(*movie));
    movie->handle = handle;
    movie->timescale = 1000U;
    movie->next_track_id = 1U;
    movie->rate_fixed = 0x00010000U;
    movie->volume_fixed = 0x0100U;
    handle->movie = movie;
    return MP4_OK;
}

int mp4MovieDestroy(Mp4Handle *handle)
{
    if (handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4Movie *movie = handle->movie;
    if (movie == NULL)
        return MP4_OK;

    Mp4Track *tr = movie->tracks;
    while (tr != NULL) {
        Mp4Track *next = tr->next;
        mp4TrackDestroy(tr);
        tr = next;
    }

    Mp4UuidBox *uuid = movie->uuid_list;
    while (uuid != NULL) {
        Mp4UuidBox *next = uuid->next;
        mp4Free(uuid);
        uuid = next;
    }

    Mp4Fragment *frag = movie->fragments;
    while (frag != NULL) {
        Mp4Fragment *next = frag->next;
        mp4Free(frag);
        frag = next;
    }

    Mp4ProfileItem *prof = movie->profiles;
    while (prof != NULL) {
        Mp4ProfileItem *next = prof->next;
        mp4Free(prof);
        prof = next;
    }

    Mp4MetaItem *meta = movie->meta_list;
    while (meta != NULL) {
        Mp4MetaItem *next = meta->next;
        mp4Free(meta->data);
        mp4Free(meta);
        meta = next;
    }

    mp4Free(movie->ftyp);
    mp4Free(movie->fragment_index);
    mp4Free(movie);
    handle->movie = NULL;
    return MP4_OK;
}

static void mp4ParseStblBox(Mp4Track *track, Mp4IoStream *stream, u64 end_pos)
{
    while ((u64)mp4IoTell(stream) + 8ULL <= end_pos) {
        s64 pos = mp4IoTell(stream);
        if (pos < 0)
            break;
        u64 box_start = (u64)pos;
        u32 box_size = mp4IoReadU32(stream);
        u32 box_type = mp4IoReadU32(stream);
        if (box_size < 8U || box_start + box_size > end_pos)
            break;
        u64 box_end = box_start + box_size;

        if (box_type == MP4_BOX_STTS) {
            mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            u32 idx;
            for (idx = 0; idx < count && (u64)mp4IoTell(stream) + 8ULL <= box_end; idx++) {
                u32 row[2];
                row[0] = mp4IoReadU32(stream);
                row[1] = mp4IoReadU32(stream);
                mp4ChunkAppendRow(&track->samples.stts_chunks, 2, row);
                track->samples.stts_count++;
            }
        } else if (box_type == MP4_BOX_STSC) {
            mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            u32 idx;
            for (idx = 0; idx < count && (u64)mp4IoTell(stream) + 12ULL <= box_end; idx++) {
                u32 row[3];
                row[0] = mp4IoReadU32(stream);
                row[1] = mp4IoReadU32(stream);
                row[2] = mp4IoReadU32(stream);
                mp4ChunkAppendRow(&track->samples.stsc.chunks, 3, row);
                track->samples.stsc.entry_count++;
            }
            track->samples.stsc.first_entry = 1;
            track->samples.stsc.first_sample = 1;
            track->samples.stsc.valid = 1;
        } else if (box_type == MP4_BOX_STSZ) {
            mp4IoReadU32(stream);
            track->samples.stsz_fixed_size = mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            track->samples.stsz_count = count;
            if (track->samples.stsz_fixed_size == 0) {
                u32 idx;
                for (idx = 0; idx < count && (u64)mp4IoTell(stream) + 4ULL <= box_end; idx++) {
                    u32 sz = mp4IoReadU32(stream);
                    if (sz > track->samples.max_sample_size)
                        track->samples.max_sample_size = sz;
                    mp4ChunkAppendRow(&track->samples.stsz_chunks, 1, &sz);
                }
            } else {
                track->samples.max_sample_size = track->samples.stsz_fixed_size;
            }
        } else if (box_type == MP4_BOX_STCO) {
            mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            track->samples.stco_is_64bit = 0;
            u32 idx;
            for (idx = 0; idx < count && (u64)mp4IoTell(stream) + 4ULL <= box_end; idx++) {
                u32 off = mp4IoReadU32(stream);
                mp4ChunkAppendRow(&track->samples.stco_chunks, 1, &off);
                track->samples.stco_count++;
            }
        } else if (box_type == MP4_BOX_STSS) {
            mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            u32 idx;
            for (idx = 0; idx < count && (u64)mp4IoTell(stream) + 4ULL <= box_end; idx++) {
                u32 sync_num = mp4IoReadU32(stream);
                mp4ChunkAppendRow(&track->samples.stss_chunks, 1, &sync_num);
                track->samples.stss_count++;
            }
        } else if (box_type == MP4_BOX_STSD) {
            mp4IoReadU32(stream);
            u32 count = mp4IoReadU32(stream);
            if (count > 0 && (u64)mp4IoTell(stream) + 16ULL <= box_end) {
                mp4IoReadU32(stream);
                track->codec.format = mp4IoReadU32(stream);
            }
        }

        mp4IoSeek(stream, (s64)box_end, 0);
    }
}

int mp4ParseTrakBox(Mp4Movie *movie, Mp4IoStream *stream, u64 end_pos)
{
    Mp4Track *track = mp4TrackCreate(movie, 0);
    if (track == NULL)
        return MP4_ERR_NOMEM;

    while ((u64)mp4IoTell(stream) + 8ULL <= end_pos) {
        s64 pos = mp4IoTell(stream);
        if (pos < 0)
            break;
        u64 box_start = (u64)pos;
        u32 box_size = mp4IoReadU32(stream);
        u32 box_type = mp4IoReadU32(stream);
        if (box_size < 8U || box_start + box_size > end_pos)
            break;
        u64 box_end = box_start + box_size;

        if (box_type == MP4_BOX_TKHD) {
            u32 ver_flags = mp4IoReadU32(stream);
            u8 version = (u8)(ver_flags >> 24);
            track->flags = ver_flags & 0x00FFFFFFU;
            if (version == 1) {
                track->creation_time = (u32)mp4IoReadU64(stream);
                track->modification_time = (u32)mp4IoReadU64(stream);
                track->track_id = mp4IoReadU32(stream);
                mp4IoReadU32(stream);
                track->duration = mp4IoReadU64(stream);
            } else {
                track->creation_time = mp4IoReadU32(stream);
                track->modification_time = mp4IoReadU32(stream);
                track->track_id = mp4IoReadU32(stream);
                mp4IoReadU32(stream);
                track->duration = mp4IoReadU32(stream);
            }
        } else if (box_type == MP4_BOX_MDIA || box_type == MP4_BOX_MINF) {
            mp4ParseTrakBox(movie, stream, box_end);
        } else if (box_type == MP4_BOX_MDHD) {
            u32 ver_flags = mp4IoReadU32(stream);
            u8 version = (u8)(ver_flags >> 24);
            if (version == 1) {
                mp4IoReadU64(stream);
                mp4IoReadU64(stream);
                track->timescale = mp4IoReadU32(stream);
                track->duration = mp4IoReadU64(stream);
            } else {
                mp4IoReadU32(stream);
                mp4IoReadU32(stream);
                track->timescale = mp4IoReadU32(stream);
                track->duration = mp4IoReadU32(stream);
            }
            track->language = mp4IoReadU16(stream);
        } else if (box_type == MP4_BOX_HDLR) {
            mp4IoReadU32(stream);
            mp4IoReadU32(stream);
            track->handler_type = mp4IoReadU32(stream);
        } else if (box_type == MP4_BOX_STBL) {
            mp4ParseStblBox(track, stream, box_end);
        }

        mp4IoSeek(stream, (s64)box_end, 0);
    }
    return MP4_OK;
}
