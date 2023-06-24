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
    unsigned char* fileName;
    unsigned char* extension;
    int attribute;
    int fileSize;
    int creationTime;
    int fineTime;
    int creationDate;
    int startingCluster;
} entry;

// void replacechar(unsigned char **s,char c1,char c2)
// {
// 	int i=0;
  
     	
//    for(i=0;s[i];i++)
// 	{  
// 		if(s[i]==c1)
// 		{
// 		   s[i]=c2;
	 
// 	    }
 
// 	}
	   
  
// }

// The function prints the file's name according to the date format in the FAT.pdf
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

// The function prints the date according to the date format in the FAT.pdf
void timePrinting(int fineTime, int creationTime) {
    //0000 0111 1110 0000
    int min = (creationTime >> 5) & 0x3f;
    //1111 1000 0000 0000
    int hour = (creationTime >> 11) & 0x1f;
    
    char am_pm[3];
    strcpy(am_pm, "AM");
    hour = 13;
    if (hour > 12) {
        hour = hour % 13;
        strcpy(am_pm, "PM");
    }
    printf("%02d:%02d %s\t", hour, min, am_pm);
    return;
}

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
        

        // char* sizeBuff = malloc(sizeof(char)*10);
        // snprintf(sizeBuff, 10, "%d", newEntry.fileSize);
        // printCenter(sizeBuff, 10);
        // printCenter(newEntry.fileName, 20);
        // printDate(newEntry.creationDate);
        // printTime(newEntry.fineTime, newEntry.creationTime);
        printf("\n");
    }
}


int main(int argc, char* argv[]) {
    char imgName[ARG_SIZE] = "\0";
    char cmd[ARG_SIZE] = "\0";
    int dirNotCp; 

    if ( (argv[1] == NULL) | (argv[2] == NULL) ) {
        printf("hw5 error : missing image file or command\n");
        exit(0);
    }
    strcpy(imgName, argv[1]);
    strcpy(cmd, argv[2]);

    FILE *imgFile;
    imgFile = fopen(imgName,"rb");
    if (!imgFile) {
        // printf("error: file open failed '%s'.\n", imgName);
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
    }
    else {
        printf("hw5 error : invalid command\n");
    }


    int bytesPerSect  = getIntData(imgFile, 0x0B, 2);
    int reserved      = getIntData(imgFile, 0x0E, 2);
    int numFAT        = getIntData(imgFile, 0x10, 1);
    int sectPerFAT    = getIntData(imgFile, 0x16, 2);
    int startOfRoot  = bytesPerSect*(reserved + numFAT*sectPerFAT);

    if (dirNotCp == 1) {
        printRootEntries(imgFile, startOfRoot);
    }


    // entry foundEntry = searchRootEntries(fp, startOfRoot, fileName);

    // writeFile(fp, argv[2], foundEntry.startingCluster, bytesPerSect, foundEntry.fileSize);

    fclose(imgFile);
}


