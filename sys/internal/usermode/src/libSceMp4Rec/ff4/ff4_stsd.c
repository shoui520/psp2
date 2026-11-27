/*
 * Sony FF4 MP4 muxer: Sample Description entries (stsd) — mp4v, avc1/avcC,
 * mp4a, mp4s, esds, sinf, STCS/3STG/3STP, LBIN, 3DSS UUID, and book/plain.
 */
#include "ff4_internal.h"

#define FF4_BOX_UUID 0x75756964u
#define FF4_BOX_SINF 0x73696E66u
#define FF4_BOX_ESDS 0x65736473u
#define FF4_BOX_AVCC 0x61766343u
#define FF4_BOX_MP4V 0x6D703476u
#define FF4_BOX_AVC1 0x61766331u
#define FF4_BOX_MP4A 0x6D703461u
#define FF4_BOX_MP4S 0x6D703473u
#define FF4_BOX_LBIN 0x4C42494Eu
#define FF4_BOX_STCS 0x53544353u
#define FF4_BOX_3STG 0x33535447u
#define FF4_BOX_3STP 0x33535450u
#define FF4_UUID_3DSS_0 0x33445353u
#define FF4_UUID_SONY_1 0x21D24FCEu
#define FF4_UUID_SONY_2 0xBB88695Cu
#define FF4_UUID_SONY_3 0xFAC9C740u

typedef struct Ff4PlainEntryData {
	uint32_t fourcc;
	uint32_t dataRefIndex;
} Ff4PlainEntryData; /* 8 bytes */

typedef struct Ff4TextEntryData {
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t version;
	uint16_t width;
	uint16_t height;
	uint16_t padA;
	uint32_t horizRes;
	uint32_t vertRes;
	uint16_t originX;
	uint16_t originY;
	uint8_t colorDepth;
	uint8_t padB[3];
} Ff4TextEntryData; /* 32 bytes */

typedef struct Ff4Mp4aEntryData {
	uint32_t mask;
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t channelCount;
	uint16_t sampleSize;
	uint32_t sampleRate;
} Ff4Mp4aEntryData; /* 20 bytes */

typedef struct Ff4VideoEntryData {
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t width;
	uint16_t height;
	uint32_t horizRes;
	uint32_t vertRes;
	uint16_t frameCount;
	uint8_t compressorName[32];
	uint16_t depth;
} Ff4VideoEntryData; /* 56 bytes */

typedef struct Ff4TextSubBoxData {
	uint8_t mode;
	uint8_t flags;
	uint16_t val1;
	uint16_t val2;
	uint16_t val3;
} Ff4TextSubBoxData; /* 8 bytes */

typedef struct Ff4EsdsDecConfigData {
	uint8_t objTypeInd;
	uint8_t streamType;
	uint8_t upStream;
	uint8_t pad;
	uint32_t bufferSizeDb;
	uint32_t maxBitrate;
	uint32_t avgBitrate;
	Ff4BoxDataBuf decSpecific;
} Ff4EsdsDecConfigData; /* 32 bytes */

typedef struct Ff4EsdsModelFull {
	uint32_t kind;
	uint32_t pad;
	union {
		Ff4EsdsDecConfigData structured;
		Ff4BoxDataBuf raw;
	} u;
} Ff4EsdsModelFull; /* 40 bytes */

typedef struct Ff4DecSpecificPlan {
	uint64_t boxSize;
	Ff4BoxDataBuf data;
} Ff4DecSpecificPlan; /* 24 bytes */

typedef struct Ff4DecConfigPlan {
	uint64_t boxSize;
	uint8_t objTypeInd;
	uint8_t streamType;
	uint8_t upStream;
	uint8_t padA;
	uint32_t bufferSizeDb;
	uint32_t maxBitrate;
	uint32_t avgBitrate;
	Ff4DecSpecificPlan *decSpecific;
	uint32_t padB;
} Ff4DecConfigPlan; /* 32 bytes */

typedef struct Ff4EsDescrPlan {
	uint64_t boxSize;
	uint16_t esId;
	uint8_t flags;
	uint8_t pad[5];
	Ff4DecConfigPlan decConfig;
	Ff4DecSpecificPlan slConfig;
} Ff4EsDescrPlan; /* 72 bytes */

typedef struct Ff4EsdsPlanFull {
	uint64_t boxSize;
	uint32_t kind;
	uint32_t pad;
	union {
		Ff4EsDescrPlan structured;
		Ff4BoxDataBuf raw;
	} u;
} Ff4EsdsPlanFull; /* 88 bytes */

typedef struct Ff4AvccPlan {
	uint64_t boxSize;
	Ff4AvccModel model;
} Ff4AvccPlan; /* 32 bytes */

typedef struct Ff4TextSubBoxPlan {
	uint64_t boxSize;
	Ff4TextSubBoxData data;
} Ff4TextSubBoxPlan; /* 16 bytes */

typedef struct Ff4PlainEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4PlainEntryData data;
} Ff4PlainEntryPlan;

typedef struct Ff4LcomEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4PlainEntryData data;
	Ff4StsdExtraPlan *extraHead;
	Ff4StsdExtraPlan *extraTail;
} Ff4LcomEntryPlan;

typedef struct Ff4Mp4sEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4PlainEntryData data;
	Ff4EsdsPlanFull *esds;
} Ff4Mp4sEntryPlan;

typedef struct Ff4TextEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4TextEntryData data;
	Ff4TextSubBoxPlan *subBox;
} Ff4TextEntryPlan;

typedef struct Ff4Mp4aEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4Mp4aEntryData data;
	Ff4EsdsPlanFull *esds;
	Ff4StsdExtraPlan *extraHead;
	Ff4StsdExtraPlan *extraTail;
} Ff4Mp4aEntryPlan;

typedef struct Ff4VideoEntryPlan {
	uint64_t boxSize;
	uint32_t fourcc;
	Ff4VideoEntryData data;
	void *codecPlan;
	Ff4DecSpecificPlan *uuidPlan;
	Ff4StsdExtraPlan *extraHead;
	Ff4StsdExtraPlan *extraTail;
} Ff4VideoEntryPlan;

typedef struct Ff4StsdEntryFull {
	uint32_t kind;
	union {
		struct {
			Ff4VideoEntryData data;
			void *codecCfg;
			Ff4BoxDataBuf *uuid3dss;
			Ff4BoxChunkNode *extraHead;
			Ff4BoxChunkNode *extraTail;
		} video;
		struct {
			Ff4Mp4aEntryData data;
			Ff4EsdsModelFull *esds;
			Ff4BoxChunkNode *extraHead;
			Ff4BoxChunkNode *extraTail;
		} audio;
		struct {
			Ff4PlainEntryData data;
			Ff4EsdsModelFull *esds;
		} mp4s;
		struct {
			Ff4PlainEntryData data;
			Ff4BoxChunkNode *extraHead;
			Ff4BoxChunkNode *extraTail;
		} lcom;
		struct {
			Ff4PlainEntryData data;
		} plain;
		struct {
			Ff4TextEntryData data;
			Ff4TextSubBoxData *subBox;
		} text;
	} u;
	struct Ff4StsdEntryFull *next;
} Ff4StsdEntryFull; /* 80 bytes */

typedef struct Ff4StsdPlanFull {
	uint32_t kind;
	uint32_t padA;
	union {
		Ff4VideoEntryPlan video;
		Ff4Mp4aEntryPlan audio;
		Ff4Mp4sEntryPlan mp4s;
		Ff4LcomEntryPlan lcom;
		Ff4PlainEntryPlan plain;
		Ff4TextEntryPlan text;
	} u;
	struct Ff4StsdPlanFull *next;
	uint32_t padB;
} Ff4StsdPlanFull; /* 104 bytes */

