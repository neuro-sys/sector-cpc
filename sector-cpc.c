/*
                              General Information

  The file structure is composed of several "layers", so to speak. The outermost
  layer is the CPCEMU disk image format. CPCEMU is an Amstrad CPC emulator from
  early 1990s, and specifies a disk format that emulates the interface of a
  floppy drive such as NEC765 used in Amstrad CPC machines. Incidentally, it has
  become a standard for emulators for the purpose of representing disk images on
  other platforms. It's specification is defined in detail in the following
  link:
  http://www.benchmarko.de/cpcemu/cpcdoc/chapter/cpcdoc7_e.html#I_FILE_STRUCTURE
  It contains information about the disk needed to emulate a floppy disk
  controller such number of tracks, sectors.

  Inside CPCEMU disk image, there is the actual data that is stored in the disk
  as a long byte stream segmented into sectors and tracks. The first layer here
  is the CP/M 2.2 disk file system. It is defined in Digital Research CP/M
  Operating System Manual, and also explained in detail in Programmer's CPM
  Handbook by Andy Johnson-Laird. CP/M defines where the file entries are
  stored, and in what format. It divides the data into blocks, in CPC's case
  1024 bytes in length, and treats a segment as 128 bytes. However these values
  are not standard, and are defined as part of the BIOS in Directory Parameter
  Block. Typically in the first 2 block, Directory Entry list is stored. It
  lists information such as file name, file extension, user number, attributes,
  file size in blocks, and Allocation table which points to the blocks where the
  respective file is stored.

  Amstrad CPC has a Disk Operation System in a ROM chip, called AMSDOS, and
  specifies extra information for the filesystem inside CP/M layer. It is
  described in the book SOFT158A DDI-1 Firmware by Locomotive Software in
  chapter 3. It's primary purpose seems to be providing compatibility with the
  casette interface, and defines information such as user name, file name, file
  type (binary, basic file, ascii), file checksum, data length, etc.

                             CPCEMU Data Structure

  - It is written and read from the .dsk file.

                            +---------------------+
                            | CPCEMU Disk Info    | <- cpcemu_disk_info_s
                            +---------------------+
                            | CPCEMU Track 1 Info | <- cpcemu_track_info_s
                            +---------------------+
                            | Segment 1           | <- u8 *buffer
                            +---------------------+
                            | Segment 2           |
                            +---------------------+
                            | ...                 |
                            +---------------------+
                            | Segment 9           |
                            +---------------------+
                            | CPCEMU Track 2 Info |
                            +---------------------+
                            | ...                 |
                            +---------------------+

                               CPM Data Structure

  - It is written and read from CPCEMU segments, and consists of a number of
    Directory Entries, and immediately followed Data Blocks. Their size are
    defined by DPB (Disk Parameter Block) in BIOS, DRM and BSH respectively.

                            +--------------------+
                            | Directory Entry 1  | <- cpm_diren_s
                            +--------------------+
                            | Directory Entry 2  |
                            +--------------------+
                            | ...                |
                            +--------------------+
                            | Directory Entry 64 |
                            +--------------------+
                            | Data Block  1      | <- u8 *buffer
                            +--------------------+
                            | Data Block  2      |
                            +--------------------+
                            | ...                |
                            +--------------------+

                             AMSDOS Data Structure

  - It is a 128 byte header written and read from CP/M Data Blocks.

                                 Code Structure

  Every layer gets its own namespace. These layers are as mentioned before:

    - CPCEMU: Low level disk access such as sector read, and write
      functions. Prefixed with `cpcemu_`.

    - CPM: CP/M file system structures, such as Directory Entry Block, and file
      records. Prefixed with `cpm_`.

    - AMSDOS: Amstrad CPC specific file system information similar to CPM. This
      is not required when disks are accessed through CP/M. Prefixed with
      `amsdos_`.

                               Improvement Notes

  Although the primary purpose of this code is to emulate Amstrad CPC disk
  images, aim for trying to keep it as parametrized and modular as possible,
  potentially making it easy to support other disk image formats, and CP/M
  versions on their own.

                                   Resources

   - http://www.benchmarko.de/cpcemu/cpcdoc/chapter/cpcdoc7_e.html
   - https://www.seasip.info/Cpm/amsform.html
   - https://www.seasip.info/Cpm/format22.html
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "platformdef.h"
#include "types.h"
#include "cpm.h"
#include "cpcemu.h"

#define VERSION "0.1.0"

void print_usage_and_exit()
{
    printf("Arguments:\n");
    printf("  --file filename.dsk <command>\n");
    printf("  --new  filename.dsk\n");
    printf("  --no-amsdos                         Do not add amsdos header.\n");
    printf("  --text                              Treat file as text, and SUB byte as EOF. [0]\n");
    printf("Options:\n");
    printf("  <command>:\n");
    printf("    dir                               Lists the contents of the disk.\n");
    printf("    dump <file_name>                  Hexdump the contents of file to standard output. [1]\n");
    printf("    extract <file_name>               Extract the contents of file into host disk.\n");
    printf("    insert <file_name> [<entry_addr>, <exec_addr>]\n"
           "                                      Insert the file in host system into disk.\n");
    printf("    del <file_name>                   Delete the file from disk.\n");
    printf("\n");
    printf("Notes:\n");
    printf(" - [0] In CP/M 2.2 there is no way to distinguish if a file is text or binary. When\n"
           "    extracting file segments of every 128 bytes, an ASCII file past SUB byte is garbage\n"
           "    as it signifies end of file. Use this flag when extracing text files\n");
    printf("\n");
    printf(" - [1] <entry_addr> and <exec_addr> are in base 16, non-numeric characters will be ignored.\n"
           "    E.g. 0x8000, or &8000 and 8000h is valid.\n");
    printf("\n");
    printf("sector-cpc " VERSION " 2019\n");
    exit(0);
}

struct getopts_s {
    struct {
        char *file_name;
        int valid;

        struct {
            char *file_name;
            int valid;
        } extract;

        struct {
            char *file_name;
            int valid;
        } dump;

        struct {
            int valid;
        } dir;

        struct {
            char *file_name;
            u16 entry_addr;
            u16 exec_addr;

            int valid;
        } insert;

        struct {
            char *file_name;
            int valid;
        } del;
    } file;

    struct {
        char *file_name;
        int valid;
    } new;

    struct {
        int valid;
    } no_amsdos;

    struct {
        int valid;
    } text;

    struct {
        int valid;
    } version;
};

void getopts(struct getopts_s *opts, int argc, char *argv[])
{
    int i;

    assert(opts);

    memset(opts, 0, sizeof(struct getopts_s));

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 == argc) {
                print_usage_and_exit();
            }

            opts->file.valid = 1;
            opts->file.file_name = argv[i + 1];
        }

        if (strcmp(argv[i], "--new") == 0) {
            if (i + 1 == argc) {
                print_usage_and_exit();
            }

            opts->new.valid = 1;
            opts->new.file_name = argv[i + 1];
        }

        if (strcmp(argv[i], "--no-amsdos") == 0) {
            opts->no_amsdos.valid = 1;
        }

        if (strcmp(argv[i], "--text") == 0) {
            opts->text.valid = 1;
        }

        if (opts->file.valid) {
            if (strcmp(argv[i], "dir") == 0) {
                opts->file.dir.valid = 1;
            }

            if (strcmp(argv[i], "dump") == 0) {
                if (i + 1 == argc) {
                    print_usage_and_exit();
                }

                opts->file.dump.valid = 1;
                opts->file.dump.file_name = argv[i + 1];
            }

            if (strcmp(argv[i], "extract") == 0) {
                if (i + 1 == argc) {
                    print_usage_and_exit();
                }

                opts->file.extract.valid = 1;
                opts->file.extract.file_name = argv[i + 1];
            }

            if (strcmp(argv[i], "insert") == 0) {
                if (i + 1 == argc) {
                    print_usage_and_exit();
                }

                opts->file.insert.valid = 1;
                opts->file.insert.file_name = argv[i + 1];

                if (i + 2 < argc) {
                    opts->file.insert.entry_addr = strtol(argv[i + 2], NULL, 16);
                }

                if (i + 3 < argc) {
                    opts->file.insert.exec_addr = strtol(argv[i + 3], NULL, 16);
                }
            }

            if (strcmp(argv[i], "del") == 0) {
                if (i + 1 == argc) {
                    print_usage_and_exit();
                }

                opts->file.del.valid = 1;
                opts->file.del.file_name = argv[i + 1];
            }
        }
    }

    if (!opts->file.valid && !opts->new.valid) {
        print_usage_and_exit();
    }

    if (opts->file.valid
        && !opts->file.dump.valid
        && !opts->file.dir.valid
        && !opts->file.extract.valid
        && !opts->file.insert.valid
        && !opts->file.del.valid) {
        print_usage_and_exit();
    }
}

int main(int argc, char *argv[])
{
    struct getopts_s opts;

    getopts(&opts, argc, argv);

    if (opts.new.valid) {
        FILE *fp;

        fp = fopen(opts.new.file_name, "wb+");
        if (!fp) {
            fprintf(stderr, "Failed to open file %s for writing.\n", opts.new.file_name);
            exit(1);
        }

        cpm_new(fp, opts.new.file_name);

        fclose(fp);
    }

    if (opts.file.valid) {
        FILE *fp;

        fp = fopen(opts.file.file_name, "rb+");
        if (!fp) {
            fprintf(stderr, "Failed to open file %s for reading.\n", opts.file.file_name);
            exit(1);
        }

        init_disk_params(fp);

        init_sector_skew_table(fp);

        if (opts.file.dir.valid) {
            cpm_dir(fp);
        }

        if (opts.file.dump.valid) {
            cpm_dump(fp, opts.file.dump.file_name, 0, 0);
        }

        if (opts.file.extract.valid) {
            cpm_dump(fp, opts.file.extract.file_name, 1, opts.text.valid);
        }

        if (opts.file.insert.valid) {
            cpm_insert(fp, opts.file.insert.file_name, opts.file.insert.entry_addr,
                       opts.file.insert.exec_addr, !opts.no_amsdos.valid);
        }

        if (opts.file.del.valid) {
            cpm_del(fp, opts.file.del.file_name);
        }

        fclose(fp);
    }

    return 0;
}
