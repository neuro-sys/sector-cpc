#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>

#include "platformdef.h"
#include "types.h"
#include "cpm.h"
#include "cpcemu.h"

const char *TEST_FILE = "TEST.BIN";
const char *TEST_COPY_FILE = "test-orig.bin";
const char *TEST_DISK = "test.dsk";

#define ONE_TRACK_SIZE_BYTES 16384
const int TEST_FILE_SIZE_BYTES = ONE_TRACK_SIZE_BYTES + 1;

int main(int argc, char *argv[])
{
    int i;
    FILE *image;
    FILE *test_file;
    FILE *test_file2;

    image = fopen(TEST_DISK, "wb+");
    assert(image);
    cpm_new(image);
    fclose(image);

    test_file = fopen(TEST_FILE, "wb+");
    assert(test_file);

    srand(time(NULL));
    for (i = 0; i < TEST_FILE_SIZE_BYTES; i++) {
        char c = rand() % 255;
        fwrite(&c, 1, 1, test_file);
    }
    fclose(test_file);

    image = fopen(TEST_DISK, "rb+");
    cpm_init(image);

    cpm_insert(image, (char *) TEST_FILE, 0, 0, 0);
    fclose(image);

    remove(TEST_COPY_FILE);
    if (rename(TEST_FILE, TEST_COPY_FILE)) {
        fprintf(stderr, "Failed to rename file from %s to %s.", TEST_FILE, TEST_COPY_FILE);
        exit(1);
    }

    image = fopen(TEST_DISK, "rb+");
    cpm_dump(image, (char *) TEST_FILE, 1, 0);
    fclose(image);

    /* Compare two files byte by byte */
    test_file = fopen(TEST_FILE, "rb");
    test_file2 = fopen(TEST_COPY_FILE, "rb");

    for (i = 0; i < TEST_FILE_SIZE_BYTES; i++) {
        int c1;
        int c2;

        c1 = fgetc(test_file);
        c2 = fgetc(test_file2);

        if (c1 != c2) {
            fprintf(stderr, "Found byte mismatch at offset %d.\n", i);
            exit(1);
        }
    }
    printf("Test passed, files are identical.\n");

    remove(TEST_FILE);
    remove(TEST_COPY_FILE);
    remove(TEST_DISK);

    return 0;
}