/* ---- Entry initializers ---- */

int32_t ff4_init_stsd_plain(Ff4SessionImpl *sess, const uint32_t *in, void *outData)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = in[1];
	tmp.dataRefIndex = in[0];
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_init_stsd_lcom(Ff4SessionImpl *sess, const uint32_t *in, void *outData)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = FF4_BOX_LBIN;
	tmp.dataRefIndex = in[0];
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_init_stsd_book(Ff4SessionImpl *sess, const uint32_t *in, void *outData)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = in[1];
	tmp.dataRefIndex = in[0];
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_init_stsd_mp4s(Ff4SessionImpl *sess, const uint32_t *in, void *outData)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = FF4_BOX_MP4S;
	tmp.dataRefIndex = in[0];
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4TextInitInput {
	uint32_t kind;
	uint32_t dataRefIndex;
	uint16_t version;
	uint16_t width;
	uint16_t height;
	uint16_t horizResHi;
	uint16_t horizResLo;
	uint16_t vertResHi;
	uint16_t vertResLo;
	uint16_t originX;
	uint16_t originY;
	uint8_t colorDepth;
} Ff4TextInitInput;

int32_t ff4_init_stsd_text(Ff4SessionImpl *sess, const void *inPtr, void *outData)
{
	const Ff4TextInitInput *in = (const Ff4TextInitInput *)inPtr;
	Ff4TextEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	if (in->kind != 8u && in->kind != 9u)
		return FF4_ERR_INTERNAL;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = (in->kind == 8u) ? FF4_BOX_STCS : FF4_BOX_3STG;
	tmp.dataRefIndex = in->dataRefIndex;
	tmp.version = in->version;
	tmp.width = in->width;
	tmp.height = in->height;
	tmp.horizRes = ((uint32_t)in->horizResHi << 16) | (uint32_t)in->horizResLo;
	tmp.vertRes = ((uint32_t)in->vertResHi << 16) | (uint32_t)in->vertResLo;
	tmp.originX = in->originX;
	tmp.originY = in->originY;
	tmp.colorDepth = in->colorDepth;
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4Mp4aInitInput {
	uint32_t dataRefIndex;
	uint16_t channelCount;
	uint16_t sampleSize;
} Ff4Mp4aInitInput;

int32_t ff4_init_stsd_mp4a(Ff4SessionImpl *sess, const void *inPtr, void *outData)
{
	const Ff4Mp4aInitInput *in = (const Ff4Mp4aInitInput *)inPtr;
	Ff4Mp4aEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.mask = 14u;
	tmp.fourcc = FF4_BOX_MP4A;
	tmp.dataRefIndex = in->dataRefIndex;
	tmp.channelCount = in->channelCount;
	tmp.sampleSize = in->sampleSize;
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4VideoInitInput {
	uint32_t kind;
	uint32_t dataRefIndex;
	uint16_t width;
	uint16_t height;
	uint16_t horizResHi;
	uint16_t horizResLo;
	uint16_t vertResHi;
	uint16_t vertResLo;
} Ff4VideoInitInput;

int32_t ff4_init_stsd_video(Ff4SessionImpl *sess, const void *inPtr, void *outData)
{
	const Ff4VideoInitInput *in = (const Ff4VideoInitInput *)inPtr;
	Ff4VideoEntryData tmp;

	if (sess == NULL || in == NULL || outData == NULL)
		return FF4_ERR_INVALID_ARG;
	if (in->kind != 1u && in->kind != 2u)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(sess, &tmp, 0, sizeof(tmp));
	tmp.fourcc = (in->kind == 1u) ? FF4_BOX_MP4V : FF4_BOX_AVC1;
	tmp.dataRefIndex = in->dataRefIndex;
	tmp.width = in->width;
	tmp.height = in->height;
	tmp.horizRes = ((uint32_t)in->horizResHi << 16) | (uint32_t)in->horizResLo;
	tmp.vertRes = ((uint32_t)in->vertResHi << 16) | (uint32_t)in->vertResLo;
	tmp.frameCount = 1u;
	if (in->kind == 2u) {
		tmp.compressorName[0] = 10u;
		ff4_memcpy(sess, &tmp.compressorName[1], 31u, "AVC Coding", 10u);
	}
	tmp.depth = 24u;
	ff4_memcpy(sess, outData, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4StsdAddInput {
	uint32_t kind;
	uint32_t body[7];
} Ff4StsdAddInput;

int32_t ff4_add_stsd_entry(Ff4SessionImpl *sess, Ff4StsdEntry **headTail,
			   const void *inputPtr, uint32_t *outIndex1)
{
	const Ff4StsdAddInput *in = (const Ff4StsdAddInput *)inputPtr;
	Ff4StsdEntryFull *node = NULL;
	uint32_t count = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || headTail == NULL || in == NULL || outIndex1 == NULL)
		return FF4_ERR_INVALID_ARG;

	node = (Ff4StsdEntryFull *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->kind = in->kind;
	switch (in->kind) {
	case 1u:
	case 2u:
		ret = ff4_init_stsd_video(sess, in->body, &node->u.video.data);
		break;
	case 3u:
		ret = ff4_init_stsd_mp4a(sess, in->body, &node->u.audio.data);
		break;
	case 4u:
		ret = ff4_init_stsd_mp4s(sess, in->body, &node->u.mp4s.data);
		break;
	case 5u:
		ret = ff4_init_stsd_book(sess, in->body, &node->u.plain.data);
		break;
	case 6u:
		ret = ff4_init_stsd_lcom(sess, in->body, &node->u.lcom.data);
		break;
	case 7u:
		ret = ff4_init_stsd_plain(sess, in->body, &node->u.plain.data);
		break;
	case 8u:
	case 9u:
		ret = ff4_init_stsd_text(sess, in->body, &node->u.text.data);
		break;
	default:
		ret = FF4_ERR_INTERNAL;
		break;
	}

	if (ret == FF4_OK) {
		Ff4StsdEntry *entry = (Ff4StsdEntry *)node;
		if (headTail[0] != NULL)
			headTail[1]->next = entry;
		else
			headTail[0] = entry;
		headTail[1] = entry;
		node = NULL;
		ret = ff4_count_stsd_entries(headTail[0], &count);
		if (ret == FF4_OK)
			*outIndex1 = count;
	}

	ff4_free_stsd_entry_list(sess, (Ff4StsdEntry *)node);
	return ret;
}

/* ---- Parameter setters ---- */

int32_t ff4_set_stsd_plain_params(Ff4SessionImpl *sess, void *dst, uint8_t mask,
				  const uint32_t *params)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || dst == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = params[0];
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = params[1];
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_stsd_lcom_params(Ff4SessionImpl *sess, void *dst, uint8_t mask,
				 const uint32_t *params)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || dst == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = params[0];
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = params[1];
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_stsd_book_params(Ff4SessionImpl *sess, void *dst, uint8_t mask,
				 const uint32_t *params)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || dst == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = params[0];
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = params[1];
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

int32_t ff4_set_stsd_mp4s_params(Ff4SessionImpl *sess, void *dst, uint8_t mask,
				 const uint32_t *params)
{
	Ff4PlainEntryData tmp;

	if (sess == NULL || dst == NULL || params == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = params[0];
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = params[1];
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4TextParamInput {
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t version;
	uint16_t width;
	uint16_t height;
	uint16_t horizResHi;
	uint16_t horizResLo;
	uint16_t vertResHi;
	uint16_t vertResLo;
	uint16_t originX;
	uint16_t originY;
	uint8_t colorDepth;
} Ff4TextParamInput;

int32_t ff4_set_stsd_text_params(Ff4SessionImpl *sess, void *dst, uint16_t mask,
				 const void *paramsPtr)
{
	const Ff4TextParamInput *in = (const Ff4TextParamInput *)paramsPtr;
	Ff4TextEntryData tmp;

	if (sess == NULL || dst == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = in->fourcc;
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = in->dataRefIndex;
	if ((mask & 4u) != 0)
		tmp.version = in->version;
	if ((mask & 8u) != 0)
		tmp.width = in->width;
	if ((mask & 0x10u) != 0)
		tmp.height = in->height;
	if ((mask & 0x20u) != 0)
		tmp.horizRes = (tmp.horizRes & 0xFFFFu) | ((uint32_t)in->horizResHi << 16);
	if ((mask & 0x40u) != 0)
		tmp.horizRes = (tmp.horizRes & 0xFFFF0000u) | (uint32_t)in->horizResLo;
	if ((mask & 0x80u) != 0)
		tmp.vertRes = (tmp.vertRes & 0xFFFFu) | ((uint32_t)in->vertResHi << 16);
	if ((mask & 0x100u) != 0)
		tmp.vertRes = (tmp.vertRes & 0xFFFF0000u) | (uint32_t)in->vertResLo;
	if ((mask & 0x200u) != 0)
		tmp.originX = in->originX;
	if ((mask & 0x400u) != 0)
		tmp.originY = in->originY;
	if ((mask & 0x800u) != 0)
		tmp.colorDepth = in->colorDepth;
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4Mp4aParamInput {
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t channelCount;
	uint16_t sampleSize;
	uint16_t sampleRateHi;
	uint16_t sampleRateLo;
} Ff4Mp4aParamInput;

int32_t ff4_set_stsd_mp4a_params(Ff4SessionImpl *sess, void *dst, uint32_t mask,
				 const void *paramsPtr)
{
	const Ff4Mp4aParamInput *in = (const Ff4Mp4aParamInput *)paramsPtr;
	Ff4Mp4aEntryData tmp;

	if (sess == NULL || dst == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = in->fourcc;
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = in->dataRefIndex;
	if ((mask & 4u) != 0)
		tmp.channelCount = in->channelCount;
	if ((mask & 8u) != 0)
		tmp.sampleSize = in->sampleSize;
	if ((mask & 0x10u) != 0)
		tmp.sampleRate = (tmp.sampleRate & 0xFFFFu) | ((uint32_t)in->sampleRateHi << 16);
	if ((mask & 0x20u) != 0)
		tmp.sampleRate = (tmp.sampleRate & 0xFFFF0000u) | (uint32_t)in->sampleRateLo;
	tmp.mask |= mask;
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4VideoParamInput {
	uint32_t fourcc;
	uint32_t dataRefIndex;
	uint16_t width;
	uint16_t height;
	uint16_t horizResHi;
	uint16_t horizResLo;
	uint16_t vertResHi;
	uint16_t vertResLo;
	uint16_t frameCount;
	uint8_t compressorName[32];
	uint16_t depth;
} Ff4VideoParamInput;

int32_t ff4_set_stsd_video_params(Ff4SessionImpl *sess, void *dst, uint16_t mask,
				  const void *paramsPtr)
{
	const Ff4VideoParamInput *in = (const Ff4VideoParamInput *)paramsPtr;
	Ff4VideoEntryData tmp;

	if (sess == NULL || dst == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;
	ff4_memcpy(sess, &tmp, sizeof(tmp), dst, sizeof(tmp));
	if ((mask & 1u) != 0)
		tmp.fourcc = in->fourcc;
	if ((mask & 2u) != 0)
		tmp.dataRefIndex = in->dataRefIndex;
	if ((mask & 4u) != 0)
		tmp.width = in->width;
	if ((mask & 8u) != 0)
		tmp.height = in->height;
	if ((mask & 0x10u) != 0)
		tmp.horizRes = (tmp.horizRes & 0xFFFFu) | ((uint32_t)in->horizResHi << 16);
	if ((mask & 0x20u) != 0)
		tmp.horizRes = (tmp.horizRes & 0xFFFF0000u) | (uint32_t)in->horizResLo;
	if ((mask & 0x40u) != 0)
		tmp.vertRes = (tmp.vertRes & 0xFFFFu) | ((uint32_t)in->vertResHi << 16);
	if ((mask & 0x80u) != 0)
		tmp.vertRes = (tmp.vertRes & 0xFFFF0000u) | (uint32_t)in->vertResLo;
	if ((mask & 0x100u) != 0)
		tmp.frameCount = in->frameCount;
	if ((mask & 0x200u) != 0)
		ff4_memcpy(sess, tmp.compressorName, sizeof(tmp.compressorName),
			   in->compressorName, sizeof(tmp.compressorName));
	if ((mask & 0x400u) != 0)
		tmp.depth = in->depth;
	ff4_memcpy(sess, dst, sizeof(tmp), &tmp, sizeof(tmp));
	return FF4_OK;
}

typedef struct Ff4StsdSetParamInput {
	uint32_t kind;
	uint32_t body[14];
} Ff4StsdSetParamInput;

int32_t ff4_set_stsd_entry_params(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, uint16_t mask,
				  const void *paramsPtr)
{
	Ff4StsdEntryFull *entry = (Ff4StsdEntryFull *)entryPtr;
	const Ff4StsdSetParamInput *in = (const Ff4StsdSetParamInput *)paramsPtr;

	if (sess == NULL || entry == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	switch (in->kind) {
	case 1u:
	case 2u:
		return ff4_set_stsd_video_params(sess, &entry->u.video.data, mask, in->body);
	case 3u:
		return ff4_set_stsd_mp4a_params(sess, &entry->u.audio.data, mask, in->body);
	case 4u:
		return ff4_set_stsd_mp4s_params(sess, &entry->u.mp4s.data, (uint8_t)mask, in->body);
	case 5u:
		return ff4_set_stsd_book_params(sess, &entry->u.plain.data, (uint8_t)mask, in->body);
	case 6u:
		return ff4_set_stsd_lcom_params(sess, &entry->u.lcom.data, (uint8_t)mask, in->body);
	case 7u:
		return ff4_set_stsd_plain_params(sess, &entry->u.plain.data, (uint8_t)mask, in->body);
	case 8u:
	case 9u:
		return ff4_set_stsd_text_params(sess, &entry->u.text.data, mask, in->body);
	default:
		return FF4_ERR_INTERNAL;
	}
}

/* ---- Codec config helpers (esds, avcC, 3STP) ---- */

typedef struct Ff4EsdsDecConfigInput {
	uint8_t objTypeInd;
	uint8_t streamType;
	uint8_t upStream;
	uint8_t pad;
	uint32_t bufferSizeDb;
	uint32_t maxBitrate;
	uint32_t avgBitrate;
	const uint8_t *decSpecific;
	uint32_t decSpecificSize;
} Ff4EsdsDecConfigInput;

typedef struct Ff4EsdsSetInput {
	uint32_t kind;
	union {
		Ff4EsdsDecConfigInput structured;
		struct {
			const uint8_t *data;
			uint32_t size;
		} raw;
	} u;
} Ff4EsdsSetInput;

int32_t ff4_copy_esds_dec_config(Ff4SessionImpl *sess, void *dstPtr, const void *srcPtr)
{
	Ff4EsdsDecConfigData *dst = (Ff4EsdsDecConfigData *)dstPtr;
	const Ff4EsdsDecConfigInput *src = (const Ff4EsdsDecConfigInput *)srcPtr;

	if (sess == NULL || dst == NULL || src == NULL)
		return FF4_ERR_INVALID_ARG;
	dst->objTypeInd = src->objTypeInd;
	dst->streamType = src->streamType;
	dst->upStream = src->upStream;
	dst->bufferSizeDb = src->bufferSizeDb;
	dst->maxBitrate = src->maxBitrate;
	dst->avgBitrate = src->avgBitrate;
	return ff4_copy_data_buf(sess, 0, &dst->decSpecific, src->decSpecific, src->decSpecificSize);
}

int32_t ff4_set_stsd_esds(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const void *esdsInputPtr)
{
	Ff4StsdEntryFull *entry = (Ff4StsdEntryFull *)entryPtr;
	const Ff4EsdsSetInput *in = (const Ff4EsdsSetInput *)esdsInputPtr;
	Ff4EsdsModelFull *esds = NULL;
	Ff4EsdsModelFull *prev = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || entry == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;

	if (entry->kind == 1u)
		prev = (Ff4EsdsModelFull *)entry->u.video.codecCfg;
	else if (entry->kind == 3u)
		prev = entry->u.audio.esds;
	else if (entry->kind == 4u)
		prev = entry->u.mp4s.esds;
	else
		return FF4_ERR_INTERNAL;

	if (prev != NULL)
		ff4_free_esds_model(sess, (Ff4EsdsModel *)prev);

	esds = (Ff4EsdsModelFull *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*esds));
	if (esds == NULL)
		return FF4_ERR_NO_MEM;

	esds->kind = in->kind;
	if (in->kind == 0u) {
		ret = ff4_copy_esds_dec_config(sess, &esds->u.structured, &in->u.structured);
	} else if (in->kind == 1u) {
		ret = ff4_copy_data_buf(sess, 0, &esds->u.raw, in->u.raw.data, in->u.raw.size);
	} else {
		ret = FF4_ERR_INTERNAL;
	}

	if (ret == FF4_OK) {
		if (entry->kind == 1u)
			entry->u.video.codecCfg = esds;
		else if (entry->kind == 3u)
			entry->u.audio.esds = esds;
		else if (entry->kind == 4u)
			entry->u.mp4s.esds = esds;
		else
			ret = FF4_ERR_INTERNAL;
		if (ret == FF4_OK)
			esds = NULL;
	}

	ff4_free_esds_model(sess, (Ff4EsdsModel *)esds);
	return ret;
}

int32_t ff4_set_stsd_avcc_hdr(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const uint8_t hdr[5])
{
	Ff4StsdEntryFull *entry = (Ff4StsdEntryFull *)entryPtr;
	Ff4AvccModel *avcc;

	if (sess == NULL || entry == NULL || hdr == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 2u)
		return FF4_ERR_INTERNAL;

	avcc = (Ff4AvccModel *)entry->u.video.codecCfg;
	if (avcc == NULL) {
		avcc = (Ff4AvccModel *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*avcc));
		if (avcc == NULL)
			return FF4_ERR_NO_MEM;
	}
	avcc->configVersion = hdr[0];
	avcc->profileInd = hdr[1];
	avcc->profileCompat = hdr[2];
	avcc->levelInd = hdr[3];
	avcc->lenSizeMinusOne = hdr[4];
	entry->u.video.codecCfg = avcc;
	return FF4_OK;
}

typedef struct Ff4AvccNaluInput {
	uint32_t kind; /* 0 = SPS, 1 = PPS */
	const uint8_t *data;
	uint32_t size;
} Ff4AvccNaluInput;

int32_t ff4_add_avcc_nalu(Ff4SessionImpl *sess, Ff4NodeBuf **headTail, const void *naluInputPtr)
{
	const Ff4AvccNaluInput *in = (const Ff4AvccNaluInput *)naluInputPtr;
	Ff4NodeBuf *node = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || headTail == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;
	if (in->kind >= 2u)
		return FF4_ERR_INTERNAL;

	node = (Ff4NodeBuf *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)&node->data, in->data, in->size);
	if (ret == FF4_OK) {
		node->next = NULL;
		if (headTail[0] != NULL)
			headTail[1]->next = node;
		else
			headTail[0] = node;
		headTail[1] = node;
		node = NULL;
	}

	ff4_free_buf_list(sess, node);
	return ret;
}

int32_t ff4_set_stsd_text_subbox(Ff4SessionImpl *sess, Ff4StsdEntry *entryPtr, const void *subBoxPtr)
{
	Ff4StsdEntryFull *entry = (Ff4StsdEntryFull *)entryPtr;
	const Ff4TextSubBoxData *in = (const Ff4TextSubBoxData *)subBoxPtr;
	Ff4TextSubBoxData *dst;

	if (sess == NULL || entry == NULL || in == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 9u)
		return FF4_ERR_INTERNAL;

	dst = entry->u.text.subBox;
	if (dst == NULL) {
		dst = (Ff4TextSubBoxData *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*dst));
		if (dst == NULL)
			return FF4_ERR_NO_MEM;
	}
	*dst = *in;
	entry->u.text.subBox = dst;
	return FF4_OK;
}

/* ---- Plain / sinf / LBIN / book writers & planners ---- */

int32_t ff4_write_plain_entry_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				      const void *planPtr,
				      uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4PlainEntryPlan *plan = (const Ff4PlainEntryPlan *)planPtr;
	Ff4PlainEntryData data;
	uint8_t buf[12];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
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

int32_t ff4_plan_plain_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			     const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4PlainEntryPlan *plan = (Ff4PlainEntryPlan *)outPlanPtr;
	Ff4PlainEntryData data;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 7u)
		return FF4_ERR_INTERNAL;

	ff4_memcpy(sess, &data, sizeof(data), &entry->u.plain.data, sizeof(data));
	plan->fourcc = data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(data), &data, sizeof(data));
	ret = ff4_write_plain_entry_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_write_sinf_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const Ff4StsdExtraPlan *plan,
			       uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, plan->data, plan->size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_sinf_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const Ff4StsdExtraPlan *plan,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_SINF,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_sinf_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_lcom_entry_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				     void *fileCtx, uint64_t pos, const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4LcomEntryPlan *plan = (const Ff4LcomEntryPlan *)planPtr;
	Ff4PlainEntryData data;
	const Ff4StsdExtraPlan *cur;
	uint8_t buf[8];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->extraHead; cur != NULL; cur = cur->next) {
		ret = ff4_write_sinf_box(sess, opts, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_plan_sinf(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4BoxChunkNode *chunk, Ff4StsdExtraPlan *outPlan)
{
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || chunk == NULL || outPlan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &outPlan->data, sizeof(Ff4BoxDataBuf), chunk, sizeof(Ff4BoxDataBuf));
	ret = ff4_write_sinf_payload(sess, NULL, 0, outPlan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &outPlan->boxSize);
	return ret;
}

int32_t ff4_plan_lcom_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4LcomEntryPlan *plan = (Ff4LcomEntryPlan *)outPlanPtr;
	Ff4StsdExtraPlan *node = NULL;
	const Ff4BoxChunkNode *cur;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 6u)
		return FF4_ERR_INTERNAL;

	plan->fourcc = entry->u.lcom.data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(plan->data), &entry->u.lcom.data, sizeof(plan->data));

	for (cur = entry->u.lcom.extraHead; cur != NULL; cur = cur->next) {
		node = (Ff4StsdExtraPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
		if (node == NULL) {
			ret = FF4_ERR_NO_MEM;
			break;
		}
		ret = ff4_plan_sinf(sess, opts, cur, node);
		if (ret != FF4_OK)
			break;
		if (plan->extraHead != NULL)
			plan->extraTail->next = node;
		else
			plan->extraHead = node;
		plan->extraTail = node;
		node = NULL;
	}

	if (ret == FF4_OK) {
		ret = ff4_write_lcom_entry_payload(sess, opts, NULL, 0, plan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_extra_plan_list(sess, node);
	return ret;
}

int32_t ff4_write_book_entry_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				     const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4PlainEntryPlan *plan = (const Ff4PlainEntryPlan *)planPtr;
	Ff4PlainEntryData data;
	uint8_t buf[12];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
		ff4_memset(sess, buf, 0, sizeof(buf));
		ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
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

int32_t ff4_plan_book_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4PlainEntryPlan *plan = (Ff4PlainEntryPlan *)outPlanPtr;
	Ff4PlainEntryData data;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 5u)
		return FF4_ERR_INTERNAL;

	ff4_memcpy(sess, &data, sizeof(data), &entry->u.plain.data, sizeof(data));
	plan->fourcc = data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(data), &data, sizeof(data));
	ret = ff4_write_book_entry_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

/* ---- esds / mp4s writers & planners ---- */

int32_t ff4_write_sl_config_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const void *planPtr,
				    uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, plan->data.data, plan->data.size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_sl_config_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				  const void *planPtr,
				  uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 6u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_sl_config_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_dec_specific_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				       const void *planPtr,
				       uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, plan->data.data, plan->data.size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_dec_specific_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				     const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 5u, NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_dec_specific_payload(sess, fileCtx, pos + total, plan,
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

int32_t ff4_write_dec_config_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				     const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecConfigPlan *plan = (const Ff4DecConfigPlan *)planPtr;
	uint8_t buf[13];
	uint32_t packed;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	packed = ((uint32_t)plan->streamType << 26) |
		 (((uint32_t)plan->upStream & 1u) << 25) |
		 0x01000000u |
		 (plan->bufferSizeDb & 0xFFFFFFu);

	ff4_memset(sess, buf, 0, sizeof(buf));
	buf[0] = plan->objTypeInd;
	ff4_put_be32(buf + 1, packed);
	ff4_put_be32(buf + 5, plan->maxBitrate);
	ff4_put_be32(buf + 9, plan->avgBitrate);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dec_specific_descr(sess, fileCtx, pos + total, plan->decSpecific,
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

int32_t ff4_write_dec_config_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				   const void *planPtr,
				   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecConfigPlan *plan = (const Ff4DecConfigPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 4u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_dec_config_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_es_descr_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				   const void *planPtr,
				   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EsDescrPlan *plan = (const Ff4EsDescrPlan *)planPtr;
	uint8_t buf[3];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 0, plan->esId);
	buf[2] = plan->flags;
	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_dec_config_descr(sess, fileCtx, pos + total, &plan->decConfig,
						 NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_sl_config_descr(sess, fileCtx, pos + total, &plan->slConfig,
						NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_es_descr(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			   const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EsDescrPlan *plan = (const Ff4EsDescrPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_descr_header(sess, fileCtx, pos, plan->boxSize, 3u, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_es_descr_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_esds_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EsdsPlanFull *plan = (const Ff4EsdsPlanFull *)planPtr;
	uint8_t verFlags[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, verFlags, 0, sizeof(verFlags));
	ret = ff4_box_write(sess, fileCtx, verFlags, sizeof(verFlags), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	if (plan->kind >= 2u)
		return FF4_ERR_INTERNAL;

	if (plan->kind == 0u)
		ret = ff4_write_es_descr(sess, fileCtx, pos + total, &plan->u.structured,
					 NULL, &total);
	else
		ret = ff4_box_write(sess, fileCtx, plan->u.raw.data, plan->u.raw.size,
				    NULL, &total);

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_esds_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4EsdsPlanFull *plan = (const Ff4EsdsPlanFull *)planPtr;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_ESDS,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_esds_payload(sess, fileCtx, pos + total, plan, NULL, &total);
	}
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_mp4s_entry_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				     void *fileCtx, uint64_t pos, const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4Mp4sEntryPlan *plan = (const Ff4Mp4sEntryPlan *)planPtr;
	Ff4PlainEntryData data;
	uint8_t buf[8];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_esds_box(sess, opts, fileCtx, pos + total, plan->esds,
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

int32_t ff4_plan_sl_config(Ff4SessionImpl *sess, void *outPlanPtr)
{
	Ff4DecSpecificPlan *plan = (Ff4DecSpecificPlan *)outPlanPtr;
	uint8_t val = 2u;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_copy_data_buf(sess, 0, &plan->data, &val, 1u);
	if (ret == FF4_OK)
		ret = ff4_write_sl_config_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
	return ret;
}

int32_t ff4_plan_dec_specific(Ff4SessionImpl *sess, const Ff4BoxDataBuf *src, void *outPlanPtr)
{
	Ff4DecSpecificPlan *plan = (Ff4DecSpecificPlan *)outPlanPtr;

	if (sess == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (plan->data.data != NULL)
		return FF4_ERR_INTERNAL;

	ff4_memcpy(sess, &plan->data, sizeof(plan->data), src, sizeof(*src));
	return ff4_write_dec_specific_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
}

int32_t ff4_plan_dec_config(Ff4SessionImpl *sess, const Ff4EsdsDecConfigData *src, void *outPlanPtr)
{
	Ff4DecConfigPlan *plan = (Ff4DecConfigPlan *)outPlanPtr;
	Ff4DecSpecificPlan *ds = NULL;
	int32_t ret = FF4_OK;

	if (sess == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->objTypeInd, 16u, src, 16u);
	if (src->decSpecific.size != 0) {
		if (plan->decSpecific != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			ds = (Ff4DecSpecificPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*ds));
			if (ds == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_dec_specific(sess, &src->decSpecific, ds);
				if (ret == FF4_OK) {
					plan->decSpecific = ds;
					ds = NULL;
				}
			}
		}
	}
	if (ret == FF4_OK)
		ret = ff4_write_dec_config_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
	ff4_pool_free(sess, ds);
	return ret;
}

int32_t ff4_plan_es_descr(Ff4SessionImpl *sess, const Ff4EsdsDecConfigData *src, void *outPlanPtr)
{
	Ff4EsDescrPlan *plan = (Ff4EsDescrPlan *)outPlanPtr;
	int32_t ret;

	if (sess == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	plan->esId = 0u;
	plan->flags = 0u;
	ret = ff4_plan_dec_config(sess, src, &plan->decConfig);
	if (ret == FF4_OK) {
		ret = ff4_plan_sl_config(sess, &plan->slConfig);
		if (ret == FF4_OK)
			ret = ff4_write_es_descr_payload(sess, NULL, 0, plan, &plan->boxSize, NULL);
	}
	return ret;
}

int32_t ff4_plan_esds(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4EsdsModelFull *esds, void *outPlanPtr)
{
	Ff4EsdsPlanFull *plan = (Ff4EsdsPlanFull *)outPlanPtr;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || esds == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (esds->kind != 0u && esds->kind != 1u)
		return FF4_ERR_INTERNAL;

	if (esds->kind == 1u) {
		if (plan->u.raw.data != NULL)
			return FF4_ERR_INTERNAL;
		ff4_memcpy(sess, &plan->u.raw, sizeof(plan->u.raw),
			   &esds->u.raw, sizeof(esds->u.raw));
	} else {
		ret = ff4_plan_es_descr(sess, &esds->u.structured, &plan->u.structured);
		if (ret != FF4_OK)
			return ret;
	}

	plan->kind = esds->kind;
	ret = ff4_write_esds_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_mp4s_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4Mp4sEntryPlan *plan = (Ff4Mp4sEntryPlan *)outPlanPtr;
	Ff4EsdsPlanFull *esdsPlan = NULL;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 4u)
		return FF4_ERR_INTERNAL;

	plan->fourcc = entry->u.mp4s.data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(plan->data), &entry->u.mp4s.data, sizeof(plan->data));

	if (entry->u.mp4s.esds != NULL) {
		if (plan->esds != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			esdsPlan = (Ff4EsdsPlanFull *)ff4_pool_alloc(sess, FF4_POOL_META,
								     sizeof(*esdsPlan));
			if (esdsPlan == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_esds(sess, opts, entry->u.mp4s.esds, esdsPlan);
				if (ret == FF4_OK) {
					plan->esds = esdsPlan;
					esdsPlan = NULL;
				}
			}
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_mp4s_entry_payload(sess, opts, NULL, 0, plan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_esds_plan(sess, esdsPlan);
	return ret;
}

/* ---- 3GPP / STCS text entries ---- */

int32_t ff4_write_3stp_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TextSubBoxPlan *plan = (const Ff4TextSubBoxPlan *)planPtr;
	Ff4TextSubBoxData data;
	uint8_t buf[7];
	uint32_t len = 1u;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret == FF4_OK) {
		ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
		ff4_memset(sess, buf, 0, sizeof(buf));
		buf[0] = (uint8_t)((data.mode << 5) | (data.flags & 0x1Fu));
		if (data.mode == 1u || data.mode == 2u) {
			ff4_put_be16(buf + 1, data.val1);
			ff4_put_be16(buf + 3, data.val2);
			ff4_put_be16(buf + 5, data.val3);
			len = 7u;
		}
		ret = ff4_box_write(sess, fileCtx, buf, len, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_3stp_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TextSubBoxPlan *plan = (const Ff4TextSubBoxPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_3STP,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_3stp_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_text_entry_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				     void *fileCtx, uint64_t pos, uint32_t kind,
				     const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TextEntryPlan *plan = (const Ff4TextEntryPlan *)planPtr;
	Ff4TextEntryData data;
	uint8_t buf[32];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind != 8u && kind != 9u)
		return FF4_ERR_INTERNAL;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
	ff4_put_be16(buf + 8, data.version);
	ff4_put_be16(buf + 12, data.width);
	ff4_put_be16(buf + 14, data.height);
	ff4_put_be32(buf + 16, data.horizRes);
	ff4_put_be32(buf + 20, data.vertRes);
	ff4_put_be16(buf + 24, data.originX);
	ff4_put_be16(buf + 26, data.originY);
	buf[28] = data.colorDepth;

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK && kind == 9u)
		ret = ff4_write_3stp_box(sess, opts, fileCtx, pos + total, plan->subBox,
					 NULL, &total);

	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_plan_3stp(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4TextSubBoxData *src, void *outPlanPtr)
{
	Ff4TextSubBoxPlan *plan = (Ff4TextSubBoxPlan *)outPlanPtr;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->data, sizeof(plan->data), src, sizeof(*src));
	ret = ff4_write_3stp_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_text_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			    const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4TextEntryPlan *plan = (Ff4TextEntryPlan *)outPlanPtr;
	Ff4TextSubBoxPlan *sub = NULL;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 8u && entry->kind != 9u)
		return FF4_ERR_INTERNAL;

	plan->fourcc = entry->u.text.data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(plan->data), &entry->u.text.data, sizeof(plan->data));

	if (entry->kind == 9u) {
		if (entry->u.text.subBox == NULL) {
			ret = FF4_ERR_INVALID_STATE;
		} else if (plan->subBox != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			sub = (Ff4TextSubBoxPlan *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*sub));
			if (sub == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_3stp(sess, opts, entry->u.text.subBox, sub);
				if (ret == FF4_OK) {
					plan->subBox = sub;
					sub = NULL;
				}
			}
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_text_entry_payload(sess, opts, NULL, 0, entry->kind, plan,
						   &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_plan_node_a(sess, sub);
	return ret;
}

/* ---- mp4a audio entry ---- */

int32_t ff4_write_mp4a_entry_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				     void *fileCtx, uint64_t pos, const void *planPtr,
				     uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4Mp4aEntryPlan *plan = (const Ff4Mp4aEntryPlan *)planPtr;
	Ff4Mp4aEntryData data;
	const Ff4StsdExtraPlan *cur;
	uint8_t buf[28];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
	ff4_put_be16(buf + 16, data.channelCount);
	ff4_put_be16(buf + 18, data.sampleSize);
	ff4_put_be32(buf + 24, data.sampleRate);

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret == FF4_OK)
		ret = ff4_write_esds_box(sess, opts, fileCtx, pos + total, plan->esds, NULL, &total);
	if (ret == FF4_OK) {
		for (cur = plan->extraHead; cur != NULL; cur = cur->next) {
			ret = ff4_write_sinf_box(sess, opts, fileCtx, pos + total, cur,
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

int32_t ff4_plan_mp4a_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t timescale,
			    const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4Mp4aEntryPlan *plan = (Ff4Mp4aEntryPlan *)outPlanPtr;
	Ff4EsdsPlanFull *esdsPlan = NULL;
	Ff4StsdExtraPlan *extraNode = NULL;
	const Ff4BoxChunkNode *cur;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 3u)
		return FF4_ERR_INTERNAL;

	plan->fourcc = entry->u.audio.data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(plan->data), &entry->u.audio.data, sizeof(plan->data));

	if ((plan->data.mask & 0x10u) == 0) {
		if ((timescale & 0xFFFF0000u) != 0)
			return FF4_ERR_INVALID_STATE;
		plan->data.sampleRate = (plan->data.sampleRate & 0xFFFFu) |
					((uint32_t)(uint16_t)timescale << 16);
	}

	if (entry->u.audio.esds != NULL) {
		if (plan->esds != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			esdsPlan = (Ff4EsdsPlanFull *)ff4_pool_alloc(sess, FF4_POOL_META,
								     sizeof(*esdsPlan));
			if (esdsPlan == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_esds(sess, opts, entry->u.audio.esds, esdsPlan);
				if (ret == FF4_OK) {
					plan->esds = esdsPlan;
					esdsPlan = NULL;
				}
			}
		}
	}

	if (ret == FF4_OK) {
		for (cur = entry->u.audio.extraHead; cur != NULL; cur = cur->next) {
			extraNode = (Ff4StsdExtraPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								       sizeof(*extraNode));
			if (extraNode == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_sinf(sess, opts, cur, extraNode);
			if (ret != FF4_OK)
				break;
			if (plan->extraHead != NULL)
				plan->extraTail->next = extraNode;
			else
				plan->extraHead = extraNode;
			plan->extraTail = extraNode;
			extraNode = NULL;
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_mp4a_entry_payload(sess, opts, NULL, 0, plan, &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_esds_plan(sess, esdsPlan);
	ff4_free_extra_plan_list(sess, extraNode);
	return ret;
}

/* ---- Video entries (mp4v, avc1/avcC, 3DSS UUID) ---- */

int32_t ff4_write_3gpp_uuid_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				    const void *planPtr,
				    uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint32_t uuid[4];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(sess, uuid, 0, sizeof(uuid));
	uuid[0] = FF4_UUID_3DSS_0;
	uuid[1] = FF4_UUID_SONY_1;
	uuid[2] = FF4_UUID_SONY_2;
	uuid[3] = FF4_UUID_SONY_3;

	ret = ff4_write_uuid_bytes(sess, fileCtx, pos, uuid, NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_box_write(sess, fileCtx, plan->data.data, plan->data.size, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_3gpp_uuid_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				uint64_t pos, const void *planPtr,
				uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4DecSpecificPlan *plan = (const Ff4DecSpecificPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL)
		return FF4_ERR_INVALID_ARG;

	if (plan != NULL) {
		ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_UUID,
					   NULL, &total);
		if (ret == FF4_OK)
			ret = ff4_write_3gpp_uuid_payload(sess, fileCtx, pos + total, plan,
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

int32_t ff4_write_avcc_nalu_list(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
				 uint32_t kind, const Ff4NodeBuf *head,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4NodeBuf *cur;
	uint8_t lenBuf[2];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind >= 2u)
		return FF4_ERR_INTERNAL;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	for (cur = head; cur != NULL; cur = cur->next) {
		if ((cur->size & ~0xFFFFULL) != 0)
			return FF4_ERR_INTERNAL;
		ff4_put_be16(lenBuf, (uint16_t)cur->size);
		ret = ff4_box_write(sess, fileCtx, lenBuf, 2u, NULL, &total);
		if (ret != FF4_OK)
			return ret;
		ret = ff4_box_write(sess, fileCtx, cur->data, cur->size, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_write_avcc_payload(Ff4SessionImpl *sess, void *fileCtx, uint64_t pos,
			       const void *planPtr, uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4AvccPlan *plan = (const Ff4AvccPlan *)planPtr;
	Ff4AvccModel model;
	uint32_t count = 0;
	uint8_t hdr[6];
	uint8_t ppsCnt;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &model, sizeof(model), &plan->model, sizeof(model));
	ret = ff4_count_buf_list(model.spsHead, &count);
	if (ret != FF4_OK)
		return ret;
	if ((count & 0xFFFFFFE0u) != 0)
		return FF4_ERR_OVERFLOW;

	ff4_memset(sess, hdr, 0, sizeof(hdr));
	hdr[0] = model.configVersion;
	hdr[1] = model.profileInd;
	hdr[2] = model.profileCompat;
	hdr[3] = model.levelInd;
	hdr[4] = (uint8_t)((model.lenSizeMinusOne & 3u) | 0xFCu);
	hdr[5] = (uint8_t)((count & 0x1Fu) | 0xE0u);

	ret = ff4_box_write(sess, fileCtx, hdr, sizeof(hdr), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_avcc_nalu_list(sess, fileCtx, pos + total, 0u, model.spsHead,
				       NULL, &total);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_count_buf_list(model.ppsHead, &count);
	if (ret != FF4_OK)
		return ret;
	if ((count & 0xFFFFFF00u) != 0)
		return FF4_ERR_OVERFLOW;

	ppsCnt = (uint8_t)count;
	ret = ff4_box_write(sess, fileCtx, &ppsCnt, 1u, NULL, &total);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_avcc_nalu_list(sess, fileCtx, pos + total, 1u, model.ppsHead,
				       NULL, &total);
	if (ret == FF4_OK) {
		if (outWritten != NULL)
			*outWritten = total;
		if (accumWritten != NULL)
			*accumWritten += total;
	}
	return ret;
}

int32_t ff4_write_avcc_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
			   uint64_t pos, const void *planPtr,
			   uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4AvccPlan *plan = (const Ff4AvccPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, FF4_BOX_AVCC,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_avcc_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_video_entry_payload(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
				      void *fileCtx, uint64_t pos, uint32_t kind,
				      const void *planPtr,
				      uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4VideoEntryPlan *plan = (const Ff4VideoEntryPlan *)planPtr;
	Ff4VideoEntryData data;
	const Ff4StsdExtraPlan *cur;
	uint8_t buf[78];
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind != 1u && kind != 2u)
		return FF4_ERR_INTERNAL;

	ret = ff4_box_seek(sess, fileCtx, pos, 0);
	if (ret != FF4_OK)
		return ret;

	ff4_memcpy(sess, &data, sizeof(data), &plan->data, sizeof(data));
	ff4_memset(sess, buf, 0, sizeof(buf));
	ff4_put_be16(buf + 6, (uint16_t)data.dataRefIndex);
	ff4_put_be16(buf + 24, data.width);
	ff4_put_be16(buf + 26, data.height);
	ff4_put_be32(buf + 28, data.horizRes);
	ff4_put_be32(buf + 32, data.vertRes);
	ff4_put_be16(buf + 40, data.frameCount);
	ff4_memcpy(sess, buf + 42, 32u, data.compressorName, 32u);
	ff4_put_be16(buf + 74, data.depth);
	buf[76] = 0xFFu;
	buf[77] = 0xFFu;

	ret = ff4_box_write(sess, fileCtx, buf, sizeof(buf), NULL, &total);
	if (ret != FF4_OK)
		return ret;

	if (kind == 2u)
		ret = ff4_write_avcc_box(sess, opts, fileCtx, pos + total, plan->codecPlan,
					 NULL, &total);
	else
		ret = ff4_write_esds_box(sess, opts, fileCtx, pos + total, plan->codecPlan,
					 NULL, &total);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_write_3gpp_uuid_box(sess, opts, fileCtx, pos + total, plan->uuidPlan,
				      NULL, &total);
	if (ret != FF4_OK)
		return ret;

	for (cur = plan->extraHead; cur != NULL; cur = cur->next) {
		ret = ff4_write_sinf_box(sess, opts, fileCtx, pos + total, cur, NULL, &total);
		if (ret != FF4_OK)
			return ret;
	}

	if (outWritten != NULL)
		*outWritten = total;
	if (accumWritten != NULL)
		*accumWritten += total;
	return FF4_OK;
}

int32_t ff4_plan_3gpp_uuid(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			   const Ff4BoxDataBuf *src, void *outPlanPtr)
{
	Ff4DecSpecificPlan *plan = (Ff4DecSpecificPlan *)outPlanPtr;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->data, sizeof(plan->data), src, sizeof(*src));
	ret = ff4_write_3gpp_uuid_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_avcc(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
		      const Ff4AvccModel *src, void *outPlanPtr)
{
	Ff4AvccPlan *plan = (Ff4AvccPlan *)outPlanPtr;
	uint64_t payloadSize = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || src == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memcpy(sess, &plan->model, sizeof(plan->model), src, sizeof(*src));
	ret = ff4_write_avcc_payload(sess, NULL, 0, plan, &payloadSize, NULL);
	if (ret == FF4_OK)
		ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	return ret;
}

int32_t ff4_plan_video_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts,
			     const Ff4StsdEntry *entryPtr, void *outPlanPtr)
{
	const Ff4StsdEntryFull *entry = (const Ff4StsdEntryFull *)entryPtr;
	Ff4VideoEntryPlan *plan = (Ff4VideoEntryPlan *)outPlanPtr;
	Ff4EsdsPlanFull *esdsPlan = NULL;
	Ff4AvccPlan *avccPlan = NULL;
	Ff4DecSpecificPlan *uuidPlan = NULL;
	Ff4StsdExtraPlan *extraNode = NULL;
	const Ff4BoxChunkNode *cur;
	uint64_t payloadSize = 0;
	int32_t ret = FF4_OK;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (entry->kind != 1u && entry->kind != 2u)
		return FF4_ERR_INTERNAL;

	plan->fourcc = entry->u.video.data.fourcc;
	ff4_memcpy(sess, &plan->data, sizeof(plan->data), &entry->u.video.data, sizeof(plan->data));

	if (entry->kind == 2u) {
		if (entry->u.video.codecCfg == NULL) {
			ret = FF4_ERR_INVALID_STATE;
		} else if (plan->codecPlan != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			avccPlan = (Ff4AvccPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								 sizeof(*avccPlan));
			if (avccPlan == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_avcc(sess, opts,
						    (const Ff4AvccModel *)entry->u.video.codecCfg,
						    avccPlan);
				if (ret == FF4_OK) {
					plan->codecPlan = avccPlan;
					avccPlan = NULL;
				}
			}
		}
	} else if (entry->u.video.codecCfg != NULL) {
		if (plan->codecPlan != NULL) {
			ret = FF4_ERR_INTERNAL;
		} else {
			esdsPlan = (Ff4EsdsPlanFull *)ff4_pool_alloc(sess, FF4_POOL_META,
								     sizeof(*esdsPlan));
			if (esdsPlan == NULL) {
				ret = FF4_ERR_NO_MEM;
			} else {
				ret = ff4_plan_esds(sess, opts,
						    (const Ff4EsdsModelFull *)entry->u.video.codecCfg,
						    esdsPlan);
				if (ret == FF4_OK) {
					plan->codecPlan = esdsPlan;
					esdsPlan = NULL;
				}
			}
		}
	}

	if (ret == FF4_OK && entry->u.video.uuid3dss != NULL) {
		uuidPlan = (Ff4DecSpecificPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								sizeof(*uuidPlan));
		if (uuidPlan == NULL) {
			ret = FF4_ERR_NO_MEM;
		} else {
			ret = ff4_plan_3gpp_uuid(sess, opts, entry->u.video.uuid3dss, uuidPlan);
			if (ret == FF4_OK) {
				plan->uuidPlan = uuidPlan;
				uuidPlan = NULL;
			}
		}
	}

	if (ret == FF4_OK) {
		for (cur = entry->u.video.extraHead; cur != NULL; cur = cur->next) {
			extraNode = (Ff4StsdExtraPlan *)ff4_pool_alloc(sess, FF4_POOL_META,
								       sizeof(*extraNode));
			if (extraNode == NULL) {
				ret = FF4_ERR_NO_MEM;
				break;
			}
			ret = ff4_plan_sinf(sess, opts, cur, extraNode);
			if (ret != FF4_OK)
				break;
			if (plan->extraHead != NULL)
				plan->extraTail->next = extraNode;
			else
				plan->extraHead = extraNode;
			plan->extraTail = extraNode;
			extraNode = NULL;
		}
	}

	if (ret == FF4_OK) {
		ret = ff4_write_video_entry_payload(sess, opts, NULL, 0, entry->kind, plan,
						    &payloadSize, NULL);
		if (ret == FF4_OK)
			ret = ff4_calc_box_size(sess, opts, payloadSize, &plan->boxSize);
	}

	ff4_free_esds_plan(sess, esdsPlan);
	ff4_free_plan_node_c(sess, avccPlan);
	ff4_free_plan_node_b(sess, uuidPlan);
	ff4_free_extra_plan_list(sess, extraNode);
	return ret;
}

/* ---- Dispatchers & entry box wrappers ---- */

int32_t ff4_plan_stsd_entry(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, uint32_t timescale,
			    const Ff4StsdEntry *entry, Ff4StsdPlan *outPlanPtr)
{
	Ff4StsdPlanFull *plan = (Ff4StsdPlanFull *)outPlanPtr;

	if (sess == NULL || opts == NULL || entry == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	switch (entry->kind) {
	case 1u:
	case 2u:
		return ff4_plan_video_entry(sess, opts, entry, &plan->u.video);
	case 3u:
		return ff4_plan_mp4a_entry(sess, opts, timescale, entry, &plan->u.audio);
	case 4u:
		return ff4_plan_mp4s_entry(sess, opts, entry, &plan->u.mp4s);
	case 5u:
		return ff4_plan_book_entry(sess, opts, entry, &plan->u.plain);
	case 6u:
		return ff4_plan_lcom_entry(sess, opts, entry, &plan->u.lcom);
	case 7u:
		return ff4_plan_plain_entry(sess, opts, entry, &plan->u.plain);
	case 8u:
	case 9u:
		return ff4_plan_text_entry(sess, opts, entry, &plan->u.text);
	default:
		return FF4_ERR_INTERNAL;
	}
}

int32_t ff4_write_plain_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				  uint64_t pos, const void *planPtr,
				  uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4PlainEntryPlan *plan = (const Ff4PlainEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_plain_entry_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_lcom_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const void *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4LcomEntryPlan *plan = (const Ff4LcomEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_lcom_entry_payload(sess, opts, fileCtx, pos + total, plan,
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

int32_t ff4_write_book_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const void *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4PlainEntryPlan *plan = (const Ff4PlainEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_book_entry_payload(sess, fileCtx, pos + total, plan, NULL, &total);
		if (ret == FF4_OK) {
			if (outWritten != NULL)
				*outWritten = total;
			if (accumWritten != NULL)
				*accumWritten += total;
		}
	}
	return ret;
}

int32_t ff4_write_mp4s_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const void *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4Mp4sEntryPlan *plan = (const Ff4Mp4sEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mp4s_entry_payload(sess, opts, fileCtx, pos + total, plan,
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

int32_t ff4_write_text_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, uint32_t kind, const void *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4TextEntryPlan *plan = (const Ff4TextEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind != 8u && kind != 9u)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_text_entry_payload(sess, opts, fileCtx, pos + total, kind, plan,
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

int32_t ff4_write_mp4a_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const void *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4Mp4aEntryPlan *plan = (const Ff4Mp4aEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_mp4a_entry_payload(sess, opts, fileCtx, pos + total, plan,
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

int32_t ff4_write_video_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				  uint64_t pos, uint32_t kind, const void *planPtr,
				  uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4VideoEntryPlan *plan = (const Ff4VideoEntryPlan *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind != 1u && kind != 2u)
		return FF4_ERR_INTERNAL;

	ret = ff4_write_box_header(sess, opts, fileCtx, pos, plan->boxSize, plan->fourcc,
				   NULL, &total);
	if (ret == FF4_OK) {
		ret = ff4_write_video_entry_payload(sess, opts, fileCtx, pos + total, kind, plan,
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

int32_t ff4_write_stsd_entry_box(Ff4SessionImpl *sess, const Ff4FormatOpts *opts, void *fileCtx,
				 uint64_t pos, const Ff4StsdPlan *planPtr,
				 uint64_t *outWritten, uint64_t *accumWritten)
{
	const Ff4StsdPlanFull *plan = (const Ff4StsdPlanFull *)planPtr;
	uint64_t total = 0;
	int32_t ret;

	if (sess == NULL || opts == NULL || plan == NULL)
		return FF4_ERR_INVALID_ARG;

	switch (plan->kind) {
	case 1u:
	case 2u:
		ret = ff4_write_video_entry_box(sess, opts, fileCtx, pos, plan->kind,
						&plan->u.video, NULL, &total);
		break;
	case 3u:
		ret = ff4_write_mp4a_entry_box(sess, opts, fileCtx, pos,
					       &plan->u.audio, NULL, &total);
		break;
	case 4u:
		ret = ff4_write_mp4s_entry_box(sess, opts, fileCtx, pos,
					       &plan->u.mp4s, NULL, &total);
		break;
	case 5u:
		ret = ff4_write_book_entry_box(sess, opts, fileCtx, pos,
					       &plan->u.plain, NULL, &total);
		break;
	case 6u:
		ret = ff4_write_lcom_entry_box(sess, opts, fileCtx, pos,
					       &plan->u.lcom, NULL, &total);
		break;
	case 7u:
		ret = ff4_write_plain_entry_box(sess, opts, fileCtx, pos,
						&plan->u.plain, NULL, &total);
		break;
	case 8u:
	case 9u:
		ret = ff4_write_text_entry_box(sess, opts, fileCtx, pos, plan->kind,
					       &plan->u.text, NULL, &total);
		break;
	default:
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

int32_t ff4_track_type_to_tkhd_flags(uint32_t kind, uint32_t *outFlags)
{
	if (outFlags == NULL)
		return FF4_ERR_INVALID_ARG;
	if (kind > 2u && kind != 6u && kind != 8u && kind != 0xFFFFFFFFu)
		return FF4_ERR_INVALID_ARG;

	if (kind == 1u || kind == 8u)
		*outFlags = 7u;
	else if (kind == 2u || kind == 6u)
		*outFlags = 6u;
	else
		*outFlags = 0u;
	return FF4_OK;
}
