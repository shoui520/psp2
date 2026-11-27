/*
 * Sony FF4 MP4 muxer: box header writers, descriptor encoding,
 * generic model/plan destructors, and linked-list helpers.
 */
#include "ff4_internal.h"

int32_t ff4_box_seek(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos, int32_t whence)
{
	if (fileCtx != NULL)
		return ff4_io_seek(sess, fileCtx, (int64_t)pos, whence);
	return FF4_OK;
}

int32_t ff4_box_write(Ff4SessionImpl *sess, void *fileCtx, const void *buf, uint64_t size,
		      uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t written = size;
	int32_t ret = FF4_OK;

	if (fileCtx != NULL && size != 0) {
		written = 0;
		ret = ff4_io_write(sess, fileCtx, buf, size, &written);
		if (ret != FF4_OK)
			return ret;
	}
	if (outWritten != NULL)
		*outWritten = written;
	if (accumWritten != NULL)
		*accumWritten += written;
	return ret;
}

int32_t ff4_write_box_header(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			     uint64_t pos, uint64_t boxSize, uint32_t fourcc,
			     uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[16];
	uint64_t hdrLen;
	uint64_t written = 0;
	int32_t ret;
	int32_t use64 = 0;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	if (opts->boxSizeMode == 0) {
		if ((boxSize >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		use64 = 0;
	} else if (opts->boxSizeMode == 1) {
		use64 = ((boxSize >> 32) != 0);
	} else if (opts->boxSizeMode == 2) {
		use64 = 1;
	} else {
		return FF4_ERR_INVALID_ARG;
	}

	if (use64) {
		ff4_put_be32(buf, 1u);
		ff4_put_be32(buf + 4, fourcc);
		ff4_put_be64(buf + 8, boxSize);
		hdrLen = 16;
	} else {
		ff4_put_be32(buf, (uint32_t)boxSize);
		ff4_put_be32(buf + 4, fourcc);
		hdrLen = 8;
	}

	ret = ff4_box_write(sess, fileCtx, buf, hdrLen, &written, NULL);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = written;
		if (accumWritten != NULL)
			*accumWritten += written;
	}
	return ret;
}

int32_t ff4_calc_box_size(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint64_t payloadSize,
			  uint64_t *outBoxSize)
{
	uint64_t hdrSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outBoxSize == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, NULL, 0, payloadSize + 8u, 0, &hdrSize, NULL);
	if (ret == FF4_OK)
		*outBoxSize = payloadSize + hdrSize;
	return ret;
}

int32_t ff4_write_short_box_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				   uint64_t boxSize, uint32_t fourcc,
				   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[6];
	uint64_t written = 0;
	int32_t ret;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if ((boxSize >> 16) != 0)
		return FF4_ERR_INTERNAL;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf, (uint16_t)boxSize);
	ff4_put_be32(buf + 2, fourcc);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), &written, NULL);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = written;
		if (accumWritten != NULL)
			*accumWritten += written;
	}
	return ret;
}

int32_t ff4_calc_short_box_size(Ff4SessionImpl *sess, uint64_t payloadSize, uint64_t *outBoxSize)
{
	uint64_t hdrSize = 0;
	int32_t ret;

	if (sess == NULL || outBoxSize == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_short_box_header(sess, NULL, 0, payloadSize, 0, &hdrSize, NULL);
	if (ret == FF4_OK)
		*outBoxSize = payloadSize + hdrSize;
	return ret;
}

int32_t ff4_write_uuid_bytes(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			     const uint32_t uuid[4], uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[16];
	uint64_t written = 0;
	int32_t ret;

	if (sess == NULL || uuid == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be32(buf + 0, uuid[0]);
	ff4_put_be32(buf + 4, uuid[1]);
	ff4_put_be32(buf + 8, uuid[2]);
	ff4_put_be32(buf + 12, uuid[3]);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), &written, NULL);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = written;
		if (accumWritten != NULL)
			*accumWritten += written;
	}
	return ret;
}

