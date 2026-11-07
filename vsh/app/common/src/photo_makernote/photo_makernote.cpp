/*
 * The PlayStation MakerNote: a "SONY PS" header, then ID3v2-style frames, of which the text
 * frames (TXXX) hold the tags. Names ours; see photo_makernote.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/list>
#include <std/string.h>
#include <std/stdlib.h>
#include "photo_makernote.h"

using namespace paf;

/* a big-endian value loaded whole */
#define PHOTO_MAKERNOTE_SWAP16(x)	((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define PHOTO_MAKERNOTE_SWAP32(x)						\
	(((x) << 24) | (((x) >> 24) & 0xff) | ((((x) >> 8) & 0xff) << 16) |	\
	 ((((x) >> 16) & 0xff) << 8))

/* the MakerNote's header: its magic, the frames' size and their number */
#define PHOTO_MAKERNOTE_HEADER_SIZE	24
#define PHOTO_MAKERNOTE_MAGIC_SIZE	18
#define PHOTO_MAKERNOTE_FRAMES_MAX	100
/* a frame's header, and the most data one frame may have */
#define PHOTO_MAKERNOTE_FRAME_HEADER_SIZE	10
#define PHOTO_MAKERNOTE_FRAME_SIZE_MAX	0x2000

/* a text frame: its encoding, language and tag before the text, and the text's terminator */
#define PHOTO_MAKERNOTE_TEXT_LANGUAGE	1
#define PHOTO_MAKERNOTE_TEXT_TAG	4
#define PHOTO_MAKERNOTE_TEXT_TEXT	9
#define PHOTO_MAKERNOTE_TEXT_OVERHEAD	10

static const char s_magic[PHOTO_MAKERNOTE_MAGIC_SIZE] = "SONY PS\0\0\0\0\0SIE\x01\0";

PhotoMakerNote::PhotoMakerNote()
{
}

void PhotoMakerNote::Clear()
{
	list<PhotoMakerNoteFrame *>::iterator it;

	for (it = m_frames.begin(); it != m_frames.end(); ++it) {
		if (*it != NULL)
			delete *it;
	}
	m_frames.clear();
}

PhotoMakerNote::~PhotoMakerNote()
{
	Clear();
}

int PhotoMakerNote::Parse(const uint8_t *data, uint32_t size)
{
	uint32_t offset;
	uint32_t framesSize;
	uint32_t count;
	uint32_t value;
	uint16_t value16;
	int ret;

	if (data == NULL || size < PHOTO_MAKERNOTE_HEADER_SIZE - 2)
		return PHOTO_MAKERNOTE_ERROR_INVALID_ARGUMENT;
	offset = 0;
	ret = PHOTO_MAKERNOTE_ERROR_PARSE;
	if (sce_paf_memcmp(data, s_magic, PHOTO_MAKERNOTE_MAGIC_SIZE) != 0)
		return PHOTO_MAKERNOTE_ERROR_INVALID_DATA;
	value = *(const uint32_t *)(data + offset + 18);
	value16 = *(const uint16_t *)(data + offset + 22);
	count = PHOTO_MAKERNOTE_SWAP16(value16);
	offset += PHOTO_MAKERNOTE_HEADER_SIZE;
	framesSize = PHOTO_MAKERNOTE_SWAP32(value);
	if (count >= PHOTO_MAKERNOTE_FRAMES_MAX)
		return PHOTO_MAKERNOTE_ERROR_INVALID_DATA;
	if (framesSize >= (uint16_t)count * PHOTO_MAKERNOTE_FRAME_SIZE_MAX)
		return PHOTO_MAKERNOTE_ERROR_INVALID_DATA;
	for (uint32_t i = 0; i < count; i++) {
		ret = ParseFrame(data, &offset, size);
		if (ret != 0)
			break;
	}
	return ret;
}

int PhotoMakerNote::IsTag(uint32_t tag, int kind)
{
	switch (tag) {
	case PHOTO_MAKERNOTE_TAG_ALAC:
	case PHOTO_MAKERNOTE_TAG_IDAC:
	case PHOTO_MAKERNOTE_TAG_LLAC:
	case PHOTO_MAKERNOTE_TAG_PHFR:
	case PHOTO_MAKERNOTE_TAG_PVFR:
	case PHOTO_MAKERNOTE_TAG_PYPR:
		if (kind == PHOTO_MAKERNOTE_TAG_KIND_VALUE || kind == PHOTO_MAKERNOTE_TAG_KIND_ANY)
			return 1;
		break;
	case PHOTO_MAKERNOTE_TAG_ALBM:
	case PHOTO_MAKERNOTE_TAG_GCID:
	case PHOTO_MAKERNOTE_TAG_GDSC:
	case PHOTO_MAKERNOTE_TAG_GPNM:
	case PHOTO_MAKERNOTE_TAG_OCOM:
	case PHOTO_MAKERNOTE_TAG_PEFV:
	case PHOTO_MAKERNOTE_TAG_PFID:
	case PHOTO_MAKERNOTE_TAG_PSIS:
	case PHOTO_MAKERNOTE_TAG_PTYP:
		if (kind == PHOTO_MAKERNOTE_TAG_KIND_TEXT || kind == PHOTO_MAKERNOTE_TAG_KIND_ANY)
			return 1;
		break;
	}
	return 0;
}

