#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

unsigned int BUFFER_SIZE = 1024;                        //Global Variables
unsigned int bytesPerSector = 0;
unsigned int totalSectorCount = 0;

char *OS_Name(char *bytes) {
    char *OSName = malloc(sizeof(BUFFER_SIZE));         //Allocate memory for OS Name
    int i = 0;
    for (i = 0; i < 8; ++i) OSName[i] = bytes[i + 3];   //Store OS Name
    return OSName;
}

char *volume_label(char *bytes) {
    char *volLabel = malloc(sizeof(BUFFER_SIZE));       //Allocate memory for disk label
    int i = 0;
    int rootDir = bytesPerSector * 19;                  //Location to the root directory
    int dataDir = bytesPerSector * 33;                  //Location to the data directory
    int offset = 0;                                     //Offset to check next entry
                                                        //For every entry in root directory
    for(offset = 0; (rootDir + offset) < dataDir; offset += 32) {
        if (bytes[rootDir + offset] == 0x00) break;     //Ensures remaining directories are not free
        if (bytes[rootDir + offset + 11] == 0x08) {     //Check attribute for volume label
                                                        //Reading the volume label
            for (i = 0; i < 11; ++i) volLabel[i] = bytes[rootDir + offset + i];
            return volLabel;
        }
    }
    return "";
}

int total_disk_space(char *bytes) {
    int totalDiskSpace = 0;                             

    totalDiskSpace = totalSectorCount * bytesPerSector; //Compute total disk space using bytes per sector and sectors

    return totalDiskSpace;
}

int free_disk_space(char *bytes) {
    int freeDiskSpace;
    int entry;

    int i = 0;
    for (i = 2; i <= totalSectorCount - 32; i++) {      //For each sector
        if (i % 2 == 0) {                               //If even
            int lowBits = bytes[bytesPerSector + (3 * i / 2)] & 0xFF;               //Use the 8 bits
            int highBits = bytes[bytesPerSector + 1 + (3 * i / 2)] & 0x0F;          //Use the 4 bits
            entry = ((highBits << 8) | lowBits);
        } else {                                        //If odd
            int lowBits = bytes[bytesPerSector + (3 * i / 2)] & 0xF0;          //Use the 4 bits
            int highBits = bytes[bytesPerSector + (1 + (3 * i / 2))] & 0xFF;   //Use the 8 bits
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0) ++freeDiskSpace;                 //If the entry is empty
    }

    freeDiskSpace = freeDiskSpace * bytesPerSector;

    return freeDiskSpace;
}

int num_files_root_dir(char *bytes) {
    int numFilesRootDir = 0;                //Counter
    int rootDir = bytesPerSector * 19;      //Root directory offset                    
    int dataDir = bytesPerSector * 33;      //Data directory offset
    int offset = 0;                         //Offset
                                            //For each entry
    for(offset = 0; (rootDir + offset) < dataDir; offset += 32) {
        if (bytes[rootDir + offset] == 0x00) break;         //Check if all directories are free
        if (bytes[rootDir + offset] == 0xE5) continue;      //Check current directory if free
        if (bytes[rootDir + offset + 11] == 0x0F) continue; //Check attribute for long file name entry
        if (bytes[rootDir + offset + 11] == 0x08) continue; //Check attribute for volume label
        if (bytes[rootDir + offset + 11] == 0x10) continue; //Check attribute for sub directory
        ++numFilesRootDir;                                  //If none of the above, increment counter
    }
    return numFilesRootDir;
}

int num_FAT_cop(char *bytes) {
    int numFatCop = bytes[16];                      //Get the number of FATs from boot sector
    return numFatCop;
}

int sect_per_FAT(char *bytes) {
    int lowBits = bytes[22];                        //First 8 bits
    int highBits = bytes[23];                       //Second 8 bits
    int sectPerFAT = ((highBits << 8) | lowBits);   //Concatanate the high and low bits
    return sectPerFAT;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {                        //Only run if 1 additional argument is passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./diskinfo [file system]\n");
        exit(1);
    }

    char *diskImage = argv[1];              //Store disk image name
    int disk = open(diskImage, O_RDONLY);   //Open disk
    if (disk == -1) {
        printf("ERROR: Failed to read %s\n", diskImage);
        exit(1);
    }
    
    struct stat disk_stat;                          //Store various information about the disc
    fstat(disk, &disk_stat);                        //in a struct
    char *bytes = mmap(NULL, disk_stat.st_size, PROT_READ, MAP_SHARED, disk, 0);
    if(bytes == "-1") {                             //If mapping fails, exit.
        printf("ERROR: Failed to retrieve data from %s\n", diskImage);
        exit(1);
    }
                                                    //Calculate bytesPerSector
    int lowBits = bytes[11];                            //First 8 bits
    int highBits = bytes[12];                           //Second 8 bits
    bytesPerSector = ((highBits << 8) | lowBits);       //Concatanate the high and low bits
                                                    //Calculate the total number of sectors
    lowBits = bytes[19];                                //First 8 bits
    highBits = bytes[20];                               //Second 8 bits
    totalSectorCount = ((highBits << 8) | lowBits);     //Concatanate the high and low bits

    char *OSName = malloc(sizeof(BUFFER_SIZE));         //Allocating memory for variables
    char *volLabel = malloc(sizeof(BUFFER_SIZE));   
    int totalDiskSpace = 0;
    int freeDiskSpace = 0;
    int numFilesRootDir = 0;
    int numFATCop = 0;
    int sectPerFAT = 0;

    OSName = OS_Name(bytes);                        //Get os name
    volLabel = volume_label(bytes);                 //Get volume label
    totalDiskSpace = total_disk_space(bytes);       //Get total disk space
    freeDiskSpace = free_disk_space(bytes);         //Get free disk space
    numFilesRootDir = num_files_root_dir(bytes);    //Count number of files in root directory
    numFATCop = num_FAT_cop(bytes);                 //Get the number of FAT Copies
    sectPerFAT = sect_per_FAT(bytes);               //Get number of sectors per FAT

    printf("OS Name: %s\n", OSName);                //Print statements
    printf("Label of the disk: %s\n", volLabel);
    printf("Total size of the disk: %d\n", totalDiskSpace);
    printf("Free size of the disk: %d\n", freeDiskSpace);
    printf("\n");
    printf("==============\n");
    printf("The number of files in the root directory (not including subdirectories): ");
    printf("%d\n", numFilesRootDir);
    printf("\n");
    printf("=============\n");
    printf("Number of FAT copies: %d\n", numFATCop);
    printf("Sectors per FAT: %d\n", sectPerFAT);

    return 0;
}