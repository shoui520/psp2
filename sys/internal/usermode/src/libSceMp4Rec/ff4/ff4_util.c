/*
 * Sony FF4 MP4 library: tracked pool allocator, buffered stream reader,
 * generic singly-linked list helpers, and ZPCK table parser.
 */
#include "ff4_internal.h"

int32_t ff4_buf_init(Ff4SessionImpl *sess, uint32_t poolType, uint64_t size, Ff4Buffer *outBuf)
{
	uint8_t *mem;

	if (sess == NULL || outBuf == NULL || size == 0u)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, outBuf, 0, sizeof(Ff4Buffer));
	outBuf->size = size;
	mem = (uint8_t *)ff4_pool_alloc(sess, poolType, size);
	outBuf->data = mem;
	if (mem == NULL)
		return FF4_ERR_NO_MEM;
	return FF4_OK;
}

int32_t ff4_buf_free(Ff4SessionImpl *sess, Ff4Buffer *buf)
{
	if (sess == NULL || buf == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_pool_free(sess, buf->data);
	buf->data = NULL;
	return FF4_OK;
}

int32_t ff4_stream_reset(Ff4SessionImpl *sess, Ff4BufStream *stream)
{
	if (sess == NULL || stream == NULL)
		return FF4_ERR_INVALID_ARG;
	if (stream->bufCap == 0u)
		return FF4_ERR_INVALID_STATE;

	stream->bufValid = 0;
	stream->bufCur = stream->bufBase;
	return ff4_io_seek(sess, stream->fileCtx, (int64_t)stream->filePos, 0);
}

int32_t ff4_stream_seek(Ff4SessionImpl *sess, Ff4BufStream *stream, int64_t offset, uint32_t whence)
{
	Ff4BufStream local;
	int64_t target;
	int32_t ret;

	if (sess == NULL || stream == NULL || whence >= 2u) {
		ff4_stream_reset(sess, stream);
		return FF4_ERR_INVALID_ARG;
	}
	if (stream->bufCap == 0u) {
		ff4_stream_reset(sess, stream);
		return FF4_ERR_NO_MEM;
	}

	ff4_memcpy(sess, &local, sizeof(local), stream, sizeof(local));
	if (whence == 1u) {
		target = (int64_t)local.filePos + (int64_t)(local.bufCur - local.bufBase) + offset;
	} else {
		target = offset;
	}

	if (whence == 1u && target >= (int64_t)local.filePos &&
	    target < (int64_t)(local.filePos + local.bufValid)) {
		local.bufCur += offset;
		ff4_memcpy(sess, stream, sizeof(local), &local, sizeof(local));
		return FF4_OK;
	}

	local.filePos = (uint64_t)target;
	local.bufValid = 0;
	local.bufCur = local.bufBase;
	ret = ff4_io_seek(sess, local.fileCtx, target, 0);
	if (ret != FF4_OK) {
		ff4_stream_reset(sess, stream);
		return ret;
	}

	ff4_memcpy(sess, stream, sizeof(local), &local, sizeof(local));
	return FF4_OK;
}

int32_t ff4_stream_open(Ff4SessionImpl *sess, void *fileCtx, uint64_t startPos,
			uint8_t *bufBase, uint64_t bufCap, Ff4BufStream *outStream)
{
	Ff4BufStream local;
	int32_t ret;

	if (sess == NULL || fileCtx == NULL || bufBase == NULL || outStream == NULL)
		return FF4_ERR_INVALID_ARG;
	if (bufCap == 0u)
		return FF4_ERR_NO_MEM;

	local.fileCtx = fileCtx;
	local.pad = 0;
	local.filePos = startPos;
	local.bufCap = bufCap;
	local.bufValid = 0;
	local.bufBase = bufBase;
	local.bufCur = bufBase;

	ret = ff4_stream_seek(sess, &local, (int64_t)startPos, 0);
	if (ret == FF4_OK)
		ff4_memcpy(sess, outStream, sizeof(local), &local, sizeof(local));
	return ret;
}

int32_t ff4_stream_read(Ff4SessionImpl *sess, Ff4BufStream *stream, uint8_t *dst,
			uint64_t size, uint64_t *outRead)
{
	Ff4BufStream local;
	uint64_t totalRead = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || stream == NULL || dst == NULL || outRead == NULL) {
		ff4_stream_reset(sess, stream);
		return FF4_ERR_INVALID_ARG;
	}
	if (stream->bufCap == 0u) {
		ff4_stream_reset(sess, stream);
		return FF4_ERR_NO_MEM;
	}

	ff4_memcpy(sess, &local, sizeof(local), stream, sizeof(local));

	while (totalRead < size) {
		uint64_t consumed;
		uint64_t remain;
		uint64_t chunk;

		if (local.bufCur < local.bufBase) {
			ff4_stream_reset(sess, stream);
			return FF4_ERR_INTERNAL;
		}
		consumed = (uint64_t)(uintptr_t)(local.bufCur - local.bufBase);
		if (local.bufValid <= consumed) {
			uint64_t got = 0;
			local.filePos += local.bufValid;
			local.bufCur = local.bufBase;
			local.bufValid = 0;
			ret = ff4_io_read(sess, local.fileCtx, local.bufBase, local.bufCap, &got);
			if (ret != FF4_OK || got == 0u) {
				ff4_stream_reset(sess, stream);
				return (ret != FF4_OK) ? ret : FF4_ERR_IO;
			}
			local.bufValid = got;
			consumed = 0;
		}

		remain = size - totalRead;
		chunk = local.bufValid - consumed;
		if (chunk > remain)
			chunk = remain;

		ff4_memcpy(sess, dst, chunk, local.bufCur, chunk);
		dst += chunk;
		local.bufCur += chunk;
		totalRead += chunk;
	}

	*outRead = totalRead;
	ff4_memcpy(sess, stream, sizeof(local), &local, sizeof(local));
	return FF4_OK;
}

