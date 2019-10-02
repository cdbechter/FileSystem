#ifndef LAB04FILESYSTEM_LAB04FILESYSTEM_H
#define LAB04FILESYSTEM_LAB04FILESYSTEM_H

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

/* block size, but num_blocks has a max of 10.5mb / block size */
#define BLOCK_SIZE 512
#define NUM_BLOCKS (10485760 / (BLOCK_SIZE + sizeof(int)))

/* File Type Enum */
typedef enum {
    file, dir
} fileType;

/* Directory Entry Struct */
typedef struct {
    char name[35];
    int start;
    int fileSize;
    fileType type: 1;
    int isOpen: 1;
} dirEntry;

/* Block Struct */
typedef struct {
    char file[BLOCK_SIZE];
    dirEntry dir[BLOCK_SIZE / sizeof(dirEntry)];
} block;

/* FAT32 struct */
typedef struct {
    int FAT[NUM_BLOCKS];
    block data[NUM_BLOCKS];
} drive;

/* File pointer struct */
typedef struct {
    dirEntry *mData;
    int ptr;
    int current;
} f_ptr;


/* Functions needed for the file system */
dirEntry *fileData(char *, drive *);
f_ptr *openFile (char *, drive *);
int clusterBlock(drive *);
void createFile(char *, fileType, drive *);
void writeFile(f_ptr *, char *, int, drive *);
void readFile(char *, f_ptr *, int, drive *);
void closeFile(f_ptr *);
void deleteFile(char *, drive *);


#endif
