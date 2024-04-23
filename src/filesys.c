// filesys.c

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

// Variable Declarations 
// Variables for Part 1:

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

char current_path[1024] = "/";  // Initially at the root directory.
char volume_label[12] = "";     // To store the volume label extracted from BootBlock.
char img_path[50] = "";

// Variables for Part 2:
// Define a global variable to store the current cluster of the working directory
uint32_t current_cluster = 0; // Root cluster at the start

// Variables for Part 3:
typedef struct{
    dentry_t entry;
    uint32_t file_pos;
    char mode[3];
}open_file_t;

#define MAX_OPEN_FILES 32
open_file_t open_files[MAX_OPEN_FILES];
int open_files_count=0;

// ============================================================================
// ============================================================================

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

    snprintf(volume_label, sizeof(volume_label), "%.11s", BootBlock.BS_VolLab);
    snprintf(img_path, 50, "%s", imgPath);
    current_cluster = BootBlock.BPB_RootClus; // For Part 2; assigns current_cluster the value of the root cluster
}

// Getting FAT32 File info
void getInfo(){
    uint32_t total_clusters = BootBlock.BPB_TotSec32 / BootBlock.BPB_SecPerClus; 
    uint32_t image_size = BootBlock.BPB_TotSec32 * BootBlock.BPB_BytsPerSec;
    uint32_t entries_in_fat = (BootBlock.BPB_FATSz32 * BootBlock.BPB_BytsPerSec) / 4;

    printf("Bytes per sector: %u\n", BootBlock.BPB_BytsPerSec);
    printf("Sectors per cluster: %u\n", BootBlock.BPB_SecPerClus);
    printf("Root cluster: %u\n", BootBlock.BPB_RootClus);
    printf("Total # of clusters in data region: %u\n", total_clusters);
    printf("Number of entries in FAT: %u\n", entries_in_fat);
    printf("Size of image (bytes): %u\n", image_size);
}

// Diplays terminal as [NAME_OF_IMAGE]/[PATH_IN_IMAGE]/>
void display_prompt() {
    printf("%s%s> ", img_path, current_path);
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

// ============================================================================
// ============================================================================

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
    uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + first_data_sector;
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
    for (int i = 0; i < (int)cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char) entry->DIR_Name[0] ==  0xE5) // Skipped deleted entry
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
        current_cluster = new_cluster;
        // Update the path
        if (strcmp(dirname, "..") == 0) {
            // Handle going up in the directory tree
            char *last_slash = strrchr(current_path, '/');
            if (last_slash != current_path) {  // Not at the root
                *last_slash = '\0';
            } else {
                *(last_slash + 1) = '\0';  // Stay at the root
            }
        } else {
            // Append new directory to path
            if (strlen(current_path) > 1) {
                strcat(current_path, "/");
            }
            strcat(current_path, dirname);
        }
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
    for (int i = 0; i < (int)cluster_size; i += sizeof(dentry_t)) {
        dentry_t *entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char)entry->DIR_Name[0] == 0xE5){ // Skipped deleted entry
            continue;
        }

        char formatted_name[12];
        format_dirname(entry->DIR_Name, formatted_name);

        printf("%s\n", formatted_name);
    }

    free(buffer);
}

// ============================================================================
// ============================================================================

// Part 3: Create

// Helper function to check if a directory or file with the given name already exists
bool check_exists(const char *name) {
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return true; // Error handling in the read_current_directory_cluster function
    }

    dentry_t *entry = NULL;
    for (int i = 0; i < (int) cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char) entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
            continue;

        char formatted_name[12];
        format_dirname(entry->DIR_Name, formatted_name);

        if (strcasecmp(formatted_name, name) == 0) {
            free(buffer);
            return true; // Found a matching directory or file
        }
    }

    free(buffer);
    return false; // No existing directory or file matches the given name
}

uint32_t find_free_cluster() {
    uint32_t fat_offset = BootBlock.BPB_RsvdSecCnt * BootBlock.BPB_BytsPerSec;
    uint32_t fat_size = BootBlock.BPB_FATSz32 * BootBlock.BPB_BytsPerSec;
    uint32_t cluster_count = BootBlock.BPB_TotSec32 / BootBlock.BPB_SecPerClus;

    uint8_t *fat_buffer = malloc(fat_size);
    if (fat_buffer == NULL) {
        perror("Memory allocation failed");
        return 0;
    }

    fseek(imgFile, fat_offset, SEEK_SET);
    fread(fat_buffer, fat_size, 1, imgFile);

    uint32_t *fat = (uint32_t *)fat_buffer;
    uint32_t free_cluster = 0;

    for (uint32_t i = 2; i < cluster_count; i++) {
        uint32_t entry = fat[i];
        if (entry == 0) {
            free_cluster = i;
            break;
        }
    }

    free(fat_buffer);
    return free_cluster;
}

