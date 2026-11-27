/*
 * Sony FF4 MP4 muxer: Media Box (mdia), Media Header Box (mdhd),
 * and Handler Reference Box (hdlr) planning and serialization.
 */
#include "ff4_internal.h"

typedef struct Ff4MdhdParamInput {
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t timescale;
	uint32_t padA;
	uint64_t duration;
	uint16_t language;
	uint16_t padB[3];
} Ff4MdhdParamInput;

int32_t ff4_set_mdhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MdhdModel *mdhd,
			    uint32_t mask, const uint32_t *params)
{
	Ff4MdhdModel tmp;
	const Ff4MdhdParamInput *in = (const Ff4MdhdParamInput *)params;

	if (sess == NULL || opts == NULL || mdhd == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &tmp, sizeof(tmp), mdhd, sizeof(tmp));

	if ((mask & 1u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->creationTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.creationTime = in->creationTime;
	}
	if ((mask & 2u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->modTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.modTime = in->modTime;
	}
	if ((mask & 4u) != 0) {
		if (in->timescale == 0)
			return FF4_ERR_INVALID_ARG;
		tmp.timescale = in->timescale;
	}
	if ((mask & 8u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->duration >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.duration = in->duration;
	}
	if ((mask & 0x10u) != 0) {
		if ((in->language & 0x8000u) != 0)
			return FF4_ERR_INVALID_ARG;
		tmp.language = in->language;
	}

	tmp.mask |= mask;
	ff4_memcpy(sess, mdhd, sizeof(*mdhd), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_mdhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MdhdModel *mdhd,
				uint8_t flags, uint64_t creationTime, uint64_t modTime)
{
	Ff4MdhdParamInput in;
	uint32_t mask = 0;

	if (sess == NULL || opts == NULL || mdhd == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &in, 0, sizeof(in));
	if ((flags & 0x10u) != 0) {
		mask |= 1u;
		in.creationTime = creationTime;
	}
	if ((flags & 0x20u) != 0) {
		mask |= 2u;
		in.modTime = modTime;
	}
	return ff4_set_mdhd_params(sess, opts, mdhd, mask, (const uint32_t *)&in);
}

int32_t ff4_set_hdlr_params(Ff4SessionImpl *sess, Ff4HdlrModel *hdlr, uint8_t mask,
			    const void *const *params)
{
	Ff4HdlrModel tmp;
	int32_t ret = FF4_OK;

	if (sess == NULL || hdlr == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &tmp, sizeof(tmp), hdlr, sizeof(tmp));
	if ((mask & 2u) != 0) {
		const char *name = (const char *)params[0];
		uint32_t len = (uint32_t)(uintptr_t)params[1];
		if (len != 0 && name == NULL)
			return FF4_ERR_INVALID_ARG;
		ret = ff4_copy_data_buf(sess, 1, &tmp.name, name, len);
	}
	if (ret == FF4_OK)
		ff4_memcpy(sess, hdlr, sizeof(*hdlr), &tmp, sizeof(tmp));
	return ret;
}

int32_t ff4_write_hdlr_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4HdlrPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4HdlrModel model;
	uint8_t hdr[24];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &model, sizeof(model), &plan->model, sizeof(model));
	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 8, model.handlerType);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, model.name.data, model.name.size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_hdlr_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4HdlrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_HDLR,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_hdlr_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mdhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4MdhdPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4MdhdModel model;
	uint8_t buf[36];
	uint64_t total = 0;
	uint32_t off;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &model, sizeof(model), &plan->model, sizeof(model));
	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[0] = plan->version;

	if (plan->version == 1) {
		ff4_put_be64(buf + 4, model.creationTime);
		ff4_put_be64(buf + 12, model.modTime);
		ff4_put_be32(buf + 20, model.timescale);
		ff4_put_be64(buf + 24, model.duration);
		off = 32;
	} else {
		ff4_put_be32(buf + 4, (uint32_t)model.creationTime);
		ff4_put_be32(buf + 8, (uint32_t)model.modTime);
		ff4_put_be32(buf + 12, model.timescale);
		ff4_put_be32(buf + 16, (uint32_t)model.duration);
		off = 20;
	}
	buf[off + 0] = (uint8_t)((model.language >> 8) & 0x7Fu);
	buf[off + 1] = (uint8_t)(model.language & 0xFFu);
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

int32_t ff4_write_mdhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MdhdPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, (plan->version == 1) ? 36u : 24u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MDHD, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mdhd_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mdia_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MdiaPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_mdhd_box(sess, opts, fileCtx, pos, &plan->mdhd, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_hdlr_box(sess, opts, fileCtx, pos + total, &plan->hdlr, NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_minf_box(sess, opts, fileCtx, pos + total, &plan->minf,
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

int32_t ff4_plan_hdlr(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4HdlrModel *model,
		      Ff4HdlrPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || model == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &outPlan->model, sizeof(outPlan->model), model, sizeof(*model));
	ret = ff4_write_hdlr_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_calc_media_duration(Ff4SessionImpl *sess, const Ff4TrackModel *track, uint64_t *outDur)
{
	uint64_t dur = 0;
	int32_t ret;

	if (sess == NULL || track == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_sum_stts_duration(track, &dur);
	if (ret == FF4_OK)
		*outDur = dur;
	return ret;
}

int32_t ff4_plan_mdhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const Ff4TrackModel *track,
		      Ff4MdhdPlan *outPlan)
{
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &outPlan->model, sizeof(outPlan->model),
		   &track->mdhdMask, sizeof(outPlan->model));

	if ((outPlan->model.mask & 8u) == 0) {
		uint64_t dur = 0;
		ret = ff4_sum_stts_duration(track, &dur);
		if (ret == FF4_OK)
			outPlan->model.duration = dur;
	}
	if (ret == FF4_OK) {
		ret = ff4_choose_box_version(opts, outPlan->model.creationTime,
					     outPlan->model.modTime, outPlan->model.duration,
					     &outPlan->version);
	}
	return ret;
}

int32_t ff4_plan_mdia(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4MdiaPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_minf(sess, opts, track, &outPlan->minf);
	if (ret == FF4_OK) {
		ret = ff4_plan_mdhd(sess, opts, track, &outPlan->mdhd);
		if (ret == FF4_OK) {
			ret = ff4_plan_hdlr(sess, opts, (const Ff4HdlrModel *)&track->handlerType,
					    &outPlan->hdlr);
			if (ret == FF4_OK) {
				ret = ff4_write_mdia_payload(sess, opts, NULL, 0, outPlan,
							     &payloadSize, NULL);
				if (ret == FF4_OK)
					ret = ff4_calc_box_size(sess, opts, payloadSize,
								&outPlan->boxSize);
			}
		}
	}
	return ret;
}

int32_t ff4_write_mdia_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MdiaPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MDIA,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mdia_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}
