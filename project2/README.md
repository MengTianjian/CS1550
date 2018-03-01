# CS/COE 1550 Project 2

## Goal:
To gain insight into kernel development by extending the Linux kernel's
functionality with your own system calls.

## Background:
We’ve also been learning about synchronization and solving the
producer/consumer problem using semaphores. In this project, we will modify the
Linux kernel to add our own implementations of `down()` and `up()` as system calls
and then use them to solve the producer/consumer problem.

Our version of the producer/consumer problem will simulate a restauarant serving pancakes.
There will be Chefs (producers) and Customers (consumers). A Chef can produce a single pancake at a time and a customer eats a single pancake.
There will be a userspace application called `prodcons` that will implement the producer/consumer problem using processes. We will use `fork()` to create additional processes. The number of consumers and producers will be specified on the command line followed by the size of the buffer.

If we run the executable as:
```
./prodcons 2 2 1000
```
we would see something like this output:

```
Chef A Produced: Pancake0
Chef B Produced: Pancake1
Chef A Produced: Pancake2
Chef B Produced: Pancake3
Chef A Produced: Pancake4
Customer A Consumed: Pancake0
Customer A Consumed: Pancake1
Customer B Consumed: Pancake2
```			

Basically we will be producing pancakes (sequential integers) and then
consuming them by printing them out to the screen. The program should run as an
infinite loop and never deadlock. All producers and consumers share the same
buffer (i.e., there is only one buffer total).

