#include "library.h"
#include "iso_font.h"

int fid;
int fid1 = 1;
size_t size;
color_t *address;
struct termios t;

void init_graphics()
{
    fid = open("/dev/fb0", O_RDWR);

    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    ioctl(fid, FBIOGET_VSCREENINFO, &v);
    ioctl(fid, FBIOGET_FSCREENINFO, &f);
    size = v.yres_virtual * f.line_length;
    size /= 2;
    address = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);

    ioctl(fid1, TCGETS, &t);
    struct termios t1 = t;
    t1.c_lflag = 0;
    ioctl(fid1, TCSETS, &t1);
}

void clear_screen()
{
    write(fid1, "\033[2J", 4);  /* This will do the trick for fid1*/
}

void exit_graphics()
{
	ioctl(fid1, TCSETS, &t);
	munmap(address, size);
	close(fid);
}

char getkey()
{
	char c[2];
	fd_set s_rd;
	FD_ZERO(&s_rd);
	FD_SET(0, &s_rd);
	int r = select(1, &s_rd, NULL, NULL, NULL);
	if (r <= 0)
	{
		perror("select()");
	}
	read(0, c, 2);
	return c[0];
}

void sleep_ms(long ms)
{
	struct timespec ts = {0};
	ts.tv_sec = ms/1000;
	ms %= 1000;
	ts.tv_nsec = ms * 1000000L;
	nanosleep(&ts, NULL);
}

void draw_pixel(int x, int y, color_t c)
{
	color_t off_p = 640*y + x;
	if (x < 640 && y < 480)
		*(address + off_p) = RMASK(c) | GMASK(c) | BMASK(c);
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
	int i, j;
	for (i = 0; i <= width; i++)
		for (j = 0; j <= height; j++)
			draw_pixel(x1+i, y1+j, c);
}

void draw_text(int x, int y, const char *text, color_t c)
{
	int b = 0;
	char a = text[b];
	while (a != '\0')
	{
		int v = (int)a;
		int i, j;
		for (i = 0; i < 16; i++)
		{
			int index = iso_font[v*16+i];
			int exp = 128;
			for (j = 7; j >= 0; j--)
			{
				if ((index/exp) == 1)
					draw_pixel(x+j, y+i, c);
				index %= exp;
				exp /= 2;
			}
		}
		b++;
		a = text[b];
		x += 8;
	}
}
