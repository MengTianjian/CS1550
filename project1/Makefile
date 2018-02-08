default: driver

driver:
	gcc -o library.o -c library.c
	gcc -o driver.o -c driver.c
	gcc -o driver driver.o library.o

clean:
	-rm -f library.o
	-rm -f driver.o
	-rm -f driver
