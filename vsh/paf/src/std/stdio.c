#include "stdio.h"

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
