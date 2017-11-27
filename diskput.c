#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

unsigned int BUFFER_SIZE = 1024;
unsigned int bytesPerSector = 512;
unsigned int totalSectorCount = 0;

int total_disk_space(char *bytes) {
    int totalDiskSpace = 0;                             

    totalDiskSpace = totalSectorCount * bytesPerSector; //Compute total disk space using bytes per sector and sectors

    return totalDiskSpace;
}

int free_disk_space(char *bytes) {
    int freeDiskSpace;
    int highBits;
    int lowBits;
    int entry;

    int i = 0;
    for (i = 2; i <= totalSectorCount - 32; i++) {      //For each sector
        if (i % 2 == 0) {                               //If even
            lowBits = bytes[bytesPerSector + (3 * i / 2)] & 0xFF;
            highBits = bytes[bytesPerSector + 1 + (3 * i / 2)] & 0x0F;
            entry = ((highBits << 8) | lowBits);
        } else {                                        //If odd
            lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xF0;
            highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0xFF;
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0x000) ++freeDiskSpace;             //If the entry
    }

    freeDiskSpace = freeDiskSpace * bytesPerSector;

    return freeDiskSpace;
}

int checkForDuplicate(char *bytes, char *fileName) {
    int rootDir = bytesPerSector * 19;
    int dataDir = bytesPerSector * 33;

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {
        if(bytes[i] == 0x00) break;
        if(bytes[i] == 0xE5) continue;
        if(bytes[i + 11] == 0x08) continue;
        if(bytes[i + 11] == 0x0F) continue;

        char *rootFile = malloc(sizeof(BUFFER_SIZE));
        char *fileExt = malloc(sizeof(BUFFER_SIZE));
        int j = 0;
        for(j = 0; j < 8; j++) {
            if (bytes[i + j] == ' ') break;
            rootFile[j] = bytes[i + j];
        }
        for(j = 0; j < 3; j++) {
            if (bytes[i + j + 8] == ' ') break;
            if (j == 0) strcat(rootFile, ".");
            fileExt[j] = bytes[i + j + 8];
        }
        strcat(rootFile, fileExt);
        if(strcmp(fileName, rootFile) == 0) return 1;
    }
    return 0;
}

int freeFatEntry(char *bytes) {
    int entry;
    int i = 0;
    for (i = 2; i <= totalSectorCount - 32; i++) {      //For each sector
        if (i % 2 == 0) {                               //If even
            int lowBits = bytes[bytesPerSector + (3 * i / 2)] & 0xFF;
            int highBits = bytes[bytesPerSector + 1 + (3 * i / 2)] & 0x0F;
            entry = ((highBits << 8) | lowBits);
        } else {                                        //If odd
            int lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xF0;
            int highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0xFF;
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0x000) return i;
    }
    printf("ERROR: Unable to find free entry\n");
    exit(1);
}

int createRootFile(char *bytes, char *fileName, int fileSize, int entry) {
    printf("Find a free root entry\n");
    fflush(stdout);
    int rootDir = bytesPerSector * 19;
    int dataDir = bytesPerSector * 33;
    int i = 0;
    for (i = rootDir; i < dataDir; i += 32) {
        if (bytes[i] == 0x00) break;
        if (i + 32 >= dataDir) {
            printf("ERROR: Unable to find free root entry\n");
            exit(1);
        }
    }

    printf("Choosing entry %d\n", entry);


    printf("Setting first logical cluster\n");
    fflush(stdout);
    bytes[i + 26] = entry & 0x00FF;
    bytes[i + 27] = ((entry) & 0xFF00) >> 8;
    
    int firstLogicalCluster = ((bytes[i + 27] << 8) & 0xFF) | (bytes[i + 26] & 0xFF);
    printf("first logical cluster  %d\n", firstLogicalCluster);

    printf("Setting file name\n");
    fflush(stdout);
    int j = 0;
    for(j = 0; j < 20; j++) {
        if(fileName[j] == '.') {
            int k = 0;
            for(k = 0; k < 3; k++) bytes[i + k + 8] = fileName[j + k + 1];
            break;
        }
        if(j < 8) bytes[i + j] = fileName[j];
    }

    printf("Setting file size\n");
    fflush(stdout);
    bytes[i + 28] = (fileSize & 0x000000FF);
    bytes[i + 29] = (fileSize & 0x0000FF00) >> 8;
    bytes[i + 30] = (fileSize & 0x00FF0000) >> 16;
    bytes[i + 31] = (fileSize & 0xFF000000) >> 24;

    printf("Label attribute as file\n");
    fflush(stdout);
    bytes[i + 11] = 0x00;

    printf("Setting creation date\n");
    fflush(stdout);
    time_t currentTime = time(NULL);
    struct tm *curTime = localtime(&currentTime);
    bytes[i + 17] = (((curTime->tm_year - 80) & 0b01111111) << 1);
    bytes[i + 17] = (((curTime->tm_mon + 1) & 0b00001000) >> 3) | bytes[i + 17];
    bytes[i + 16] = (((curTime->tm_mon + 1) & 0b00000111) << 5);
    bytes[i + 16] = ((curTime->tm_mday & 0b00011111)) | bytes[i + 16];
    bytes[i + 15] = ((curTime->tm_hour & 0b00011111) << 3);
    bytes[i + 15] = ((curTime->tm_min & 0b00111000) >> 3) | bytes[i + 15];
    bytes[i + 14] = ((curTime->tm_min & 0b00000111) << 5);
    return 0;
}

