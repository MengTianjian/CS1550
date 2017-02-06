#include "library.h"
int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do
	{
		clear_screen();
		draw_text(x, y, "Hello!", 60000);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;

		draw_rect(x, y, 50, 50, 255);
		sleep_ms(2000);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
