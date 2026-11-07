/*
 * A reader of MPO files (CIPA DC-007, Multi-Picture Format): a JPEG's pictures, their MP types
 * and sizes, and the tags of each picture's APPn segments. The photo apps, the image dialogs and
 * jpeg_promoter build the same C library; no binary names it, its folder or its functions, so
 * all of them are ours.
 */
#ifndef MPO_MPO_H
#define MPO_MPO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* what the library returns */
#define MPO_ERROR_INVALID_ARGUMENT	((int)0x80000004)

/* an MP image's type, as the library returns it */
#define MPO_TYPE_PANORAMA	0x200001
#define MPO_TYPE_DISPARITY	0x200002
#define MPO_TYPE_MULTI_ANGLE	0x200003

typedef struct MpoHandle MpoHandle;

/* the memory and the file the library reads with */
typedef struct MpoCallbacks {
	void *(*alloc)(size_t size, void *arg);
	void *allocArg;
	int (*free)(void *ptr, void *arg);
	void *freeArg;
	void *reader;
	void *readerArg;
} MpoCallbacks;

/* a tag of the selected segment */
typedef struct MpoTagInfo {
	uint32_t type;
	uint32_t size;
	uint32_t count;
} MpoTagInfo;

int MpoClose(MpoHandle *mpo);
/* the MPO file at path, or the callbacks' reader without one */
int MpoOpen(MpoHandle **mpo, const MpoCallbacks *callbacks, const char *path);
int MpoGetImageCount(MpoHandle *mpo);
int MpoGetImageWidth(MpoHandle *mpo, int index);
int MpoGetImageHeight(MpoHandle *mpo, int index);
int MpoGetImageType(MpoHandle *mpo, int index);
/* the picture the others are shown from */
int MpoGetRepresentativeImage(MpoHandle *mpo);
int MpoSelectImage(MpoHandle *mpo, int index);
/* the selected picture's segment with marker */
int MpoSelectSegment(MpoHandle *mpo, uint16_t marker);
int MpoSelectIfd(MpoHandle *mpo, unsigned int ifd);
/* the tag's info, and its value into buf when there is one */
int MpoGetTag(MpoHandle *mpo, int tag, MpoTagInfo *info, void *buf, int size);

#ifdef __cplusplus
}
#endif

#endif
