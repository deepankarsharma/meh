
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "jpeglib.h"

#include "meh.h"

struct error_mgr{
	struct jpeg_error_mgr pub;
	jmp_buf jmp_buffer;
};

struct jpeg_t{
	struct image img;
	FILE *f;
	struct jpeg_decompress_struct cinfo;
	struct error_mgr jerr;
};

static void error_exit(j_common_ptr cinfo){
	(void) cinfo;
	printf("\nerror!\n");
	exit(1);
}

static struct image *jpeg_open(FILE *f){
	struct jpeg_t *j;

	rewind(f);
	if(getc(f) != 0xff || getc(f) != 0xd8)
		return NULL;

	j = malloc(sizeof(struct jpeg_t));
	j->f = f;
	rewind(f);

	j->cinfo.err = jpeg_std_error(&j->jerr.pub);
	j->jerr.pub.error_exit = error_exit;

	jpeg_create_decompress(&j->cinfo);
	jpeg_stdio_src(&j->cinfo, f);
	jpeg_read_header(&j->cinfo, TRUE);

	/* parameters */
	j->cinfo.do_fancy_upsampling = 0;
	j->cinfo.do_block_smoothing = 0;
	j->cinfo.quantize_colors = 0;
	j->cinfo.dct_method = JDCT_FASTEST;

	jpeg_calc_output_dimensions(&j->cinfo);

	j->img.bufwidth = j->cinfo.output_width;
	j->img.bufheight = j->cinfo.output_height;

	return (struct image *)j;
}

static int jpeg_read(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	unsigned int row_stride;
	int a = 0, b;
	unsigned int x, y;

	row_stride = j->cinfo.output_width * j->cinfo.output_components;

	jpeg_start_decompress(&j->cinfo);

	unsigned char *buf = img->buf;

	if(j->cinfo.output_components == 3){
		JSAMPARRAY buffer = &buf;
		for(y = 0; y < j->cinfo.output_height; y++){
			jpeg_read_scanlines(&j->cinfo, buffer, 1);
			buf += row_stride;
		}
	}else if(j->cinfo.output_components == 1){
		JSAMPARRAY buffer = (*j->cinfo.mem->alloc_sarray)((j_common_ptr)&j->cinfo, JPOOL_IMAGE, row_stride, 4);
		for(y = 0; y < j->cinfo.output_height; ){
			int n = jpeg_read_scanlines(&j->cinfo, buffer, 4);
			for(b = 0; b < n; b++){
				for(x = 0; x < j->cinfo.output_width; x++){
					img->buf[a++] = buffer[b][x];
					img->buf[a++] = buffer[b][x];
					img->buf[a++] = buffer[b][x];
				}
			}
			y += n;
		}
	}
	jpeg_finish_decompress(&j->cinfo);

	return 0;
}

void jpeg_close(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	jpeg_destroy_decompress(&j->cinfo);
	fclose(j->f);
}

struct imageformat libjpeg = {
	jpeg_open,
	jpeg_read,
	jpeg_close
};