void create_dot_entries(uint32_t new_cluster, uint32_t parent_cluster) {
    uint32_t cluster_size = BootBlock.BPB_BytsPerSec * BootBlock.BPB_SecPerClus;
    uint8_t *buffer = malloc(cluster_size);
    if (!buffer) {
        perror("Memory allocation failed");
        return;
    }
    memset(buffer, 0, cluster_size); // Clear the buffer

    dentry_t dot_entry, dot_dot_entry;
    memset(&dot_entry, 0, sizeof(dentry_t));
    memset(&dot_dot_entry, 0, sizeof(dentry_t));

    // Create the "." entry
    strncpy(dot_entry.DIR_Name, ".          ", 11);
    dot_entry.DIR_Attr = 0x10;
    dot_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    dot_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;

    // Create the ".." entry
    strncpy(dot_dot_entry.DIR_Name, "..         ", 11);
    dot_dot_entry.DIR_Attr = 0x10;
    dot_dot_entry.DIR_FstClusLO = parent_cluster & 0xFFFF;
    dot_dot_entry.DIR_FstClusHI = (parent_cluster >> 16) & 0xFFFF;

    // Write the "." and ".." entries to the new directory cluster
    memcpy(buffer, &dot_entry, sizeof(dentry_t));
    memcpy(buffer + sizeof(dentry_t), &dot_dot_entry, sizeof(dentry_t));

    uint32_t first_sector_of_cluster = ((new_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, cluster_size, 1, imgFile);

    free(buffer);
}

void create_directory(const char *dirname) {
    if (check_exists(dirname)) {
        printf("Error: Directory '%s' already exists.\n", dirname);
        return;
    }

    // Create the directory entry for the new directory
    dentry_t new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(dentry_t));
    snprintf(new_dir_entry.DIR_Name, 11, "%.11s", dirname);
    new_dir_entry.DIR_Attr = 0x10; // Directory attribute
    new_dir_entry.DIR_FstClusHI = 0;
    new_dir_entry.DIR_FstClusLO = 0; // Cluster number will be assigned later
    new_dir_entry.DIR_FileSize = 0;

    // Find the first available cluster for the new directory
    uint32_t new_cluster = find_free_cluster();
    if (new_cluster == 0) {
        printf("Error: No free clusters available to create the directory.\n");
        return;
    }
    new_dir_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    new_dir_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;

    // Write the new directory entry to the current directory cluster
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Error handling in the read_current_directory_cluster function
    }

    dentry_t *entry = NULL;
    for (int i = 0; i < (int)cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00 || (unsigned char)entry->DIR_Name[0] == 0xE5) {
            memcpy(entry, &new_dir_entry, sizeof(dentry_t));
            break;
        }
    }

    uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, cluster_size, 1, imgFile);
    free(buffer);

    // Create the "." and ".." entries in the new directory cluster
    create_dot_entries(new_cluster,current_cluster);

    printf("Directory '%s' created successfully.\n", dirname);
}
// Create a new file with the given name
void create_file(const char *filename) {
    if (check_exists(filename)) {
        printf("Error: File '%s' already exists.\n", filename);
        return;
    }


    // Create the directory entry for the new file
    dentry_t new_file_entry;
    memset(&new_file_entry, 0, sizeof(dentry_t));

    snprintf(new_file_entry.DIR_Name, 11, "%.11s", filename);
    new_file_entry.DIR_Attr = 0x00; // File attribute
    new_file_entry.DIR_FstClusHI = 0;
    new_file_entry.DIR_FstClusLO = 0;
    new_file_entry.DIR_FileSize = 0;

    // Write the new file entry to the current directory cluster
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Error handling in the read_current_directory_cluster function
    }


    // Find the first available slot in the current directory cluster
    dentry_t *entry = NULL;
    for (int i = 0; i < (int) cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00 || (unsigned char)entry->DIR_Name[0] == 0xE5) {
            memcpy(entry, &new_file_entry, sizeof(dentry_t));
            break;
        }
    }

    // Write back the modified buffer to the image file
    uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, cluster_size, 1, imgFile);

    free(buffer);

    printf("File '%s' created successfully.\n", filename);
}

