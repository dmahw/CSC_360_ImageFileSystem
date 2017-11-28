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

unsigned int BUFFER_SIZE = 1024;                //Global variables
unsigned int bytesPerSector = 512;
unsigned int totalSectorCount = 0;

int total_disk_space(char *bytes) {             //Total disk size
    int totalDiskSpace = 0;                             

    totalDiskSpace = totalSectorCount * bytesPerSector;

    return totalDiskSpace;
}

int free_disk_space(char *bytes) {              //Free disk space
    int freeDiskSpace;
    int entry;

    int i = 0;
    for (i = 2; i <= totalSectorCount - 32; i++) {
        if (i % 2 == 0) {
            int lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xFF;
            int highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0x0F;
            entry = ((highBits << 8) | lowBits);
        } else {
            int lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xF0;
            int highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0xFF;
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0x000) ++freeDiskSpace;
    }

    freeDiskSpace = freeDiskSpace * bytesPerSector;

    return freeDiskSpace;
}

int checkForDuplicate(char *bytes, char *fileName) {        //Check for duplicates
    int rootDir = bytesPerSector * 19;                      //Pointer to root directory
    int dataDir = bytesPerSector * 33;                      //Pointer to data directory

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {                //For each root entry
        if(bytes[i] == 0x00) break;                         //Stop if all remaining entry is free
        if(bytes[i] == 0xE5) continue;                      //Skip if entry is free
        if(bytes[i + 11] == 0x08) continue;                 //Skip if entry is a volume label
        if(bytes[i + 11] == 0x0F) continue;                 //Skip if entry if a continuation of previous entry

        char *rootFile = malloc(sizeof(BUFFER_SIZE));
        char *rootFileExt = malloc(sizeof(BUFFER_SIZE));
        int j = 0;
        for(j = 0; j < 8; j++) {                            //Get file name
            if (bytes[i + j] == ' ') break; 
            rootFile[j] = bytes[i + j];
        }
        for(j = 0; j < 3; j++) {                            //Get extension name
            if (bytes[i + j + 8] == ' ') break;
            if (j == 0) strcat(rootFile, ".");
            rootFileExt[j] = bytes[i + j + 8];
        }
        strcat(rootFile, rootFileExt);                      //Create file name using extension and file name
        if(strcmp(fileName, rootFile) == 0) return 1;       //If the strings match, found duplicate
    }
    return 0;
}

int freeFatEntry(char *bytes) {                             //Find the next available FAT entry
    int entry;  
    int i = 0;
    for (i = 2; i <= totalSectorCount - 32; i++) {
        if (i % 2 == 0) {
            int lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xFF;
            int highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0x0F;
            entry = ((highBits << 8) | lowBits);
        } else {
            int lowBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xF0;
            int highBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))] & 0xFF;
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0x000) return i;                        //Found an empty entry
    }
    printf("ERROR: Unable to find free entry\n");
    exit(1);
}

int createRootFile(char *bytes, char *fileName, int fileSize, int entry) {      //In root create a file
    int rootDir = bytesPerSector * 19;
    int dataDir = bytesPerSector * 33;
    int i = 0;
    for (i = rootDir; i < dataDir; i += 32) {                   //Find available entry in root
        if (bytes[i] == 0x00) break;
        if (i + 32 >= dataDir) {
            printf("ERROR: Unable to find free root entry\n");
            exit(1);
        }
    }

    bytes[i + 26] = entry & 0x00FF;                 //Store the free entry in the first logical cluster
    bytes[i + 27] = ((entry) & 0xFF00) >> 8;
    
    int j = 0;
    for(j = 0; j < 20; j++) {                       //Store the file name with the extension
        if(fileName[j] == '.') {
            int k = 0;
            for(k = 0; k < 3; k++) bytes[i + k + 8] = fileName[j + k + 1];
            break;
        }
        if(j < 8) bytes[i + j] = fileName[j];
    }

    bytes[i + 28] = (fileSize & 0x000000FF) >> 0;        //Parse the file size in to 4 seperate bytes
    bytes[i + 29] = (fileSize & 0x0000FF00) >> 8;   //Shift all the bytes to the respective location
    bytes[i + 30] = (fileSize & 0x00FF0000) >> 16;  //Store each 8 bytes into the seperate bytes
    bytes[i + 31] = (fileSize & 0xFF000000) >> 24;

    bytes[i + 11] = 0x00;

    time_t currentTime = time(NULL);                //Get current time
    struct tm *curTime = localtime(&currentTime);   //Set the time to local time
    bytes[i + 17] = (((curTime->tm_year - 80) & 0b01111111) << 1);                  //Parse year into 7 bits and store
    bytes[i + 17] = (((curTime->tm_mon + 1) & 0b00001000) >> 3) | bytes[i + 17];    //Parse month into 4 bits and store in respective location
    bytes[i + 16] = (((curTime->tm_mon + 1) & 0b00000111) << 5);                    //Retain any information with other information stored.
    bytes[i + 16] = ((curTime->tm_mday & 0b00011111)) | bytes[i + 16];              //Parse day into 5 bits and store
    bytes[i + 15] = ((curTime->tm_hour & 0b00011111) << 3);                         //Parse hour into 5 bits and store
    bytes[i + 15] = ((curTime->tm_min & 0b00111000) >> 3) | bytes[i + 15];          //Parse minutes into 6 bits (respectively) and store
    bytes[i + 14] = ((curTime->tm_min & 0b00000111) << 5);
    return 0;
}

