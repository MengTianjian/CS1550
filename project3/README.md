# CS/COE 1550 Project 3

## Goal:
To gain a better understanding of page replacement algorithms by writing a program to run a simulation of several different algorithms on traces of memory references.

## Background:
In lecture, we have discussed several page replacement algorithms.  For this project, you will simulate running the following 4:

* OPT
	* Simulate what the optimal page replacement algorithm would choose if it had perfect knowledge
* Clock
	* Implement a circular queue improvement to the second chance algorithm as described in lecture
* Aging
	* Implement the use of an additional Referenced byte (8-bits) to implement aging
* WSClock
	* Discussed in lecture, specifically:
		* Use the line number of the file as your virtual time
		* Use a tau value specified via a command line argument to determine that a page is out of the working set
		* On a page fault with no free frames available:
			* Scan the page table looking at valid pages, each time continuing from where you left off previously
			* If you find a page that is referenced, record the current virtual time as the last used time for that page
			* If you find a page that is unreferenced, older than tau, and clean, evict it and stop
			* If you find a page that is unreferenced and dirty, write it out to disk and mark it as clean
			* If you make it through the whole page table, evict the page with the oldest timestamp

## Specification
1. Your simulation program should be written in C and run using the following command line arguments:

	```
	./vmsim –n numframes -a opt|clock|aging|work [-r refresh] [-t tau] tracefile;
	```

1. You should use a 32-bit address space, and all pages should be 4KiB in size.

1. Your program should run through the memory references in the specified tracefile and display the action taken for each address (hit, page fault – no eviction, page fault – evict clean, page fault – evict dirty).  When the trace is over, your program should print out summary statistics in the following format:

	```
	Clock
	Number of frames:       8
	Total memory accesses:  1000000
	Total page faults:      181856
	Total writes to disk:   29401
	```

1. You are provided with two sample memory traces. We will grade with two additional ones. The traces are available at `/u/OSLab/original/` in the files `swim.trace.gz` and `gcc.trace.gz`.  Each trace is gzip compressed, so you will have to copy each trace to your directory under `/u/OSLab/` and then decompress it like so:

	```
	gunzip swim.trace.gz
	```

1. Each line of the trace file contains a memory address in hexadecimal followed by a R or W character to indicate if that access was a read or a write. For example, `gcc.trace` trace starts like this:
	
	```
	0041f7a0 R
	13f5e2c0 R
	```

	You can parse each line with the following code:
	
	```
	unsigned int address;
	char mode;

	fscanf(file, "%x %c", &addr, &mode);
	```

1. In addition to writing the code for your simulation, you will need to construct a writeup detailing the results of running the simulations with graphs plotting the number of page faults versus the number of frames and your conclusions on which algorithm would be best to use in a real OS.  This file should be a pdf named `writeup.pdf`.
	* For Aging, you have a refresh parameter to set. Try to find a good refresh period that works well. You do not need to find the absolute minimum, just approximately how long to wait. Plot your results in a graph and discuss why your choice of refresh seemed to be the best.
	* For WSClock, you have to try to determine a good choice of tau. Plot your results. 
	* For each of your four algorithms (with Aging using the refresh you determined and WSClock with the tau you determined), describe in a document the resulting page fault statistics for 8, 16, 32, and 64 frames. Use this information to determine which algorithm you think might be most appropriate for use in an actual operating system. Use OPT as the baseline for your comparisons.

## Submission Guidelines:
* **DO NOT** add any of the trace files to your git repository.
* **DO NOT SUBMIT** any IDE package files.
* Your repository must include:
	* Your `vmsim.c` source file
	* A valid Makefile for vmsim
	* Your `writeup.pdf` file 
* You must be able to compile your prodcons program by running `make`.
* You must be able to run your simulation program as specified above.
* You must fill out info_sheet.txt.
* Be sure to remember to push the latest copy of your code back to your GitHub repository before the the assignment is due.  At the deadline, the repositories will automatically be copied for grading.  Whatever is present in your GitHub repository at that time will be considered your submission for this assignment.

## Additional Notes/Hints:
* Implementing OPT in a naïve fashion will lead to unacceptable performance. It should not take more than 5 minutes to run your program.

## Grading Rubric:
* Trace file parsing: 5
* OPT works properly: 10
* OPT simulation is reasonable efficient: 10
* Clock simulation works as specified: 15
* Aging simulation works as specified: 20
* WSClock simulation works as specified: 25
* Writeup addresses all specified criteria and makes reasonable conclusions: 20
* Assignment info sheet/submission:  5