// ============================================================================
// ============================================================================

// Part 4: Read
void open_file(char* input){
    while(*input == ' ')
        input++;

    char filename[13];
    char flags[4];
    sscanf(input, "%s %s", filename, flags);//seperate input

    if(strcmp(flags,"-r") != 0 && strcmp(flags,"-w")!= 0 &&//make sure flags are valid and present
        strcmp(flags, "-rw") != 0 && strcmp(flags, "-wr") != 0){
            printf("invalid mode: %s", flags);
            return;
    }

    for(int i = 0; i <open_files_count; i++){//check to make sure file is not already open
        if(strncmp(open_files[i].entry.DIR_Name, filename, 11) == 0){
            printf("File alr open: %s\n",filename);
            return;
        }
    }
    uint32_t cluster_size;
    uint8_t* buffer = read_current_directory_cluster(&cluster_size);//read current directory
    if(!buffer){//return if empty
        return;
    }

    bool file_found = false;
    dentry_t *file_entry = NULL;
    for(int i = 0; i < (int)cluster_size; i+=sizeof(dentry_t)){
        file_entry = (dentry_t *)(buffer + i);
        if(file_entry->DIR_Name[0] == 0x00)break;
        if((unsigned char)file_entry->DIR_Name[0] == 0xE5) continue;

        char formatted_name[12];
        format_dirname(file_entry->DIR_Name, formatted_name);
        if(strcmp(formatted_name, filename) == 0 && !(file_entry->DIR_Attr & 0x10)){
            file_found=true;//file has been found
            break;
        }
    }
    if(!file_found){//cant open file, prob doesnt exist
        printf("File not found:%s\n", filename);
        free(buffer);
        return;
    }
    if(open_files_count>=MAX_OPEN_FILES){//too many open, max is 32
        printf("Max files opened\n");
        free(buffer);
        return;
    }
    //file successfully opened, increment counters, set it to open
    open_files[open_files_count].entry = *file_entry;
    strcpy(open_files[open_files_count].mode, flags);
    open_files[open_files_count].file_pos = 0;
    open_files_count++;
    printf("File opened successfully: %s with flags: %s\n", filename, flags);
    free(buffer);//reallocate space
}

void close_file(char* input){

    while(*input == ' ')
        input++;

    char filename[13];
    sscanf(input, "%s", filename);//seperate input

    bool file_found = false;

    for(int i = 0; i < open_files_count;i++){
        char formatted_name[12];
        format_dirname(open_files[i].entry.DIR_Name, formatted_name);
        if(strcmp(formatted_name, filename) == 0){
            file_found=true;//found the file
            break;
        }
    }


    if(!file_found){//file not found, type or doesnt exist
        printf("File not found:%s\n", filename);

        return;
    }
    for(int y = 0; y<open_files_count-1;y++){
        open_files[y] = open_files[y+1];//iterate through
    }

    open_files_count--;//file found, close it out
    printf("file closed successfully: %s\n", filename);
}

void list_all(){
    if(open_files_count == 0){//no files to list
        printf("No files to open currently.\n");
        return;
    }

    printf("active open files:\n");
    for(int i = 0; i<open_files_count;i++){//iterate through the files, print mode and permissions and name
        char formatted_name[12];
        format_dirname(open_files[i].entry.DIR_Name, formatted_name);
        printf("%-11s\t%s\t%u\n", formatted_name, open_files[i].mode, open_files[i].file_pos);
    }
}

void set_file_offset(char *input){
    char filename[13];
    int offset;
    char pos_str[10];
    sscanf(input, "%s %d %s", filename, &offset, pos_str);//seperate input

    int test;//get different options
    if(strcmp(pos_str, "SEEK_SET") == 0){
        test = SEEK_SET;
    }else if(strcmp(pos_str, "SEEK_CUR") ==0){
        test = SEEK_CUR;
    }else if(strcmp(pos_str, "SEEK_END") == 0){
        test = SEEK_END;
    }else {
        printf("Invalid choice: %s\n", pos_str);
        return;    
    }

    for(int i = 0; i <open_files_count; i++){
        char formatted_name[12];
        format_dirname(open_files[i].entry.DIR_Name, formatted_name);//loop through opened files
        if(strcmp(formatted_name,filename) ==0){
            uint32_t new_pos;
            switch(test){//change offset according to input
                case SEEK_SET:
                    new_pos = offset;
                    break;
                case SEEK_CUR:
                    new_pos = open_files[i].file_pos + offset;
                    break;
                case SEEK_END:
                    new_pos = open_files[i].entry.DIR_FileSize+offset;
                    break;
                default:
                    printf("invalid operation\n");
                    return;
            }
            if(new_pos > open_files[i].entry.DIR_FileSize){
                printf("Error: pos out of bounds.\n");
                return;
            }
            open_files[i].file_pos = new_pos;//successful
            printf("File pos updated: %s to %u\n", filename, new_pos);
            return;
        }
    }
    printf("File not open: %s\n", filename);
}

