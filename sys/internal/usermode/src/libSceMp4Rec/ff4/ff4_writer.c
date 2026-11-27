/*
 * Sony FF4 MP4 muxer: public writer API and top-level box/track/fragment
 * orchestrators.
 */
#include "ff4_internal.h"

/* Sony UUID constants */
static const uint32_t s_mtsdUuid[4] = {
	0x4D545344u, 0x21D24FCEu, 0xBB88695Cu, 0xFAC9C740u
};
static const uint32_t s_xmpUuid[4] = {
	0x508DEEF1u, 0x21864FCEu, 0xAAB21A44u, 0xD249B086u
};
static const uint32_t s_zpckUuid[4] = {
	0x5A50434Bu, 0x21D24FCEu, 0xBB88695Cu, 0xFAC9C740u
};

/* Internal parameter structs matching Sony's ABI */
typedef struct Ff4WriterCreateCfg {
	uint64_t maxMemorySize;
	Ff4FormatOpts opts;
} Ff4WriterCreateCfg;

typedef struct Ff4VideoTrackInitIn {
	uint32_t fourcc;
	uint32_t padA;
	uint32_t trackId;
	uint32_t width;
	uint32_t height;
	uint32_t timescale;
	uint16_t language;
	uint16_t padB;
	uint32_t handlerType;
} Ff4VideoTrackInitIn;

typedef struct Ff4AudioTrackInitIn {
	uint32_t fourcc;
	uint32_t padA;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t padB;
} Ff4AudioTrackInitIn;

typedef struct Ff4SimpleTrackInitIn {
	uint32_t fourcc;
	uint32_t padA;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t padB;
} Ff4SimpleTrackInitIn;

typedef struct Ff4OdsmTrackInitIn {
	uint32_t fourcc;
	uint32_t padA;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t padB;
	uint8_t profileBytes[16];
} Ff4OdsmTrackInitIn;

typedef struct Ff4BookTrackInitIn {
	uint32_t fourcc;
	uint32_t padA;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t padB;
	uint32_t extraParam;
} Ff4BookTrackInitIn;

typedef struct Ff4TrefParamIn {
	uint32_t type;
	uint32_t targetTrackId;
} Ff4TrefParamIn;

typedef struct Ff4ElstParamIn {
	uint64_t segmentDuration;
	int64_t mediaTime;
	uint32_t mediaRate;
} Ff4ElstParamIn;

typedef struct Ff4UrnDrefIn {
	const char *name;
	uint32_t nameSize;
	const char *location;
	uint32_t locationSize;
} Ff4UrnDrefIn;

typedef struct Ff4UrlDrefIn {
	const char *location;
	uint32_t locationSize;
} Ff4UrlDrefIn;

typedef struct Ff4DrefInitBuf {
	uint32_t fourcc;
	uint32_t flags;
	Ff4BoxDataBuf loc;
	Ff4BoxDataBuf name;
} Ff4DrefInitBuf;

typedef struct Ff4StsdAddBuf {
	uint32_t kind;
	uint32_t words[24];
} Ff4StsdAddBuf;

typedef struct Ff4RawEsdsIn {
	const uint8_t *data;
	uint32_t size;
} Ff4RawEsdsIn;

typedef struct Ff4SinfIn {
	const uint8_t *data;
	uint32_t size;
} Ff4SinfIn;

typedef struct Ff4Uuid3dssIn {
	uint32_t f0;
	uint32_t f1;
	uint32_t f2;
	uint32_t f3;
	uint32_t f4;
	uint32_t f5;
	uint32_t f6;
} Ff4Uuid3dssIn;

typedef struct Ff4StsdVideoLayout {
	uint32_t kind;
	uint32_t pad;
	uint32_t fields[13];
	void *codecCfg;
	void *uuid3dss;
	Ff4BoxChunkNode *sinfHead;
	Ff4BoxChunkNode *sinfTail;
} Ff4StsdVideoLayout;

typedef struct Ff4StsdAudioLayout {
	uint32_t kind;
	uint32_t pad;
	uint32_t fields[4];
	void *esds;
	Ff4BoxChunkNode *sinfHead;
	Ff4BoxChunkNode *sinfTail;
} Ff4StsdAudioLayout;

typedef struct Ff4StsdTextLayout {
	uint32_t kind;
	uint32_t pad;
	uint32_t fields[7];
	void *subBox;
} Ff4StsdTextLayout;

typedef struct Ff4StsdLcomLayout {
	uint32_t kind;
	uint32_t pad;
	uint32_t f08;
	Ff4BoxChunkNode *sinfHead;
	Ff4BoxChunkNode *sinfTail;
} Ff4StsdLcomLayout;

typedef struct Ff4MtsmGroupIn {
	uint32_t fourcc;
	uint8_t isUtf16;
	uint8_t pad;
	uint16_t lang;
	uint32_t version;
} Ff4MtsmGroupIn;

typedef struct Ff4RawUuidPlan {
	uint64_t boxSize;
	uint32_t uuid[4];
} Ff4RawUuidPlan;

typedef struct Ff4ProfPlanLocal {
	uint64_t boxSize;
	uint64_t fprfBoxSize;
	uint32_t baseField;
	uint32_t padA;
	Ff4ProfModel *model;
	uint32_t padB;
} Ff4ProfPlanLocal;

typedef struct Ff4OdSamplePlanBuf {
	uint64_t totalSize;
	uint32_t words[16];
} Ff4OdSamplePlanBuf;

/* ---- Writer lifecycle ---- */

int32_t Ff4DestroyWriter(Ff4Writer writerHandle)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer != NULL) {
		ff4_free_writer_models(writer->sess, &writer->models);
		ff4_buf_free(writer->sess, &writer->scratchBuf);
		ff4_pool_free(writer->sess, writer);
	}
	return FF4_OK;
}

int32_t Ff4CreateWriter(Ff4Session session, const Ff4WriterInitParam *param, Ff4Writer *outWriter)
{
	Ff4SessionImpl *sess = (Ff4SessionImpl *)session;
	const Ff4WriterCreateCfg *cfg = (const Ff4WriterCreateCfg *)param;
	Ff4WriterImpl *writer = NULL;
	int32_t ret;

	if (sess == NULL || cfg == NULL || outWriter == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_pool_init(sess, 0u, cfg->maxMemorySize, 0u, sizeof(Ff4WriterImpl));
	if (ret != FF4_OK)
		return ret;

	writer = (Ff4WriterImpl *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*writer));
	if (writer == NULL) {
		ret = FF4_ERR_NO_MEM;
	} else {
		writer->sess = sess;
		ret = ff4_buf_init(sess, FF4_POOL_META, 0x2000u, &writer->scratchBuf);
		if (ret == FF4_OK) {
			writer->opts = cfg->opts;
			*outWriter = writer;
			return FF4_OK;
		}
	}

	if (writer != NULL) {
		ff4_free_writer_models(writer->sess, &writer->models);
		ff4_buf_free(writer->sess, &writer->scratchBuf);
		ff4_pool_free(writer->sess, writer);
	}
	return ret;
}

/* ---- FTYP box ---- */

int32_t Ff4SetFtyp(Ff4Writer writerHandle, const Ff4FtypParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4FtypModel *ftyp;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.ftyp != NULL)
		return FF4_ERR_INVALID_STATE;

	ftyp = (Ff4FtypModel *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*ftyp));
	if (ftyp == NULL)
		return FF4_ERR_NO_MEM;

	ftyp->majorBrand = param->majorBrand;
	ftyp->minorVersion = param->minorVersion;
	writer->models.ftyp = ftyp;
	return FF4_OK;
}

