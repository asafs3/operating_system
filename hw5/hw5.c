/*
HW5 - Reading the FAT12 file system in Linux
Asaf Schwartz 208436048
*/
#include <stdio.h>
#include <math.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define ARG_SIZE 512


typedef struct {
    unsigned char* fullName;    // more convinient to cp function
    unsigned char* fileName;    // more convinient to dir function
    unsigned char* extension;   // more convinient to cp function
    int attribute;
    int fileSize;
    int creationTime;
    int fineTime;
    int creationDate;
    int startingCluster;        // needed for cp function, cluser is a unit of allocation comprising a set of logically contiguous sectors
} entry;



// ==================================================
//                    common functions
// ==================================================


// The function returns a buffer contain the data found at the offset location written as a string of unsigned char
 unsigned char* getCharData(FILE *fp, int offset, int bytesNum) {
    // reading by bytes, unsigned char fits this case, 8 bits without a sign bit

    unsigned char* buff = malloc(sizeof(unsigned char)*bytesNum);
    
    // set the seek pointer to the right location
    fseek(fp, offset, SEEK_SET);

    // read the desired number of bytes from the binary file
    fread(buff, 1, bytesNum, fp);

    // set back the file position to the beginning of the given file
    rewind(fp);

    return buff;
}

// The function gets the data in the given location as an int.
// The data is extracted as unsigned char, the function takes that data and convert it the its integer value
int getIntData(FILE *fp, int offset, int bytesNum) {
    // get the data in bits into a buffer 
    unsigned char* buff = getCharData(fp, offset, bytesNum); // get the data in the given location
    int i, value = 0;
    // the given buffer holds bytes as a string, for interpertation it is needed to weight each byte in the string
    // according to its position, in the first byte, bit 0 is with weigh 2^0, bit 1 with 2^1 and so on
    // in the second byte, bit 0 needs to be with a weight of 2^7, bit 1 with 2^8 and so on, it could be achieved
    // by 8 left logical shifting of that byte. the third byte needs to be shifted by 16 and so on.   
    for (i = 0; i < bytesNum; i++) {

        // (unsigned char)buff[i] : By casting it to (unsigned char), we ensure that it is treated as an unsigned value.
        // (unsigned int)(unsigned char)buff[i] : cast the unsigned character to an unsigned integer. (expand from 8 to 32 bits)
        // shifting the current byte by the corresponding index, to give the correct weight to this byte.
        value += (unsigned int)(unsigned char)buff[i] << 8*i;
    }
    free(buff);
    return value;
}

// The function extract a bit's value in the given index inside a byte (8 bits)
int getBit(unsigned char byte, int index) {
    unsigned char val = byte & (1 << index);
    if (val == 0) {
        return 0;
    }
    else {
        return 1;
    }
     
}

// ==================================================
//                    dir functions
// ==================================================


// The function prints the file's name according to the name format in the FAT.pdf
void namePrinting(unsigned char* fileName, unsigned char* extension){
        int first = 0;
        int space_till_end = 0;
        int i = 0;
        while (i < 8) {
            if (fileName[i] == 32){
                if (first == 0) {
                    space_till_end = 1;
                    first = i;
                }
                
            }
            else {
                if ( (i> first) && (first != 0) ) {
                    space_till_end = 0;
                    break;
                }
            }

            i ++;
        }
        if (space_till_end) {
            fileName[first] = '\0';
        }

        printf("%s", fileName);
        if (extension[0] != 32) {
            printf(".");
            printf("%s", extension);
        }


}

// The function prints the file's creation date according to the date format in the FAT.pdf
void datePrinting(int date) {
    //0000 0000 0001 1111
    int day = date & 0x1f;
    //0000 0001 1110 0000
    int month = (date >> 5) & 0xf;
    //1111 1110 0000 0000
    int year = 1980+((date >> 9) & 0x7f);
    printf("%d/%d/%d\t", month, day, year);
    return;
}

// The function prints the date according to the time format in the FAT.pdf
void timePrinting(int fineTime, int creationTime) {
    //0000 0111 1110 0000
    int min = (creationTime >> 5) & 0x3f;
    //1111 1000 0000 0000
    int hour = (creationTime >> 11) & 0x1f;
    
    char am_pm[3];
    strcpy(am_pm, "AM");
    hour = 13;
    if ( (hour >= 12) ) {
        hour = hour % 12;
        strcpy(am_pm, "PM");
    }
    if (hour == 0) {
        hour = 12;
    }
    printf("%02d:%02d %s\t", hour, min, am_pm);
    return;
}


