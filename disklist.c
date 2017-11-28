#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

unsigned int BUFFER_SIZE = 1024;        //Global Variables

int main(int argc, char *argv[]) {
    if (argc != 2) {                    //Only run if 1 additional argument is passed in
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
                                                        //Calculate the bytes per sector
    int lowBits = bytes[11];                                //First 8 bits
    int highBits = bytes[12];                               //Second 8 bits
    int bytesPerSector = ((highBits << 8) | lowBits);       //Concatanate the high and low bits

    int rootDir = bytesPerSector * 19;      //Starting address to root directory
    int dataDir = bytesPerSector * 33;      //Starting address to the data directory

    int i = 0;
    for(i = rootDir; i < dataDir; i += 32) {        //For every entry
        if(bytes[i] == 0x00) break;                 //Stop if all remaining entries are free
        if(bytes[i] == 0xE5) continue;              //Skip entry if entry is free
        if(bytes[i + 11] == 0x08) continue;         //Skip entry if entry is a volume label
        if(bytes[i + 11] == 0x0F) continue;         //Skip entry if the entry is a continuation of previous entry

        char type = 'F';                                        //Allocate storage for the file type. Default is File
        int fileSize = 0;                                       //File size memory
        char *fileName = malloc(sizeof(BUFFER_SIZE));           //File name memory
        char *fileExt = malloc(sizeof(BUFFER_SIZE));            //File extension memory
        int month = 0, day = 0, year = 0, hour = 0, minute = 0; //Date memory
        int aBits = 0, bBits = 0, cBits = 0, dBits = 0;         //Variables used to determine file size

        if(bytes[i + 11] == 0x10) type = 'D';                   //Change file type to directory if directory attribute is set

        dBits = bytes[i + 28] & 0x00FF;             //Grab 4th byte
        cBits = bytes[i + 29] & 0x00FF;             //Grab 3rd byte
        bBits = bytes[i + 30] & 0x00FF;             //Grab 2nd byte
        aBits = bytes[i + 31] & 0x00FF;             //Grab 1st byte
        fileSize = (aBits << 24) | (bBits << 16) | (cBits << 8) | dBits;    //Combine all bits from above to compute file size

        int j = 0;              
        for(j = 0; j < 8; j++) {                    //Grab the file name from offset 0 - 7
            if (bytes[i + j] == ' ') break;         //Stop if there are no more characters to copy
            fileName[j] = bytes[i + j];
        }
        for(j = 0; j < 3; j++) {                    //Grab the file extension from offset 8 - 11 
            if (bytes[i + j + 8] == ' ') break;     //Stop if there are no more characters to copy
            if (j == 0) strcat(fileName, ".");      //Only append a '.' if there is a file extension
            fileExt[j] = bytes[i + j + 8];
        }
        strcat(fileName, fileExt);                  //Combine the file name and file extension
        
        year = ((bytes[i + 17] & 0b11111110) >> 1) + 1980;                                      //Grab the year + 1980 since date was recorded since than. 7 bits long
        month = ((bytes[i + 17] & 0b00000001) << 3) | ((bytes[i + 16] & 0b11100000) >> 5);      //Grab the month 4 bits long
        day = (bytes[i + 16] & 0b00011111);                                                     //Grab the day 5 bits long

        hour = ((bytes[i + 15] & 0b11111000) >> 3);                                             //Grab the hour 5 bits long
        minute = ((bytes[i + 15] & 0b00000111) << 3) | ((bytes[i + 14] & 0b11100000) >> 5);     //Grab the minutes 6 bits long
        
        printf("%c %10d %20s %02d/%02d/%d %02d:%02d\n", type, fileSize, fileName, month, day, year, hour, minute);  //Print statement
    }       

    return 0;
}