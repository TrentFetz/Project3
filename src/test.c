#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024  // Adjust this buffer size as needed

void print_file_contents(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Allocate a buffer to read file contents
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Read and print file contents in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);  // Write to stdout (console)
    }

    // Check for read errors or end-of-file
    if (ferror(file)) {
        perror("Error reading file");
    }

    fclose(file);  // Close the file
}

int main() {
    const char *filename = "fat32.img";
    print_file_contents(filename);
    return 0;
}
