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
    current_cluster = BootBlock.BPB_RootClus; // For Part 2; assigns current_cluster the value of the root cluster
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

// Diplays terminal as [NAME_OF_IMAGE]/[PATH_IN_IMAGE]/>
void display_prompt() {
    printf("[%s]%s> ", volume_label, current_path);
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
    for (int i = 0; i < cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00) // No more entries
            break;
        if (entry->DIR_Name[0] == 0xE5) // Skipped deleted entry
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

// Create a new directory with the given name
void create_directory(const char *dirname) {
    if (check_exists(dirname)) {
        printf("Error: Directory '%s' already exists.\n", dirname);
        return;
    }

    // Create the directory entry for the new directory
    dentry_t new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(dentry_t));
    snprintf(new_dir_entry.DIR_Name, 12, "%.11s", dirname);
    new_dir_entry.DIR_Attr = 0x10; // Directory attribute
    new_dir_entry.DIR_FstClusHI = 0;
    new_dir_entry.DIR_FstClusLO = 0;
    new_dir_entry.DIR_FileSize = 0;

    // Write the new directory entry to the current directory cluster
    uint32_t cluster_size;
    uint8_t *buffer = read_current_directory_cluster(&cluster_size);
    if (!buffer) {
        return; // Error handling in the read_current_directory_cluster function
    }

    // Find the first available slot in the current directory cluster
    dentry_t *entry = NULL;
    for (int i = 0; i < cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00 || entry->DIR_Name[0] == 0xE5) {
            memcpy(entry, &new_dir_entry, sizeof(dentry_t));
            break;
        }
    }

    // Write back the modified buffer to the image file
    uint32_t first_sector_of_cluster = ((current_cluster - 2) * BootBlock.BPB_SecPerClus) + (BootBlock.BPB_RsvdSecCnt + (BootBlock.BPB_NumFATs * BootBlock.BPB_FATSz32));
    fseek(imgFile, first_sector_of_cluster * BootBlock.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, cluster_size, 1, imgFile);

    free(buffer);

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
    snprintf(new_file_entry.DIR_Name, 12, "%.11s", filename);
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
    for (int i = 0; i < cluster_size; i += sizeof(dentry_t)) {
        entry = (dentry_t *)(buffer + i);
        if (entry->DIR_Name[0] == 0x00 || entry->DIR_Name[0] == 0xE5) {
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
    for(int i = 0; i< cluster_size; i+=sizeof(dentry_t)){
        file_entry = (dentry_t *)(buffer + i);
        if(file_entry->DIR_Name[0] == 0x00)break;
        if(file_entry->DIR_Name[0] == 0xE5) continue;

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
        printf("Error: file not open on does not exist.\n" , filename);
        return;
    }
    if(open_files[file_index].entry.DIR_Attr & 0x10){//invalid input
        printf("Error: '%s' is a directory.\n", filename);
        return;
    }
    if (strchr(open_files[file_index].mode, 'r') == NULL) {//invalid permissions
        printf("Error: file not opened for reading.\n" ,filename);
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
    printf("Data read: &.*s\n", read_size, buffer);

    open_files[file_index].file_pos = end_pos;
    free(buffer);
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
            open_file(command + 5);
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