int32_t ff4_write_descr_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       uint64_t payloadSize, uint8_t tag,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t encoded[5];
	uint8_t out[5];
	uint64_t rem = payloadSize;
	uint64_t written = 0;
	uint32_t count = 0;
	uint32_t i;
	int32_t ret;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, encoded, 0, sizeof(encoded));
	ff4_memset(sess, out, 0, sizeof(out));
	out[0] = tag;

	for (i = 0; i < 4; i++) {
		uint8_t part = (uint8_t)(rem & 0x7Fu);
		rem = (rem - part) >> 7;
		encoded[i] = (i == 0) ? part : (uint8_t)(part | 0x80u);
		count = i + 1;
		if (rem == 0)
			break;
	}
	if (rem != 0)
		return FF4_ERR_OVERFLOW;

	for (i = 0; i < count; i++)
		out[1 + i] = encoded[count - 1 - i];

	ret = ff4_box_write(sess, fileCtx, out, (uint64_t)(1u + count), &written, NULL);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = payloadSize + written;
		if (accumWritten != NULL)
			*accumWritten += written;
	}
	return ret;
}

int32_t ff4_copy_data_buf(Ff4SessionImpl *sess, int32_t isString, Ff4BoxDataBuf *dst,
			  const void *src, uint64_t size)
{
	uint8_t *copy = NULL;
	uint64_t allocSize = 0;

	if (sess == NULL || dst == NULL)
		return FF4_ERR_INVALID_ARG;

	if (dst->data != NULL) {
		ff4_pool_free(sess, dst->data);
		dst->data = NULL;
		dst->size = 0;
	}

	if (size != 0) {
		allocSize = isString ? (ff4_strnlen((const char *)src, size) + 1u) : size;
		copy = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, allocSize);
		if (copy == NULL)
			return FF4_ERR_NO_MEM;
		ff4_memcpy(sess, copy, allocSize, src, allocSize);
	}

	dst->data = copy;
	dst->size = allocSize;
	return FF4_OK;
}

int32_t ff4_choose_box_version(const Ff4FormatOpts *opts, uint64_t v1, uint64_t v2, uint64_t v3,
			       uint8_t *outVersion)
{
	int32_t hi1 = ((v1 >> 32) != 0);
	int32_t hi2 = ((v2 >> 32) != 0);
	int32_t hi3 = ((v3 >> 32) != 0);
	uint8_t ver = 0;

	if (opts == NULL || outVersion == NULL)
		return FF4_ERR_INVALID_ARG;

	if (opts->boxVerMode == 0 || opts->boxVerMode == 2) {
		if (hi1 && hi2 && hi3)
			return FF4_ERR_INVALID_STATE;
		ver = 0;
	} else if (opts->boxVerMode == 1) {
		ver = (uint8_t)((hi1 || hi2 || hi3) ? 1 : 0);
	} else if (opts->boxVerMode == 3) {
		ver = 1;
	} else {
		return FF4_ERR_INTERNAL;
	}

	*outVersion = ver;
	return FF4_OK;
}

