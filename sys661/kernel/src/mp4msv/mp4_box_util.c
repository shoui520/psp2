#include "mp4msv_internal.h"

MP4_NOINLINE void mp4FreeMovie(Mp4Movie *movie)
{
    if (movie == NULL) {
        return;
    }
    if (movie->ftyp.size != 0 && movie->ftyp.compat_brands != NULL) {
        mp4MemFree(movie->ftyp.compat_brands);
    }
    if (movie->prof.size != 0) {
        if (movie->prof.fprf_list != NULL) {
            mp4MemFree(movie->prof.fprf_list);
        }
        if (movie->prof.aprf_list != NULL) {
            mp4MemFree(movie->prof.aprf_list);
        }
        if (movie->prof.vprf_list != NULL) {
            mp4MemFree(movie->prof.vprf_list);
        }
    }
    if (movie->lici.first_buf != NULL) {
        mp4MemFree(movie->lici.first_buf);
    }
    if (movie->lici.second_buf != NULL) {
        mp4MemFree(movie->lici.second_buf);
    }
    mp4FreeMoovChildren(movie);
    mp4MoofListFree(movie->moof_head);
    if (movie->uuid_head != NULL) {
        mp4FreeUuidList(movie->uuid_head);
    }
    mp4MemFree(movie);
}

MP4_NOINLINE void mp4FreeMoovChildren(Mp4Movie *movie)
{
    if (movie == NULL) {
        return;
    }
    if (movie->iods != NULL) {
        mp4FreeIods(movie->iods);
    }
    while (movie->track_count != 0) {
        mp4MovieRemoveTrackByIndex(movie, 0);
    }
    if (movie->usmt != NULL) {
        mp4FreeUsmt(movie->usmt);
    }
    if (movie->udta != NULL) {
        mp4FreeUdta(movie->udta);
    }
    if (movie->mvex != NULL) {
        mp4MvexFree(movie->mvex);
    }
}

MP4_NOINLINE void mp4FreeIods(Mp4IodsBox *iods)
{
    if (iods->payload != NULL) {
        mp4MemFree(iods->payload);
    }
    mp4MemFree(iods);
}

MP4_NOINLINE void mp4FreeChunkArray(Mp4ChunkArray *head)
{
    Mp4ChunkArray *cur = head;
    while (cur != NULL) {
        Mp4ChunkArray *next = cur->next;
        if (cur->words != NULL) {
            mp4MemFree(cur->words);
        }
        if (cur->extra_a != NULL) {
            mp4MemFree(cur->extra_a);
        }
        if (cur->wide_words != NULL) {
            mp4MemFree(cur->wide_words);
        }
        mp4MemFree(cur);
        cur = next;
    }
}

MP4_NOINLINE void mp4FreeSampleEntry(Mp4SampleEntry *entry)
{
    uint32_t idx;

    if (entry->codec_desc != NULL) {
        mp4MemFree(entry->codec_desc);
    }
    if (entry->esds != NULL) {
        mp4FreeEsds(entry->esds);
    }
    if (entry->avcc != NULL) {
        mp4FreeAvcc(entry->avcc);
    }
    if (entry->sinf != NULL) {
        mp4FreeSinf(entry->sinf);
    }
    for (idx = 0; idx < 3; idx++) {
        if (entry->extra[idx] != NULL) {
            mp4FreeExtraBox(entry->extra[idx]);
        }
    }
    mp4MemFree(entry);
}

MP4_NOINLINE void mp4FreeExtraBox(Mp4ExtraBox *box)
{
    if (box->payload != NULL) {
        mp4MemFree(box->payload);
    }
    mp4MemFree(box);
}

MP4_NOINLINE void mp4FreeEsds(Mp4EsdsBox *esds)
{
    if (esds->payload != NULL) {
        mp4MemFree(esds->payload);
    }
    mp4MemFree(esds);
}