uint32_t calculate_file_offset(dentry_t entry){
    uint32_t first_sector_of_cluster = ((entry.DIR_FstClusLO -2) * BootBlock.BPB_SecPerClus) + BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32);
    return first_sector_of_cluster * BootBlock.BPB_BytsPerSec;
}

void read_file(char *input){
    char filename[13];
    int size;
    sscanf(input, "%s %d", filename, &size);//format the input

    bool file_open = false;
    int file_index = -1;

    for(int i = 0; i < open_files_count; i++){//loop through opened files
        char formatted_name[12];
        format_dirname(open_files[i].entry.DIR_Name, formatted_name);
        if(strcmp(formatted_name, filename) == 0){
            file_open=true;//file found
            file_index = i;
            break;
        }
    }

    if(!file_open){
        printf("Error: file '%s' not open or does not exist.\n", filename);
        return;
    }
    if(open_files[file_index].entry.DIR_Attr & 0x10){//invalid input
        printf("Error: '%s' is a directory.\n", filename);
        return;
    }
    if (strchr(open_files[file_index].mode, 'r') == NULL) {//invalid permissions
        printf("Error: file '%s'not opened for reading.\n" ,filename);
        return;
    }

    //open the file
    uint32_t start_pos = open_files[file_index].file_pos;
    uint32_t end_pos = start_pos + size;
    if(end_pos > open_files[file_index].entry.DIR_FileSize){
        end_pos = open_files[file_index].entry.DIR_FileSize;
    }
    //calculate appropriate offset
    fseek(imgFile, calculate_file_offset(open_files[file_index].entry), SEEK_SET);
    fseek(imgFile, start_pos, SEEK_CUR);

    //current size of text
    int read_size = end_pos - start_pos;
    char *buffer = malloc(read_size);
    if(buffer == NULL){//something happened?
        perror("Mem alloc failed");
        return;
    }
    fread(buffer, read_size, 1, imgFile);
    printf("Data read: %.*s\n", read_size, buffer);

    open_files[file_index].file_pos = end_pos;
    free(buffer);
}

// ============================================================================
// ============================================================================

void extend_file(dentry_t *entry, uint32_t new_file_size) {
    uint32_t current_size = entry->DIR_FileSize;
    uint32_t cluster_size = BootBlock.BPB_BytsPerSec * BootBlock.BPB_SecPerClus;

    if (new_file_size <= current_size) {
        // File size is not increasing, no need to extend
        return;
    }

    uint32_t cur_cluster = entry->DIR_FstClusLO | (entry->DIR_FstClusHI << 16);
    uint32_t next_cluster = 0;
    // uint32_t data_start = BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32);

    // Traverse the existing cluster chain
    while (cur_cluster != 0) {
        uint32_t fat_offset = cur_cluster * sizeof(uint32_t);
        fseek(imgFile, BootBlock.BPB_RsvdSecCnt * BootBlock.BPB_BytsPerSec + fat_offset, SEEK_SET);
        fread(&next_cluster, sizeof(uint32_t), 1, imgFile);

        if (current_size + cluster_size >= new_file_size) {
            // Enough space in the existing cluster chain
            break;
        }

        current_size += cluster_size;
        cur_cluster = next_cluster;
    }

    // Allocate new clusters as needed
    while (current_size < new_file_size) {
        uint32_t free_cluster = find_free_cluster();
        if (free_cluster == 0) {
            printf("Error: No free clusters available to extend the file.\n");
            return;
        }

        if (next_cluster == 0) {
            // First cluster in the chain
            entry->DIR_FstClusLO = free_cluster & 0xFFFF;
            entry->DIR_FstClusHI = (free_cluster >> 16) & 0xFFFF;
        } else {
            // Update the FAT entry for the previous cluster
            uint32_t fat_offset = next_cluster * sizeof(uint32_t);
            fseek(imgFile, BootBlock.BPB_RsvdSecCnt * BootBlock.BPB_BytsPerSec + fat_offset, SEEK_SET);
            fwrite(&free_cluster, sizeof(uint32_t), 1, imgFile);
        }

        next_cluster = free_cluster;
        current_size += cluster_size;
    }

    // Update the file size
    entry->DIR_FileSize = new_file_size;
}

