/*
 * Sony FF4 MP4 muxer: Movie Fragment Box (moof), Movie Fragment Header (mfhd),
 * Track Fragment Box (traf), Track Fragment Header (tfhd), Track Run Box (trun),
 * and fragment cache I/O.
 */
#include "ff4_internal.h"

int32_t ff4_stsc_cursor_peek_next(const Ff4StscCursor *cur, uint32_t *outFirstChunk)
{
	const Ff4StblRunBlock *blk;
	const uint32_t *entry;

	if (cur == NULL || outFirstChunk == NULL)
		return FF4_ERR_INVALID_ARG;

	blk = cur->blk;
	entry = cur->curEntry + 3;
	if (blk != NULL && blk->count <= cur->entryInBlk + 1u) {
		blk = blk->next;
		if (blk != NULL)
			entry = (const uint32_t *)blk->entries;
	}
	*outFirstChunk = (blk != NULL) ? entry[0] : (cur->totalChunks + 1u);
	return FF4_OK;
}

int32_t ff4_frag_cursor_init(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag, Ff4FragCursor *outCur)
{
	Ff4FragCursor cur;
	int32_t ret;

	if (sess == NULL || frag == NULL || outCur == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &cur, 0, sizeof(cur));
	ret = ff4_count_stco_entries(frag->stcoHead, &cur.chunkCount);
	if (ret != FF4_OK)
		return ret;

	cur.stts.blk = frag->sttsHead;
	if (cur.stts.blk != NULL)
		cur.stts.curEntry = (uint32_t *)cur.stts.blk->entries;

	cur.ctts.blk = frag->cttsHead;
	if (cur.ctts.blk != NULL)
		cur.ctts.curEntry = (uint32_t *)cur.ctts.blk->entries;

	cur.stsz.fixedSize = frag->fixedSampleSize;
	cur.stsz.blk = frag->stszHead;
	if (cur.stsz.blk != NULL)
		cur.stsz.curEntry = (uint32_t *)cur.stsz.blk->entries;

	cur.stss.hasSync = frag->hasSyncSample;
	ret = ff4_count_stss_entries(frag->stssHead, &cur.stss.totalSync);
	if (ret != FF4_OK)
		return ret;

	cur.stss.blk = frag->stssHead;
	if (cur.stss.blk != NULL)
		cur.stss.curEntry = (uint32_t *)cur.stss.blk->entries;

	cur.stco.blk = frag->stcoHead;
	if (cur.stco.blk != NULL)
		cur.stco.curEntry = (uint64_t *)cur.stco.blk->entries;

	cur.stsc.totalChunks = cur.chunkCount;
	cur.stsc.blk = frag->stscHead;
	if (cur.stsc.blk != NULL)
		cur.stsc.curEntry = (uint32_t *)cur.stsc.blk->entries;

	ret = ff4_stsc_cursor_peek_next(&cur.stsc, &cur.stsc.nextFirstChunk);
	if (ret == FF4_OK)
		ff4_memcpy(sess, outCur, sizeof(*outCur), &cur, sizeof(cur));
	return ret;
}

int32_t ff4_stts_cursor_next(int32_t advance, Ff4SttsCursor *cur, uint32_t *outVal)
{
	uint32_t runCount;
	uint32_t val;

	if (cur == NULL || outVal == NULL)
		return FF4_ERR_INVALID_ARG;
	if (cur->blk == NULL)
		return FF4_ERR_NOT_FOUND;

	runCount = cur->curEntry[0];
	val = cur->curEntry[1];
	if (advance != 0) {
		cur->sampleInRun++;
		if (cur->sampleInRun >= runCount) {
			cur->sampleInRun = 0;
			cur->entryInBlk++;
			cur->curEntry += 2;
			if (cur->entryInBlk >= cur->blk->count) {
				cur->entryInBlk = 0;
				cur->blk = cur->blk->next;
				if (cur->blk != NULL)
					cur->curEntry = (uint32_t *)cur->blk->entries;
			}
		}
	}
	*outVal = val;
	return FF4_OK;
}

int32_t ff4_ctts_cursor_next(int32_t advance, Ff4SttsCursor *cur, uint32_t *outVal)
{
	uint32_t runCount;
	uint32_t val;

	if (cur == NULL || outVal == NULL)
		return FF4_ERR_INVALID_ARG;
	if (cur->blk == NULL)
		return FF4_ERR_NOT_FOUND;

	runCount = cur->curEntry[0];
	val = cur->curEntry[1];
	if (advance != 0) {
		cur->sampleInRun++;
		if (cur->sampleInRun >= runCount) {
			cur->sampleInRun = 0;
			cur->entryInBlk++;
			cur->curEntry += 2;
			if (cur->entryInBlk >= cur->blk->count) {
				cur->entryInBlk = 0;
				cur->blk = cur->blk->next;
				if (cur->blk != NULL)
					cur->curEntry = (uint32_t *)cur->blk->entries;
			}
		}
	}
	*outVal = val;
	return FF4_OK;
}