// The function iterates through all files in root directory and prints their information
void printRootEntries(FILE *fp, int startOfRoot) {
    
    // extracting the root directory number of entries from PBP 
    int rootEntNum = getIntData(fp, 0x11, 2);
    
    // iterate the root entries
    int i;
    for (i = 0; i < rootEntNum; i++) {
        // each entry is 32 bytes
        int index = startOfRoot+i*32;
        int check = getIntData(fp, index, 1);
        if (check == 0) break;
        else if (check == 229) continue;
        entry newEntry = {
            0,
            getCharData(fp, index, 8  ), // file name - first 8 bytes, the name is of type unsigned char
            getCharData(fp, index+8, 3), // file extension - next 3 bytes, the name is of type unsigned char
            getIntData(fp, index+11, 1), // attributes bits - the 12th byte, this parameter is of type int
            getIntData(fp, index+28, 4), // file size - 4 bytes, this parameter is of type int
            getIntData(fp, index+14, 2), // creation time - 2 bytes, this parameter is of type int
            getIntData(fp, index+13, 1), // creation time - byte, this parameter is of type int 
            getIntData(fp, index+16, 2), // creation date - 2 bytes, this parameter is of type int
            getIntData(fp, index+26, 2),

        };
        // if (newEntry.attribute == 0 && newEntry.startingCluster <= 0) continue; //If not a file/directory
        // check that the element is not a volume label
        if (getBit(newEntry.attribute, 3)) {
            continue;
        } 
        // if (newEntry.attribute == 0) continue; //If not a file/directory
        datePrinting(newEntry.creationDate);
        timePrinting(newEntry.fineTime, newEntry.creationTime);
        // printf("%d\t", newEntry.creationDate);
        // printf("%d\t", newEntry.creationTime);
        // printf("%d\t", newEntry.fineTime);
        // if the 4th bit in the attribute filed is 1 the entry is a dictionary
        if (getBit(newEntry.attribute, 4)) {
            printf("<DIR>\t\t");
        }
        else {
            printf("%d\t\t", newEntry.fileSize);

        }
        namePrinting(newEntry.fileName, newEntry.extension);
    
        printf("\n");
    }
}



// ==================================================
//                    cp functions
// ==================================================

// The function converts the input filename to the format stored in FAT12
// the format is unsigned char, in uppercase.
// consists of 11 bytes, 8 is the file name, if it is shorter - a space is inserted (ASCII 32)
// and 3 last bytes is the extension (if no extension so it is filled with spaces )
unsigned char* convertFileName(char* fileName) {
    unsigned char* converted = (unsigned char*)malloc(sizeof(unsigned char)*11);
    int i, j = 0;
    for(i = 0; i < strlen(fileName); i++) {
        // if detected '.', so the rest of the name should be filled with spaces (untill the 8th byte)
        if (fileName[i] == '.') {
            for (j = i; j < 8; j++) {
                converted[j] = 32;
            }
            continue;
        }
        // before reaching '.', uppercase the character and set it in the converted name
        // after reaching '.' and filling the rest with spaces until the 8th byte, continue filling the extension
        converted[j++] = toupper(fileName[i]);
    }
    // if the name length is shorter than the 11 bytes given, fill the rest with spaces
    while (j < 11) converted[j++] = 32;

    return converted;
}

// The function searches for the given file in the root dictionary's entries
entry searchRootEntry(FILE *imgFile, int startOfRoot, unsigned char* filename, char* inputName) {
    int totalRoot = getIntData(imgFile, 0x11, 2);
    int i;
    for (i = 0; i < totalRoot; i++) {
        // each entry is 32 bytes
        int index = startOfRoot+i*32;
        int check = getIntData(imgFile, index, 1);
        if (check == 0) break;
        else if (check == 229) continue;

        entry newEntry = {
            getCharData(imgFile, index, 11),   // filename
            0,
            0,
            getIntData(imgFile, index+11, 1), // attribute
            getIntData(imgFile, index+28, 4),  // filesize
            0,
            0,
            0,
            getIntData(imgFile, index+26, 2) // startingCluster

        };
        if (newEntry.attribute == 0 && newEntry.startingCluster <= 0) continue; //If not a file/directory
        else if (getBit(newEntry.attribute, 3)) continue; // if its a volume label
        
        // if the current entry is the right entry, return it to the copying function
        int t= 0;
    
        // the byte length read for the fullName field is 11 bytes and same for the filename by the generation of it 
        while (t < 11) {
            if (filename[t] != newEntry.fullName[t]) {
                break;
            }
            t ++;
        }
        if (t == 11) {
            return newEntry;
        }        
    }
    printf("File %s does not exist in the root directory\n", inputName);
    fclose(imgFile);
    exit(0);
}


