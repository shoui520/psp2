/*
 * SceCamImportDialogPlugin: ScePictureFileCreator (JPEG/BMP/RAW image encoder,
 * EXIF/GPS/MakerNote writer, overlay alpha-blending, and photo0 export).
 */
#ifndef CAMERAIMPORT_DIALOG_PICTURE_FILE_CREATOR_H
#define CAMERAIMPORT_DIALOG_PICTURE_FILE_CREATOR_H

#include <stdint.h>
#include <scetypes.h>

class ImageEncodeJobItem;

/* ScePictureFileCreator error codes */
#define PIC_CREATOR_ERROR_CANCELED		((int)0x80103001)
#define PIC_CREATOR_ERROR_INVALID_ARGUMENT	((int)0x80103002)
#define PIC_CREATOR_ERROR_OUT_OF_MEMORY		((int)0x80103003)
#define PIC_CREATOR_ERROR_IO_ERROR		((int)0x80103004)
#define PIC_CREATOR_ERROR_GENERIC		((int)0x80103005)
#define PIC_CREATOR_ERROR_INVALID_STATE		((int)0x80103006)

/* Image output formats */
enum {
	PIC_FORMAT_JPEG = 0,
	PIC_FORMAT_BMP = 1,
	PIC_FORMAT_RAW = 2
};

/* Global ScePictureFileCreator & PhotoExport lifecycle */
int PictureFileCreatorInit(void);
int PictureFileCreatorTerm(void);
int PictureFileCreatorEncode(ImageEncodeJobItem *job);

int PhotoExportInit(void);
int PhotoExportTerm(void);
int PhotoExportRegisterFile(const char *filePath, uint64_t *outPhotoId);

#endif /* CAMERAIMPORT_DIALOG_PICTURE_FILE_CREATOR_H */
