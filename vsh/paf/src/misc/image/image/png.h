/*
 * The libpng 1.5.10 interface as libpaf's PNG codec uses it. libpaf links
 * libpng 1.5.10 (its version strings), built with setjmp error handling over
 * paf's own setjmp and longjmp.
 *
 * PROVISIONAL: declared here until libpaf's libpng (0x8116d000..) is vendored
 * into contrib; this file then gives way to its png.h and pngconf.h.
 */
#ifndef _PAF_IMAGE_PNG_H
#define _PAF_IMAGE_PNG_H

#include <stddef.h>
#include <std/setjmp.h>

#define PNG_LIBPNG_VER_STRING	"1.5.10"

#define PNG_COLOR_TYPE_GRAY		0
#define PNG_COLOR_TYPE_RGB		2
#define PNG_COLOR_TYPE_PALETTE		3
#define PNG_COLOR_TYPE_GRAY_ALPHA	4
#define PNG_COLOR_TYPE_RGB_ALPHA	6

#define PNG_INTERLACE_NONE		0
#define PNG_COMPRESSION_TYPE_BASE	0
#define PNG_FILTER_TYPE_BASE		0
#define PNG_ALL_FILTERS			0xf8
#define PNG_FILLER_AFTER		1
#define PNG_INFO_tRNS			0x0010

typedef unsigned char png_byte;
typedef unsigned short png_uint_16;
typedef unsigned int png_uint_32;
typedef size_t png_size_t;
typedef size_t png_alloc_size_t;
typedef void *png_voidp;
typedef png_byte *png_bytep;
typedef png_byte **png_bytepp;
typedef const char *png_const_charp;

typedef struct png_color_struct {
	png_byte red;
	png_byte green;
	png_byte blue;
} png_color;
typedef png_color *png_colorp;

typedef struct png_color_16_struct {
	png_byte index;
	png_uint_16 red;
	png_uint_16 green;
	png_uint_16 blue;
	png_uint_16 gray;
} png_color_16;
typedef png_color_16 *png_color_16p;

typedef struct png_struct_def png_struct;
typedef png_struct *png_structp;
typedef png_struct **png_structpp;
typedef struct png_info_def png_info;
typedef png_info *png_infop;
typedef png_info **png_infopp;

typedef void (*png_error_ptr)(png_structp, png_const_charp);
typedef void (*png_rw_ptr)(png_structp, png_bytep, png_size_t);
typedef void (*png_flush_ptr)(png_structp);
typedef void (*png_read_status_ptr)(png_structp, png_uint_32, int);
typedef png_voidp (*png_malloc_ptr)(png_structp, png_alloc_size_t);
typedef void (*png_free_ptr)(png_structp, png_voidp);
typedef void (*png_longjmp_ptr)(jmp_buf, int);

#ifdef __cplusplus
extern "C" {
#endif

png_structp png_create_read_struct_2(png_const_charp user_png_ver, png_voidp error_ptr,
				     png_error_ptr error_fn, png_error_ptr warn_fn,
				     png_voidp mem_ptr, png_malloc_ptr malloc_fn,
				     png_free_ptr free_fn);
png_structp png_create_write_struct(png_const_charp user_png_ver, png_voidp error_ptr,
				    png_error_ptr error_fn, png_error_ptr warn_fn);
png_infop png_create_info_struct(png_structp png_ptr);
void png_destroy_read_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
			     png_infopp end_info_ptr_ptr);
void png_destroy_write_struct(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr);

void png_set_mem_fn(png_structp png_ptr, png_voidp mem_ptr, png_malloc_ptr malloc_fn,
		    png_free_ptr free_fn);
png_voidp png_get_mem_ptr(png_structp png_ptr);
void png_set_read_fn(png_structp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn);
void png_set_write_fn(png_structp png_ptr, png_voidp io_ptr, png_rw_ptr write_data_fn,
		      png_flush_ptr output_flush_fn);
void png_set_read_status_fn(png_structp png_ptr, png_read_status_ptr read_row_fn);
png_voidp png_get_io_ptr(png_structp png_ptr);
jmp_buf *png_set_longjmp_fn(png_structp png_ptr, png_longjmp_ptr longjmp_fn,
			    size_t jmp_buf_size);
void png_error(png_structp png_ptr, png_const_charp error_message);

void png_read_info(png_structp png_ptr, png_infop info_ptr);
png_uint_32 png_get_IHDR(png_structp png_ptr, png_infop info_ptr, png_uint_32 *width,
			 png_uint_32 *height, int *bit_depth, int *color_type,
			 int *interlace_method, int *compression_method, int *filter_method);
png_uint_32 png_get_PLTE(png_structp png_ptr, png_infop info_ptr, png_colorp *palette,
			 int *num_palette);
png_uint_32 png_get_valid(png_structp png_ptr, png_infop info_ptr, png_uint_32 flag);
png_uint_32 png_get_tRNS(png_structp png_ptr, png_infop info_ptr, png_bytep *trans_alpha,
			 int *num_trans, png_color_16p *trans_color);

void png_set_strip_alpha(png_structp png_ptr);
void png_set_packing(png_structp png_ptr);
void png_set_filler(png_structp png_ptr, png_uint_32 filler, int flags);
void png_set_strip_16(png_structp png_ptr);
void png_set_gray_to_rgb(png_structp png_ptr);
void png_set_expand_gray_1_2_4_to_8(png_structp png_ptr);
int png_set_interlace_handling(png_structp png_ptr);

void png_start_read_image(png_structp png_ptr);
void png_read_row(png_structp png_ptr, png_bytep row, png_bytep display_row);
void png_read_end(png_structp png_ptr, png_infop info_ptr);

void png_set_filter(png_structp png_ptr, int method, int filters);
void png_set_compression_level(png_structp png_ptr, int level);
void png_set_IHDR(png_structp png_ptr, png_infop info_ptr, png_uint_32 width,
		  png_uint_32 height, int bit_depth, int color_type, int interlace_method,
		  int compression_method, int filter_method);
void png_write_info(png_structp png_ptr, png_infop info_ptr);
void png_write_image(png_structp png_ptr, png_bytepp image);
void png_write_end(png_structp png_ptr, png_infop info_ptr);

#ifdef __cplusplus
}
#endif

/* The jmp_buf libpng jumps back to on an error, through paf's longjmp. */
#define png_jmpbuf(png_ptr) \
	(*png_set_longjmp_fn((png_ptr), sce_paf_longjmp, sizeof(jmp_buf)))

#endif /* _PAF_IMAGE_PNG_H */
