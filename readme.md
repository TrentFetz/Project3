
# Project 3

Overview of assignment 
```
1. Mount the Image File
2. Navigation
3. Create
4. Read
5. Update
6. Delete
```

## Group Members

- **Maverick Haghighat Schiller:** mgh21a@fsu.edu
- **Trent Fetzer:** tjf20bj@fsu.edu
- **Osher Steel:** os19h@fsu.edu
  
## Division of Labor

### Part 1. Mount the Image File
Assigned to: Maverick Haghighat Schiller, Trent Fetzer
### Part 2. Navigation
  Assigned to: Maverick Haghighat Schiller, Trent Fetzer
### Part 3. Create
  Assigned to: Maverick Haghighat Schiller, Osher Steel
### Part 4. Read
  Assigned to: Trent Fetzer, Osher Steel
### Part 5. Update
  Assigned to: Osher Steel, Maverick Haghighat Schiller
### Part 6. Delete
  Assigned to: Osher Steel, Trent Fetzer

## File Listing
```
├── Makefile
├── readme.md
└── src
  └── filesys.c
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc` for C/C++

### Compilation
1. Open the src folder using: `cd src`
2. Compile the program using: `gcc filesys.c -o filesys.c`

### Execution
3. Run the program using: `./filesys <FAT32_FILE>`
4. Test the program by using commands: `info, cd, ls, mkdir, creat, open, close, lsof, lseek, read, write, rm, rmdir, exit`

## Bugs
- There is a small bug that occurs when trying to move up a directory using `cd ..`. This error may reside in the FAT32 file rather than the code's logic as it does not occur on newly created directories.
- Occasional errors with `extend_file()` or `find_free_cluster()` functions. Unsure of error's souce.
- Spaghetti code: write only works on pre-existing files when commenting out sp `open_files[file_index].file_pos = new_file_size`.
