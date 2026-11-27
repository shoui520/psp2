/*
 * Sony FF4 MP4 muxer: Track Box (trak), Track Header (tkhd), Edit List
 * (edts/elst), Track Reference (tref), and track creation helpers.
 */
#include "ff4_internal.h"

#define FF4_HDLR_PDAT FF4_FCC('P','D','A','T')
#define FF4_HDLR_BOOK FF4_FCC('B','O','O','K')
#define FF4_HDLR_ODSM FF4_FCC('o','d','s','m')
#define FF4_HDLR_GRPH FF4_FCC('G','R','P','H')
#define FF4_HDLR_SOUN FF4_FCC('s','o','u','n')
#define FF4_HDLR_VIDE FF4_FCC('v','i','d','e')

typedef struct Ff4TkhdParamInput {
	uint32_t flags;
	uint32_t padA;
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t trackId;
	uint32_t padB;
	uint64_t duration;
	int16_t layer;
	int16_t altGroup;
	uint8_t volHi;
	uint8_t volLo;
	uint16_t padC;
	int32_t matrix[9];
	uint16_t widthHi;
	uint16_t widthLo;
	uint16_t heightHi;
	uint16_t heightLo;
} Ff4TkhdParamInput;

typedef struct Ff4ElstPlan {
	uint64_t boxSize;
	uint8_t version;
	uint8_t pad[3];
	Ff4EditListModel *edts;
} Ff4ElstPlan;

typedef struct Ff4EdtsPlan {
	uint64_t boxSize;
	Ff4ElstPlan elst;
} Ff4EdtsPlan;

typedef struct Ff4TkhdPlanLocal {
	uint8_t version;
	uint8_t padA[7];
	uint32_t mask;
	uint32_t flags;
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t trackId;
	uint32_t padB;
	uint64_t duration;
	int16_t layer;
	int16_t altGroup;
	int16_t volume;
	uint16_t padC;
	int32_t matrix[9];
	uint32_t width;
	uint32_t height;
	uint32_t padD;
} Ff4TkhdPlanLocal;

static inline int32_t ff4_init_track_common(Ff4SessionImpl *sess, uint32_t mediaKind, uint32_t tkhdMask,
				     uint32_t trackId, uint32_t timescale, uint16_t lang,
				     int32_t noEdts, uint32_t hdlrType, const char *hdlrName,
				     uint32_t hdlrNameLen, uint32_t minfType, int32_t noZpck,
				     Ff4TrackModel **outTrack)
{
	Ff4TrackModel *trk = NULL;
	uint32_t tkhdFlags = 0;
	uint32_t zpckVal[2];
	int32_t ret;

	ff4_memset(sess, zpckVal, 0, sizeof(zpckVal));
	trk = (Ff4TrackModel *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trk));
	if (trk == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_track_type_to_tkhd_flags(mediaKind, &tkhdFlags);
	if (ret == FF4_OK) {
		trk->tkhdMask = tkhdMask;
		trk->tkhdFlags = tkhdFlags;
		trk->trackId = trackId;
		trk->matrix[0] = 0x10000;
		trk->matrix[4] = 0x10000;
		trk->matrix[8] = 0x40000000;
		trk->mdhdTimescale = timescale;
		trk->mdhdLanguage = (uint16_t)(lang & 0x7FFFu);

		if (noEdts == 0) {
			trk->edts = (Ff4EditListModel *)ff4_pool_alloc(sess, FF4_POOL_META,
								       sizeof(*trk->edts));
			if (trk->edts == NULL)
				ret = FF4_ERR_NO_MEM;
		}
	}
	if (ret == FF4_OK) {
		trk->handlerType = hdlrType;
		ret = ff4_copy_data_buf(sess, 1, &trk->handlerName, hdlrName, hdlrNameLen);
	}
	if (ret == FF4_OK) {
		trk->minfType = minfType;
		if (noZpck == 0) {
			zpckVal[0] = mediaKind;
			ret = ff4_add_mtdt_zpck_header(sess, &trk->mtdtHead, 0, zpckVal);
		}
	}

	if (ret == FF4_OK) {
		*outTrack = trk;
		trk = NULL;
	}
	ff4_free_track_list(sess, trk);
	return ret;
}

