#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#define OPT 1
#define CLOCK 2
#define AGING 3
#define WSCLOCK 4

void read_access(int i, unsigned int addr, unsigned char mode);
void read_tracefile(char *tracefile);
void init_frame();
void access_frame();
void my_opt(int cur, unsigned int index, unsigned char mode);
int find_next(int cur, unsigned int index);
void my_clock(unsigned int index, unsigned char mode);
void my_aging(unsigned int index, unsigned char mode);
void my_wsclock(int cur, unsigned int index, unsigned char mode);

struct accessStruct {
    unsigned int index;
    unsigned int offset;
    unsigned char mode;
};

struct frameStruct {
    unsigned int index;
    unsigned int valid;
    unsigned int dirty;
    unsigned int referenced;
    unsigned int tim;
};

int numframes = 0;
int algorithm = 0;
int refresh = 0;
int tau = 0;
int totalAccess = 0;
struct accessStruct *accessArray = NULL;
struct frameStruct *frameArray = NULL;
int hits = 0;
int faults = 0;
int writes = 0;
int clocks = 0;

int main(int argc, char *argv[]) {
    int opt;
    char *algo = NULL;
    char *tracefile = NULL;

    while ((opt = getopt(argc, argv, "n:a:r:t:")) != -1) {
        switch (opt) {
            case 'n':
                numframes = atoi(optarg);
                break;
            case 'a':
                algo = optarg;
                break;
            case 'r':
                refresh = atoi(optarg);
                break;
            case 't':
                tau = atoi(optarg);
                break;
            default:
                fprintf(stderr,\
                    "Usage: %s -n numframes -a opt|clock|aging|work [-r refresh] [-t tau] tractfile\n",\
                    argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    tracefile = argv[optind];
    if (!strcmp(algo, "opt")) {
        algorithm = OPT;
    }
    else if (!strcmp(algo, "clock")) {
        algorithm = CLOCK;
    }
    else if (!strcmp(algo, "aging")) {
        algorithm = AGING;
    }
    else if (!strcmp(algo, "work")) {
        algorithm = WSCLOCK;
    }
    else {
        exit(1);
    }
    //printf("%d\t%s\t%s\n", numframes, algo, tracefile);

    read_tracefile(tracefile);
    init_frame();
    access_frame();
    printf("%s\n", algo);
    printf("Number of frames:\t%d\n", numframes);
    printf("Total memory accesses:\t%d\n", totalAccess);
    printf("Total page faults:\t%d\n", faults);
    printf("Total page hits:\t%d\n", hits);
    printf("Total writes to disk:\t%d\n", writes);
    free(accessArray);
    free(frameArray);
    return 0;
}

void read_access(int i, unsigned int addr, unsigned char mode) {
    accessArray[i].index = addr & 0xfff;
    accessArray[i].offset = addr >> 12;
    accessArray[i].mode = mode;
}

void read_tracefile(char *tracefile) {
    FILE *file = fopen(tracefile, "rb");
    if (!file) {
        exit(1);
    }

    unsigned int addr = 0;
    unsigned char mode = 0;
    int size = 0;
    while (fscanf(file, "%x %c", &addr, &mode) == 2) {
        totalAccess++;
    }
    rewind(file);
    accessArray = (struct accessStruct*)malloc(totalAccess*sizeof(struct accessStruct));
    if (!accessArray) {
        exit(1);
    }

    int i = 0;
    while (fscanf(file, "%x %c", &addr, &mode) == 2) {
        read_access(i, addr, mode);
        i++;
    }

    fclose(file);
}

void init_frame() {
    frameArray = (struct frameStruct*)malloc(numframes*sizeof(struct frameStruct));
    if (!frameArray) {
        exit(1);
    }

    int i;
    for (i = 0; i < numframes; i++) {
        frameArray[i].valid = 0;
        frameArray[i].dirty = 0;
        frameArray[i].referenced = 0;
        frameArray[i].tim = 0;
    }
}

void access_frame() {
    int i, j, empty;
    unsigned int index;
    unsigned char mode;
    for (i = 0; i < totalAccess; i++) {
        index = accessArray[i].index;
        //printf("%d\n", index);
        mode = accessArray[i].mode;
        empty = -1;

        if (algorithm == AGING && i%refresh == 0) {
            for (j = 0; j < numframes; j++) {
                frameArray[j].referenced >>= 1;
            }
        }

        for (j = 0; j < numframes; j++) {
            if (frameArray[j].valid) {
                if (frameArray[j].index == index) {
                    hits++;
                    if (mode == 'W') {
                        frameArray[j].dirty = 1;
                    }
                    if (algorithm == OPT) {
                        frameArray[j].tim = find_next(i, index);
                    }
                    if (algorithm == CLOCK || algorithm == WSCLOCK) {
                        frameArray[j].referenced = 1;
                    }
                    if (algorithm == AGING) {
                        frameArray[j].referenced &= 0x80;
                    }
                    empty = 0;
                    break;
                }
            }
            else {
                empty = 1;
                break;
            }
        }

        if (empty == 1) {
            faults++;
            frameArray[j].valid = 1;
            frameArray[j].index = index;
            if (algorithm == OPT) {
                frameArray[j].tim = find_next(i, index);
            }
            if (algorithm == CLOCK || algorithm == WSCLOCK) {
                frameArray[j].referenced = 1;
            }
            if (algorithm == AGING) {
                frameArray[j].referenced = 0x80;
            }
            if (mode == 'W') {
                frameArray[j].dirty = 1;
            }
        }
        else if (empty == -1) {
            faults++;
            switch (algorithm) {
                case OPT:
                    my_opt(i, index, mode);
                    break;
                case CLOCK:
                    my_clock(index, mode);
                    break;
                case AGING:
                    my_aging(index, mode);
                    break;
                case WSCLOCK:
                    my_wsclock(i, index, mode);
                    break;
            }
        }
    }
}

void my_opt(int cur, unsigned int index, unsigned char mode) {
    int i;
    unsigned int max = 0;
    int j = -1;
    for (i = 0; i < numframes; i++) {
        if (frameArray[i].tim > max) {
            max = frameArray[i].tim;
            j = i;
        }
    }
    frameArray[j].index = index;
    if (frameArray[j].dirty) {
        writes++;
    }
    if (mode == 'R') {
        frameArray[j].dirty = 0;
    }
    else if (mode == 'W') {
        frameArray[j].dirty = 1;
    }
    frameArray[j].tim = find_next(cur, index);
}

int find_next(int cur, unsigned int index) {
    int i;
    for (i = cur+1; i < totalAccess; i++) {
        if (index == accessArray[i].index) {
            return i;
        }
    }
    return totalAccess;
}

void my_clock(unsigned int index, unsigned char mode) {
    int i = clocks;
    while (1) {
        if (!frameArray[i].referenced) {
            frameArray[i].index = index;
            frameArray[i].referenced = 1;
            if (frameArray[i].dirty) {
                writes++;
            }
            if (mode == 'R') {
                frameArray[i].dirty = 0;
            }
            else if (mode == 'W') {
                frameArray[i].dirty = 1;
            }
            break;
        }
        else {
            frameArray[i].referenced = 0;
        }
        i = (i+1)%numframes;
    }
    clocks = (i+1)%numframes;
}

void my_aging(unsigned int index, unsigned char mode) {
    int i;
    unsigned int min = 0x100;
    int j = -1;
    for (i = 0; i < numframes; i++) {
        if (frameArray[i].referenced < min) {
            min = frameArray[i].referenced;
            j = i;
        }
    }
    frameArray[j].index = index;
    frameArray[j].referenced = 0x80;
    if (frameArray[j].dirty) {
        writes++;
    }
    if (mode == 'R') {
        frameArray[j].dirty = 0;
    }
    else if (mode == 'W') {
        frameArray[j].dirty = 1;
    }
}

void my_wsclock(int cur, unsigned int index, unsigned char mode) {
    int i = clocks;
    int min = totalAccess;
    int j = -1;
    while (1) {
        if (!frameArray[i].referenced) {
            if (cur-frameArray[i].tim > tau && !frameArray[i].dirty) {
                frameArray[i].index = index;
                frameArray[i].referenced = 1;
                if (mode == 'R') {
                    frameArray[i].dirty = 0;
                }
                else if (mode == 'W') {
                    frameArray[i].dirty = 1;
                }
                break;
            }
            if (frameArray[i].dirty) {
                writes++;
                frameArray[i].dirty = 0;
            }
        }
        else {
            frameArray[i].tim = cur;
            frameArray[i].referenced = 0;
        }
        if (frameArray[i].tim < min) {
            min = frameArray[i].tim;
            j = i;
        }
        i = (i+1)%numframes;
        if (i == clocks) {
            frameArray[j].index = index;
            frameArray[j].referenced = 1;
            if (frameArray[i].dirty) {
                writes++;
            }
            if (mode == 'R') {
                frameArray[j].dirty = 0;
            }
            else if (mode == 'W') {
                frameArray[j].dirty = 1;
            }
            break;
        }
    }
    clocks = (i+1)%numframes;
}
