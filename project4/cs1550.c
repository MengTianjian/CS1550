/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

// Free space tracking
#define MAX_DISK_SIZE 5 * (1 << 20)
#define MAX_FAT_ENTRIES (MAX_DISK_SIZE/BLOCK_SIZE)

// struct cs1550_file_alloc_table_block {
// 	short table[MAX_FAT_ENTRIES];
// };
//
// typedef struct cs1550_file_alloc_table_block cs1550_fat_block;

/* Helper functions */
void get_path(const char *path, char *dname, char *fname, char *fext);
long check_dir(char *dname);
int check_file(char *dname, char *fname, char *fext);
cs1550_root_directory* open_root(void);
cs1550_directory_entry* open_dir(char *dname);
cs1550_disk_block* open_block(long nStartBlock);
void write_root(cs1550_root_directory *root);
void write_dir(cs1550_directory_entry *dir, char *dname);
void write_block(cs1550_disk_block *block, long nStartBlock);
long next_free_block(void);
void make_dir(long nStartBlock, char *dname);
void make_file(long nStartBlock, char *dname, char *fname, char *fext);
void read_file(char *buf, off_t offset, size_t fsize, long nStartBlock);
void write_file(const char *buf, off_t offset, size_t size, char *dname, int i);

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//is path the root dir?
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else
	{
		char dname[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char fext[MAX_EXTENSION + 1];
		get_path(path, dname, fname, fext);
		if (strcmp(dname, "\0") != 0 && strcmp(fname, "\0") == 0) //Check if name is subdirectory
		{
			if (check_dir(dname) != -1)
			{
				//Might want to return a structure with these fields
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
			}
			else
				res = -ENOENT;
		}
		else if (strcmp(fname, "\0") != 0) //Check if name is a regular file
		{
			int i = check_file(dname, fname, fext);
			if (i != -1)
			{
				cs1550_directory_entry *dir = open_dir(dname);
				//regular file, probably want to be read and write
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 1; //file links
				stbuf->st_size = dir->files[i].fsize;
				res = 0; // no error
				free(dir);
			}
			else
				res = -ENOENT;
		}
		else //Else return that path doesn't exist
			res = -ENOENT;
	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	int res = 0;
	if (strcmp(path, "/") == 0) //If it is root directory
	{
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		cs1550_root_directory *root = open_root();
		int i;
		for (i = 0; i < root->nDirectories; i++)
			filler(buf, root->directories[i].dname, NULL, 0);
		free(root);
	}
	else
	{
		char dname[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char fext[MAX_EXTENSION + 1];
		get_path(path, dname, fname, fext);
		if (strcmp(dname, "\0") != 0 && strcmp(fname, "\0") == 0) //Check if name is subdirectory
		{
			int nStartBlock = check_dir(dname);
			if (nStartBlock != -1) //If the directory exists
			{
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				cs1550_directory_entry *dir = open_dir(dname);
				int i;
				for (i = 0; i < dir->nFiles; i++)
				{
					if (strcmp(dir->files[i].fext, "\0") == 0) //If the file doesn't have extension
						filler(buf, dir->files[i].fname, NULL, 0);
					else
					{
						char *fname_dot_fext = (char *)malloc((MAX_FILENAME+MAX_EXTENSION+2)*sizeof(char));
						strcpy(fname_dot_fext, dir->files[i].fname);
						strcat(fname_dot_fext, ".");
						strcat(fname_dot_fext, dir->files[i].fext);
						filler(buf, fname_dot_fext, NULL, 0);
					}
				}
				free(dir);
			}
			else
				res = -ENOENT;
		}
		else
			res = -ENOENT;
	}
	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) mode;

	int res = 0;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	get_path(path, dname, fname, fext);
	if (strlen(dname) > MAX_FILENAME) //If directory name is too long
		res = -ENAMETOOLONG;
	else
	{
		if (strcmp(fname, "\0") != 0) //If it is not in root directory
			res = -EPERM;
		else
		{
			long nStartBlock = check_dir(dname);
			if (nStartBlock != -1) //If it already exists
				res = -EEXIST;
			else
			{
				cs1550_root_directory *root = open_root();
				if (root->nDirectories >= MAX_DIRS_IN_ROOT) //If directory number is more than max
					res = -EPERM;
				else
				{
					nStartBlock = next_free_block();
					if (nStartBlock == -1) //If no free block
						res = -ENOLCK;
					else
						make_dir(nStartBlock, dname);
				}
				free(root);
			}
		}
	}
	return res;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	int res = 0;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	get_path(path, dname, fname, fext);
	if (strcmp(fname, "\0") == 0) //If it is in root directory
		res = -EPERM;
	else
	{
		if (strlen(fname) > MAX_FILENAME || strlen(fext) > MAX_EXTENSION) //If file name or extension name is too long
			res = -ENAMETOOLONG;
		else
		{
			long nStartBlock = check_dir(dname);
			if (nStartBlock == -1) //If directory does not exist
				res = -EPERM;
			else
			{
				cs1550_directory_entry *dir = open_dir(dname);
				if (dir->nFiles >= MAX_FILES_IN_DIR) //If file number is more than max
					res = -EPERM;
				else
				{
					if (check_file(dname, fname, fext) != -1) //If file already exists
						res = -EEXIST;
					else
					{
						nStartBlock = next_free_block();
						if (nStartBlock == -1) //If no free block
							res = -ENOLCK;
						else
							make_file(nStartBlock, dname, fname, fext);
					}
				}
				free(dir);
			}
		}
	}
	return res;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) size;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	int res = 0;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	get_path(path, dname, fname, fext);
	if (strcmp(fname, "\0") == 0) //If path is a directory
		res = -EISDIR;
	else
	{
		if (check_dir(dname) == -1) //If directory doesn't exist
			res = -ENOENT;
		else
		{
			int i = check_file(dname, fname, fext);
			if (i == -1) //If file doesn't exist
				res = -ENOENT;
			else
			{
				cs1550_directory_entry *dir = open_dir(dname);
				int fsize = dir->files[i].fsize;
				if (offset > fsize) //If offset is beyond the file size
					res = -EFBIG;
				else
				{
					int nStartBlock = dir->files[i].nStartBlock;
					read_file(buf, offset, fsize, nStartBlock);
					res = fsize;
				}
				free(dir);
			}
		}
	}
	return res;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) size;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	int res = 0;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	get_path(path, dname, fname, fext);
	if (strcmp(fname, "\0") == 0) //If path is a directory
		res = -EISDIR;
	else
	{
		if (check_dir(dname) == -1) //If directory doesn't exist
			res = -ENOENT;
		else
		{
			int i = check_file(dname, fname, fext);
			if (i == -1) //If file doesn't exist
				res = -ENOENT;
			else
			{
				cs1550_directory_entry *dir = open_dir(dname);
				int fsize = dir->files[i].fsize;
				if (offset > fsize) //If offset is beyond the file size
					res = -EFBIG;
				else
				{
					fsize = size+offset;
					write_file(buf, offset, fsize, dname, i);
					res = fsize;
				}
				free(dir);
			}
		}
	}
	return res;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


