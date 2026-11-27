/*
 * Sony FF4 MP4 muxer: Movie Box (moov), Movie Header (mvhd), Initial Object
 * Descriptor (iods), Movie Extends (mvex, mehd, trex), and metadata descriptors.
 */
#include "ff4_internal.h"

typedef struct Ff4MvhdParamInput {
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t timescale;
	uint32_t padA;
	uint64_t duration;
	uint16_t rateHi;
	uint16_t rateLo;
	uint8_t volHi;
	uint8_t volLo;
	uint16_t padB;
	int32_t matrix[9];
	uint32_t nextTrackId;
} Ff4MvhdParamInput;

typedef struct Ff4MvhdPlanLocal {
	uint8_t version;
	uint8_t padA[7];
	uint32_t mask;
	uint32_t padB;
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t timescale;
	uint32_t padC;
	uint64_t duration;
	uint32_t rate;
	uint16_t volume;
	uint16_t padD;
	int32_t matrix[9];
	uint32_t nextTrackId;
} Ff4MvhdPlanLocal;

int32_t ff4_init_iods(Ff4SessionImpl *sess, Ff4MovieModel *moov, const uint8_t *profileBytes,
		      uint32_t esId)
{
	Ff4IodsModel *iods;

	if (sess == NULL || moov == NULL || profileBytes == NULL)
		return FF4_ERR_INVALID_ARG;
	if (moov->iods != NULL)
		return FF4_ERR_INTERNAL;

	iods = (Ff4IodsModel *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*iods));
	if (iods == NULL)
		return FF4_ERR_NO_MEM;

	iods->objectDescrId = 1022u;
	iods->includeInline = 0xFEu;
	iods->odProfile = 0xFFu;
	iods->sceneProfile = profileBytes[12];
	iods->audioProfile = profileBytes[13];
	iods->visualProfile = 0xFFu;
	iods->esId = esId;
	moov->iods = iods;
	return FF4_OK;
}