int32_t ff4_stco_cursor_next(int32_t advance, Ff4StcoCursor *cur, uint64_t *outOff)
{
	uint64_t val;

	if (cur == NULL || outOff == NULL)
		return FF4_ERR_INVALID_ARG;
	if (cur->blk == NULL)
		return FF4_ERR_NOT_FOUND;

	val = *cur->curEntry;
	if (advance != 0) {
		cur->entryInBlk++;
		cur->curEntry++;
		if (cur->entryInBlk >= cur->blk->count) {
			cur->entryInBlk = 0;
			cur->blk = cur->blk->next;
			if (cur->blk != NULL)
				cur->curEntry = (uint64_t *)cur->blk->entries;
		}
	}
	*outOff = val;
	return FF4_OK;
}

int32_t ff4_stsc_cursor_next(int32_t advance, Ff4StscCursor *cur, uint32_t *outSamples, uint32_t *outDesc)
{
	uint32_t samples;
	uint32_t desc;
	int32_t ret = FF4_OK;

	if (cur == NULL || outSamples == NULL || outDesc == NULL)
		return FF4_ERR_INVALID_ARG;
	if (cur->blk == NULL)
		return FF4_ERR_NOT_FOUND;

	samples = cur->curEntry[1];
	desc = cur->curEntry[2];
	if (advance != 0) {
		cur->curChunk++;
		if (cur->curChunk + 1u >= cur->nextFirstChunk) {
			cur->entryInBlk++;
			cur->curEntry += 3;
			if (cur->entryInBlk >= cur->blk->count) {
				cur->entryInBlk = 0;
				cur->blk = cur->blk->next;
				if (cur->blk != NULL)
					cur->curEntry = (uint32_t *)cur->blk->entries;
			}
			ret = ff4_stsc_cursor_peek_next(cur, &cur->nextFirstChunk);
		}
	}
	if (ret == FF4_OK) {
		*outSamples = samples;
		*outDesc = desc;
	}
	return ret;
}

int32_t ff4_stsz_cursor_next(int32_t advance, Ff4StszCursor *cur, uint32_t *outSize)
{
	uint32_t sz;

	if (cur == NULL || outSize == NULL)
		return FF4_ERR_INVALID_ARG;

	sz = cur->fixedSize;
	if (sz == 0) {
		if (cur->blk == NULL)
			return FF4_ERR_NOT_FOUND;
		sz = *cur->curEntry;
		if (advance != 0) {
			cur->entryInBlk++;
			cur->curEntry++;
			if (cur->entryInBlk >= cur->blk->count) {
				cur->entryInBlk = 0;
				cur->blk = cur->blk->next;
				if (cur->blk != NULL)
					cur->curEntry = (uint32_t *)cur->blk->entries;
			}
		}
	}
	*outSize = sz;
	return FF4_OK;
}

int32_t ff4_stss_cursor_read(int32_t advance, Ff4StssCursor *cur, uint32_t *outSample)
{
	uint32_t val = 0;

	if (cur == NULL || outSample == NULL)
		return FF4_ERR_INVALID_ARG;

	if (cur->blk != NULL) {
		val = *cur->curEntry;
		if (advance != 0) {
			cur->entryInBlk++;
			cur->curEntry++;
			if (cur->entryInBlk >= cur->blk->count) {
				cur->entryInBlk = 0;
				cur->blk = cur->blk->next;
				if (cur->blk != NULL)
					cur->curEntry = (uint32_t *)cur->blk->entries;
			}
		}
	}
	*outSample = val;
	return FF4_OK;
}

int32_t ff4_stss_cursor_flags(uint32_t sampleNum1, Ff4StssCursor *cur, uint32_t *outFlags)
{
	uint32_t syncSample = 0;
	uint32_t flags = 0x10000u;
	int32_t ret = FF4_OK;

	if (cur == NULL || outFlags == NULL)
		return FF4_ERR_INVALID_ARG;

	if (cur->totalSync == 0 && cur->hasSync != 0) {
		*outFlags = 0;
		return FF4_OK;
	}
	if (cur->totalSync == 0) {
		*outFlags = flags;
		return FF4_OK;
	}

	while (ret == FF4_OK) {
		ret = ff4_stss_cursor_read(0, cur, &syncSample);
		if (ret != FF4_OK)
			return ret;
		if (sampleNum1 == syncSample) {
			*outFlags = 0;
			return FF4_OK;
		}
		if (sampleNum1 < syncSample || syncSample == 0) {
			*outFlags = flags;
			return FF4_OK;
		}
		ret = ff4_stss_cursor_read(1, cur, &syncSample);
	}
	return ret;
}

