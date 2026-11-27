/*
 * Sony FF4 MP4 muxer: Movie Fragment Random Access Box (mfra),
 * Track Fragment Random Access Box (tfra), and MFRA Offset Box (mfro).
 */
#include "ff4_internal.h"

static inline uint8_t *ff4_put_var_be(uint8_t *p, uint32_t val, uint32_t lenMinusOne)
{
	if (lenMinusOne == 0) {
		p[0] = (uint8_t)val;
		return p + 1;
	}
	if (lenMinusOne == 1) {
		p[0] = (uint8_t)(val >> 8);
		p[1] = (uint8_t)val;
		return p + 2;
	}
	if (lenMinusOne == 2) {
		p[0] = (uint8_t)(val >> 16);
		p[1] = (uint8_t)(val >> 8);
		p[2] = (uint8_t)val;
		return p + 3;
	}
	ff4_put_be32(p, val);
	return p + 4;
}

static uint32_t ff4_bytes_needed_m1(uint32_t maxVal)
{
	if (maxVal >= 0x1000000u)
		return 3u;
	if (maxVal >= 0x10000u)
		return 2u;
	if (maxVal >= 0x100u)
		return 1u;
	return 0u;
}

int32_t ff4_calc_traf_number(const Ff4MovieModel *moov, uint32_t fragIdx, uint32_t trackIdx,
			     uint32_t *outTrafNum)
{
	const Ff4TrackModel *track;
	uint32_t curIdx = 1;
	uint32_t trafNum = 1;
	int32_t ret;

	if (moov == NULL || outTrafNum == NULL)
		return FF4_ERR_INVALID_ARG;

	track = moov->trackHead;
	while (track != NULL && curIdx != trackIdx) {
		Ff4SampleTableModel *frag = NULL;
		ret = ff4_get_fragment_at(track->fragHead, fragIdx, &frag);
		if (ret != FF4_OK)
			return ret;
		if (frag == NULL)
			return FF4_ERR_NOT_FOUND;
		if (frag->totalSamples != 0)
			trafNum++;
		track = track->next;
		curIdx++;
	}

	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	*outTrafNum = trafNum;
	return FF4_OK;
}

int32_t ff4_serialize_tfra_entries(Ff4SessionImpl *sess, const Ff4TfraPlan *plan, uint8_t *dst)
{
	const Ff4StblRunBlock *blk;
	Ff4SampleTableModel *cachedFrag = NULL;
	uint32_t cachedFragIdx = 0;
	uint32_t trafLenM1;
	uint32_t trunLenM1;
	uint32_t sampleLenM1;
	uint8_t *out = dst;
	int32_t ret = FF4_OK;

	if (sess == NULL || plan == NULL || dst == NULL)
		return FF4_ERR_INVALID_ARG;

	trafLenM1 = (plan->lengthSizeField >> 4) & 3u;
	trunLenM1 = (plan->lengthSizeField >> 2) & 3u;
	sampleLenM1 = plan->lengthSizeField & 3u;

	for (blk = plan->track->tfraHead; blk != NULL; blk = blk->next) {
		const Ff4TfraEntry *entries = (const Ff4TfraEntry *)blk->entries;
		uint32_t i;

		for (i = 0; i < blk->count; i++) {
			uint32_t trafNum = 0;

			if (cachedFragIdx != entries[i].fragIndex) {
				ret = ff4_get_fragment_at(plan->track->fragHead,
							  entries[i].fragIndex, &cachedFrag);
				if (ret != FF4_OK)
					return ret;
				if (cachedFrag == NULL)
					return FF4_ERR_NOT_FOUND;
				cachedFragIdx = entries[i].fragIndex;
			}
			if (cachedFrag == NULL)
				return FF4_ERR_INTERNAL;

			if (plan->version == 1) {
				ff4_put_be64(out, entries[i].time);
				ff4_put_be64(out + 8, cachedFrag->fragBaseOff);
				out += 16;
			} else {
				ff4_put_be32(out, (uint32_t)entries[i].time);
				ff4_put_be32(out + 4, (uint32_t)cachedFrag->fragBaseOff);
				out += 8;
			}

			ret = ff4_calc_traf_number(plan->moov, entries[i].fragIndex,
						   entries[i].trackIndex, &trafNum);
			if (ret != FF4_OK)
				return ret;

			out = ff4_put_var_be(out, trafNum, trafLenM1);
			out = ff4_put_var_be(out, entries[i].trunNum, trunLenM1);
			out = ff4_put_var_be(out, entries[i].sampleNum, sampleLenM1);
		}
	}
	return ret;
}