int32_t ff4_set_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4MovieModel *moov,
			   uint32_t trackIndex, uint8_t flags, uint64_t creationTime, uint64_t modTime)
{
	Ff4TrackModel *track = NULL;
	Ff4MtsmEntry *mtsm;
	int32_t ret;

	if (sess == NULL || opts == NULL || moov == NULL)
		return FF4_ERR_INVALID_ARG;

	if (trackIndex != 0) {
		ret = ff4_get_track_at(moov->trackHead, trackIndex, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_NOT_FOUND;
		ret = ff4_set_tkhd_timestamps(sess, opts, track, flags, creationTime, modTime);
		if (ret == FF4_OK)
			ret = ff4_set_mdhd_timestamps(sess, opts,
						      (Ff4MdhdModel *)&track->mdhdMask,
						      flags, creationTime, modTime);
		return ret;
	}

	ret = ff4_set_mvhd_timestamps(sess, opts, moov, flags, creationTime, modTime);
	if (ret != FF4_OK)
		return ret;

	for (track = moov->trackHead; track != NULL; track = track->next) {
		ret = ff4_set_tkhd_timestamps(sess, opts, track, flags, creationTime, modTime);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_set_mdhd_timestamps(sess, opts, (Ff4MdhdModel *)&track->mdhdMask,
					      flags, creationTime, modTime);
		if (ret != FF4_OK)
			return ret;
	}

	for (mtsm = moov->mtsmHead; mtsm != NULL; mtsm = mtsm->next) {
		ret = ff4_set_mtsm_timestamps(sess, opts, mtsm, flags, creationTime, modTime);
		if (ret != FF4_OK)
			return ret;
	}
	return FF4_OK;
}

int32_t ff4_write_ftyp_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4FtypPlan *plan, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint8_t brandBuf[4];
	uint64_t total = 0;
	const Ff4NodeU32 *node;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 0, plan->model->majorBrand);
	ff4_put_be32(hdr + 4, plan->model->minorVersion);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (node = plan->model->compatHead; node != NULL; node = node->next) {
		ff4_put_be32(brandBuf, node->value);
		ret = ff4_box_write(sess, fileCtx, brandBuf, sizeof(brandBuf), NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_plan_ftyp(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4FtypModel *model,
		      Ff4FtypPlan *outPlan)
{
	Ff4FtypPlan tmp;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || model == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	tmp.boxSize = 0;
	tmp.model = model;
	tmp.pad = 0;

	ret = ff4_write_ftyp_payload(sess, NULL, 0, &tmp, &payloadSize, NULL);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outPlan, sizeof(*outPlan), &tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_write_ftyp_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4FtypPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_FTYP,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_ftyp_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

/* ---- Model destructors ---- */

int32_t ff4_free_meta_list_all(Ff4SessionImpl *sess, Ff4MetaDataList *meta)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (meta != NULL) {
		ff4_free_tref_list(sess, meta->trefHead);
		meta->trefHead = NULL;
		meta->trefTail = NULL;
		ff4_free_meta_list(sess, meta->dataHead);
		meta->dataHead = NULL;
		meta->dataTail = NULL;
	}
	return FF4_OK;
}

int32_t ff4_free_chunk_node_list(Ff4SessionImpl *sess, Ff4BoxChunkNode *head)
{
	Ff4BoxChunkNode *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4BoxChunkNode *next = cur->next;
		ff4_pool_free(sess, cur->data);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_esds_model(Ff4SessionImpl *sess, Ff4EsdsModel *esds)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (esds != NULL) {
		if (esds->kind == 0) {
			ff4_pool_free(sess, esds->decSpecific);
		} else if (esds->kind == 1) {
			ff4_pool_free(sess, esds->rawData);
		} else {
			return FF4_ERR_INTERNAL;
		}
		ff4_pool_free(sess, esds);
	}
	return FF4_OK;
}

int32_t ff4_free_avcc_model(Ff4SessionImpl *sess, Ff4AvccModel *avcc)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (avcc != NULL) {
		ff4_free_buf_list(sess, avcc->spsHead);
		ff4_free_buf_list(sess, avcc->ppsHead);
		ff4_pool_free(sess, avcc);
	}
	return FF4_OK;
}

int32_t ff4_free_stsd_entry_list(Ff4SessionImpl *sess, Ff4StsdEntry *head)
{
	Ff4StsdEntry *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4StsdEntry *next = cur->next;
		switch (cur->kind) {
		case 1:
			ff4_free_esds_model(sess, (Ff4EsdsModel *)cur->u.video.codecCfg);
			ff4_free_buf_node(sess, (Ff4NodeBuf *)cur->u.video.paspOrBtrt);
			ff4_free_chunk_node_list(sess, cur->u.video.extraHead);
			break;
		case 2:
			ff4_free_avcc_model(sess, (Ff4AvccModel *)cur->u.video.codecCfg);
			ff4_free_buf_node(sess, (Ff4NodeBuf *)cur->u.video.paspOrBtrt);
			ff4_free_chunk_node_list(sess, cur->u.video.extraHead);
			break;
		case 3:
			ff4_free_esds_model(sess, cur->u.audio.esds);
			ff4_free_chunk_node_list(sess, cur->u.audio.extraHead);
			break;
		case 4:
			ff4_free_esds_model(sess, (Ff4EsdsModel *)cur->u.generic.payload);
			break;
		case 5:
		case 7:
			break;
		case 6:
			ff4_free_chunk_node_list(sess, (Ff4BoxChunkNode *)cur->u.generic.payload);
			break;
		case 8:
		case 9:
			ff4_free_single(sess, cur->u.generic.subBox);
			break;
		default:
			return FF4_ERR_INTERNAL;
		}
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_mtsm_entry_list(Ff4SessionImpl *sess, Ff4MtsmEntry *head)
{
	Ff4MtsmEntry *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4MtsmEntry *next = cur->next;
		ff4_free_dref_list(sess, cur->drefHead);
		ff4_free_stsd_entry_list(sess, cur->stsdHead);
		ff4_free_triple_list(sess, cur->mdstHead);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_mtdt_entry_list(Ff4SessionImpl *sess, Ff4MtdtEntry *head)
{
	Ff4MtdtEntry *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4MtdtEntry *next = cur->next;
		ff4_pool_free(sess, cur->data);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_edit_list(Ff4SessionImpl *sess, Ff4EditListModel *edts)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (edts != NULL) {
		ff4_free_edit_entry_list(sess, edts->head);
		ff4_pool_free(sess, edts);
	}
	return FF4_OK;
}

int32_t ff4_free_track_list(Ff4SessionImpl *sess, Ff4TrackModel *head)
{
	Ff4TrackModel *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4TrackModel *next = cur->next;
		ff4_free_track_sample_tables(sess, &cur->trexMask);
		ff4_free_group_list(sess, cur->trefHead);
		ff4_free_edit_list(sess, cur->edts);
		ff4_pool_free(sess, cur->handlerName.data);
		ff4_free_dref_list(sess, cur->drefHead);
		ff4_free_stsd_entry_list(sess, cur->stsdHead);
		ff4_free_mtdt_entry_list(sess, cur->mtdtHead);
		ff4_free_meta_item_list(sess, (Ff4MetaItemNode *)cur->mtdfHead);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_movie_model(Ff4SessionImpl *sess, Ff4MovieModel *moov)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (moov != NULL) {
		ff4_pool_free(sess, moov->iods);
		ff4_free_track_list(sess, moov->trackHead);
		ff4_free_mtdt_entry_list(sess, moov->mtdtHead);
		ff4_free_meta_item_list(sess, (Ff4MetaItemNode *)moov->mtdfHead);
		ff4_free_mtsm_entry_list(sess, moov->mtsmHead);
		ff4_pool_free(sess, moov);
	}
	return FF4_OK;
}

int32_t ff4_free_prof_entry_list(Ff4SessionImpl *sess, Ff4ProfEntry *head)
{
	Ff4ProfEntry *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4ProfEntry *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_prof_model(Ff4SessionImpl *sess, Ff4ProfModel *prof)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (prof != NULL) {
		ff4_free_prof_entry_list(sess, prof->head);
		ff4_pool_free(sess, prof);
	}
	return FF4_OK;
}

int32_t ff4_free_compat_list(Ff4SessionImpl *sess, Ff4NodeU32 *head)
{
	Ff4NodeU32 *cur = head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4NodeU32 *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_ftyp_model(Ff4SessionImpl *sess, Ff4FtypModel *ftyp)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ftyp != NULL) {
		ff4_free_compat_list(sess, ftyp->compatHead);
		ff4_pool_free(sess, ftyp);
	}
	return FF4_OK;
}

int32_t ff4_free_writer_models(Ff4SessionImpl *sess, Ff4WriterModels *models)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models != NULL) {
		ff4_free_ftyp_model(sess, models->ftyp);
		ff4_free_prof_model(sess, models->prof);
		ff4_free_movie_model(sess, models->moov);
		ff4_free_meta_list_all(sess, &models->meta);
	}
	return FF4_OK;
}