int32_t ff4_build_trun_data(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			    const Ff4TfhdPlan *tfhd, Ff4FragCursor *cursor, Ff4TrunData *outData)
{
	Ff4TrunData *trun = NULL;
	Ff4FragCursor saved;
	uint64_t chunkOff = 0;
	uint32_t sampleCount = 0;
	uint32_t sampleDesc = 0;
	uint32_t flags = 0;
	uint32_t i;
	uint32_t entryStride;
	uint64_t tableBytes;
	uint8_t *buf;
	uint8_t *p;
	int32_t dataOff;
	int32_t ret = FF4_OK;

	if (sess == NULL || scratch == NULL || opts == NULL || tfhd == NULL ||
	    cursor == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;

	trun = (Ff4TrunData *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trun));
	if (trun == NULL)
		return FF4_ERR_NO_MEM;

	ff4_memcpy(sess, &saved, sizeof(saved), cursor, sizeof(saved));
	ret = ff4_stco_cursor_next(1, &cursor->stco, &chunkOff);
	if (ret == FF4_OK)
		ret = ff4_stsc_cursor_next(1, &cursor->stsc, &sampleCount, &sampleDesc);

	if (ret == FF4_OK) {
		if (tfhd->baseDataOffset != chunkOff || opts->tfraMode == 0)
			flags |= 1u;

		for (i = 0; i < sampleCount; i++) {
			uint32_t dur = 0, sz = 0, sflags = 0, cto = 0;
			ret = ff4_stts_cursor_next(1, &cursor->stts, &dur);
			if (ret != FF4_OK)
				break;
			if (tfhd->defSampleDur != dur)
				flags |= 0x100u;

			ret = ff4_stsz_cursor_next(1, &cursor->stsz, &sz);
			if (ret != FF4_OK)
				break;
			if (tfhd->defSampleSize != sz)
				flags |= 0x200u;

			ret = ff4_stss_cursor_flags(cursor->sampleIdx + 1u, &cursor->stss, &sflags);
			if (ret != FF4_OK)
				break;
			if (i != 0) {
				if (tfhd->defSampleFlags != sflags)
					flags |= 0x400u;
			} else if (tfhd->defSampleFlags != sflags) {
				flags |= 4u;
			}

			ret = ff4_ctts_cursor_next(1, &cursor->ctts, &cto);
			if (ret != FF4_OK)
				break;
			if (cto != 0)
				flags |= 0x800u;

			cursor->sampleIdx++;
		}
	}

	if (ret == FF4_OK) {
		if ((flags & 0x400u) != 0)
			flags &= ~4u;

		entryStride = (((flags & 0x100u) != 0) ? 4u : 0u) +
			      (((flags & 0x200u) != 0) ? 4u : 0u) +
			      (((flags & 0x400u) != 0) ? 4u : 0u) +
			      (((flags & 0x800u) != 0) ? 4u : 0u);
		tableBytes = (uint64_t)sampleCount * (uint64_t)entryStride;

		if (scratch->size >= tableBytes) {
			buf = scratch->data;
		} else {
			buf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, tableBytes);
			trun->tempAlloc = buf;
			if (buf == NULL)
				ret = FF4_ERR_NO_MEM;
		}
	}

	if (ret == FF4_OK) {
		trun->tablePtr = buf;
		p = buf;
		ff4_memcpy(sess, cursor, sizeof(*cursor), &saved, sizeof(saved));
		ret = ff4_stco_cursor_next(1, &cursor->stco, &chunkOff);
		if (ret == FF4_OK)
			ret = ff4_stsc_cursor_next(1, &cursor->stsc, &sampleCount, &sampleDesc);
		if (ret == FF4_OK) {
			for (i = 0; i < sampleCount; i++) {
				uint32_t dur = 0, sz = 0, sflags = 0, cto = 0;
				ret = ff4_stts_cursor_next(1, &cursor->stts, &dur);
				if (ret != FF4_OK)
					break;
				if ((flags & 0x100u) != 0) {
					ff4_put_be32(p, dur);
					p += 4;
				}
				ret = ff4_stsz_cursor_next(1, &cursor->stsz, &sz);
				if (ret != FF4_OK)
					break;
				if ((flags & 0x200u) != 0) {
					ff4_put_be32(p, sz);
					p += 4;
				}
				ret = ff4_stss_cursor_flags(cursor->sampleIdx + 1u, &cursor->stss, &sflags);
				if (ret != FF4_OK)
					break;
				if (i == 0)
					trun->firstSampleFlags = sflags;
				if ((flags & 0x400u) != 0) {
					ff4_put_be32(p, sflags);
					p += 4;
				}
				ret = ff4_ctts_cursor_next(1, &cursor->ctts, &cto);
				if (ret != FF4_OK)
					break;
				if ((flags & 0x800u) != 0) {
					ff4_put_be32(p, cto);
					p += 4;
				}
				cursor->sampleIdx++;
			}
		}
	}

	if (ret == FF4_OK) {
		trun->flags = flags;
		trun->sampleCount = sampleCount;
		if (chunkOff >= tfhd->baseDataOffset) {
			uint64_t diff = chunkOff - tfhd->baseDataOffset;
			if (diff > 0x7FFFFFFFu)
				ret = FF4_ERR_OVERFLOW;
			else
				dataOff = (int32_t)diff;
		} else {
			uint64_t diff = tfhd->baseDataOffset - chunkOff;
			if (diff > 0x80000000u)
				ret = FF4_ERR_OVERFLOW;
			else
				dataOff = (int32_t)(chunkOff - tfhd->baseDataOffset);
		}
	}

	if (ret == FF4_OK) {
		trun->dataOffset = dataOff;
		ff4_memcpy(sess, outData, sizeof(*outData), trun, sizeof(*trun));
		trun->tablePtr = NULL;
		trun->tempAlloc = NULL;
	}

	if (trun != NULL)
		ff4_pool_free(sess, trun->tempAlloc);
	ff4_pool_free(sess, trun);
	return ret;
}