int32_t ff4_write_tfra_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, void *fileCtx,
			       uint64_t pos, const Ff4TfraPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[16];
	uint64_t total = 0;
	uint64_t tableBytes;
	uint8_t *tempAlloc = NULL;
	uint8_t *workBuf;
	int32_t ret;

	if (sess == NULL || scratch == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 0, ((uint32_t)plan->version << 24) | (plan->flags & 0xFFFFFFu));
	ff4_put_be32(hdr + 4, plan->trackId);
	ff4_put_be32(hdr + 8, plan->lengthSizeField);
	ff4_put_be32(hdr + 12, plan->entryCount);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	tableBytes = (uint64_t)plan->entryCount * (uint64_t)plan->entryStride;
	if (scratch->size >= tableBytes) {
		workBuf = scratch->data;
	} else {
		tempAlloc = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, tableBytes);
		if (tempAlloc == NULL)
			return FF4_ERR_NO_MEM;
		workBuf = tempAlloc;
	}

	ret = ff4_serialize_tfra_entries(sess, plan, workBuf);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, workBuf, tableBytes, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}

	ff4_pool_free(sess, tempAlloc);
	return ret;
}

int32_t ff4_plan_tfra(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      Ff4MovieModel *moov, Ff4TrackModel *track, Ff4TfraPlan *outPlan)
{
	Ff4TfraPlan tmp;
	Ff4TfraEntry maxEntry;
	Ff4SampleTableModel *maxFrag = NULL;
	const Ff4StblRunBlock *blk;
	uint32_t maxTrafNum = 0;
	uint32_t trafLenM1, trunLenM1, sampleLenM1;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	ff4_memset(sess, &maxEntry, 0, sizeof(maxEntry));
	tmp.moov = moov;
	tmp.track = track;
	tmp.trackId = track->trackId;

	ret = ff4_count_run_block_entries(track->tfraHead, &tmp.entryCount);
	if (ret != FF4_OK)
		return ret;

	if (moov == NULL)
		return FF4_ERR_INVALID_ARG;

	for (blk = track->tfraHead; blk != NULL; blk = blk->next) {
		const Ff4TfraEntry *entries = (const Ff4TfraEntry *)blk->entries;
		uint32_t i;

		for (i = 0; i < blk->count; i++) {
			uint32_t trafNum = 0;
			if (maxEntry.time < entries[i].time)
				maxEntry.time = entries[i].time;
			if (maxEntry.fragIndex < entries[i].fragIndex)
				maxEntry.fragIndex = entries[i].fragIndex;
			ret = ff4_calc_traf_number(moov, entries[i].fragIndex,
						   entries[i].trackIndex, &trafNum);
			if (ret != FF4_OK)
				return ret;
			if (maxTrafNum < trafNum) {
				maxTrafNum = trafNum;
				maxEntry.trackIndex = entries[i].trackIndex;
			}
			if (maxEntry.trunNum < entries[i].trunNum)
				maxEntry.trunNum = entries[i].trunNum;
			if (maxEntry.sampleNum < entries[i].sampleNum)
				maxEntry.sampleNum = entries[i].sampleNum;
		}
	}

	ret = ff4_get_fragment_at(track->fragHead, maxEntry.fragIndex, &maxFrag);
	if (ret != FF4_OK)
		return ret;
	if (maxFrag == NULL)
		return FF4_ERR_NOT_FOUND;

	if (opts->boxVerMode == 2) {
		if ((maxFrag->fragBaseOff >> 32) != 0 || (maxEntry.time >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.version = 0;
	} else if (opts->boxVerMode == 0 || opts->boxVerMode == 3) {
		tmp.version = 1;
	} else {
		tmp.version = (uint8_t)(((maxFrag->fragBaseOff >> 32) != 0 ||
					 (maxEntry.time >> 32) != 0) ? 1 : 0);
	}

	trafLenM1 = ff4_bytes_needed_m1(maxTrafNum);
	trunLenM1 = ff4_bytes_needed_m1(maxEntry.trunNum);
	sampleLenM1 = ff4_bytes_needed_m1(maxEntry.sampleNum);

	tmp.lengthSizeField = ((trafLenM1 & 3u) << 4) | ((trunLenM1 & 3u) << 2) | (sampleLenM1 & 3u);
	tmp.entryStride = ((tmp.version == 1) ? 16u : 8u) + trafLenM1 + trunLenM1 + sampleLenM1 + 3u;

	ret = ff4_write_tfra_payload(sess, scratch, NULL, 0, &tmp, &payloadSize, NULL);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_write_tfra_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4TfraPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TFRA,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_tfra_payload(sess, scratch, fileCtx, pos + total, plan,
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

int32_t ff4_plan_mfro(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MfraPlan *mfra,
		      uint32_t *outSize)
{
	const Ff4TfraPlan *cur;
	uint64_t tfraSum = 0;
	uint64_t mfroBoxSize = 0;
	uint64_t mfraTotal = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || mfra == NULL || outSize == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = mfra->tfraHead; cur != NULL; cur = cur->next)
		tfraSum += cur->boxSize;

	ret = ff4_calc_box_size(sess, opts, 8u, &mfroBoxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_calc_box_size(sess, opts, tfraSum + mfroBoxSize, &mfraTotal);
	if (ret != FF4_OK)
		return ret;

	if ((mfraTotal >> 32) != 0)
		return FF4_ERR_INVALID_STATE;

	*outSize = (uint32_t)mfraTotal;
	return FF4_OK;
}

int32_t ff4_write_mfro_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *mfroSize, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[8];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || mfroSize == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf + 4, *mfroSize);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mfro_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const uint32_t *mfroSize,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || mfroSize == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, 8u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MFRO, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mfro_payload(sess, fileCtx, pos + total, mfroSize, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mfra_payload(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			       void *fileCtx, uint64_t pos, const Ff4MfraPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TfraPlan *cur;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = plan->tfraHead; cur != NULL; cur = cur->next) {
		ret = ff4_write_tfra_box(sess, scratch, opts, fileCtx, pos + total, cur,
					 NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	ret = ff4_write_mfro_box(sess, opts, fileCtx, pos + total, &plan->mfroSize, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mfra_box(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
			   void *fileCtx, uint64_t pos, const Ff4MfraPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || scratch == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MFRA,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mfra_payload(sess, scratch, opts, fileCtx, pos + total, plan,
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

int32_t ff4_free_mfra_plan(Ff4SessionImpl *sess, Ff4MfraPlan *plan)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan != NULL) {
		Ff4TfraPlan *cur = plan->tfraHead;
		while (cur != NULL) {
			Ff4TfraPlan *next = cur->next;
			ff4_pool_free(sess, cur);
			cur = next;
		}
		ff4_pool_free(sess, plan);
	}
	return FF4_OK;
}

int32_t ff4_plan_mfra(Ff4SessionImpl *sess, Ff4Buffer *scratch, const Ff4FormatOpts *opts,
		      Ff4MovieModel *moov, Ff4MfraPlan *outPlan)
{
	Ff4MfraPlan *plan = NULL;
	Ff4TfraPlan *node = NULL;
	Ff4TrackModel *track;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || scratch == NULL || opts == NULL || moov == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan = (Ff4MfraPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	for (track = moov->trackHead; track != NULL && ret == FF4_OK; track = track->next) {
		uint32_t cnt = 0;
		if (track->tfraHead == NULL)
			continue;
		ret = ff4_count_run_block_entries(track->tfraHead, &cnt);
		if (ret != FF4_OK || cnt == 0)
			continue;
		node = (Ff4TfraPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_tfra(sess, scratch, opts, moov, track, node);
		if (ret != FF4_OK) {
			ff4_pool_free(sess, node);
			node = NULL;
			break;
		}
		if (plan->tfraHead != NULL)
			plan->tfraTail->next = node;
		else
			plan->tfraHead = node;
		plan->tfraTail = node;
		node = NULL;
	}

	if (ret == FF4_OK)
		ret = ff4_plan_mfro(sess, opts, plan, &plan->mfroSize);
	if (ret == FF4_OK)
		ret = ff4_write_mfra_payload(sess, scratch, opts, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	if (ret == FF4_OK) {
		ff4_memcpy(sess, outPlan, sizeof(*outPlan), plan, sizeof(*plan));
		plan->tfraHead = NULL;
		plan->tfraTail = NULL;
	}

	ff4_free_mfra_plan(sess, plan);
	return ret;
}