int createNewFile(char *bytes, char *file, char *fileName, int fileSize) {
    int i = 0;
    for (i = 0; i < strlen(fileName); i++) fileName[i] = toupper(fileName[i]);      //Convert file name to all upper

    if(checkForDuplicate(bytes, fileName) != 0) {                       //Check for duplicates
        printf("ERROR: Duplicate file\n");                              //ERROR
        exit(1);
    }

    int copiedBytes = 0;
    int entry = freeFatEntry(bytes);                                    //Find an available FAT entry
    if (createRootFile(bytes, fileName, fileSize, entry) != 0) {        //Create the root file
        printf("ERROR: Failed to create a root entry\n");
        exit(1);
    }

    while (copiedBytes < fileSize) {                                    //Until copiedBytes = fileSize
        if (entry % 2 == 0) {                                           //Store the current entry and parse it into the memory map
            bytes[bytesPerSector + (int)(3 * entry / 2)] = entry & 0xFF;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (entry >> 8) & 0x0F;
        } else {
            bytes[bytesPerSector + (int)(3 * entry / 2)] = (entry << 4) & 0xF0;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (entry >> 4) & 0xFF;
        } 
        int cluster = (31 + entry) * bytesPerSector;                    //Determing the physical cluster to copy from

        int k = 0;                          
        for (k = 0; k < bytesPerSector; k++) {              //For each byte into the sector
            if(copiedBytes == fileSize) {                   //If we have copied the number of required bytes
                if (entry % 2 == 0) {                       //Set the current/last cluster as the last cluster
                    bytes[bytesPerSector + (int)(3 * entry / 2)] = 0xFFF & 0xFF;
                    bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (0xFFF >> 8) & 0x0F;
                } else {
                    bytes[bytesPerSector + (int)(3 * entry / 2)] = (0xFFF << 4) & 0xF0;
                    bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (0xFFF >> 4) & 0xFF;
                }  
                return 0;
            }
            bytes[cluster + k] = file[copiedBytes];         //Copy the bytes
            ++copiedBytes;                                  //Log the number of bytes
        }

        int newEntry = freeFatEntry(bytes);                 //Get the next free FAT entry
        if (entry % 2 == 0) {                               //Map current entry location to the next entry location
            bytes[bytesPerSector + (int)(3 * entry / 2)] = newEntry & 0xFF;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (newEntry >> 8) & 0x0F;
        } else {
            bytes[bytesPerSector + (int)(3 * entry / 2)] = (newEntry << 4) & 0xF0;
            bytes[bytesPerSector + (int)(1 + (3 * entry / 2))] = (newEntry >> 4) & 0xFF;
        } 
        entry = newEntry;                                   //Use the new free entry
    }
    return 0;
    
}

int main(int argc, char *argv[]) {
    if (argc != 3) {                        //Stop if not 2 additional arguments are passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./diskput [file system] [file name]\n");
        exit(1);
    }

    char *diskImage = argv[1];              //Declare naming
    char *fileName = argv[2];

    int disk = open(diskImage, O_RDWR);     //Open disk for read and write
    if (disk == -1) {                       //ERROR
        printf("ERROR: Failed to read %s\n", diskImage);
        exit(1);
    }

    int diskFile = open(fileName, O_RDONLY);    //Open the file to read from with read only
    if (diskFile == -1) {                       //ERROR
        printf("File not found.\n");
        exit(1);
    }
    
    struct stat disk_stat;                      //Retrieve disk information
    fstat(disk, &disk_stat);
    char *bytes = mmap(NULL, disk_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk, 0);
    if(bytes == "-1") {
        printf("ERROR: Failed to retrieve data from %s\n", diskImage);
        exit(1);
    }

    struct stat file_stat;                      //Retrieve file information
    fstat(diskFile, &file_stat);
    char *file = mmap(NULL, file_stat.st_size, PROT_READ, MAP_SHARED, diskFile, 0);
    if(bytes == "-1") {
        printf("ERROR: Failed to retrieve data from %s\n", file);
        exit(1);
    }
    int fileSize = file_stat.st_size;           //Set the fileSize to the file size of the copying file


    int lowBits = bytes[11];                        //Compute bytes per sector
    int highBits = bytes[12];
    bytesPerSector = ((highBits << 8) | lowBits);

    lowBits = bytes[19];                            //Grab the total sector count
    highBits = bytes[20];
    totalSectorCount = ((highBits << 8) | lowBits); 

    if(free_disk_space(bytes) < fileSize) {         //If there is not enough free space, stop
        printf("No enough free space in the disk image\n");
        exit(1);
    }

    if(createNewFile(bytes, file, fileName, fileSize) != 0) {   //Create a new file
        printf("ERROR: Failed to create new file\n");           //ERROR
        exit(1);
    } else {
        printf("SUCCESS: Created new file %s\n", fileName);     //SUCCESS
    }

    return 0;
}