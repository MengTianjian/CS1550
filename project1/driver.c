#include <stdio.h>
#include <stdlib.h>
//#include "library.c"

int main()
{
    init_graphics();
    clear_screen();
    //draw_pixel(5,5,0xffff);
    int x = 100;
    int y = 100;
    draw_rect(x, y, 100, 100, 0xffff);
    char k = ' ';
    while (k != 'q') {
        k = getkey();
        if (k == ' ') {
            continue;
        }
        if (k == 'w') {
            y = y - 10;
        }
        else if (k == 's') {
            y = y + 10;
        }
        else if (k == 'a') {
            x = x - 10;
        }
        else if (k == 'd') {
            x = x + 10;
        }
        clear_screen();
        draw_rect(x, y, 100, 100, 0xffff);
    }
    clear_screen();
    //draw_pixel(10,10,0xffff);
    draw_circle(100, 100, 100, 0xffff);
    sleep_ms(5000);
    clear_screen();
    exit_graphics();
    return 0;
}
