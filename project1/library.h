/* 
 * Graphics Library header file
 * (c) Mohammad H. Mofrad, 2017
 */

#ifndef _GRAPHICS_INCLUDED_H
#define _GRAPHICS_INCLUDED_H

//#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <termios.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>

typedef unsigned short color_t;    // |15 11|10  5|4  0|
                                   // |red  |green|blue|
                                   //   5   + 6   + 5  =  16 bits
// Macros to extract RGB colors 
#define BMASK(c) (c & 0x001F) // Blue mask
#define GMASK(c) (c & 0x07E0) // Green mask
#define RMASK(c) (c & 0xF800) // Red mask

void clear_screen();
void init_graphics();
void exit_graphics();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t c);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

#endif
