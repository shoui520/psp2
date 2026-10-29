#ifndef PAF_STD_STDIO_H
#define PAF_STD_STDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct sce_paf_FILE {
	int32_t type;
	int32_t fd;
	uint32_t status;
	int32_t unget_char;
	int32_t seek_offset;
	int32_t buff_attr;
	int32_t buff_off;
	int32_t buff_len;
} sce_paf_FILE;

enum PafFileStatus {
	PAF_FILE_STATUS_ERROR = 1,
	PAF_FILE_STATUS_EOF = 2
};

#define sce_paf_feof ScePafStdc_7CA739D5
#define sce_paf_ferror ScePafStdc_AC25A31C
#define sce_paf_fileno ScePafStdc_17A23255
#define sce_paf_clearerr ScePafStdc_6FAF02EB
#define sce_paf_ungetc ScePafStdc_84C69938

int sce_paf_feof(sce_paf_FILE *stream);
int sce_paf_ferror(sce_paf_FILE *stream);
int sce_paf_fileno(sce_paf_FILE *stream);
void sce_paf_clearerr(sce_paf_FILE *stream);
int sce_paf_ungetc(int ch, sce_paf_FILE *stream);
int sce_paf_fclose(sce_paf_FILE *stream);
size_t sce_paf_fread(void *ptr, size_t size, size_t count, sce_paf_FILE *stream);
int sce_paf_fgetc(sce_paf_FILE *stream);

#endif