/* Helper functions */
void get_path(const char *path, char *dname, char *fname, char *fext)
{
	dname[0] = '\0';
	fname[0] = '\0';
	fext[0] = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
}

//If directory exists, return its start block; else return -1
long check_dir(char *dname)
{
	long res = -1;
	cs1550_root_directory *root = open_root();
	int i;
	for (i = 0; i < root->nDirectories; i++)
	{
		if (strcmp(dname, root->directories[i].dname) == 0)
		{
			res = root->directories[i].nStartBlock;
			break;
		}
	}
	free(root);
	return res;
}

//If file exists, return its index in directory; else return -1
int check_file(char *dname, char *fname, char *fext)
{
	long res = -1;
	long nStartBlock = check_dir(dname);
	if (nStartBlock != -1)
	{
		cs1550_directory_entry *dir = open_dir(dname);
		int i;
		for (i = 0; i < dir->nFiles; i++)
		{
			if (strcmp(fext, dir->files[i].fext) == 0 && strcmp(fname, dir->files[i].fname) == 0)
			{
				res = i;
				break;
			}
		}
		free(dir);
	}
	return res;
}

//Open the disk file and return the root.
cs1550_root_directory* open_root(void)
{
	cs1550_root_directory *root = malloc(BLOCK_SIZE);
	FILE *f = fopen(".disk", "rb");
	if (f != NULL)
	{
		fread(root, BLOCK_SIZE, 1, f);
		fclose(f);
	}
	return root;
}

//Open the disk and return the directory required.
cs1550_directory_entry* open_dir(char *dname)
{
	cs1550_directory_entry *dir = malloc(BLOCK_SIZE);
	long nStartBlock = check_dir(dname);
	FILE *f = fopen(".disk", "rb");
	if (f != NULL)
	{
		fseek(f, nStartBlock*BLOCK_SIZE, SEEK_SET);
		fread(dir, BLOCK_SIZE, 1, f);
		fclose(f);
	}
	return dir;
}

//Open the disk and return the block required.
cs1550_disk_block* open_block(long nStartBlock)
{
	cs1550_disk_block *block = malloc(BLOCK_SIZE);
	FILE *f = fopen(".disk", "rb");
	if (f != NULL)
	{
		fseek(f, nStartBlock*BLOCK_SIZE, SEEK_SET);
		fread(block, BLOCK_SIZE, 1, f);
		fclose(f);
	}
	return block;
}

