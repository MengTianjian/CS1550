#include <stdio.h>
#include <stdlib.h>
#include <linux/sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>

struct Node {
	struct task_struct *task;
	struct Node *next;
};

struct cs1550_sem {
	int value;
	struct Node *head;
	struct Node *tail;
};

void down(struct cs1550_sem *sem) {
	syscall(__NR_sys_cs1550_down, sem);
}

void up(struct cs1550_sem *sem) {
	syscall(__NR_sys_cs1550_up, sem);
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		printf("Wrong arguments\n");
		return 1;
	}
	int prod = atoi(argv[1]);
	int cons = atoi(argv[2]);
	int bufferSize = atoi(argv[3]);

	void *sem_ptr = mmap(NULL, 3*sizeof(struct cs1550_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	struct cs1550_sem *empty = (struct cs1550_sem*)sem_ptr;
	struct cs1550_sem *full = (struct cs1550_sem*)sem_ptr + 1;
	struct cs1550_sem *mutex = (struct cs1550_sem*)sem_ptr + 2;
	empty->value = bufferSize;
	full->value = 0;
	mutex->value = 1;

	void *ptr = mmap(NULL, (bufferSize+3)*sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	int *n = (int*)ptr;
	int *in = (int*)ptr + 1;
	int *out = (int*)ptr + 2;
	int *buffer = (int*)ptr + 3;
	*n = bufferSize;
	*in = 0;
	*out = 0;

	int i;
	for (i = 0; i < bufferSize; i++) {
		buffer[i] = 0;
	}

	for (i = 0; i < prod; i++) {
		if (fork() == 0) {
			int p;
			while (1) {
				down(empty);
				down(mutex);
				p = *in;
				buffer[*in] = p;
				printf("Chef %c Produced: Pancake %d\n", i+65, p);
				*in = (*in + 1) % *n;
				up(mutex);
				up(full);
			}
		}
	}

	for (i = 0; i < cons; i++) {
		if (fork() == 0) {
			int c;
			while (1) {
				down(full);
				down(mutex);
				c = buffer[*out];
				printf("Consumer %c Consumed: Pancake %d\n", i+65, c);
				*out = (*out + 1) % *n;
				up(mutex);
				up(empty);
			}
		}
	}
	wait();
	return 0;
}
