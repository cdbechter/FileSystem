/* Lab04 - Filesystems. This filesystems employs a simple linked list style of coding
    to create directory, create a file, write to the file, read the file, close the file, 
    delete the file, and unmap the virtual drive.
*/

/* to make a blank file using Mac's terminal, the command is 
    dd if=/dev/zero of=5MBDrive bs=5000000 count=1
    the of="whatever you wanna name it" and bs=Number of bytes -- 5 mil for 5mb
*/

#include "Lab04filesystem.h"

/* start with null directory entry */
dirEntry NULL_dirEntry = {0};

int main(int args, char * argv[]) {
    f_ptr *f;
    /* opens drive provided by the user, only 5MBDrive or 10MBDrive or 
        shows the below error to help the user pick a drive */
    if(args < 2) {
        printf("You must pick a drive, 5MBDrive or 10MBDrive. Please try again.\n");
        exit(0);
    } else {
        int fd = open(argv[1], O_RDWR);
        if(fd == -1) {
            printf("Error opening drive.\n");
            exit(1);
        }

        /* mapping memory for directory drive */
        drive *dirDrive = mmap(0, sizeof(drive), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(dirDrive == MAP_FAILED) {
            printf("Error with mmap.\n");
            exit(1);
        }
        close(fd);

        /* sets the start of the root directory */
        dirDrive->FAT[0] = -1;

        /* create a testMessage directory and testfile */
        createFile("/dir", dir, dirDrive);
        createFile("/dir/testFile.txt", file, dirDrive);
        char *testMessage = "There can be only 1!!!";

        /* opens file, writes to file, closes file  */
        f = openFile("/dir/testFile.txt", dirDrive);
        writeFile(f, testMessage, strlen(testMessage), dirDrive);
        closeFile(f);
        
        /* open, read, close file (only way i could print it to screen) */
        f = openFile("/dir/testFile.txt", dirDrive);
        char textOutput[strlen(testMessage) + 1];
        readFile(textOutput, f, strlen(testMessage), dirDrive);
        closeFile(f);
        
        /* prints the contents of testFile */
        printf("%s\n", textOutput);
        
        /* deletes testFile */
        deleteFile("/dir/testFile.txt", dirDrive);

        /* try to open deleted file */
        printf("Try to reopen the file we just deleted. Should get, it doesn't exist.\n");
        openFile("/dir/testFile.txt", dirDrive);

        /* unmaps memory */
        if(munmap(dirDrive, sizeof(drive)) == -1) {
            printf("Error unmapping.\n");
            exit(1);
        }
    }
    return 0;
}

/* gets the data to put into file */
dirEntry *fileData(char *filePath, drive *dirDrive) {
    int found = 0, i, block;
    fileType last = dir;

    /* copy filePath to copyFP */
    char *copyFP = (char *) malloc(sizeof(char *) * strlen(filePath) + 1);
    strcpy(copyFP, filePath);
    
    /* Returns the mData of the root dir. */
    if (strcmp(copyFP, "") == 0) {
        dirEntry *root = (dirEntry *) malloc(sizeof(dirEntry));
        root->type = dir;
        root->start = 0;
        return root;
    }

    /* start with null pointer for directory data */
    dirEntry *mData = &NULL_dirEntry;

    /* splits path into tokens and cycles through them. */
    for (char *token = strtok(copyFP, "/"); token != NULL; token = strtok(NULL, "/")) {
        /* returns NULL if last in file */
        if (last == file) {
            free(copyFP);
            return NULL;
        }
        /* cycles through the entire directory */
        for (block = mData->start; block != -1 && !found; block = dirDrive->FAT[block]) {
            for (i = 0; i < BLOCK_SIZE / sizeof(dirEntry) && !found; i++) {
                if (strcmp(dirDrive->data[block].dir[i].name, token) == 0) {
                    /* Name matches. */
                    last = dirDrive->data[block].dir[i].type;
                    mData = &(dirDrive->data[block].dir[i]);
                    found++;
                }
            }
        }
        /* nothing found means file doesn't exist. */
        if (found == 0) {
            free(copyFP);
            return NULL;
        }
        found = 0;
    }
    /* file not found */
    if (mData == &NULL_dirEntry) {
        free(copyFP);
        return NULL;
    }
    free(copyFP);
    return mData;
}

/* Pointer file to Open the File */
f_ptr *openFile(char *filePath, drive *dirDrive) {
    /* copy filepath to copyFP */
    char *copyFP = (char *) malloc(sizeof(char *) * strlen(filePath) + 1);
    strcpy(copyFP, filePath);
    dirEntry *mData = fileData(copyFP, dirDrive);

    /* error handling for doesn't exist or open already */
    if (mData == NULL) {
        printf("File doesn't exist.\n");
        free(copyFP);
        return NULL;
    } else if (mData->isOpen) {
        free(copyFP);
        printf("File is already open.\n");
        return NULL;
    }

    /* file pointer is created and returned */
    f_ptr *file = (f_ptr *) malloc(sizeof(f_ptr));
    file->mData = mData;
    file->ptr = 0;
    file->current = file->mData->start;
    file->mData->isOpen = 1;

    free(copyFP);
    return file;
}

/* Finds the first empty block, if there are any empty blocks and returns that 
    empty blocks FAT number while also setting it to occupied. */
int clusterBlock(drive *dirDrive) {
    int i;
    for (i = 1; i < NUM_BLOCKS; i++) {
        if (dirDrive->FAT[i] == 0) {
            dirDrive->FAT[i] = -1;
            return i;
        }
    }
    /* Returns -1 if no blocks available */
    return -1;
}

/* Create File Function */
void createFile(char *filePath, fileType type, drive *dirDrive) {
    int i, j;
    int dirStart;

    /* copy filePath to copyFP*/
    char *copyFP = (char *) malloc(sizeof(char *) * strlen(filePath) + 1); 
    strcpy(copyFP, filePath);

    /* checks if file already exists. */
    if (fileData(copyFP, dirDrive) != NULL) {
        printf("Directory and/or File exists.\n");
        free(copyFP);
        return;
    }

    /* splits the name of the file */
    char *name = strrchr(copyFP, '/');
    if (name == NULL) {
        free(copyFP);
        return;
    } else {
        *name = '\0';
        name = name + 1;
    }

    dirEntry *dirmData = fileData(copyFP, dirDrive);
    /* error handling for existing file otherwise start the block */
    if (dirmData == NULL) {
        printf("Directory doesn't exist.\n");
        free(copyFP);
        return;
    } else {
        dirStart = dirmData->start;
    }

    /* creates block for the new file. */
    int fileStart = clusterBlock(dirDrive);
    /* error handling */
    if (fileStart == -1) {
        free(copyFP);
        printf("Unable to create block.\n");
        return;
    }
    /* finds first empty slot in the directory */
    for (j = 0; dirStart != -1; dirStart = dirDrive->FAT[dirStart]) {
        for (i = 0; i < BLOCK_SIZE / sizeof(dirEntry); i++) {
            if (dirDrive->data[dirStart].dir[i].name[0] == '\0') {
                dirDrive->data[dirStart].dir[i].start = fileStart;
                dirDrive->data[dirStart].dir[i].fileSize = 0;
                dirDrive->data[dirStart].dir[i].type = type;
                strcpy(dirDrive->data[dirStart].dir[i].name, name);
                free(copyFP);
                return;
            }
        }
    }

    /* creates new directory cluster */
    int newDirBlock = clusterBlock(dirDrive);
    /* error handling */
    if (newDirBlock == -1) {
        free(copyFP);
        printf("Unable to create block.\n");
        return;
    }
    /* fills in the file for the new block. */
    dirDrive->FAT[dirStart] = newDirBlock;
    dirDrive->data[newDirBlock].dir[0].start = fileStart;
    dirDrive->data[newDirBlock].dir[0].fileSize = 0;
    dirDrive->data[newDirBlock].dir[0].type = type;
    strcpy(dirDrive->data[newDirBlock].dir[0].name, name);
    free(copyFP);
}

/* Write File Function -- just writes data to the file */
void writeFile(f_ptr *file, char *data, int len, drive *dirDrive) {
    /* readData determines how much has already been read while dataLeft is the still 
        to be written data */
    int readData = 0;
    int dataLeft;

    /* finds out how much data is left to write */
    if(file->ptr + len > BLOCK_SIZE) {
        dataLeft = BLOCK_SIZE - file->ptr;
    } else {
        dataLeft = len;
    }

    /* cycle through all the data till there is none left to write */
    while (len > 0) {
        /* copyFP dataLeft into block */
        memcpy(&(dirDrive->data[file->current].file[file->ptr]), data + readData, dataLeft);
        /* length is decremented by data left, file pointer, data read, and file size are 
            incremented by the data left to write */
        len -= dataLeft;
        file->ptr += dataLeft;
        readData += dataLeft;
        file->mData->fileSize += dataLeft;
        /* check if its the end of block, add a new block if so, move to the new 
            block, reset file pointer */
        if (file->ptr == BLOCK_SIZE) {
            dirDrive->FAT[file->current] = clusterBlock(dirDrive);
            file->current = dirDrive->FAT[file->current];
            file->ptr = 0;
        }
        /* finds out how much data is left to write */
        if(len > BLOCK_SIZE) {
            dataLeft = BLOCK_SIZE;
        } else {
            dataLeft = len;
        }
    }
}

/* Reads the data from the file. */
void readFile(char *dest, f_ptr *file, int len, drive *dirDrive) {
    /* readData determines how much has already been read while dataLeft is the still 
        to be written data */    
    int readData = 0;
    int dataLeft;

    /* clears destination */
    memset(dest, 0, len);

    /* dataLeft to be read */
    if(file->ptr + len > BLOCK_SIZE) {
        dataLeft = BLOCK_SIZE - file->ptr;
    } else {
        dataLeft = len;
    }

    /* cycles through the blocks of data  till nothing left to read */
    while (len > 0 && file->current != -1) {
        /* copyFP data into destination. */
        memcpy(dest + readData, &(dirDrive->data[file->current].file[file->ptr]), dataLeft);
        /* readData increments, dataLeft decrements as well as the file pointer */
        readData += dataLeft;
        len -= dataLeft;
        file->ptr += dataLeft;
        /* checks if the end of the block has been reached, if so, reset file pointer, 
            goto the next block */
        if (file->ptr == BLOCK_SIZE) {
            file->ptr = 0;
            file->current = dirDrive->FAT[file->current];
        }
        /* dataLeft to be read */
        if(len > BLOCK_SIZE) {
            dataLeft = BLOCK_SIZE;
        } else {
            dataLeft = len;
        }
    }
}

/* Close File Function */
void closeFile(f_ptr *file) {
    /* sets the file to closed and frees it's pointer */
    file->mData->isOpen = 0;
    free(file);
}

/* Delete File Function */
void deleteFile(char *filePath, drive *dirDrive) {
    int i, j;
    
    /* copy filepath to copyFP */
    char *copyFP = (char *) malloc(sizeof(char *) * strlen(filePath) + 1);
    strcpy(copyFP, filePath);
    
    /* grabs the file */
    dirEntry *file = fileData(copyFP, dirDrive);
    
    /* error handling if the file is open or doesn't exist. */
    if (file->isOpen) {
        printf("File is currently in use.\n");
        free(copyFP);
        return;
    } else if(file == NULL) {
        printf("File doesn't exist exist.\n");
        free(copyFP);
        return;
    }

    /* Sets all of the blocks free, deletes file dirEntry and free copyFP */
    for (i = file->start, j = -1; i != -1; i = j) {
        j = dirDrive->FAT[i];
        dirDrive->FAT[i] = 0;
    }
    memset(file, 0, sizeof(dirEntry));
    free(copyFP);
}