MP4_NOINLINE void mp4FreeAvcc(Mp4AvccBox *avcc)
{
    Mp4AvccNal nal;
    uint32_t count = 0;
    uint32_t idx;

    if (avcc->sps_array != NULL) {
        mp4DynArrayCount(avcc->sps_array, &count);
        for (idx = 0; idx < count; idx++) {
            if (mp4DynArrayGet(avcc->sps_array, idx, &nal) == 0 && nal.nal_bytes != NULL) {
                mp4MemFree(nal.nal_bytes);
            }
        }
        mp4DynArrayDestroy(avcc->sps_array);
    }
    if (avcc->pps_array != NULL) {
        mp4DynArrayCount(avcc->pps_array, &count);
        for (idx = 0; idx < count; idx++) {
            if (mp4DynArrayGet(avcc->pps_array, idx, &nal) == 0 && nal.nal_bytes != NULL) {
                mp4MemFree(nal.nal_bytes);
            }
        }
        mp4DynArrayDestroy(avcc->pps_array);
    }
    mp4MemFree(avcc);
}

MP4_NOINLINE void mp4FreeSinf(Mp4SinfBox *sinf)
{
    if (sinf != NULL) {
        mp4MemFree(sinf->payload);
    }
    mp4MemFree(sinf);
}

MP4_NOINLINE void mp4FreeUsmt(Mp4UsmtBox *usmt)
{
    if (usmt != NULL) {
        if (usmt->mtdf.entries != NULL) {
            mp4MemFree(usmt->mtdf.entries);
        }
        if (usmt->mtdt.entries != NULL) {
            uint32_t idx;
            for (idx = 0; idx < usmt->mtdt.entry_count; idx++) {
                mp4MtdtExtFree(&usmt->mtdt.entries[idx].ext);
            }
            mp4MemFree(usmt->mtdt.entries);
        }
        {
            Mp4DrefEntry *dref_cur = usmt->mtsm.dinf.dref.head;
            while (dref_cur != NULL) {
                Mp4DrefEntry *next = dref_cur->next;
                if (dref_cur->location != NULL) {
                    mp4MemFree(dref_cur->location);
                }
                mp4MemFree(dref_cur);
                dref_cur = next;
            }
        }
        {
            Mp4SampleEntry *stsd_cur = usmt->mtsm.stsd.head;
            while (stsd_cur != NULL) {
                Mp4SampleEntry *next = stsd_cur->next;
                mp4FreeSampleEntry(stsd_cur);
                stsd_cur = next;
            }
        }
        mp4MemFree(usmt->mtsm.mcst.entries);
    }
    mp4MemFree(usmt);
}

MP4_NOINLINE void mp4FreeUdta(Mp4UdtaBox *udta)
{
    if (udta != NULL) {
        uint32_t idx;
        for (idx = 0; idx < udta->item_count; idx++) {
            mp4MemFree(udta->items[idx].payload);
        }
        mp4MemFree(udta->items);
    }
    mp4MemFree(udta);
}

MP4_NOINLINE void mp4FreeUuidList(Mp4UuidNode *head)
{
    Mp4UuidNode *cur = head;
    while (cur != NULL) {
        Mp4UuidNode *next = cur->next;
        mp4MemFree(cur);
        cur = next;
    }
}

