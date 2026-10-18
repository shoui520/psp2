/*
 * SceSblSpfsoMgr (file name unknown): SPFSO files, signed SCE containers
 * read into a memory block and checked by SceSblAuthMgr.
 */

#include <iofilemgr/iofilemgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/sysmem.h>
#include "post_ss_mgr_internal.h"
#include <sysmem/qafmgr.h>
#include <authmgr/sblauthmgr.h>

#define SCE_SBL_ERROR_SPFSO_NO_MEMORY		0x800f1b0c	/* names unknown */
#define SCE_SBL_ERROR_SPFSO_INVALID_ARGUMENT	0x800f1b16
#define SCE_SBL_ERROR_SPFSO_INVALID_FORMAT	0x800f1b24

#define SPFSO_SIZE_MAX		0x8000	/* names unknown */
#define SPFSO_MEMBLOCK_TYPE	0x1020d006
#define SPFSO_PATH_MAX		0x100

#define SCE_HEADER_MAGIC	0x454353	/* "SCE" */
#define SCE_HEADER_VERSION	3
#define SCE_HEADER_TYPE_SPFSO	6		/* name unknown */
#define SCE_HEADER_ATTR_SPFSO	0x40		/* name unknown */

#define SPFSO_SEGMENT_TYPE	1		/* names unknown */
#define SPFSO_SEGMENT_HEADER	0x200

/* the SCE header of an SPFSO (names unknown) */
typedef struct SceSpfsoHeader {
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt8 attribute;
	SceUInt8 reserved;
	SceUInt16 headerType;
	SceUInt32 metadataOffset;
	SceUInt64 headerSize;
	SceUInt64 dataSize;
	SceUInt64 fileSize;
} SceSpfsoHeader;

/* what follows the header: the payload after a 0x200-byte segment header (names unknown) */
typedef struct SceSpfsoSegment {
	SceUInt32 type;
	SceUInt32 headerSize;
	SceUInt32 size;
} SceSpfsoSegment;

int SceSblPostSsMgrForDriver_BDF18922(const char *path, SceSblSpfsoCtx *ctx)
{
	SceSize size, done;
	SceOff end;
	SceUID fd, uid;
	int n;

	if (ctx == NULL || path == NULL)
		return SCE_SBL_ERROR_SPFSO_INVALID_FORMAT;
	ctx->uid = 0;
	ctx->base = NULL;
	ctx->size = 0;
	if ((strncmp(path, "gro0:", 5) != 0 && strncmp(path, "ur0:", 4) != 0 &&
	     strncmp(path, "ux0:", 4) != 0 &&
	     (!SceQafMgrForDriver_082A4FC2() || SceSblAIMgrForDriver_D78B04A2() ||
	      strncmp(path, "host0:", 6) != 0)) ||
	    strnlen(path, SPFSO_PATH_MAX) >= SPFSO_PATH_MAX)
		return SCE_SBL_ERROR_SPFSO_INVALID_ARGUMENT;

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return SCE_ERROR_ERRNO_ENOENT;
	end = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (end < 0)
		goto io_error;
	if (end > SPFSO_SIZE_MAX) {
		sceIoClose(fd);
		return SCE_SBL_ERROR_SPFSO_INVALID_FORMAT;
	}
	size = end;
	uid = SceSysmemForDriver_C94850C9("SceSblSpsfoMgr", SPFSO_MEMBLOCK_TYPE,
				      (size + 0xfff) & ~0xfff, NULL);
	ctx->uid = uid;
	if (uid < 0 || SceSysmemForDriver_A841EDDA(uid, &ctx->base) != 0) {
		sceIoClose(fd);
		return SCE_SBL_ERROR_SPFSO_NO_MEMORY;
	}
	ctx->size = size;
	if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0)
		goto io_error;
	for (done = 0; done < ctx->size; done += n) {
		n = sceIoRead(fd, (char *)ctx->base + done, ctx->size - done);
		if (n <= 0)
			goto io_error;
	}
	sceIoClose(fd);
	return 0;

io_error:
	sceIoClose(fd);
	return SCE_ERROR_ERRNO_EIO;
}

int SceSblPostSsMgrForDriver_AD3B0078(SceSblSpfsoCtx *ctx)
{
	if (ctx == NULL)
		return SCE_SBL_ERROR_SPFSO_INVALID_ARGUMENT;
	if (ctx->uid > 0)
		SceSysmemForDriver_009E1C61(ctx->uid);
	return 0;
}

/* check the context and give its payload */
int SceSblPostSsMgrForDriver_686B9461(SceSblSpfsoCtx *ctx, void **data, SceSize *size)
{
	const SceSpfsoHeader *header;
	const SceSpfsoSegment *segment;
	int ret;

	if (ctx == NULL || data == NULL || size == NULL || ctx->base == NULL ||
	    ctx->size > SPFSO_SIZE_MAX)
		return SCE_SBL_ERROR_SPFSO_INVALID_ARGUMENT;
	header = ctx->base;
	if (header->magic != SCE_HEADER_MAGIC || header->version != SCE_HEADER_VERSION ||
	    !(header->attribute & SCE_HEADER_ATTR_SPFSO) ||
	    header->headerType != SCE_HEADER_TYPE_SPFSO ||
	    header->headerSize >= header->fileSize || header->dataSize >= header->fileSize ||
	    header->fileSize != header->headerSize + header->dataSize ||
	    header->fileSize != ctx->size)
		return SCE_SBL_ERROR_SPFSO_INVALID_FORMAT;
	segment = (const SceSpfsoSegment *)((const char *)header + header->headerSize);
	if (segment == NULL || segment->type != SPFSO_SEGMENT_TYPE ||
	    segment->headerSize != SPFSO_SEGMENT_HEADER ||
	    segment->size != header->fileSize - SPFSO_SEGMENT_HEADER - header->headerSize)
		return SCE_SBL_ERROR_SPFSO_INVALID_FORMAT;
	ret = sceSblAuthMgrVerifySpsfo(ctx);
	if (ret != 0)
		return ret;
	*data = (char *)ctx->base + (SceSize)header->headerSize + SPFSO_SEGMENT_HEADER;
	*size = segment->size;
	return 0;
}
