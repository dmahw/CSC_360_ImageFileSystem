#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

char *OS_Name(char *bytes) {
    char *OSName = malloc(sizeof(BUFFER_SIZE));         //Allocate memory for OS Name
    int i = 0;
    for (i = 0; i < 8; ++i) OSName[i] = bytes[i + 3];   //Store OS Name
    return OSName;
}

char *volume_label(char *bytes) {
    char *volLabel = malloc(sizeof(BUFFER_SIZE));       //Allocate memory for disk label
    int i = 0;
    int rootDir = 512 * 19;                             //Location to the root directory
    int dataDir = 512 * 33;                             //Location to the data directory
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

    int lowBits = bytes[19];                            //First 8 bits
    int highBits = bytes[20];                           //Second 8 bits
    int totalSectorCount = ((highBits << 8) | lowBits); //Concatanate the high and low bits

    lowBits = bytes[11];                                //First 8 bits
    highBits = bytes[12];                               //Second 8 bits
    int bytesPerSector = ((highBits << 8) | lowBits);   //Concatanate the high and low bits

    totalDiskSpace = totalSectorCount * bytesPerSector; //Compute total disk space using bytes per sector and sectors

    return totalDiskSpace;
}

int free_disk_space(char *bytes) {
    int freeDiskSpace;
    int highBits;
    int lowBits;
    int entry;

    lowBits = bytes[11];                                //First 8 bits
    highBits = bytes[12];                               //Second 8 bits
    int bytesPerSector = ((highBits << 8) | lowBits);   //Concatanate the high and low bits

    lowBits = bytes[19];                                //First 8 bits
    highBits = bytes[20];                               //Second 8 bits
    int totalSectorCount = ((highBits << 8) | lowBits); //Concatanate the high and low bits

    int i = 0;
    for (i = 2; i < totalSectorCount; i++) {
        if (i % 2 == 0) {
            lowBits = bytes[bytesPerSector + (3 * i / 2)];
            highBits = bytes[bytesPerSector + 1 + (3 * i / 2)] & 0x0F;
            entry = ((highBits << 8) | lowBits);
        } else {
            lowBits = bytes[bytesPerSector + (int)(1 + (3 * i / 2))];
            highBits = bytes[bytesPerSector + (int)(3 * i / 2)] & 0xF0;
            entry = ((highBits << 4) | (lowBits >> 4));
        }
        if(entry == 0x000) ++freeDiskSpace;
    }

    freeDiskSpace = freeDiskSpace * bytesPerSector;

    return freeDiskSpace;
}

int num_files_root_dir(char *bytes) {
    int numFilesRootDir = 0;        //Counter
    int rootDir = 512 * 19;         //Root directory offset                    
    int dataDir = 512 * 33;         //Data directory offset
    int offset = 0;                 //Offset
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
    int numFatCop = bytes[16];  //Get the number of FATs from boot sector
    return numFatCop;
}

int sect_per_FAT(char *bytes) {
    int lowBits = bytes[22];                        //First 8 bits
    int highBits = bytes[23];                       //Second 8 bits
    int sectPerFAT = ((highBits << 8) | lowBits);   //Concatanate the high and low bits
    return sectPerFAT;
}