MP4_NOINLINE int32_t mp4StszAppend(Mp4StszBox *stsz, uint32_t sample_size)
{
    uint32_t val = sample_size;

    if ((stsz->const_sample_size == sample_size) & (sample_size != 0U)) {
        stsz->sample_count++;
        return 0;
    }
    if ((stsz->sample_count == 0U) & (sample_size != 0U)) {
        stsz->sample_count++;
        stsz->const_sample_size = sample_size;
        return 0;
    }
    if (stsz->const_sample_size != 0) {
        if (mp4StszExpandConst(stsz) != 0) {
            return MP4_ERR_NOMEM;
        }
    }
    if (mp4ChunkArrayAppendU32(&stsz->entries, 1U, &val, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    stsz->sample_count++;
    return 0;
}

MP4_NOINLINE int32_t mp4StszExpandConst(Mp4StszBox *stsz)
{
    uint32_t val = stsz->const_sample_size;
    uint32_t idx;

    for (idx = 0; idx < stsz->sample_count; idx++) {
        if (mp4ChunkArrayAppendU32(&stsz->entries, 1U, &val, 100U) != 0) {
            mp4FreeChunkArray(stsz->entries);
            stsz->entries = NULL;
            return MP4_ERR_NOMEM;
        }
    }
    stsz->const_sample_size = 0;
    return 0;
}

MP4_NOINLINE void mp4CalcMoovSize(Mp4Movie *movie)
{
    uint32_t idx;

    if (movie == NULL || movie->size == 0) {
        return;
    }
    movie->size = 8;
    if (movie->mvhd.size != 0) {
        movie->mvhd.size = 108;
        movie->size = 116;
    }
    if (movie->iods != NULL) {
        movie->size += movie->iods->size;
    }
    for (idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *track = mp4MovieGetTrackAt(movie, idx);
        if (track != NULL) {
            mp4CalcTrakSize(track);
            movie->size += mp4MovieGetTrackAt(movie, idx)->size;
        }
    }
    if (movie->mvex != NULL) {
        mp4MvexCalcSize(movie->mvex);
        movie->size += movie->mvex->size;
    }
    if (movie->usmt != NULL) {
        mp4CalcUsmtSize(movie->usmt);
        movie->size += movie->usmt->size;
    }
}

MP4_NOINLINE void mp4CalcTrakSize(Mp4Track *track)
{
    track->size = 8;
    if (track->tkhd.size != 0) {
        track->tkhd.size = 92;
        track->size = 100;
    }
    if (track->edts.size != 0) {
        mp4CalcEdtsSize(&track->edts);
        track->size += track->edts.size;
    }
    if (track->mdia.size != 0) {
        mp4CalcMdiaSize(&track->mdia);
        track->size += track->mdia.size;
    }
    if (track->tref != NULL) {
        mp4CalcTrefSize(track->tref);
        track->size += track->tref->size;
    }
    if (track->usmt != NULL) {
        mp4CalcUsmtSize(track->usmt);
        track->size += track->usmt->size;
    }
}

MP4_NOINLINE void mp4CalcTrefSize(Mp4TrefBox *tref)
{
    tref->size = 8;
    if (tref->sync_ref != NULL) {
        tref->sync_ref->size = 4U * (uint32_t)tref->sync_ref->count + 8U;
        tref->size += tref->sync_ref->size;
    }
    if (tref->mpod_ref != NULL) {
        tref->mpod_ref->size = 4U * (uint32_t)tref->mpod_ref->count + 8U;
        tref->size += tref->mpod_ref->size;
    }
}

MP4_NOINLINE void mp4CalcEdtsSize(Mp4EdtsBox *edts)
{
    edts->size = 8;
    if (edts->elst.size != 0) {
        mp4CalcElstSize(&edts->elst);
        edts->size += edts->elst.size;
    }
}

MP4_NOINLINE void mp4CalcElstSize(Mp4ElstBox *elst)
{
    elst->size = 12U * elst->entry_count + 16U;
}

MP4_NOINLINE void mp4CalcMdiaSize(Mp4MdiaBox *mdia)
{
    mdia->size = 8;
    if (mdia->minf.size != 0) {
        mp4CalcMinfSize(&mdia->minf);
        mdia->size += mdia->minf.size;
    }
    if (mdia->mdhd.size != 0) {
        mdia->mdhd.size = 32;
        mdia->size += 32;
    }
    if (mdia->hdlr.size != 0) {
        uint32_t name_len = mp4CbStrlen(mdia->hdlr.name);
        mdia->hdlr.size = name_len + 33U;
        mdia->size += mdia->hdlr.size;
    }
}

MP4_NOINLINE void mp4CalcMinfSize(Mp4MinfBox *minf)
{
    minf->size = 8;
    if (minf->vmhd.size != 0) {
        minf->vmhd.size = 20;
        minf->size = 28;
    }
    if (minf->smhd.size != 0) {
        minf->smhd.size = 16;
        minf->size += 16;
    }
    if (minf->nmhd.size != 0) {
        minf->nmhd.size = 12;
        minf->size += 12;
    }
    if (minf->dinf.size != 0) {
        mp4CalcDinfSize(&minf->dinf);
        minf->size += minf->dinf.size;
    }
    if (minf->active_stbl != NULL) {
        mp4CalcStblBoxSize(minf->active_stbl);
        minf->size += minf->active_stbl->size;
    } else if (minf->stbl.size != 0) {
        mp4CalcStblBoxSize(&minf->stbl);
        minf->size += minf->stbl.size;
    }
}

MP4_NOINLINE int32_t mp4CalcStblBoxSize(Mp4Stbl *stbl)
{
    int32_t ret = mp4StblRecalcSizes(stbl);
    if (stbl->stsd.size != 0) {
        mp4CalcStsdSizeWrap(&stbl->stsd, 0, stbl->stsd.entry_count, stbl->track->movie->self->handle->stream);
        stbl->size += stbl->stsd.size;
    }
    return ret;
}

MP4_NOINLINE void mp4CalcStsdSizeWrap(Mp4StsdBox *stsd, uint32_t start_idx, uint32_t count, Mp4IoStream *stream)
{
    mp4CalcStsdSize(stsd, start_idx, count, stream->io_mode);
}

MP4_NOINLINE void mp4CalcStsdSize(Mp4StsdBox *stsd, uint32_t start_idx, uint32_t count, uint32_t io_mode)
{
    uint32_t end_idx = start_idx + count;
    uint32_t idx;
    (void)io_mode;

    stsd->size = 16;
    for (idx = start_idx; idx < end_idx; idx++) {
        Mp4SampleEntry *entry = NULL;
        if (mp4StsdGetEntry(stsd, idx + 1U, &entry) == 0) {
            uint32_t extra_idx;
            if (entry->handler_type == MP4_TAG_ODSM) {
                entry->size = 16;
            } else if (entry->handler_type == MP4_TAG_SOUN) {
                Mp4AudioDesc *aud = (Mp4AudioDesc *)entry->codec_desc;
                entry->size = (aud->version == 0) ? 36U : 52U;
            } else if (entry->handler_type == MP4_TAG_VIDE) {
                entry->size = 86;
            } else if (entry->handler_type == MP4_TAG_GRPH) {
                entry->size = 40;
            }
            for (extra_idx = 0; extra_idx < 3; extra_idx++) {
                if (entry->extra[extra_idx] != NULL) {
                    entry->size += entry->extra[extra_idx]->size;
                }
            }
            if (entry->esds != NULL) {
                entry->size += entry->esds->size;
            }
            if (entry->avcc != NULL) {
                mp4CalcAvccSize(entry->avcc);
                entry->size += entry->avcc->size;
            }
            stsd->size += entry->size;
        }
    }
}

MP4_NOINLINE void mp4CalcAvccSize(Mp4AvccBox *avcc)
{
    Mp4AvccNal nal;
    uint32_t count = 0;
    uint32_t idx;

    avcc->size = 15;
    if (avcc->sps_array != NULL) {
        mp4DynArrayCount(avcc->sps_array, &count);
        for (idx = 0; idx < count; idx++) {
            if (mp4DynArrayGet(avcc->sps_array, idx, &nal) == 0) {
                avcc->size += (uint32_t)nal.length + 2U;
            }
        }
        avcc->sps_count = (uint8_t)count;
    }
    if (avcc->pps_array != NULL) {
        mp4DynArrayCount(avcc->pps_array, &count);
        for (idx = 0; idx < count; idx++) {
            if (mp4DynArrayGet(avcc->pps_array, idx, &nal) == 0) {
                avcc->size += (uint32_t)nal.length + 2U;
            }
        }
        avcc->pps_count = (uint8_t)count;
    }
}

MP4_NOINLINE void mp4CalcUsmtSize(Mp4UsmtBox *usmt)
{
    usmt->size = 24;
    if (usmt->mtdf.size != 0) {
        mp4CalcMtdfSize(&usmt->mtdf);
        usmt->size += usmt->mtdf.size;
    }
    if (usmt->mtdt.size != 0) {
        mp4CalcMtdtSize(&usmt->mtdt);
        usmt->size += usmt->mtdt.size;
    }
    if (usmt->mtsm.size != 0) {
        uint32_t mcst_size;
        uint32_t mtsm_total;
        usmt->mtsm.size = 40;
        mp4CalcDinfSize(&usmt->mtsm.dinf);
        usmt->mtsm.size += usmt->mtsm.dinf.size;
        mp4CalcStsdSize(&usmt->mtsm.stsd, 0, usmt->mtsm.stsd.entry_count, 0);
        mcst_size = 20U * usmt->mtsm.mcst.entry_count + 16U;
        mtsm_total = usmt->mtsm.size + usmt->mtsm.stsd.size + mcst_size;
        usmt->mtsm.mcst.size = mcst_size;
        usmt->mtsm.size = mtsm_total;
        usmt->size += mtsm_total;
    }
}

MP4_NOINLINE void mp4CalcMtdfSize(Mp4MtdfBox *mtdf)
{
    uint32_t idx;
    uint32_t total = 10;

    mtdf->size = 10;
    for (idx = 0; idx < mtdf->entry_count; idx++) {
        total += mtdf->entries[idx].size;
    }
    mtdf->size = total;
}

MP4_NOINLINE void mp4CalcMtdtSize(Mp4MtdtBox *mtdt)
{
    uint32_t idx;

    mtdt->size = 10;
    for (idx = 0; idx < mtdt->entry_count; idx++) {
        if (mtdt->entries[idx].encoding == 257U) {
            mtdt->entries[idx].size = (uint16_t)(12U * mtdt->entries[idx].ext.item_count + 22U);
        }
        mtdt->size += mtdt->entries[idx].size;
    }
}

MP4_NOINLINE void mp4CalcDinfSize(Mp4DinfBox *dinf)
{
    dinf->size = 8;
    if (dinf->dref.size != 0) {
        mp4CalcDrefSize(&dinf->dref);
        dinf->size += dinf->dref.size;
    }
}

MP4_NOINLINE void mp4CalcDrefSize(Mp4DrefBox *dref)
{
    uint32_t idx;

    dref->size = 16;
    for (idx = 0; idx < dref->entry_count; idx++) {
        Mp4DrefEntry *entry = NULL;
        if (mp4DrefGetEntry((uint16_t)(idx + 1U), dref, &entry) == 0) {
            mp4CalcDrefEntrySize(entry);
            dref->size += entry->size;
        }
    }
}

MP4_NOINLINE void mp4CalcDrefEntrySize(Mp4DrefEntry *entry)
{
    entry->size = 12;
    if (entry->flags[2] == 0 && entry->tag == MP4_TAG_URL && entry->location != NULL) {
        entry->size += mp4CbStrlen(entry->location) + 1U;
    }
}

MP4_NOINLINE void mp4CalcProfSize(Mp4ProfBox *prof)
{
    uint32_t idx;

    if (prof == NULL || prof->size == 0) {
        return;
    }
    prof->size = 32;
    for (idx = 0; idx < prof->fprf_count; idx++) {
        prof->size += prof->fprf_list[idx].size;
    }
    for (idx = 0; idx < prof->aprf_count; idx++) {
        prof->size += prof->aprf_list[idx].size;
    }
    for (idx = 0; idx < prof->vprf_count; idx++) {
        prof->size += prof->vprf_list[idx].size;
    }
}

MP4_NOINLINE int32_t mp4ChunkArrayAccumFind(Mp4ChunkArray *head, uint32_t *rem_io, uint32_t col, uint32_t *out_idx)
{
    uint32_t idx;
    for (idx = 0; ; idx++) {
        uint32_t val = 0;
        int32_t ret = mp4ChunkArrayGetU32(head, idx, col, &val);
        if (val >= *rem_io) {
            *out_idx = idx;
            return 0;
        }
        if (ret != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        *rem_io -= val;
    }
}

MP4_NOINLINE void mp4FreeExtraBoxAlt(Mp4ExtraBox *box)
{
    if (box->payload != NULL) {
        mp4MemFree(box->payload);
    }
    mp4MemFree(box);
}

MP4_NOINLINE void mp4FreeTrack(Mp4Track *track)
{
    Mp4MinfBox *minf = &track->mdia.minf;
    Mp4DrefEntry *dref_cur = minf->dinf.dref.head;

    while (dref_cur != NULL) {
        Mp4DrefEntry *next = dref_cur->next;
        if (dref_cur->location != NULL) {
            mp4MemFree(dref_cur->location);
        }
        mp4MemFree(dref_cur);
        dref_cur = next;
    }

    if (minf->active_stbl != NULL && minf->active_stbl != &minf->stbl) {
        Mp4Stbl *alt = minf->active_stbl;
        Mp4SampleEntry *entry_cur;
        mp4StblDetachFragCtx(alt);
        if (alt->stsd.entry_count != 0) {
            alt->stsd.head = NULL;
        }
        alt->stsd.entry_count = 0;
        mp4StblDetachFragCtx(alt);
        entry_cur = alt->stsd.head;
        while (entry_cur != NULL) {
            Mp4SampleEntry *next = entry_cur->next;
            mp4FreeSampleEntry(entry_cur);
            entry_cur = next;
        }
        if (alt->stts.entries != NULL) {
            mp4FreeChunkArray(alt->stts.entries);
        }
        if (alt->stss.entries != NULL) {
            mp4FreeChunkArray(alt->stss.entries);
        }
        if (alt->stsc.entries != NULL) {
            mp4FreeChunkArray(alt->stsc.entries);
        }
        if (alt->stsz.entries != NULL) {
            mp4FreeChunkArray(alt->stsz.entries);
        }
        if (alt->stco.entries != NULL) {
            mp4FreeChunkArray(alt->stco.entries);
        }
        if (alt->ctts.entries != NULL) {
            mp4FreeChunkArray(alt->ctts.entries);
        }
        mp4MemFree(alt);
    }
    minf->active_stbl = NULL;

    if (minf->stbl.cache != NULL) {
        mp4CacheDestroy(minf->stbl.cache);
    }
    mp4StblDetachFragCtx(&minf->stbl);
    {
        Mp4SampleEntry *entry_cur = minf->stbl.stsd.head;
        while (entry_cur != NULL) {
            Mp4SampleEntry *next = entry_cur->next;
            mp4FreeSampleEntry(entry_cur);
            entry_cur = next;
        }
    }
    if (minf->stbl.stts.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.stts.entries);
    }
    if (minf->stbl.stss.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.stss.entries);
    }
    if (minf->stbl.stsc.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.stsc.entries);
    }
    if (minf->stbl.stsz.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.stsz.entries);
    }
    if (minf->stbl.stco.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.stco.entries);
    }
    if (minf->stbl.ctts.entries != NULL) {
        mp4FreeChunkArray(minf->stbl.ctts.entries);
    }
    if (track->edts.elst.entries != NULL) {
        mp4FreeChunkArray(track->edts.elst.entries);
    }

    if (track->tref != NULL) {
        if (track->tref->sync_ref != NULL) {
            if (track->tref->sync_ref->track_ids != NULL) {
                mp4MemFree(track->tref->sync_ref->track_ids);
            }
            mp4MemFree(track->tref->sync_ref);
        }
        if (track->tref->mpod_ref != NULL) {
            if (track->tref->mpod_ref->track_ids != NULL) {
                mp4MemFree(track->tref->mpod_ref->track_ids);
            }
            mp4MemFree(track->tref->mpod_ref);
        }
        mp4MemFree(track->tref);
    }
    if (track->usmt != NULL) {
        mp4FreeUsmt(track->usmt);
    }
    if (track->udta != NULL) {
        mp4FreeUdta(track->udta);
    }
    mp4MemFree(track);
}

