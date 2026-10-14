/*
 * The AVC parser's errors and its parameter set caches (0x862c0c-0x862f28).
 */
#include "service.h"

#define SET_SIZE		1088
#define SET2_SIZE		16
#define BUFFER_SIZE		256
#define SLICE_SIZE		896

/* a fatal error closes what the stage has open, then goes back to the job's jump */
void videoParserError(const VideoStream *stream, VideoParser *parser, uint32_t stage,
	int32_t code)
{
	VideoSliceJob *job = (VideoSliceJob *)parser;	/* the parser opens a job's state */
	VideoNal *nal = parser->nal;
	VideoParameterCache *cache = (VideoParameterCache *)stream->parameterCache;

	if (code < VIDEO_ERROR_FATAL) {
		nal->error = code;
		return;
	}
	switch (stage) {
	case VIDEO_STAGE_ADD_SET:
		vnzPublicClosePrivate(job->parameterSet, SET_SIZE);
		videoCacheDrop(cache, nal->parameterSet);
		break;
	case VIDEO_STAGE_BUFFER:
		vnzPublicClosePrivate(job->buffer, BUFFER_SIZE);
		break;
	case VIDEO_STAGE_ADD_SET2:
		vnzPublicCloseProtected(job->parameterSet, SET_SIZE);
		vnzPublicClosePrivate(job->parameterSet2, SET2_SIZE);
		videoCacheDrop2(cache, nal->parameterSet2);
		break;
	case VIDEO_STAGE_SETS:
	case VIDEO_STAGE_SETS2:
		vnzPublicCloseProtected(job->parameterSet, SET_SIZE);
		/* fall through */
	case VIDEO_STAGE_SET2:
		vnzPublicCloseProtected(job->parameterSet2, SET2_SIZE);
		break;
	case VIDEO_STAGE_SLICE:
		vnzPublicClosePrivate(job->slice, SLICE_SIZE);
		break;
	}
	videoLongJump(job->jump, code);
}

/* the id's node, moved to the front; -1 when it is not there */
static inline int32_t cacheFind(VideoCacheList *list, VideoCacheNode *nodes, uint32_t id)
{
	int32_t index, previous, i;

	if (!list->capacity)
		return id;
	index = list->head;
	previous = index;
	for (i = 0; i < list->count; i++) {
		if (nodes[index].id == id) {
			nodes[previous].next = nodes[index].next;
			if (index != list->head) {
				nodes[index].next = list->head;
				list->head = index;
			}
			return index;
		}
		previous = index;
		index = (uint8_t)nodes[index].next;
	}
	return -1;
}

/* a node for the id at the front: its own, a free one, or the last used (+256) */
static inline int32_t cacheAdd(VideoCacheList *list, VideoCacheNode *nodes, uint32_t id)
{
	int32_t index, previous, slot, head, i;

	if (!list->capacity)
		return id;
	head = list->head;
	index = head;
	previous = index;
	for (i = 0; i < list->count; i++) {
		if (nodes[index].id == id) {
			nodes[previous].next = nodes[index].next;
			slot = index;
			goto found;
		}
		previous = index;
		index = (uint8_t)nodes[index].next;
	}
	if (list->count != list->capacity) {
		slot = list->count;
		nodes[slot].id = id;
	} else {
		slot = previous + VIDEO_CACHE_EVICTED;
		nodes[previous].id = id;
	}
found:
	if (slot >= list->capacity) {
		index = slot - VIDEO_CACHE_EVICTED;
	} else {
		if (slot == list->count)
			list->count++;
		if (slot == head)
			return slot;
		index = slot;
	}
	nodes[index].next = head;
	list->head = index;
	return slot;
}

/* the front node goes */
static inline void cacheDrop(VideoCacheList *list, VideoCacheNode *nodes, uint32_t index)
{
	if (!list->capacity)
		return;
	if (list->count < 2) {
		list->head = 0;
		list->count = 0;
		return;
	}
	list->head = nodes[index].next;
	list->count--;
}

int32_t videoCacheFind(VideoParameterCache *cache, uint32_t id)
{
	return cacheFind(&cache->lists[0], cache->nodes, id);
}

int32_t videoCacheAdd(VideoParameterCache *cache, uint32_t id)
{
	return cacheAdd(&cache->lists[0], cache->nodes, id);
}

void videoCacheDrop(VideoParameterCache *cache, uint32_t index)
{
	cacheDrop(&cache->lists[0], cache->nodes, index);
}

int32_t videoCacheFind2(VideoParameterCache *cache, uint32_t id)
{
	return cacheFind(&cache->lists[1], &cache->nodes[cache->lists[0].capacity], id);
}

int32_t videoCacheAdd2(VideoParameterCache *cache, uint32_t id)
{
	return cacheAdd(&cache->lists[1], &cache->nodes[cache->lists[0].capacity], id);
}

void videoCacheDrop2(VideoParameterCache *cache, uint32_t index)
{
	cacheDrop(&cache->lists[1], &cache->nodes[cache->lists[0].capacity], index);
}