//Open the disk and return the index of next free block if any; else return -1
long next_free_block(void)
{
	long nStartBlock = -1;
	FILE *f = fopen(".disk", "rb");
	if (f != NULL)
	{
		cs1550_disk_block *block = open_block(MAX_FAT_ENTRIES-1);
		nStartBlock = block->nNextBlock;
		if (nStartBlock == 0)
		{
			nStartBlock++;
			block->nNextBlock++;
		}
		if (nStartBlock != -1)
		{
			block->nNextBlock++;
			if (block->nNextBlock == MAX_FAT_ENTRIES-1)
			{
				block->nNextBlock = -1;
			}
			write_block(block, MAX_FAT_ENTRIES-1);
		}
		free(block);
		fclose(f);
	}
	return nStartBlock;
}

//Write new root to the disk.
void write_root(cs1550_root_directory *root)
{
	FILE *f = fopen(".disk", "rb+");
	fseek(f, 0, SEEK_SET);
	fwrite(root, BLOCK_SIZE, 1, f);
	fclose(f);
}

//Write new directory to the disk.
void write_dir(cs1550_directory_entry *dir, char *dname)
{
	long nStartBlock = check_dir(dname);
	FILE *f = fopen(".disk", "rb+");
	fseek(f, nStartBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(dir, BLOCK_SIZE, 1, f);
	fclose(f);
}

//Write new block to the disk.
void write_block(cs1550_disk_block *block, long nStartBlock)
{
	FILE *f = fopen(".disk", "rb+");
	fseek(f, nStartBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(block, BLOCK_SIZE, 1, f);
	fclose(f);
}

//Create the new directory and update the disk file
void make_dir(long nStartBlock, char *dname)
{
	cs1550_root_directory *root = open_root();
	strcpy(root->directories[root->nDirectories].dname, dname);
	root->directories[root->nDirectories].nStartBlock = nStartBlock;
	root->nDirectories++;
	write_root(root);
	free(root);
}

//Create the new file and update the disk file
void make_file(long nStartBlock, char *dname, char *fname, char *fext)
{
	cs1550_directory_entry *dir = open_dir(dname);
	strcpy(dir->files[dir->nFiles].fname, fname);
	strcpy(dir->files[dir->nFiles].fext, fext);
	dir->files[dir->nFiles].nStartBlock = nStartBlock;
	dir->files[dir->nFiles].fsize = 0;
	dir->nFiles++;
	write_dir(dir, dname);
	cs1550_disk_block *block = open_block(nStartBlock);
	block->nNextBlock = -1;
	write_block(block, nStartBlock);
	free(block);
	free(dir);
}

//Read file to buffer
void read_file(char *buf, off_t offset, size_t fsize, long nStartBlock)
{
	cs1550_disk_block *block = open_block(nStartBlock);
	int i;
	for (i = 0; i < offset / BLOCK_SIZE; i++)
	{
		nStartBlock = block->nNextBlock;
		free(block);
		block = open_block(nStartBlock);
	}
	for (i = offset; i < fsize; i++)
	{
		if (i % BLOCK_SIZE == 0)
		{
			nStartBlock = block->nNextBlock;
			free(block);
			block = open_block(nStartBlock);
		}
		buf[i - offset] = block->data[i % BLOCK_SIZE];
	}
	free(block);
}

//Write data to the file
void write_file(const char *buf, off_t offset, size_t size, char *dname, int i)
{
	cs1550_directory_entry *dir = open_dir(dname);
	dir->files[i].fsize = size;
	long nStartBlock = dir->files[i].nStartBlock;
	cs1550_disk_block *block = open_block(nStartBlock);
	for (i = 0; i < offset / BLOCK_SIZE; i++)
	{
		nStartBlock = block->nNextBlock;
		free(block);
		block = open_block(nStartBlock);
	}
	for (i = offset; i < size; i++)
	{
		if (i % BLOCK_SIZE == 0)
		{
			if (block->nNextBlock == -1)
			{
				block->nNextBlock = next_free_block();
				write_block(block, nStartBlock);
				nStartBlock = block->nNextBlock;
				block = open_block(nStartBlock);
				block->nNextBlock = -1;
			}
			else
			{
				write_block(block, nStartBlock);
				nStartBlock = block->nNextBlock;
				block = open_block(nStartBlock);
			}
		}
		block->data[i % BLOCK_SIZE] = buf[i - offset];
	}
	block->nNextBlock = -1;
	write_block(block, nStartBlock);
	write_dir(dir, dname);
	free(block);
	free(dir);
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
