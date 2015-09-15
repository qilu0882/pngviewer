
#ifndef __FRAMEBUF_H_
#define __FRAMEBUF_H_

#include <linux/fb.h>

typedef unsigned char __u8;
typedef unsigned int __u32;

struct bpp16_pixel {
	unsigned short red:5;
	unsigned short green:6;
	unsigned short blue:5;
};

struct bpp24_pixel {
	__u8 red;
	__u8 green;
	__u8 blue;
};

struct bpp32_pixel {
	__u8 red;
	__u8 green;
	__u8 blue;
	__u8 alpha;
};

struct rgb888_sample {
	__u8 b1;
	__u8 b2;
	__u8 b3;
};

struct rgb565_sample {
	unsigned short b1:5,
		b2:6,
		b3:5;
};

struct fb_info {
	int fd;
	__u32 xres;
	__u32 yres;
	__u32 bpp;
	__u32 smem_len;
	__u8 *fb_mem;
};

static __inline__ void fb_set_a_pixel(struct fb_info *fb_info, int line, int col,
		    __u8 red, __u8 green, __u8 blue,
		    __u8 alpha)
{
	switch (fb_info->bpp) {
	case 16:
		{
			struct bpp16_pixel *pixel = (struct bpp16_pixel *)fb_info->fb_mem;
			pixel[line * fb_info->xres + col].red = (red >> 3);
			pixel[line * fb_info->xres + col].green = (green >> 2);
			pixel[line * fb_info->xres + col].blue = (blue >> 3);
		}
		break;
	case 24:
		{
			struct bpp24_pixel *pixel = (struct bpp24_pixel *)fb_info->fb_mem;
			pixel[line * fb_info->xres + col].red = red;
			pixel[line * fb_info->xres + col].green = green;
			pixel[line * fb_info->xres + col].blue = blue;
		}
		break;

	case 32:
		{
			struct bpp32_pixel *pixel = (struct bpp32_pixel *)fb_info->fb_mem;
			pixel[line * fb_info->xres + col].red = red;
			pixel[line * fb_info->xres + col].green = green;
			pixel[line * fb_info->xres + col].blue = blue;
			pixel[line * fb_info->xres + col].alpha = alpha;
		}
		break;
	default:
		break;
	}
}


#endif /* __FRAMEBUF_H_ */