int32_t ff4_stream_read_be16(Ff4SessionImpl *sess, Ff4BufStream *stream, uint16_t *outVal)
{
	uint16_t raw = 0;
	uint64_t got = 0;

	if (outVal == NULL)
		return FF4_ERR_INTERNAL;
	if (ff4_stream_read(sess, stream, (uint8_t *)&raw, 2u, &got) != FF4_OK || got != 2u)
		return FF4_ERR_IO;
	*outVal = ff4_bswap16(raw);
	return FF4_OK;
}

int32_t ff4_stream_read_be32(Ff4SessionImpl *sess, Ff4BufStream *stream, uint32_t *outVal)
{
	uint32_t raw = 0;
	uint64_t got = 0;

	if (outVal == NULL)
		return FF4_ERR_INTERNAL;
	if (ff4_stream_read(sess, stream, (uint8_t *)&raw, 4u, &got) != FF4_OK || got != 4u)
		return FF4_ERR_IO;
	*outVal = ff4_bswap32(raw);
	return FF4_OK;
}

int32_t ff4_stream_read_be64(Ff4SessionImpl *sess, Ff4BufStream *stream, uint64_t *outVal)
{
	uint32_t raw[2] = { 0, 0 };
	uint64_t got = 0;

	if (outVal == NULL)
		return FF4_ERR_INTERNAL;
	if (ff4_stream_read(sess, stream, (uint8_t *)raw, 8u, &got) != FF4_OK || got != 8u)
		return FF4_ERR_IO;
	*outVal = ((uint64_t)ff4_bswap32(raw[0]) << 32) | (uint64_t)ff4_bswap32(raw[1]);
	return FF4_OK;
}