typedef struct Ff4MtdtInputLocal {
	uint32_t fourcc;
	uint8_t isUtf16;
	uint8_t padA;
	uint16_t langCode;
	uint16_t dataType;
	uint16_t padB;
	uint8_t *data;
	uint32_t size;
} Ff4MtdtInputLocal;

int32_t ff4_free_mtdt_input(Ff4SessionImpl *sess, void *ptr)
{
	Ff4MtdtInputLocal *inp = (Ff4MtdtInputLocal *)ptr;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (inp != NULL) {
		ff4_pool_free(sess, inp->data);
		ff4_pool_free(sess, inp);
	}
	return FF4_OK;
}

/* ---- Plan destructors ---- */

int32_t ff4_free_extra_plan_list(Ff4SessionImpl *sess, void *head)
{
	Ff4StsdExtraPlan *cur = (Ff4StsdExtraPlan *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4StsdExtraPlan *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_esds_plan(Ff4SessionImpl *sess, void *plan)
{
	Ff4EsdsPlan *ep = (Ff4EsdsPlan *)plan;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ep != NULL) {
		if (ep->kind == 0) {
			ff4_pool_free(sess, ep->decBufA);
			ff4_pool_free(sess, ep->decBufB);
		} else if (ep->kind > 1) {
			return FF4_ERR_INTERNAL;
		}
		ff4_pool_free(sess, ep);
	}
	return FF4_OK;
}

int32_t ff4_free_plan_node_a(Ff4SessionImpl *sess, void *ptr)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ptr != NULL)
		ff4_pool_free(sess, ptr);
	return FF4_OK;
}

