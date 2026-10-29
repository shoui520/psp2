#include "stdio.h"
#include "../compiler.h"
#include <iofilemgr/iofilemgr.h>

int sce_paf_fclose(sce_paf_FILE *stream)
{
	if (!stream || !stream->type)
		return -1;
	if ((uint8_t)stream->type == 2)
		return 0;
	if (stream->fd >= 0) {
		sceIoClose(stream->fd);
		stream->type = 0;
		stream->fd = -1;
		return 0;
	}
	stream->type = 0;
	return -1;
}

size_t sce_paf_fread(void *ptr, size_t size, size_t count, sce_paf_FILE *stream)
{
	unsigned char *dst = ptr;
	uint32_t remaining, total = 0;
	int32_t offset;
	int result;

	if (!stream || !stream->type)
		return 0;
	if (!(stream->type & 0x8000)) {
		stream->status |= PAF_FILE_STATUS_ERROR;
		return 0;
	}
	if (stream->fd < 0 || stream->status)
		return 0;
	remaining = size * count;
	if (!remaining)
		return 0;
	if (stream->unget_char != -1) {
		*dst++ = stream->unget_char;
		stream->unget_char = -1;
		remaining--;
		total = 1;
	}
	while (stream->status == 0 && (int32_t)remaining > 0) {
		result = sceIoRead(stream->fd, dst, remaining);
		offset = (int32_t)sceIoLseek(stream->fd, 0, SCE_SEEK_CUR);
		if (offset >= 0)
			stream->seek_offset = offset;
		if (result < 0) {
			stream->status |= PAF_FILE_STATUS_ERROR;
			return 0;
		}
		if (!result)
			stream->status |= PAF_FILE_STATUS_EOF;
		remaining -= result;
		total += result;
		dst += result;
	}
	return (int32_t)total / (int32_t)size;
}

size_t sce_paf_fwrite(const void *ptr, size_t size, size_t count, sce_paf_FILE *stream)
{
	const unsigned char *src = ptr;
	uint32_t remaining, total = 0;
	int result;

	if (!stream || !stream->type)
		return 0;
	if (!(stream->type & 0x4000)) {
		stream->status |= PAF_FILE_STATUS_ERROR;
		return 0;
	}
	if (stream->fd < 0 || stream->status)
		return 0;
	remaining = size * count;
	if (!remaining)
		return 0;
	while ((int32_t)remaining > 0) {
		result = sceIoWrite(stream->fd, src, remaining);
		if (result < 0) {
			stream->status |= PAF_FILE_STATUS_ERROR;
			return 0;
		}
		remaining -= result;
		total += result;
		src += result;
		if (stream->status)
			break;
	}
	return (int32_t)total / (int32_t)size;
}

int sce_paf_fgetc(sce_paf_FILE *stream)
{
	unsigned char ch;

	if (sce_paf_fread(&ch, 1, 1, stream) == 1)
		return ch;
	return -1;
}

int sce_paf_fputc(int ch, sce_paf_FILE *stream)
{
	unsigned char byte = ch;

	if (sce_paf_fwrite(&byte, 1, 1, stream) == 1)
		return ch;
	return -1;
}

int sce_paf_fseek(sce_paf_FILE *stream, long offset, int origin)
{
	int32_t position;

	if (!stream || !stream->type || stream->fd < 0)
		return -1;
	stream->unget_char = -1;
	position = (int32_t)sceIoLseek(stream->fd, offset, origin);
	if (position < 0)
		return -1;
	stream->seek_offset = position;
	stream->status &= ~PAF_FILE_STATUS_EOF;
	return 0;
}

long sce_paf_ftell(sce_paf_FILE *stream)
{
	if (!stream || !stream->type || stream->fd < 0)
		return -1;
	if (stream->unget_char == -1)
		return stream->seek_offset;
	return stream->seek_offset > 0 ? stream->seek_offset - 1 : 0;
}

int sce_paf_feof(sce_paf_FILE *stream)
{
	if (!stream || !stream->type || stream->fd < 0)
		return 0;
	return (stream->status & PAF_FILE_STATUS_EOF) != 0;
}

int sce_paf_ferror(sce_paf_FILE *stream)
{
	if (!stream || !stream->type || stream->fd < 0)
		return 1;
	return (stream->status & PAF_FILE_STATUS_ERROR) != 0;
}

int sce_paf_fileno(sce_paf_FILE *stream)
{
	if (!stream || !stream->type || stream->fd < 0)
		return -1;
	return stream->fd;
}

void sce_paf_clearerr(sce_paf_FILE *stream)
{
	if (stream && stream->type && stream->fd >= 0)
		stream->status = 0;
}

int sce_paf_ungetc(int ch, sce_paf_FILE *stream)
{
	if (stream->unget_char != -1)
		return -1;
	stream->unget_char = ch;
	return ch;
}
