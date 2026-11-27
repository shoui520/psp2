/*
 * SceLibPvf's rasterizer: its layer over libSceFt2's FreeType. It keeps the opened faces, sets
 * the size, skew and emboldening every glyph is rendered with, substitutes a glyph's vertical
 * form (pvf_gsub.c) and converts FreeType's bitmaps to the 4- and 8-bit images libpvf draws.
 *
 * No Sony record names this file or its functions; the names are ours (agent/namecheck).
 */

#include <stdarg.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <iofilemgr/iofilemgr_common.h>
#include <font/libpvf.h>
#include <libSceFt2/libSceFt2.h>
#include FT_GLYPH_H
#include FT_OUTLINE_H
#ifndef SCE_PVF_2014
#include FT_OPENTYPE_VALIDATE_H
#endif
#include FT_MODULE_H
#include "libpvf_internal.h"

#ifdef SCE_PVF_2014
#define sceClibMemcpy_safe	sceClibMemcpy
#endif

#define PVF_PI		3.141592653589793
#define PVF_2PI		6.283185307179586
#define PVF_PI_2	1.5707963267948966
/* degrees to radians */
#define PVF_DEG_TO_RAD	0.017453292519943295

/* FreeType's outline glyphs ('outl') */
#define PVF_GLYPH_FORMAT_OUTLINE	FT_GLYPH_FORMAT_OUTLINE

/* the file cache a font file is read through (ioctl 0x1001) */
#define PVF_FILE_CACHE_SYSTEM	0x4000	/* sa0:, the system fonts */
#define PVF_FILE_CACHE_USER	0x8000

/* a FreeType error as libpvf's */
#ifdef SCE_PVF_2014
static inline ScePvf_t_error PvfRasterizerError(FT_Error error)
#else
__attribute__((noinline)) ScePvf_t_error PvfRasterizerError(FT_Error error)
#endif
{
	switch (error) {
	case 0x00:			/* FT_Err_Ok */
		return 0;
	case 0x01:			/* Cannot_Open_Resource */
	case 0x51:			/* Cannot_Open_Stream */
		return SCE_PVF_ERROR_FILEOPEN;
	case 0x02:			/* Unknown_File_Format */
	case 0x03:			/* Invalid_File_Format */
	case 0x54: case 0x55: case 0x56: case 0x57: case 0x58:	/* stream reads */
		return SCE_PVF_ERROR_FILEREAD;
	case 0x04:			/* Invalid_Version */
	case 0x05:			/* Lower_Module_Version */
		return SCE_PVF_ERROR_ILLEGALVERSION;
	case 0x06:			/* Invalid_Argument */
		return SCE_PVF_ERROR_ARG;
	case 0x07:			/* Unimplemented_Feature */
	case 0x0A:			/* Missing_Property */
	case 0x10: case 0x11: case 0x12: case 0x13:	/* glyph and character errors */
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x61: case 0x62: case 0x63:	/* Raster errors */
	case 0x70:			/* Corrupted_Font_Header */
		return SCE_PVF_ERROR_NOSUPPORT;
	case 0x08:			/* Invalid_Table */
	case 0x09:			/* Invalid_Offset */
	case 0x20: case 0x21: case 0x22: case 0x23:	/* handle errors */
	case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
	case 0x30: case 0x31:		/* Too_Many_Drivers, Too_Many_Extensions */
	case 0x80: case 0x81: case 0x82: case 0x83:	/* TrueType and SFNT errors */
	case 0x84: case 0x85: case 0x86: case 0x87:
	case 0x88: case 0x89: case 0x8A: case 0x8B:
	case 0x8C: case 0x8D: case 0x8E: case 0x8F:
	case 0x90: case 0x91: case 0x92: case 0x93:
	case 0x94: case 0x95: case 0x96: case 0x97:
	case 0x98: case 0x99: case 0x9A: case 0x9B:
	case 0xA0: case 0xA1: case 0xA2: case 0xA3:	/* CFF, CID and Type 1 errors */
	case 0xB0: case 0xB1: case 0xB2: case 0xB3:	/* BDF errors */
	case 0xB4: case 0xB5: case 0xB6: case 0xB7:
	case 0xB8: case 0xB9: case 0xBA:
		return SCE_PVF_ERROR_DATAINCONSISTENT;
	case 0x40:			/* Out_Of_Memory */
	case 0x41:			/* Unlisted_Object */
		return SCE_PVF_ERROR_NOMEMORY;
	case 0x52:			/* Invalid_Stream_Seek */
	case 0x53:			/* Invalid_Stream_Skip */
		return SCE_PVF_ERROR_FILESEEK;
	default:
		return SCE_PVF_ERROR_UNKNOWN;
	}
}

#ifndef SCE_PVF_2014
/* the vertical-substitution table's memory: the library's */
static void *pvfGsubAlloc(void *user, ScePvf_t_u32 size)
{
	PvfRasterizer *rasterizer = user;

	return rasterizer->alloc(rasterizer->memUserData, size);
}

static void pvfGsubFree(void *user, void *ptr)
{
	PvfRasterizer *rasterizer = user;

	rasterizer->free(rasterizer->memUserData, ptr);
}
#endif

/* FreeType's memory object, and FreeType's memory through it */
static void *pvfFtAllocSelf(void *user, long size)
{
	PvfRasterizer *rasterizer = user;

	return rasterizer->alloc(rasterizer->userData, size);
}

static void *pvfFtAlloc(FT_Memory memory, long size)
{
	PvfRasterizer *rasterizer = memory->user;

	return rasterizer->alloc(rasterizer->userData, size);
}

