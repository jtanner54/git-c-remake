#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "zpipe.h"
#include <zlib.h>

int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }
    
    const char *command = argv[1];
    
    if (strcmp(command, "init") == 0) {
        /*link for mode_t: https://man7.org/linux/man-pages/man2/chmod.2.html
        it's basically a bitmask that's ORing together diff RWX perms
        https://jameshfisher.com/2017/02/24/what-is-mode_t/
        octal representation
        000 111 000 000 RWX mask for owner
        000 000 101 000 R and X for group
        000 000 000 101 R and X for other
        000 000 000 000 no id or swapped text*/
        
        if (mkdir(".git", 0755) == -1 || 
            mkdir(".git/objects", 0755) == -1 || 
            mkdir(".git/refs", 0755) == -1) {
            fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
            return 1;
        }
        
        FILE *headFile = fopen(".git/HEAD", "w");
        if (headFile == NULL) {
            fprintf(stderr, "Failed to create .git/HEAD file: %s\n", strerror(errno));
            return 1;
        }
        fprintf(headFile, "ref: refs/heads/main\n");
        fclose(headFile);
        
        printf("Initialized git directory\n");
    } else if (strcmp(command, "cat-file") == 0) {
        char full_path[55] = ""; // always same length
        char* first_part_filename = ".git/objects/";
        // assume fourth arg is text file
        char second_part_filename[4] = ""; // first two digits of git-object with /
        second_part_filename[2] = '/';
        strncpy(second_part_filename, argv[3], 2);
        char third_part_filename[39] = "";
        strncpy(third_part_filename, argv[3] + 2, 38); // need to add 2 to pointer to skip first 2 chars

        strcat(full_path, first_part_filename);
        strcat(full_path, second_part_filename);
        strcat(full_path, third_part_filename);

        FILE* file = fopen(full_path, "rb");
        if (file == NULL) {
            fprintf(stderr, "CAN'T OPEN FILE: %s\n", strerror(errno));
        }

        fflush(stdout);
        FILE* dest_file = tmpfile();
        int ret = inf(file, dest_file);
        if (ret != Z_OK) {
            fprintf(stderr, "Decompression failed with error code: %d\n", ret);
            if (ret == Z_MEM_ERROR) fprintf(stderr, "Out of memory\n");
            if (ret == Z_DATA_ERROR) fprintf(stderr, "Invalid or incomplete deflate data\n");
            if (ret == Z_VERSION_ERROR) fprintf(stderr, "zlib version mismatch\n");
            return 1;
        }

        // reset pointer to start of file
        rewind(dest_file);

        if (dest_file != NULL) {
            char ch;
            int occurence = 0;

            while ((ch = fgetc(dest_file)) != EOF) {
                // have to get rid of Git adds a space 
                // followed by the size in bytes of the content, and adding a final null byte:
                if (ch == '\0' && occurence == 0) {
                    occurence += 1;
                    continue;
                }
                if (occurence > 0)
                    printf("%c", ch);
            }
        }

        fclose(dest_file);
        fclose(file);
    } else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }
    
    return 0;
}