void write_file(char *input) {
    char filename[13];
    char string[256];
    sscanf(input, "%s %256s", filename, string);

    bool file_open = false;
    int file_index = -1;


    for(int i = 0; i < open_files_count; i++){//loop through opened files
        char formatted_name[12];
        format_dirname(open_files[i].entry.DIR_Name, formatted_name);
        if(strcmp(formatted_name, filename) == 0){
            file_open=true;//file found
            file_index = i;
            break;
        }
    }
    
    if (!file_open) {
        printf("Error: file '%s' not open or does not exist.\n", filename);
        return;
    }

    if (open_files[file_index].entry.DIR_Attr & 0x10) {
        printf("Error: '%s' is a directory.\n", filename);
        return;
    }

    if (strchr(open_files[file_index].mode, 'w') == NULL) {
        printf("Error: file '%s' not opened for writing.\n", filename);
        return;
    }

    uint32_t file_offset = open_files[file_index].file_pos;
    uint32_t string_length = strlen(string);
    uint32_t new_file_size = file_offset + string_length;

    if (new_file_size > open_files[file_index].entry.DIR_FileSize) {
        extend_file(&open_files[file_index].entry, new_file_size);
    }

    fseek(imgFile, calculate_file_offset(open_files[file_index].entry), SEEK_SET);
    fseek(imgFile, file_offset, SEEK_CUR);
    fwrite(string, sizeof(char), string_length, imgFile);

    // open_files[file_index].file_pos = new_file_size;
}

// ============================================================================
// ============================================================================

// Part 6: rm and rmdir

uint8_t* read_directory_cluster(uint32_t* cluster_size, uint32_t dir_cluster) {
    *cluster_size = BootBlock.BPB_BytsPerSec * BootBlock.BPB_SecPerClus;
    uint8_t *buffer = malloc(*cluster_size);
    if (!buffer) {
        perror("Memory allocation failed");
        return NULL;
    }

    uint32_t first_data_sector = BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32);
    uint32_t first_sector_of_cluster = ((dir_cluster - 2) * BootBlock.BPB_SecPerClus) + first_data_sector;
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fread(buffer, *cluster_size, 1, imgFile);
    return buffer;
}

void reclaim_file_clusters(dentry_t *entry) {
    uint32_t current_cluster = entry->DIR_FstClusLO | (entry->DIR_FstClusHI << 16);
    uint32_t next_cluster = 0;
    // uint32_t data_start = BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32);

    while (current_cluster != 0) {
        uint32_t fat_offset = current_cluster * sizeof(uint32_t);
        fseek(imgFile, BootBlock.BPB_RsvdSecCnt * BootBlock.BPB_BytsPerSec + fat_offset, SEEK_SET);
        fread(&next_cluster, sizeof(uint32_t), 1, imgFile);

        // Mark the cluster as free in the FAT
        uint32_t free_cluster = 0;
        fseek(imgFile, BootBlock.BPB_RsvdSecCnt * BootBlock.BPB_BytsPerSec + fat_offset, SEEK_SET);
        fwrite(&free_cluster, sizeof(uint32_t), 1, imgFile);

        current_cluster = next_cluster;
    }
}

void remove_file(char *input) {
    char filename[13];
    sscanf(input, "%s", filename);

    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Error handling in the read_current_directory_cluster function
    }

    dentry_t *entry = NULL;
    int entry_index = -1;
    for (int i = 0; i < (int) cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char)entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
            continue;

        char formatted_name[12];
        format_dirname(entry->DIR_Name, formatted_name);

        if (strcasecmp(formatted_name, filename) == 0) {
            if (entry->DIR_Attr & 0x10) { // Directory attribute
                printf("Error: '%s' is a directory.\n", filename);
                free(buffer);
                return;
            }

            // Check if the file is opened
            for (int j = 0; j < open_files_count; j++) {
                char open_name[12];
                format_dirname(open_files[j].entry.DIR_Name, open_name);
                if (strcasecmp(open_name, filename) == 0) {
                    printf("Error: File '%s' is currently open.\n", filename);
                    free(buffer);
                    return;
                }
            }

            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        printf("Error: File '%s' not found.\n", filename);
        free(buffer);
        return;
    }

    // Remove the file entry from the directory
    entry->DIR_Name[0] = 0xE5; // Mark the entry as deleted

    // Write back the modified buffer to the image file
    uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, cluster_size, 1, imgFile);

    free(buffer);

    // Reclaim the actual file data
    reclaim_file_clusters(entry);

    printf("File '%s' deleted successfully.\n", filename);
}