## Specification
1. Settup up x86 QEMU
	* We will be using a different version of Linux and QEMU (x86-based instead of ARM) for this project.
		* The disk image and a copy of QEMU for Windows is available by clicking [here](http://people.cs.pitt.edu/~nlf4/cs1550/handouts/qemu.zip).
		* For Linux users and Mac users wanting to use the homebrew version, I have a [test version](http://people.cs.pitt.edu/~nlf4/cs1550/handouts/qemu-test.zip) of the disk image and a start.sh script to run it. It should be identical to the version from the first bullet point in terms of functionality, but actually boot with a recent version of QEMU. IF THE ORIGINAL WORKS FOR YOU, DON'T BOTHER WITH THIS ONE.

1. Working with the Linux Kernel
	1. Setting up the Kernel Source
		1. On `thoth.cs.pitt.edu`, copy the linux-2.6.23.1.tar.bz file to your local space under /u/OSLab/USERNAME:
			
			```
			ssh USERNAME@thoth.cs.pitt.edu
			cd /u/OSLab/USERNAME
			cp /u/OSLab/original/linux-2.6.23.1.tar.bz2 .
			```
		
		1. Extract:

			```
			tar xfj linux-2.6.23.1.tar.bz2
			```

		1. Change into the `linux-2.6.23.1/` directory:
			
			```
			cd linux-2.6.23.1
			```
		
		1. Copy the `.config` file:
			
			```
			cp /u/OSLab/original/.config .
			```

			* You should only need to copy the kernel source and config once, however redoing the expansion of the kernel source tarball will undo any changes you've made and give you a fresh copy of the kernel should things go horribly awry.

		
		1. Build:
			
			```
			make ARCH=i386 bzImage
			```

	1. Rebuilding the Kernel
		* Anytime you make changes to the kernel source, you can reissue the build command from the `linux-2.6.23.1/` directory:
			
			```
			make ARCH=i386 bzImage
			```

	1. Copying the Files to QEMU
		* From QEMU, you will need to download two files from the new kernel that you just built. The kernel itself is a file named `bzImage` that lives in the directory `linux-2.6.23.1/arch/i386/boot/`. There is also a supporting file called `System.map` in the `linux-2.6.23.1/` directory that tells the system how to find the system calls.

		* Use scp to download the kernel to a home directory (`/root/` if root):
			
			```
			scp USERNAME@thoth.cs.pitt.edu:/u/OSLab/USERNAME/linux-2.6.23.1/arch/i386/boot/bzImage .
			scp USERNAME@thoth.cs.pitt.edu:/u/OSLab/USERNAME/linux-2.6.23.1/System.map .
			```


	1. Installing the Rebuilt Kernel in QEMU
		1. As root in your QEMU instance (either by logging in or via `su`):

			```
			cp bzImage /boot/bzImage-devel
			cp System.map /boot/System.map-devel
			```
		
		1. Respond `y` to the prompts to overwrite. Please note that we are replacing the -devel files, the others are the original unmodified kernel so that if your kernel fails to boot for some reason, you will always have a clean version to boot QEMU.
			
		1. You need to update the bootloader when the kernel changes. To do this (do it every time you install a new kernel if you like) as root type:
			
			```
			lilo
			```
			
			`lilo` stands for LInux LOader, and is responsible for the menu that allows you to choose which version of the kernel to boot into.

	1. Booting into the Modified Kernel
		* As root, you simply can use the reboot command to cause the system to restart. When LILO starts (the red menu) make sure to use the arrow keys to select the linux(devel) option and hit enter.

1. Implement syscalls for synchronization
	1. You will first need to create a semaphore data type and the two operations we described in class, `down()` and `up()`. To encapsulate the semaphore, we'll make a simple struct that contains the integer value:

		```
		struct cs1550_sem {
			int value;
			//Some process queue of your devising
		};
		```
	
	1. You will then make two new system calls to operate on our semaphores that each have the following signatures:
		
		```
		asmlinkage long sys_cs1550_down(struct cs1550_sem *sem)
		```
		
		```
		asmlinkage long sys_cs1550_up(struct cs1550_sem *sem)
		```

		1. Sleeping
			* As part of your `down()` operation, there is a potential for the current process to sleep. In Linux, we can do that as part of a two-step process.
				* Mark the task as not ready (but can be awoken by signals):

					```
					set_current_state(TASK_INTERRUPTIBLE);
					```

				* Invoke the scheduler to pick a ready task:
					
					```
					schedule();
					```

		1. Waking Up
			* As part of up(), you potentially need to wake up a sleeping process. You can do this via:
			
				```
				wake_up_process(sleeping_task);
				```
				
				Where sleeping_task is a `struct task_struct` that represents a process put to sleep in your `down()`. You can get the current process's `task_struct` by accessing the global variable `current`. You may need to save these someplace.

		1. Atomicity
			* You need to implement the semaphore as part of the kernel because we need to do our increment or decrement and the following check on it atomically. In class we said that we could disable interrupts to achieve this. In Linux, this is no longer the preferred way of doing in kernel synchronization due to the fact that we might be running on a multicore or multiprocessor machine. For this project, you should use spin locks.

			* We can create a spinlock with a provided macro:
				
				```
				DEFINE_SPINLOCK(sem_lock);
				```
				
			* We can then surround our critical regions with the following:
				
				```
				spin_lock(&sem_lock);
				spin_unlock(&sem_lock);
				```	

	1. Adding new syscalls to the Linux kernel
		* To add a new syscall to the Linux kernel, there are three main files that need to be modified:
			1. `linux-2.6.23.1/kernel/sys.c` This file contains the actual implementation of the system calls.
			1. `linux-2.6.23.1/arch/i386/kernel/syscall_table.S` This file declares the number that corresponds to the syscalls.
			1. `linux-2.6.23.1/include/asm/unistd.h` This file exposes the syscall number to C programs which wish to use it.

1. Producer/Consumer implementation
	* Implementing and Building the prodcons Program
		* As you implement your syscalls, you are also going to want to test them via your co-developed `prodcons` program. The first thing we need is a way to use our new syscalls. We do this by using the `syscall()` function. The syscall function takes as its first parameter the number that represents which system call we would like to make. The remainder of the parameters are passed as the parameters to our syscall function. We have the syscall numbers exported as `#define`s of the form `__NR_syscall` via our `unistd.h` file that we modified when we added our syscalls.
			
		* We can write wrapper functions or macros to make the syscalls appear more natural in a C program. For example, you could write:
			
			```
			void down(cs1550_sem *sem) {
				syscall(__NR_cs1550_down, sem);
			}
			```
			
		* However if we try to build our code using gcc, the `linux/unistd.h` file that will be preprocessed in will be the one of the kernel version that is currently running and we will get an undefined symbol error. This is because the default `unistd.h` is not the one that we changed. What instead needs to be done is that we need to tell `gcc` to look for the new include files with the `-I` option:

			```
			gcc -m32 -o prodcons -I /u/OSLab/USERNAME/linux-2.6.23.1/include/ prodcons.c
			```

	* Shared Memory in prodcons

		* To make our buffer and our semaphores, what we need is for multiple processes to be able to share the same memory region. We can ask for N bytes of RAM from the OS directly by using `mmap()`:

			```
			void *ptr = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
			```

		* The return value will be an address to the start of this page in RAM. We can then steal portions of that page to hold our variables much as we did in the `malloc()` project from 449. For example, if we wanted two integers to be stored in the page, we could do the following to allocate them and initialize them:

			```
			int *first;
			int *second;
			first = ptr;
			second = first + 1;
			*first = 0;
			*second = 0;
			```

		* At this point we have one process and some RAM that contains our variables. But we now need to share that to a second process. The good news is that a mmap'ed region (with the `MAP_SHARED` flag) remains accessible in the child process after a `fork()`. So all we need to do for this to work is to do the `mmap()` in main before `fork()` and then use the variables in the appropriate way afterwards.

	* Running prodcons
		* We cannot run our prodcons program on thoth.cs.pitt.edu because its kernel does not have the new syscalls in it. However, we can test the program under QEMU once we have installed the modified kernel. We first need to download prodcons using scp as we did for the kernel. However, we can just run it from our home directory without any installation necessary.

## Submission Guidelines:
* **DO NOT** add any QEMU files to your git repository.
* **DO NOT** add any Linux kernel source or compiled kernels to your git repository.
* **DO NOT SUBMIT** any IDE package files.
* Your repository must inclued:
	* Your `prodcons.c` source file
	* A valid Makefile for prodcons
	* The `sys.c` that you modified to add your up/down system calls
	* The `syscall_table.S` that you modified to add your up/down system calls
	* The `unistd.h` that you modified to add your up/down system calls
* You must be able to compile your prodcons program by running `make`.
* You must be able to run your prodcons program by running `./prodcons`.
* You must fill out info_sheet.txt.
* Be sure to remember to push the latest copy of your code back to your GitHub repository before the the assignment is due.  At the deadline, the repositories will automatically be copied for grading.  Whatever is present in your GitHub repository at that time will be considered your submission for this assignment.

## Additional Notes/Hints:
* `printk()` is the version of `printf()` you can use for debugging messages from the kernel.
* In general, you can use some library standard C functions, but not all. If they do an OS call, they may not work.
* Try different buffer sizes to make sure your program doesn't deadlock.

## Grading Rubric:
* up/down syscalls are callable by a user program: 10
* up/down sleep and wakeup processes properly: 20
* up/down operate atomically: 20
* prodcons implements a solution to the producer/consumer problem: 25
* prodcons does not deadlock: 20
* Assignment info sheet/submission:  5