static void pvfFtFree(FT_Memory memory, void *block)
{
	PvfRasterizer *rasterizer = memory->user;

	rasterizer->free(rasterizer->userData, block);
}

static void *pvfFtRealloc(FT_Memory memory, long curSize, long newSize, void *block)
{
	PvfRasterizer *rasterizer = memory->user;

	(void)curSize;
	return rasterizer->realloc(rasterizer->userData, block, newSize);
}

/* the rasterizer's messages, which the release build drops */
static void pvfRasterizerPrintf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	va_end(args);
}

#ifndef SCE_PVF_2014
/* a face's vertical substitutions, read from its GSUB table the first time they are needed */
static void pvfLoadVertTable(PvfRasterizer *rasterizer, ScePvf_t_s32 face)
{
	PvfFaceTable *table = rasterizer->table;
	FT_Bytes base, gdef, gpos, gsub, jstf;
	PvfGsubMemory memory;

	if (table->vertTables[face] != NULL)
		return;
	base = NULL;
	gdef = NULL;
	gpos = NULL;
	gsub = NULL;
	jstf = NULL;
	FT_OpenType_Validate(table->faces[face], FT_VALIDATE_GSUB, &base, &gdef, &gpos, &gsub,
		&jstf);
	memory.user = rasterizer;
	memory.alloc = pvfGsubAlloc;
	memory.free = pvfGsubFree;
	if (gsub != NULL)
		rasterizer->table->vertTables[face] = PvfGsubNew(&memory, gsub);
	if (base != NULL)
		rasterizer->free(rasterizer->memUserData, (void *)base);
	if (gdef != NULL)
		rasterizer->free(rasterizer->memUserData, (void *)gdef);
	if (gpos != NULL)
		rasterizer->free(rasterizer->memUserData, (void *)gpos);
	if (gsub != NULL)
		rasterizer->free(rasterizer->memUserData, (void *)gsub);
	if (jstf != NULL)
		rasterizer->free(rasterizer->memUserData, (void *)jstf);
}
#endif

/* x - 2pi n for the n that brings it into [-pi, pi] */
static inline float pvfReduceAngle(float x)
{
	float n, t;

	n = (float)((double)x / PVF_2PI);
	if (!(n < -2147483648.0f) && !(n > 2147483648.0f)) {
		t = (float)(int)n;
		if (n < 0.0f && n - t < 0.0f)
			t = t - 1.0f;
		n = t;
	}
	x = (float)((double)x - (double)(n + n) * PVF_PI);
	if ((double)x > PVF_PI)
		x = (float)((double)x - PVF_2PI);
	if ((double)x < -PVF_PI)
		x = (float)((double)x + PVF_2PI);
	return x;
}

/* sin(x) for x in [-pi/2, pi/2], Taylor's series to x^11 */
static inline float pvfSinSeries(float x)
{
	float x2 = x * x;
	float p;

	p = 2.75573188e-06f + x2 * -2.50521079e-08f;
	p = x2 * p - 0.000198412701f;
	p = 0.00833333377f + x2 * p;
	p = x2 * p - 0.166666672f;
	return x * (1.0f + x2 * p);
}

#ifdef SCE_PVF_2014
static __attribute__((noinline)) float pvfSin(float x)
#else
static float pvfSin(float x)
#endif
{
	x = pvfReduceAngle(x);
	if (x == 0.0f)
		return 0.0f;
	if ((double)x == PVF_PI_2)
		return 1.0f;
	if ((double)x == -PVF_PI_2)
		return -1.0f;
	if ((double)__builtin_fabsf(x) < PVF_PI_2)
		return pvfSinSeries(x);
	if (x < 0.0f)
		return pvfSinSeries((float)(-PVF_PI - (double)x));
	return pvfSinSeries((float)(PVF_PI - (double)x));
}

#ifdef SCE_PVF_2014
static __attribute__((noinline)) float pvfCos(float x)
#else
static float pvfCos(float x)
#endif
{
	x = pvfReduceAngle(x);
	if (x == 0.0f)
		return 1.0f;
	if ((double)x == PVF_PI_2 || (double)x == -PVF_PI_2)
		return 0.0f;
	if (x < 0.0f)
		return pvfSinSeries((float)((double)x + PVF_PI_2));
	return pvfSinSeries((float)(PVF_PI_2 - (double)x));
}

void PvfRasterizerSetLibCHook(void)
{
	__sceLibFt2SetLibCHook();
}

void PvfRasterizerDoneLibCHook(void)
{
	__sceLibFt2DoneLibCHook();
}