int32_t ff4_write_trun_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4TfhdPlan *tfhd,
			       Ff4FragCursor *cursor, uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4TrunData *trun = NULL;
	uint8_t hdr[16];
	uint32_t hdrLen;
	uint64_t total = 0;
	uint32_t entryStride;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || tfhd == NULL || cursor == NULL)
		return FF4_ERR_INVALID_ARG;

	trun = (Ff4TrunData *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trun));
	if (trun == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_build_trun_data(sess, scratch, opts, tfhd, cursor, trun);
	if (ret == FF4_OK)
		ret = ff4_box_seek(sess, fileCtx, pos, 0);

	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 0, trun->flags & 0xFFFFFFu);
		ff4_put_be32(hdr + 4, trun->sampleCount);
		hdrLen = 8;
		if ((trun->flags & 1u) != 0) {
			ff4_put_be32(hdr + hdrLen, (uint32_t)trun->dataOffset);
			hdrLen += 4;
		}
		if ((trun->flags & 4u) != 0) {
			ff4_put_be32(hdr + hdrLen, trun->firstSampleFlags);
			hdrLen += 4;
		}
		ret = ff4_box_write(sess, fileCtx, hdr, hdrLen, NULL, &total);
	}

	if (ret == FF4_OK) {
		entryStride = (((trun->flags & 0x100u) != 0) ? 4u : 0u) +
			      (((trun->flags & 0x200u) != 0) ? 4u : 0u) +
			      (((trun->flags & 0x400u) != 0) ? 4u : 0u) +
			      (((trun->flags & 0x800u) != 0) ? 4u : 0u);
		ret = ff4_box_write(sess, fileCtx, trun->tablePtr,
				    (uint64_t)trun->sampleCount * (uint64_t)entryStride,
				    NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}

	ff4_pool_free(sess, trun->tempAlloc);
	ff4_pool_free(sess, trun);
	return ret;
}

int32_t ff4_plan_trun(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      const Ff4TfhdPlan *tfhd, Ff4FragCursor *cursor, Ff4TrunPlan *outPlan)
{
	Ff4TrunPlan tmp;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || tfhd == NULL ||
	    cursor == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	ret = ff4_write_trun_payload(sess, scratch, opts, NULL, 0, tfhd, cursor,
				     &payloadSize, NULL);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_free_traf_plan_list(Ff4SessionImpl *sess, Ff4TrafPlan *head)
{
	Ff4TrafPlan *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4TrafPlan *next = cur->next;
		Ff4TrunPlan *tr = cur->trunHead;
		while (tr != NULL) {
			Ff4TrunPlan *tnext = tr->next;
			ff4_pool_free(sess, tr);
			tr = tnext;
		}
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_moof_plan(Ff4SessionImpl *sess, Ff4MoofPlan *plan)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan != NULL) {
		ff4_free_traf_plan_list(sess, plan->trafHead);
		ff4_pool_free(sess, plan);
	}
	return FF4_OK;
}

int32_t ff4_get_fragment_count(const Ff4MovieModel *moov, uint32_t *outCount)
{
	const Ff4TrackModel *track;
	uint32_t maxFrags = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	if (moov == NULL)
		return FF4_ERR_INVALID_STATE;

	for (track = moov->trackHead; track != NULL; track = track->next) {
		if (track->fragTail != NULL) {
			const Ff4SampleTableModel *f = track->fragHead;
			uint32_t cnt = 0;
			while (f != NULL) {
				f = f->next;
				if (f == NULL)
					break;
				cnt++;
			}
			if (maxFrags < cnt)
				maxFrags = cnt;
		}
	}

	if (maxFrags != 0) {
		if (moov->fragCount != maxFrags)
			return FF4_ERR_INTERNAL;
	} else {
		maxFrags = moov->fragCount;
	}

	*outCount = maxFrags;
	return FF4_OK;
}

