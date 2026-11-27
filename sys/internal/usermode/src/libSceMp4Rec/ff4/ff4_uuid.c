/*
 * Sony FF4 MP4 muxer: Proprietary UUID boxes — PROF (FPRF/VPRF/APRF/BPRF/PPRF)
 * and USMT (MTDT/MTDF/MTSM/MTSH/MTCT) planning, manipulation, and serialization.
 */
#include "ff4_internal.h"

#define FF4_BOX_UUID 0x75756964u
#define FF4_BOX_VPRF 0x56505246u
#define FF4_BOX_APRF 0x41505246u
#define FF4_BOX_BPRF 0x42505246u
#define FF4_BOX_PPRF 0x50505246u
#define FF4_BOX_FPRF 0x46505246u
#define FF4_BOX_MTCT 0x4D544354u
#define FF4_BOX_MTSH 0x4D545348u
#define FF4_BOX_MTSM 0x4D54534Du
#define FF4_BOX_MTDT 0x4D544454u
#define FF4_BOX_MTDF 0x4D544446u

#define FF4_UUID_PROF_0 0x50524F46u
#define FF4_UUID_USMT_0 0x55534D54u
#define FF4_UUID_SONY_1 0x21D24FCEu
#define FF4_UUID_SONY_2 0xBB88695Cu
#define FF4_UUID_SONY_3 0xFAC9C740u

typedef struct Ff4RawUuidPlan {
	uint64_t boxSize;
	uint32_t uuid[4];
} Ff4RawUuidPlan;

typedef struct Ff4ProfPlan {
	uint64_t boxSize;
	Ff4ProfModel *prof;
	uint32_t pad;
} Ff4ProfPlan;

typedef struct Ff4MtdtInput {
	uint32_t fourcc;
	uint8_t isUtf16;
	uint8_t padA;
	uint16_t langCode;
	uint16_t dataType;
	uint16_t padB;
	const uint8_t *data;
	uint32_t size;
} Ff4MtdtInput;

typedef struct Ff4MtdfInput {
	uint32_t fourcc;
	uint16_t langCode;
	uint16_t dataType;
	const uint8_t *data;
	uint32_t size;
} Ff4MtdfInput;

typedef struct Ff4ZpckTableInput {
	uint32_t fourcc;
	uint8_t isUtf16;
	uint8_t pad;
	uint16_t langCode;
	uint32_t versionVal;
} Ff4ZpckTableInput;

typedef struct Ff4MtshModel {
	uint64_t creationTime;
	uint64_t modTime;
	uint32_t streamId;
	uint32_t pad;
} Ff4MtshModel;

typedef struct Ff4MtshPlan {
	uint8_t version;
	uint8_t pad[7];
	Ff4MtshModel model;
} Ff4MtshPlan;

typedef struct Ff4MtctPlan {
	uint64_t boxSize;
	Ff4NodeTriple *head;
	Ff4NodeTriple *tail;
} Ff4MtctPlan;

typedef struct Ff4MtdtPlan {
	uint64_t boxSize;
	Ff4MtdtEntry *head;
	uint32_t pad;
} Ff4MtdtPlan;

typedef struct Ff4MtdfPlan {
	uint64_t boxSize;
	Ff4MtdfEntry *head;
	uint32_t pad;
} Ff4MtdfPlan;

typedef struct Ff4UsmtModel {
	Ff4MtdtEntry *mtdtHead;
	Ff4MtdtEntry *mtdtTail;
	Ff4MtdfEntry *mtdfHead;
	Ff4MtdfEntry *mtdfTail;
	Ff4MtsmEntry *mtsmHead;
	Ff4MtsmEntry *mtsmTail;
} Ff4UsmtModel;

