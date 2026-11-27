/*
 * Sony FF4 MP4 muxer: Media Information Box (minf), Media Header Boxes
 * (vmhd, smhd, nmhd), and Data Information / Reference Boxes (dinf, dref, url, urn).
 */
#include "ff4_internal.h"

int32_t ff4_set_vmhd_params(Ff4SessionImpl *sess, uint8_t hdrData[8], uint8_t mask,
			    const uint16_t *params)
{
	uint8_t tmp[8];

	if (sess == NULL || hdrData == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, tmp, sizeof(tmp), hdrData, sizeof(tmp));
	if ((mask & 1u) != 0) {
		tmp[0] = (uint8_t)(params[0] & 0xFFu);
		tmp[1] = (uint8_t)(params[0] >> 8);
		ff4_memcpy(sess, tmp + 2, 6u, params + 1, 6u);
	}
	ff4_memcpy(sess, hdrData, 8u, tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_smhd_params(Ff4SessionImpl *sess, uint8_t hdrData[2], uint8_t mask,
			    const uint8_t *params)
{
	uint8_t tmp[2];

	if (sess == NULL || hdrData == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, tmp, sizeof(tmp), hdrData, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp[1] = params[0];
	if ((mask & 2u) != 0)
		tmp[0] = params[1];
	ff4_memcpy(sess, hdrData, 2u, tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_copy_url_entry(Ff4SessionImpl *sess, const void *const *params, Ff4BoxDataBuf *outLoc)
{
	Ff4BoxDataBuf tmp;
	int32_t ret;

	if (params == NULL || outLoc == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	ret = ff4_copy_data_buf(sess, 1, &tmp, params[0], (uint32_t)(uintptr_t)params[1]);
	if (ret == FF4_OK)
		ff4_memcpy(sess, outLoc, sizeof(*outLoc), &tmp, sizeof(tmp));
	return ret;
}

int32_t ff4_copy_urn_entry(Ff4SessionImpl *sess, const void *const *params,
			   Ff4BoxDataBuf *outNameAndLoc)
{
	Ff4BoxDataBuf tmp[2];
	char emptyStr[4];
	int32_t ret;

	if (params == NULL || outNameAndLoc == NULL)
		return FF4_ERR_INVALID_ARG;

	emptyStr[0] = '\0';
	ff4_memset(sess, tmp, 0, sizeof(tmp));
	if (params[1] != NULL)
		ret = ff4_copy_data_buf(sess, 1, &tmp[0], params[0], (uint32_t)(uintptr_t)params[1]);
	else
		ret = ff4_copy_data_buf(sess, 1, &tmp[0], emptyStr, 1u);

	if (ret == FF4_OK) {
		ret = ff4_copy_data_buf(sess, 1, &tmp[1], params[2], (uint32_t)(uintptr_t)params[3]);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outNameAndLoc, sizeof(tmp), tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_add_dref_entry(Ff4SessionImpl *sess, Ff4DrefEntry **listHeadTail,
			   const uint32_t *entryIn, uint32_t *outIndex)
{
	Ff4DrefEntry *node = NULL;
	uint32_t count = 0;
	int32_t ret = FF4_OK;

	if (listHeadTail == NULL || entryIn == NULL || outIndex == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4DrefEntry *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	if (entryIn[0] == FF4_BOX_URL) {
		ret = ff4_copy_url_entry(sess, (const void *const *)(entryIn + 1),
					 (Ff4BoxDataBuf *)&node->location);
	} else if (entryIn[0] == FF4_BOX_URN) {
		ret = ff4_copy_urn_entry(sess, (const void *const *)(entryIn + 1),
					 (Ff4BoxDataBuf *)&node->location);
	} else {
		ret = FF4_ERR_INTERNAL;
	}

	if (ret == FF4_OK) {
		node->type = entryIn[0];
		if (listHeadTail[0] != NULL) {
			listHeadTail[1]->next = node;
		} else {
			listHeadTail[0] = node;
		}
		listHeadTail[1] = node;
		node = NULL;
		ret = ff4_count_dref_list(listHeadTail[0], &count);
		if (ret == FF4_OK)
			*outIndex = count;
	}

	ff4_free_dref_list(sess, node);
	return ret;
}

int32_t ff4_write_url_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4BoxDataBuf *loc, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[4];
	uint64_t total = 0;
	uint32_t flags;
	int32_t ret;

	if (sess == NULL || loc == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	flags = (loc->size == 0) ? 1u : 0u;
	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr, flags);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, loc->data, loc->size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_urn_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			      const Ff4BoxDataBuf *nameAndLoc, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || nameAndLoc == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, nameAndLoc[0].data, nameAndLoc[0].size,
				    NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_box_write(sess, fileCtx, nameAndLoc[1].data, nameAndLoc[1].size,
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

int32_t ff4_write_dref_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const Ff4DrefEntry *entry,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || entry == NULL)
		return FF4_ERR_INVALID_ARG;

	if (entry->type == FF4_BOX_URL) {
		ret = ff4_calc_box_size(sess, opts, entry->locationSz + 4u, &boxSize);
	} else if (entry->type == FF4_BOX_URN) {
		ret = ff4_calc_box_size(sess, opts, entry->locationSz + entry->nameSz + 4u, &boxSize);
	} else {
		return FF4_ERR_INTERNAL;
	}
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, entry->type, NULL, &total);
	if (ret != FF4_OK)
		return ret;

	if (entry->type == FF4_BOX_URL) {
		ret = ff4_write_url_payload(sess, fileCtx, pos + total,
					    (const Ff4BoxDataBuf *)&entry->location, NULL, &total);
	} else if (entry->type == FF4_BOX_URN) {
		ret = ff4_write_urn_payload(sess, fileCtx, pos + total,
					    (const Ff4BoxDataBuf *)&entry->location, NULL, &total);
	} else {
		return FF4_ERR_INTERNAL;
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_dref_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4DrefPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	uint64_t total = 0;
	const Ff4DrefEntry *entry;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_count_dref_list(plan->head, &count);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 4, count);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (entry = plan->head; entry != NULL; entry = entry->next) {
		ret = ff4_write_dref_entry_box(sess, opts, fileCtx, pos + total, entry,
					       NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_dref_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4DrefPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_DREF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dref_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_dinf_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4DinfPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_dref_box(sess, opts, fileCtx, pos, &plan->dref, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_dinf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4DinfPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_DINF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dinf_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_nmhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_nmhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan->fourcc != FF4_BOX_NMHD)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_NMHD,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_nmhd_payload(sess, fileCtx, pos + total, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_smhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint16_t *balance, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[8];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || balance == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 4, *balance);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_smhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan->fourcc != FF4_BOX_SMHD)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_SMHD,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_smhd_payload(sess, fileCtx, pos + total,
					     (const uint16_t *)plan->hdrData, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_vmhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint16_t *vmhdWords, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[12];
	uint64_t total = 0;
	int32_t i;
	int32_t ret;

	if (sess == NULL || vmhdWords == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[3] = 1;
	ff4_put_be16(buf + 4, vmhdWords[0]);
	for (i = 0; i < 3; i++)
		ff4_put_be16(buf + 6 + i * 2, vmhdWords[1 + i]);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_vmhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfHdrPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan->fourcc != FF4_BOX_VMHD)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_VMHD,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_vmhd_payload(sess, fileCtx, pos + total,
					     (const uint16_t *)plan->hdrData, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_minf_hdr_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MinfHdrPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan->fourcc == FF4_BOX_VMHD)
		ret = ff4_write_vmhd_box(sess, opts, fileCtx, pos, plan, NULL, &total);
	else if (plan->fourcc == FF4_BOX_SMHD)
		ret = ff4_write_smhd_box(sess, opts, fileCtx, pos, plan, NULL, &total);
	else if (plan->fourcc == FF4_BOX_NMHD)
		ret = ff4_write_nmhd_box(sess, opts, fileCtx, pos, plan, NULL, &total);
	else
		return FF4_ERR_INTERNAL;

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_minf_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MinfPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_minf_hdr_box(sess, opts, fileCtx, pos, &plan->hdr, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dinf_box(sess, opts, fileCtx, pos + total, &plan->dinf,
					 NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_stbl_box(sess, opts, fileCtx, pos + total, &plan->stbl,
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

int32_t ff4_plan_dref(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4DrefEntry *const *listHeadTail,
		      Ff4DrefPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || listHeadTail == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &outPlan->head, 8u, listHeadTail, 8u);
	ret = ff4_write_dref_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_plan_dinf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4DrefEntry *const *listHeadTail,
		      Ff4DinfPlan *outPlan)
{
	uint64_t drefTotal = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || listHeadTail == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_dref(sess, opts, listHeadTail, &outPlan->dref);
	if (ret == FF4_OK) {
		ret = ff4_write_dref_box(sess, opts, NULL, 0, &outPlan->dref, NULL, &drefTotal);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, drefTotal, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_minf_hdr(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const void *minfHdrModel,
			  Ff4MinfHdrPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || minfHdrModel == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &outPlan->fourcc, 12u, minfHdrModel, 12u);
	if (outPlan->fourcc == FF4_BOX_VMHD)
		ret = ff4_write_vmhd_payload(sess, NULL, 0, (const uint16_t *)outPlan->hdrData,
					     &payloadSize, NULL);
	else if (outPlan->fourcc == FF4_BOX_SMHD)
		ret = ff4_write_smhd_payload(sess, NULL, 0, (const uint16_t *)outPlan->hdrData,
					     &payloadSize, NULL);
	else if (outPlan->fourcc == FF4_BOX_NMHD)
		ret = ff4_write_nmhd_payload(sess, NULL, 0, &payloadSize, NULL);
	else
		return FF4_ERR_INTERNAL;

	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_plan_minf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4MinfPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_minf_hdr(sess, opts, &track->minfType, &outPlan->hdr);
	if (ret == FF4_OK) {
		ret = ff4_plan_dinf(sess, opts, &track->drefHead, &outPlan->dinf);
		if (ret == FF4_OK) {
			ret = ff4_plan_stbl(sess, opts, track, &outPlan->stbl);
			if (ret == FF4_OK) {
				ret = ff4_write_minf_payload(sess, opts, NULL, 0, outPlan,
							     &payloadSize, NULL);
				if (ret == FF4_OK)
					ret = ff4_calc_box_size(sess, opts, payloadSize,
								&outPlan->boxSize);
			}
		}
	}
	return ret;
}

int32_t ff4_write_minf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MinfPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MINF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_minf_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}
