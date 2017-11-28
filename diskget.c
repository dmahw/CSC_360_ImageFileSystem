#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

unsigned int BUFFER_SIZE = 1024;        //Global variables
unsigned int bytesPerSector = 512;

int copyFile(char *fileName, char *bytes) {
    int rootDir = bytesPerSector * 19;          //Address to root directory
    int dataDir = bytesPerSector * 33;          //Address to data directory

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {    //For each entry in root directory
        if(bytes[i] == 0x00) break;             //Stop if all remaining entries are free
        if(bytes[i] == 0xE5) continue;          //Skip entry if entry is empty
        if(bytes[i + 11] == 0x08) continue;     //Skip if the entry is a volume label
        if(bytes[i + 11] == 0x0F) continue;     //Skip if the entry is a continuation of the previous entry
        if(bytes[i + 11] == 0x10) continue;     //Skip if the entry is a directory

        char *rootFile = malloc(sizeof(BUFFER_SIZE));       //Allocate memory for getting the root file name
        char *rootFileExt = malloc(sizeof(BUFFER_SIZE));    //Allocate memory for getting the root file extension

        int j = 0;
        for(j = 0; j < 8; j++) {                //Grab the filename from the root directory
            if (bytes[i + j] == ' ') break;     //Stop if remaining characters are empty
            rootFile[j] = bytes[i + j];     
        }
        for(j = 0; j < 3; j++) {                //Grab the file extension from the root directory
            if (bytes[i + j + 8] == ' ') break; //Stop if remaining characters are empty
            if (j == 0) strcat(rootFile, ".");  //Only append a '.' if there exists an extension
            rootFileExt[j] = bytes[i + j + 8];
        }
        strcat(rootFile, rootFileExt);          //Create the file name by combining the rootFile name and the extension

        if(strcmp(fileName, rootFile) == 0) {       //Check to see if the file is the file we are looking for
            int dBits = bytes[i + 28] & 0x00FF;                                     //Compute the size of the root file
            int cBits = bytes[i + 29] & 0x00FF;                                     //
            int bBits = bytes[i + 30] & 0x00FF;                                     //
            int aBits = bytes[i + 31] & 0x00FF;                                     //
            int fileSize = (aBits << 24) | (bBits << 16) | (cBits << 8) | dBits;    //

            int copiedBytes = 0;                                                    //If all remaining subroutines past
            int copiedDiskFile = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 00777); //Open/create/replace a new file to copy rootfile into. 
            if (copiedDiskFile == -1) {                                             //ERROR
                printf("ERROR: Unable to write or create to new file\n");
                exit(1);
            }

            if (lseek(copiedDiskFile, fileSize-1, SEEK_SET) == -1) {                //Allow writing of the file
                printf("ERROR: Unable to seek EOF\n");                              //ERROR
                exit(1);
            }

            if (write(copiedDiskFile, "\0", 1) != 1) {                              //Allow writing of the file
                printf("ERROR: Unable to write to last byte\n");                    //ERROR
                exit(1);
            }

            struct stat disk_stat;                          //Store various information about the file
            fstat(copiedDiskFile, &disk_stat);              //
            char *copiedFile = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, copiedDiskFile, 0);     //Map all the bytes to allow for writing
            if (copiedFile == MAP_FAILED) {                                     //ERROR
                printf("ERROR: Unable to allocate memory for new file\n");
                exit(1);
            }

            int cluster = ((bytes[i + 27] << 8) & 0xFF) | (bytes[i + 26] & 0xFF);       //Get the first logical cluster from root
            int entry = cluster;                                        //Use the first logical cluster

            while((entry != 0xFFF) && (copiedBytes != fileSize)) {      //While the entry is not the EOF,
                int physicalCluster = bytesPerSector * (31 + entry);    //Use cluster to determine the physical cluster address in data directory

                int k = 0;
                for (k = 0; k < bytesPerSector; k++) {                      //Copy all the bytes in the physical cluster
                    if(copiedBytes == fileSize) break;                      //Stop copying once copied all bytes
                    copiedFile[copiedBytes] = bytes[physicalCluster + k];   
                    ++copiedBytes;

                    if(fileSize - copiedBytes <= bytesPerSector) {          //If the remaining cluster is less than the bytes per sectory
                        int l = 0;
                        for(l = 0; l < fileSize - copiedBytes; l++) {                       //Copy the remaining bytes and stop.
                            copiedFile[copiedBytes + l] = bytes[physicalCluster + k + l];
                        }
                        copiedBytes += l;
                    }
                }
                                                                //Grab a new entry
                if (entry % 2 == 0) {                           //If even
                    int lowBits = bytes[bytesPerSector + (3 * entry / 2)] & 0xFF;       
                    int highBits = bytes[bytesPerSector + 1 + (3 * entry / 2)] & 0x0F;
                    entry = ((highBits << 8) | lowBits);
                } else {                                        //If odd
                    int lowBits = bytes[bytesPerSector + (3 * entry / 2)] & 0xF0;
                    int highBits = bytes[bytesPerSector + (1 + (3 * entry / 2))] & 0xFF;
                    entry = ((highBits << 4) | (lowBits >> 4));
                }
            }
            munmap(copiedFile, disk_stat.st_size);              //Close the memory map
            close(copiedDiskFile);                              //Close the copied disk file
            return 0;
        } 
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {                        //Only run if 2 additional argument is passed in
        printf("ERROR: Incorrect Usage.\nUSAGE:\n\t./diskget [file system] [file name]\n");
        exit(1);
    }

    char *diskImage = argv[1];              //Store disk image name
    char *fileName = argv[2];               //Store file name
    
    int i = 0;                              //Convert passed in file name to all caps since root dir stores names in all caps
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
                                                    //Compute the number of bytes per sector
    int lowBits = bytes[11];                                //First 8 bits
    int highBits = bytes[12];                               //Second 8 bits
    bytesPerSector = ((highBits << 8) | lowBits);           //Concatanate the high and low bits

    if (copyFile(fileName, bytes) != 0) {
        printf("File not found.\n");
        exit(1);
    }

    return 0;
}