int32_t ff4_free_plan_node_b(Ff4SessionImpl *sess, void *ptr)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ptr != NULL)
		ff4_pool_free(sess, ptr);
	return FF4_OK;
}

int32_t ff4_free_plan_node_c(Ff4SessionImpl *sess, void *ptr)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ptr != NULL)
		ff4_pool_free(sess, ptr);
	return FF4_OK;
}

int32_t ff4_free_stsd_plan_list(Ff4SessionImpl *sess, void *head)
{
	Ff4StsdPlan *cur = (Ff4StsdPlan *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4StsdPlan *next = cur->next;
		switch (cur->kind) {
		case 1:
			ff4_free_esds_plan(sess, cur->u.video.codecPlan);
			ff4_free_plan_node_b(sess, cur->u.video.paspPlan);
			ff4_free_extra_plan_list(sess, cur->u.video.extraHead);
			break;
		case 2:
			ff4_free_plan_node_a(sess, cur->u.video.codecPlan);
			ff4_free_plan_node_b(sess, cur->u.video.paspPlan);
			ff4_free_extra_plan_list(sess, cur->u.video.extraHead);
			break;
		case 3:
			ff4_free_esds_plan(sess, cur->u.audio.esdsPlan);
			ff4_free_extra_plan_list(sess, cur->u.audio.extraHead);
			break;
		case 4:
			ff4_free_esds_plan(sess, (void *)(uintptr_t)cur->bodyWords[3]);
			break;
		case 5:
		case 7:
			break;
		case 6:
			ff4_free_extra_plan_list(sess, (void *)(uintptr_t)cur->bodyWords[3]);
			break;
		case 8:
		case 9:
			ff4_free_plan_node_c(sess, cur->u.audio.subBoxPlan);
			break;
		default:
			return FF4_ERR_INTERNAL;
		}
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_mtsm_plan_list(Ff4SessionImpl *sess, void *head)
{
	Ff4MtsmPlan *cur = (Ff4MtsmPlan *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4MtsmPlan *next = cur->next;
		ff4_free_stsd_plan_list(sess, cur->stsdHead);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_uuid_plan(Ff4SessionImpl *sess, void *plan)
{
	Ff4UuidPlan *up = (Ff4UuidPlan *)plan;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (up != NULL) {
		ff4_pool_free(sess, up->mtdtPlan);
		ff4_pool_free(sess, up->mtdfPlan);
		ff4_free_mtsm_plan_list(sess, up->mtsmHead);
		ff4_pool_free(sess, up);
	}
	return FF4_OK;
}

int32_t ff4_free_tref_plan_list(Ff4SessionImpl *sess, void *head)
{
	Ff4TrefTypePlan *cur = (Ff4TrefTypePlan *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4TrefTypePlan *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_tref_plan(Ff4SessionImpl *sess, void *plan)
{
	Ff4TrefPlan *tp = (Ff4TrefPlan *)plan;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (tp != NULL) {
		ff4_free_tref_plan_list(sess, tp->head);
		ff4_pool_free(sess, tp);
	}
	return FF4_OK;
}

int32_t ff4_free_trak_plan_list(Ff4SessionImpl *sess, void *head)
{
	Ff4TrakPlan *cur = (Ff4TrakPlan *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4TrakPlan *next = cur->next;
		ff4_free_tref_plan(sess, cur->trefPlan);
		ff4_pool_free(sess, cur->edtsPlan);
		ff4_free_stsd_plan_list(sess, cur->mdia.minf.stbl.stsdHead);
		ff4_pool_free(sess, cur->mdia.minf.stbl.cttsPlan);
		ff4_pool_free(sess, cur->mdia.minf.stbl.stssPlan);
		ff4_free_uuid_plan(sess, cur->uuidPlan);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_moov_plan(Ff4SessionImpl *sess, void *plan)
{
	Ff4MoovPlan *mp = (Ff4MoovPlan *)plan;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (mp != NULL) {
		ff4_free_mvex_plan(sess, mp->mvexPlan);
		ff4_pool_free(sess, mp->iodsPlan);
		ff4_free_trak_plan_list(sess, mp->trakHead);
		ff4_free_uuid_plan(sess, mp->uuidPlan);
		ff4_pool_free(sess, mp);
	}
	return FF4_OK;
}

int32_t ff4_free_run_block_list_a(Ff4SessionImpl *sess, void *head)
{
	Ff4RunEntryNode *cur = (Ff4RunEntryNode *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4RunEntryNode *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_run_block_list_b(Ff4SessionImpl *sess, void *head)
{
	Ff4RunEntryNode *cur = (Ff4RunEntryNode *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4RunEntryNode *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_run_block_list_c(Ff4SessionImpl *sess, void *head)
{
	Ff4RunEntryNode *cur = (Ff4RunEntryNode *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4RunEntryNode *next = cur->next;
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_run_group_list(Ff4SessionImpl *sess, void *head)
{
	Ff4RunGroupNode *cur = (Ff4RunGroupNode *)head;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		Ff4RunGroupNode *next = cur->next;
		ff4_free_run_block_list_a(sess, cur->listAHead);
		ff4_free_run_block_list_b(sess, cur->listBHead);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	return FF4_OK;
}

int32_t ff4_free_run_table(Ff4SessionImpl *sess, void *table)
{
	Ff4RunTable *rt = (Ff4RunTable *)table;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (rt != NULL) {
		ff4_free_run_group_list(sess, rt->groupHead);
		ff4_free_run_block_list_c(sess, rt->extraHead);
		ff4_pool_free(sess, rt);
	}
	return FF4_OK;
}

/* ---- List counters and lookup helpers ---- */

int32_t ff4_count_prof_entries(const Ff4ProfModel *prof, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	if (prof != NULL) {
		const Ff4ProfEntry *cur = prof->head;
		count = 1;
		while (cur != NULL) {
			count++;
			cur = cur->next;
		}
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_tracks(const Ff4TrackModel *head, uint32_t *outCount)
{
	uint32_t count = 0;
	const Ff4TrackModel *cur = head;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_mtdt_entries(const Ff4MtdtEntry *head, uint32_t *outCount)
{
	uint32_t count = 0;
	const Ff4MtdtEntry *cur = head;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_stsd_entries(const Ff4StsdEntry *head, uint32_t *outCount)
{
	uint32_t count = 0;
	const Ff4StsdEntry *cur = head;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_stsd_plans(const void *head, uint32_t *outCount)
{
	uint32_t count = 0;
	const Ff4StsdPlan *cur = (const Ff4StsdPlan *)head;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_mtsm_entries(const Ff4MtsmEntry *head, uint32_t *outCount)
{
	uint32_t count = 0;
	const Ff4MtsmEntry *cur = head;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		count++;
		cur = cur->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_find_u32_node(Ff4NodeU32 *head, uint32_t value, Ff4NodeU32 **outNode)
{
	Ff4NodeU32 *cur = head;

	if (outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (cur->value == value)
			break;
		cur = cur->next;
	}
	*outNode = cur;
	return FF4_OK;
}

int32_t ff4_find_track_by_id(Ff4TrackModel *head, uint32_t trackId,
			     Ff4TrackModel **outTrack, uint32_t *outIndex1)
{
	Ff4TrackModel *cur = head;
	uint32_t idx = 0;

	while (cur != NULL) {
		idx++;
		if (cur->trackId == trackId)
			break;
		cur = cur->next;
	}
	if (outTrack != NULL)
		*outTrack = cur;
	if (outIndex1 != NULL)
		*outIndex1 = idx;
	return FF4_OK;
}

int32_t ff4_get_track_at(Ff4TrackModel *head, uint32_t index1, Ff4TrackModel **outTrack)
{
	Ff4TrackModel *cur = head;
	uint32_t idx = 0;

	if (index1 == 0 || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (++idx == index1)
			break;
		cur = cur->next;
	}
	*outTrack = cur;
	return FF4_OK;
}

int32_t ff4_get_mtdt_at(Ff4MtdtEntry *head, uint32_t index1, Ff4MtdtEntry **outEntry)
{
	Ff4MtdtEntry *cur = head;
	uint32_t idx = 0;

	if (index1 == 0 || outEntry == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (++idx == index1)
			break;
		cur = cur->next;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_get_mtsm_at(Ff4MtsmEntry *head, uint32_t index1, Ff4MtsmEntry **outEntry)
{
	Ff4MtsmEntry *cur = head;
	uint32_t idx = 0;

	if (index1 == 0 || outEntry == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (++idx == index1)
			break;
		cur = cur->next;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_find_mtsm_by_id(Ff4MtsmEntry *head, uint32_t streamId, Ff4MtsmEntry **outEntry)
{
	Ff4MtsmEntry *cur = head;

	if (outEntry == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (cur->streamId == streamId)
			break;
		cur = cur->next;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_find_group_by_tag(Ff4NodeGroup *head, uint32_t tag, Ff4NodeGroup **outGroup)
{
	Ff4NodeGroup *cur = head;

	if (outGroup == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (cur->tag == tag)
			break;
		cur = cur->next;
	}
	*outGroup = cur;
	return FF4_OK;
}

int32_t ff4_find_u32_with_index(Ff4NodeU32 *head, uint32_t value,
				Ff4NodeU32 **outNode, uint32_t *outIndex1)
{
	Ff4NodeU32 *cur = head;
	uint32_t idx = 0;

	if (outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		idx++;
		if (cur->value == value)
			break;
		cur = cur->next;
	}
	*outNode = cur;
	if (outIndex1 != NULL)
		*outIndex1 = idx;
	return FF4_OK;
}

int32_t ff4_get_stsd_at(Ff4StsdEntry *head, uint32_t index1, Ff4StsdEntry **outEntry)
{
	Ff4StsdEntry *cur = head;
	uint32_t idx = 0;

	if (index1 == 0 || outEntry == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if (++idx == index1)
			break;
		cur = cur->next;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_find_tref_by_id(Ff4TrefEntry *head, uint16_t id, Ff4TrefEntry **outEntry)
{
	Ff4TrefEntry *cur = head;

	if (outEntry == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if ((uint16_t)cur->type == id)
			break;
		cur = cur->next;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_find_u8_node(Ff4NodeU32 *head, uint8_t val, Ff4NodeU32 **outNode)
{
	Ff4NodeU32 *cur = head;

	if (outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (cur != NULL) {
		if ((uint8_t)cur->value == val)
			break;
		cur = cur->next;
	}
	*outNode = cur;
	return FF4_OK;
}