void remove_directory(const char *input) {
    char dirname[13];
    sscanf(input, "%s", dirname);

    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Error handling in the read_current_directory_cluster function
    }

    dentry_t *entry = NULL;
    int entry_index = -1;
    for (int i = 0; i < (int) cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char)entry->DIR_Name[0] == 0xE5) // Skip deleted entry
            continue;

        char formatted_name[12];
        format_dirname(entry->DIR_Name, formatted_name);

        if (strcasecmp(formatted_name, dirname) == 0) {
            if (!(entry->DIR_Attr & 0x10)) { // Not a directory
                printf("Error: '%s' is not a directory.\n", dirname);
                free(buffer);
                return;
            }

            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        printf("Error: Directory '%s' not found.\n", dirname);
        free(buffer);
        return;
    }

    // Check if the directory is empty
    uint32_t dir_cluster = entry->DIR_FstClusLO | (entry->DIR_FstClusHI << 16);
    uint32_t dir_cluster_size;
    uint8_t *dir_buffer = read_directory_cluster(&dir_cluster_size,dir_cluster);

    if (!dir_buffer) {
        free(buffer);
        return; // Error handling in the read_current_directory_cluster function
    }

    int empty_dir = 1;
    dentry_t *dir_entry = NULL;
    for (int i = 0; i < (int)dir_cluster_size; i += sizeof(dentry_t)) {
        dir_entry = (dentry_t *)(dir_buffer + i);
        if (dir_entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if ((unsigned char)dir_entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
            continue;

        char formatted_name[12];
        format_dirname(dir_entry->DIR_Name, formatted_name);

        if (strcmp(formatted_name, ".") != 0 && strcmp(formatted_name, "..") != 0) {
            empty_dir = 0;
            break;
        }
    }

    if (empty_dir) {
        // Remove the directory entry from the current directory
        entry->DIR_Name[0] = 0xE5; // Mark the entry as deleted

        // Write back the modified buffer to the image file
        uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
        fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
        fwrite(buffer, cluster_size, 1, imgFile);

        free(buffer);
        free(dir_buffer);

        // Reclaim the clusters occupied by the directory
        reclaim_file_clusters(entry);

        printf("Directory '%s' removed successfully.\n", dirname);
    } else {
        printf("Error: Directory '%s' is not empty.\n", dirname);
        free(buffer);
        free(dir_buffer);
    }
}

// ============================================================================
// ============================================================================

// Main Functions

void main_process() {
    char command[256];
    while (1) {
        display_prompt();
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
        else if (strncmp(command, "mkdir ", 6) == 0)
            create_directory(command + 6); // Skip "mkdir "
        else if (strncmp(command, "creat ", 6) == 0)
            create_file(command + 6); // Skip "creat "
        else if(strncmp(command, "open ", 5) == 0)
            open_file(command + 5);
        else if(strncmp(command, "close ", 6) == 0)
            close_file(command + 6);
        else if(strncmp(command, "lsof", 5) == 0)
            list_all();
        else if(strncmp(command, "lseek ", 6) == 0)
            set_file_offset(command + 6);
        else if(strncmp(command, "read ", 5) == 0)
            read_file(command + 5);
        else if(strncmp(command, "write ", 6) == 0)
            write_file(command + 6);
        else if(strncmp(command, "rm ", 3) == 0)
            remove_file(command + 3);
        else if(strncmp(command, "rmdir ", 6) == 0)
            remove_directory(command + 6);
        else
            printf("Invalid command.\n");
    }
}

int main(int argc, char const *argv[])
{
    // Open and mount FAT32 File
    if (argc != 2) {
        printf("Usage: filesys <FAT32 ISO>\n");
        return 1;
    }
    mount_fat32(argv[1]);

    // Run main process
    main_process();

    return 0;
}
