/*
 * Sony FF4 MP4 muxer: Sample Table Box (stbl) — stsd, stts, ctts, stsc, stsz,
 * stco/co64, stss, OD sample descriptors, tfra tracking, and fragment cache I/O.
 */
#include "ff4_internal.h"

#define FF4_BOX_STBL 0x7374626Cu
#define FF4_BOX_STSD 0x73747364u
#define FF4_BOX_STTS 0x73747473u
#define FF4_BOX_CTTS 0x63747473u
#define FF4_BOX_STSC 0x73747363u
#define FF4_BOX_STSZ 0x7374737Au
#define FF4_BOX_STCO 0x7374636Fu
#define FF4_BOX_CO64 0x636F3634u
#define FF4_BOX_STSS 0x73747373u
#define FF4_CACHE_MAGIC 0x46463452u /* "FF4R" */

typedef struct Ff4OdItemPlan {
	uint64_t boxSize;
	uint32_t value;
	struct Ff4OdItemPlan *next;
} Ff4OdItemPlan;

typedef struct Ff4OdEntryPlan {
	uint64_t boxSize;
	uint16_t odId;
	uint16_t pad;
	Ff4OdItemPlan *incHead;
	Ff4OdItemPlan *incTail;
	Ff4OdItemPlan *refHead;
	Ff4OdItemPlan *refTail;
	struct Ff4OdEntryPlan *next;
} Ff4OdEntryPlan;

typedef struct Ff4OdUpdatePlan {
	uint64_t boxSize;
	Ff4OdEntryPlan *head;
	Ff4OdEntryPlan *tail;
} Ff4OdUpdatePlan;

typedef struct Ff4OdSamplePlan {
	uint64_t boxSize;
	Ff4OdUpdatePlan update;
	Ff4MetaBoxPlan meta;
} Ff4OdSamplePlan;

typedef struct Ff4SttsState {
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
	uint32_t *lastEntry;
} Ff4SttsState;

typedef struct Ff4CttsState {
	uint32_t version;
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
	uint32_t *lastEntry;
} Ff4CttsState;

typedef struct Ff4StscState {
	uint32_t firstChunk;
	uint32_t pendingSamples;
	uint32_t curDesc;
	uint32_t committedChunks;
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
	uint32_t *lastEntry;
} Ff4StscState;

typedef struct Ff4StszState {
	uint32_t fixedSize;
	uint32_t totalSamples;
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
} Ff4StszState;

typedef struct Ff4StcoState {
	uint32_t firstSample;
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
} Ff4StcoState;

typedef struct Ff4StssState {
	uint32_t syncCount;
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
} Ff4StssState;

typedef struct Ff4TfraState {
	uint32_t blockCap;
	Ff4StblRunBlock *head;
	Ff4StblRunBlock *tail;
} Ff4TfraState;

typedef struct Ff4TrackFragTable {
	uint32_t trexMask;
	uint32_t trexDescIdx;
	uint32_t trexSampleDur;
	uint32_t trexSampleSize;
	uint32_t trexSampleFlags;
	uint32_t tfraBlockCap;
	Ff4StblRunBlock *tfraHead;
	Ff4StblRunBlock *tfraTail;
	Ff4SampleTableModel *fragHead;
	Ff4SampleTableModel *fragTail;
} Ff4TrackFragTable;

typedef struct Ff4SampleInput {
	uint32_t size;
	uint32_t pad;
	uint64_t offset;
	uint32_t duration;
	uint32_t isSync;
	int32_t compOffset;
	uint32_t descIndex;
} Ff4SampleInput;

typedef struct Ff4StsdListPlan {
	uint64_t boxSize;
	Ff4StsdPlan *head;
	Ff4StsdPlan *tail;
} Ff4StsdListPlan;

static void ff4_free_run_blocks(Ff4SessionImpl *sess, Ff4StblRunBlock *head)
{
	Ff4StblRunBlock *cur = head;

	while (cur != NULL) {
		Ff4StblRunBlock *next = cur->next;
		if (cur->entries != NULL)
			ff4_pool_free(sess, cur->entries);
		ff4_pool_free(sess, cur);
		cur = next;
	}
}

/* ---- OD sample descriptor serialization & planning ---- */