int32_t ff4_write_raw_uuid_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *rawUuidPlan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4RawUuidPlan *plan = (const Ff4RawUuidPlan *)rawUuidPlan;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_UUID,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_uuid_bytes(sess, fileCtx, pos + total, plan->uuid, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_pprf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *words, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[16];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || words == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be32(buf + 4, words[0]);
		ff4_put_be32(buf + 8, words[1]);
		ff4_put_be32(buf + 12, words[2]);
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

int32_t ff4_write_bprf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *words, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[16];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || words == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be32(buf + 4, words[0]);
		ff4_put_be32(buf + 8, words[1]);
		ff4_put_be32(buf + 12, words[2]);
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

int32_t ff4_write_aprf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *words, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[36];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || words == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be32(buf + 4, words[0]);
		ff4_put_be32(buf + 8, words[1]);
		ff4_put_be32(buf + 12, words[2]);
		ff4_put_be32(buf + 16, words[3]);
		ff4_put_be32(buf + 20, words[4]);
		ff4_put_be32(buf + 24, words[5]);
		ff4_put_be32(buf + 28, words[6]);
		ff4_put_be32(buf + 32, words[7]);
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

int32_t ff4_write_vprf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *words, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[44];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || words == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be32(buf + 4, words[0]);
		ff4_put_be32(buf + 8, words[1]);
		ff4_put_be32(buf + 12, words[2]);
		ff4_put_be32(buf + 16, words[3]);
		ff4_put_be32(buf + 20, words[4]);
		ff4_put_be32(buf + 24, words[5]);
		ff4_put_be32(buf + 28, words[6]);
		ff4_put_be32(buf + 32, words[7]);
		ff4_put_be32(buf + 36, words[8]);
		ff4_put_be32(buf + 40, words[9]);
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

int32_t ff4_write_prof_entries(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4ProfEntry *head,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4ProfEntry *cur;
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	for (cur = head; cur != NULL; cur = cur->next) {
		if (cur->fourcc != FF4_BOX_VPRF && cur->fourcc != FF4_BOX_APRF &&
		    cur->fourcc != FF4_BOX_BPRF && cur->fourcc != FF4_BOX_PPRF)
			return FF4_ERR_INTERNAL;

		switch (cur->fourcc) {
		case FF4_BOX_VPRF:
			ret = ff4_calc_box_size(sess, opts, 44u, &boxSize);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_box_header(sess, opts, fileCtx, pos + total, boxSize,
						   FF4_BOX_VPRF, NULL, &total);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_vprf_payload(sess, fileCtx, pos + total, cur->fields,
						     NULL, &total);
			if (ret != FF4_OK)
				return ret;
			break;
		case FF4_BOX_PPRF:
			ret = ff4_calc_box_size(sess, opts, 16u, &boxSize);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_box_header(sess, opts, fileCtx, pos + total, boxSize,
						   FF4_BOX_PPRF, NULL, &total);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_pprf_payload(sess, fileCtx, pos + total, cur->fields,
						     NULL, &total);
			if (ret != FF4_OK)
				return ret;
			break;
		case FF4_BOX_BPRF:
			ret = ff4_calc_box_size(sess, opts, 16u, &boxSize);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_box_header(sess, opts, fileCtx, pos + total, boxSize,
						   FF4_BOX_BPRF, NULL, &total);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_bprf_payload(sess, fileCtx, pos + total, cur->fields,
						     NULL, &total);
			if (ret != FF4_OK)
				return ret;
			break;
		case FF4_BOX_APRF:
			ret = ff4_calc_box_size(sess, opts, 36u, &boxSize);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_box_header(sess, opts, fileCtx, pos + total, boxSize,
						   FF4_BOX_APRF, NULL, &total);
			if (ret != FF4_OK)
				return ret;
			ret = ff4_write_aprf_payload(sess, fileCtx, pos + total, cur->fields,
						     NULL, &total);
			if (ret != FF4_OK)
				return ret;
			break;
		default:
			return FF4_ERR_INTERNAL;
		}
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_fprf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const uint32_t *fprfFlags, uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t buf[12];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || fprfFlags == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be32(buf + 4, *fprfFlags);
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

int32_t ff4_write_fprf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const uint32_t *fprfFlags,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || fprfFlags == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, 12u, &boxSize);
	if (ret == FF4_OK) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_FPRF,
					   NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_fprf_payload(sess, fileCtx, pos + total, fprfFlags,
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

int32_t ff4_write_prof_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const void *profPlan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4ProfPlan *plan = (const Ff4ProfPlan *)profPlan;
	uint32_t uuid[4];
	uint8_t hdr[8];
	uint32_t count = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, uuid, 0, sizeof(uuid));
	uuid[0] = FF4_UUID_PROF_0;
	uuid[1] = FF4_UUID_SONY_1;
	uuid[2] = FF4_UUID_SONY_2;
	uuid[3] = FF4_UUID_SONY_3;

	ret = ff4_write_uuid_bytes(sess, fileCtx, pos, uuid, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_count_prof_entries(plan->prof, &count);
	if (ret == FF4_OK) {
		ff4_memset(sess, hdr, 0, sizeof(hdr));
		ff4_put_be32(hdr + 4, count);
		ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	}
	if (ret == FF4_OK && plan->prof != NULL) {
		ret = ff4_write_fprf_box(sess, opts, fileCtx, pos + total, &plan->prof->baseField,
					 NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_prof_entries(sess, opts, fileCtx, pos + total,
						     plan->prof->head, NULL, &total);
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

int32_t ff4_plan_prof(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, Ff4ProfModel *prof,
		      void *outProfPlan)
{
	Ff4ProfPlan tmp;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || prof == NULL || outProfPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.prof = prof;
	ret = ff4_write_prof_payload(sess, opts, NULL, 0, &tmp, &payloadSize, NULL);
	if (ret == FF4_OK) {
		ret = ff4_calc_box_size(sess, opts, payloadSize, &tmp.boxSize);
		if (ret == FF4_OK)
			ff4_memcpy(sess, outProfPlan, sizeof(tmp), &tmp, sizeof(tmp));
	}
	return ret;
}

int32_t ff4_write_prof_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *profPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4ProfPlan *plan = (const Ff4ProfPlan *)profPlan;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_UUID,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_prof_payload(sess, opts, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_find_mtdt_entry(Ff4SessionImpl *sess, Ff4MtdtEntry *head, uint32_t fourcc,
			    uint16_t langCode, uint16_t dataType,
			    Ff4MtdtEntry **outEntry, uint32_t *outIndex1)
{
	Ff4MtdtEntry *cur = head;
	uint32_t idx = 0;

	if (sess == NULL || outEntry == NULL)
		return FF4_ERR_INVALID_ARG;

	for (; cur != NULL; cur = cur->next) {
		idx++;
		if (cur->fourcc == fourcc && cur->langCode == langCode && cur->dataType == dataType)
			break;
	}
	*outEntry = cur;
	if (outIndex1 != NULL)
		*outIndex1 = idx;
	return FF4_OK;
}

int32_t ff4_add_mtdt_entry(Ff4SessionImpl *sess, Ff4MtdtEntry **headTail,
			   const void *inputPtr, uint32_t *outIndex1)
{
	const Ff4MtdtInput *in = (const Ff4MtdtInput *)inputPtr;
	Ff4MtdtEntry *node = NULL;
	Ff4MtdtEntry *existing = NULL;
	uint32_t idx = 0;
	int32_t ret;

	if (sess == NULL || headTail == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4MtdtEntry *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->fourcc = in->fourcc;
	node->isUtf16 = in->isUtf16;
	node->langCode = in->langCode;
	node->dataType = in->dataType;

	ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&node->data, in->data, in->size);
	if (ret == FF4_OK) {
		ret = ff4_find_mtdt_entry(sess, headTail[0], in->fourcc, in->langCode,
					  in->dataType, &existing, &idx);
		if (ret == FF4_OK) {
			if (existing != NULL) {
				existing->isUtf16 = node->isUtf16;
				ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&existing->data,
							node->data, node->size);
			} else {
				if (headTail[0] != NULL)
					headTail[1]->next = node;
				else
					headTail[0] = node;
				headTail[1] = node;
				node = NULL;
				ret = ff4_count_mtdt_entries(headTail[0], &idx);
			}
			if (ret == FF4_OK && outIndex1 != NULL)
				*outIndex1 = idx;
		}
	}

	ff4_free_mtdt_entry_list(sess, node);
	return ret;
}

int32_t ff4_encode_zpck_header(Ff4SessionImpl *sess, const Ff4ZpckHeader *hdr,
			       Ff4BoxDataBuf *outBuf)
{
	uint8_t *buf;

	if (sess == NULL || hdr == NULL || outBuf == NULL)
		return FF4_ERR_INVALID_ARG;

	buf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, 8u);
	if (buf == NULL)
		return FF4_ERR_NO_MEM;

	ff4_put_be32(buf + 0, hdr->fourcc);
	buf[4] = hdr->version;
	ff4_put_be16(buf + 6, hdr->flags);
	outBuf->data = buf;
	outBuf->size = 8u;
	return FF4_OK;
}

int32_t ff4_add_mtdt_zpck_header(Ff4SessionImpl *sess, Ff4MtdtEntry **headTail,
				 uint8_t isUtf16, const uint32_t *hdrVals)
{
	Ff4MtdtInput *in = NULL;
	Ff4BoxDataBuf buf;
	int32_t ret;

	if (sess == NULL || headTail == NULL || hdrVals == NULL)
		return FF4_ERR_INVALID_ARG;

	in = (Ff4MtdtInput *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*in));
	if (in == NULL)
		return FF4_ERR_NO_MEM;

	in->fourcc = 10u;
	in->isUtf16 = isUtf16;
	in->langCode = 0x55C4u;
	in->dataType = 0u;

	ret = ff4_encode_zpck_header(sess, (const Ff4ZpckHeader *)hdrVals, &buf);
	if (ret == FF4_OK) {
		in->data = buf.data;
		in->size = (uint32_t)buf.size;
		buf.data = NULL;
		ret = ff4_add_mtdt_entry(sess, headTail, in, NULL);
	}
	ff4_free_mtdt_input(sess, in);
	return ret;
}

int32_t ff4_encode_zpck_table(Ff4SessionImpl *sess, const Ff4ZpckTable *table,
			      Ff4BoxDataBuf *outBuf)
{
	uint32_t count = 0;
	uint64_t totalSize;
	uint8_t *buf = NULL;
	uint8_t *p;
	const Ff4NodePair *cur;
	int32_t ret;

	if (sess == NULL || table == NULL || outBuf == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_count_pair_list(table->head, &count);
	if (ret == FF4_OK) {
		totalSize = 12ULL * (uint64_t)count + 12ULL;
		buf = (uint8_t *)ff4_pool_alloc(sess, FF4_POOL_META, totalSize);
		if (buf == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ff4_put_be32(buf + 0, table->version);
			ff4_put_be32(buf + 8, count);
			p = buf + 12;
			for (cur = table->head; cur != NULL; cur = cur->next) {
				if (p - buf < 12) {
					ret = FF4_ERR_INTERNAL;
					break;
				}
				ff4_put_be32(p + 0, cur->a);
				ff4_put_be32(p + 4, cur->b);
				p += 12;
			}
			if (ret == FF4_OK) {
				outBuf->data = buf;
				outBuf->size = totalSize;
				buf = NULL;
			}
		}
	}
	ff4_pool_free(sess, buf);
	return ret;
}

int32_t ff4_add_mtdt_zpck_table(Ff4SessionImpl *sess, Ff4MtdtEntry **headTail,
				const void *inputPtr, uint32_t *outIndex1)
{
	const Ff4ZpckTableInput *param = (const Ff4ZpckTableInput *)inputPtr;
	Ff4MtdtInput *in = NULL;
	Ff4ZpckTable tbl;
	Ff4BoxDataBuf buf;
	uint32_t idx = 0;
	int32_t ret;

	if (sess == NULL || headTail == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;

	in = (Ff4MtdtInput *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*in));
	if (in == NULL)
		return FF4_ERR_NO_MEM;

	in->fourcc = param->fourcc;
	in->isUtf16 = param->isUtf16;
	in->langCode = param->langCode;
	in->dataType = 0x0101u;

	ff4_memset(sess, &tbl, 0, sizeof(tbl));
	tbl.version = param->versionVal;

	ret = ff4_encode_zpck_table(sess, &tbl, &buf);
	if (ret == FF4_OK) {
		in->data = buf.data;
		in->size = (uint32_t)buf.size;
		buf.data = NULL;
		ret = ff4_add_mtdt_entry(sess, headTail, in, &idx);
		if (ret == FF4_OK && outIndex1 != NULL)
			*outIndex1 = idx;
	}
	ff4_free_mtdt_input(sess, in);
	return ret;
}

int32_t ff4_write_zpck_table_to_buf(Ff4SessionImpl *sess, Ff4BoxDataBuf *dstBuf,
				    const Ff4ZpckTable *table)
{
	Ff4BoxDataBuf *tmp;
	int32_t ret;

	if (sess == NULL || dstBuf == NULL || table == NULL)
		return FF4_ERR_INVALID_ARG;

	tmp = (Ff4BoxDataBuf *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*tmp));
	if (tmp == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_encode_zpck_table(sess, table, tmp);
	if (ret == FF4_OK)
		ret = ff4_copy_data_buf(sess, 0, dstBuf, tmp->data, tmp->size);

	ff4_pool_free(sess, tmp->data);
	ff4_pool_free(sess, tmp);
	return ret;
}

int32_t ff4_append_zpck_pair(Ff4SessionImpl *sess, Ff4ZpckTable *table, const uint32_t pair[2])
{
	Ff4NodePair *node;

	if (sess == NULL || table == NULL || pair == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4NodePair *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->a = pair[0];
	node->b = pair[1];
	node->next = NULL;

	if (table->head != NULL)
		table->tail->next = node;
	else
		table->head = node;
	table->tail = node;
	return FF4_OK;
}

int32_t ff4_append_zpck_entry_to_mtdt(Ff4SessionImpl *sess, Ff4MtdtEntry *mtdt,
				      const uint32_t pair[2])
{
	Ff4ZpckTable *table = NULL;
	int32_t ret;

	if (sess == NULL || mtdt == NULL || pair == NULL)
		return FF4_ERR_INVALID_ARG;

	table = (Ff4ZpckTable *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*table));
	if (table == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_parse_zpck_table(sess, mtdt->data, 0, mtdt->size, table);
	if (ret == FF4_OK) {
		ret = ff4_append_zpck_pair(sess, table, pair);
		if (ret == FF4_OK)
			ret = ff4_write_zpck_table_to_buf(sess, (Ff4BoxDataBuf *)&mtdt->data, table);
	}
	ff4_free_zpck_table(sess, table);
	return ret;
}

int32_t ff4_set_mtdt_item(Ff4SessionImpl *sess, Ff4MovieModel *moov, uint32_t trackIdx,
			  const void *inputPtr)
{
	const Ff4MtdtInput *in = (const Ff4MtdtInput *)inputPtr;
	Ff4TrackModel *track = NULL;
	Ff4MtdtEntry **headTail;
	int32_t ret;

	if (sess == NULL || moov == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	if ((in->isUtf16 & 0xFEu) != 0 || (in->langCode & 0x8000u) != 0 ||
	    in->size > 0xFFF5u || (in->size != 0 && in->data == NULL))
		return FF4_ERR_INVALID_ARG;

	if (trackIdx != 0) {
		ret = ff4_get_track_at(moov->trackHead, trackIdx, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_NOT_FOUND;
		headTail = &track->mtdtHead;
	} else {
		headTail = &moov->mtdtHead;
	}
	return ff4_add_mtdt_entry(sess, headTail, in, NULL);
}

int32_t ff4_get_mtdt_zpck_header(Ff4SessionImpl *sess, Ff4MtdtEntry *const *headTail,
				 Ff4ZpckHeader *outHdr, Ff4MtdtEntry **outEntry)
{
	Ff4ZpckHeader hdr;
	Ff4MtdtEntry *entry = NULL;
	int32_t ret;

	if (sess == NULL || headTail == NULL || outHdr == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &hdr, 0, sizeof(hdr));
	ret = ff4_find_mtdt_entry(sess, headTail[0], 10u, 0x55C4u, 0u, &entry, NULL);
	if (ret == FF4_OK) {
		if (entry != NULL)
			ret = ff4_parse_zpck_header(sess, entry->data, 0, entry->size, &hdr);
		if (ret == FF4_OK) {
			ff4_memcpy(sess, outHdr, sizeof(*outHdr), &hdr, sizeof(hdr));
			if (outEntry != NULL)
				*outEntry = entry;
		}
	}
	return ret;
}

int32_t ff4_find_mtdf_entry(Ff4SessionImpl *sess, Ff4MtdfEntry *head, uint32_t fourcc,
			    uint16_t langCode, uint16_t dataType, Ff4MtdfEntry **outEntry)
{
	Ff4MtdfEntry *cur = head;

	if (sess == NULL || outEntry == NULL)
		return FF4_ERR_INVALID_ARG;

	for (; cur != NULL; cur = cur->next) {
		if (cur->fourcc == fourcc && cur->langCode == langCode && cur->dataType == dataType)
			break;
	}
	*outEntry = cur;
	return FF4_OK;
}

int32_t ff4_add_mtdf_entry(Ff4SessionImpl *sess, Ff4MtdfEntry **headTail, const void *inputPtr)
{
	const Ff4MtdfInput *in = (const Ff4MtdfInput *)inputPtr;
	Ff4MtdfEntry *node = NULL;
	Ff4MtdfEntry *existing = NULL;
	int32_t ret;

	if (sess == NULL || headTail == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4MtdfEntry *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->fourcc = in->fourcc;
	node->langCode = in->langCode;
	node->dataType = in->dataType;

	ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&node->data, in->data, in->size);
	if (ret == FF4_OK) {
		ret = ff4_find_mtdf_entry(sess, headTail[0], in->fourcc, in->langCode,
					  in->dataType, &existing);
		if (ret == FF4_OK) {
			if (existing != NULL) {
				ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&existing->data,
							node->data, node->size);
			} else {
				if (headTail[0] != NULL)
					headTail[1]->next = node;
				else
					headTail[0] = node;
				headTail[1] = node;
				node = NULL;
			}
		}
	}

	ff4_free_meta_item_list(sess, (Ff4MetaItemNode *)node);
	return ret;
}

int32_t ff4_set_mtdf_item(Ff4SessionImpl *sess, Ff4MovieModel *moov, uint32_t trackIdx,
			  const void *inputPtr)
{
	const Ff4MtdfInput *in = (const Ff4MtdfInput *)inputPtr;
	Ff4TrackModel *track = NULL;
	Ff4MtdfEntry **headTail;
	int32_t ret;

	if (sess == NULL || moov == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	if ((in->langCode & 0x8000u) != 0 || in->size > 0xFFF5u ||
	    (in->size != 0 && in->data == NULL))
		return FF4_ERR_INVALID_ARG;

	if (trackIdx != 0) {
		ret = ff4_get_track_at(moov->trackHead, trackIdx, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_NOT_FOUND;
		headTail = &track->mtdfHead;
	} else {
		headTail = &moov->mtdfHead;
	}
	return ff4_add_mtdf_entry(sess, headTail, in);
}

int32_t ff4_is_valid_mtsm_id(uint32_t id, int32_t *outValid)
{
	if (outValid == NULL)
		return FF4_ERR_INVALID_ARG;
	*outValid = (id >= 0xF800u && id <= 0xFFFFu) ? 1 : 0;
	return FF4_OK;
}

int32_t ff4_is_mtsm_id_available(uint32_t id, Ff4MtsmEntry *head, int32_t *outAvail)
{
	Ff4MtsmEntry *found = NULL;
	int32_t valid = 0;
	int32_t ret;

	if (outAvail == NULL)
		return FF4_ERR_INVALID_ARG;

	valid = (id >= 0xF800u && id <= 0xFFFFu) ? 1 : 0;
	if (valid != 0) {
		ret = ff4_find_mtsm_by_id(head, id, &found);
		if (ret != FF4_OK)
			return ret;
		valid = (found == NULL) ? 1 : 0;
	}
	*outAvail = valid;
	return FF4_OK;
}

int32_t ff4_set_mtsh_params(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    Ff4MtsmEntry *mtsm, uint32_t mask, const void *paramsPtr)
{
	const Ff4MtshModel *params = (const Ff4MtshModel *)paramsPtr;
	Ff4MtshModel tmp;

	if (sess == NULL || opts == NULL || mtsm == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &tmp, sizeof(tmp), mtsm, sizeof(tmp));
	if ((mask & 1u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (params->creationTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.creationTime = params->creationTime;
	}
	if ((mask & 2u) != 0) {
		if ((opts->boxVerMode == 0 || opts->boxVerMode == 2) &&
		    (params->modTime >> 32) != 0)
			return FF4_ERR_INVALID_STATE;
		tmp.modTime = params->modTime;
	}
	if ((mask & 4u) != 0)
		tmp.streamId = params->streamId;

	ff4_memcpy(sess, mtsm, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_mtsm_timestamps(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				Ff4MtsmEntry *mtsm, uint8_t flags,
				uint64_t creationTime, uint64_t modTime)
{
	Ff4MtshModel tmp;
	uint32_t mask = 0;

	if (sess == NULL || opts == NULL || mtsm == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	if ((flags & 0x40u) != 0) {
		mask = 1u;
		tmp.creationTime = creationTime;
	}
	if ((flags & 0x80u) != 0) {
		mask |= 2u;
		tmp.modTime = modTime;
	}
	return ff4_set_mtsh_params(sess, opts, mtsm, mask, &tmp);
}

int32_t ff4_add_mtct_entry(Ff4SessionImpl *sess, Ff4NodeTriple **headTail,
			   const uint32_t vals[3], uint32_t *outCount)
{
	Ff4NodeTriple *node;
	uint32_t count = 0;
	int32_t ret;

	if (headTail == NULL || vals == NULL || outCount == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4NodeTriple *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->a = vals[0];
	node->b = vals[1];
	node->c = vals[2];
	node->next = NULL;

	if (headTail[0] != NULL)
		headTail[1]->next = node;
	else
		headTail[0] = node;
	headTail[1] = node;

	ret = ff4_count_triple_list(headTail[0], &count);
	if (ret == FF4_OK)
		*outCount = count;
	return ret;
}

int32_t ff4_write_mtct_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *mtctPlan, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtctPlan *plan = (const Ff4MtctPlan *)mtctPlan;
	Ff4NodeTriple *listHT[2];
	const Ff4NodeTriple *cur;
	uint32_t count = 0;
	uint8_t hdr[8];
	uint8_t item[20];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, listHT, sizeof(listHT), &plan->head, sizeof(listHT));
	ret = ff4_count_triple_list(listHT[0], &count);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be32(hdr + 4, count);
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = listHT[0]; cur != NULL; cur = cur->next) {
		ff4_memset(sess, item, 0, sizeof(item));
		ff4_put_be32(item + 0, cur->a);
		ff4_put_be32(item + 4, cur->b);
		ff4_put_be32(item + 8, cur->c);
		ret = ff4_box_write(sess, fileCtx, item, sizeof(item), NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_mtct_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *mtctPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtctPlan *plan = (const Ff4MtctPlan *)mtctPlan;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MTCT,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mtct_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mtsh_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *mtshPlan, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtshPlan *plan = (const Ff4MtshPlan *)mtshPlan;
	Ff4MtshModel model;
	uint8_t buf[32];
	uint32_t len;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &model, sizeof(model), &plan->model, sizeof(model));
	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[0] = plan->version;
	if (plan->version == 1u) {
		ff4_put_be64(buf + 4, model.creationTime);
		ff4_put_be64(buf + 12, model.modTime);
		ff4_put_be32(buf + 20, model.streamId);
		len = 32u;
	} else {
		ff4_put_be32(buf + 4, (uint32_t)model.creationTime);
		ff4_put_be32(buf + 8, (uint32_t)model.modTime);
		ff4_put_be32(buf + 12, model.streamId);
		len = 24u;
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

int32_t ff4_write_mtsh_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *mtshPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtshPlan *plan = (const Ff4MtshPlan *)mtshPlan;
	uint64_t boxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(sess, opts, (plan->version == 1u) ? 32u : 24u, &boxSize);
	if (ret == FF4_OK) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, boxSize, FF4_BOX_MTSH,
					   NULL, &total);
		if (ret == FF4_OK) {
			ret = ff4_write_mtsh_payload(sess, fileCtx, pos + total, plan, NULL, &total);
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

int32_t ff4_write_mtsm_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4MtsmPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_mtsh_box(sess, opts, fileCtx, pos, plan->mthdPlan, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dinf_box(sess, opts, fileCtx, pos + total,
					 (const Ff4DinfPlan *)plan->dinfPlan, NULL, &total);
	}
	if (ret == FF4_OK) {
		ret = ff4_write_stsd_box(sess, opts, fileCtx, pos + total,
					 &plan->stsdBoxSize, NULL, &total);
	}
	if (ret == FF4_OK) {
		ret = ff4_write_mtct_box(sess, opts, fileCtx, pos + total,
					 plan->mdstPlan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mtsm_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4MtsmPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MTSM,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_mtsm_payload(sess, opts, fileCtx, pos + total, plan,
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

int32_t ff4_write_mtdt_item_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MtdtEntry *entry,
				    uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[4];
	uint16_t packedLang;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || entry == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	packedLang = (uint16_t)((entry->langCode & 0x7FFFu) | ((uint32_t)entry->isUtf16 << 15));
	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be16(hdr + 0, packedLang);
	ff4_put_be16(hdr + 2, entry->dataType);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, entry->data, entry->size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mtdt_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *mtdtPlan, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtdtPlan *plan = (const Ff4MtdtPlan *)mtdtPlan;
	const Ff4MtdtEntry *cur;
	uint32_t count = 0;
	uint8_t hdr[2];
	uint64_t itemBoxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_count_mtdt_entries(plan->head, &count);
	if (ret != FF4_OK)
		return ret;
	if ((count & 0xFFFF0000u) != 0)
		return FF4_ERR_OVERFLOW;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be16(hdr, (uint16_t)count);
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_calc_short_box_size(sess, cur->size + 4ULL, &itemBoxSize);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_write_short_box_header(sess, fileCtx, pos + total, itemBoxSize,
						 cur->fourcc, NULL, &total);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_write_mtdt_item_payload(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_mtdt_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *mtdtPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtdtPlan *plan = (const Ff4MtdtPlan *)mtdtPlan;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MTDT,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_mtdt_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mtdf_item_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const Ff4MtdfEntry *entry,
				    uint64_t *outWritten, uint64_t *accumWritten)
{
	uint8_t hdr[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || entry == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be16(hdr + 0, (uint16_t)(entry->langCode & 0x7FFFu));
	ff4_put_be16(hdr + 2, entry->dataType);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, entry->data, entry->size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mtdf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *mtdfPlan, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtdfPlan *plan = (const Ff4MtdfPlan *)mtdfPlan;
	const Ff4MtdfEntry *cur;
	uint32_t count = 0;
	uint8_t hdr[2];
	uint64_t itemBoxSize = 0;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_count_meta_item_list((const Ff4MetaItemNode *)plan->head, &count);
	if (ret != FF4_OK)
		return ret;
	if ((count & 0xFFFF0000u) != 0)
		return FF4_ERR_OVERFLOW;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_put_be16(hdr, (uint16_t)count);
	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->head; cur != NULL; cur = cur->next) {
		ret = ff4_calc_short_box_size(sess, cur->size + 4ULL, &itemBoxSize);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_write_short_box_header(sess, fileCtx, pos + total, itemBoxSize,
						 cur->fourcc, NULL, &total);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_write_mtdf_item_payload(sess, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_mtdf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *mtdfPlan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4MtdfPlan *plan = (const Ff4MtdfPlan *)mtdfPlan;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_MTDF,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_mtdf_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_usmt_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			       uint64_t pos, const Ff4UuidPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint32_t uuid[4];
	const Ff4MtsmPlan *cur;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, uuid, 0, sizeof(uuid));
	uuid[0] = FF4_UUID_USMT_0;
	uuid[1] = FF4_UUID_SONY_1;
	uuid[2] = FF4_UUID_SONY_2;
	uuid[3] = FF4_UUID_SONY_3;

	ret = ff4_write_uuid_bytes(sess, fileCtx, pos, uuid, NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_mtdf_box(sess, opts, fileCtx, pos + total, plan->mtdfPlan,
					 NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_mtdt_box(sess, opts, fileCtx, pos + total, plan->mtdtPlan,
					 NULL, &total);
	if (ret == FF4_OK) {
		for (cur = plan->mtsmHead; cur != NULL; cur = cur->next) {
			ret = ff4_write_mtsm_box(sess, opts, fileCtx, pos + total, cur,
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

int32_t ff4_plan_mtct(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4NodeTriple *const *headTail, void *outMtctPlan)
{
	Ff4MtctPlan *plan = (Ff4MtctPlan *)outMtctPlan;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || headTail == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->head, sizeof(Ff4NodeTriple *) * 2u, headTail,
		   sizeof(Ff4NodeTriple *) * 2u);
	ret = ff4_write_mtct_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_mtsh(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4MtsmEntry *mtsm, void *outMtshPlan)
{
	Ff4MtshPlan *plan = (Ff4MtshPlan *)outMtshPlan;

	if (sess == NULL || opts == NULL || mtsm == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->model, sizeof(plan->model), mtsm, sizeof(plan->model));
	return ff4_choose_box_version(opts, plan->model.creationTime, plan->model.modTime,
				      0ULL, &plan->version);
}

int32_t ff4_plan_mtsm(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4MtsmEntry *mtsm, Ff4MtsmPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_plan_mtsh(sess, opts, mtsm, outPlan->mthdPlan);
	if (ret == FF4_OK)
		ret = ff4_plan_dinf(sess, opts, &mtsm->drefHead, (Ff4DinfPlan *)outPlan->dinfPlan);
	if (ret == FF4_OK)
		ret = ff4_plan_stsd_list(sess, opts, 0u, &mtsm->stsdHead, &outPlan->stsdBoxSize);
	if (ret == FF4_OK)
		ret = ff4_plan_mtct(sess, opts, &mtsm->mdstHead, outPlan->mdstPlan);
	if (ret == FF4_OK)
		ret = ff4_write_mtsm_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_plan_mtdf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4MtdfEntry *head, void *outMtdfPlan)
{
	Ff4MtdfPlan *plan = (Ff4MtdfPlan *)outMtdfPlan;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan->head = head;
	ret = ff4_write_mtdf_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_mtdt(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      Ff4MtdtEntry *head, void *outMtdtPlan)
{
	Ff4MtdtPlan *plan = (Ff4MtdtPlan *)outMtdtPlan;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan->head = head;
	ret = ff4_write_mtdt_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_usmt(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *usmtModel,
		      Ff4UuidPlan *outPlan)
{
	Ff4UsmtModel *model = (Ff4UsmtModel *)usmtModel;
	Ff4MtdtPlan *mtdt = NULL;
	Ff4MtdfPlan *mtdf = NULL;
	Ff4MtsmPlan *mtsmPlan = NULL;
	Ff4MtsmEntry *mtsm;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || model == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	if (model->mtdtHead != NULL) {
		mtdt = (Ff4MtdtPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*mtdt));
		if (mtdt == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_mtdt(sess, opts, model->mtdtHead, mtdt);
			if (ret == FF4_OK) {
				outPlan->mtdtPlan = mtdt;
				mtdt = NULL;
			}
		}
	}

	if (ret == FF4_OK && model->mtdfHead != NULL) {
		mtdf = (Ff4MtdfPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*mtdf));
		if (mtdf == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_mtdf(sess, opts, model->mtdfHead, mtdf);
			if (ret == FF4_OK) {
				outPlan->mtdfPlan = mtdf;
				mtdf = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		for (mtsm = model->mtsmHead; mtsm != NULL; mtsm = mtsm->next) {
			mtsmPlan = (Ff4MtsmPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								 sizeof(*mtsmPlan));
			if (mtsmPlan == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_mtsm(sess, opts, mtsm, mtsmPlan);
			if (ret != FF4_OK)
				break;
			if (outPlan->mtsmHead != NULL)
				outPlan->mtsmTail->next = mtsmPlan;
			else
				outPlan->mtsmHead = mtsmPlan;
			outPlan->mtsmTail = mtsmPlan;
			mtsmPlan = NULL;
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_usmt_payload(sess, opts, NULL, 0, outPlan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	}

	ff4_pool_free(sess, mtdt);
	ff4_pool_free(sess, mtdf);
	ff4_free_mtsm_plan_list(sess, mtsmPlan);
	return ret;
}

int32_t ff4_write_usmt_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4UuidPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_UUID,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_usmt_payload(sess, opts, fileCtx, pos + total, plan,
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
