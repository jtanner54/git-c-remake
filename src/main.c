#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include "zlib.h"
#include <assert.h>

#define CHUNK 16384

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

        FILE* file = fopen(full_path, "r");
        if (file == NULL) {
            fprintf(stderr, "CAN'T OPEN FILE: %s\n", strerror(errno));
        }

        int ret;
        unsigned have;
        z_stream strm;
        unsigned char in[CHUNK];
        unsigned char out[CHUNK];

        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        ret = inflateInit(&strm);
        if (ret != Z_OK) 
            return ret;

        do {
            strm.avail_in = fread(in, 1, CHUNK, file);
            if (ferror(file)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
            if (strm.avail_in == 0)
                break;
            strm.next_in = in;

            do {
                strm.avail_out = CHUNK;
                strm.next_out = out;
                ret = inflate(&strm, Z_NO_FLUSH);
                assert(ret != Z_STREAM_ERROR);

                switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&strm);
                    return ret;
                }

                have = CHUNK - strm.avail_out;
                if (fwrite(out, 1, have, stdout) != have || ferror(stdout)) {
                    (void)inflateEnd(&strm);
                    return Z_ERRNO;
                }
            } while (strm.avail_out == 0);
        } while (ret != Z_STREAM_END);

        (void)inflateEnd(&strm);
        ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
        if (ret == Z_OK)
        printf("%d\n", ret);

        fclose(file);
    } else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }
    
    return 0;
}