PvfRasterizer *PvfRasterizerNew(ScePvf_t_s32 maxFaces, void *userData, void *memUserData,
	void *(*alloc)(void *userData, ScePvf_t_u32 size),
	void *(*realloc)(void *userData, void *ptr, ScePvf_t_u32 size),
	void (*free)(void *userData, void *ptr))
{
	PvfRasterizer *rasterizer;
	PvfFaceTable *table;
	ScePvf_t_s32 i;

	__sceLibFt2SetLibCHook();
	if (maxFaces <= 0)
		return NULL;
	if (alloc == NULL || realloc == NULL)
		return NULL;
	if (free == NULL)
		return NULL;
	rasterizer = alloc(memUserData, sizeof(*rasterizer));
	if (rasterizer == NULL)
		return NULL;
	table = alloc(memUserData, sizeof(*table));
	rasterizer->table = table;
	if (table == NULL)
		goto free_rasterizer;
	table->maxFaces = maxFaces;
	table->numFaces = 0;
	table->faces = alloc(memUserData, maxFaces * sizeof(*table->faces));
	table->buffers = alloc(memUserData, maxFaces * sizeof(*table->buffers));
#ifndef SCE_PVF_2014
	table->vertTables = alloc(memUserData, maxFaces * sizeof(*table->vertTables));
	if (table->faces != NULL) {
		if (table->buffers != NULL && table->vertTables != NULL) {
			for (i = 0; i < maxFaces; i++) {
				table->faces[i] = NULL;
				table->buffers[i] = NULL;
				table->vertTables[i] = NULL;
			}
#else
	if (table->faces != NULL) {
		if (table->buffers != NULL) {
			for (i = 0; i < maxFaces; i++) {
				table->faces[i] = NULL;
				table->buffers[i] = NULL;
			}
#endif
			rasterizer->realloc = realloc;
			rasterizer->userData = userData;
			rasterizer->memUserData = memUserData;
			rasterizer->free = free;
			rasterizer->alloc = alloc;
			rasterizer->forceAutohint = 0;
			rasterizer->embolden = 0;
			rasterizer->slant = 0;
			rasterizer->skew.xy = 0;
			rasterizer->skew.yx = 0;
			rasterizer->antialias = 1;
			rasterizer->skew.yy = 0x10000;
			rasterizer->skew.xx = 0x10000;
			table->memory = SceFt2_04DCBE87(rasterizer, pvfFtAllocSelf, pvfFtAlloc,
				pvfFtRealloc, pvfFtFree);
			if (table->memory == NULL)
				goto free_rasterizer;
			if (FT_New_Library(table->memory, &table->library) == 0) {
				FT_Add_Default_Modules(table->library);
				return rasterizer;
			}
			free(memUserData, table->memory);
			goto free_rasterizer;
		}
		free(memUserData, table->faces);
	}
	if (table->buffers != NULL)
		free(memUserData, table->buffers);
#ifndef SCE_PVF_2014
	if (table->vertTables != NULL)
		free(memUserData, table->vertTables);
#endif
	free(memUserData, rasterizer->table);
free_rasterizer:
	free(memUserData, rasterizer);
	return NULL;
}

void PvfRasterizerDelete(PvfRasterizer *rasterizer)
{
	PvfFaceTable *table;
	ScePvf_t_s32 i;

	if (rasterizer == NULL)
		return;
	table = rasterizer->table;
	if (table->faces != NULL) {
		for (i = 0; i < table->maxFaces; i++) {
			if (table->faces[i] != NULL)
				FT_Done_Face(table->faces[i]);
		}
		rasterizer->free(rasterizer->memUserData, table->faces);
	}
	if (table->buffers != NULL) {
		for (i = 0; i < table->maxFaces; i++) {
			if (table->buffers[i] != NULL)
				rasterizer->free(rasterizer->memUserData, table->buffers[i]);
		}
		rasterizer->free(rasterizer->memUserData, table->buffers);
	}
#ifndef SCE_PVF_2014
	if (table->vertTables != NULL) {
		for (i = 0; i < table->maxFaces; i++) {
			if (table->vertTables[i] != NULL)
				PvfGsubDelete(table->vertTables[i]);
		}
		rasterizer->free(rasterizer->memUserData, table->vertTables);
	}
#endif
	FT_Done_FreeType(table->library);
	rasterizer->free(rasterizer->memUserData, rasterizer->table);
	rasterizer->free(rasterizer->memUserData, rasterizer);
}

/* the first unused face, or -1 */
static inline ScePvf_t_s32 pvfFreeFace(PvfFaceTable *table)
{
	ScePvf_t_s32 i;

	for (i = 0; i < table->maxFaces; i++) {
		if (table->faces[i] == NULL)
			return i;
	}
	return -1;
}

#ifdef SCE_PVF_2014
ScePvf_t_s32 PvfRasterizerOpenFile(PvfRasterizer *rasterizer, const char *path,
	ScePvf_t_s32 stream, ScePvf_t_error *errorCode)
{
	const ScePvf_t_s32 subFontIndex = 0;
#else
ScePvf_t_s32 PvfRasterizerOpenFile(PvfRasterizer *rasterizer, const char *path,
	ScePvf_t_s32 stream, ScePvf_t_s32 subFontIndex, ScePvf_t_error *errorCode)
{
#endif
	PvfFaceTable *table;
	VfsBuffCtlIoctlArg cache;
	SceIoStat stat;
	ScePvf_t_error error;
	ScePvf_t_s32 face;
	SceSize size;
	SceUID fd;
	FT_Error result;

	if (rasterizer == NULL)
		return -1;
	table = rasterizer->table;
	face = pvfFreeFace(table);
	if (face < 0)
		return -1;
	if (stream == 1) {
		if (FT_New_Face(table->library, path, subFontIndex, &table->faces[face]) == 0) {
			table->numFaces++;
			return face;
		}
		table->faces[face] = NULL;
		pvfRasterizerPrintf("Can not open font file (%s).\n", path);
		error = SCE_PVF_ERROR_FILEOPEN;
		goto fail;
	}
	error = SCE_PVF_ERROR_FILEOPEN;
	if (sceIoGetstat(path, &stat) != 0)
		goto fail;
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		pvfRasterizerPrintf("Can not open font file.\n");
		error = SCE_PVF_ERROR_FILEOPEN;
		goto fail;
	}
	sceClibPrintf("LIBPVF>>> FILE: %s\n", path);
	sceClibMemset(&cache, 0, sizeof(cache));
	cache.ioCacheSize = sceClibStrncmp(path, "sa0:", 4) != 0 ? PVF_FILE_CACHE_USER :
		PVF_FILE_CACHE_SYSTEM;
	cache.mask = 8;
	sceIoIoctl(fd, 0x1001, &cache, sizeof(cache), NULL, 0);
	size = stat.st_size;
	table->buffers[face] = rasterizer->alloc(rasterizer->userData, size);
	if (table->buffers[face] == NULL) {
		pvfRasterizerPrintf("Can not alloc memory.\n");
		sceIoClose(fd);
		error = SCE_PVF_ERROR_NOMEMORY;
		goto fail;
	}
	if (sceIoRead(fd, table->buffers[face], size) != size) {
		rasterizer->free(rasterizer->userData, table->buffers[face]);
		table->buffers[face] = NULL;
		sceIoClose(fd);
		error = SCE_PVF_ERROR_FILEREAD;
		goto fail;
	}
	if (sceIoClose(fd) < 0) {
		pvfRasterizerPrintf("Can not close font file.\n");
		rasterizer->free(rasterizer->userData, table->buffers[face]);
		error = SCE_PVF_ERROR_FILECLOSE;
		table->buffers[face] = NULL;
		goto fail;
	}
	result = FT_New_Memory_Face(table->library, table->buffers[face], size, subFontIndex,
		&table->faces[face]);
	if (result == 0) {
		table->numFaces++;
		return face;
	}
	table->faces[face] = NULL;
	rasterizer->free(rasterizer->userData, table->buffers[face]);
	table->buffers[face] = NULL;
	pvfRasterizerPrintf("Can not open font file (%s).\n", path);
	error = PvfRasterizerError(result);
	if (error == 0)
		return face;
fail:
	*errorCode = error;
	return -1;
}

#ifdef SCE_PVF_2014
ScePvf_t_s32 PvfRasterizerOpenMemory(PvfRasterizer *rasterizer, const void *buffer,
	ScePvf_t_s32 size, ScePvf_t_error *errorCode)
{
	const ScePvf_t_s32 subFontIndex = 0;
#else
ScePvf_t_s32 PvfRasterizerOpenMemory(PvfRasterizer *rasterizer, const void *buffer,
	ScePvf_t_s32 size, ScePvf_t_s32 subFontIndex, ScePvf_t_error *errorCode)
{
#endif
	PvfFaceTable *table;
	ScePvf_t_error error;
	ScePvf_t_s32 face;
	FT_Error result;

	if (rasterizer == NULL)
		return -1;
	table = rasterizer->table;
	face = pvfFreeFace(table);
	if (face < 0)
		return -1;
	result = FT_New_Memory_Face(table->library, buffer, size, subFontIndex,
		&table->faces[face]);
	if (result != 0) {
		table->faces[face] = NULL;
		pvfRasterizerPrintf("Can not open font file (%8.8x/%dbyte(s)).\n", buffer, size);
		error = PvfRasterizerError(result);
		if (error != 0) {
			face = -1;
			*errorCode = error;
		}
		return face;
	}
	table->numFaces++;
	return face;
}

ScePvf_t_error PvfRasterizerClose(PvfRasterizer *rasterizer, ScePvf_t_s32 face)
{
	PvfFaceTable *table;
	FT_Error error;

	if (face < 0)
		return SCE_PVF_ERROR_FILECLOSE;
	table = rasterizer->table;
	if (face >= table->maxFaces)
		return SCE_PVF_ERROR_FILECLOSE;
	error = 0;
	if (table->faces[face] != NULL) {
		error = FT_Done_Face(table->faces[face]);
		table->faces[face] = NULL;
	}
	if (table->buffers[face] != NULL) {
		rasterizer->free(rasterizer->memUserData, table->buffers[face]);
		table->buffers[face] = NULL;
	}
#ifndef SCE_PVF_2014
	if (table->vertTables[face] != NULL) {
		PvfGsubDelete(table->vertTables[face]);
		table->vertTables[face] = NULL;
	}
#endif
	table->numFaces--;
	return PvfRasterizerError(error);
}

#ifdef SCE_PVF_2014
void PvfRasterizerSetAntiAlias(PvfRasterizer *rasterizer, ScePvf_t_s32 enable)
{
	if (rasterizer != NULL)
		rasterizer->antialias = enable;
}

void PvfRasterizerSetForceAutohint(PvfRasterizer *rasterizer, ScePvf_t_s32 enable)
{
	if (rasterizer != NULL)
		rasterizer->forceAutohint = enable;
}

void PvfRasterizerSetResolution(PvfRasterizer *rasterizer, FT_UInt hResolution,
	FT_UInt vResolution)
{
	if (rasterizer != NULL) {
		rasterizer->hResolution = hResolution;
		rasterizer->vResolution = vResolution;
	}
}
#else
void PvfRasterizerSetResolution(PvfRasterizer *rasterizer, ScePvf_t_f32 hResolution,
	ScePvf_t_f32 vResolution)
{
	if (rasterizer != NULL) {
		rasterizer->hResolution = hResolution;
		rasterizer->vResolution = vResolution;
	}
}
#endif

void PvfRasterizerSetCharSize(PvfRasterizer *rasterizer, FT_F26Dot6 width, FT_F26Dot6 height)
{
	if (rasterizer != NULL) {
		rasterizer->charWidth = width;
		rasterizer->charHeight = height;
	}
}

void PvfRasterizerSetEmbolden(PvfRasterizer *rasterizer, FT_Pos embolden)
{
	if (rasterizer != NULL)
		rasterizer->embolden = embolden;
}

void PvfRasterizerSetSkew(PvfRasterizer *rasterizer, ScePvf_t_f32 angleX, ScePvf_t_f32 angleY)
{
	ScePvf_t_f32 matrix[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	ScePvf_t_f32 radX, radY, sinX, sinY, cosX, cosY;

	if (rasterizer == NULL)
		return;
	if (PVF_IS_NONZERO(angleX) || PVF_IS_NONZERO(angleY)) {
		radY = angleY * PVF_DEG_TO_RAD;
		radX = angleX * PVF_DEG_TO_RAD;
		sinY = pvfSin(radY);
		sinX = pvfSin(radX);
		cosY = pvfCos(radY);
		cosX = pvfCos(radX);
		matrix[0] = cosX * (1.0f / __builtin_fabsf(cosX));
		matrix[1] = sinY * (1.0f / __builtin_fabsf(cosY));
		matrix[2] = -(1.0f / __builtin_fabsf(cosX) * sinX);
		matrix[3] = cosY * (1.0f / __builtin_fabsf(cosY));
	}
	rasterizer->skew.xx = (FT_Fixed)(matrix[0] * 65536.0f);
	rasterizer->skew.xy = (FT_Fixed)(matrix[1] * 65536.0f);
	rasterizer->skew.yx = (FT_Fixed)(matrix[2] * 65536.0f);
	rasterizer->skew.yy = (FT_Fixed)(matrix[3] * 65536.0f);
}

void PvfRasterizerSetSlant(PvfRasterizer *rasterizer, FT_Fixed slant)
{
	if (rasterizer != NULL)
		rasterizer->slant = slant;
}

void PvfRasterizerGetFaceInfo(PvfRasterizer *rasterizer, ScePvf_t_s32 face, PvfFaceInfo *info)
{
	FT_Face ftFace = rasterizer->table->faces[face];

	info->styleFlags = ftFace->style_flags;
	info->faceFlags = ftFace->face_flags;
	info->numFaces = ftFace->num_faces;
#ifndef SCE_PVF_2014
	info->faceIndex = ftFace->face_index;
#endif
	info->numGlyphs = ftFace->num_glyphs;
	info->numCharmaps = ftFace->num_charmaps;
	sceClibStrncpy(info->familyName, ftFace->family_name, sizeof(info->familyName) - 1);
	info->familyName[sizeof(info->familyName) - 1] = '\0';
	sceClibStrncpy(info->styleName, ftFace->style_name, sizeof(info->styleName) - 1);
	info->styleName[sizeof(info->styleName) - 1] = '\0';
	info->unitsPerEM = ftFace->units_per_EM;
	info->ascender = ftFace->ascender;
	info->descender = ftFace->descender;
	info->height = ftFace->height;
	info->maxAdvanceWidth = ftFace->max_advance_width;
	info->maxAdvanceHeight = ftFace->max_advance_height;
	info->underlinePosition = ftFace->underline_position;
	info->underlineThickness = ftFace->underline_thickness;
}

ScePvf_t_bool PvfRasterizerHasChar(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode)
{
	if (rasterizer == NULL)
		return 0;
	return FT_Get_Char_Index(rasterizer->table->faces[face], charCode) != 0;
}

#ifndef SCE_PVF_2014
ScePvf_t_bool PvfRasterizerHasVertChar(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode)
{
	FT_UInt glyph;

	if (rasterizer == NULL)
		return 0;
	glyph = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (glyph == 0)
		return 0;
	pvfLoadVertTable(rasterizer, face);
	if (rasterizer->table->vertTables[face] == NULL)
		return 0;
	return PvfGsubGetVertGlyph(rasterizer->table->vertTables[face], glyph) >= 0;
}
#endif

/* the face at the rasterizer's size */
static inline FT_Error pvfSetCharSize(PvfRasterizer *rasterizer, FT_Face face)
{
	return FT_Set_Char_Size(face, rasterizer->charWidth, rasterizer->charHeight,
		(FT_UInt)rasterizer->hResolution, (FT_UInt)rasterizer->vResolution);
}

/* how a glyph is loaded */
static inline FT_Int32 pvfLoadFlags(PvfRasterizer *rasterizer)
{
	return rasterizer->forceAutohint != 0 ? FT_LOAD_FORCE_AUTOHINT : FT_LOAD_NO_HINTING;
}

#ifndef SCE_PVF_2014
ScePvf_t_error PvfRasterizerGetKerning(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode left, ScePvf_t_charCode right, ScePvf_t_iKerningInfo *kerning)
{
	FT_Vector delta = { 0, 0 };
	FT_Face ftFace;
	FT_UInt leftGlyph, rightGlyph;
	FT_Error error;

	if (rasterizer == NULL)
		return 0;
	ftFace = rasterizer->table->faces[face];
	if (!FT_HAS_KERNING(ftFace) || right == 0 || left == 0) {
		kerning->xOffset64 = 0;
		kerning->yOffset64 = 0;
		return 0;
	}
	leftGlyph = FT_Get_Char_Index(ftFace, left);
	rightGlyph = FT_Get_Char_Index(ftFace, right);
	pvfSetCharSize(rasterizer, ftFace);
	error = FT_Get_Kerning(ftFace, leftGlyph, rightGlyph, FT_KERNING_UNFITTED, &delta);
	if (error != 0)
		return PvfRasterizerError(error);
	kerning->xOffset64 = delta.x;
	kerning->yOffset64 = delta.y;
	return 0;
}
#endif

ScePvf_t_error PvfRasterizerGetGlyphMetrics(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, FT_Glyph_Metrics *metrics)
{
	FT_UInt glyph;
	FT_Error error;

	if (rasterizer == NULL || metrics == NULL)
		return SCE_PVF_ERROR_ARG;
	glyph = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (glyph == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	error = pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	if (error != 0)
		return PvfRasterizerError(error);
	error = FT_Load_Glyph(rasterizer->table->faces[face], glyph, pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	*metrics = rasterizer->table->faces[face]->glyph->metrics;
	return 0;
}

#ifndef SCE_PVF_2014
/* the glyph of charCode, its vertical form if the face has one (and *vertical then 1) */
static inline FT_UInt pvfVertGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face, FT_UInt glyph,
	ScePvf_t_s32 *vertical)
{
	struct PvfGsub *vertTable;
	FT_Int vertGlyph;

	pvfLoadVertTable(rasterizer, face);
	if (vertical != NULL)
		*vertical = 0;
	vertTable = rasterizer->table->vertTables[face];
	if (vertTable != NULL) {
		vertGlyph = PvfGsubGetVertGlyph(vertTable, glyph);
		if (vertGlyph >= 0) {
			glyph = vertGlyph;
			if (vertical != NULL)
				*vertical = 1;
		}
	}
	return glyph;
}

ScePvf_t_error PvfRasterizerGetVertGlyphMetrics(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, FT_Glyph_Metrics *metrics, ScePvf_t_s32 *vertical)
{
	FT_UInt glyph;
	FT_Error error;

	if (rasterizer == NULL || metrics == NULL)
		return SCE_PVF_ERROR_ARG;
	glyph = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (glyph == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	glyph = pvfVertGlyph(rasterizer, face, glyph, vertical);
	error = pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	if (error != 0)
		return PvfRasterizerError(error);
	error = FT_Load_Glyph(rasterizer->table->faces[face], glyph, pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	*metrics = rasterizer->table->faces[face]->glyph->metrics;
	return 0;
}
#endif

/* the loaded glyph's outline emboldened and skewed (or slanted) */
static inline void pvfTransformGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face)
{
	FT_Matrix matrix;

	FT_Outline_Embolden(&rasterizer->table->faces[face]->glyph->outline,
		rasterizer->embolden);
	if (rasterizer->slant != 0) {
		matrix.xx = 0x10000;
		matrix.xy = rasterizer->slant;
		matrix.yx = 0;
		matrix.yy = 0x10000;
	} else {
		matrix = rasterizer->skew;
	}
	FT_Outline_Transform(&rasterizer->table->faces[face]->glyph->outline, &matrix);
}

#ifdef SCE_PVF_2014
ScePvf_t_error PvfRasterizerGetOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, ScePvf_t_s32 *numContours, ScePvf_t_s32 **contours,
	PvfOutlinePoint **points)
{
	FT_Glyph glyph;
	FT_UInt index;
	FT_Int32 loadFlags;
	FT_Error error;

	if (rasterizer == NULL || numContours == NULL || contours == NULL || points == NULL)
		return SCE_PVF_ERROR_ARG;
	index = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (index == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	loadFlags = rasterizer->forceAutohint != 0 ?
		(FT_LOAD_FORCE_AUTOHINT | FT_LOAD_NO_SCALE) :
		(FT_LOAD_NO_HINTING | FT_LOAD_NO_SCALE);
	error = FT_Load_Glyph(rasterizer->table->faces[face], index, loadFlags);
	if (error != 0)
		return PvfRasterizerError(error);
	error = FT_Get_Glyph(rasterizer->table->faces[face]->glyph, &glyph);
	if (error != 0) {
		FT_Done_Glyph(glyph);
		return PvfRasterizerError(error);
	}
	if (glyph->format == PVF_GLYPH_FORMAT_OUTLINE) {
		FT_Outline *outline = &((FT_OutlineGlyph)glyph)->outline;
		FT_Vector *srcPoints = outline->points;
		char *srcTags = outline->tags;
		short *srcContours = outline->contours;
		int i;

		*numContours = outline->n_contours;
		*contours = rasterizer->alloc(rasterizer->userData,
			outline->n_contours * sizeof(**contours));
		if (*contours != NULL) {
			*points = rasterizer->alloc(rasterizer->userData,
				outline->n_points * sizeof(**points));
			if (*points != NULL) {
				for (i = 0; i < outline->n_contours; i++)
					(*contours)[i] = srcContours[i];
				for (i = 0; i < outline->n_points; i++) {
					(*points)[i].x = srcPoints[i].x;
					(*points)[i].y = srcPoints[i].y;
					(*points)[i].tag = srcTags[i];
				}
			} else {
				rasterizer->free(rasterizer->userData, contours);
			}
		}
	} else {
		pvfRasterizerPrintf("NOT OUTLINE\n");
	}
	FT_Done_Glyph(glyph);
	return 0;
}

ScePvf_t_error PvfRasterizerFreeOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 *contours,
	PvfOutlinePoint *points)
{
	if (rasterizer == NULL)
		return SCE_PVF_ERROR_ARG;
	if (contours != NULL)
		rasterizer->free(rasterizer->memUserData, contours);
	if (points != NULL)
		rasterizer->free(rasterizer->memUserData, points);
	return 0;
}
#else
/* a copy of an outline glyph's outline, in the library's memory */
static inline ScePvf_t_error pvfCopyOutline(PvfRasterizer *rasterizer, FT_Glyph glyph,
	int *flags, short *numContours, short *numPoints, short **contours, FT_Vector **points,
	char **tags)
{
	FT_Outline *outline = &((FT_OutlineGlyph)glyph)->outline;
	FT_Vector *srcPoints;
	char *srcTags;
	short *srcContours;
	int i;

	if (glyph->format != PVF_GLYPH_FORMAT_OUTLINE) {
		FT_Done_Glyph(glyph);
		return SCE_PVF_ERROR_NOSUPPORT;
	}
	srcPoints = outline->points;
	srcTags = outline->tags;
	srcContours = outline->contours;
	*numContours = outline->n_contours;
	*numPoints = outline->n_points;
	*contours = NULL;
	*points = NULL;
	*tags = NULL;
	*contours = rasterizer->alloc(rasterizer->userData,
		outline->n_contours * sizeof(**contours));
	if (*contours == NULL)
		goto nomemory;
	*points = rasterizer->alloc(rasterizer->userData, outline->n_points * sizeof(**points));
	if (*points == NULL)
		goto free_contours;
	*tags = rasterizer->alloc(rasterizer->userData, outline->n_points);
	if (*tags == NULL) {
		rasterizer->free(rasterizer->userData, *points);
		goto free_contours;
	}
	*flags = outline->flags;
	for (i = 0; i < outline->n_contours; i++)
		(*contours)[i] = srcContours[i];
	for (i = 0; i < outline->n_points; i++) {
		(*points)[i] = srcPoints[i];
		(*tags)[i] = srcTags[i];
	}
	FT_Done_Glyph(glyph);
	return 0;
free_contours:
	rasterizer->free(rasterizer->userData, *contours);
nomemory:
	FT_Done_Glyph(glyph);
	return SCE_PVF_ERROR_NOMEMORY;
}

ScePvf_t_error PvfRasterizerGetOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, int *flags, short *numContours, short *numPoints,
	short **contours, FT_Vector **points, char **tags)
{
	FT_Glyph glyph;
	FT_UInt index;
	FT_Error error;

	if (rasterizer == NULL || numContours == NULL || contours == NULL || points == NULL)
		return SCE_PVF_ERROR_ARG;
	index = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (index == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	error = FT_Load_Glyph(rasterizer->table->faces[face], index, pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	pvfTransformGlyph(rasterizer, face);
	error = FT_Get_Glyph(rasterizer->table->faces[face]->glyph, &glyph);
	if (error != 0) {
		FT_Done_Glyph(glyph);
		return PvfRasterizerError(error);
	}
	return pvfCopyOutline(rasterizer, glyph, flags, numContours, numPoints, contours, points,
		tags);
}

ScePvf_t_error PvfRasterizerGetVertOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, int *flags, short *numContours, short *numPoints,
	short **contours, FT_Vector **points, char **tags, ScePvf_t_s32 *vertical)
{
	FT_Glyph glyph;
	FT_UInt index;
	FT_Error error;

	if (rasterizer == NULL || numContours == NULL || contours == NULL || points == NULL)
		return SCE_PVF_ERROR_ARG;
	index = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (index == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	index = pvfVertGlyph(rasterizer, face, index, vertical);
	pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	error = FT_Load_Glyph(rasterizer->table->faces[face], index, pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	pvfTransformGlyph(rasterizer, face);
	error = FT_Get_Glyph(rasterizer->table->faces[face]->glyph, &glyph);
	if (error != 0) {
		FT_Done_Glyph(glyph);
		return PvfRasterizerError(error);
	}
	return pvfCopyOutline(rasterizer, glyph, flags, numContours, numPoints, contours, points,
		tags);
}

ScePvf_t_error PvfRasterizerFreeOutline(PvfRasterizer *rasterizer, short *contours,
	FT_Vector *points, char *tags)
{
	if (rasterizer == NULL)
		return SCE_PVF_ERROR_ARG;
	if (contours != NULL)
		rasterizer->free(rasterizer->memUserData, contours);
	if (points != NULL)
		rasterizer->free(rasterizer->memUserData, points);
	if (tags != NULL)
		rasterizer->free(rasterizer->memUserData, tags);
	return 0;
}
#endif

/* a FreeType bitmap as one byte a pixel (depth8) or two pixels a byte, in the library's memory */
static inline ScePvf_t_error pvfConvertBitmap(PvfRasterizer *rasterizer, FT_Bitmap *bitmap,
	ScePvf_t_s32 depth8, ScePvf_t_s32 *pitch, ScePvf_t_u8 **buffer)
{
	const ScePvf_t_u8 *src, *s;
	ScePvf_t_u8 *dst, *d;
	ScePvf_t_s32 rows, width, srcPitch, x, y, bit;
	signed char bits;
	ScePvf_t_u8 gray;

	rows = bitmap->rows;
	width = bitmap->width;
	src = bitmap->buffer;
	srcPitch = bitmap->pitch;
	if (depth8) {
		if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
			dst = rasterizer->alloc(rasterizer->userData, width * rows);
			*buffer = dst;
			if (pitch != NULL)
				*pitch = width;
			if (dst == NULL)
				return SCE_PVF_ERROR_NOMEMORY;
			for (y = 0; y < rows; y++) {
				s = src;
				bits = *s;
				bit = 0;
				for (x = 0; x < width; x++) {
					*dst++ = bits >> 7;
					if ((++bit & 7) != 0) {
						bits <<= 1;
					} else {
						bits = *++s;
						bit = 0;
					}
				}
				src += srcPitch;
			}
			return 0;
		}
		if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) {
			*buffer = NULL;
			return 0;
		}
		dst = rasterizer->alloc(rasterizer->userData, srcPitch * rows);
		*buffer = dst;
		if (dst == NULL)
			return SCE_PVF_ERROR_NOMEMORY;
		sceClibMemcpy_safe(dst, src, srcPitch * rows);
		return 0;
	}
	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
		*pitch = (width + 1) / 2;
		dst = rasterizer->alloc(rasterizer->userData, *pitch * rows);
		*buffer = dst;
		if (dst == NULL)
			return SCE_PVF_ERROR_NOMEMORY;
		for (y = 0; y < rows; y++) {
			s = src;
			bits = *s;
			d = dst;
			*d = 0;
			bit = 0;
			for (x = 0; x < width; x++) {
				if (x & 1) {
					*d = ((bits >> 7) & 0xF0) | (*d & 0x0F);
					d++;
				} else {
					*d = ((bits >> 7) & 0x0F) | (*d & 0xF0);
				}
				if ((++bit & 7) != 0) {
					bits <<= 1;
				} else {
					bits = *++s;
					bit = 0;
				}
			}
			if (width & 1)
				*d &= 0x0F;
			src += srcPitch;
			dst += *pitch;
		}
		return 0;
	}
	if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY) {
		*buffer = NULL;
		return 0;
	}
	*pitch = (width + 1) / 2;
	dst = rasterizer->alloc(rasterizer->userData, *pitch * rows);
	*buffer = dst;
	if (dst == NULL)
		return SCE_PVF_ERROR_NOMEMORY;
	for (y = 0; y < rows; y++) {
		d = dst;
		*d = 0;
		for (x = 0; x < width; x++) {
			gray = (src[x] + 8) / 17;
			if (x & 1) {
				*d = (*d & 0x0F) | (gray << 4);
				d++;
			} else {
				*d = gray | (*d & 0xF0);
			}
		}
		if (width & 1)
			*d &= 0x0F;
		src += srcPitch;
		dst += *pitch;
	}
	return 0;
}

/* the loaded glyph rendered, its placement and (if buffer) its image */
static inline ScePvf_t_error pvfRenderGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_s32 depth8, ScePvf_t_s32 *width, ScePvf_t_s32 *rows, ScePvf_t_s32 *top,
	ScePvf_t_s32 *left, ScePvf_t_s32 *pitch, ScePvf_t_u8 **buffer)
{
	FT_BitmapGlyph bitmapGlyph;
	FT_Vector origin;
	FT_Glyph glyph;
	FT_Error error;
	ScePvf_t_error result;

	pvfTransformGlyph(rasterizer, face);
	error = FT_Get_Glyph(rasterizer->table->faces[face]->glyph, &glyph);
	if (error == 0) {
		origin.x = 0;
		origin.y = 0;
		error = FT_Glyph_To_Bitmap(&glyph, rasterizer->antialias != 0 ?
			FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, &origin, 1);
	}
	if (error != 0) {
		FT_Done_Glyph(glyph);
		return PvfRasterizerError(error);
	}
	bitmapGlyph = (FT_BitmapGlyph)glyph;
	if (width != NULL)
		*width = bitmapGlyph->bitmap.width;
	if (rows != NULL)
		*rows = bitmapGlyph->bitmap.rows;
	if (pitch != NULL)
		*pitch = bitmapGlyph->bitmap.pitch;
	if (left != NULL)
		*left = bitmapGlyph->left;
	if (top != NULL)
		*top = bitmapGlyph->top;
	result = 0;
	if (buffer != NULL)
		result = pvfConvertBitmap(rasterizer, &bitmapGlyph->bitmap, depth8, pitch, buffer);
	FT_Done_Glyph(glyph);
	return result;
}

ScePvf_t_error PvfRasterizerRenderGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, ScePvf_t_s32 depth8, ScePvf_t_s32 *width,
	ScePvf_t_s32 *rows, ScePvf_t_s32 *top, ScePvf_t_s32 *left, ScePvf_t_s32 *pitch,
	ScePvf_t_u8 **buffer)
{
	FT_UInt glyph;
	FT_Error error;

	if (rasterizer == NULL)
		return SCE_PVF_ERROR_ARG;
	glyph = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (glyph == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	error = pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	if (error == 0)
		error = FT_Load_Glyph(rasterizer->table->faces[face], glyph,
			pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	return pvfRenderGlyph(rasterizer, face, depth8, width, rows, top, left, pitch, buffer);
}

#ifndef SCE_PVF_2014
ScePvf_t_error PvfRasterizerRenderVertGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, ScePvf_t_s32 depth8, ScePvf_t_s32 *width,
	ScePvf_t_s32 *rows, ScePvf_t_s32 *top, ScePvf_t_s32 *left, ScePvf_t_s32 *pitch,
	ScePvf_t_u8 **buffer, ScePvf_t_s32 *vertical)
{
	FT_UInt glyph;
	FT_Error error;

	if (rasterizer == NULL)
		return SCE_PVF_ERROR_ARG;
	glyph = FT_Get_Char_Index(rasterizer->table->faces[face], charCode);
	if (glyph == 0)
		return SCE_PVF_ERROR_NOGLYPH;
	glyph = pvfVertGlyph(rasterizer, face, glyph, vertical);
	error = pvfSetCharSize(rasterizer, rasterizer->table->faces[face]);
	if (error == 0)
		error = FT_Load_Glyph(rasterizer->table->faces[face], glyph,
			pvfLoadFlags(rasterizer));
	if (error != 0)
		return PvfRasterizerError(error);
	return pvfRenderGlyph(rasterizer, face, depth8, width, rows, top, left, pitch, buffer);
}
#endif

ScePvf_t_error PvfRasterizerFreeBitmap(PvfRasterizer *rasterizer, ScePvf_t_u8 *buffer)
{
	if (rasterizer == NULL || buffer == NULL)
		return SCE_PVF_ERROR_ARG;
	rasterizer->free(rasterizer->memUserData, buffer);
	return 0;
}