//Reads an entry number in the FAT and returns number of next cluster
int getNextCluster(FILE *fp, int bytesPerSect, int logicalClust) {
    int offset = floor(logicalClust*3.0/2.0);
    int bit16  = getIntData(fp, bytesPerSect + offset, 2);
    int bit12 = logicalClust%2 == 0 ? bit16 & 0xfff : bit16 >> 4;
    return bit12;
}

//Reads the data from the disk image and writes to a file in current directory
void cpToLinux(FILE *fp, char* linuxName, int startCluster, int bytesPerSect, int neededByteNum) {
    int SecPerClust = getIntData(fp, 0x0D, 1);
    int bytesPerClust = SecPerClust*bytesPerSect;
    FILE *linuxFile = fopen(linuxName, "w");
    if (linuxFile == NULL) {
        printf("hw5 error : could not write %s\n", linuxName);
        exit(1);
    }
    // start writing the disk file's clusters
    int currCluster = startCluster;
    // cluster value 0xFFF : Cluster is allocated and is the final cluster for the file (indicates end-of-file).
    // cluser value 0xFF8-0xFFE : Reserved and should not be used. May be interpreted as an allocated cluster and the final
    // cluster in the file (indicating end-of-file condition). 
    // to conclude to keep searching for next cluster we need to find cluster values smaller than 0xFF8
    while (currCluster != 0 && currCluster < 0xFF8) {
        // writing the current bytes of the cluser into the file
        // if the remaining number of bytes needed for this file is less than the number of bytes in a cluster,
        // so write only the number of remaining bytes
        int numBytesWrite = bytesPerClust < neededByteNum ? bytesPerClust : neededByteNum;
        int startOfData = 33*bytesPerSect + bytesPerClust*(currCluster - 2);
        fwrite(getCharData(fp, startOfData, numBytesWrite), 1, startOfData, linuxFile);
        neededByteNum -= bytesPerClust;
        currCluster = getNextCluster(fp, bytesPerSect, currCluster);
    }

    printf("File copied from disk successfuly!\n");
    fclose(linuxFile);
    return;
}

void cpDiskFile(FILE* imgFile, int startOfRoot, int bytesPerSect, char* fatFile, char* linuxFile) {

    unsigned char* fatFileformatted = convertFileName(fatFile);
    entry found = searchRootEntry(imgFile, startOfRoot, fatFileformatted, fatFile);
    cpToLinux(imgFile, linuxFile, found.startingCluster, bytesPerSect, found.fileSize);
    return;

}


int main(int argc, char* argv[]) {

    // ============================
    // getting the user's arguments
    // ============================
    char imgName[ARG_SIZE] = "\0";
    char cmd[ARG_SIZE] = "\0";
    int dirNotCp; 
    char fatFile[ARG_SIZE], linuxFile[ARG_SIZE];

    if ( (argv[1] == NULL) | (argv[2] == NULL) ) {
        printf("hw5 error : missing image file or command\n");
        exit(0);
    }
    strcpy(imgName, argv[1]);
    strcpy(cmd, argv[2]);
    FILE *imgFile;
    imgFile = fopen(imgName,"rb");
    if (!imgFile) {
        printf("hw5 error : invalid command (filesystem image) - failed to open '%s'.\n", imgName);
        exit(0);
    }

    if (strcmp(cmd,"dir") == 0) {
        dirNotCp = 1;
    }
    else if (strcmp(cmd,"cp") == 0) {
        dirNotCp = 0;
        if ( (argv[3] == NULL) | (argv[4] == NULL) ) {
            printf("hw5 error :invalid command, cp require a source file and a destination file\n");
            exit(0);
        }
        else {
            strcpy(fatFile, argv[3]);
            strcpy(linuxFile, argv[4]);
        }
    }
    else {
        printf("hw5 error : invalid command\n");
    }

    // ==================================
    // get main file system's information
    // ==================================
    int bytesPerSect  = getIntData(imgFile, 0x0B, 2);
    int reserved      = getIntData(imgFile, 0x0E, 2);
    int numFAT        = getIntData(imgFile, 0x10, 1);
    int sectPerFAT    = getIntData(imgFile, 0x16, 2);
    int startOfRoot  = bytesPerSect*(reserved + numFAT*sectPerFAT);

    if (dirNotCp == 1) {
        printRootEntries(imgFile, startOfRoot);
    }
    else {
        cpDiskFile(imgFile, startOfRoot, bytesPerSect, fatFile, linuxFile);
    }




    fclose(imgFile);
}