int32_t ff4_create_pdat_track(Ff4SessionImpl *sess, const uint32_t *params, Ff4TrackModel **outTrack)
{
	if (sess == NULL || params == NULL || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_init_track_common(sess, 1u, 8u, params[2], 1u, (uint16_t)params[3],
				     (int32_t)params[1], FF4_HDLR_PDAT,
				     "Packed Data Media Handler", 26u,
				     FF4_BOX_NMHD, (int32_t)params[0], outTrack);
}

int32_t ff4_create_book_track(Ff4SessionImpl *sess, const uint32_t *params, Ff4TrackModel **outTrack)
{
	if (sess == NULL || params == NULL || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_init_track_common(sess, 1u, 8u, params[2], 1u, (uint16_t)params[3],
				     (int32_t)params[1], FF4_HDLR_BOOK,
				     "Book Media Handler", 19u,
				     FF4_BOX_NMHD, (int32_t)params[0], outTrack);
}

int32_t ff4_create_odsm_track(Ff4SessionImpl *sess, const uint32_t *params, uint32_t movieTimescale,
			      Ff4TrackModel **outTrack)
{
	if (sess == NULL || params == NULL || movieTimescale == 0 || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_init_track_common(sess, 1u, 8u, params[2], movieTimescale, 21956u,
				     (int32_t)params[1], FF4_HDLR_ODSM,
				     "ObjectDescriptor Media Handler", 31u,
				     FF4_BOX_NMHD, (int32_t)params[0], outTrack);
}

typedef struct Ff4VideoTrackInit {
	uint32_t noZpck;
	uint32_t noEdts;
	uint32_t trackId;
	uint16_t widthHi;
	uint16_t widthLo;
	uint16_t heightHi;
	uint16_t heightLo;
	uint32_t timescale;
	uint16_t language;
	uint16_t pad;
	uint32_t mediaKind;
} Ff4VideoTrackInit;

typedef struct Ff4AudioTrackInit {
	uint32_t noZpck;
	uint32_t noEdts;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t pad;
} Ff4AudioTrackInit;

int32_t ff4_create_grph_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack)
{
	const Ff4VideoTrackInit *p = (const Ff4VideoTrackInit *)params;
	Ff4TrackModel *trk = NULL;
	int32_t ret;

	if (sess == NULL || p == NULL || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_track_common(sess, 8u, 15368u, p->trackId, p->timescale, p->language,
				    (int32_t)p->noEdts, FF4_HDLR_GRPH,
				    "Graphics Media Handler", 23u,
				    FF4_BOX_NMHD, (int32_t)p->noZpck, &trk);
	if (ret == FF4_OK) {
		trk->layer = -1;
		trk->width = ((uint32_t)p->widthHi << 16) | p->widthLo;
		trk->height = ((uint32_t)p->heightHi << 16) | p->heightLo;
		trk->mdhdMask = 20u;
		*outTrack = trk;
	}
	return ret;
}

int32_t ff4_create_soun_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack)
{
	const Ff4AudioTrackInit *p = (const Ff4AudioTrackInit *)params;
	Ff4TrackModel *trk = NULL;
	int32_t ret;

	if (sess == NULL || p == NULL || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_track_common(sess, 1u, 8u, p->trackId, p->timescale, p->language,
				    (int32_t)p->noEdts, FF4_HDLR_SOUN,
				    "Sound Media Handler", 20u,
				    FF4_BOX_SMHD, (int32_t)p->noZpck, &trk);
	if (ret == FF4_OK) {
		trk->volume = 256;
		trk->mdhdMask = 20u;
		*outTrack = trk;
	}
	return ret;
}

int32_t ff4_create_vide_track(Ff4SessionImpl *sess, const void *params, Ff4TrackModel **outTrack)
{
	const Ff4VideoTrackInit *p = (const Ff4VideoTrackInit *)params;
	Ff4TrackModel *trk = NULL;
	int32_t ret;

	if (sess == NULL || p == NULL || outTrack == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_track_common(sess, p->mediaKind, 15368u, p->trackId, p->timescale,
				    p->language, (int32_t)p->noEdts, FF4_HDLR_VIDE,
				    "Video Media Handler", 20u,
				    FF4_BOX_VMHD, (int32_t)p->noZpck, &trk);
	if (ret == FF4_OK) {
		trk->width = ((uint32_t)p->widthHi << 16) | p->widthLo;
		trk->height = ((uint32_t)p->heightHi << 16) | p->heightLo;
		trk->mdhdMask = 20u;
		*outTrack = trk;
	}
	return ret;
}

int32_t ff4_add_track(Ff4SessionImpl *sess, Ff4MovieModel *moov, const uint32_t *params,
		      uint32_t *outTrackIndex)
{
	Ff4TrackModel *trk = NULL;
	uint32_t count = 0;
	int32_t ret;

	if (sess == NULL || moov == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	switch (params[0]) {
	case FF4_HDLR_VIDE:
		ret = ff4_create_vide_track(sess, params + 1, &trk);
		break;
	case FF4_HDLR_SOUN:
		ret = ff4_create_soun_track(sess, params + 1, &trk);
		break;
	case FF4_HDLR_ODSM:
		ret = ff4_create_odsm_track(sess, params + 1, moov->timescale, &trk);
		break;
	case FF4_HDLR_PDAT:
		ret = ff4_create_pdat_track(sess, params + 1, &trk);
		break;
	case FF4_HDLR_GRPH:
		ret = ff4_create_grph_track(sess, params + 1, &trk);
		break;
	case FF4_HDLR_BOOK:
		ret = ff4_create_book_track(sess, params + 1, &trk);
		break;
	default:
		return FF4_ERR_INTERNAL;
	}

	if (ret != FF4_OK)
		return ret;
	if (trk == NULL)
		return FF4_ERR_INTERNAL;

	if (moov->trackHead != NULL)
		moov->trackTail->next = trk;
	else
		moov->trackHead = trk;
	moov->trackTail = trk;

	ret = ff4_count_tracks(moov->trackHead, &count);
	if (ret == FF4_OK)
		*outTrackIndex = count;
	return ret;
}

int32_t ff4_set_tkhd_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
			    uint32_t mask, const void *params)
{
	uint8_t tmpBuf[96];
	Ff4TrackModel *tmp = (Ff4TrackModel *)tmpBuf;
	const Ff4TkhdParamInput *in = (const Ff4TkhdParamInput *)params;

	if (sess == NULL || opts == NULL || track == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, tmpBuf, sizeof(tmpBuf), track, sizeof(tmpBuf));

	if ((mask & 1u) != 0) {
		if ((in->flags & 0xFF000000u) != 0)
			return FF4_ERR_INVALID_ARG;
		tmp->tkhdFlags = in->flags & 0xFFFFFFu;
	}
	if ((mask & 2u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->creationTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->creationTime = in->creationTime;
	}
	if ((mask & 4u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->modTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->modTime = in->modTime;
	}
	if ((mask & 8u) != 0)
		tmp->trackId = in->trackId;
	if ((mask & 0x10u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (in->duration >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp->duration = in->duration;
	}
	if ((mask & 0x20u) != 0)
		tmp->layer = in->layer;
	if ((mask & 0x40u) != 0)
		tmp->altGroup = in->altGroup;
	if ((mask & 0x80u) != 0)
		tmp->volume = (int16_t)(((uint16_t)tmp->volume & 0x00FFu) |
					((uint16_t)in->volHi << 8));
	if ((mask & 0x100u) != 0)
		tmp->volume = (int16_t)(((uint16_t)tmp->volume & 0xFF00u) |
					(uint16_t)in->volLo);
	if ((mask & 0x200u) != 0)
		ff4_memcpy(sess, tmp->matrix, sizeof(tmp->matrix), in->matrix, sizeof(tmp->matrix));
	if ((mask & 0x400u) != 0)
		tmp->width = (tmp->width & 0x0000FFFFu) | ((uint32_t)in->widthHi << 16);
	if ((mask & 0x800u) != 0)
		tmp->width = (tmp->width & 0xFFFF0000u) | (uint32_t)in->widthLo;
	if ((mask & 0x1000u) != 0)
		tmp->height = (tmp->height & 0x0000FFFFu) | ((uint32_t)in->heightHi << 16);
	if ((mask & 0x2000u) != 0)
		tmp->height = (tmp->height & 0xFFFF0000u) | (uint32_t)in->heightLo;

	tmp->tkhdMask |= mask;
	ff4_memcpy(sess, track, sizeof(tmpBuf), tmpBuf, sizeof(tmpBuf));
	return FF4_OK;
}

int32_t ff4_set_tkhd_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
				uint8_t flags, uint64_t creationTime, uint64_t modTime)
{
	Ff4TkhdParamInput in;
	uint32_t mask = 0;

	if (sess == NULL || opts == NULL || track == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &in, 0, sizeof(in));
	if ((flags & 4u) != 0) {
		mask |= 2u;
		in.creationTime = creationTime;
	}
	if ((flags & 8u) != 0) {
		mask |= 4u;
		in.modTime = modTime;
	}
	return ff4_set_tkhd_params(sess, opts, track, mask, &in);
}

int32_t ff4_is_valid_track_id(uint32_t trackId, int32_t *outValid)
{
	if (outValid == NULL)
		return FF4_ERR_INVALID_ARG;
	*outValid = ((trackId != 0 && trackId <= 0x7Fu) ||
		     (trackId >= 0x400u && trackId <= 0xF7FFu)) ? 1 : 0;
	return FF4_OK;
}

int32_t ff4_alloc_next_track_id(Ff4SessionImpl *sess, const Ff4TrackModel *head, uint32_t *outId)
{
	const Ff4TrackModel *cur;
	uint32_t maxId = 0;
	uint32_t candidate;

	if (sess == NULL || outId == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = head; cur != NULL; cur = cur->next) {
		if (maxId < cur->trackId)
			maxId = cur->trackId;
	}

	if (maxId < 0x7Fu) {
		*outId = maxId + 1u;
		return FF4_OK;
	}
	if (maxId < 0x400u) {
		*outId = 0x400u;
		return FF4_OK;
	}
	if (maxId < 0xF7FFu) {
		*outId = maxId + 1u;
		return FF4_OK;
	}

	for (candidate = 1u; candidate <= 0x7Fu; candidate++) {
		for (cur = head; cur != NULL; cur = cur->next) {
			if (cur->trackId == candidate)
				break;
		}
		if (cur == NULL) {
			*outId = candidate;
			return FF4_OK;
		}
	}
	for (candidate = 0x400u; candidate <= 0xF7FFu; candidate++) {
		for (cur = head; cur != NULL; cur = cur->next) {
			if (cur->trackId == candidate)
				break;
		}
		if (cur == NULL) {
			*outId = candidate;
			return FF4_OK;
		}
	}
	return FF4_ERR_NOT_FOUND;
}

int32_t ff4_is_track_id_available(uint32_t trackId, const Ff4TrackModel *head, int32_t *outAvail)
{
	int32_t valid = 0;
	Ff4TrackModel *found = NULL;
	int32_t ret = FF4_OK;

	if (outAvail == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_is_valid_track_id(trackId, &valid);
	if (valid != 0) {
		ret = ff4_find_track_by_id((Ff4TrackModel *)head, trackId, &found, NULL);
		if (ret != FF4_OK)
			return ret;
		valid = (found == NULL) ? 1 : 0;
	}
	*outAvail = valid;
	return FF4_OK;
}

int32_t ff4_classify_id_box_type(uint32_t id, uint32_t *outFourcc)
{
	int32_t isTrack = 0;
	int32_t isMtsm = 0;
	int32_t ret;

	if (outFourcc == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_is_valid_track_id(id, &isTrack);
	ret = ff4_is_valid_mtsm_id(id, &isMtsm);
	if (ret != FF4_OK)
		return ret;

	if (isTrack != 0 && isMtsm == 0)
		*outFourcc = FF4_BOX_TKHD;
	else if (isTrack == 0 && isMtsm != 0)
		*outFourcc = FF4_BOX_MTHD;
	else
		return FF4_ERR_INVALID_ARG;
	return FF4_OK;
}

int32_t ff4_remap_track_id_refs(Ff4SessionImpl *sess, Ff4MovieModel *moov,
				uint32_t oldId, uint32_t newId)
{
	Ff4MtdtEntry *mtdt;
	Ff4TrackModel *trk;
	Ff4ZpckTable *table = NULL;
	int32_t ret = FF4_OK;

	if (moov == NULL)
		return FF4_ERR_INVALID_ARG;

	if (moov->iods != NULL && moov->iods->esId == oldId)
		moov->iods->esId = newId;

	for (mtdt = moov->mtdtHead; mtdt != NULL; mtdt = mtdt->next) {
		if (mtdt->dataType == 257u) {
			Ff4NodePair *pair;
			table = (Ff4ZpckTable *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*table));
			if (table == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_parse_zpck_table(sess, mtdt->data, 0, mtdt->size, table);
			if (ret != FF4_OK)
				break;
			for (pair = table->head; pair != NULL; pair = pair->next) {
				if (pair->a == oldId)
					pair->a = newId;
			}
			ret = ff4_write_zpck_table_to_buf(sess, (Ff4BoxDataBuf *)&mtdt->data, table);
			if (ret != FF4_OK)
				break;
			ff4_free_zpck_table(sess, table);
			table = NULL;
		}
	}

	if (ret == FF4_OK) {
		for (trk = moov->trackHead; trk != NULL; trk = trk->next) {
			Ff4NodeGroup *grp;
			for (grp = trk->trefHead; grp != NULL; grp = grp->next) {
				Ff4NodeU32 *idNode;
				for (idNode = grp->children; idNode != NULL; idNode = idNode->next) {
					if (idNode->value == oldId)
						idNode->value = newId;
				}
			}
		}
	}

	ff4_free_zpck_table(sess, table);
	return ret;
}

int32_t ff4_add_tref_target_id(Ff4SessionImpl *sess, Ff4NodeGroup *group, const uint32_t *param,
			       uint32_t *outIndex)
{
	Ff4NodeU32 *found = NULL;
	Ff4NodeU32 *node;
	uint32_t idx = 0;
	int32_t ret;

	if (sess == NULL || group == NULL || param == NULL || outIndex == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_find_u32_with_index(group->children, param[1], &found, &idx);
	if (ret != FF4_OK)
		return ret;

	if (found == NULL) {
		node = (Ff4NodeU32 *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL)
			return FF4_ERR_NO_MEM;
		node->value = param[1];
		if (group->children != NULL)
			group->childrenTail->next = node;
		else
			group->children = node;
		group->childrenTail = node;
		ret = ff4_count_u32_list(group->children, &idx);
	}
	if (ret == FF4_OK)
		*outIndex = idx;
	return ret;
}

int32_t ff4_add_track_reference(Ff4SessionImpl *sess, Ff4TrackModel *track, const uint32_t *param,
				uint32_t *outIndex)
{
	Ff4NodeGroup *found = NULL;
	Ff4NodeGroup *node = NULL;
	uint32_t idx = 0;
	int32_t ret;

	if (sess == NULL || track == NULL || param == NULL || outIndex == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_find_group_by_tag(track->trefHead, param[0], &found);
	if (ret == FF4_OK) {
		if (found != NULL) {
			ret = ff4_add_tref_target_id(sess, found, param, &idx);
		} else {
			node = (Ff4NodeGroup *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
			if (node == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				node->tag = param[0];
				ret = ff4_add_tref_target_id(sess, node, param, &idx);
				if (ret == FF4_OK) {
					if (track->trefHead != NULL)
						track->trefTail->next = node;
					else
						track->trefHead = node;
					track->trefTail = node;
					node = NULL;
				}
			}
		}
		if (ret == FF4_OK)
			*outIndex = idx;
	}

	ff4_free_group_list(sess, node);
	return ret;
}

typedef struct Ff4EditEntryInput {
	uint64_t segmentDur;
	int64_t mediaTime;
	int16_t mediaRateInt;
	int16_t mediaRateFrac;
} Ff4EditEntryInput;

int32_t ff4_append_edit_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			      Ff4EditListModel *edts, const uint32_t *entryIn)
{
	const Ff4EditEntryInput *in = (const Ff4EditEntryInput *)entryIn;
	Ff4EditEntryNode *node;

	if (sess == NULL || opts == NULL || edts == NULL || entryIn == NULL)
		return FF4_ERR_INVALID_ARG;

	if (opts->boxVerMode == 0 || opts->boxVerMode == 2) {
		if ((in->segmentDur >> 32) != 0 || ((uint64_t)in->mediaTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
	}

	node = (Ff4EditEntryNode *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	ff4_memcpy(sess, node->f, 20u, entryIn, 20u);
	if (edts->head != NULL)
		edts->tail->next = node;
	else
		edts->head = node;
	edts->tail = node;
	return FF4_OK;
}

int32_t ff4_add_track_edit(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   Ff4TrackModel *track, const uint32_t *entryIn)
{
	Ff4EditListModel *created = NULL;
	int32_t ret;

	if (sess == NULL || opts == NULL || track == NULL || entryIn == NULL)
		return FF4_ERR_INVALID_ARG;

	if (track->edts != NULL) {
		ret = ff4_append_edit_entry(sess, opts, track->edts, entryIn);
	} else {
		created = (Ff4EditListModel *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*created));
		if (created == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_append_edit_entry(sess, opts, created, entryIn);
			if (ret == FF4_OK) {
				track->edts = created;
				created = NULL;
			}
		}
	}

	ff4_free_edit_list(sess, created);
	return ret;
}

int32_t ff4_write_elst_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4ElstPlan *plan = (const Ff4ElstPlan *)planPtr;
	Ff4EditListModel edts;
	uint8_t hdr[8];
	uint8_t row[20];
	uint32_t count = 0;
	uint64_t total = 0;
	const Ff4EditEntryNode *cur;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &edts, sizeof(edts), plan->edts, sizeof(edts));
	ret = ff4_count_edit_entry_list(edts.head, &count);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	hdr[0] = plan->version;
	ff4_put_be32(hdr + 4, count);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = edts.head; cur != NULL; cur = cur->next) {
		const Ff4EditEntryInput *in = (const Ff4EditEntryInput *)cur->f;
		uint32_t rowLen;
		ff4_memset(sess, row, 0, sizeof(row));
		if (plan->version == 1) {
			ff4_put_be64(row + 0, in->segmentDur);
			ff4_put_be64(row + 8, (uint64_t)in->mediaTime);
			ff4_put_be16(row + 16, (uint16_t)in->mediaRateInt);
			ff4_put_be16(row + 18, (uint16_t)in->mediaRateFrac);
			rowLen = 20;
		} else {
			ff4_put_be32(row + 0, (uint32_t)in->segmentDur);
			ff4_put_be32(row + 4, (uint32_t)in->mediaTime);
			ff4_put_be16(row + 8, (uint16_t)in->mediaRateInt);
			ff4_put_be16(row + 10, (uint16_t)in->mediaRateFrac);
			rowLen = 12;
		}
		ret = ff4_box_write(sess, fileCtx, row, rowLen, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_elst_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4ElstPlan *plan = (const Ff4ElstPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_ELST,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_elst_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_edts_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *planPtr,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EdtsPlan *plan = (const Ff4EdtsPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_elst_box(sess, opts, fileCtx, pos, &plan->elst, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_edts_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EdtsPlan *plan = (const Ff4EdtsPlan *)planPtr;
	uint64_t total = 0;
	uint64_t body = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_EDTS,
					   NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_elst_box(sess, opts, fileCtx, pos + total, &plan->elst,
						 NULL, &body);
			if (ret == FF4_OK)
				total += body;
		}
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_tref_type_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4TrefTypePlan *plan,
				    uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4NodeGroup grp;
	uint8_t buf[4];
	uint64_t total = 0;
	const Ff4NodeU32 *cur;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &grp, sizeof(grp), plan->group, sizeof(grp));
	for (cur = grp.children; cur != NULL; cur = cur->next) {
		ff4_put_be32(buf, cur->value);
		ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}
	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_tref_type_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				uint64_t pos, const Ff4TrefTypePlan *plan,
				uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan->group == NULL)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->group->tag,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_tref_type_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_tref_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4TrefPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TrefTypePlan *cur;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_write_tref_type_box(sess, opts, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}
	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_tref_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrefPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	uint64_t body = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TREF,
					   NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_tref_payload(sess, opts, fileCtx, pos + total, plan,
						     &body, NULL);
			if (ret == FF4_OK)
				total += body;
		}
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_tkhd_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TkhdPlanLocal *plan = (const Ff4TkhdPlanLocal *)planPtr;
	uint8_t buf[96];
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
	ff4_put_be32(buf + 0, ((uint32_t)plan->version << 24) | (plan->flags & 0xFFFFFFu));
	if (plan->version == 1) {
		ff4_put_be64(buf + 4, plan->creationTime);
		ff4_put_be64(buf + 12, plan->modTime);
		ff4_put_be32(buf + 20, plan->trackId);
		ff4_put_be64(buf + 28, plan->duration);
		off = 44;
	} else {
		ff4_put_be32(buf + 4, (uint32_t)plan->creationTime);
		ff4_put_be32(buf + 8, (uint32_t)plan->modTime);
		ff4_put_be32(buf + 12, plan->trackId);
		ff4_put_be32(buf + 20, (uint32_t)plan->duration);
		off = 32;
	}

	ff4_put_be16(buf + off + 0, (uint16_t)plan->layer);
	ff4_put_be16(buf + off + 2, (uint16_t)plan->altGroup);
	ff4_put_be16(buf + off + 4, (uint16_t)plan->volume);
	off += 8;

	for (i = 0; i < 9; i++) {
		ff4_put_be32(buf + off, (uint32_t)plan->matrix[i]);
		off += 4;
	}
	ff4_put_be32(buf + off + 0, plan->width);
	ff4_put_be32(buf + off + 4, plan->height);
	off += 8;

	ret = ff4_box_write(sess, fileCtx, buf, off, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_tkhd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TkhdPlanLocal *plan = (const Ff4TkhdPlanLocal *)planPtr;
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, (plan->version == 1) ? 96u : 84u, &boxSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_TKHD, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_tkhd_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_trak_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4TrakPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_tkhd_box(sess, opts, fileCtx, pos, &plan->tkhdVersion, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_tref_box(sess, opts, fileCtx, pos + total, plan->trefPlan,
					 NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_edts_box(sess, opts, fileCtx, pos + total, plan->edtsPlan,
					 NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_mdia_box(sess, opts, fileCtx, pos + total, &plan->mdia,
					 NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_usmt_box(sess, opts, fileCtx, pos + total, plan->uuidPlan,
					 NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_sum_edit_duration(Ff4SessionImpl *sess, const Ff4EditListModel *edts, uint64_t *outDur)
{
	const Ff4EditEntryNode *cur;
	uint64_t hi = 0;
	uint64_t lo = 0;
	int32_t overflow = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;

	if (edts != NULL) {
		for (cur = edts->head; cur != NULL; cur = cur->next) {
			const Ff4EditEntryInput *in = (const Ff4EditEntryInput *)cur->f;
			ret = ff4_u128_add(&overflow, &hi, &lo, 0, in->segmentDur);
			if (ret != FF4_OK)
				return ret;
			if (overflow != 0 || hi != 0)
				return FF4_ERR_OVERFLOW;
		}
	}
	*outDur = lo;
	return FF4_OK;
}

int32_t ff4_calc_track_duration(Ff4SessionImpl *sess, uint32_t movieTimescale,
				const Ff4TrackModel *track, uint64_t *outDur)
{
	uint64_t hi = 0;
	uint64_t lo = 0;
	uint64_t rem = 0;
	uint64_t mediaDur = 0;
	int32_t ret;

	if (sess == NULL || movieTimescale == 0 || track == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;

	if (track->edts != NULL) {
		ret = ff4_sum_edit_duration(sess, track->edts, &lo);
		if (ret != FF4_OK)
			return ret;
	} else {
		if ((track->mdhdMask & 8u) != 0) {
			mediaDur = track->mdhdDuration;
		} else {
			ret = ff4_calc_media_duration(sess, track, &mediaDur);
			if (ret != FF4_OK)
				return ret;
		}
		ret = ff4_u64_mul128(mediaDur, movieTimescale, &hi, &lo);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_u128_div64(hi, lo, track->mdhdTimescale, &hi, &lo, &rem);
		if (ret != FF4_OK)
			return ret;
		if (hi != 0)
			return FF4_ERR_OVERFLOW;
	}

	*outDur = lo;
	return FF4_OK;
}

int32_t ff4_plan_tkhd(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t movieTimescale,
		      const Ff4TrackModel *track, void *outTkhdPlan)
{
	Ff4TkhdPlanLocal *plan = (Ff4TkhdPlanLocal *)outTkhdPlan;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || movieTimescale == 0 ||
	    track == NULL || outTkhdPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->mask, 96u, track, 96u);
	if ((plan->mask & 0x10u) == 0)
		ret = ff4_calc_track_duration(sess, movieTimescale, track, &plan->duration);
	if (ret == FF4_OK)
		ret = ff4_choose_box_version(opts, plan->creationTime, plan->modTime,
					     plan->duration, &plan->version);
	return ret;
}

int32_t ff4_choose_elst_version(const Ff4FormatOpts *opts, const Ff4EditListModel *edts,
				uint8_t *outVersion)
{
	const Ff4EditEntryNode *cur;
	int32_t durHi = 0;
	int32_t timeHi = 0;
	uint8_t ver;

	if (opts == NULL || outVersion == NULL)
		return FF4_ERR_INVALID_ARG;

	ver = (opts->boxVerMode == 3) ? 1u : 0u;
	if (edts != NULL) {
		for (cur = edts->head; cur != NULL && ver == 0; cur = cur->next) {
			const Ff4EditEntryInput *in = (const Ff4EditEntryInput *)cur->f;
			if ((in->segmentDur >> 32) != 0)
				durHi = 1;
			if ((int32_t)(in->mediaTime >> 32) > 0)
				timeHi = 1;

			if (opts->boxVerMode == 3) {
				ver = 1;
			} else if (opts->boxVerMode > 3) {
				return FF4_ERR_INTERNAL;
			} else if (opts->boxVerMode == 1) {
				if (durHi || timeHi)
					ver = 1;
			} else {
				if (durHi && timeHi)
					return FF4_ERR_INVALID_STATE;
			}
		}
	}

	*outVersion = ver;
	return FF4_OK;
}

int32_t ff4_plan_elst(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4EditListModel *edts, void *outElstPlan)
{
	Ff4ElstPlan *plan = (Ff4ElstPlan *)outElstPlan;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || edts == NULL || outElstPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan->edts = edts;
	ret = ff4_choose_elst_version(opts, edts, &plan->version);
	if (ret == FF4_OK) {
		ret = ff4_write_elst_payload(sess, NULL, 0, plan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_edts(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4EditListModel *edts, void *outEdtsPlan)
{
	Ff4EdtsPlan *plan = (Ff4EdtsPlan *)outEdtsPlan;
	uint64_t elstSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || edts == NULL || outEdtsPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_elst(sess, opts, edts, &plan->elst);
	if (ret == FF4_OK) {
		ret = ff4_write_elst_box(sess, opts, NULL, 0, &plan->elst, NULL, &elstSize);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, elstSize, &plan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_tref_type(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   Ff4NodeGroup *group, Ff4TrefTypePlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || group == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	outPlan->group = group;
	ret = ff4_write_tref_type_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_plan_tref(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4NodeGroup *groupHead, Ff4TrefPlan *outPlan)
{
	Ff4NodeGroup *grp = groupHead;
	Ff4TrefTypePlan *node = NULL;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || groupHead == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	while (grp != NULL) {
		node = (Ff4TrefTypePlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_tref_type(sess, opts, grp, node);
		if (ret != FF4_OK)
			break;
		if (outPlan->head != NULL)
			outPlan->tail->next = node;
		else
			outPlan->head = node;
		outPlan->tail = node;
		node = NULL;
		grp = grp->next;
	}

	if (ret == FF4_OK)
		ret = ff4_write_tref_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);

	ff4_free_tref_plan_list(sess, node);
	return ret;
}

int32_t ff4_plan_trak(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t movieTimescale,
		      Ff4TrackModel *track, Ff4TrakPlan *outPlan)
{
	Ff4TrefPlan *trefPlan = NULL;
	Ff4EdtsPlan *edtsPlan = NULL;
	Ff4UuidPlan *uuidPlan = NULL;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || movieTimescale == 0 ||
	    track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_mdia(sess, opts, track, &outPlan->mdia);
	if (ret == FF4_OK && track->trefHead != NULL) {
		trefPlan = (Ff4TrefPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*trefPlan));
		if (trefPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_tref(sess, opts, track->trefHead, trefPlan);
			if (ret == FF4_OK) {
				outPlan->trefPlan = trefPlan;
				trefPlan = NULL;
			}
		}
	}
	if (ret == FF4_OK && track->edts != NULL) {
		edtsPlan = (Ff4EdtsPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*edtsPlan));
		if (edtsPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_edts(sess, opts, track->edts, edtsPlan);
			if (ret == FF4_OK) {
				outPlan->edtsPlan = edtsPlan;
				edtsPlan = NULL;
			}
		}
	}
	if (ret == FF4_OK)
		ret = ff4_plan_tkhd(sess, opts, movieTimescale, track, &outPlan->tkhdVersion);

	if (ret == FF4_OK && (track->mtdtHead != NULL || track->mtdfHead != NULL)) {
		uuidPlan = (Ff4UuidPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*uuidPlan));
		if (uuidPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_usmt(sess, opts, &track->mtdtHead, uuidPlan);
			if (ret == FF4_OK) {
				outPlan->uuidPlan = uuidPlan;
				uuidPlan = NULL;
			}
		}
	}
	if (ret == FF4_OK)
		ret = ff4_write_trak_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);

	ff4_pool_free(sess, edtsPlan);
	ff4_free_tref_plan(sess, trefPlan);
	ff4_free_uuid_plan(sess, uuidPlan);
	return ret;
}

int32_t ff4_write_trak_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4TrakPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_TRAK,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_trak_payload(sess, opts, fileCtx, pos + total, plan,
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

typedef struct Ff4RawUuidPlan {
	uint64_t boxSize;
	uint32_t uuid[4];
} Ff4RawUuidPlan;

int32_t ff4_plan_raw_uuid(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, const uint32_t uuid[4],
			  uint64_t extraSize, void *outPlan)
{
	Ff4RawUuidPlan tmp;
	uint64_t uuidSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || uuid == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	ff4_memcpy(sess, tmp.uuid, sizeof(tmp.uuid), uuid, sizeof(tmp.uuid));
	ret = ff4_write_uuid_bytes(sess, NULL, 0, tmp.uuid, &uuidSize, NULL);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, extraSize + uuidSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outPlan, sizeof(tmp), &tmp, sizeof(tmp));
	}
	return ret;
}
