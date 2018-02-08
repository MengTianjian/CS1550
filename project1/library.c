#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <termios.h>

typedef unsigned short color_t;
#define BLUE(color) (color & 0x001f)
#define GREEN(color) (color & 0x07e0)
#define RED(color) (color & 0xf800)

int fb;
color_t *address;
size_t size;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
struct termios t0;

void init_graphics()
{
    fb = open("/dev/fb0", O_RDWR);
    //if (fb == -1) {
    //    exit(1);
    //}
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);

    size = var.yres_virtual * fix.line_length;
    address = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);

    ioctl(0, TCGETS, &t0);
    struct termios t1 = t0;
    t1.c_lflag = t1.c_lflag & (~ECHO) & (~ICANON);
    ioctl(0, TCSETS, &t1);
}

void exit_graphics()
{
    munmap(address, size);
    close(fb);

    ioctl(0, TCSETS, &t0);
}

void clear_screen()
{
    write(0, "\033[2J", 4);
}

char getkey()
{
    char c = ' ';
    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    retval = select(1, &rfds, NULL, NULL, &tv);
    if (retval)
    {
        read(0, &c, 1);
    }
    return c;
}

void sleep_ms(long ms)
{
    struct timespec tim;
    tim.tv_sec = ms / 1000;
    tim.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&tim, NULL);
}

void draw_pixel(int x, int y, color_t color)
{
    if (x < fix.line_length / 2 && y < var.yres_virtual) {
        int pixel = (fix.line_length * y / 2) + x;
        *(address + pixel) = color;
    }
}

void draw_rect(int x1, int y1, int width, int height, color_t color)
{
    int i, j;
    for (i = 0; i <= width; i++) {
        for (j = 0; j <= height; j++) {
            draw_pixel(x1 + i, y1 + j, color);
        }
    }
}

void draw_circle(int x, int y, int r, color_t color)
{
    int x0 = r - 1;
    int y0 = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (r << 1);

    while (x0 >= y0)
    {
        draw_pixel(x + x0, y + y0, color);
        draw_pixel(x + y0, y + x0, color);
        draw_pixel(x - y0, y + x0, color);
        draw_pixel(x - x0, y + y0, color);
        draw_pixel(x - x0, y - y0, color);
        draw_pixel(x - y0, y - x0, color);
        draw_pixel(x + y0, y - x0, color);
        draw_pixel(x + x0, y - y0, color);

        if (err <= 0)
        {
            y0++;
            err += dy;
            dy += 2;
        }

        if (err > 0)
        {
            x0--;
            dx += 2;
            err += dx - (r << 1);
        }
    }
}
