#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <png.h>

#include "framebuf.h"

int x_offs = 0;
int y_offs = 0;

int width;
int height;
int stride;
int pixelSize;
int format;
unsigned char* pData;

int color_type;
int bit_depth;
int channels;

static int read_png(const char* name)
{
	int result = 0;
	unsigned char header[8];
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	int bytesRead;
	
	FILE* fp = fopen(name, "rb");
	if (fp == NULL) {
		result = -1;
		goto exit;
	}

	bytesRead = fread(header, 1, sizeof(header), fp);
	if (bytesRead != sizeof(header)) {
		result = -2;
		goto exit;
	}

	if (png_sig_cmp(header, 0, sizeof(header))) {
        	result = -3;
        	goto exit;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
        	result = -4;
        	goto exit;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
        	result = -5;
        	goto exit;
    	}

	if (setjmp(png_jmpbuf(png_ptr))) {
        	result = -6;
        	goto exit;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, sizeof(header));
	png_read_info(png_ptr, info_ptr);
	
	width = info_ptr->width;
	height = info_ptr->height;
	stride = 4 * width;
	pixelSize = stride * height;
	
	color_type = info_ptr->color_type;
	bit_depth = info_ptr->bit_depth;
	channels = info_ptr->channels;
    if (!(bit_depth == 8 &&
          ((channels == 3 && color_type == PNG_COLOR_TYPE_RGB) ||
           (channels == 4 && color_type == PNG_COLOR_TYPE_RGBA) ||
           (channels == 1 && color_type == PNG_COLOR_TYPE_PALETTE)))) {
        return -7;
        goto exit;
    }

    pData = malloc(pixelSize);
    if (pData == NULL) {
        result = -8;
        goto exit;
    }
    //format = (channels == 3) ? GGL_PIXEL_FORMAT_RGBX_8888 : GGL_PIXEL_FORMAT_RGBA_8888;

    int alpha = 0;
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
        alpha = 1;
    }

    unsigned int y;
    if (channels == 3 || (channels == 1 && !alpha)) {
        for (y = 0; y < height; ++y) {
            unsigned char* pRow = pData + y * stride;
            png_read_row(png_ptr, pRow, NULL);

            int x;
            for(x = width - 1; x >= 0; x--) {
                int sx = x * 3;
                int dx = x * 4;
                unsigned char r = pRow[sx];
                unsigned char g = pRow[sx + 1];
                unsigned char b = pRow[sx + 2];
                unsigned char a = 0xff;
                pRow[dx    ] = r; // r
                pRow[dx + 1] = g; // g
                pRow[dx + 2] = b; // b
                pRow[dx + 3] = a;
            }
        }
    } else {
        for (y = 0; y < height; ++y) {
            unsigned char* pRow = pData + y * stride;
            png_read_row(png_ptr, pRow, NULL);
        }
    }

exit:
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	
	if (fp != NULL) {
		fclose(fp);
	}
	return result;
}

static void disp_png(struct fb_info *fb_info)
{
	unsigned char* pRow;
	unsigned char r, g, b, a;
	int dx;
	int x, y;

	for (y = 0; y < height; ++y) {
		pRow = pData + y * stride;
		
		for(x = width - 1; x >= 0; x--) {
			dx = 4 * x;
			r = pRow[dx    ]; // red
			g = pRow[dx + 1]; // green
			b = pRow[dx + 2]; // blue
			a = pRow[dx + 3]; // alpha

			fb_set_a_pixel(fb_info, y + y_offs, x + x_offs, b, g, r, a);	// R-G-B-Alpha
		}
	}
}

static int init_framebuffer(char *fbdev, struct fb_info *fb_info)
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	// Open the file for reading and writing
	fb_info->fd = open(fbdev, O_RDWR);
	if (fb_info->fd < 0) {
		fprintf(stderr, "FB: Cannot open framebuffer device %s (%s)\n", fbdev, strerror(errno));
		return -ENODEV;
	}

	// Get fixed screen information
	if (ioctl(fb_info->fd, FBIOGET_FSCREENINFO, &finfo)) {
		fprintf(stderr, "FB: Cannot read fixed information (%s)\n", strerror(errno));
		return -EINVAL;
	}

	// Get variable screen information
	if (ioctl(fb_info->fd, FBIOGET_VSCREENINFO, &vinfo)) {
		fprintf(stderr, "FB: Cannot read variable information (%s)\n", strerror(errno));
		return -EINVAL;
	}

	// Put variable screen information
	vinfo.activate |= FB_ACTIVATE_FORCE | FB_ACTIVATE_NOW;
	if (ioctl(fb_info->fd, FBIOPUT_VSCREENINFO, &vinfo)) {
		fprintf(stderr, "FB: Cannot write variable information (%s)\n", strerror(errno));
		return -EINVAL;
	}

	// Figure out the size of the screen in bytes
	fb_info->xres = vinfo.xres;
	fb_info->yres = vinfo.yres;
	fb_info->bpp = vinfo.bits_per_pixel;
	fb_info->smem_len = finfo.smem_len;

	// Map the device to memory
	fb_info->fb_mem = (__u8 *)mmap(0,
				       finfo.smem_len,
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED,
				       fb_info->fd,
				       0);
	if (!fb_info->fb_mem) {
		fprintf(stderr, "FB: Fail to mmap framebuffer device to memory (%s)\n", strerror(errno));
		return -EINVAL;
	}

	fprintf(stdout, "FB: Width:%d  Height:%d  BPP:%d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	return 0;
}

void finish_framebuffer(struct fb_info *fb_info)
{
	if (fb_info->fb_mem)
		munmap(fb_info->fb_mem, fb_info->smem_len);
	fb_info->fb_mem = NULL;
	if (fb_info->fd > 0)
		close(fb_info->fd);
	fb_info->fd = -1;
}

int main(int argc ,char ** argv)
{
	struct fb_info fbinfo;

	char *pngfile = NULL;
	char *fbdev = NULL;

 	int ret = 0;

	if (argc != 3) {
		puts("PNG Viewer");
		puts("Usage: ");
		puts("\tpngviewer  <fb node>  <png file>\n"); 
		return  -1 ;
	}

	fbdev = argv[1];
	pngfile = argv[2];

	printf("%s <%s> <%s>\n", argv[0], fbdev, pngfile);

	ret = init_framebuffer(fbdev, &fbinfo);
	if (ret != 0) {
		fprintf(stderr, "Quit ...\n");
		goto error;
	}

	ret = read_png(pngfile);
	if (ret < 0)
		goto error;

	disp_png(&fbinfo);

error:
	if (pData) {
		free(pData);
		pData = NULL;
	}
	finish_framebuffer(&fbinfo);
	return ret;
}