MP4_NOINLINE void mp4FreeTref(Mp4TrefBox *tref)
{
    Mp4TrefTypeBox *sub = tref->sync_ref;
    if (sub != NULL) {
        if (sub->track_ids != NULL) {
            mp4MemFree(sub->track_ids);
        }
        mp4MemFree(sub);
    }
    sub = tref->mpod_ref;
    if (sub != NULL) {
        if (sub->track_ids != NULL) {
            mp4MemFree(sub->track_ids);
        }
        mp4MemFree(sub);
    }
    mp4MemFree(tref);
}

MP4_NOINLINE void mp4FreeTrefType(Mp4TrefTypeBox *tref_type)
{
    if (tref_type->track_ids != NULL) {
        mp4MemFree(tref_type->track_ids);
    }
    mp4MemFree(tref_type);
}

MP4_NOINLINE void mp4FreeStblData(Mp4Stbl *stbl)
{
    Mp4SampleEntry *cur;

    mp4StblDetachFragCtx(stbl);
    cur = stbl->stsd.head;
    while (cur != NULL) {
        Mp4SampleEntry *next = cur->next;
        mp4FreeSampleEntry(cur);
        cur = next;
    }
    if (stbl->stts.entries != NULL) {
        mp4FreeChunkArray(stbl->stts.entries);
    }
    if (stbl->stss.entries != NULL) {
        mp4FreeChunkArray(stbl->stss.entries);
    }
    if (stbl->stsc.entries != NULL) {
        mp4FreeChunkArray(stbl->stsc.entries);
    }
    if (stbl->stsz.entries != NULL) {
        mp4FreeChunkArray(stbl->stsz.entries);
    }
    if (stbl->stco.entries != NULL) {
        mp4FreeChunkArray(stbl->stco.entries);
    }
    if (stbl->ctts.entries != NULL) {
        mp4FreeChunkArray(stbl->ctts.entries);
    }
}