int32_t ff4_write_trun_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TfhdPlan *tfhd,
			   const Ff4TrunPlan *plan, Ff4FragCursor *cursor,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || tfhd == NULL ||
	    plan == NULL || cursor == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TRUN,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_trun_payload(sess, scratch, opts, fileCtx, pos + total,
					     tfhd, cursor, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_tfhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4TfhdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[32];
	uint32_t len;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf + 0, plan->flags & 0xFFFFFFu);
	ff4_put_be32(buf + 4, plan->trackId);
	len = 8;
	if ((plan->flags & 1u) != 0) {
		ff4_put_be64(buf + len, plan->baseDataOffset);
		len += 8;
	}
	if ((plan->flags & 2u) != 0) {
		ff4_put_be32(buf + len, plan->sampleDescIdx);
		len += 4;
	}
	if ((plan->flags & 8u) != 0) {
		ff4_put_be32(buf + len, plan->defSampleDur);
		len += 4;
	}
	if ((plan->flags & 0x10u) != 0) {
		ff4_put_be32(buf + len, plan->defSampleSize);
		len += 4;
	}
	if ((plan->flags & 0x20u) != 0) {
		ff4_put_be32(buf + len, plan->defSampleFlags);
		len += 4;
	}

	ret = ff4_box_write(sess, fileCtx, buf, len, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_tfhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TfhdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TFHD,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_tfhd_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_traf_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4TrafPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4FragCursor cursor;
	const Ff4TrunPlan *tr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_tfhd_box(sess, opts, fileCtx, pos, &plan->tfhd, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_frag_cursor_init(sess, plan->frag, &cursor);

	if (ret == FF4_OK) {
		for (tr = plan->trunHead; tr != NULL; tr = tr->next) {
			ret = ff4_write_trun_box(sess, scratch, opts, fileCtx, pos + total,
						 &plan->tfhd, tr, &cursor, NULL, &total);
			if (ret != FF4_OK)
				return ret;
		}
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_traf_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TrafPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TRAF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_traf_payload(sess, scratch, opts, fileCtx, pos + total, plan,
					     NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mfhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *seqNum, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[8];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || seqNum == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf + 4, *seqNum);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mfhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const uint32_t *seqNum,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || seqNum == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, 8u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MFHD, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mfhd_payload(sess, fileCtx, pos + total, seqNum, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_moof_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4MoofPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TrafPlan *traf;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_mfhd_box(sess, opts, fileCtx, pos, &plan->seqNumber, NULL, &total);
	if (ret == FF4_OK) {
		for (traf = plan->trafHead; traf != NULL; traf = traf->next) {
			ret = ff4_write_traf_box(sess, scratch, opts, fileCtx, pos + total,
						 traf, NULL, &total);
			if (ret != FF4_OK)
				return ret;
		}
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_plan_tfhd(Ff4SessionImpl *sess, uint32_t fragIdx, const Ff4FormatOpts *opts,
		      Ff4TrackModel *track, Ff4TfhdPlan *outPlan)
{
	Ff4TfhdPlan tmp;
	Ff4SampleTableModel *frag = NULL;
	uint64_t payloadSize = 0;
	uint32_t flags = 1u;
	uint32_t val;
	int32_t ret;

	if (sess == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
	if (ret != FF4_OK)
		return ret;
	if (frag == NULL)
		return FF4_ERR_NOT_FOUND;

	tmp.trackId = track->trackId;
	if ((frag->tfhdFlags & 1u) != 0) {
		tmp.baseDataOffset = frag->baseDataOffset;
	} else if (frag->stcoHead != NULL && frag->stcoHead->entries != NULL) {
		tmp.baseDataOffset = ((const uint64_t *)frag->stcoHead->entries)[0];
	}

	val = track->trexDescIdx;
	if ((frag->tfhdFlags & 2u) != 0)
		val = frag->sampleDescIdx;
	else if (frag->stscHead != NULL && frag->stscHead->entries != NULL)
		val = ((const uint32_t *)frag->stscHead->entries)[2];
	tmp.sampleDescIdx = val;
	if (val != track->trexDescIdx)
		flags |= 2u;

	val = track->trexSampleDur;
	if ((frag->tfhdFlags & 8u) != 0)
		val = frag->defSampleDur;
	else if (frag->sttsHead != NULL && frag->sttsHead->entries != NULL)
		val = ((const uint32_t *)frag->sttsHead->entries)[1];
	tmp.defSampleDur = val;
	if (val != track->trexSampleDur)
		flags |= 8u;

	if ((frag->tfhdFlags & 0x10u) != 0) {
		val = frag->defSampleSize;
	} else if (frag->fixedSampleSize != 0) {
		val = frag->fixedSampleSize;
	} else if (frag->stszHead != NULL && frag->stszHead->entries != NULL) {
		val = ((const uint32_t *)frag->stszHead->entries)[0];
	} else {
		val = 0;
	}
	tmp.defSampleSize = val;
	if (val != track->trexSampleSize)
		flags |= 0x10u;

	if ((frag->tfhdFlags & 0x20u) != 0) {
		val = frag->defSampleFlags;
	} else if (frag->stssHead == NULL && frag->hasSyncSample != 0) {
		val = 0;
	} else {
		val = 0x10000u;
	}
	tmp.defSampleFlags = val;
	if (val != track->trexSampleFlags)
		flags |= 0x20u;

	tmp.flags = flags;
	ret = ff4_write_tfhd_payload(sess, NULL, 0, &tmp, NULL, &payloadSize);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_plan_traf(Ff4SessionImpl *sess, Ff4Buffer *scratch, uint32_t fragIdx,
		      const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4SampleTableModel *frag, Ff4TrafPlan *outPlan)
{
	Ff4TrafPlan *plan = NULL;
	Ff4TrunPlan *trun = NULL;
	Ff4FragCursor cursor;
	uint64_t payloadSize = 0;
	uint32_t i;
	int32_t ret = FF4_OK;

	if (sess == NULL || scratch == NULL || opts == NULL || track == NULL ||
	    frag == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan = (Ff4TrafPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_frag_cursor_init(sess, frag, &cursor);
	if (ret == FF4_OK)
		ret = ff4_plan_tfhd(sess, fragIdx, opts, track, &plan->tfhd);

	if (ret == FF4_OK) {
		for (i = 0; i < cursor.chunkCount; i++) {
			trun = (Ff4TrunPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trun));
			if (trun == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_trun(sess, scratch, opts, &plan->tfhd, &cursor, trun);
			if (ret != FF4_OK) {
				ff4_pool_free(sess, trun);
				trun = NULL;
				break;
			}
			if (plan->trunHead != NULL)
				plan->trunTail->next = trun;
			else
				plan->trunHead = trun;
			plan->trunTail = trun;
			trun = NULL;
		}
	}

	if (ret == FF4_OK) {
		plan->frag = frag;
		plan->next = NULL;
		ret = ff4_write_traf_payload(sess, scratch, opts, NULL, 0, plan, NULL, &payloadSize);
		if (ret == FF4_OK) {
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
			if (ret == FF4_OK) {
				ff4_memcpy(sess, outPlan, sizeof(*outPlan), plan, sizeof(*plan));
				plan->trunHead = NULL;
				plan->trunTail = NULL;
			}
		}
	}

	ff4_free_traf_plan_list(sess, plan);
	return ret;
}

int32_t ff4_set_mfhd_seq(Ff4SessionImpl *sess, uint32_t seqNum, uint32_t *outSeq)
{
	if (sess == NULL || seqNum == 0 || outSeq == NULL)
		return FF4_ERR_INVALID_ARG;
	*outSeq = seqNum;
	return FF4_OK;
}

int32_t ff4_plan_moof(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      uint32_t fragIdx, Ff4MovieModel *moov, Ff4MoofPlan *outPlan)
{
	Ff4MoofPlan tmp;
	Ff4TrafPlan *traf = NULL;
	Ff4TrackModel *track;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	if (sess == NULL || scratch == NULL || opts == NULL || moov == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_set_mfhd_seq(sess, fragIdx, &tmp.seqNumber);
	if (ret == FF4_OK) {
		for (track = moov->trackHead; track != NULL; track = track->next) {
			Ff4SampleTableModel *frag = NULL;
			uint32_t chunkCount = 0;

			if (track->fragTail == NULL)
				break;
			ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
			if (ret != FF4_OK)
				break;
			if (frag == NULL) {
				ret = FF4_ERR_NOT_FOUND;
				break;
			}
			ret = ff4_count_stco_entries(frag->stcoHead, &chunkCount);
			if (ret != FF4_OK)
				break;
			if (chunkCount != 0) {
				traf = (Ff4TrafPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								     sizeof(*traf));
				if (traf == NULL) {
					ret = FF4_ERR_NO_MEM;
					break;
				}
				ret = ff4_plan_traf(sess, scratch, fragIdx, opts, track, frag, traf);
				if (ret != FF4_OK)
					break;
				if (tmp.trafHead != NULL)
					tmp.trafTail->next = traf;
				else
					tmp.trafHead = traf;
				tmp.trafTail = traf;
				traf = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_moof_payload(sess, scratch, opts, NULL, 0, &tmp, NULL, &payloadSize);
		if (ret == FF4_OK) {
			ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
			if (ret == FF4_OK) {
				ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
				tmp.trafHead = NULL;
				tmp.trafTail = NULL;
			}
		}
	}

	ff4_free_traf_plan_list(sess, traf);
	ff4_free_traf_plan_list(sess, tmp.trafHead);
	return ret;
}

int32_t ff4_write_moof_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4MoofPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MOOF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_moof_payload(sess, scratch, opts, fileCtx, pos + total, plan,
					     NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_begin_movie_fragment(Ff4SessionImpl *sess, Ff4WriterModels *models)
{
	uint32_t trackCount = 0;
	uint32_t i;
	int32_t ret;

	if (sess == NULL || models == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
	if (ret != FF4_OK)
		return ret;

	for (i = 1; i <= trackCount; i++) {
		Ff4TrackModel *track = NULL;
		ret = ff4_get_track_at(models->moov->trackHead, i, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_INVALID_STATE;
		if (track->fragTail == NULL) {
			ret = ff4_append_fragment_node(sess, &track->trexMask);
			if (ret != FF4_OK)
				return ret;
		}
		ret = ff4_append_fragment_node(sess, &track->trexMask);
		if (ret != FF4_OK)
			return ret;
	}

	models->moov->fragCount++;
	return FF4_OK;
}

int32_t ff4_query_fragment_state(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx,
				 int32_t *outAllHaveChunks, int32_t *outNoPendingStsc,
				 int32_t *outNotLastFrag, int32_t *outHasAnySamples)
{
	uint32_t fragCount = 0;
	uint32_t trackCount = 0;
	uint32_t i;
	int32_t allHaveChunks = 1;
	int32_t noPendingStsc = 1;
	int32_t notLastFrag = 1;
	int32_t hasAnySamples = 0;
	int32_t ret;

	if (sess == NULL || models == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_fragment_count(models->moov, &fragCount);
	if (ret != FF4_OK)
		return ret;

	if (fragIdx == fragCount) {
		notLastFrag = 0;
	} else if (fragCount < fragIdx + 1u) {
		if (fragCount != 0)
			return FF4_ERR_NOT_FOUND;
		notLastFrag = 0;
	}

	ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
	if (ret != FF4_OK)
		return ret;

	for (i = 1; i <= trackCount; i++) {
		Ff4TrackModel *track = NULL;
		Ff4SampleTableModel *frag = NULL;
		ret = ff4_get_track_at(models->moov->trackHead, i, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_INVALID_STATE;
		if (track->fragTail != NULL && track->fragTail->stscChunkCount != 0)
			noPendingStsc = 0;
		ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
		if (ret != FF4_OK)
			return ret;
		if (frag != NULL && frag->totalSamples != 0) {
			hasAnySamples = 1;
			if (frag->stcoHead == NULL)
				allHaveChunks = 0;
		}
	}

	if (outAllHaveChunks != NULL)
		*outAllHaveChunks = allHaveChunks;
	if (outNoPendingStsc != NULL)
		*outNoPendingStsc = noPendingStsc;
	if (outNotLastFrag != NULL)
		*outNotLastFrag = notLastFrag;
	if (outHasAnySamples != NULL)
		*outHasAnySamples = hasAnySamples;
	return FF4_OK;
}

int32_t ff4_clear_fragment_samples(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx)
{
	uint32_t startFrag, endFrag;
	uint32_t trackCount = 0;
	uint32_t f, t;
	int32_t ret;

	if (sess == NULL || models == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	if (fragIdx != 0) {
		int32_t notLast = 0;
		startFrag = fragIdx;
		endFrag = fragIdx + 1u;
		ret = ff4_query_fragment_state(sess, models, fragIdx, NULL, NULL, &notLast, NULL);
		if (ret != FF4_OK)
			return ret;
		if (notLast == 0)
			return FF4_ERR_INVALID_STATE;
	} else {
		int32_t hasSamples = 0;
		endFrag = 0;
		startFrag = 1;
		ret = ff4_get_fragment_count(models->moov, &endFrag);
		if (ret != FF4_OK)
			return ret;
		if (endFrag <= 1u)
			return FF4_ERR_INVALID_STATE;
		ret = ff4_query_fragment_state(sess, models, endFrag, NULL, NULL, NULL, &hasSamples);
		if (ret != FF4_OK)
			return ret;
		if (hasSamples != 0)
			return FF4_ERR_INVALID_STATE;
	}

	ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
	if (ret != FF4_OK)
		return ret;

	for (f = startFrag; f < endFrag; f++) {
		for (t = 1; t <= trackCount; t++) {
			Ff4TrackModel *track = NULL;
			Ff4SampleTableModel *frag = NULL;
			ret = ff4_get_track_at(models->moov->trackHead, t, &track);
			if (ret != FF4_OK)
				return ret;
			if (track == NULL)
				return FF4_ERR_INVALID_STATE;
			ret = ff4_get_fragment_at(track->fragHead, f, &frag);
			if (ret != FF4_OK)
				return ret;
			if (frag != NULL)
				ff4_clear_fragment_node(sess, frag);
		}
	}
	return FF4_OK;
}

int32_t ff4_write_fragment_cache(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t fragIdx,
				 void *fileCtx, uint64_t pos, uint64_t *outWritten)
{
	int32_t allHaveChunks = 0;
	int32_t notLast = 0;
	uint32_t trackCount = 0;
	uint32_t i;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || models == NULL || fragIdx == 0 || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_query_fragment_state(sess, models, fragIdx, &allHaveChunks, NULL, &notLast, NULL);
	if (ret != FF4_OK)
		return ret;
	if (notLast == 0 || allHaveChunks == 0)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
	if (ret != FF4_OK)
		return ret;

	for (i = 1; i <= trackCount; i++) {
		Ff4TrackModel *track = NULL;
		Ff4SampleTableModel *frag = NULL;
		if (i == 1u) {
			ret = ff4_write_cache_header(sess, fileCtx, pos, 0, fragIdx, NULL, &total);
			if (ret != FF4_OK)
				return ret;
		}
		ret = ff4_get_track_at(models->moov->trackHead, i, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_INVALID_STATE;
		ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
		if (ret != FF4_OK)
			return ret;
		if (frag == NULL)
			return FF4_ERR_NOT_FOUND;
		ret = ff4_write_cache_fragment(sess, fileCtx, pos + total, frag, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	ret = ff4_write_cache_header(sess, fileCtx, pos, total, fragIdx, NULL, NULL);
	if (ret == FF4_OK)
		*outWritten = total;
	return ret;
}

int32_t ff4_read_fragment_cache(Ff4SessionImpl *sess, Ff4Buffer *scratch, Ff4WriterModels *models,
				uint32_t fragIdx, void *fileCtx, uint64_t pos, uint64_t availSize)
{
	Ff4BufStream *stream = NULL;
	int32_t notLast = 0;
	uint32_t trackCount = 0;
	uint32_t i;
	uint64_t consumed = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || scratch == NULL || models == NULL || fileCtx == NULL ||
	    fragIdx == 0 || availSize == 0) {
		ret = FF4_ERR_INVALID_ARG;
	} else if (models->moov == NULL) {
		ret = FF4_ERR_INVALID_STATE;
	} else {
		ret = ff4_query_fragment_state(sess, models, fragIdx, NULL, NULL, &notLast, NULL);
		if (ret == FF4_OK) {
			if (notLast == 0) {
				ret = FF4_ERR_INVALID_STATE;
			} else {
				stream = (Ff4BufStream *)ff4_pool_alloc(sess, FF4_POOL_META,
									sizeof(*stream));
				if (stream == NULL)
					ret = FF4_ERR_NO_MEM;
			}
		}
		if (ret == FF4_OK)
			ret = ff4_stream_open(sess, fileCtx, pos, scratch->data,
					      scratch->size, stream);
		if (ret == FF4_OK)
			ret = ff4_read_cache_header(sess, stream, pos, availSize, fragIdx,
						    NULL, &consumed);
		if (ret == FF4_OK)
			ret = ff4_clear_fragment_samples(sess, models, fragIdx);
		if (ret == FF4_OK)
			ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
		if (ret == FF4_OK) {
			for (i = 1; i <= trackCount; i++) {
				Ff4TrackModel *track = NULL;
				Ff4SampleTableModel *frag = NULL;
				ret = ff4_get_track_at(models->moov->trackHead, i, &track);
				if (ret != FF4_OK)
					break;
				if (track == NULL) {
					ret = FF4_ERR_INVALID_STATE;
					break;
				}
				ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
				if (ret != FF4_OK)
					break;
				if (frag == NULL) {
					ret = FF4_ERR_NOT_FOUND;
					break;
				}
				if (availSize < consumed) {
					ret = FF4_ERR_FORMAT;
					break;
				}
				ret = ff4_read_cache_fragment(sess, stream, pos + consumed,
							      availSize - consumed, frag,
							      NULL, &consumed);
				if (ret != FF4_OK)
					break;
			}
		}
	}

	if (ret != FF4_OK)
		ff4_clear_fragment_samples(sess, models, fragIdx);
	ff4_pool_free(sess, stream);
	return ret;
}

int32_t ff4_set_fragment_base_offset(Ff4WriterModels *models, uint32_t fragIdx, uint64_t baseOff)
{
	uint32_t trackCount = 0;
	uint32_t i;
	int32_t ret;

	if (models == NULL || fragIdx == 0)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_count_tracks(models->moov->trackHead, &trackCount);
	if (ret != FF4_OK)
		return ret;
	if (trackCount == 0)
		return FF4_ERR_INVALID_STATE;

	for (i = 1; i <= trackCount; i++) {
		Ff4TrackModel *track = NULL;
		Ff4SampleTableModel *frag = NULL;
		ret = ff4_get_track_at(models->moov->trackHead, i, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_INVALID_STATE;
		ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
		if (ret != FF4_OK)
			return ret;
		if (frag == NULL)
			return FF4_ERR_NOT_FOUND;
		frag->fragBaseOff = baseOff;
	}
	return FF4_OK;
}

typedef struct Ff4TfhdOverrideInput {
	uint64_t baseDataOffset;
	uint32_t sampleDescIdx;
	uint32_t defSampleDur;
	uint32_t defSampleSize;
	uint32_t defSampleFlags;
} Ff4TfhdOverrideInput;

int32_t ff4_set_tfhd_override(Ff4WriterModels *models, uint32_t fragIdx, uint32_t trackIdx,
			      uint8_t mask, const void *params)
{
	Ff4TrackModel *track = NULL;
	Ff4SampleTableModel *frag = NULL;
	const Ff4TfhdOverrideInput *in = (const Ff4TfhdOverrideInput *)params;
	int32_t ret;

	if (models == NULL || fragIdx == 0 || trackIdx == 0 || params == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(models->moov->trackHead, trackIdx, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
	if (ret != FF4_OK)
		return ret;
	if (frag == NULL)
		return FF4_ERR_NOT_FOUND;

	if ((mask & 1u) != 0) {
		frag->tfhdFlags |= 1u;
		frag->baseDataOffset = in->baseDataOffset;
	}
	if ((mask & 2u) != 0) {
		frag->tfhdFlags |= 2u;
		frag->sampleDescIdx = in->sampleDescIdx;
	}
	if ((mask & 8u) != 0) {
		frag->tfhdFlags |= 8u;
		frag->defSampleDur = in->defSampleDur;
	}
	if ((mask & 0x10u) != 0) {
		frag->tfhdFlags |= 0x10u;
		frag->defSampleSize = in->defSampleSize;
	}
	if ((mask & 0x20u) != 0) {
		frag->tfhdFlags |= 0x20u;
		frag->defSampleFlags = in->defSampleFlags;
	}
	return FF4_OK;
}
