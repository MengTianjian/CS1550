# CS/COE 1550 Project 1

## Goal:
To better your understanding of low-level handling of system calls
by writing a small graphics library that can set a pixel to a particular color,
draw some basic shapes, and read keypresses.

## Background:

## Specifications:
1. You will provide the following library functions (explained further below):

	| Library Call | System Call(s) used |
	| ------------ | ------------------- |
	| void init_graphics() | open, ioctl, mmap |
	| void exit_graphics() | ioctl |
	| void clear_screen() | write |
	| char getkey() | select, read |
	| void sleep_ms(long ms) | nanosleep |
	| void draw_pixel(int x, int y, color_t color) |  |
	| void draw_rect(int x1, int y1, int width, int height, color_t c) |  |
	| void draw_circle(int x, int y, int r, color_t color) |  |

1. Each library function must be implemented using only the Linux syscalls. You may not use any C standard library functions in your library anywhere.
1. Installing and Running QEMU:
	1. Download the [qemu-arm.zip](http://people.cs.pitt.edu/~nlf4/cs1550/handouts/qemu-arm.zip) file from the website and extract the files into a folder.
		* Do *not* download or unzip these files to your git repository folder! We will use git to transfer the files between your computer and the VM image.
	1. System-specific instructions:
		* Linux:
			* Using your appropriate package manager, install qemu-system-arm, likely part of your distro's qemu package. Then run start.sh in the zipped folder to launch QEMU.
		* macOS:
			* Install QEMU through Homebrew. If you don't have Homebrew, open a terminal and type:
				```
				ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"
				```
			* Go through the install steps. When done, install qemu by typing:
				```
				brew install qemu
				```
			* Now you can run start.sh in the zipped folder to launch qemu.
		* Windows:
			* Double-click the start.bat file in the folder to launch QEMU.
1. Setting up the Development Environment
	1. To keep downloads small, the disk image we have provided does not have a full development environment installed. To install gcc, gdb, git, and ssh/scp, run the script:
		```
		./devtools.sh
		```
	1. When this finishes downloading and installing, you should have the ability to use most basic Linux commands, `nano` for text editing, and `gcc`, `gdb`, `git`, and `ssh/scp/sftp`. These commands will survive a reboot, so this only needs to be done once.
	1. If you want to install other programs, you may use the Tiny Core Linux distribution's package installer:
		```
		tce
		```
		This lets you search for a particular package or program name and install it.
	1. Shutdown linux using the command (rebooting has been turned into poweroff by an option to QEMU):
		```
		sudo reboot
		```
	1. Finally, make a copy of disk.qcow2 someplace safe (but not in your project git repository!). If things go wrong, you can always restore back to this point by replacing the disk.qcow2 file in this directory with the one you've backed up.
1. Transferring files into/out of QEMU:
	* You can use GitHub as an intermediary to transfer code between your host OS and the QEMU disk image.
	* First, you'll need to clone your project respository from GitHub somewhere on your computer (host OS).
	* Once you have developed code that you would like to test, make a commit on your host OS and push the changes to GitHub.
	* The first time you go to test code in the VM, you will simply make a clone of your GitHub repository within the guest OS.  After that, you can use `git pull` to grab the most recent version on GitHub.  To avoid any tricky situations, I recommend only updating the repository on your host OS, and only issue pull's in the guest OS repository (and also don't make any chages (e.g., uploads or deletes) through the GitHub web interface!).
		* Note that the QEMU image we have provided does not have a running instance of ntp, and will not have a valid system time (it will initialize to UNIX time 0 at boot).  To be able to clone your repository from GitHub using HTTPS, you will need to set a reasonable system time.  This can be done using the `date` command, E.g.:
			```
			sudo date -s 2018.01.22-12:00
			```
			This will need to be done anytime you restart the VM.