int PhotoMakerNote::GetText(const PhotoMakerNoteFrame *frame, PhotoMakerNoteText *text)
{
	const uint8_t *data = frame->m_data;
	uint32_t size = frame->m_size;
	uint32_t value = *(const uint32_t *)(data + PHOTO_MAKERNOTE_TEXT_TAG);
	uint32_t tag = PHOTO_MAKERNOTE_SWAP32(value);

	if (size < PHOTO_MAKERNOTE_TEXT_OVERHEAD)
		return -1;
	if (IsTag(tag, PHOTO_MAKERNOTE_TAG_KIND_TEXT) != 0) {
		text->valid = true;
		sce_paf_memcpy(text->language, data + PHOTO_MAKERNOTE_TEXT_LANGUAGE,
			       sizeof(text->language));
		text->tag = tag;
		text->text = (const char *)data + PHOTO_MAKERNOTE_TEXT_TEXT;
		text->length = size - PHOTO_MAKERNOTE_TEXT_OVERHEAD;
	} else {
		text->valid = false;
	}
	return 0;
}

int PhotoMakerNote::GetTag(uint32_t tag, PhotoMakerNoteText *text, int index)
{
	list<PhotoMakerNoteFrame *>::iterator it;
	int n;
	int ret;

	if (IsTag(tag, PHOTO_MAKERNOTE_TAG_KIND_TEXT) != 1)
		return PHOTO_MAKERNOTE_ERROR_INVALID_ARGUMENT;
	n = 0;
	for (it = m_frames.begin(); it != m_frames.end(); ++it) {
		const PhotoMakerNoteFrame *frame = *it;
		uint32_t id = *(const uint32_t *)frame->m_id;

		if (PHOTO_MAKERNOTE_SWAP32(id) == PHOTO_MAKERNOTE_FRAME_TXXX) {
			ret = GetText(frame, text);
			if (text->tag == tag && index == ++n)
				return ret != 0 ? PHOTO_MAKERNOTE_ERROR_INVALID_DATA : 0;
		}
	}
	return PHOTO_MAKERNOTE_ERROR_NOT_FOUND;
}

int PhotoMakerNote::ParseFrame(const uint8_t *data, uint32_t *offset, uint32_t size)
{
	PhotoMakerNoteFrame *frame;
	int length;

	if (*offset + PHOTO_MAKERNOTE_FRAME_HEADER_SIZE > size)
		return PHOTO_MAKERNOTE_ERROR_INVALID_DATA;
	frame = NULL;
	frame = new PhotoMakerNoteFrame();
	if (frame != NULL) {
		length = frame->Parse(data + *offset, size - *offset);
		if (length != -1) {
			if (length < -1)
				length = -length;
			else if (length == 0)
				goto fail;
			*offset += length;
			m_frames.push_back(frame);
			return 0;
		}
	}
fail:
	if (frame != NULL)
		delete frame;
	return PHOTO_MAKERNOTE_ERROR_PARSE;
}

__attribute__((noinline)) PhotoMakerNoteFrame::PhotoMakerNoteFrame()
	: m_size(0), m_data(NULL), m_flags(0), m_owned(false)
{
	*(uint32_t *)m_id = 0;
}

__attribute__((noinline)) PhotoMakerNoteFrame::~PhotoMakerNoteFrame()
{
	Reset();
}

int PhotoMakerNoteFrame::Parse(const uint8_t *data, uint32_t size)
{
	uint32_t length;

	Reset();
	if (size < PHOTO_MAKERNOTE_FRAME_HEADER_SIZE)
		return -1;
	if ((data[7] & 0x80) == 0)
		length = ((data[4] & 0x7f) << 21) | ((data[5] & 0x7f) << 14) |
			 ((data[6] & 0x7f) << 7) | (data[7] & 0x7f);
	else
		length = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
	if (length + PHOTO_MAKERNOTE_FRAME_HEADER_SIZE > size)
		return -1;
	sce_paf_memcpy(m_id, data, sizeof(m_id));
	if (m_id[0] == 0)
		return -1;
	m_data = (uint8_t *)sce_paf_malloc(length);
	if (m_data == NULL)
		return -1;
	m_size = length;
	m_owned = true;
	m_flags = (data[8] << 8) | data[9];
	sce_paf_memcpy(m_data, data + PHOTO_MAKERNOTE_FRAME_HEADER_SIZE, length);
	return length + PHOTO_MAKERNOTE_FRAME_HEADER_SIZE;
}