int32_t ff4_set_mvhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
			    uint32_t mask, const uint32_t *params)
{
	uint8_t tmpBuf[88];
	Ff4MovieModel *tmp = (Ff4MovieModel *)tmpBuf;
	const Ff4MvhdParamInput *in = (const Ff4MvhdParamInput *)params;

	if (sess == NULL || opts == NULL || moov == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, tmpBuf, sizeof(tmpBuf), moov, sizeof(tmpBuf));

	if ((mask & 1u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->creationTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->creationTime = in->creationTime;
	}
	if ((mask & 2u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->modTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->modTime = in->modTime;
	}
	if ((mask & 4u) != 0) {
		if (in->timescale == 0)
			return FF4_ERR_INVALID_ARG;
		tmp->timescale = in->timescale;
	}
	if ((mask & 8u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->duration >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->duration = in->duration;
	}
	if ((mask & 0x10u) != 0)
		tmp->rate = (tmp->rate & 0x0000FFFFu) | ((uint32_t)in->rateHi << 16);
	if ((mask & 0x20u) != 0)
		tmp->rate = (tmp->rate & 0xFFFF0000u) | (uint32_t)in->rateLo;
	if ((mask & 0x40u) != 0)
		tmp->volume = (uint16_t)((tmp->volume & 0x00FFu) | ((uint16_t)in->volHi << 8));
	if ((mask & 0x80u) != 0)
		tmp->volume = (uint16_t)((tmp->volume & 0xFF00u) | (uint16_t)in->volLo);
	if ((mask & 0x100u) != 0)
		ff4_memcpy(sess, tmp->matrix, sizeof(tmp->matrix), in->matrix, sizeof(tmp->matrix));
	if ((mask & 0x200u) != 0)
		tmp->nextTrackId = in->nextTrackId;

	tmp->mvhdMask |= mask;
	ff4_memcpy(sess, moov, sizeof(tmpBuf), tmpBuf, sizeof(tmpBuf));
	return FF4_OK;
}

int32_t ff4_set_mvhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
				uint8_t flags, uint64_t creationTime, uint64_t modTime)
{
	Ff4MvhdParamInput in;
	uint32_t mask = 0;

	if (sess == NULL || opts == NULL || moov == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &in, 0, sizeof(in));
	if ((flags & 1u) != 0) {
		mask |= 1u;
		in.creationTime = creationTime;
	}
	if ((flags & 2u) != 0) {
		mask |= 2u;
		in.modTime = modTime;
	}
	return ff4_set_mvhd_params(sess, opts, moov, mask, (const uint32_t *)&in);
}

int32_t ff4_set_iods_params(Ff4SessionImpl *sess, Ff4IodsModel *iods, uint8_t mask,
			    const uint16_t *params)
{
	Ff4IodsModel tmp;
	const uint8_t *bytes = (const uint8_t *)params;

	if (sess == NULL || iods == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &tmp, sizeof(tmp), iods, sizeof(tmp));
	if ((mask & 1u) != 0) {
		if ((params[0] & 0xFC00u) != 0)
			return FF4_ERR_INVALID_ARG;
		tmp.objectDescrId = params[0];
	}
	if ((mask & 2u) != 0) {
		if ((bytes[2] & 0xFEu) != 0)
			return FF4_ERR_INVALID_ARG;
		tmp.urlFlag = bytes[2];
	}
	if ((mask & 4u) != 0)
		tmp.includeInline = bytes[3];
	if ((mask & 8u) != 0)
		tmp.odProfile = bytes[4];
	if ((mask & 0x10u) != 0)
		tmp.sceneProfile = bytes[5];
	if ((mask & 0x20u) != 0)
		tmp.audioProfile = bytes[6];
	if ((mask & 0x40u) != 0)
		tmp.visualProfile = bytes[7];

	ff4_memcpy(sess, iods, sizeof(*iods), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_write_es_id_inc_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const uint32_t *esIdPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || esIdPtr == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf, esIdPtr[2]);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_es_id_inc_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const uint64_t *sizeAndEsId, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || sizeAndEsId == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, *sizeAndEsId, 14u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_es_id_inc_payload(sess, fileCtx, pos + total,
						  (const uint32_t *)sizeAndEsId, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_iod_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4IodsModel model;
	uint8_t buf[7];
	uint16_t packedId;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &model, sizeof(model), &plan->model, sizeof(model));
	packedId = (uint16_t)(((model.objectDescrId & 0x3FFu) << 6) |
			      ((model.urlFlag & 1u) << 4) | 0x0Fu);
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf, packedId);
	buf[2] = model.includeInline;
	buf[3] = model.odProfile;
	buf[4] = model.sceneProfile;
	buf[5] = model.audioProfile;
	buf[6] = model.visualProfile;

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_es_id_inc_descr(sess, fileCtx, pos + total, &plan->esIdSize,
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

int32_t ff4_write_iod_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			    const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->odSize, 16u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_iod_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_iods_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4IodsPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_iod_descr(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_iods_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4IodsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_IODS,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_iods_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mvhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MoovPlan *moovMvhd, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MvhdPlanLocal *plan = (const Ff4MvhdPlanLocal *)moovMvhd;
	uint8_t buf[112];
	uint64_t total = 0;
	uint32_t off;
	int32_t i;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[0] = plan->version;
	if (plan->version == 1) {
		ff4_put_be64(buf + 4, plan->creationTime);
		ff4_put_be64(buf + 12, plan->modTime);
		ff4_put_be32(buf + 20, plan->timescale);
		ff4_put_be64(buf + 24, plan->duration);
		off = 32;
	} else {
		ff4_put_be32(buf + 4, (uint32_t)plan->creationTime);
		ff4_put_be32(buf + 8, (uint32_t)plan->modTime);
		ff4_put_be32(buf + 12, plan->timescale);
		ff4_put_be32(buf + 16, (uint32_t)plan->duration);
		off = 20;
	}

	ff4_put_be32(buf + off, plan->rate);
	ff4_put_be16(buf + off + 4, plan->volume);
	off += 16; /* rate(4) + volume(2) + reserved(10) */

	for (i = 0; i < 9; i++) {
		ff4_put_be32(buf + off, (uint32_t)plan->matrix[i]);
		off += 4;
	}
	off += 24; /* pre_defined[6] */
	ff4_put_be32(buf + off, plan->nextTrackId);
	off += 4;

	ret = ff4_box_write(sess, fileCtx, buf, off, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mvhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MoovPlan *moovMvhd,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MvhdPlanLocal *plan = (const Ff4MvhdPlanLocal *)moovMvhd;
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, (plan->version == 1) ? 112u : 100u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MVHD, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mvhd_payload(sess, fileCtx, pos + total, moovMvhd, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_moov_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MoovPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TrakPlan *trak;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_mvhd_box(sess, opts, fileCtx, pos,
				 (const Ff4MoovPlan *)&plan->mvhdVersion, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_iods_box(sess, opts, fileCtx, pos + total,
					 (const Ff4IodsPlan *)plan->iodsPlan, NULL, &total);
	if (ret == FF4_OK) {
		for (trak = plan->trakHead; trak != NULL; trak = trak->next) {
			ret = ff4_write_trak_box(sess, opts, fileCtx, pos + total, trak,
						 NULL, &total);
			if (ret != FF4_OK)
				return ret;
		}
		ret = ff4_write_mvex_box(sess, opts, fileCtx, pos + total,
					 (const Ff4MvexPlan *)plan->mvexPlan, NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_usmt_box(sess, opts, fileCtx, pos + total, plan->uuidPlan,
						 NULL, &total);
			if (ret == FF4_OK) {
				if (outWritten != NULL)
					*outWritten = total;
				if (accumWritten != NULL)
					*accumWritten += total;
			}
		}
	}
	return ret;
}

int32_t ff4_plan_es_id_inc(Ff4SessionImpl *sess, const Ff4IodsModel *model, void *outEsIdPlan)
{
	uint64_t *out = (uint64_t *)outEsIdPlan;
	uint32_t *words = (uint32_t *)outEsIdPlan;

	if (sess == NULL || model == NULL || outEsIdPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	words[2] = model->esId;
	return ff4_write_es_id_inc_payload(sess, NULL, 0, words, out, NULL);
}

int32_t ff4_plan_iod_descr(Ff4SessionImpl *sess, const Ff4IodsModel *model, void *outOdPlan)
{
	Ff4IodsPlan *plan = (Ff4IodsPlan *)((uint8_t *)outOdPlan - 8);
	int32_t ret;

	if (sess == NULL || model == NULL || outOdPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->model, sizeof(plan->model), model, sizeof(*model));
	ret = ff4_plan_es_id_inc(sess, model, &plan->esIdSize);
	if (ret == FF4_OK)
		ret = ff4_write_iod_payload(sess, NULL, 0, plan, &plan->odSize, NULL);
	return ret;
}

int32_t ff4_plan_iods(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4IodsModel *model,
		      Ff4IodsPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || model == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_iod_descr(sess, model, &outPlan->odSize);
	if (ret == FF4_OK) {
		ret = ff4_write_iods_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_calc_next_track_id(Ff4SessionImpl *sess, const Ff4TrackModel *head, uint32_t *outNextId)
{
	uint32_t maxId = 0;
	uint32_t nextId;
	const Ff4TrackModel *cur = head;

	if (sess == NULL || outNextId == NULL)
		return FF4_ERR_INVALID_ARG;

	while (cur != NULL) {
		if (maxId < cur->trackId)
			maxId = cur->trackId;
		cur = cur->next;
	}

	if (maxId < 0x7Fu)
		nextId = maxId + 1u;
	else if (maxId < 0x400u)
		nextId = 0x400u;
	else if (maxId < 0xF7FFu)
		nextId = maxId + 1u;
	else
		nextId = 0xFFFFFFFFu;

	*outNextId = nextId;
	return FF4_OK;
}

int32_t ff4_calc_movie_duration(Ff4SessionImpl *sess, uint32_t movieTimescale,
				const Ff4TrackModel *head, uint64_t *outDur)
{
	const Ff4TrackModel *cur = head;
	uint64_t maxDur = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;

	while (cur != NULL) {
		uint64_t dur = 0;
		if ((cur->tkhdMask & 0x10u) != 0) {
			dur = cur->duration;
		} else {
			ret = ff4_calc_track_duration(sess, movieTimescale, cur, &dur);
			if (ret != FF4_OK)
				return ret;
		}
		if (maxDur < dur)
			maxDur = dur;
		cur = cur->next;
	}

	*outDur = maxDur;
	return FF4_OK;
}

int32_t ff4_plan_mvhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      void *outMvhdPlan)
{
	Ff4MvhdPlanLocal *plan = (Ff4MvhdPlanLocal *)outMvhdPlan;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || moov == NULL || outMvhdPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->mask, 88u, moov, 88u);
	if ((plan->mask & 8u) == 0)
		ret = ff4_calc_movie_duration(sess, moov->timescale, moov->trackHead,
					      &plan->duration);
	if (ret == FF4_OK && (plan->mask & 0x200u) == 0)
		ret = ff4_calc_next_track_id(sess, moov->trackHead, &plan->nextTrackId);
	if (ret == FF4_OK)
		ret = ff4_choose_box_version(opts, plan->creationTime, plan->modTime,
					     plan->duration, &plan->version);
	return ret;
}

int32_t ff4_plan_moov(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
		      Ff4MoovPlan *outPlan)
{
	Ff4TrackModel *track;
	Ff4TrakPlan *trakPlan = NULL;
	Ff4IodsPlan *iodsPlan = NULL;
	Ff4MvexPlan *mvexPlan = NULL;
	Ff4UuidPlan *uuidPlan = NULL;
	uint32_t fragCount = 0;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || moov == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (track = moov->trackHead; track != NULL; track = track->next) {
		trakPlan = (Ff4TrakPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trakPlan));
		if (trakPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_trak(sess, opts, moov->timescale, track, trakPlan);
		if (ret != FF4_OK)
			break;
		if (outPlan->trakHead != NULL)
			outPlan->trakTail->next = trakPlan;
		else
			outPlan->trakHead = trakPlan;
		outPlan->trakTail = trakPlan;
		trakPlan = NULL;
	}

	if (ret == FF4_OK)
		ret = ff4_plan_mvhd(sess, opts, moov, &outPlan->mvhdVersion);

	if (ret == FF4_OK && moov->iods != NULL) {
		iodsPlan = (Ff4IodsPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*iodsPlan));
		if (iodsPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_iods(sess, opts, moov->iods, iodsPlan);
			if (ret == FF4_OK) {
				outPlan->iodsPlan = iodsPlan;
				iodsPlan = NULL;
			}
		}
	}

	if (ret == FF4_OK)
		ret = ff4_get_fragment_count(moov, &fragCount);

	if (ret == FF4_OK && fragCount != 0) {
		mvexPlan = (Ff4MvexPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*mvexPlan));
		if (mvexPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_mvex(sess, opts, moov, mvexPlan);
			if (ret == FF4_OK) {
				outPlan->mvexPlan = mvexPlan;
				mvexPlan = NULL;
			}
		}
	}

	if (ret == FF4_OK && (moov->mtdtHead != NULL || moov->mtdfHead != NULL ||
			      moov->mtsmHead != NULL)) {
		uuidPlan = (Ff4UuidPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*uuidPlan));
		if (uuidPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_usmt(sess, opts, &moov->mtdtHead, uuidPlan);
			if (ret == FF4_OK) {
				outPlan->uuidPlan = uuidPlan;
				uuidPlan = NULL;
			}
		}
	}

	if (ret == FF4_OK)
		ret = ff4_write_moov_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);

	ff4_pool_free(sess, iodsPlan);
	ff4_free_trak_plan_list(sess, trakPlan);
	ff4_free_mvex_plan(sess, mvexPlan);
	ff4_free_uuid_plan(sess, uuidPlan);
	return ret;
}

int32_t ff4_write_moov_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MoovPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MOOV,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_moov_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_plan_mehd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      Ff4MehdPlan *outPlan)
{
	Ff4MehdPlan tmp;
	const Ff4TrackModel *track;
	uint64_t maxDur = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || moov == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	for (track = moov->trackHead; track != NULL; track = track->next) {
		if (track->fragTail != NULL) {
			uint64_t d1 = track->fragTail->baseDecodeTime;
			uint64_t d2 = track->fragTail->totalDuration;
			uint64_t scaled = 0;
			if (~d1 < d2)
				return FF4_ERR_OVERFLOW;
			ret = ff4_rescale_u64(d1 + d2, track->mdhdTimescale, moov->timescale,
					      &scaled);
			if (ret != FF4_OK)
				return ret;
			if (maxDur < scaled)
				maxDur = scaled;
		}
	}

	if (opts->mehdMode == 2 && (maxDur >> 32) != 0)
		return FF4_ERR_INVALID_STATE;

	tmp.version = (uint8_t)((opts->mehdMode == 3 || (maxDur >> 32) != 0) ? 1 : 0);
	tmp.fragDuration = maxDur;
	ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_plan_trex(Ff4SessionImpl *sess, const Ff4TrackModel *track, Ff4TrexPlan *outPlan)
{
	Ff4TrexPlan tmp;

	if (sess == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.trackId = track->trackId;
	tmp.descIdx = track->trexDescIdx;
	tmp.sampleDur = track->trexSampleDur;
	tmp.sampleSize = track->trexSampleSize;
	tmp.sampleFlags = track->trexSampleFlags;
	ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_free_mvex_plan(Ff4SessionImpl *sess, void *plan)
{
	Ff4MvexPlan *mp = (Ff4MvexPlan *)plan;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (mp != NULL) {
		Ff4TrexPlan *cur = mp->trexHead;
		ff4_pool_free(sess, mp->mehd);
		while (cur != NULL) {
			Ff4TrexPlan *next = cur->next;
			ff4_pool_free(sess, cur);
			cur = next;
		}
		ff4_pool_free(sess, mp);
	}
	return FF4_OK;
}

int32_t ff4_write_trex_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4TrexPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[24];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf + 4, plan->trackId);
	ff4_put_be32(buf + 8, plan->descIdx);
	ff4_put_be32(buf + 12, plan->sampleDur);
	ff4_put_be32(buf + 16, plan->sampleSize);
	ff4_put_be32(buf + 20, plan->sampleFlags);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_trex_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrexPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, 24u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_TREX, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_trex_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mehd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MehdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[12];
	uint64_t total = 0;
	uint32_t len;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[0] = plan->version;
	if (plan->version == 1) {
		ff4_put_be64(buf + 4, plan->fragDuration);
		len = 12;
	} else {
		ff4_put_be32(buf + 4, (uint32_t)plan->fragDuration);
		len = 8;
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

int32_t ff4_write_mehd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MehdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, (plan->version == 1) ? 12u : 8u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MEHD, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mehd_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mvex_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MvexPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TrexPlan *trex;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan->mehd != NULL)
		ret = ff4_write_mehd_box(sess, opts, fileCtx, pos, plan->mehd, NULL, &total);
	if (ret == FF4_OK) {
		for (trex = plan->trexHead; trex != NULL; trex = trex->next) {
			ret = ff4_write_trex_box(sess, opts, fileCtx, pos + total, trex,
						 NULL, &total);
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

int32_t ff4_plan_mvex(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4MovieModel *moov,
		      Ff4MvexPlan *outPlan)
{
	Ff4MvexPlan *plan = NULL;
	Ff4MehdPlan *mehd = NULL;
	Ff4TrexPlan *trex = NULL;
	const Ff4TrackModel *track;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || moov == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan = (Ff4MvexPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	if (opts->mehdMode != 0) {
		mehd = (Ff4MehdPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*mehd));
		if (mehd == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_mehd(sess, opts, moov, mehd);
			if (ret == FF4_OK) {
				plan->mehd = mehd;
				mehd = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		for (track = moov->trackHead; track != NULL; track = track->next) {
			trex = (Ff4TrexPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trex));
			if (trex == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_trex(sess, track, trex);
			if (ret != FF4_OK)
				break;
			if (plan->trexHead != NULL)
				plan->trexTail->next = trex;
			else
				plan->trexHead = trex;
			plan->trexTail = trex;
			trex = NULL;
		}
	}

	if (ret == FF4_OK)
		ret = ff4_write_mvex_payload(sess, opts, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	if (ret == FF4_OK) {
		ff4_memcpy(sess, outPlan, sizeof(*outPlan), plan, sizeof(*plan));
		plan->mehd = NULL;
		plan->trexHead = NULL;
		plan->trexTail = NULL;
	}

	ff4_pool_free(sess, mehd);
	ff4_pool_free(sess, trex);
	ff4_free_mvex_plan(sess, plan);
	return ret;
}

int32_t ff4_write_mvex_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MvexPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MVEX,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_mvex_payload(sess, opts, fileCtx, pos + total, plan,
						     NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_set_trex_defaults(Ff4WriterModels *models, uint32_t trackIdx, uint8_t mask,
			      const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (models == NULL || trackIdx == 0 || params == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(models->moov->trackHead, trackIdx, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0) {
		track->trexMask |= 1u;
		track->trexDescIdx = params[0];
	}
	if ((mask & 2u) != 0) {
		track->trexMask |= 2u;
		track->trexSampleDur = params[1];
	}
	if ((mask & 4u) != 0) {
		track->trexMask |= 4u;
		track->trexSampleSize = params[2];
	}
	if ((mask & 8u) != 0) {
		track->trexMask |= 8u;
		track->trexSampleFlags = params[3];
	}
	return FF4_OK;
}

int32_t ff4_finalize_trex_defaults(Ff4WriterModels *models)
{
	Ff4TrackModel *track;

	if (models == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	for (track = models->moov->trackHead; track != NULL; track = track->next) {
		Ff4SampleTableModel *frag = track->fragHead;
		if ((track->trexMask & 1u) == 0) {
			if (frag != NULL && frag->stscHead != NULL && frag->stscHead->entries != NULL)
				track->trexDescIdx = ((const uint32_t *)frag->stscHead->entries)[2];
			else
				track->trexDescIdx = 1u;
		}
		if ((track->trexMask & 2u) == 0) {
			if (frag != NULL && frag->sttsHead != NULL && frag->sttsHead->entries != NULL)
				track->trexSampleDur = ((const uint32_t *)frag->sttsHead->entries)[1];
			else
				track->trexSampleDur = 0u;
		}
		if ((track->trexMask & 4u) == 0) {
			uint32_t sz = 0;
			if (frag != NULL) {
				if (frag->fixedSampleSize != 0)
					sz = frag->fixedSampleSize;
				else if (frag->stszHead != NULL && frag->stszHead->entries != NULL)
					sz = ((const uint32_t *)frag->stszHead->entries)[0];
			}
			track->trexSampleSize = sz;
		}
		if ((track->trexMask & 8u) == 0) {
			uint32_t flags = 0x10000u;
			if (frag != NULL && frag->stssHead == NULL && frag->hasSyncSample != 0)
				flags = 0u;
			track->trexSampleFlags = flags;
		}
	}
	return FF4_OK;
}

int32_t ff4_is_valid_tref_id_hi(uint32_t id, int32_t *outValid)
{
	if (outValid == NULL)
		return FF4_ERR_INVALID_ARG;
	*outValid = (id >= 0x100u && id <= 0x3FDu) ? 1 : 0;
	return FF4_OK;
}

int32_t ff4_is_valid_tref_id_lo(uint32_t id, int32_t *outValid)
{
	if (outValid == NULL)
		return FF4_ERR_INVALID_ARG;
	*outValid = (id >= 0x80u && id <= 0xFFu) ? 1 : 0;
	return FF4_OK;
}

int32_t ff4_add_tref_tag(Ff4SessionImpl *sess, Ff4TrefEntry *entry, const void *tagParam)
{
	const uint8_t *bytes = (const uint8_t *)tagParam;
	Ff4NodeU32 *found = NULL;
	Ff4NodeU32 *node;
	int32_t ret;

	if (sess == NULL || entry == NULL || tagParam == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_find_u8_node(entry->extraIds, bytes[8], &found);
	if (ret == FF4_OK && found == NULL) {
		node = (Ff4NodeU32 *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL)
			return FF4_ERR_NO_MEM;
		node->value = bytes[8];
		if (entry->extraIds != NULL)
			entry->extraIdsTail->next = node;
		else
			entry->extraIds = node;
		entry->extraIdsTail = node;
	}
	return ret;
}

int32_t ff4_set_tref_value(Ff4SessionImpl *sess, Ff4TrefEntry *entry, const void *valParam)
{
	const uint32_t *words = (const uint32_t *)valParam;
	Ff4NodeU32 *node;

	if (sess == NULL || entry == NULL || valParam == NULL)
		return FF4_ERR_INVALID_ARG;

	if (entry->trackIds != NULL) {
		entry->trackIds->value = words[1];
	} else {
		node = (Ff4NodeU32 *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL)
			return FF4_ERR_NO_MEM;
		node->value = words[1];
		entry->trackIds = node;
		entry->trackIdsTail = node;
	}
	return FF4_OK;
}

int32_t ff4_add_meta_tref(Ff4SessionImpl *sess, Ff4TrefEntry **headTail, const uint16_t *param)
{
	Ff4TrefEntry *found = NULL;
	Ff4TrefEntry *node = NULL;
	int32_t ret;

	if (sess == NULL || headTail == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_find_tref_by_id(headTail[0], param[0], &found);
	if (ret == FF4_OK) {
		if (found != NULL) {
			ret = ff4_set_tref_value(sess, found, param);
			if (ret == FF4_OK)
				ret = ff4_add_tref_tag(sess, found, param);
		} else {
			node = (Ff4TrefEntry *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
			if (node == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				node->type = param[0];
				ret = ff4_set_tref_value(sess, node, param);
				if (ret == FF4_OK)
					ret = ff4_add_tref_tag(sess, node, param);
				if (ret == FF4_OK) {
					if (headTail[0] != NULL)
						headTail[1]->next = node;
					else
						headTail[0] = node;
					headTail[1] = node;
					node = NULL;
				}
			}
		}
	}

	ff4_free_tref_list(sess, node);
	return ret;
}

typedef struct Ff4MetaItemInput {
	uint32_t padA;
	uint32_t padB;
	uint8_t tag;
	uint8_t padC;
	uint16_t flags;
	const void *data;
	uint32_t size;
} Ff4MetaItemInput;

int32_t ff4_add_meta_item(Ff4SessionImpl *sess, Ff4MetaDataList *meta, const void *param)
{
	const Ff4MetaItemInput *in = (const Ff4MetaItemInput *)param;
	Ff4MetaItemNode *found = NULL;
	Ff4MetaItemNode *node = NULL;
	int32_t ret;

	if (sess == NULL || meta == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_find_meta_item_by_tag(meta->dataHead, in->tag, &found, NULL);
	if (ret == FF4_OK) {
		if (found != NULL) {
			found->flags = in->flags;
			ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&found->data,
						in->data, in->size);
		} else {
			node = (Ff4MetaItemNode *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
			if (node == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				node->tag = in->tag;
				node->flags = in->flags;
				ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&node->data,
							in->data, in->size);
				if (ret == FF4_OK) {
					if (meta->dataHead != NULL)
						meta->dataTail->next = node;
					else
						meta->dataHead = node;
					meta->dataTail = node;
					node = NULL;
				}
			}
		}
	}

	ff4_free_meta_list(sess, node);
	return ret;
}

int32_t ff4_write_meta_item_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MetaItemPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4MetaItemNode item;
	uint8_t hdr[3];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &item, sizeof(item), plan->item, sizeof(item));
	ff4_memset(sess, hdr, 0, sizeof(hdr));
	hdr[0] = item.tag;
	hdr[1] = (uint8_t)(item.flags >> 8);
	hdr[2] = (uint8_t)(item.flags & 0xFFu);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, item.data, item.size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_meta_item_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const Ff4MetaItemPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 11u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_meta_item_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_meta_list_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MetaBoxPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MetaItemPlan *cur;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_write_meta_item_descr(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}
	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_meta_list_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const Ff4MetaBoxPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	uint64_t bodyTotal = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 5u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_meta_list_payload(sess, fileCtx, pos + total, plan,
						  &bodyTotal, NULL);
		if (ret == FF4_OK) {
			total += bodyTotal;
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}