int32_t Ff4AddCompatibleBrand(Ff4Writer writerHandle, uint32_t brand)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4NodeU32 *existing = NULL;
	Ff4NodeU32 *node;
	int32_t ret;

	if (writer == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.ftyp == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_find_u32_node(writer->models.ftyp->compatHead, brand, &existing);
	if (ret != FF4_OK)
		return ret;
	if (existing != NULL)
		return FF4_OK;

	node = (Ff4NodeU32 *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	node->value = brand;
	if (writer->models.ftyp->compatHead != NULL)
		writer->models.ftyp->compatTail->next = node;
	else
		writer->models.ftyp->compatHead = node;
	writer->models.ftyp->compatTail = node;
	return FF4_OK;
}

int32_t Ff4WriteFtyp(Ff4Writer writerHandle, void *fileCtx, uint64_t offset, uint64_t *outWritten)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4FtypPlan plan;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.ftyp == NULL)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &plan, 0, sizeof(plan));
	ret = ff4_plan_ftyp(writer->sess, &writer->opts, writer->models.ftyp, &plan);
	if (ret == FF4_OK) {
		ret = ff4_write_ftyp_box(writer->sess, &writer->opts, fileCtx, offset,
					 &plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}
	return ret;
}

/* ---- Sony PROF UUID box ---- */

int32_t Ff4CreateProfileBox(Ff4Writer writerHandle, uint32_t *outProfileId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4ProfModel *prof;

	if (writer == NULL || outProfileId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof != NULL)
		return FF4_ERR_INVALID_STATE;

	prof = (Ff4ProfModel *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*prof));
	if (prof == NULL)
		return FF4_ERR_NO_MEM;

	prof->baseField = *outProfileId;
	writer->models.prof = prof;
	return FF4_OK;
}

static int32_t ff4_append_prof_entry(Ff4WriterImpl *writer, Ff4ProfEntry *entry)
{
	if (writer->models.prof->head != NULL)
		writer->models.prof->tail->next = entry;
	else
		writer->models.prof->head = entry;
	writer->models.prof->tail = entry;
	return FF4_OK;
}

int32_t Ff4AddVideoProfile(Ff4Writer writerHandle, const Ff4ProfVideoParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4ProfEntry *entry;
	int32_t valid = 0;
	int32_t ret;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_valid_track_id(param->trackIndex, &valid);
	if (ret != FF4_OK)
		return ret;
	if (valid == 0)
		return FF4_ERR_INVALID_ARG;

	entry = (Ff4ProfEntry *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*entry));
	if (entry == NULL)
		return FF4_ERR_NO_MEM;

	entry->fourcc = FF4_BOX_VPRF;
	entry->fields[0] = param->trackIndex;
	entry->fields[1] = param->codecFourcc;
	entry->fields[2] = param->profileLevel;
	entry->fields[3] = param->reserved0;
	entry->fields[4] = param->avgBitrateKbps;
	entry->fields[5] = param->maxBitrateKbps;
	entry->fields[6] = ((uint32_t)param->fpsInt << 16) | (uint32_t)param->fpsFrac;
	entry->fields[7] = ((uint32_t)param->fpsInt2 << 16) | (uint32_t)param->fpsFrac2;
	entry->fields[8] = ((uint32_t)param->width << 16) | (uint32_t)param->height;
	entry->fields[9] = param->aspectRatio;
	return ff4_append_prof_entry(writer, entry);
}

int32_t Ff4AddAudioProfile(Ff4Writer writerHandle, const Ff4ProfAudioParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4ProfEntry *entry;
	int32_t valid = 0;
	int32_t ret;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_valid_track_id(param->trackIndex, &valid);
	if (ret != FF4_OK)
		return ret;
	if (valid == 0)
		return FF4_ERR_INVALID_ARG;

	entry = (Ff4ProfEntry *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*entry));
	if (entry == NULL)
		return FF4_ERR_NO_MEM;

	entry->fourcc = FF4_BOX_APRF;
	entry->fields[0] = param->trackIndex;
	entry->fields[1] = param->codecFourcc;
	entry->fields[2] = param->profileLevel;
	entry->fields[3] = param->reserved0;
	entry->fields[4] = param->avgBitrateKbps;
	entry->fields[5] = param->maxBitrateKbps;
	entry->fields[6] = param->sampleRate;
	entry->fields[7] = param->channels;
	return ff4_append_prof_entry(writer, entry);
}

int32_t ff4_add_bprf_profile(Ff4WriterImpl *writer, const uint32_t *param)
{
	Ff4ProfEntry *entry;
	int32_t valid = 0;
	int32_t ret;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_valid_track_id(param[0], &valid);
	if (ret != FF4_OK)
		return ret;
	if (valid == 0)
		return FF4_ERR_INVALID_ARG;

	entry = (Ff4ProfEntry *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*entry));
	if (entry == NULL)
		return FF4_ERR_NO_MEM;

	entry->fourcc = FF4_BOX_BPRF;
	entry->fields[0] = param[0];
	entry->fields[1] = param[1];
	entry->fields[2] = param[2];
	entry->fields[3] = param[3];
	entry->fields[4] = param[4];
	return ff4_append_prof_entry(writer, entry);
}

int32_t ff4_add_pprf_profile(Ff4WriterImpl *writer, const uint32_t *param)
{
	Ff4ProfEntry *entry;
	int32_t valid = 0;
	int32_t ret;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_valid_track_id(param[0], &valid);
	if (ret != FF4_OK)
		return ret;
	if (valid == 0)
		return FF4_ERR_INVALID_ARG;

	entry = (Ff4ProfEntry *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*entry));
	if (entry == NULL)
		return FF4_ERR_NO_MEM;

	entry->fourcc = FF4_BOX_PPRF;
	entry->fields[0] = param[0];
	entry->fields[1] = param[1];
	entry->fields[2] = param[2];
	entry->fields[3] = param[3];
	entry->fields[4] = param[4];
	return ff4_append_prof_entry(writer, entry);
}

int32_t Ff4WriteProfileBox(Ff4Writer writerHandle, void *fileCtx, uint64_t offset, uint64_t *outWritten)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4ProfPlanLocal plan;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.prof == NULL)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &plan, 0, sizeof(plan));
	ret = ff4_plan_prof(writer->sess, &writer->opts, writer->models.prof, &plan);
	if (ret == FF4_OK) {
		ret = ff4_write_prof_box(writer->sess, &writer->opts, fileCtx, offset,
					 &plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}
	return ret;
}

/* ---- MOOV box & movie header ---- */

int32_t Ff4SetMovieTimescale(Ff4Writer writerHandle, uint32_t timescale)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MovieModel *moov;

	if (writer == NULL || timescale == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov != NULL)
		return FF4_ERR_INVALID_STATE;

	moov = (Ff4MovieModel *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*moov));
	if (moov == NULL)
		return FF4_ERR_NO_MEM;

	moov->timescale = timescale;
	moov->rate = 0x00010000u;
	moov->volume = 0x0100u;
	moov->matrix[0] = 0x00010000;
	moov->matrix[4] = 0x00010000;
	moov->matrix[8] = 0x40000000;
	writer->models.moov = moov;
	return FF4_OK;
}

