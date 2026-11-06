/*
 * The source managers libpaf's JPEG codec reads through (Sony's, names ours): libjpeg's
 * jdatasrc.c with the stdio file replaced by the codec's source, a paf File read at its
 * position or a buffer handed over whole. A premature end of the data is an error, not the
 * upstream warning with an inserted EOI.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <file/file.h>
#include <jpeglib.h>
#include <jerror.h>
#include "jpeg_paf.h"

using paf::File;

/* the codec's source (JpgCodec::Source) */
struct JpegPafSource {
	void *data;			/* a File, or the buffer's bytes */
	int64_t size;
	uint32_t pos;
	uint32_t reserved;
};

typedef int32_t (*JpegPafReadFunc)(JpegPafSource *source, JOCTET *buf, size_t size);

typedef struct {
	struct jpeg_source_mgr pub;
	JpegPafSource *source;
	JOCTET *buffer;
	JpegPafReadFunc read;
	boolean start_of_file;		/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

#define INPUT_BUF_SIZE	8192		/* choose an efficiently fread'able size */

static void init_source(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

	src->start_of_file = TRUE;
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;
	size_t nbytes;

	nbytes = src->read(src->source, src->buffer, INPUT_BUF_SIZE);

	if (nbytes == 0) {
		if (src->start_of_file)	/* Treat empty input file as fatal error */
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		/* Insert a fake EOI marker */
		src->buffer[0] = (JOCTET)0xFF;
		src->buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;

	return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct jpeg_source_mgr *src = cinfo->src;

	if (num_bytes > 0) {
		while (num_bytes > (long)src->bytes_in_buffer) {
			num_bytes -= (long)src->bytes_in_buffer;
			fill_input_buffer(cinfo);
		}
		src->next_input_byte += (size_t)num_bytes;
		src->bytes_in_buffer -= (size_t)num_bytes;
	}
}

static void term_source(j_decompress_ptr cinfo)
{
	/* no work necessary here */
}

/* a buffer is handed over whole: nothing more to read */
static int32_t read_mem(JpegPafSource *source, JOCTET *buf, size_t size)
{
	return 0;
}

static int32_t read_file(JpegPafSource *source, JOCTET *buf, size_t size)
{
	if (source != NULL && buf != NULL)
		return ((File *)source->data)->Read(buf, size);

	return 0;
}

int jpeg_paf_mem_src(j_decompress_ptr cinfo, void *source)
{
	JpegPafSource *s = (JpegPafSource *)source;
	my_src_ptr src;
	uint32_t pos;
	int64_t size;
	int64_t remain;
	int ret;

	if (cinfo->src == NULL) {	/* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
						   sizeof(my_source_mgr));
		src = (my_src_ptr)cinfo->src;
		src->buffer = NULL;
	}

	src = (my_src_ptr)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->source = s;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.next_input_byte = NULL;
	src->pub.term_source = term_source;
	src->read = read_mem;

	pos = s->pos;
	size = s->size;
	remain = size - pos;
	ret = 0;
	if (remain > 0) {
		ret = (int)remain;
		if (size == (uint32_t)size) {
			src->pub.next_input_byte = (const JOCTET *)s->data + pos;
			src->pub.bytes_in_buffer = (size_t)remain;
			s->pos = (uint32_t)size;
		}
	}

	return ret;
}

int jpeg_paf_file_src(j_decompress_ptr cinfo, void *source)
{
	my_src_ptr src;

	if (cinfo->src == NULL) {	/* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
						   sizeof(my_source_mgr));
		src = (my_src_ptr)cinfo->src;
		src->buffer = (JOCTET *)
			(*cinfo->mem->alloc_large)((j_common_ptr)cinfo, JPOOL_PERMANENT,
						   INPUT_BUF_SIZE * sizeof(JOCTET));
	}

	src = (my_src_ptr)cinfo->src;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->source = (JpegPafSource *)source;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->read = read_file;
	src->pub.init_source = init_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	return 0;
}
