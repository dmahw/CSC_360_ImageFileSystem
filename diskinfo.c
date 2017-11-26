#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

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

    char *OSName = malloc(sizeof(BUFFER_SIZE));     //Allocating memory for variables
    char *volLabel = malloc(sizeof(BUFFER_SIZE));   
    int totalDiskSpace = 0;
    int freeDiskSpace = 0;
    int numFilesRootDir = 0;
    int numFATCop = 0;
    int sectPerFAT = 0;
    
    struct stat disk_stat;                          //Store various information about the disc
    fstat(disk, &disk_stat);                        //in a struct
    char *bytes = mmap(NULL, disk_stat.st_size, PROT_READ, MAP_SHARED, disk, 0);
    if(bytes == "-1") {                             //If mapping fails, exit.
        printf("ERROR: Failed to retrieve data from %s\n", diskImage);
        exit(1);
    }

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