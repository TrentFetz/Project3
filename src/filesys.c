#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

// Variable Declarations 

typedef struct __attribute__((packed)) BPB {
    // below 36 bytes are the main bpb
	uint8_t BS_jmpBoot[3];
	char BS_OEMName[8];
	uint16_t BPB_BytsPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    char BPB_Reserved[12];
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved1;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    char BS_VolLab[11]; 
    char BS_FilSysType[8];
} bpb_t;

typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; // DIR_NTRes, DIR_CrtTimeTenth, DIR_CrtTime, DIR_CrtDate, 
                       // DIR_LstAccDate. Since these fields are not used in
                       // Project 3, just define as a placeholder.
    uint16_t DIR_FstClusHI;
    char padding_2[4]; // DIR_WrtTime, DIR_WrtDate
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;


// Part 1: Mount the Image File

FILE *imgFile;
bpb_t BootBlock;

// Opening the FAT32 file
void mount_fat32(const char *imgPath) {
    imgFile = fopen(imgPath, "rb+");
    if (imgFile == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fread(&BootBlock, sizeof(BootBlock), 1, imgFile);
}

// Getting FAT32 File info
void getInfo(){
    printf("BS_jmpBoot: %02X %02X %02X\n",BootBlock.BS_jmpBoot[0], BootBlock.BS_jmpBoot[1], BootBlock.BS_jmpBoot[2]);
    printf("BS_OEMName: %.8s\n", BootBlock.BS_OEMName);
    printf("BPB_BytsPerSec: %u\n", BootBlock.BPB_BytsPerSec);
    printf("BPB_SecPerClus: %u\n", BootBlock.BPB_SecPerClus);
    printf("BPB_RsvdSecCnt: %u\n", BootBlock.BPB_RsvdSecCnt);
    printf("BPB_NumFATs: %u\n", BootBlock.BPB_NumFATs);
    printf("BPB_RootEntCnt: %u\n", BootBlock.BPB_RootEntCnt);
    printf("BPB_TotSec16: %u\n", BootBlock.BPB_TotSec16);
    printf("BPB_Media: %02X\n", BootBlock.BPB_Media);
    printf("BPB_FATSz16: %u\n", BootBlock.BPB_FATSz16);
    printf("BPB_SecPerTrk: %u\n", BootBlock.BPB_SecPerTrk);
    printf("BPB_NumHeads: %u\n", BootBlock.BPB_NumHeads);
    printf("BPB_HiddSec: %u\n", BootBlock.BPB_HiddSec);
    printf("BPB_TotSec32: %u\n", BootBlock.BPB_TotSec32);
    printf("BPB_FATSz32: %u\n", BootBlock.BPB_FATSz32);
    printf("BPB_ExtFlags: %u\n", BootBlock.BPB_ExtFlags);
    printf("BPB_FSVer: %u\n", BootBlock.BPB_FSVer);
    printf("BPB_RootClus: %u\n", BootBlock.BPB_RootClus);
    printf("BPB_FSInfo: %u\n", BootBlock.BPB_FSInfo);
    printf("BPB_BkBootSec: %u\n", BootBlock.BPB_BkBootSec);
    printf("BPB_Reserved: %.12s\n", BootBlock.BPB_Reserved);
    printf("BS_DrvNum: %u\n", BootBlock.BS_DrvNum);
    printf("BS_Reserved1: %u\n", BootBlock.BS_Reserved1);
    printf("BS_BootSig: %02X\n", BootBlock.BS_BootSig);
    printf("BS_VolID: %u\n", BootBlock.BS_VolID);
    printf("BS_VolLab: %.11s\n", BootBlock.BS_VolLab);
    printf("BS_FilSysType: %.8s\n", BootBlock.BS_FilSysType);
}

// Exiting the program
void exitProgram() {
    // close the image file
    if (imgFile != NULL) {
        fclose(imgFile);
        imgFile = NULL;
    }
    // free any other resources here ...
    exit(0);
}

// Part 2: Navigation

// Defining Directory Structure
typedef struct directory_node {
    dentry_t entry;
    struct directory_node* next;
} dir_node_t;

// Reads the content of the current directory cluster into a buffer
uint8_t* read_current_directory_cluster(uint32_t* cluster_size) {
    *cluster_size = BootBlock.BPB_BytsPerSec * BootBlock.BPB_SecPerClus;
    uint8_t *buffer = malloc(*cluster_size);
    if (!buffer) {
        perror("Memory allocation failed");
        return NULL;
    }

    uint32_t first_data_sector = BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32);
    uint32_t first_sector_of_cluster = ((BootBlock.BPB_RootClus - 2) * BootBlock.BPB_SecPerClus) + first_data_sector;
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fread(buffer, *cluster_size, 1, imgFile);
    return buffer;
}

// Formats directory names from FAT32 to a more readable format
void format_dirname(const char *DIR_Name, char *formatted_name) {
    // Copy the directory name to formatted_name
    snprintf(formatted_name, 12, "%.11s", DIR_Name);
    
    // Trim trailing spaces
    int len = strlen(formatted_name);
    while (len > 0 && formatted_name[len - 1] == ' ') {
        formatted_name[len - 1] = '\0'; // Replace trailing space with null terminator
        len--;
    }
}



// Changing the directory
void change_directory(const char* dirname) {
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Memory allocation failed, error handled in the helper function
    }

    int found = 0;
    dentry_t *entry = NULL;
    for (int i = 0; i < cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if (entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
            continue;
        if (entry->DIR_Attr & 0x10) { // Directory attribute
            char formatted_name[12];
            format_dirname(entry->DIR_Name, formatted_name);

            /* TESTING FLAG: 
            printf("Comparing: '%s' with '%s'\n", formatted_name, dirname);
            */
            
            // Compare ignoring case and trailing spaces
            if (strcasecmp(formatted_name, dirname) == 0) {
                found = 1;
                break;
            }
        }
    }

    if (found) {
        uint32_t new_cluster = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;
        // Update current_cluster globally or using a shared variable
        uint32_t current_cluster = new_cluster;
        printf("Changing directory to %s\n", dirname);
    } else {
        printf("Directory not found\n");
    }

    free(buffer);
}

// Listing the directories
void list_directory() {
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Memory allocation failed, error handled in the helper function
    }

    printf("Listing directory contents:\n");
    for (int i = 0; i < cluster_size; i += sizeof(dentry_t)) {
        dentry_t *entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if (entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
            continue;
        if (entry->DIR_Attr & 0x10) { // Directory attribute
            char formatted_name[12];
            format_dirname(entry->DIR_Name, formatted_name);
            printf("%s\n", formatted_name);
        }
    }

    free(buffer);
}


// Main Functions

void main_process() {
    char command[256];
    while (1) {
        printf("[FAT32Shell]>");
        fgets(command, 256, stdin);

        // remove trailing newline
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0)
            exitProgram();
        else if (strcmp(command, "info") == 0)
            getInfo();
        else if (strncmp(command, "cd ", 3) == 0)
            change_directory(command + 3); // Skip "cd "
        else if (strcmp(command, "ls") == 0)
            list_directory();
        else
            printf("Invalid command.\n");
    }
}

int main(int argc, char const *argv[])
{

    if (argc != 2) {
        printf("Usage: filesys <FAT32 ISO>\n");
        return 1;
    }
    mount_fat32(argv[1]);
    main_process();

    getInfo();

    // 1. open the fat32.img

    // 2. mount the fat32.img

    // 3. main procees

    // 4. close all opened files

    // 5. close the fat32.img

    return 0;
}