1. Library function specifications:
	1. init_graphics()
		* In this function, you should do any work necessary to initialize the graphics library. This will be at least these four things:
			1. You will need to open the graphics device. The memory that backs a video display is often called a framebuffer. In the version of Linux we are using, the kernel has been built to allow you direct access to the framebuffer. As we learned in 449, hardware devices in Unix/Linux are usually exposed to user programs via the /dev/ filesystem. In our case, we will find a file, /dev/fb0, that represents the first (zero-th) framebuffer attached to the computer.  Since it appears to be a file, it can be opened using the open() syscall. To set a pixel, we only need to do basic file operations. You could seek to a location and then write some data. However, we will have a better way.
			1. We can do something special to make writing to the screen easier than using a whole bunch of seeks and writes. Since each of those is a system call on their own, using them to do a lot of graphics operations would be slow. Instead, we can ask the OS to map a file into our address space so that we can treat it as an array and use loads and stores to manipulate the file contents. We will be covering this more later when we discuss memory management, but for now, we want to use this idea of memory mapping for our library. The mmap() system call takes a file descriptor from open (and a bunch of other parameters) and returns a void *, an address in our address space that now represents the contents of the file. That means that we can use pointer arithmetic or array subscripting to set each individual pixel, provided we know how many pixels there are and how many bytes of data is associated with each pixel.
				* One note is that we must use the MAP_SHARED parameter to mmap() because other parts of the OS want to use the framebuffer too.
			1. In order to use the memory mapping we have just established correctly, we need some information about the screen. We need to know the resolution (number of pixels) in the x and y directions as well as the bit-depth: the amount of colors we can use. The machine that QEMU is configured to emulate is in a basic 640x480 mode with 16-bit color. This means that there are 640 pixels in the x direction (0 is leftmost and 639 is rightmost) and 480 pixels in the y direction (0 is topmost, 479 is bottommost). In 16-bit color, we store the intensity of the three primary colors of additive light, Red, Green, and Blue, all packed together into a single 16-bit number.  Since 16 isn't evenly divisible by three (RGB), we devote the upper 5 bits to storing red intensity (0-31), the middle 6 bits to store green intensity (0-63), and the low order 5 bits to store blue intensity (0-31).
				* You can use typedef to make a color type, color_t, that is an unsigned 16-bit value. You can also make a macro or function to encode a color_t from three RGB values using bit shifting and masking to make a single 16-bit number.
				* To get the screen size and bits per pixels, we can use a special system call: ioctl. This system call is used to query and set parameters for almost any device connected to the system. You pass it a file descriptor and a particular number that represents the request you're making of that device. We will use two requests: FBIOGET_VSCREENINFO and FBIOGET_FSCREENINFO. The first will give back a struct fb_var_screeninfo that will give the virtual resolution. The second will give back a struct fb_fix_screeninfo from which we can determine the bit depth. The total size of the mmap()'ed file would be the yres_virtual field of the first struct multiplied by the line_length field of the second.
			1. Our final step is to use the ioctl system call to disable keypress echo (displaying the keys as you're typing on the screen automatically) and buffering the keypresses. The commands we will need are TCGETS and TCSETS. These will yield or take a struct termios that describes the current terminal settings. You will want to disable canonical mode by unsetting the ICANON bit and disabling ECHO by forcing that bit to zero as well.
				* There is one problem with this. When the terminal settings are changed, the changes last even after the program terminates. That's why we have the exit_graphics() call. It can clean up the terminal settings by restoring them to what they were prior to us changing them.
				* That handles the normal termination, but it's always possible that our program abnormally terminates, i.e., it crashes. In that case when you try to type something at the commandline, it will not show up on the screen. What you're typing is still working, but you cannot see it. A reboot will fix this, but I found it useful to make a little helper program I called "fix" to turn echo back on in this event.
	1. exit_graphics()
		* This is your function to undo whatever it is that needs to be cleaned up before the program exits. Many things will be cleaned up automatically if we forget, for instance files will be closed and memory can be unmapped. It's always a good idea to do it yourself with close() and munmap() though.
		* What you'll definitely need to do is to make an ioctl() to reenable key press echoing and buffering as described above.
	1. clear_screen()
		* We will use an ANSI escape code to clear the screen rather than blanking it by drawing a giant rectangle of black pixels. ANSI escape codes are a sequence of characters that are not meant to be displayed as text but rather interpreted as commands to the terminal. We can print the string "\033[2J" to tell the terminal to clear itself.
	1. getkey()
		* To make games, we probably want some sort of user input. We will use key press input and we can read a single character using the read() system call. However, read() is blocking and will cause our program to not draw unless the user has typed something. Instead, we want to know if there is a keypress at all, and if so, read it.
		* This can be done using the Linux non-blocking system call select().
	1. sleep_ms()
		* We will use the system call nanosleep() to make our program sleep between frames of graphics being drawn. From its name you can guess that it has nanosecond precision, but we don't need that level of granularity. We will instead sleep for a specified number of milliseconds and just multiply that by 1,000,000.
		* We do not need to worry about the call being interrupted and so the second parameter to nanosleep() can be NULL.
	1. draw_pixel()
		* This is the main drawing code, where the work is actually done. We want to set the pixel at coordinate (x, y) to the specified color. You will use the given coordinates to scale the base address of the memory-mapped framebuffer using pointer arithmetic. The frame buffer will be stored in row-major order, meaning that the first row starts at offset 0 and then that is followed by the second row of pixels, and so on.
	1. draw_rect()
		* Using draw_pixel, make a rectangle with corners (x1, y1), (x1+width,y1), (x1+width,y1+height), (x1, y1+height).
		
	1. draw_circle()
		* Use the midpoint circle algorithm to draw a circle at (x, y) with radius r.
			* http://en.wikipedia.org/wiki/Midpoint_circle_algorithm#Example
1. Once your library code is completed, you will need to make a small driver program that illustrates all of the functionality of your library.
1. You should create a Makefile (compatible with GNU Make) that will compile your library code and driver.

## Submission Guidelines:
* **DO NOT** add any QEMU files to your git repository.
* **DO NOT SUBMIT** any IDE package files.
* You must name your library file "library.c" and your driver file "driver.c".
* Your repository must include a valid Makefile for your project.
* You must be able to compile your library and driver by running "make".
* You must be able to run your driver program by running "./driver".
* You must fill out info_sheet.txt.
* Be sure to remember to push the latest copy of your code back to your GitHub repository before the the assignment is due.  At the deadline, the repositories will automatically be copied for grading.  Whatever is present in your GitHub repository at that time will be considered your submission for this assignment.

## Additional Notes/Hints:
* This project will require extensive reading of man pages.  The system calls that you will need to make are listed in the table at the top of this document. To understand how they work, you will need to carefully study their man pages.  Be sure to use the `man 2 ...` page (for system calls) and not `man 3 ...` (for library calls).

## Grading Rubric:
* init_graphics:  15
* exit_graphics:  10
* clear_screen:  10
* getkey:  10
* sleep_ms:  10
* draw_pixel:  10
* draw_rect:  10
* draw_circle:  10
* Driver program:  10
* Assignment info sheet/submission:  5