int32_t ff4_write_od_es_id_ref_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdItemPlan *plan = (const Ff4OdItemPlan *)planPtr;
	uint8_t val = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, &val, 0, sizeof(val));
		val = (uint8_t)plan->value;
		ret = ff4_box_write(sess, fileCtx, &val, 1u, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_od_es_id_ref_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				     const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdItemPlan *plan = (const Ff4OdItemPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 10u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_od_es_id_ref_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_od_es_id_inc_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdItemPlan *plan = (const Ff4OdItemPlan *)planPtr;
	uint8_t buf[2];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be16(buf, (uint16_t)plan->value);
		ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_od_es_id_inc_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				     const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdItemPlan *plan = (const Ff4OdItemPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 15u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_od_es_id_inc_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_od_entry_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				   const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdEntryPlan *plan = (const Ff4OdEntryPlan *)planPtr;
	const Ff4OdItemPlan *cur;
	uint8_t hdr[2];
	uint16_t packed;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	packed = (uint16_t)(((plan->odId & 0x3FFu) << 6) | 0x1Fu);
	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be16(hdr, packed);
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->incHead; cur != NULL; cur = cur->next) {
		ret = ff4_write_od_es_id_inc_descr(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}
	for (cur = plan->refHead; cur != NULL; cur = cur->next) {
		ret = ff4_write_od_es_id_ref_descr(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_od_entry_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				 const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdEntryPlan *plan = (const Ff4OdEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 17u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_od_entry_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_od_update_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdUpdatePlan *plan = (const Ff4OdUpdatePlan *)planPtr;
	const Ff4OdEntryPlan *cur;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_write_od_entry_descr(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}
	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_od_update_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdUpdatePlan *plan = (const Ff4OdUpdatePlan *)planPtr;
	uint64_t total = 0;
	uint64_t bodyTotal = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 1u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_od_update_payload(sess, fileCtx, pos + total, plan,
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

int32_t ff4_write_od_sample_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4OdSamplePlan *plan = (const Ff4OdSamplePlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_od_update_descr(sess, fileCtx, pos, &plan->update, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_meta_list_descr(sess, fileCtx, pos + total, &plan->meta,
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

int32_t ff4_plan_meta_item(Ff4SessionImpl *sess, Ff4MetaItemNode *item, Ff4MetaItemPlan *outPlan)
{
	if (sess == NULL || item == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	outPlan->item = item;
	return ff4_write_meta_item_payload(sess, NULL, 0, outPlan, &outPlan->boxSize, NULL);
}

int32_t ff4_plan_meta_list(Ff4SessionImpl *sess, Ff4MetaDataList *meta, Ff4MetaBoxPlan *outPlan)
{
	Ff4MetaItemNode *cur;
	Ff4MetaItemPlan *node = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || meta == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = meta->dataHead; cur != NULL; cur = cur->next) {
		node = (Ff4MetaItemPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_meta_item(sess, cur, node);
		if (ret != FF4_OK)
			break;
		if (outPlan->head != NULL)
			outPlan->tail->next = node;
		else
			outPlan->head = node;
		outPlan->tail = node;
		node = NULL;
	}

	if (ret == FF4_OK)
		ret = ff4_write_meta_list_payload(sess, NULL, 0, outPlan, &outPlan->boxSize, NULL);

	ff4_free_run_block_list_a(sess, node);
	return ret;
}

int32_t ff4_plan_od_es_id_ref(Ff4SessionImpl *sess, const uint8_t *valPtr, void *outPlanPtr)
{
	Ff4OdItemPlan *plan = (Ff4OdItemPlan *)outPlanPtr;

	if (sess == NULL || valPtr == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	plan->value = *valPtr;
	return ff4_write_od_es_id_ref_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
}

int32_t ff4_plan_od_es_id_inc(Ff4SessionImpl *sess, const uint32_t *valPtr, void *outPlanPtr)
{
	Ff4OdItemPlan *plan = (Ff4OdItemPlan *)outPlanPtr;

	if (sess == NULL || valPtr == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	plan->value = *valPtr;
	return ff4_write_od_es_id_inc_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
}

int32_t ff4_plan_od_entry(Ff4SessionImpl *sess, Ff4TrefEntry *tref, void *outPlanPtr)
{
	Ff4OdEntryPlan *plan = (Ff4OdEntryPlan *)outPlanPtr;
	Ff4NodeU32 *cur;
	Ff4OdItemPlan *incNode = NULL;
	Ff4OdItemPlan *refNode = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || tref == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan->odId = (uint16_t)tref->type;
	for (cur = tref->trackIds; cur != NULL; cur = cur->next) {
		incNode = (Ff4OdItemPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*incNode));
		if (incNode == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_od_es_id_inc(sess, &cur->value, incNode);
		if (ret != FF4_OK)
			break;
		if (plan->incHead != NULL)
			plan->incTail->next = incNode;
		else
			plan->incHead = incNode;
		plan->incTail = incNode;
		incNode = NULL;
	}

	if (ret == FF4_OK) {
		for (cur = tref->extraIds; cur != NULL; cur = cur->next) {
			uint8_t byteVal = (uint8_t)cur->value;
			refNode = (Ff4OdItemPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								  sizeof(*refNode));
			if (refNode == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_od_es_id_ref(sess, &byteVal, refNode);
			if (ret != FF4_OK)
				break;
			if (plan->refHead != NULL)
				plan->refTail->next = refNode;
			else
				plan->refHead = refNode;
			plan->refTail = refNode;
			refNode = NULL;
		}
	}

	if (ret == FF4_OK)
		ret = ff4_write_od_entry_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);

	ff4_free_run_block_list_c(sess, incNode);
	ff4_free_run_block_list_b(sess, refNode);
	return ret;
}

int32_t ff4_plan_od_update(Ff4SessionImpl *sess, Ff4MetaDataList *meta, void *outPlanPtr)
{
	Ff4OdUpdatePlan *plan = (Ff4OdUpdatePlan *)outPlanPtr;
	Ff4TrefEntry *cur;
	Ff4OdEntryPlan *node = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || meta == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = meta->trefHead; cur != NULL; cur = cur->next) {
		node = (Ff4OdEntryPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_od_entry(sess, cur, node);
		if (ret != FF4_OK)
			break;
		if (plan->head != NULL)
			plan->tail->next = node;
		else
			plan->head = node;
		plan->tail = node;
		node = NULL;
	}

	if (ret == FF4_OK)
		ret = ff4_write_od_update_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);

	ff4_free_run_group_list(sess, node);
	return ret;
}

int32_t ff4_plan_od_sample(Ff4SessionImpl *sess, Ff4MetaDataList *meta, void *outPlanPtr)
{
	Ff4OdSamplePlan *plan = (Ff4OdSamplePlan *)outPlanPtr;
	int32_t ret;

	if (sess == NULL || meta == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_od_update(sess, meta, &plan->update);
	if (ret == FF4_OK) {
		ret = ff4_plan_meta_list(sess, meta, &plan->meta);
		if (ret == FF4_OK)
			ret = ff4_write_od_sample_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
	}
	return ret;
}

/* ---- Run-block counters and allocators ---- */

int32_t ff4_count_run_block_entries(const Ff4StblRunBlock *head, uint32_t *outCount)
{
	const Ff4StblRunBlock *cur = head;
	uint32_t total = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	for (; cur != NULL; cur = cur->next)
		total += cur->count;
	*outCount = total;
	return FF4_OK;
}

int32_t ff4_count_stco_entries(const Ff4StblRunBlock *head, uint32_t *outCount)
{
	const Ff4StblRunBlock *cur = head;
	uint32_t total = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	for (; cur != NULL; cur = cur->next)
		total += cur->count;
	*outCount = total;
	return FF4_OK;
}

int32_t ff4_count_stss_entries(const Ff4StblRunBlock *head, uint32_t *outCount)
{
	const Ff4StblRunBlock *cur = head;
	uint32_t total = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	for (; cur != NULL; cur = cur->next)
		total += cur->count;
	*outCount = total;
	return FF4_OK;
}

int32_t ff4_alloc_run_block(Ff4SessionImpl *sess, uint32_t cap, uint32_t elemSize,
			    Ff4StblRunBlock **headPtr, Ff4StblRunBlock **tailPtr)
{
	Ff4StblRunBlock *blk = NULL;
	void *buf;
	int32_t ret = FF4_OK;

	if (sess == NULL || cap == 0 || elemSize == 0 || headPtr == NULL || tailPtr == NULL)
		return FF4_ERR_INVALID_ARG;

	blk = (Ff4StblRunBlock *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*blk));
	if (blk == NULL) {
		ret = FF4_ERR_NO_MEM;
	} else {
		buf = ff4_pool_alloc(sess, FF4_POOL_META, (uint64_t)(cap * elemSize));
		blk->entries = buf;
		if (buf == NULL) {
			ret = FF4_ERR_NO_MEM;
			ff4_pool_free(sess, blk->entries);
		} else {
			if (*headPtr == NULL)
				*headPtr = blk;
			if (*tailPtr != NULL)
				(*tailPtr)->next = blk;
			*tailPtr = blk;
			blk = NULL;
		}
	}
	ff4_pool_free(sess, blk);
	return ret;
}

int32_t ff4_stts_append(Ff4SessionImpl *sess, uint32_t delta, void *sttsStatePtr)
{
	Ff4SttsState *st = (Ff4SttsState *)sttsStatePtr;
	Ff4StblRunBlock *tail;
	uint32_t *entry;
	int32_t ret = FF4_OK;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (st->lastEntry != NULL && st->lastEntry[1] == delta) {
		st->lastEntry[0]++;
		return FF4_OK;
	}

	tail = st->tail;
	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, 8u, &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}

	entry = (uint32_t *)tail->entries + 2u * tail->count;
	entry[0] = 1u;
	entry[1] = delta;
	st->lastEntry = entry;
	tail->count++;
	return FF4_OK;
}

int32_t ff4_ctts_append(Ff4SessionImpl *sess, uint32_t offset, void *cttsStatePtr)
{
	Ff4CttsState *st = (Ff4CttsState *)cttsStatePtr;
	Ff4StblRunBlock *tail;
	uint32_t *entry;
	int32_t ret = FF4_OK;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (st->lastEntry != NULL && st->lastEntry[1] == offset) {
		st->lastEntry[0]++;
		return FF4_OK;
	}

	tail = st->tail;
	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, 8u, &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}

	entry = (uint32_t *)tail->entries + 2u * tail->count;
	entry[0] = 1u;
	entry[1] = offset;
	st->lastEntry = entry;
	tail->count++;
	return FF4_OK;
}

int32_t ff4_stsc_add_sample(uint32_t descIdx, void *stscStatePtr)
{
	Ff4StscState *st = (Ff4StscState *)stscStatePtr;

	if (st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (st->pendingSamples == 0) {
		st->firstChunk = st->committedChunks + 1u;
		st->curDesc = descIdx;
	}
	st->pendingSamples++;
	return FF4_OK;
}

int32_t ff4_stsc_commit_chunk(Ff4SessionImpl *sess, void *stscStatePtr)
{
	Ff4StscState *st = (Ff4StscState *)stscStatePtr;
	Ff4StblRunBlock *tail;
	uint32_t *entry;
	int32_t ret;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (st->lastEntry == NULL || st->lastEntry[1] != st->pendingSamples ||
	    st->lastEntry[2] != st->curDesc) {
		tail = st->tail;
		if (tail == NULL || tail->count >= st->blockCap) {
			ret = ff4_alloc_run_block(sess, st->blockCap, 12u, &st->head, &st->tail);
			if (ret != FF4_OK)
				return ret;
			tail = st->tail;
		}
		entry = (uint32_t *)tail->entries + 3u * tail->count;
		entry[0] = st->firstChunk;
		entry[1] = st->pendingSamples;
		entry[2] = st->curDesc;
		st->lastEntry = entry;
		tail->count++;
	}

	st->pendingSamples = 0;
	st->committedChunks++;
	return FF4_OK;
}

int32_t ff4_stsz_push_entry(Ff4SessionImpl *sess, uint32_t size, void *stszStatePtr)
{
	Ff4StszState *st = (Ff4StszState *)stszStatePtr;
	Ff4StblRunBlock *tail = st->tail;
	int32_t ret = FF4_OK;

	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, 4u, &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}
	((uint32_t *)tail->entries)[tail->count++] = size;
	return FF4_OK;
}

int32_t ff4_stsz_append(Ff4SessionImpl *sess, uint32_t size, void *stszStatePtr)
{
	Ff4StszState *st = (Ff4StszState *)stszStatePtr;
	uint32_t i;
	int32_t ret;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (st->totalSamples == 0) {
		st->fixedSize = size;
		st->totalSamples = 1u;
		return FF4_OK;
	}

	if (st->fixedSize != 0 && st->fixedSize != size) {
		for (i = 0; i < st->totalSamples; i++) {
			ret = ff4_stsz_push_entry(sess, st->fixedSize, st);
			if (ret != FF4_OK)
				return ret;
		}
		st->fixedSize = 0;
	}

	if (st->fixedSize == 0) {
		ret = ff4_stsz_push_entry(sess, size, st);
		if (ret != FF4_OK)
			return ret;
	}
	st->totalSamples++;
	return FF4_OK;
}

int32_t ff4_stco_append(Ff4SessionImpl *sess, uint64_t offset, void *stcoStatePtr)
{
	Ff4StcoState *st = (Ff4StcoState *)stcoStatePtr;
	Ff4StblRunBlock *tail;
	int32_t ret;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	tail = st->tail;
	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, 8u, &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}
	((uint64_t *)tail->entries)[tail->count++] = offset;
	return FF4_OK;
}

int32_t ff4_stss_push_entry(Ff4SessionImpl *sess, uint32_t sampleNum1, void *stssStatePtr)
{
	Ff4StssState *st = (Ff4StssState *)stssStatePtr;
	Ff4StblRunBlock *tail = st->tail;
	int32_t ret = FF4_OK;

	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, 4u, &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}
	((uint32_t *)tail->entries)[tail->count++] = sampleNum1;
	return FF4_OK;
}

int32_t ff4_stss_append(Ff4SessionImpl *sess, uint32_t isSync, uint32_t sampleNum1,
			void *stssStatePtr)
{
	Ff4StssState *st = (Ff4StssState *)stssStatePtr;
	uint32_t i;
	int32_t ret = FF4_OK;

	if (sess == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	if (isSync != 0) {
		if (st->tail != NULL || (st->syncCount == 0 && sampleNum1 != 1u)) {
			ret = ff4_stss_push_entry(sess, sampleNum1, st);
			if (ret != FF4_OK)
				return ret;
		}
		st->syncCount++;
	} else if (st->tail == NULL && st->syncCount != 0) {
		for (i = 1u; i < sampleNum1; i++) {
			ret = ff4_stss_push_entry(sess, i, st);
			if (ret != FF4_OK)
				return ret;
		}
	}
	return ret;
}

int32_t ff4_tfra_append(Ff4SessionImpl *sess, const Ff4TfraEntry *entry, void *tfraStatePtr)
{
	Ff4TfraState *st = (Ff4TfraState *)tfraStatePtr;
	Ff4StblRunBlock *tail;
	Ff4TfraEntry *dst;
	int32_t ret;

	if (sess == NULL || entry == NULL || st == NULL)
		return FF4_ERR_INVALID_ARG;

	tail = st->tail;
	if (tail == NULL || tail->count >= st->blockCap) {
		ret = ff4_alloc_run_block(sess, st->blockCap, sizeof(Ff4TfraEntry),
					  &st->head, &st->tail);
		if (ret != FF4_OK)
			return ret;
		tail = st->tail;
	}
	dst = (Ff4TfraEntry *)tail->entries + tail->count;
	ff4_memcpy(sess, dst, sizeof(*dst), entry, sizeof(*entry));
	tail->count++;
	return FF4_OK;
}

/* ---- Plan initializers & predicates ---- */

int32_t ff4_init_stts_plan(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag,
			   Ff4SttsPlan *outPlan)
{
	if (sess == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	outPlan->head = (frag != NULL) ? frag->sttsHead : NULL;
	return FF4_OK;
}

int32_t ff4_init_ctts_plan(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag,
			   Ff4CttsPlan *outPlan)
{
	if (sess == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	outPlan->version = (frag != NULL) ? (uint8_t)frag->cttsVersion : 0u;
	outPlan->head = (frag != NULL) ? frag->cttsHead : NULL;
	return FF4_OK;
}

int32_t ff4_init_stsc_plan(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag,
			   Ff4SttsPlan *outPlan)
{
	if (sess == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	outPlan->head = (frag != NULL) ? frag->stscHead : NULL;
	return FF4_OK;
}

int32_t ff4_init_stsz_plan(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag,
			   Ff4StszPlan *outPlan)
{
	uint32_t fixedSize = 0;
	uint32_t sampleCount = 0;
	Ff4StblRunBlock *head = NULL;

	if (sess == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	if (frag != NULL) {
		const Ff4StblRunBlock *cur = frag->stszHead;
		uint32_t varCount = 0;
		fixedSize = frag->fixedSampleSize;
		for (; cur != NULL; cur = cur->next)
			varCount += cur->count;
		if (varCount != 0) {
			sampleCount = varCount;
			head = frag->stszHead;
		} else {
			sampleCount = frag->totalSamples;
		}
	}

	outPlan->fixedSize = fixedSize;
	outPlan->sampleCount = sampleCount;
	outPlan->head = head;
	return FF4_OK;
}

int32_t ff4_init_stco_plan(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   const Ff4SampleTableModel *frag, Ff4StcoPlan *outPlan)
{
	uint32_t elemSize = 4u;
	Ff4StblRunBlock *head = NULL;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	if (opts->chunkOffMode == 2u)
		elemSize = 8u;

	if (frag != NULL) {
		Ff4StblRunBlock *blk = frag->stcoHead;
		head = blk;
		for (; blk != NULL && elemSize != 8u; blk = blk->next) {
			const uint64_t *offs = (const uint64_t *)blk->entries;
			uint32_t i;
			for (i = 0; i < blk->count; i++) {
				if ((offs[i] >> 32) != 0) {
					elemSize = 8u;
					break;
				}
			}
		}
		if (opts->chunkOffMode == 0 && elemSize == 8u)
			return FF4_ERR_INVALID_STATE;
	}

	outPlan->fourcc = (elemSize == 4u) ? FF4_BOX_STCO : FF4_BOX_CO64;
	outPlan->head = head;
	return FF4_OK;
}

int32_t ff4_init_stss_plan(Ff4SessionImpl *sess, const Ff4SampleTableModel *frag,
			   Ff4SttsPlan *outPlan)
{
	if (sess == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;
	outPlan->head = (frag != NULL) ? frag->stssHead : NULL;
	return FF4_OK;
}

int32_t ff4_needs_ctts_box(const void *trackFragPtr, int32_t *outNeeds)
{
	const Ff4TrackFragTable *ft = (const Ff4TrackFragTable *)trackFragPtr;
	int32_t needed = 0;

	if (ft == NULL || outNeeds == NULL)
		return FF4_ERR_INVALID_ARG;

	if (ft->fragHead != NULL) {
		const Ff4StblRunBlock *head = ft->fragHead->cttsHead;
		const Ff4StblRunBlock *cur = head;
		uint32_t total = 0;

		for (; cur != NULL; cur = cur->next)
			total += cur->count;

		if (total > 1u) {
			needed = 1;
		} else if (head != NULL && head->entries != NULL &&
			   ((const uint32_t *)head->entries)[1] != 0) {
			needed = 1;
		}
	}
	*outNeeds = needed;
	return FF4_OK;
}

int32_t ff4_needs_stss_box(const void *trackFragPtr, int32_t *outNeeds)
{
	const Ff4TrackFragTable *ft = (const Ff4TrackFragTable *)trackFragPtr;
	int32_t needed = 1;

	if (ft == NULL || outNeeds == NULL)
		return FF4_ERR_INVALID_ARG;

	if (ft->fragHead != NULL) {
		if (ft->fragHead->stssHead == NULL)
			needed = (ft->fragHead->hasSyncSample == 0) ? 1 : 0;
	} else {
		needed = 0;
	}
	*outNeeds = needed;
	return FF4_OK;
}

/* ---- Fragment lifecycle & sample addition ---- */

int32_t ff4_clear_fragment_node(Ff4SessionImpl *sess, Ff4SampleTableModel *frag)
{
	if (sess == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_free_run_blocks(sess, frag->sttsHead);
	frag->sttsHead = NULL;
	frag->sttsTail = NULL;
	frag->sttsLastEntry = NULL;

	ff4_free_run_blocks(sess, frag->cttsHead);
	frag->cttsHead = NULL;
	frag->cttsTail = NULL;
	frag->cttsLastEntry = NULL;

	ff4_free_run_blocks(sess, frag->stscHead);
	frag->stscHead = NULL;
	frag->stscTail = NULL;
	frag->stscLastEntry = NULL;

	ff4_free_run_blocks(sess, frag->stszHead);
	frag->stszHead = NULL;
	frag->stszTail = NULL;

	ff4_free_run_blocks(sess, frag->stcoHead);
	frag->stcoHead = NULL;
	frag->stcoTail = NULL;

	ff4_free_run_blocks(sess, frag->stssHead);
	frag->stssHead = NULL;
	frag->stssTail = NULL;
	return FF4_OK;
}

int32_t ff4_free_track_sample_tables(Ff4SessionImpl *sess, void *trexAndFrags)
{
	Ff4TrackFragTable *ft = (Ff4TrackFragTable *)trexAndFrags;
	Ff4SampleTableModel *cur;

	if (sess == NULL || ft == NULL)
		return FF4_ERR_INVALID_ARG;

	cur = ft->fragHead;
	while (cur != NULL) {
		Ff4SampleTableModel *next = cur->next;
		ff4_clear_fragment_node(sess, cur);
		ff4_pool_free(sess, cur);
		cur = next;
	}
	ff4_free_run_blocks(sess, ft->tfraHead);
	return FF4_OK;
}

int32_t ff4_append_fragment_node(Ff4SessionImpl *sess, void *trexAndFrags)
{
	Ff4TrackFragTable *ft = (Ff4TrackFragTable *)trexAndFrags;
	Ff4SampleTableModel *node = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || ft == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4SampleTableModel *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->minPresTime = 0x7FFFFFFFFFFFFFFFULL;
	node->sttsBlockCap = 1000u;
	node->cttsBlockCap = 1000u;
	node->stscBlockCap = 1000u;
	node->stszBlockCap = 1000u;
	node->stcoBlockCap = 1000u;
	node->stssBlockCap = 1000u;

	if (ft->fragHead == NULL) {
		ft->fragHead = node;
		ft->tfraBlockCap = 1000u;
	}
	if (ft->fragTail != NULL) {
		if (~ft->fragTail->baseDecodeTime < ft->fragTail->totalDuration) {
			ret = FF4_ERR_OVERFLOW;
		} else {
			ft->fragTail->next = node;
			node->baseDecodeTime = ft->fragTail->baseDecodeTime +
					       ft->fragTail->totalDuration;
		}
	}
	if (ret == FF4_OK) {
		ft->fragTail = node;
		node = NULL;
	}
	ff4_pool_free(sess, node);
	return ret;
}

int32_t ff4_get_fragment_at(Ff4SampleTableModel *head, uint32_t index0, Ff4SampleTableModel **outFrag)
{
	Ff4SampleTableModel *cur = head;
	uint32_t i;

	if (outFrag == NULL)
		return FF4_ERR_INVALID_ARG;
	for (i = 0; i < index0 && cur != NULL; i++)
		cur = cur->next;
	*outFrag = cur;
	return FF4_OK;
}

int32_t ff4_add_signed_offset(uint64_t base, int64_t delta, uint64_t *outVal)
{
	if (outVal == NULL)
		return FF4_ERR_INVALID_ARG;

	if (delta >= 0) {
		if (~base < (uint64_t)delta)
			return FF4_ERR_OVERFLOW;
		*outVal = base + (uint64_t)delta;
	} else {
		uint64_t mag = (uint64_t)(-delta);
		if (base < mag)
			return FF4_ERR_OVERFLOW;
		*outVal = base - mag;
	}
	return FF4_OK;
}

int32_t ff4_shift_chunk_offsets(Ff4SessionImpl *sess, int32_t useFragMode, uint32_t trackIdx,
				uint32_t fragIdx, uint32_t chunkIdx1, int64_t delta,
				Ff4TrackModel *trackListHead)
{
	Ff4TrackModel *track = trackListHead;
	int32_t ret = FF4_OK;

	if (sess == NULL || trackListHead == NULL)
		return FF4_ERR_INVALID_ARG;

	if (trackIdx != 0) {
		ret = ff4_get_track_at(trackListHead, trackIdx, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_INVALID_STATE;
	}

	while (track != NULL) {
		Ff4SampleTableModel *frag = track->fragHead;

		if (useFragMode != 0) {
			uint32_t skip = (fragIdx != 0) ? fragIdx : 1u;
			uint32_t k;
			for (k = 0; k < skip && frag != NULL; k++)
				frag = frag->next;
			if (frag == NULL)
				return FF4_ERR_NOT_FOUND;
		}

		while (frag != NULL) {
			Ff4StblRunBlock *blk = frag->stcoHead;
			if (chunkIdx1 != 0) {
				uint32_t passed = 0;
				while (blk != NULL && chunkIdx1 > passed + blk->count) {
					passed += blk->count;
					blk = blk->next;
				}
				if (blk != NULL) {
					uint64_t *offs = (uint64_t *)blk->entries;
					uint32_t i;
					if (offs == NULL)
						return FF4_ERR_INTERNAL;
					for (i = 0; i < blk->count; i++) {
						if (passed + i + 1u == chunkIdx1) {
							uint64_t updated = 0;
							ret = ff4_add_signed_offset(offs[i], delta,
										    &updated);
							if (ret != FF4_OK)
								return ret;
							offs[i] = updated;
							break;
						}
					}
				} else if (trackIdx != 0 && (useFragMode == 0 || fragIdx != 0)) {
					return FF4_ERR_NOT_FOUND;
				}
			} else {
				while (blk != NULL) {
					uint64_t *offs = (uint64_t *)blk->entries;
					uint32_t i;
					if (offs == NULL)
						return FF4_ERR_INTERNAL;
					for (i = 0; i < blk->count; i++) {
						uint64_t updated = 0;
						ret = ff4_add_signed_offset(offs[i], delta, &updated);
						if (ret != FF4_OK)
							return ret;
						offs[i] = updated;
					}
					blk = blk->next;
				}
			}

			if (useFragMode == 0 || fragIdx != 0)
				break;
			frag = frag->next;
		}

		if (trackIdx != 0)
			break;
		track = track->next;
	}
	return FF4_OK;
}

int32_t ff4_get_track_duration_in_timescale(uint32_t movieTimescale, const Ff4TrackModel *track,
					    uint64_t *outDur)
{
	uint64_t dur = 0;
	int32_t ret;

	if (track == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;

	if (track->fragHead != NULL) {
		ret = ff4_rescale_u64(track->fragHead->totalDuration, track->mdhdTimescale,
				      movieTimescale, &dur);
		if (ret != FF4_OK)
			return ret;
	}
	*outDur = dur;
	return FF4_OK;
}

int32_t ff4_get_track_min_pres_in_timescale(uint32_t movieTimescale, const Ff4TrackModel *track,
					    uint64_t *outMinPres)
{
	uint64_t val = 0;
	int32_t ret;

	if (track == NULL || outMinPres == NULL)
		return FF4_ERR_INVALID_ARG;

	if (track->fragHead != NULL) {
		ret = ff4_rescale_u64(track->fragHead->minPresTime, track->mdhdTimescale,
				      movieTimescale, &val);
		if (ret != FF4_OK)
			return ret;
	}
	*outMinPres = val;
	return FF4_OK;
}

int32_t ff4_sum_stts_duration(const Ff4TrackModel *track, uint64_t *outDur)
{
	if (track == NULL || outDur == NULL)
		return FF4_ERR_INVALID_ARG;
	*outDur = (track->fragHead != NULL) ? track->fragHead->totalDuration : 0ULL;
	return FF4_OK;
}

int32_t ff4_add_sample(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t trackIdx,
		       const void *samplePtr, int32_t recordTfra)
{
	const Ff4SampleInput *in = (const Ff4SampleInput *)samplePtr;
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsdEntry = NULL;
	Ff4SampleTableModel *frag;
	uint64_t room;
	uint64_t compDelta;
	int32_t ret;

	if (sess == NULL || models == NULL || trackIdx == 0 || in == NULL)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(models->moov->trackHead, trackIdx, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_INVALID_STATE;

	if (track->fragTail == NULL) {
		ret = ff4_append_fragment_node(sess, &track->trexMask);
		if (ret != FF4_OK)
			return ret;
	}

	ret = ff4_get_stsd_at(track->stsdHead, in->descIndex, &stsdEntry);
	if (ret != FF4_OK)
		return ret;
	if (stsdEntry == NULL)
		return FF4_ERR_INVALID_STATE;

	frag = track->fragTail;
	room = ~frag->totalDuration;
	if (room < (uint64_t)in->duration)
		return FF4_ERR_OVERFLOW;
	if (frag->cttsVersion == 1u) {
		if (in->compOffset >= 0 && room < (uint64_t)(uint32_t)in->compOffset)
			return FF4_ERR_OVERFLOW;
	} else {
		if (room < (uint64_t)(uint32_t)in->compOffset)
			return FF4_ERR_OVERFLOW;
	}

	if (frag->stscChunkCount == 0) {
		ret = ff4_stco_append(sess, in->offset, &frag->stcoFirstSample);
		if (ret != FF4_OK)
			return ret;
		frag->stcoFirstSample = frag->totalSamples;
	}

	if (recordTfra != 0 && track->fragHead != frag) {
		uint32_t fragCount = 0;
		uint32_t chunkCount = 0;
		const Ff4StblRunBlock *blk;
		Ff4TfraEntry tfraEntry;

		ret = ff4_get_fragment_count(models->moov, &fragCount);
		if (ret != FF4_OK)
			return ret;
		if (fragCount == 0)
			return FF4_ERR_INTERNAL;

		for (blk = frag->stcoHead; blk != NULL; blk = blk->next)
			chunkCount += blk->count;

		tfraEntry.time = frag->baseDecodeTime + frag->totalDuration;
		tfraEntry.fragIndex = fragCount;
		tfraEntry.trackIndex = trackIdx;
		tfraEntry.trunNum = chunkCount;
		tfraEntry.sampleNum = frag->totalSamples - frag->stcoFirstSample + 1u;

		ret = ff4_tfra_append(sess, &tfraEntry, &track->tfraBlockCap);
		if (ret != FF4_OK)
			return ret;
	}

	ret = ff4_stts_append(sess, in->duration, &frag->sttsBlockCap);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_ctts_append(sess, (uint32_t)in->compOffset, &frag->cttsVersion);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_stsc_add_sample(in->descIndex, &frag->stscFirstChunk);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_stsz_append(sess, in->size, &frag->fixedSampleSize);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_stss_append(sess, in->isSync, frag->totalSamples, &frag->hasSyncSample);
	if (ret != FF4_OK)
		return ret;

	compDelta = (frag->cttsVersion == 1u)
			? (uint64_t)(int64_t)in->compOffset
			: (uint64_t)(uint32_t)in->compOffset;
	if (frag->minPresTime > frag->totalDuration + compDelta)
		frag->minPresTime = frag->totalDuration + compDelta;
	frag->totalDuration += (uint64_t)in->duration;
	return FF4_OK;
}

int32_t ff4_commit_chunk(Ff4SessionImpl *sess, Ff4WriterModels *models, uint32_t trackIdx)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (sess == NULL || models == NULL || trackIdx == 0)
		return FF4_ERR_INVALID_ARG;
	if (models->moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(models->moov->trackHead, trackIdx, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL || track->fragTail == NULL || track->fragTail->stscChunkCount == 0)
		return FF4_ERR_INVALID_STATE;

	return ff4_stsc_commit_chunk(sess, &track->fragTail->stscFirstChunk);
}

/* ---- Box payload writers & box wrappers ---- */

int32_t ff4_write_stss_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4SttsPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_box_seek(sess, fileCtx, pos, 0);
		if (ret == FF4_OK)
			ret = ff4_count_run_block_entries(plan->head, &count);
		if (ret == FF4_OK) {
			ff4_memset(sess, hdr, 0, sizeof(hdr));
			ff4_put_be32(hdr + 4, count);
			ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
		}
		if (ret == FF4_OK) {
			for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
				uint64_t bytes = 4ULL * (uint64_t)blk->count;
				const uint32_t *src = (const uint32_t *)blk->entries;
				uint8_t *p;
				uint32_t i;

				tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
				if (tmpBuf == NULL) {
					ret = FF4_ERR_NO_MEM;
					break;
				}
				p = tmpBuf;
				for (i = 0; i < blk->count; i++) {
					ff4_put_be32(p, src[i]);
					p += 4;
				}
				ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
				if (ret != FF4_OK)
					break;
				ff4_pool_free(sess, tmpBuf);
				tmpBuf = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_stss_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4SttsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STSS,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_stss_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_stco_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4StcoPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	uint32_t elemSize = 4u;
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK)
		ret = ff4_count_stco_entries(plan->head, &count);
	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 4, count);
		ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	}
	if (ret == FF4_OK) {
		if (plan->fourcc == FF4_BOX_CO64)
			elemSize = 8u;
		for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
			uint64_t bytes = (uint64_t)elemSize * (uint64_t)blk->count;
			const uint64_t *src = (const uint64_t *)blk->entries;
			uint8_t *p;
			uint32_t i;

			tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
			if (tmpBuf == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			p = tmpBuf;
			for (i = 0; i < blk->count; i++) {
				if (plan->fourcc == FF4_BOX_STCO) {
					ff4_put_be32(p, (uint32_t)src[i]);
					p += 4;
				} else {
					ff4_put_be64(p, src[i]);
					p += 8;
				}
			}
			ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
			if (ret != FF4_OK)
				break;
			ff4_pool_free(sess, tmpBuf);
			tmpBuf = NULL;
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_stco_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4StcoPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stco_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_stsz_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4StszPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[12];
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 4, plan->fixedSize);
		ff4_put_be32(hdr + 8, plan->sampleCount);
		ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	}
	if (ret == FF4_OK) {
		for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
			uint64_t bytes = 4ULL * (uint64_t)blk->count;
			const uint32_t *src = (const uint32_t *)blk->entries;
			uint8_t *p;
			uint32_t i;

			tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
			if (tmpBuf == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			p = tmpBuf;
			for (i = 0; i < blk->count; i++) {
				ff4_put_be32(p, src[i]);
				p += 4;
			}
			ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
			if (ret != FF4_OK)
				break;
			ff4_pool_free(sess, tmpBuf);
			tmpBuf = NULL;
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_stsz_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4StszPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STSZ,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stsz_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_stsc_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4SttsPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK)
		ret = ff4_count_run_block_entries(plan->head, &count);
	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 4, count);
		ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	}
	if (ret == FF4_OK) {
		for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
			uint64_t bytes = 12ULL * (uint64_t)blk->count;
			const uint32_t *src = (const uint32_t *)blk->entries;
			uint8_t *p;
			uint32_t i;

			tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
			if (tmpBuf == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			p = tmpBuf;
			for (i = 0; i < blk->count; i++) {
				ff4_put_be32(p + 0, src[0]);
				ff4_put_be32(p + 4, src[1]);
				ff4_put_be32(p + 8, src[2]);
				src += 3;
				p += 12;
			}
			ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
			if (ret != FF4_OK)
				break;
			ff4_pool_free(sess, tmpBuf);
			tmpBuf = NULL;
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_stsc_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4SttsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STSC,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stsc_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_ctts_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4CttsPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_box_seek(sess, fileCtx, pos, 0);
		if (ret == FF4_OK)
			ret = ff4_count_run_block_entries(plan->head, &count);
		if (ret == FF4_OK) {
			ff4_memset(sess, hdr, 0, sizeof(hdr));
			hdr[0] = plan->version;
			ff4_put_be32(hdr + 4, count);
			ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
		}
		if (ret == FF4_OK) {
			for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
				uint64_t bytes = 8ULL * (uint64_t)blk->count;
				const uint32_t *src = (const uint32_t *)blk->entries;
				uint8_t *p;
				uint32_t i;

				tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
				if (tmpBuf == NULL) {
					ret = FF4_ERR_NO_MEM;
					break;
				}
				p = tmpBuf;
				for (i = 0; i < blk->count; i++) {
					ff4_put_be32(p + 0, src[0]);
					ff4_put_be32(p + 4, src[1]);
					src += 2;
					p += 8;
				}
				ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
				if (ret != FF4_OK)
					break;
				ff4_pool_free(sess, tmpBuf);
				tmpBuf = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_ctts_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4CttsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_CTTS,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_ctts_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_stts_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4SttsPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[8];
	uint32_t count = 0;
	const Ff4StblRunBlock *blk;
	uint8_t *tmpBuf = NULL;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK)
		ret = ff4_count_run_block_entries(plan->head, &count);
	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 4, count);
		ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	}
	if (ret == FF4_OK) {
		for (blk = plan->head; blk != NULL && blk->count != 0; blk = blk->next) {
			uint64_t bytes = 8ULL * (uint64_t)blk->count;
			const uint32_t *src = (const uint32_t *)blk->entries;
			uint8_t *p;
			uint32_t i;

			tmpBuf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, bytes);
			if (tmpBuf == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			p = tmpBuf;
			for (i = 0; i < blk->count; i++) {
				ff4_put_be32(p + 0, src[0]);
				ff4_put_be32(p + 4, src[1]);
				src += 2;
				p += 8;
			}
			ret = ff4_box_write(sess, fileCtx, tmpBuf, bytes, NULL, &total);
			if (ret != FF4_OK)
				break;
			ff4_pool_free(sess, tmpBuf);
			tmpBuf = NULL;
		}
	}

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	ff4_pool_free(sess, tmpBuf);
	return ret;
}

int32_t ff4_write_stts_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4SttsPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STTS,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stts_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_stsd_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *stsdListPlan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4StsdListPlan *plan = (const Ff4StsdListPlan *)stsdListPlan;
	const Ff4StsdPlan *cur;
	uint8_t hdr[8];
	uint32_t count = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_count_stsd_plans(plan->head, &count);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 4, count);
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_write_stsd_entry_box(sess, opts, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_stsd_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *stsdListPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4StsdListPlan *plan = (const Ff4StsdListPlan *)stsdListPlan;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STSD,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stsd_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_stbl_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4StblPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_stsd_box(sess, opts, fileCtx, pos, &plan->stsdBoxSize, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stts_box(sess, opts, fileCtx, pos + total, &plan->stts, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_ctts_box(sess, opts, fileCtx, pos + total, plan->cttsPlan, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stsc_box(sess, opts, fileCtx, pos + total, &plan->stsc, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stsz_box(sess, opts, fileCtx, pos + total, &plan->stsz, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stco_box(sess, opts, fileCtx, pos + total, &plan->stco, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stss_box(sess, opts, fileCtx, pos + total, plan->stssPlan, NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

/* ---- Box planners ---- */

int32_t ff4_plan_stss(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4SttsPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_stss_plan(sess, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_stss_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_stco(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4StcoPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_stco_plan(sess, opts, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_stco_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_stsz(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4StszPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_stsz_plan(sess, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_stsz_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_stsc(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4SttsPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_stsc_plan(sess, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_stsc_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_ctts(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4CttsPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_ctts_plan(sess, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_ctts_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_stts(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4SampleTableModel *frag, Ff4SttsPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_init_stts_plan(sess, frag, outPlan);
	if (ret == FF4_OK) {
		ret = ff4_write_stts_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}
	return ret;
}

int32_t ff4_plan_stsd_list(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   uint32_t timescale, Ff4StsdEntry *const *stsdHeadTail,
			   void *outStsdPlan)
{
	Ff4StsdListPlan *plan = (Ff4StsdListPlan *)outStsdPlan;
	Ff4StsdEntry *cur;
	Ff4StsdPlan *node = NULL;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || stsdHeadTail == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = stsdHeadTail[0]; cur != NULL; cur = cur->next) {
		node = (Ff4StsdPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		node->kind = cur->kind;
		ret = ff4_plan_stsd_entry(sess, opts, timescale, cur, node);
		if (ret != FF4_OK)
			break;
		if (plan->head != NULL)
			plan->tail->next = node;
		else
			plan->head = node;
		plan->tail = node;
		node = NULL;
	}

	if (ret == FF4_OK) {
		ret = ff4_write_stsd_payload(sess, opts, NULL, 0, plan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_stsd_plan_list(sess, node);
	return ret;
}

int32_t ff4_plan_stbl(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4TrackModel *track,
		      Ff4StblPlan *outPlan)
{
	Ff4CttsPlan *ctts = NULL;
	Ff4SttsPlan *stss = NULL;
	int32_t needed = 0;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || track == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_stsd_list(sess, opts, track->mdhdTimescale, &track->stsdHead,
				 &outPlan->stsdBoxSize);
	if (ret == FF4_OK)
		ret = ff4_plan_stts(sess, opts, track->fragHead, &outPlan->stts);
	if (ret == FF4_OK) {
		ret = ff4_needs_ctts_box(&track->trexMask, &needed);
		if (ret == FF4_OK && needed != 0) {
			ctts = (Ff4CttsPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*ctts));
			if (ctts == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_ctts(sess, opts, track->fragHead, ctts);
				if (ret == FF4_OK) {
					outPlan->cttsPlan = ctts;
					ctts = NULL;
				}
			}
		}
	}
	if (ret == FF4_OK)
		ret = ff4_plan_stsc(sess, opts, track->fragHead, &outPlan->stsc);
	if (ret == FF4_OK)
		ret = ff4_plan_stsz(sess, opts, track->fragHead, &outPlan->stsz);
	if (ret == FF4_OK)
		ret = ff4_plan_stco(sess, opts, track->fragHead, &outPlan->stco);
	if (ret == FF4_OK) {
		ret = ff4_needs_stss_box(&track->trexMask, &needed);
		if (ret == FF4_OK && needed != 0) {
			stss = (Ff4SttsPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*stss));
			if (stss == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_stss(sess, opts, track->fragHead, stss);
				if (ret == FF4_OK) {
					outPlan->stssPlan = stss;
					stss = NULL;
				}
			}
		}
	}
	if (ret == FF4_OK) {
		ret = ff4_write_stbl_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}

	ff4_pool_free(sess, ctts);
	ff4_pool_free(sess, stss);
	return ret;
}

int32_t ff4_write_stbl_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4StblPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_STBL,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_stbl_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

/* ---- Fragment cache serialization & deserialization ---- */

int32_t ff4_write_cache_header(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       uint64_t totalSize, uint32_t fragIdx,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[16];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		buf[0] = 'F';
		buf[1] = 'F';
		buf[2] = '4';
		buf[3] = 'R';
		ff4_put_be64(buf + 4, totalSize);
		ff4_put_be32(buf + 12, fragIdx);
		ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_cache_fragment(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				 const Ff4SampleTableModel *frag,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	Ff4FormatOpts opts;
	Ff4SttsPlan stts, stsc, stss;
	Ff4CttsPlan ctts;
	Ff4StszPlan stsz;
	Ff4StcoPlan stco;
	uint64_t total = 0;
	int32_t ret;

	opts.boxSizeMode = 2u;
	opts.boxVerMode = 0u;
	opts.chunkOffMode = 1u;
	opts.mehdMode = 0u;
	opts.tfraMode = 0u;

	ff4_memset(sess, &stts, 0, sizeof(stts));
	ff4_memset(sess, &ctts, 0, sizeof(ctts));
	ff4_memset(sess, &stsc, 0, sizeof(stsc));
	ff4_memset(sess, &stsz, 0, sizeof(stsz));
	ff4_memset(sess, &stco, 0, sizeof(stco));
	ff4_memset(sess, &stss, 0, sizeof(stss));

	if (sess == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_stts(sess, &opts, frag, &stts);
	if (ret == FF4_OK)
		ret = ff4_plan_ctts(sess, &opts, frag, &ctts);
	if (ret == FF4_OK)
		ret = ff4_plan_stsc(sess, &opts, frag, &stsc);
	if (ret == FF4_OK)
		ret = ff4_plan_stsz(sess, &opts, frag, &stsz);
	if (ret == FF4_OK)
		ret = ff4_plan_stco(sess, &opts, frag, &stco);
	if (ret == FF4_OK)
		ret = ff4_plan_stss(sess, &opts, frag, &stss);

	if (ret == FF4_OK)
		ret = ff4_write_stts_box(sess, &opts, fileCtx, pos + total, &stts, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_ctts_box(sess, &opts, fileCtx, pos + total, &ctts, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stsc_box(sess, &opts, fileCtx, pos + total, &stsc, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stsz_box(sess, &opts, fileCtx, pos + total, &stsz, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stco_box(sess, &opts, fileCtx, pos + total, &stco, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_stss_box(sess, &opts, fileCtx, pos + total, &stss, NULL, &total);

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_read_cache_header(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			      uint64_t availSize, uint32_t fragIdx,
			      uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[16];
	uint64_t nread = 0;
	uint32_t magic;
	uint64_t storedSize;
	uint32_t storedFrag;
	int32_t ret;

	if (sess == NULL || stream == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_stream_seek(sess, stream, (int64_t)pos, 0u);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_stream_read(sess, stream, buf, sizeof(buf), &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != sizeof(buf))
		return FF4_ERR_IO;

	magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
		((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	storedSize = ((uint64_t)buf[4] << 56) | ((uint64_t)buf[5] << 48) |
		     ((uint64_t)buf[6] << 40) | ((uint64_t)buf[7] << 32) |
		     ((uint64_t)buf[8] << 24) | ((uint64_t)buf[9] << 16) |
		     ((uint64_t)buf[10] << 8) | (uint64_t)buf[11];
	storedFrag = ((uint32_t)buf[12] << 24) | ((uint32_t)buf[13] << 16) |
		     ((uint32_t)buf[14] << 8) | (uint32_t)buf[15];

	if (magic != FF4_CACHE_MAGIC || storedSize != availSize || storedFrag != fragIdx)
		return FF4_ERR_IO;

	if (outRead != NULL)
		*outRead = nread;
	if (accumRead != NULL)
		*accumRead += nread;
	return FF4_OK;
}

int32_t ff4_read_box_header(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			    uint64_t availSize, uint32_t *outFourcc,
			    uint64_t *outBoxSize, uint64_t *outHdrSize)
{
	uint8_t buf[8];
	uint64_t nread = 0;
	uint64_t boxSize = 0;
	uint64_t hdrSize = 0;
	uint32_t fourcc;
	int32_t ret;

	if (sess == NULL || stream == NULL || outFourcc == NULL || outBoxSize == NULL)
		return FF4_ERR_INVALID_ARG;
	if (availSize < 8u)
		return FF4_ERR_FORMAT;

	ret = ff4_stream_seek(sess, stream, (int64_t)pos, 0u);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_stream_read(sess, stream, buf, sizeof(buf), &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	hdrSize = nread;
	boxSize = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
		  ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	fourcc = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
		 ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];

	if (boxSize == 1u) {
		if (availSize < 16u)
			return FF4_ERR_FORMAT;
		ret = ff4_stream_read_be64(sess, stream, &boxSize);
		if (ret != FF4_OK)
			return ret;
		hdrSize += 8u;
	}

	if (boxSize != 0 && boxSize < 8u)
		return FF4_ERR_FORMAT;
	if (boxSize == 0)
		boxSize = availSize;
	if (boxSize < hdrSize)
		return FF4_ERR_FORMAT;
	if (boxSize > availSize)
		boxSize = availSize;

	*outFourcc = fourcc;
	*outBoxSize = boxSize;
	if (outHdrSize != NULL)
		*outHdrSize = hdrSize;
	return FF4_OK;
}

int32_t ff4_read_stts_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t entryCount,
			      Ff4SampleTableModel *frag, uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[8];
	uint64_t nread = 0;
	uint64_t total = 0;
	uint32_t i, j;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	for (i = 0; i < entryCount; i++) {
		uint32_t count, delta;
		ret = ff4_stream_read(sess, stream, buf, 8u, &nread);
		if (ret != FF4_OK)
			return ret;
		if (nread != 8u)
			return FF4_ERR_IO;
		total += 8u;
		count = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
		delta = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
			((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
		for (j = 0; j < count; j++) {
			ret = ff4_stts_append(sess, delta, &frag->sttsBlockCap);
			if (ret != FF4_OK)
				return ret;
		}
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stts_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[8];
	uint32_t entryCount;
	uint64_t rem;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc != FF4_BOX_STTS)
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 8u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	rem = boxSize - (hdrSize + 8u);
	entryCount = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		     ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	if ((rem & 7u) != 0 || (uint64_t)entryCount != (rem >> 3))
		return FF4_ERR_IO;

	ret = ff4_read_stts_entries(sess, stream, entryCount, frag, &bodyRead, NULL);
	if (ret == FF4_OK) {
		uint64_t total = hdrSize + 8u + bodyRead;
		if (outRead != NULL)
			*outRead = total;
		if (accumRead != NULL)
			*accumRead += total;
	}
	return ret;
}

int32_t ff4_read_ctts_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t entryCount,
			      Ff4SampleTableModel *frag, uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[8];
	uint64_t nread = 0;
	uint64_t total = 0;
	uint32_t i, j;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	for (i = 0; i < entryCount; i++) {
		uint32_t count, offset;
		ret = ff4_stream_read(sess, stream, buf, 8u, &nread);
		if (ret != FF4_OK)
			return ret;
		if (nread != 8u)
			return FF4_ERR_IO;
		total += 8u;
		count = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
		offset = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
			 ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
		for (j = 0; j < count; j++) {
			ret = ff4_ctts_append(sess, offset, &frag->cttsVersion);
			if (ret != FF4_OK)
				return ret;
		}
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_ctts_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[8];
	uint32_t entryCount;
	uint64_t rem;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc != FF4_BOX_CTTS)
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 8u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	rem = boxSize - (hdrSize + 8u);
	entryCount = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		     ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	if ((rem & 7u) != 0 || (uint64_t)entryCount != (rem >> 3))
		return FF4_ERR_IO;

	ret = ff4_read_ctts_entries(sess, stream, entryCount, frag, &bodyRead, NULL);
	if (ret == FF4_OK) {
		uint64_t total = hdrSize + 8u + bodyRead;
		if (outRead != NULL)
			*outRead = total;
		if (accumRead != NULL)
			*accumRead += total;
	}
	return ret;
}

int32_t ff4_read_stsc_entry(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t *outRead,
			    uint32_t *outFirstChunk, uint32_t *outSamples, uint32_t *outDesc)
{
	uint8_t buf[12];
	uint64_t nread = 0;
	int32_t ret;

	if (sess == NULL || stream == NULL || outRead == NULL || outFirstChunk == NULL ||
	    outSamples == NULL || outDesc == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_stream_read(sess, stream, buf, sizeof(buf), &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != sizeof(buf))
		return FF4_ERR_IO;

	*outRead = 12u;
	*outFirstChunk = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			 ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
	*outSamples = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
		      ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
	*outDesc = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
		   ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
	return FF4_OK;
}

int32_t ff4_read_stsc_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t entryCount,
			      Ff4SampleTableModel *frag, uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t totalChunks;
	uint32_t curFirst = 0, curSamples = 0, curDesc = 0;
	uint32_t nextFirst = 0, nextSamples = 0, nextDesc = 0;
	uint64_t stepRead = 0;
	uint64_t total = 0;
	uint32_t i, s;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	totalChunks = frag->stscCommittedChunks;
	frag->stscFirstChunk = 0;
	frag->stscChunkCount = 0;
	frag->stscCurDesc = 0;
	frag->stscCommittedChunks = 0;

	for (i = 0; i < entryCount; i++) {
		if (i == 0) {
			ret = ff4_read_stsc_entry(sess, stream, &stepRead,
						  &curFirst, &curSamples, &curDesc);
			if (ret != FF4_OK)
				return ret;
			total += stepRead;
		} else {
			curFirst = nextFirst;
			curSamples = nextSamples;
			curDesc = nextDesc;
		}

		if (i + 1u < entryCount) {
			ret = ff4_read_stsc_entry(sess, stream, &stepRead,
						  &nextFirst, &nextSamples, &nextDesc);
			if (ret != FF4_OK)
				return ret;
			total += stepRead;
		} else {
			nextFirst = totalChunks + 1u;
		}

		while (curFirst < nextFirst) {
			for (s = 0; s < curSamples; s++) {
				ret = ff4_stsc_add_sample(curDesc, &frag->stscFirstChunk);
				if (ret != FF4_OK)
					return ret;
			}
			ret = ff4_stsc_commit_chunk(sess, &frag->stscFirstChunk);
			if (ret != FF4_OK)
				return ret;
			curFirst++;
		}
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stsc_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[8];
	uint32_t entryCount;
	uint64_t rem;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc != FF4_BOX_STSC)
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 8u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	rem = boxSize - (hdrSize + 8u);
	entryCount = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		     ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	if ((rem % 12ULL) != 0 || (uint64_t)entryCount != (rem / 12ULL))
		return FF4_ERR_IO;

	ret = ff4_read_stsc_entries(sess, stream, entryCount, frag, &bodyRead, NULL);
	if (ret == FF4_OK) {
		uint64_t total = hdrSize + 8u + bodyRead;
		if (outRead != NULL)
			*outRead = total;
		if (accumRead != NULL)
			*accumRead += total;
	}
	return ret;
}

int32_t ff4_read_stsz_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t sampleCount,
			      uint32_t elemBytes, Ff4SampleTableModel *frag,
			      uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[4];
	uint64_t nread = 0;
	uint64_t total = 0;
	uint32_t i;
	uint32_t val = 0;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	if (elemBytes == 0) {
		uint8_t curByte = 0;
		for (i = 0; i < sampleCount; i++) {
			if ((i & 1u) != 0) {
				val = curByte & 0x0Fu;
			} else {
				ret = ff4_stream_read(sess, stream, &curByte, 1u, &nread);
				if (ret != FF4_OK)
					return ret;
				if (nread != 1u)
					return FF4_ERR_IO;
				val = (uint32_t)curByte >> 4;
			}
			ret = ff4_stsz_append(sess, val, &frag->fixedSampleSize);
			if (ret != FF4_OK)
				return ret;
		}
	} else {
		for (i = 0; i < sampleCount; i++) {
			ret = ff4_stream_read(sess, stream, buf, elemBytes, &nread);
			if (ret != FF4_OK)
				return ret;
			if (nread != elemBytes)
				return FF4_ERR_IO;
			total += nread;
			if (elemBytes == 1u)
				val = buf[0];
			else if (elemBytes == 2u)
				val = ((uint32_t)buf[0] << 8) | (uint32_t)buf[1];
			else if (elemBytes == 4u)
				val = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
				      ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
			ret = ff4_stsz_append(sess, val, &frag->fixedSampleSize);
			if (ret != FF4_OK)
				return ret;
		}
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stsz_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[12];
	uint32_t fixedSize, sampleCount;
	uint64_t rem, total;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc != FF4_BOX_STSZ)
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 12u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 12u)
		return FF4_ERR_IO;

	fixedSize = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		    ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	sampleCount = ((uint32_t)hdr[8] << 24) | ((uint32_t)hdr[9] << 16) |
		      ((uint32_t)hdr[10] << 8) | (uint32_t)hdr[11];
	rem = boxSize - (hdrSize + 12u);
	if ((rem & 3u) != 0)
		return FF4_ERR_IO;

	total = hdrSize + 12u;
	if (rem != 0) {
		if ((uint64_t)sampleCount != (rem >> 2))
			return FF4_ERR_IO;
		ret = ff4_read_stsz_entries(sess, stream, sampleCount, 4u, frag, &bodyRead, NULL);
		if (ret != FF4_OK)
			return ret;
		total += bodyRead;
	}

	frag->fixedSampleSize = fixedSize;
	frag->totalSamples = sampleCount;
	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stco_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t entryCount,
			      uint32_t elemBytes, Ff4SampleTableModel *frag,
			      uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[8];
	uint64_t nread = 0;
	uint64_t total = 0;
	uint32_t i;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || elemBytes == 0)
		return FF4_ERR_INVALID_ARG;

	for (i = 0; i < entryCount; i++) {
		uint64_t off;
		ret = ff4_stream_read(sess, stream, buf, elemBytes, &nread);
		if (ret != FF4_OK)
			return ret;
		if (nread != elemBytes)
			return FF4_ERR_IO;
		total += nread;
		if (elemBytes == 4u) {
			off = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			      ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
		} else {
			off = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) |
			      ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32) |
			      ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
			      ((uint64_t)buf[6] << 8) | (uint64_t)buf[7];
		}
		ret = ff4_stco_append(sess, off, &frag->stcoFirstSample);
		if (ret != FF4_OK)
			return ret;
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stco_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint32_t elemBytes;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[8];
	uint32_t entryCount;
	uint64_t rem;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc == FF4_BOX_STCO)
		elemBytes = 4u;
	else if (fourcc == FF4_BOX_CO64)
		elemBytes = 8u;
	else
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 8u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	rem = boxSize - (hdrSize + 8u);
	entryCount = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		     ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	if ((rem % (uint64_t)elemBytes) != 0 || (uint64_t)entryCount != (rem / (uint64_t)elemBytes))
		return FF4_ERR_IO;

	ret = ff4_read_stco_entries(sess, stream, entryCount, elemBytes, frag, &bodyRead, NULL);
	if (ret == FF4_OK) {
		uint64_t total = hdrSize + 8u + bodyRead;
		if (outRead != NULL)
			*outRead = total;
		if (accumRead != NULL)
			*accumRead += total;
	}
	return ret;
}

int32_t ff4_read_stss_entries(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t entryCount,
			      Ff4SampleTableModel *frag, uint64_t *outRead, uint64_t *accumRead)
{
	uint8_t buf[4];
	uint64_t nread = 0;
	uint64_t total = 0;
	uint32_t i;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL)
		return FF4_ERR_INVALID_ARG;

	for (i = 0; i < entryCount; i++) {
		uint32_t sampleNum;
		ret = ff4_stream_read(sess, stream, buf, 4u, &nread);
		if (ret != FF4_OK)
			return ret;
		if (nread != 4u)
			return FF4_ERR_IO;
		total += 4u;
		sampleNum = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			    ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
		ret = ff4_stss_push_entry(sess, sampleNum, &frag->hasSyncSample);
		if (ret != FF4_OK)
			return ret;
	}

	if (outRead != NULL)
		*outRead = total;
	if (accumRead != NULL)
		*accumRead += total;
	return FF4_OK;
}

int32_t ff4_read_stss_box(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
			  uint64_t availSize, Ff4SampleTableModel *frag,
			  uint64_t *outRead, uint64_t *accumRead)
{
	uint32_t fourcc = 0;
	uint64_t boxSize = 0, hdrSize = 0, nread = 0, bodyRead = 0;
	uint8_t hdr[8];
	uint32_t entryCount;
	uint64_t rem;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_box_header(sess, stream, pos, availSize, &fourcc, &boxSize, &hdrSize);
	if (ret != FF4_OK)
		return ret;
	if (fourcc != FF4_BOX_STSS)
		return FF4_ERR_IO;

	ret = ff4_stream_read(sess, stream, hdr, 8u, &nread);
	if (ret != FF4_OK)
		return ret;
	if (nread != 8u)
		return FF4_ERR_IO;

	rem = boxSize - (hdrSize + 8u);
	entryCount = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
		     ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
	if ((rem & 3u) != 0 || (uint64_t)entryCount != (rem >> 2))
		return FF4_ERR_IO;

	ret = ff4_read_stss_entries(sess, stream, entryCount, frag, &bodyRead, NULL);
	if (ret == FF4_OK) {
		uint64_t total = hdrSize + 8u + bodyRead;
		if (outRead != NULL)
			*outRead = total;
		if (accumRead != NULL)
			*accumRead += total;
	}
	return ret;
}

int32_t ff4_read_cache_fragment(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t pos,
				uint64_t availSize, Ff4SampleTableModel *frag,
				uint64_t *outRead, uint64_t *accumRead)
{
	uint64_t consumed = 0;
	int32_t ret;

	if (sess == NULL || stream == NULL || frag == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_read_stts_box(sess, stream, pos, availSize, frag, NULL, &consumed);
	if (ret != FF4_OK)
		return ret;
	if (availSize <= consumed)
		return FF4_ERR_FORMAT;

	ret = ff4_read_ctts_box(sess, stream, pos + consumed, availSize - consumed,
				frag, NULL, &consumed);
	if (ret != FF4_OK)
		return ret;
	if (availSize <= consumed)
		return FF4_ERR_FORMAT;

	ret = ff4_read_stsc_box(sess, stream, pos + consumed, availSize - consumed,
				frag, NULL, &consumed);
	if (ret != FF4_OK)
		return ret;
	if (availSize <= consumed)
		return FF4_ERR_FORMAT;

	ret = ff4_read_stsz_box(sess, stream, pos + consumed, availSize - consumed,
				frag, NULL, &consumed);
	if (ret != FF4_OK)
		return ret;
	if (availSize <= consumed)
		return FF4_ERR_FORMAT;

	ret = ff4_read_stco_box(sess, stream, pos + consumed, availSize - consumed,
				frag, NULL, &consumed);
	if (ret != FF4_OK)
		return ret;
	if (availSize <= consumed)
		return FF4_ERR_FORMAT;

	ret = ff4_read_stss_box(sess, stream, pos + consumed, availSize - consumed,
				frag, NULL, &consumed);
	if (ret == FF4_OK) {
		if (outRead != NULL)
			*outRead = consumed;
		if (accumRead != NULL)
			*accumRead += consumed;
	}
	return ret;
}
