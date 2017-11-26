#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

unsigned int BUFFER_SIZE = 1024;

int listRootDir(char *bytes) {
    int rootDir = 512 * 19;
    int dataDir = 512 * 33;

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {
        if(bytes[i] == 0x00) break;
        if(bytes[i] == 0xE5) continue;
        if(bytes[i + 11] == 0x08) continue;
        if(bytes[i + 11] == 0x0F) continue;

        char type = 'F';
        int fileSize = 0;
        char *fileName = malloc(sizeof(BUFFER_SIZE));
        char *fileExt = malloc(sizeof(BUFFER_SIZE));
        int month = 0, day = 0, year = 0, hour = 0, minute = 0;
        int aBits = 0, bBits = 0, cBits = 0, dBits = 0;

        if(bytes[i + 11] == 0x10) type = 'D';

        dBits = bytes[i + 28] & 0x00FF;
        cBits = bytes[i + 29] & 0x00FF;
        bBits = bytes[i + 30] & 0x00FF;
        aBits = bytes[i + 31] & 0x00FF;
        fileSize = (aBits << 24) | (bBits << 16) | (cBits << 8) | dBits;

        int j = 0;
        for(j = 0; j < 8; j++) {
            if (bytes[i + j] == ' ') break;
            fileName[j] = bytes[i + j];
        }
        for(j = 0; j < 3; j++) {
            if (bytes[i + j + 8] == ' ') break;
            if (j == 0) strcat(fileName, ".");
            fileExt[j] = bytes[i + j + 8];
        }
        strcat(fileName, fileExt);
        
        year = ((bytes[i + 17] & 0b11111110) >> 1) + 1980;
        month = ((bytes[i + 17] & 0b00000001) << 3) | ((bytes[i + 16] & 0b11100000) >> 5);
        day = (bytes[i + 16] & 0b00011111);

        hour = ((bytes[i + 15] & 0b11111000) >> 3);
        minute = ((bytes[i + 15] & 0b00000111) << 3) + ((bytes[i + 14] & 0b11100000) >> 5);
        
        printf("%c %10d %20s %02d/%02d/%d %02d:%02d\n", type, fileSize, fileName, month, day, year, hour, minute);
    }   
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {                        //Only run if 1 additional argument is passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./disklist [file system]\n");
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

    if(listRootDir(bytes) != 0) return -1;

    return 0;
}