int createNewFile(char *bytes, char *file, char *fileName, int fileSize) {
    printf("Checking for duplicates\n");
    fflush(stdout);
    if(checkForDuplicate(bytes, fileName) != 0) {
        printf("ERROR: Duplicate file\n");
        exit(1);
    }

    int i = 0;
    for (i = 0; i < strlen(fileName); i++) fileName[i] = toupper(fileName[i]);

    printf("Creating root entry\n");
    fflush(stdout);
    int copiedBytes = 0;
    int entry = freeFatEntry(bytes);
    if (createRootFile(bytes, fileName, fileSize, entry) != 0) {
        printf("ERROR: Failed to create a root entry\n");
        exit(1);
    }

    printf("Copying file\n");
    fflush(stdout);
    while (copiedBytes < fileSize) {
        int cluster = (31 + entry) * bytesPerSector;
        int k = 0;

        printf("copied bytes %d\n", copiedBytes);
        fflush(stdout);
        printf("fileSize %d\n", fileSize);
        fflush(stdout);
        for (k = 0; k < bytesPerSector; k++) {
            if(copiedBytes == fileSize) {
                printf("all done\n");
                fflush(stdout);
                if (entry % 2 == 0) {
                    bytes[bytesPerSector + (3 * entry / 2)] = 0xFFF & 0xFF;
                    bytes[bytesPerSector + 1 + (3 * entry / 2)] = (0xFFF >> 8) & 0x0F;
                } else {
                    bytes[bytesPerSector + (int)(3 * entry / 2)] = (0xFFF << 4) & 0xF0;
                    bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (0xFFF >> 4) & 0xFF;
                }  
                return 0;
            }
            bytes[cluster + k] = file[copiedBytes];
            ++copiedBytes;
        }

        printf("finishing current entry %d\n", entry);
        fflush(stdout);
        if (entry % 2 == 0) {
            bytes[bytesPerSector + (3 * entry / 2)] = 0x69 & 0xFF;
            bytes[bytesPerSector + 1 + (3 * entry / 2)] = (0x69 >> 8) & 0x0F;
        } else {
            bytes[bytesPerSector + (int)(3 * entry / 2)] = (0x69 << 4) & 0xF0;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (0x69 >> 4) & 0xFF;
        } 

        printf("find new fat entry\n");
        fflush(stdout);
        int newEntry = freeFatEntry(bytes);

        printf("setting new FAT entry %d\n", entry);
        fflush(stdout);
        if (entry % 2 == 0) {
            bytes[bytesPerSector + (3 * entry / 2)] = newEntry & 0xFF;
            bytes[bytesPerSector + 1 + (3 * entry / 2)] = (newEntry >> 8) & 0x0F;
        } else {
            bytes[bytesPerSector + (int)(3 * entry / 2)] = (newEntry << 4) & 0xF0;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (newEntry >> 4) & 0xFF;
        } 
        entry = newEntry;
    }
    return 0;
    
}

int main(int argc, char *argv[]) {
    if (argc != 3) {                        //Only run if 2 additional argument is passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./diskput [file system] [file name]\n");
        exit(1);
    }

    char *diskImage = argv[1];              //Store disk image name
    char *fileName = argv[2];               //Store file name

    int disk = open(diskImage, O_RDWR);   //Open disk
    if (disk == -1) {
        printf("ERROR: Failed to read %s\n", diskImage);
        exit(1);
    }

    int diskFile = open(fileName, O_RDONLY);   //Open file
    if (diskFile == -1) {
        printf("File not found.\n");
        exit(1);
    }
    
    struct stat disk_stat;                          //Store various information about the disc
    fstat(disk, &disk_stat);                        //in a struct
    char *bytes = mmap(NULL, disk_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk, 0);
    if(bytes == "-1") {                             //If mapping fails, exit.
        printf("ERROR: Failed to retrieve data from %s\n", diskImage);
        exit(1);
    }

    struct stat file_stat;                          //Store various information about the disc
    fstat(diskFile, &file_stat);                    //in a struct
    int fileSize = file_stat.st_size;
    char *file = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, diskFile, 0);
    if(bytes == "-1") {                             //If mapping fails, exit.
        printf("ERROR: Failed to retrieve data from %s\n", file);
        exit(1);
    }

    int lowBits = bytes[11];                                //First 8 bits
    int highBits = bytes[12];                               //Second 8 bits
    bytesPerSector = ((highBits << 8) | lowBits);           //Concatanate the high and low bits

    lowBits = bytes[19];                            //First 8 bits
    highBits = bytes[20];                           //Second 8 bits
    totalSectorCount = ((highBits << 8) | lowBits); //Concatanate the high and low bits

    if(createNewFile(bytes, file, fileName, fileSize) != 0) {
        printf("ERROR: Failed to create new file\n");
        exit(1);
    }

    return 0;
}