int32_t ff4_set_movie_params(Ff4WriterImpl *writer, uint32_t mask, const uint32_t *params)
{
	if (writer == NULL || params == NULL || (mask & ~0x3FFu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_set_mvhd_params(writer->sess, &writer->opts, writer->models.moov, mask, params);
}

int32_t Ff4WriteMoov(Ff4Writer writerHandle, void *fileCtx, uint64_t offset, uint64_t *outWritten)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MoovPlan *plan = NULL;
	int32_t allHaveChunks = 0;
	int32_t noPendingStsc = 0;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_query_fragment_state(writer->sess, &writer->models, 0u,
				       &allHaveChunks, &noPendingStsc, NULL, NULL);
	if (ret != FF4_OK)
		return ret;
	if (allHaveChunks == 0 || noPendingStsc == 0)
		return FF4_ERR_INVALID_STATE;

	plan = (Ff4MoovPlan *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_plan_moov(writer->sess, &writer->opts, writer->models.moov, plan);
	if (ret == FF4_OK) {
		ret = ff4_write_moov_box(writer->sess, &writer->opts, fileCtx, offset,
					 plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}

	ff4_free_moov_plan(writer->sess, plan);
	return ret;
}

/* ---- Track creation and configuration ---- */

int32_t ff4_alloc_track_id(Ff4WriterImpl *writer, uint32_t *outTrackId)
{
	if (writer == NULL || outTrackId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_alloc_next_track_id(writer->sess, writer->models.moov->trackHead, outTrackId);
}

int32_t Ff4AddVideoTrack(Ff4Writer writerHandle, const Ff4VideoTrackParam *param, uint32_t *outTrackHandle)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4VideoTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0 || (param->flags != 1u && param->flags != 2u))
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('v','i','d','e');
	in.trackId = param->trackId;
	in.width = ((uint32_t)param->width << 16) | (uint32_t)param->widthFrac;
	in.height = ((uint32_t)param->height << 16) | (uint32_t)param->heightFrac;
	in.timescale = param->timescale;
	in.language = param->language;
	in.handlerType = param->flags;
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t Ff4AddAudioTrack(Ff4Writer writerHandle, const Ff4AudioTrackParam *param, uint32_t *outTrackHandle)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4AudioTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('s','o','u','n');
	in.trackId = param->trackId;
	in.timescale = param->timescale;
	in.language = param->language;
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t ff4_add_grph_track(Ff4WriterImpl *writer, const Ff4SimpleTrackInitIn *param, uint32_t *outTrackHandle)
{
	Ff4SimpleTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('G','R','P','H');
	in.trackId = param->trackId;
	in.timescale = param->timescale;
	in.language = param->language;
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t ff4_add_odsm_track(Ff4WriterImpl *writer, const Ff4OdsmTrackInitIn *param, uint32_t *outTrackHandle)
{
	Ff4OdsmTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0 || writer->models.moov->iods != NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('o','d','s','m');
	in.trackId = param->trackId;
	in.timescale = param->timescale;
	in.language = param->language;
	ff4_memcpy(writer->sess, in.profileBytes, sizeof(in.profileBytes),
		   param->profileBytes, sizeof(in.profileBytes));
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t ff4_add_book_track(Ff4WriterImpl *writer, const Ff4BookTrackInitIn *param, uint32_t *outTrackHandle)
{
	Ff4BookTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('B','O','O','K');
	in.trackId = param->trackId;
	in.timescale = param->timescale;
	in.language = param->language;
	in.extraParam = param->extraParam;
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t ff4_add_pdat_track(Ff4WriterImpl *writer, const Ff4SimpleTrackInitIn *param, uint32_t *outTrackHandle)
{
	Ff4SimpleTrackInitIn in;
	int32_t avail = 0;
	int32_t ret;

	if (writer == NULL || param == NULL || outTrackHandle == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->timescale == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_track_id_available(param->trackId, writer->models.moov->trackHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.fourcc = FF4_FCC('P','D','A','T');
	in.trackId = param->trackId;
	in.timescale = param->timescale;
	in.language = param->language;
	return ff4_add_track(writer->sess, writer->models.moov, (const uint32_t *)&in, outTrackHandle);
}

int32_t ff4_set_movie_iods_params(Ff4WriterImpl *writer, uint8_t mask, const uint16_t *params)
{
	if (writer == NULL || params == NULL || (mask & 0x80u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL || writer->models.moov->iods == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_set_iods_params(writer->sess, writer->models.moov->iods, mask, params);
}

int32_t Ff4SetTrackAttribute(Ff4Writer writerHandle, uint32_t trackHandle, uint32_t attrFlags, const void *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || (attrFlags & ~0x7FFu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	if ((attrFlags & 8u) != 0) {
		uint32_t newId = ((const uint32_t *)param)[6];
		int32_t avail = 0;
		ret = ff4_is_track_id_available(newId, writer->models.moov->trackHead, &avail);
		if (ret != FF4_OK)
			return ret;
		if (avail == 0)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_tkhd_params(writer->sess, &writer->opts, track, attrFlags, param);
}

int32_t ff4_set_track_mdhd_params(Ff4WriterImpl *writer, uint32_t trackHandle, uint32_t mask,
				  const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || params == NULL || (mask & ~0x1Fu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	return ff4_set_mdhd_params(writer->sess, &writer->opts,
				   (Ff4MdhdModel *)&track->mdhdMask, mask, params);
}

int32_t ff4_set_track_hdlr_params(Ff4WriterImpl *writer, uint32_t trackHandle, uint8_t mask,
				  const void *const *params)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || params == NULL || (mask & ~1u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	return ff4_set_hdlr_params(writer->sess, (Ff4HdlrModel *)&track->handlerType, mask, params);
}

int32_t ff4_set_track_vmhd_params(Ff4WriterImpl *writer, uint32_t trackHandle, uint8_t mask,
				  const uint16_t *params)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || params == NULL || (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->minfType != 1u)
		return FF4_ERR_INVALID_STATE;

	return ff4_set_vmhd_params(writer->sess, track->minfHdrData, mask, params);
}

int32_t ff4_set_track_smhd_params(Ff4WriterImpl *writer, uint32_t trackHandle, uint8_t mask,
				  const uint8_t *params)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || params == NULL || (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->minfType != 2u)
		return FF4_ERR_INVALID_STATE;

	return ff4_set_smhd_params(writer->sess, track->minfHdrData, mask, params);
}

int32_t ff4_add_track_ref(Ff4WriterImpl *writer, uint32_t trackHandle, const Ff4TrefParamIn *param,
			  uint32_t *outIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4TrackModel *target = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_find_track_by_id(writer->models.moov->trackHead, param->targetTrackId, &target, NULL);
	if (ret != FF4_OK)
		return ret;
	if (target == NULL)
		return FF4_ERR_INVALID_ARG;

	return ff4_add_track_reference(writer->sess, track, (const uint32_t *)param, outIndex);
}

int32_t ff4_add_track_edit_entry(Ff4WriterImpl *writer, uint32_t trackHandle, const Ff4ElstParamIn *param)
{
	Ff4TrackModel *track = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	return ff4_add_track_edit(writer->sess, &writer->opts, track, (const uint32_t *)param);
}

int32_t Ff4FinalizeTrack(Ff4Writer writerHandle, uint32_t trackHandle, const uint32_t *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4ElstParamIn entry;
	uint64_t trackDur = 0;
	uint64_t minPres = 0;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_track_duration_in_timescale(writer->models.moov->timescale, track, &trackDur);
	if (ret != FF4_OK)
		return ret;
	ret = ff4_get_track_min_pres_in_timescale(writer->models.moov->timescale, track, &minPres);
	if (ret != FF4_OK)
		return ret;

	ff4_memset(writer->sess, &entry, 0, sizeof(entry));
	entry.segmentDuration = trackDur;
	entry.mediaTime = (int64_t)minPres;
	entry.mediaRate = 0x00010000u;
	return ff4_add_track_edit(writer->sess, &writer->opts, track, (const uint32_t *)&entry);
}

/* ---- Track data references (dref) ---- */

int32_t ff4_add_track_urn_dref(Ff4WriterImpl *writer, uint32_t trackHandle, const Ff4UrnDrefIn *param,
			       uint32_t *outRefIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4DrefInitBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outRefIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->name == NULL && param->nameSize != 0)
		return FF4_ERR_INVALID_ARG;
	if (param->location == NULL && param->locationSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.fourcc = FF4_BOX_URN;
	buf.flags = (param->nameSize == 0 && param->locationSize == 0) ? 1u : 0u;
	buf.loc.data = (uint8_t *)param->location;
	buf.loc.size = param->locationSize;
	buf.name.data = (uint8_t *)param->name;
	buf.name.size = param->nameSize;
	return ff4_add_dref_entry(writer->sess, &track->drefHead, (const uint32_t *)&buf, outRefIndex);
}

int32_t Ff4AddTrackDataRef(Ff4Writer writerHandle, uint32_t trackHandle, const Ff4DataRefParam *param,
			   uint32_t *outRefIndex)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	const Ff4UrlDrefIn *in = (const Ff4UrlDrefIn *)param;
	Ff4TrackModel *track = NULL;
	Ff4DrefInitBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || in == NULL || outRefIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (in->location == NULL && in->locationSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.fourcc = FF4_BOX_URL;
	buf.flags = (in->locationSize == 0) ? 1u : 0u;
	buf.loc.data = (uint8_t *)in->location;
	buf.loc.size = in->locationSize;
	return ff4_add_dref_entry(writer->sess, &track->drefHead, (const uint32_t *)&buf, outRefIndex);
}

/* ---- Track sample descriptions (stsd) ---- */

int32_t Ff4AddVideoSampleDesc(Ff4Writer writerHandle, uint32_t trackHandle,
			      const Ff4VideoSampleDescParam *param, uint32_t *outDescIndex)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->codecType != 1u && param->codecType != 2u)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('v','i','d','e'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param->dataRefIndex, &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = param->codecType;
	buf.words[0] = (param->codecType == 2u) ? FF4_BOX_AVC1 : FF4_BOX_MP4V;
	buf.words[1] = param->dataRefIndex;
	buf.words[2] = ((uint32_t)param->height << 16) | (uint32_t)param->width;
	buf.words[3] = ((uint32_t)param->horizResInt << 16) | (uint32_t)param->horizResFrac;
	buf.words[4] = ((uint32_t)param->vertResInt << 16) | (uint32_t)param->vertResFrac;
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t Ff4AddAudioSampleDesc(Ff4Writer writerHandle, uint32_t trackHandle,
			      const Ff4AudioSampleDescParam *param, uint32_t *outDescIndex)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('s','o','u','n'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param->dataRefIndex, &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 3u;
	buf.words[0] = FF4_BOX_MP4A;
	buf.words[1] = param->dataRefIndex;
	buf.words[2] = ((uint32_t)param->sampleSize << 16) | (uint32_t)param->channelCount;
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t ff4_add_text_sample_desc(Ff4WriterImpl *writer, uint32_t trackHandle,
				 const uint32_t *param, uint32_t *outDescIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param[0] != 8u && param[0] != 9u)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('G','R','P','H'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param[1], &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = param[0];
	buf.words[0] = (param[0] == 9u) ? FF4_FCC('3','S','T','G') : FF4_FCC('S','T','C','S');
	buf.words[1] = param[1];
	buf.words[2] = param[2];
	buf.words[3] = param[3];
	buf.words[4] = param[4];
	buf.words[5] = param[5];
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t ff4_add_mp4s_sample_desc(Ff4WriterImpl *writer, uint32_t trackHandle,
				 const uint32_t *param, uint32_t *outDescIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('o','d','s','m'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param[0], &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 4u;
	buf.words[0] = FF4_BOX_MP4S;
	buf.words[1] = param[0];
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t ff4_add_book_sample_desc(Ff4WriterImpl *writer, uint32_t trackHandle,
				 const uint32_t *param, uint32_t *outDescIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('B','O','O','K'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param[0], &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 5u;
	buf.words[0] = FF4_FCC('b','o','o','k');
	buf.words[1] = param[0];
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t ff4_add_plain_sample_desc(Ff4WriterImpl *writer, uint32_t trackHandle,
				  const uint32_t *param, uint32_t *outDescIndex)
{
	Ff4TrackModel *track = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;
	if (track->handlerType != FF4_FCC('P','D','A','T'))
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_dref_at(track->drefHead, param[0], &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 7u;
	buf.words[0] = FF4_FCC('P','L','A','N');
	buf.words[1] = param[0];
	return ff4_add_stsd_entry(writer->sess, &track->stsdHead, &buf, outDescIndex);
}

int32_t ff4_set_video_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				       uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~0x7FFu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u && stsd->kind != 2u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0) {
		uint32_t fcc = params[0];
		if (stsd->kind == 1u) {
			if (fcc != FF4_BOX_MP4V && fcc != FF4_FCC('e','n','c','v'))
				return FF4_ERR_INVALID_ARG;
		} else if (fcc != FF4_BOX_AVC1 && fcc != FF4_FCC('e','n','c','v')) {
			return FF4_ERR_INVALID_ARG;
		}
	}
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_audio_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				       uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~0x1Fu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 3u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0) {
		uint32_t fcc = params[0];
		if (fcc != FF4_BOX_MP4A && fcc != FF4_FCC('e','n','c','a'))
			return FF4_ERR_INVALID_ARG;
	}
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_text_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				      uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~0x3Fu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 8u && stsd->kind != 9u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0) {
		uint32_t fcc = params[0];
		if (stsd->kind == 8u) {
			if (fcc != FF4_FCC('S','T','C','S'))
				return FF4_ERR_INVALID_ARG;
		} else if (fcc != FF4_FCC('3','S','T','G')) {
			return FF4_ERR_INVALID_ARG;
		}
	}
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_mp4s_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				      uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 4u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0 && params[0] != FF4_BOX_MP4S)
		return FF4_ERR_INVALID_ARG;
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_book_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				      uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 5u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0 && params[0] != FF4_FCC('b','o','o','k'))
		return FF4_ERR_INVALID_ARG;
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_plain_sample_desc_attr(Ff4WriterImpl *writer, uint32_t trackHandle,
				       uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 7u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0 && params[0] != FF4_FCC('P','L','A','N'))
		return FF4_ERR_INVALID_ARG;
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(track->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

/* ---- Codec configuration (esds, avcC, 3DSS, 3STP, sinf) ---- */

int32_t Ff4SetAudioEsds(Ff4Writer writerHandle, uint32_t trackHandle, uint32_t descIndex,
			const Ff4EsdsParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4EsdsModel in;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->decoderSpecificInfo == NULL && param->decoderSpecificInfoSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u && stsd->kind != 3u && stsd->kind != 4u)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.kind = 0u;
	in.esId = param->objectType;
	in.streamType = param->bufferSizeDb;
	in.bufferSizeDb = param->maxBitrate;
	in.maxBitrate = param->avgBitrate;
	in.decSpecific = (uint8_t *)param->decoderSpecificInfo;
	in.decSpecificSize = param->decoderSpecificInfoSize;
	return ff4_set_stsd_esds(writer->sess, stsd, &in);
}

int32_t ff4_set_track_raw_esds(Ff4WriterImpl *writer, uint32_t trackHandle, uint32_t descIndex,
			       const Ff4RawEsdsIn *param)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4EsdsModel in;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u && stsd->kind != 3u && stsd->kind != 4u)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.kind = 1u;
	in.rawData = (uint8_t *)param->data;
	in.decSpecificSize = param->size;
	return ff4_set_stsd_esds(writer->sess, stsd, &in);
}

int32_t Ff4SetAvcConfigHeader(Ff4Writer writerHandle, uint32_t trackHandle, uint32_t descIndex,
			      const Ff4AvcConfigParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	uint8_t hdr[5];
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 2u)
		return FF4_ERR_INVALID_STATE;

	hdr[0] = param->configurationVersion;
	hdr[1] = param->profileIndication;
	hdr[2] = param->profileCompatibility;
	hdr[3] = param->levelIndication;
	hdr[4] = param->lengthSizeMinusOne;
	return ff4_set_stsd_avcc_hdr(writer->sess, stsd, hdr);
}

int32_t Ff4AddAvcSps(Ff4Writer writerHandle, uint32_t trackHandle, uint32_t descIndex,
		     const Ff4NalDataParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4StsdVideoLayout *v;
	Ff4AvccModel *avcc;
	Ff4BoxDataBuf in;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0 || (param->size >> 16) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 2u)
		return FF4_ERR_INVALID_STATE;

	v = (Ff4StsdVideoLayout *)stsd;
	avcc = (Ff4AvccModel *)v->codecCfg;
	if (avcc == NULL)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.data = (uint8_t *)param->data;
	in.size = param->size;
	return ff4_add_avcc_nalu(writer->sess, &avcc->spsHead, &in);
}

int32_t Ff4AddAvcPps(Ff4Writer writerHandle, uint32_t trackHandle, uint32_t descIndex,
		     const Ff4NalDataParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4StsdVideoLayout *v;
	Ff4AvccModel *avcc;
	Ff4BoxDataBuf in;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0 || (param->size >> 16) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 2u)
		return FF4_ERR_INVALID_STATE;

	v = (Ff4StsdVideoLayout *)stsd;
	avcc = (Ff4AvccModel *)v->codecCfg;
	if (avcc == NULL)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.data = (uint8_t *)param->data;
	in.size = param->size;
	return ff4_add_avcc_nalu(writer->sess, &avcc->ppsHead, &in);
}

int32_t ff4_set_avc_3dss_uuid(Ff4WriterImpl *writer, uint32_t trackHandle, uint32_t descIndex,
			      const Ff4Uuid3dssIn *param)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4StsdVideoLayout *v;
	Ff4Uuid3dssIn *node;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u && stsd->kind != 2u)
		return FF4_ERR_INVALID_STATE;

	v = (Ff4StsdVideoLayout *)stsd;
	if (v->uuid3dss != NULL)
		return FF4_ERR_INVALID_STATE;

	node = (Ff4Uuid3dssIn *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	ff4_memcpy(writer->sess, node, sizeof(*node), param, sizeof(*param));
	v->uuid3dss = node;
	return FF4_OK;
}

int32_t ff4_set_text_3stp_params(Ff4WriterImpl *writer, uint32_t trackHandle, uint32_t descIndex,
				 const void *param)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 8u && stsd->kind != 9u)
		return FF4_ERR_INVALID_STATE;

	return ff4_set_stsd_text_subbox(writer->sess, stsd, param);
}

static int32_t ff4_append_sinf_node(Ff4SessionImpl *sess, Ff4BoxChunkNode **head,
				    Ff4BoxChunkNode **tail, const Ff4SinfIn *param)
{
	Ff4BoxChunkNode *node;
	int32_t ret;

	node = (Ff4BoxChunkNode *)ff4_pool_alloc(sess, FF4_POOL_META, sizeof(*node));
	if (node == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_copy_data_buf(sess, 0, (Ff4BoxDataBuf *)node, param->data, param->size);
	if (ret != FF4_OK) {
		ff4_free_chunk_node_list(sess, node);
		return ret;
	}

	if (*head != NULL)
		(*tail)->next = node;
	else
		*head = node;
	*tail = node;
	return FF4_OK;
}

int32_t ff4_add_track_sinf(Ff4WriterImpl *writer, uint32_t trackHandle, uint32_t descIndex,
			   const Ff4SinfIn *param)
{
	Ff4TrackModel *track = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
	if (ret != FF4_OK)
		return ret;
	if (track == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(track->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;

	if (stsd->kind == 1u || stsd->kind == 2u) {
		Ff4StsdVideoLayout *v = (Ff4StsdVideoLayout *)stsd;
		return ff4_append_sinf_node(writer->sess, &v->sinfHead, &v->sinfTail, param);
	}
	if (stsd->kind == 3u) {
		Ff4StsdAudioLayout *a = (Ff4StsdAudioLayout *)stsd;
		return ff4_append_sinf_node(writer->sess, &a->sinfHead, &a->sinfTail, param);
	}
	return FF4_ERR_INVALID_STATE;
}

/* ---- Sony USMT / MTDT / MTDF / ZPCK / MTSD metadata ---- */

int32_t Ff4AddUsmtMetadata(Ff4Writer writerHandle, uint32_t flags, const Ff4MetadataEntryParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_set_mtdt_item(writer->sess, writer->models.moov, flags, param);
}

int32_t ff4_set_track_zpck_header(Ff4WriterImpl *writer, uint32_t trackHandle,
				  const uint32_t *hdrParams)
{
	Ff4TrackModel *track = NULL;
	Ff4MtdtEntry **headTail;
	Ff4ZpckHeader curHdr;
	Ff4MtdtEntry *found = NULL;
	int32_t ret;

	if (writer == NULL || hdrParams == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	if (trackHandle != 0) {
		ret = ff4_get_track_at(writer->models.moov->trackHead, trackHandle, &track);
		if (ret != FF4_OK)
			return ret;
		if (track == NULL)
			return FF4_ERR_NOT_FOUND;
		headTail = &track->mtdtHead;
	} else {
		headTail = &writer->models.moov->mtdtHead;
	}

	ret = ff4_get_mtdt_zpck_header(writer->sess, headTail, &curHdr, &found);
	if (ret != FF4_OK)
		return ret;
	if (found != NULL)
		return FF4_ERR_INVALID_STATE;

	return ff4_add_mtdt_zpck_header(writer->sess, headTail, 1u, hdrParams);
}

int32_t Ff4AddUsmtExtraMetadata(Ff4Writer writerHandle, uint32_t flags, const Ff4MetadataExtraParam *param)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_set_mtdf_item(writer->sess, writer->models.moov, flags, param);
}

int32_t Ff4CreateMtsdBox(Ff4Writer writerHandle, uint32_t *outBoxId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	uint32_t id;
	int32_t ret;

	if (writer == NULL || outBoxId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	for (id = 0xF800u; id <= 0xFFFFu; id++) {
		int32_t avail = 0;
		ret = ff4_is_mtsm_id_available(id, writer->models.moov->mtsmHead, &avail);
		if (ret != FF4_OK)
			return ret;
		if (avail != 0) {
			*outBoxId = id;
			return FF4_OK;
		}
	}
	return FF4_ERR_OVERFLOW;
}

int32_t Ff4AddMtsdTrack(Ff4Writer writerHandle, uint32_t boxId, uint32_t *outTrackId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MtsmEntry *entry;
	int32_t avail = 0;
	uint32_t count = 0;
	int32_t ret;

	if (writer == NULL || outTrackId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_mtsm_id_available(boxId, writer->models.moov->mtsmHead, &avail);
	if (ret != FF4_OK)
		return ret;
	if (avail == 0)
		return FF4_ERR_INVALID_ARG;

	entry = (Ff4MtsmEntry *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*entry));
	if (entry == NULL)
		return FF4_ERR_NO_MEM;

	entry->streamId = boxId;
	if (writer->models.moov->mtsmHead != NULL)
		writer->models.moov->mtsmTail->next = entry;
	else
		writer->models.moov->mtsmHead = entry;
	writer->models.moov->mtsmTail = entry;

	ret = ff4_count_mtsm_entries(writer->models.moov->mtsmHead, &count);
	if (ret == FF4_OK)
		*outTrackId = count;
	return ret;
}

int32_t ff4_set_mtsm_params(Ff4WriterImpl *writer, uint32_t mtsmHandle, uint32_t mask,
			    const uint32_t *params)
{
	Ff4MtsmEntry *mtsm = NULL;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || params == NULL || (mask & ~7u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	if ((mask & 4u) != 0) {
		int32_t avail = 0;
		ret = ff4_is_mtsm_id_available(params[4], writer->models.moov->mtsmHead, &avail);
		if (ret != FF4_OK)
			return ret;
		if (avail == 0)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_mtsh_params(writer->sess, &writer->opts, mtsm, mask, params);
}

int32_t ff4_add_mtsm_urn_dref(Ff4WriterImpl *writer, uint32_t mtsmHandle, const Ff4UrnDrefIn *param,
			      uint32_t *outRefIndex)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4DrefInitBuf buf;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || param == NULL || outRefIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->name == NULL && param->nameSize != 0)
		return FF4_ERR_INVALID_ARG;
	if (param->location == NULL && param->locationSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.fourcc = FF4_BOX_URN;
	buf.flags = (param->nameSize == 0 && param->locationSize == 0) ? 1u : 0u;
	buf.loc.data = (uint8_t *)param->location;
	buf.loc.size = param->locationSize;
	buf.name.data = (uint8_t *)param->name;
	buf.name.size = param->nameSize;
	return ff4_add_dref_entry(writer->sess, &mtsm->drefHead, (const uint32_t *)&buf, outRefIndex);
}

int32_t Ff4AddMtsdDataRef(Ff4Writer writerHandle, uint32_t trackId, const Ff4DataRefParam *param,
			  uint32_t *outRefIndex)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	const Ff4UrlDrefIn *in = (const Ff4UrlDrefIn *)param;
	Ff4MtsmEntry *mtsm = NULL;
	Ff4DrefInitBuf buf;
	int32_t ret;

	if (writer == NULL || trackId == 0 || in == NULL || outRefIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (in->location == NULL && in->locationSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, trackId, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.fourcc = FF4_BOX_URL;
	buf.flags = (in->locationSize == 0) ? 1u : 0u;
	buf.loc.data = (uint8_t *)in->location;
	buf.loc.size = in->locationSize;
	return ff4_add_dref_entry(writer->sess, &mtsm->drefHead, (const uint32_t *)&buf, outRefIndex);
}

int32_t Ff4AddMtsdSampleDesc(Ff4Writer writerHandle, uint32_t trackId,
			     const Ff4ThumbSampleDescParam *param, uint32_t *outDescIndex)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MtsmEntry *mtsm = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || trackId == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->type != 1u)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, trackId, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_dref_at(mtsm->drefHead, param->dataRefIndex, &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 1u;
	buf.words[0] = FF4_BOX_MP4V;
	buf.words[1] = param->dataRefIndex;
	buf.words[2] = ((uint32_t)param->height << 16) | (uint32_t)param->width;
	buf.words[3] = ((uint32_t)param->horizResInt << 16) | (uint32_t)param->horizResFrac;
	buf.words[4] = ((uint32_t)param->vertResInt << 16) | (uint32_t)param->vertResFrac;
	return ff4_add_stsd_entry(writer->sess, &mtsm->stsdHead, &buf, outDescIndex);
}

int32_t Ff4SetMtsdSampleDescAttr(Ff4Writer writerHandle, uint32_t trackId, uint32_t descIndex,
				 uint32_t attrId, const void *attrData)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	const uint32_t *params = (const uint32_t *)attrData;
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || trackId == 0 || descIndex == 0 || params == NULL ||
	    (attrId & ~0x7FFu) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, trackId, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u)
		return FF4_ERR_INVALID_STATE;

	if ((attrId & 1u) != 0) {
		uint32_t fcc = params[0];
		if (fcc != FF4_BOX_MP4V && fcc != FF4_FCC('P','N','G',' ') &&
		    fcc != FF4_FCC('e','n','c','v'))
			return FF4_ERR_INVALID_ARG;
	}
	if ((attrId & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(mtsm->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, (uint16_t)attrId, params);
}

int32_t ff4_add_mtsm_lcom_sample_desc(Ff4WriterImpl *writer, uint32_t mtsmHandle,
				      const uint32_t *param, uint32_t *outDescIndex)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4DrefEntry *dref = NULL;
	Ff4StsdAddBuf buf;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || param == NULL || outDescIndex == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_dref_at(mtsm->drefHead, param[0], &dref);
	if (ret != FF4_OK)
		return ret;
	if (dref == NULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &buf, 0, sizeof(buf));
	buf.kind = 6u;
	buf.words[0] = FF4_BOX_LBIN;
	buf.words[1] = param[0];
	return ff4_add_stsd_entry(writer->sess, &mtsm->stsdHead, &buf, outDescIndex);
}

int32_t ff4_set_mtsm_lcom_sample_desc_attr(Ff4WriterImpl *writer, uint32_t mtsmHandle,
					   uint32_t descIndex, uint16_t mask, const uint32_t *params)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || descIndex == 0 || params == NULL ||
	    (mask & ~3u) != 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 6u)
		return FF4_ERR_INVALID_STATE;

	if ((mask & 1u) != 0 && params[0] != FF4_BOX_LBIN && params[0] != FF4_FCC('e','n','c','m'))
		return FF4_ERR_INVALID_ARG;
	if ((mask & 2u) != 0) {
		Ff4DrefEntry *dref = NULL;
		ret = ff4_get_dref_at(mtsm->drefHead, params[1], &dref);
		if (ret != FF4_OK)
			return ret;
		if (dref == NULL)
			return FF4_ERR_INVALID_ARG;
	}

	return ff4_set_stsd_entry_params(writer->sess, stsd, mask, params);
}

int32_t ff4_set_mtsm_esds(Ff4WriterImpl *writer, uint32_t mtsmHandle, uint32_t descIndex,
			  const Ff4EsdsParam *param)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4EsdsModel in;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || descIndex == 0 || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (param->decoderSpecificInfo == NULL && param->decoderSpecificInfoSize != 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.kind = 0u;
	in.esId = param->objectType;
	in.streamType = param->bufferSizeDb;
	in.bufferSizeDb = param->maxBitrate;
	in.maxBitrate = param->avgBitrate;
	in.decSpecific = (uint8_t *)param->decoderSpecificInfo;
	in.decSpecificSize = param->decoderSpecificInfoSize;
	return ff4_set_stsd_esds(writer->sess, stsd, &in);
}

int32_t ff4_set_mtsm_raw_esds(Ff4WriterImpl *writer, uint32_t mtsmHandle, uint32_t descIndex,
			      const Ff4RawEsdsIn *param)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	Ff4EsdsModel in;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;
	if (stsd->kind != 1u)
		return FF4_ERR_INVALID_STATE;

	ff4_memset(writer->sess, &in, 0, sizeof(in));
	in.kind = 1u;
	in.rawData = (uint8_t *)param->data;
	in.decSpecificSize = param->size;
	return ff4_set_stsd_esds(writer->sess, stsd, &in);
}

int32_t ff4_add_mtsm_sinf(Ff4WriterImpl *writer, uint32_t mtsmHandle, uint32_t descIndex,
			  const Ff4SinfIn *param)
{
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	int32_t ret;

	if (writer == NULL || mtsmHandle == 0 || descIndex == 0 || param == NULL ||
	    param->data == NULL || param->size == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, mtsmHandle, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, descIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_NOT_FOUND;

	if (stsd->kind == 1u) {
		Ff4StsdVideoLayout *v = (Ff4StsdVideoLayout *)stsd;
		return ff4_append_sinf_node(writer->sess, &v->sinfHead, &v->sinfTail, param);
	}
	if (stsd->kind == 6u) {
		Ff4StsdLcomLayout *l = (Ff4StsdLcomLayout *)stsd;
		return ff4_append_sinf_node(writer->sess, &l->sinfHead, &l->sinfTail, param);
	}
	return FF4_ERR_INVALID_STATE;
}

int32_t Ff4AddMtsdSample(Ff4Writer writerHandle, uint32_t trackId, const Ff4ThumbItemParam *param,
			 uint32_t *outSampleId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MtsmEntry *mtsm = NULL;
	Ff4StsdEntry *stsd = NULL;
	uint32_t vals[3];
	int32_t ret;

	if (writer == NULL || trackId == 0 || param == NULL || outSampleId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, trackId, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_stsd_at(mtsm->stsdHead, param->sampleDescIndex, &stsd);
	if (ret != FF4_OK)
		return ret;
	if (stsd == NULL)
		return FF4_ERR_INVALID_ARG;

	vals[0] = param->sampleDescIndex;
	vals[1] = param->offset;
	vals[2] = param->size;
	return ff4_add_mtct_entry(writer->sess, &mtsm->mdstHead, vals, outSampleId);
}

int32_t Ff4AddMtsdGroup(Ff4Writer writerHandle, const Ff4ThumbGroupParam *param, uint32_t *outGroupId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL || param == NULL || outGroupId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_add_mtdt_zpck_table(writer->sess, &writer->models.moov->mtdtHead, param, outGroupId);
}

int32_t Ff4LinkMtsdSample(Ff4Writer writerHandle, uint32_t groupId, uint32_t trackId, uint32_t sampleId)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	Ff4MtdtEntry *mtdt = NULL;
	Ff4MtsmEntry *mtsm = NULL;
	Ff4NodeTriple *sample = NULL;
	uint32_t pair[2];
	int32_t ret;

	if (writer == NULL || groupId == 0 || trackId == 0 || sampleId == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_mtdt_at(writer->models.moov->mtdtHead, groupId, &mtdt);
	if (ret != FF4_OK)
		return ret;
	if (mtdt == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_mtsm_at(writer->models.moov->mtsmHead, trackId, &mtsm);
	if (ret != FF4_OK)
		return ret;
	if (mtsm == NULL)
		return FF4_ERR_NOT_FOUND;

	ret = ff4_get_triple_at(mtsm->mdstHead, sampleId, &sample);
	if (ret != FF4_OK)
		return ret;
	if (sample == NULL)
		return FF4_ERR_NOT_FOUND;

	pair[0] = mtsm->streamId;
	pair[1] = sampleId;
	return ff4_append_zpck_entry_to_mtdt(writer->sess, mtdt, pair);
}

static int32_t ff4_write_named_uuid_header(Ff4WriterImpl *writer, void *fileCtx, uint64_t pos,
					   const uint32_t uuid[4], uint64_t payloadSize,
					   uint64_t *outHeaderSize)
{
	Ff4RawUuidPlan plan;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outHeaderSize == NULL)
		return FF4_ERR_INVALID_ARG;
	if (payloadSize > 0xFFFFFFFFFFFFFFDFULL)
		return FF4_ERR_INVALID_ARG;

	ff4_memset(writer->sess, &plan, 0, sizeof(plan));
	ret = ff4_plan_raw_uuid(writer->sess, &writer->opts, uuid, payloadSize, &plan);
	if (ret == FF4_OK) {
		ret = ff4_write_raw_uuid_box(writer->sess, &writer->opts, fileCtx, pos,
					     &plan, &written, NULL);
		if (ret == FF4_OK)
			*outHeaderSize = written;
	}
	return ret;
}

int32_t Ff4WriteMtsdHeader(Ff4Writer writerHandle, void *fileCtx, uint64_t offset,
			   uint64_t payloadSize, uint64_t *outHeaderSize)
{
	return ff4_write_named_uuid_header((Ff4WriterImpl *)writerHandle, fileCtx, offset,
					   s_mtsdUuid, payloadSize, outHeaderSize);
}

/* ---- Timestamps & generic/UUID box headers ---- */

int32_t Ff4SetCreationTime(Ff4Writer writerHandle, uint32_t trackIndex, uint8_t flags,
			   uint64_t creationTime, uint64_t modTime)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_set_timestamps(writer->sess, &writer->opts, writer->models.moov,
				  trackIndex, flags, creationTime, modTime);
}

int32_t Ff4WriteMdatHeader(Ff4Writer writerHandle, void *fileCtx, uint64_t offset, uint32_t fourcc,
			   uint64_t payloadSize, uint64_t *outHeaderSize)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;
	uint64_t boxSize = 0;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outHeaderSize == NULL)
		return FF4_ERR_INVALID_ARG;
	if (payloadSize > 0xFFFFFFFFFFFFFFEFULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_calc_box_size(writer->sess, &writer->opts, payloadSize, &boxSize);
	if (ret == FF4_OK) {
		ret = ff4_write_box_header(writer->sess, &writer->opts, fileCtx, offset,
					   boxSize, fourcc, &written, NULL);
		if (ret == FF4_OK)
			*outHeaderSize = written;
	}
	return ret;
}

int32_t ff4_write_custom_uuid_header(Ff4WriterImpl *writer, void *fileCtx, uint64_t offset,
				     const uint32_t uuid[4], uint64_t payloadSize,
				     uint64_t *outHeaderSize)
{
	if (uuid == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_write_named_uuid_header(writer, fileCtx, offset, uuid, payloadSize, outHeaderSize);
}

int32_t ff4_write_xmp_uuid_header(Ff4WriterImpl *writer, void *fileCtx, uint64_t offset,
				  uint64_t payloadSize, uint64_t *outHeaderSize)
{
	return ff4_write_named_uuid_header(writer, fileCtx, offset, s_xmpUuid, payloadSize, outHeaderSize);
}

int32_t ff4_write_zpck_uuid_header(Ff4WriterImpl *writer, void *fileCtx, uint64_t offset,
				   uint64_t payloadSize, uint64_t *outHeaderSize)
{
	return ff4_write_named_uuid_header(writer, fileCtx, offset, s_zpckUuid, payloadSize, outHeaderSize);
}

/* ---- OD / Meta descriptor sample APIs ---- */

int32_t ff4_alloc_od_id_hi(Ff4WriterImpl *writer, uint32_t *outId)
{
	uint32_t id;
	int32_t ret;

	if (writer == NULL || outId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	for (id = 0x100u; id <= 0x3FDu; id++) {
		Ff4TrefEntry *found = NULL;
		ret = ff4_find_tref_by_id(writer->models.meta.trefHead, (uint16_t)id, &found);
		if (ret != FF4_OK)
			return ret;
		if (found == NULL) {
			*outId = id;
			return FF4_OK;
		}
	}
	return FF4_ERR_OVERFLOW;
}

int32_t ff4_alloc_od_id_lo(Ff4WriterImpl *writer, uint32_t *outId)
{
	uint32_t id;
	int32_t ret;

	if (writer == NULL || outId == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	for (id = 0x80u; id <= 0xFFu; id++) {
		Ff4MetaItemNode *found = NULL;
		ret = ff4_find_meta_item_by_tag(writer->models.meta.dataHead, (uint8_t)id, &found, NULL);
		if (ret != FF4_OK)
			return ret;
		if (found == NULL) {
			*outId = id;
			return FF4_OK;
		}
	}
	return FF4_ERR_OVERFLOW;
}

int32_t ff4_add_od_entry(Ff4WriterImpl *writer, const uint16_t *param)
{
	int32_t validHi = 0;
	int32_t validLo = 0;
	int32_t ret;

	if (writer == NULL || param == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_is_valid_tref_id_hi(param[0], &validHi);
	if (ret != FF4_OK)
		return ret;
	if (validHi == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_is_valid_tref_id_lo(((const uint8_t *)param)[8], &validLo);
	if (ret != FF4_OK)
		return ret;
	if (validLo == 0)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_add_meta_tref(writer->sess, &writer->models.meta.trefHead, param);
	if (ret == FF4_OK)
		ret = ff4_add_meta_item(writer->sess, &writer->models.meta, param);
	return ret;
}

int32_t ff4_write_od_sample(Ff4WriterImpl *writer, void *fileCtx, uint64_t offset, uint64_t *outWritten)
{
	Ff4OdSamplePlanBuf *plan = NULL;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	if (writer->models.meta.trefHead == NULL && writer->models.meta.dataHead == NULL)
		return FF4_ERR_INVALID_STATE;

	plan = (Ff4OdSamplePlanBuf *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_plan_od_sample(writer->sess, &writer->models.meta, plan);
	if (ret == FF4_OK) {
		ret = ff4_write_od_sample_payload(writer->sess, fileCtx, offset, plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}

	ff4_free_run_table(writer->sess, plan);
	return ret;
}

/* ---- Sample recording and chunk / fragment management ---- */

int32_t Ff4AddTrackSample(Ff4Writer writerHandle, uint32_t trackHandle,
			  const Ff4SampleEntry *sample, uint32_t flags)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL || trackHandle == 0 || sample == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_add_sample(writer->sess, &writer->models, trackHandle, sample, (int32_t)flags);
}

int32_t Ff4FlushTrackChunk(Ff4Writer writerHandle, uint32_t trackHandle)
{
	Ff4WriterImpl *writer = (Ff4WriterImpl *)writerHandle;

	if (writer == NULL || trackHandle == 0)
		return FF4_ERR_INVALID_ARG;
	return ff4_commit_chunk(writer->sess, &writer->models, trackHandle);
}

int32_t ff4_shift_track_chunk_offsets(Ff4WriterImpl *writer, uint32_t trackHandle,
				      uint32_t chunkIdx1, int64_t delta)
{
	if (writer == NULL || trackHandle == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_shift_chunk_offsets(writer->sess, 0, trackHandle, 0u, chunkIdx1, delta,
				       writer->models.moov->trackHead);
}

int32_t ff4_shift_frag_chunk_offsets(Ff4WriterImpl *writer, uint32_t fragIdx, uint32_t trackHandle,
				     uint32_t chunkIdx1, int64_t delta)
{
	if (writer == NULL || fragIdx == 0 || trackHandle == 0)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;
	return ff4_shift_chunk_offsets(writer->sess, 1, trackHandle, fragIdx, chunkIdx1, delta,
				       writer->models.moov->trackHead);
}

int32_t ff4_set_writer_trex_defaults(Ff4WriterImpl *writer, uint32_t trackHandle, uint8_t mask,
				     const uint32_t *params)
{
	if (writer == NULL || trackHandle == 0 || params == NULL || (mask & ~0xFu) != 0)
		return FF4_ERR_INVALID_ARG;
	return ff4_set_trex_defaults(&writer->models, trackHandle, mask, params);
}

int32_t ff4_begin_first_fragment(Ff4WriterImpl *writer)
{
	uint32_t fragCount = 0;
	int32_t ret;

	if (writer == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_fragment_count(writer->models.moov, &fragCount);
	if (ret != FF4_OK)
		return ret;
	if (fragCount != 0)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_finalize_trex_defaults(&writer->models);
	if (ret == FF4_OK)
		ret = ff4_begin_movie_fragment(writer->sess, &writer->models);
	return ret;
}

int32_t ff4_begin_next_fragment(Ff4WriterImpl *writer)
{
	uint32_t fragCount = 0;
	int32_t ret;

	if (writer == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_get_fragment_count(writer->models.moov, &fragCount);
	if (ret != FF4_OK)
		return ret;
	if (fragCount == 0)
		return FF4_ERR_INVALID_STATE;

	return ff4_begin_movie_fragment(writer->sess, &writer->models);
}

int32_t ff4_set_writer_frag_base_offset(Ff4WriterImpl *writer, uint32_t fragIdx, uint64_t baseOff)
{
	if (writer == NULL || fragIdx == 0)
		return FF4_ERR_INVALID_ARG;
	return ff4_set_fragment_base_offset(&writer->models, fragIdx, baseOff);
}

int32_t ff4_set_writer_tfhd_override(Ff4WriterImpl *writer, uint32_t fragIdx, uint32_t trackHandle,
				     uint8_t mask, const void *params)
{
	if (writer == NULL || fragIdx == 0 || trackHandle == 0 || params == NULL ||
	    (mask & ~0x3Bu) != 0)
		return FF4_ERR_INVALID_ARG;
	return ff4_set_tfhd_override(&writer->models, fragIdx, trackHandle, mask, params);
}

int32_t ff4_write_moof(Ff4WriterImpl *writer, uint32_t fragIdx, void *fileCtx, uint64_t offset,
		       uint64_t *outWritten)
{
	Ff4MoofPlan *plan = NULL;
	int32_t allHaveChunks = 0;
	int32_t noPendingStsc = 0;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || fragIdx == 0 || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	ret = ff4_query_fragment_state(writer->sess, &writer->models, fragIdx,
				       &allHaveChunks, &noPendingStsc, NULL, NULL);
	if (ret != FF4_OK)
		return ret;
	if (allHaveChunks == 0 || noPendingStsc == 0)
		return FF4_ERR_INVALID_STATE;

	plan = (Ff4MoofPlan *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_plan_moof(writer->sess, &writer->scratchBuf, &writer->opts, fragIdx,
			    writer->models.moov, plan);
	if (ret == FF4_OK) {
		ret = ff4_write_moof_box(writer->sess, &writer->scratchBuf, &writer->opts,
					 fileCtx, offset, plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}

	ff4_free_moof_plan(writer->sess, plan);
	return ret;
}

int32_t ff4_write_mfra(Ff4WriterImpl *writer, void *fileCtx, uint64_t offset, uint64_t *outWritten)
{
	Ff4MfraPlan *plan = NULL;
	uint64_t written = 0;
	int32_t ret;

	if (writer == NULL || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	if (writer->models.moov == NULL)
		return FF4_ERR_INVALID_STATE;

	plan = (Ff4MfraPlan *)ff4_pool_alloc(writer->sess, FF4_POOL_META, sizeof(*plan));
	if (plan == NULL)
		return FF4_ERR_NO_MEM;

	ret = ff4_plan_mfra(writer->sess, &writer->scratchBuf, &writer->opts,
			    writer->models.moov, plan);
	if (ret == FF4_OK) {
		ret = ff4_write_mfra_box(writer->sess, &writer->scratchBuf, &writer->opts,
					 fileCtx, offset, plan, &written, NULL);
		if (ret == FF4_OK)
			*outWritten = written;
	}

	ff4_free_mfra_plan(writer->sess, plan);
	return ret;
}

int32_t ff4_clear_writer_fragment(Ff4WriterImpl *writer, uint32_t fragIdx)
{
	if (writer == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_clear_fragment_samples(writer->sess, &writer->models, fragIdx);
}

int32_t ff4_write_writer_fragment_cache(Ff4WriterImpl *writer, uint32_t fragIdx, void *fileCtx,
					uint64_t offset, uint64_t *outWritten)
{
	if (writer == NULL || fragIdx == 0 || outWritten == NULL)
		return FF4_ERR_INVALID_ARG;
	return ff4_write_fragment_cache(writer->sess, &writer->models, fragIdx, fileCtx,
					offset, outWritten);
}

int32_t ff4_read_writer_fragment_cache(Ff4WriterImpl *writer, uint32_t fragIdx, void *fileCtx,
				       uint64_t offset, uint64_t availSize)
{
	if (writer == NULL || fragIdx == 0 || fileCtx == NULL || availSize == 0)
		return FF4_ERR_INVALID_ARG;
	return ff4_read_fragment_cache(writer->sess, &writer->scratchBuf, &writer->models,
				       fragIdx, fileCtx, offset, availSize);
}
