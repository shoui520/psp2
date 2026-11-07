/*
 * The MakerNote Sony's PlayStation software writes into a photo's EXIF: a "SONY PS" header and
 * ID3v2-style frames, the text frames (TXXX) carrying the screenshot's tags (the game's name,
 * description and content ID). The photo apps, the image dialogs and the media promoters
 * (jpeg_promoter, png_promoter) build the same code from vsh/app/common/src/photo_makernote, the
 * folder their paths name; the file names, classes and methods are ours.
 */
#ifndef PHOTO_MAKERNOTE_PHOTO_MAKERNOTE_H
#define PHOTO_MAKERNOTE_PHOTO_MAKERNOTE_H

#include <stddef.h>
#include <stdint.h>
#include <std/stdlib.h>
#include <std/list>

/* what the parser returns */
#define PHOTO_MAKERNOTE_ERROR_INVALID_ARGUMENT	((int)0x80000001)
#define PHOTO_MAKERNOTE_ERROR_INVALID_DATA	((int)0x80000002)
#define PHOTO_MAKERNOTE_ERROR_NOT_FOUND		((int)0x80000003)
#define PHOTO_MAKERNOTE_ERROR_PARSE		((int)0x8000000F)

/* the frame the text tags are in */
#define PHOTO_MAKERNOTE_FRAME_TXXX		0x54585858	/* 'TXXX' */

/* the tags of the text frames: text tags */
#define PHOTO_MAKERNOTE_TAG_ALBM		0x414C424D	/* 'ALBM' */
#define PHOTO_MAKERNOTE_TAG_GCID		0x47434944	/* 'GCID' */
#define PHOTO_MAKERNOTE_TAG_GDSC		0x47445343	/* 'GDSC' */
#define PHOTO_MAKERNOTE_TAG_GPNM		0x47504E4D	/* 'GPNM' */
#define PHOTO_MAKERNOTE_TAG_OCOM		0x4F434F4D	/* 'OCOM' */
#define PHOTO_MAKERNOTE_TAG_PEFV		0x50454656	/* 'PEFV' */
#define PHOTO_MAKERNOTE_TAG_PFID		0x50464944	/* 'PFID' */
#define PHOTO_MAKERNOTE_TAG_PSIS		0x50534953	/* 'PSIS' */
#define PHOTO_MAKERNOTE_TAG_PTYP		0x50545950	/* 'PTYP' */
/* and value tags */
#define PHOTO_MAKERNOTE_TAG_ALAC		0x414C4143	/* 'ALAC' */
#define PHOTO_MAKERNOTE_TAG_IDAC		0x49444143	/* 'IDAC' */
#define PHOTO_MAKERNOTE_TAG_LLAC		0x4C4C4143	/* 'LLAC' */
#define PHOTO_MAKERNOTE_TAG_PHFR		0x50484652	/* 'PHFR' */
#define PHOTO_MAKERNOTE_TAG_PVFR		0x50564652	/* 'PVFR' */
#define PHOTO_MAKERNOTE_TAG_PYPR		0x50595052	/* 'PYPR' */

/* what a tag is read as, for PhotoMakerNote::IsTag */
enum {
	PHOTO_MAKERNOTE_TAG_KIND_TEXT = 0,
	PHOTO_MAKERNOTE_TAG_KIND_VALUE = 1,
	PHOTO_MAKERNOTE_TAG_KIND_ANY = 2
};

/* one frame: its ID, its flags and a copy of its data */
class PhotoMakerNoteFrame {
public:
	PhotoMakerNoteFrame();
	~PhotoMakerNoteFrame();

	/* the frame at data: its length with the header, or -1 */
	int Parse(const uint8_t *data, uint32_t size);

	void Reset()
	{
		if (m_owned) {
			if (m_data != NULL)
				sce_paf_free(m_data);
		}
		m_id[0] = 0;
		m_owned = false;
		m_size = 0;
		m_data = NULL;
		m_flags = 0;
	}

	char m_id[4];
	uint32_t m_size;
	uint8_t *m_data;
	uint16_t m_flags;
	/* m_data was allocated by Parse */
	bool m_owned;
};

/* a text frame's content */
class PhotoMakerNoteText {
public:
	PhotoMakerNoteText() : tag(0), text(NULL), length(0), valid(false)
	{
		language[0] = 0;
	}

	uint32_t tag;
	char language[3];
	const char *text;
	uint32_t length;
	bool valid;
};

class PhotoMakerNote {
public:
	PhotoMakerNote();
	virtual ~PhotoMakerNote();

	/* the frame at *offset of data, appended; *offset moves past it */
	virtual int ParseFrame(const uint8_t *data, uint32_t *offset, uint32_t size);

	void Clear();
	/* the MakerNote's frames */
	int Parse(const uint8_t *data, uint32_t size);
	/* 1 when tag is one of the given kind */
	int IsTag(uint32_t tag, int kind);
	/* a text frame's tag and text */
	int GetText(const PhotoMakerNoteFrame *frame, PhotoMakerNoteText *text);
	/* the index-th text frame with the tag, from 1 */
	int GetTag(uint32_t tag, PhotoMakerNoteText *text, int index);

	paf::list<PhotoMakerNoteFrame *> m_frames;
};

#endif
