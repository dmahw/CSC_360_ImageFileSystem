#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

unsigned int BUFFER_SIZE = 1024;

unsigned int bytesPerSector = 512;

int copyFile(char *fileName, char *bytes) {
    int rootDir = bytesPerSector * 19;
    int dataDir = bytesPerSector * 33;

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {
        if(bytes[i] == 0x00) break;
        if(bytes[i] == 0xE5) continue;
        if(bytes[i + 11] == 0x08) continue;
        if(bytes[i + 11] == 0x0F) continue;
        if(bytes[i + 11] == 0x10) continue;

        char *rootFile = malloc(sizeof(BUFFER_SIZE));
        char *fileExt = malloc(sizeof(BUFFER_SIZE));

        int j = 0;
        for(j = 0; j < 8; j++) {
            if (bytes[i + j] == ' ') break;
            rootFile[j] = bytes[i + j];
            printf("%s\n", rootFile);
            fflush(stdout);
        }
        for(j = 0; j < 8; j++) {
            if (bytes[i + j + 8] == ' ') break;
            if (j == 0) strcat(rootFile, ".");
            fileExt[j] = bytes[i + j + 8];
            printf("%s\n", fileExt);
            fflush(stdout);
        }
        strcat(rootFile, fileExt);

        if(strcmp(fileName, rootFile) == 0) {
            printf("%s %s\n", fileName, rootFile);
            fflush(stdout);

            int dBits = bytes[i + 28] & 0x00FF;
            int cBits = bytes[i + 29] & 0x00FF;
            int bBits = bytes[i + 30] & 0x00FF;
            int aBits = bytes[i + 31] & 0x00FF;
            int fileSize = (aBits << 24) | (bBits << 16) | (cBits << 8) | dBits;
            
            printf("%d\n", fileSize);
            fflush(stdout);

            int copiedBytes = 0;
            int copiedDiskFile = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 00777);
            if (copiedDiskFile == -1) {
                printf("ERROR: Unable to open disk for new file\n");
                exit(1);
            }

            if (lseek(copiedDiskFile, fileSize-1, SEEK_SET) == -1) {
                printf("ERROR: Unable to seek EOF\n");
                exit(1);
            }

            if (write(copiedDiskFile, "", 1) != 1) {
                printf("ERROR: Unable to write to last byte\n");
                exit(1);
            }

            

            struct stat disk_stat;                          //Store various information about the disc
            fstat(copiedDiskFile, &disk_stat);
            char *copiedFile = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, copiedDiskFile, 0);
            if (copiedFile == MAP_FAILED) {
                printf("ERROR: Unable to allocate memory for new file\n");
                exit(1);
            }

            int firstLogicalCluster = (bytes[i + 27] << 8) | bytes[i + 26];
            int entry = firstLogicalCluster;

            do {
                if(copiedBytes == 0) {
                    entry = firstLogicalCluster;
                } else {
                    if (entry % 2 == 0) {                               //If even
                        int lowBits = bytes[bytesPerSector + (3 * entry / 2)];
                        int highBits = bytes[bytesPerSector + 1 + (3 * entry / 2)] & 0x0F;
                        entry = ((highBits << 8) | lowBits);
                    } else {                                        //If odd
                        int lowBits = bytes[bytesPerSector + (int)(3 * entry / 2)] & 0xF0;
                        int highBits = bytes[bytesPerSector + (int)(1 + (3 * entry / 2))];
                        entry = ((highBits << 4) | (lowBits >> 4));
                    }
                }
                
                int physicalCluster = bytesPerSector * (31 + entry);

                int k = 0;
                for (k = 0; k < bytesPerSector; k++) {
                    if(copiedBytes == fileSize) break;
                    copiedFile[copiedBytes] = bytes[physicalCluster + k];
                    ++copiedBytes;

                    if(fileSize - copiedBytes <= bytesPerSector) {
                        int l = 0;
                        for(l = 0; l < fileSize - copiedBytes; l++) {
                            copiedFile[copiedBytes + l] = bytes[physicalCluster + k + l];
                        }
                        copiedBytes += l;
                    }
                }
                printf("%d\n", copiedBytes);
                fflush(stdout);
            } while((entry != 0xFFF) && (copiedBytes != fileSize));
            munmap(copiedFile, disk_stat.st_size);
            close(copiedDiskFile);   
        } 
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {                        //Only run if 2 additional argument is passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./diskinfo [file system] [file name]\n");
        exit(1);
    }

    char *diskImage = argv[1];              //Store disk image name
    char *fileName = argv[2];               //Store file name
    
    int i = 0;
    for (i = 0; i < strlen(fileName); i++) fileName[i] = toupper(fileName[i]);

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

    int lowBits = bytes[11];                                //First 8 bits
    int highBits = bytes[12];                               //Second 8 bits
    bytesPerSector = ((highBits << 8) | lowBits);           //Concatanate the high and low bits

    copyFile(fileName, bytes);

    return 0;
}