MP4_NOINLINE int32_t mp4StcoAppend(Mp4StcoBox *stco, uint64_t offset_val)
{
    uint64_t val = offset_val;
    if (mp4ChunkArrayAppendU64(&stco->entries, 1U, &val, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    stco->entry_count++;
    return 0;
}

MP4_NOINLINE int32_t mp4StscAppend(Mp4StscBox *stsc, const uint32_t *triple)
{
    if (mp4ChunkArrayAppendU32(&stsc->entries, 3U, triple, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    stsc->entry_count++;
    return 0;
}

MP4_NOINLINE int32_t mp4SttsAppend(Mp4SttsBox *stts, const uint32_t *pair)
{
    if (mp4ChunkArrayAppendU32(&stts->entries, 2U, pair, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    stts->entry_count++;
    return 0;
}

MP4_NOINLINE int32_t mp4CttsAppend(Mp4CttsBox *ctts, const uint32_t *pair)
{
    if (mp4ChunkArrayAppendU32(&ctts->entries, 2U, pair, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    ctts->entry_count++;
    return 0;
}

MP4_NOINLINE int32_t mp4StssAppend(Mp4StssBox *stss, uint32_t sample_num)
{
    uint32_t val = sample_num;
    if (mp4ChunkArrayAppendU32(&stss->entries, 1U, &val, 100U) != 0) {
        return MP4_ERR_NOMEM;
    }
    stss->entry_count++;
    return 0;
}

MP4_NOINLINE Mp4Fsm *mp4StblGetFsm(Mp4Stbl *stbl)
{
    return stbl->track->movie->self->handle->stream->fsm;
}