int32_t ff4_free_meta_item_list(Ff4SessionImpl *sess, Ff4MetaItemNode *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4MetaItemNode *next = head->next;
		ff4_pool_free(sess, head->data);
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_pair_list(Ff4SessionImpl *sess, Ff4NodePair *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodePair *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_zpck_table(Ff4SessionImpl *sess, Ff4ZpckTable *table)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (table != NULL) {
		ff4_free_pair_list(sess, table->head);
		ff4_pool_free(sess, table);
	}
	return FF4_OK;
}

int32_t ff4_free_triple_list(Ff4SessionImpl *sess, Ff4NodeTriple *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeTriple *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_u32_list(Ff4SessionImpl *sess, Ff4NodeU32 *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeU32 *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_group_list(Ff4SessionImpl *sess, Ff4NodeGroup *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeGroup *next = head->next;
		ff4_free_u32_list(sess, head->children);
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_edit_entry_list(Ff4SessionImpl *sess, Ff4EditEntryNode *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4EditEntryNode *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_dref_list(Ff4SessionImpl *sess, Ff4DrefEntry *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4DrefEntry *next = head->next;
		if (head->type == FF4_BOX_URN) {
			ff4_pool_free(sess, head->location);
			ff4_pool_free(sess, head->name);
		} else if (head->type == FF4_BOX_URL) {
			ff4_pool_free(sess, head->location);
		}
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_buf_list(Ff4SessionImpl *sess, Ff4NodeBuf *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeBuf *next = head->next;
		ff4_pool_free(sess, head->data);
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_buf_node(Ff4SessionImpl *sess, Ff4NodeBuf *node)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (node != NULL) {
		ff4_pool_free(sess, node->data);
		ff4_pool_free(sess, node);
	}
	return FF4_OK;
}

int32_t ff4_free_single(Ff4SessionImpl *sess, void *ptr)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (ptr != NULL)
		ff4_pool_free(sess, ptr);
	return FF4_OK;
}

int32_t ff4_free_brand_list(Ff4SessionImpl *sess, Ff4NodeU32 *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeU32 *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_id_list(Ff4SessionImpl *sess, Ff4NodeU32 *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4NodeU32 *next = head->next;
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_tref_list(Ff4SessionImpl *sess, Ff4TrefEntry *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4TrefEntry *next = head->next;
		ff4_free_u32_list(sess, head->trackIds);
		ff4_free_u32_list(sess, head->extraIds);
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_free_meta_list(Ff4SessionImpl *sess, Ff4MetaItemNode *head)
{
	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		Ff4MetaItemNode *next = head->next;
		ff4_pool_free(sess, head->data);
		ff4_pool_free(sess, head);
		head = next;
	}
	return FF4_OK;
}

int32_t ff4_count_meta_item_list(const Ff4MetaItemNode *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_pair_list(const Ff4NodePair *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_triple_list(const Ff4NodeTriple *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_u32_list(const Ff4NodeU32 *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_edit_entry_list(const Ff4EditEntryNode *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_dref_list(const Ff4DrefEntry *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_count_buf_list(const Ff4NodeBuf *head, uint32_t *outCount)
{
	uint32_t count = 0;

	if (outCount == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		count++;
		head = head->next;
	}
	*outCount = count;
	return FF4_OK;
}

int32_t ff4_get_triple_at(Ff4NodeTriple *head, uint32_t index1, Ff4NodeTriple **outNode)
{
	uint32_t cur = 0;

	if (index1 == 0u || outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		if (++cur == index1)
			break;
		head = head->next;
	}
	*outNode = head;
	return FF4_OK;
}

int32_t ff4_get_u32_at(Ff4NodeU32 *head, uint32_t index1, Ff4NodeU32 **outNode)
{
	uint32_t cur = 0;

	if (index1 == 0u || outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		if (++cur == index1)
			break;
		head = head->next;
	}
	*outNode = head;
	return FF4_OK;
}

int32_t ff4_get_dref_at(Ff4DrefEntry *head, uint32_t index1, Ff4DrefEntry **outNode)
{
	uint32_t cur = 0;

	if (index1 == 0u || outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		if (++cur == index1)
			break;
		head = head->next;
	}
	*outNode = head;
	return FF4_OK;
}

int32_t ff4_find_meta_item_by_tag(Ff4MetaItemNode *head, uint8_t tag,
			       Ff4MetaItemNode **outNode, uint32_t *outIndex1)
{
	uint32_t idx = 0;

	if (outNode == NULL)
		return FF4_ERR_INVALID_ARG;
	while (head != NULL) {
		idx++;
		if (head->tag == tag)
			break;
		head = head->next;
	}
	*outNode = head;
	if (outIndex1 != NULL)
		*outIndex1 = idx;
	return FF4_OK;
}

void *ff4_pool_alloc(Ff4SessionImpl *sess, uint32_t poolType, uint64_t size)
{
	uint64_t total;
	uint8_t *raw;
	uint8_t hdr[8];

	if (sess == NULL || (poolType != FF4_POOL_META && poolType != FF4_POOL_CACHE))
		return NULL;

	total = size + 8u;
	if ((total >> 32) != 0u)
		return NULL;

	if (sess->limitEnabled != 0u) {
		if (poolType == FF4_POOL_META) {
			if (sess->pool1Limit < sess->pool1Used + total)
				return NULL;
		} else {
			if (sess->pool2Limit < sess->pool2Used + total)
				return NULL;
		}
	}

	raw = (uint8_t *)ff4_calloc(sess, total);
	if (raw == NULL)
		return NULL;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	ff4_memcpy(sess, hdr, 3u, "FF4", 3u);
	hdr[3] = (uint8_t)poolType;
	ff4_put_be32(hdr + 4, (uint32_t)total);
	ff4_memcpy(sess, raw, sizeof(hdr), hdr, sizeof(hdr));

	if (sess->limitEnabled != 0u) {
		if (poolType == FF4_POOL_META)
			sess->pool1Used += total;
		else
			sess->pool2Used += total;
	}
	return raw + 8;
}

int32_t ff4_pool_parse_header(Ff4SessionImpl *sess, const void *ptr, Ff4AllocHeader *outHdr)
{
	Ff4AllocHeader hdr;
	const uint8_t *base;

	if (sess == NULL || outHdr == NULL)
		return FF4_ERR_INVALID_ARG;
	if ((uintptr_t)ptr < 8u)
		return FF4_ERR_INVALID_ARG;

	base = (const uint8_t *)ptr - 8;
	ff4_memset(sess, &hdr, 0, sizeof(hdr));
	ff4_memcpy(sess, hdr.magic, 3u, base, 3u);
	hdr.poolType = base[3];
	hdr.allocSize = ff4_get_be32(base + 4);

	if (ff4_memcmp(sess, hdr.magic, "FF4", 3u) != 0)
		return FF4_ERR_INTERNAL;
	if (hdr.poolType != FF4_POOL_META && hdr.poolType != FF4_POOL_CACHE)
		return FF4_ERR_INTERNAL;
	if (hdr.allocSize < 8u)
		return FF4_ERR_INTERNAL;

	ff4_memcpy(sess, outHdr, sizeof(hdr), &hdr, sizeof(hdr));
	return FF4_OK;
}

void ff4_pool_free(Ff4SessionImpl *sess, void *ptr)
{
	Ff4AllocHeader hdr;
	uint8_t *base;

	if (sess == NULL || ptr == NULL)
		return;
	if (ff4_pool_parse_header(sess, ptr, &hdr) != FF4_OK || (uintptr_t)ptr < 8u)
		return;

	base = (uint8_t *)ptr - 8;
	ff4_memset(sess, base, 0, 8u);
	ff4_free(sess, base);

	if (sess->limitEnabled != 0u) {
		if (hdr.poolType == FF4_POOL_META)
			sess->pool1Used -= hdr.allocSize;
		else
			sess->pool2Used -= hdr.allocSize;
	}
}

int32_t ff4_pool_init(Ff4SessionImpl *sess, uint32_t useDefaultCache, uint64_t totalMem,
		      uint32_t cachePercent, uint64_t minMetaMem)
{
	uint64_t pool1 = 0;
	uint64_t pool2 = 0;
	uint32_t enabled = 0;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	if (totalMem != 0u) {
		uint64_t prodHi = 0;
		uint64_t prodLo = 0;
		uint64_t rem = 0;
		int32_t ret;

		if (cachePercent > 100u)
			return FF4_ERR_INVALID_ARG;

		enabled = 1u;
		ret = ff4_u64_mul128(totalMem, (uint64_t)cachePercent, &prodHi, &prodLo);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_u128_div64(prodHi, prodLo, 100u, &prodHi, &prodLo, &rem);
		if (ret != FF4_OK)
			return ret;

		pool2 = prodLo;
		if (pool2 > totalMem)
			return FF4_ERR_INTERNAL;
		pool1 = totalMem - pool2;
		if (minMetaMem > pool1)
			return FF4_ERR_INVALID_ARG;
	} else if (useDefaultCache != 0u) {
		pool2 = 0x20000u;
	}

	sess->limitEnabled = enabled;
	sess->pad = 0;
	sess->pool1Limit = pool1;
	sess->pool2Limit = pool2;
	sess->pool1Used = 0;
	sess->pool2Used = 0;
	return FF4_OK;
}

int32_t ff4_parse_zpck_header(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			      uint64_t size, Ff4ZpckHeader *outHdr)
{
	Ff4ZpckHeader hdr;
	const uint8_t *p;

	if (sess == NULL || buf == NULL || outHdr == NULL)
		return FF4_ERR_INVALID_ARG;
	if (size < 8u)
		return FF4_ERR_FORMAT;

	p = buf + (uintptr_t)offset;
	ff4_memset(sess, &hdr, 0, sizeof(hdr));
	hdr.fourcc = ff4_get_be32(p);
	hdr.version = p[4];
	hdr.flags = ff4_get_be16(p + 6);
	ff4_memcpy(sess, outHdr, sizeof(hdr), &hdr, sizeof(hdr));
	return FF4_OK;
}

int32_t ff4_parse_zpck_entries(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			       uint64_t size, uint32_t entryCount, Ff4ZpckTable *table)
{
	const uint8_t *cur;
	const uint8_t *end;
	uint32_t i;

	if (sess == NULL || buf == NULL || table == NULL)
		return FF4_ERR_INVALID_ARG;

	cur = buf + (uintptr_t)offset;
	end = cur + (uintptr_t)size;
	for (i = 0; i < entryCount; i++) {
		Ff4NodePair *node;

		if ((uintptr_t)(end - cur) < 12u)
			break;
		node = (Ff4NodePair *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(Ff4NodePair));
		if (node == NULL)
			return FF4_ERR_NO_MEM;
		node->a = ff4_get_be32(cur);
		node->b = ff4_get_be32(cur + 4);
		node->next = NULL;
		cur += 12;

		if (table->head != NULL)
			table->tail->next = node;
		else
			table->head = node;
		table->tail = node;
	}
	return FF4_OK;
}

int32_t ff4_parse_zpck_table_hdr(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
				 uint64_t size, Ff4ZpckTableHdr *outHdr)
{
	Ff4ZpckTableHdr hdr;
	const uint8_t *p;

	if (sess == NULL || buf == NULL || outHdr == NULL)
		return FF4_ERR_INVALID_ARG;
	if (size < 12u)
		return FF4_ERR_FORMAT;

	p = buf + (uintptr_t)offset;
	ff4_memset(sess, &hdr, 0, sizeof(hdr));
	hdr.version = ff4_get_be32(p);
	hdr.entryCount = ff4_get_be32(p + 8);
	hdr.dataOffset = offset + 12u;
	hdr.dataSize = size - 12u;
	ff4_memcpy(sess, outHdr, sizeof(hdr), &hdr, sizeof(hdr));
	return FF4_OK;
}

int32_t ff4_parse_zpck_table(Ff4SessionImpl *sess, const uint8_t *buf, uint64_t offset,
			     uint64_t size, Ff4ZpckTable *outTable)
{
	Ff4ZpckTableHdr hdr;
	int32_t ret;

	if (sess == NULL || buf == NULL || outTable == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_parse_zpck_table_hdr(sess, buf, offset, size, &hdr);
	if (ret != FF4_OK)
		return ret;

	outTable->version = hdr.version;
	return ff4_parse_zpck_entries(sess, buf, hdr.dataOffset, hdr.dataSize, hdr.entryCount, outTable);
}
