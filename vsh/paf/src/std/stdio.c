#include "stdio.h"
#include "ctype.h"
#include "string.h"
#include "stdc++/atexit.h"
#include "../compiler.h"
#include "../thread/mutex.h"
#include <iofilemgr/iofilemgr.h>

static Mutex stdioMutex;
static sce_paf_FILE streamTable[20];

int StdioInitialize(void)
{
	extern void *__dso_handle;

	MutexConstruct(&stdioMutex, "ScePafStdio", MutexOption_None);
	return __aeabi_atexit(&stdioMutex,
				(void (*)(void *))MutexDestroy, &__dso_handle);
}

void *_stdinit_stdio(void)
{
	return sce_paf_memset(streamTable, 0, sizeof(*streamTable));
}

/* inlined into each opener in Sony's code */
static __attribute__((always_inline)) int ParseStreamMode(const char *mode)
{
	int ch, type;

	if (!mode || !*mode)
		return -1;
	ch = *mode;
	while (sce_paf_tolower(ch) == 'b')
		ch = *++mode;
	switch (sce_paf_tolower(*mode)) {
	case 'r':
		type = 0x8000;
		break;
	case 'w':
		type = 0x4000;
		break;
	case 'a':
		type = 0x6000;
		break;
	default:
		return -1;
	}
	ch = *++mode;
	while (sce_paf_tolower(ch) == 'b')
		ch = *++mode;
	if (*mode == '+')
		type |= 0xc000;
	return type;
}

static __attribute__((always_inline)) sce_paf_FILE *AllocateStream(const char *mode)
{
	sce_paf_FILE *stream;
	int i, type;

	MutexLock(&stdioMutex);
	for (i = 0; i < 20; i++) {
		if (!(uint8_t)streamTable[i].type)
			break;
	}
	if (i == 20 || (type = ParseStreamMode(mode)) < 0) {
		MutexUnlock(&stdioMutex);
		return NULL;
	}
	stream = &streamTable[i];
	stream->type = type | 1;
	MutexUnlock(&stdioMutex);
	return stream;
}

static __attribute__((always_inline)) void SetStreamPosition(sce_paf_FILE *stream, int type,
							  int fd)
{
	int32_t position;

	if (!stream || !type || fd < 0)
		return;
	stream->unget_char = -1;
	position = (int32_t)sceIoLseek(fd, 0,
				(type & 0x2000) ? SCE_SEEK_END : SCE_SEEK_SET);
	if (position >= 0) {
		stream->seek_offset = position;
		stream->status &= ~PAF_FILE_STATUS_EOF;
	}
}

sce_paf_FILE *sce_paf_fopen(const char *filename, const char *mode)
{
	sce_paf_FILE *stream;
	int ch, access, flags, type, fd, i;

	if (!filename || !mode)
		return NULL;
	MutexLock(&stdioMutex);
	for (i = 0; i < 20; i++) {
		if (!(uint8_t)streamTable[i].type)
			break;
	}
	if (i == 20 || !mode || !*mode) {
		MutexUnlock(&stdioMutex);
		return NULL;
	}
	ch = *mode;
	while (sce_paf_tolower(ch) == 'b')
		ch = *++mode;
	flags = 0;
	switch (sce_paf_tolower(*mode)) {
	case 'r':
		access = SCE_O_RDONLY;
		type = 0x8000;
		break;
	case 'w':
		access = SCE_O_WRONLY;
		flags = SCE_O_CREAT | SCE_O_TRUNC;
		type = 0x4000;
		break;
	case 'a':
		access = SCE_O_WRONLY;
		flags = SCE_O_CREAT | SCE_O_APPEND;
		type = 0x6000;
		break;
	default:
		MutexUnlock(&stdioMutex);
		return NULL;
	}
	ch = *++mode;
	while (sce_paf_tolower(ch) == 'b')
		ch = *++mode;
	if (*mode == '+') {
		access |= SCE_O_RDWR;
		type |= 0xc000;
	}
	flags |= access;
	stream = &streamTable[i];
	stream->type = type | 1;
	MutexUnlock(&stdioMutex);
	stream->status = 0;
	fd = sceIoOpen(filename, flags, 0606);
	type = stream->type;
	stream->fd = fd;
	stream->unget_char = -1;
	SetStreamPosition(stream, type, fd);
	if (stream->fd < 0) {
		stream->type = 0;
		return NULL;
	}
	return stream;
}

sce_paf_FILE *sce_paf_fdopen(int fd, const char *mode)
{
	sce_paf_FILE *stream;
	int type;

	if (!mode)
		return NULL;
	stream = AllocateStream(mode);
	if (!stream)
		return NULL;
	type = stream->type;
	stream->fd = fd;
	stream->status = 0;
	stream->unget_char = -1;
	SetStreamPosition(stream, type, fd);
	if (stream->fd < 0) {
		stream->type = 0;
		return NULL;
	}
	return stream;
}

int sce_paf_fflush(sce_paf_FILE *stream)
{
	(void)stream;
	return 0;
}

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
	int32_t remaining, total;
	int result;
	uint32_t status;

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
	total = 0;
	do {
		if (remaining <= 0)
			break;
		result = sceIoWrite(stream->fd, src, remaining);
		status = stream->status;
		if (result < 0) {
			stream->status = status | PAF_FILE_STATUS_ERROR;
			return 0;
		}
		remaining -= result;
		total += result;
		src += result;
	} while (status == 0);
	return total / (int32_t)size;
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
