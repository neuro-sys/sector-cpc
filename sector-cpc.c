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
      is not required when disks are accessed through CP/M. Prefied with
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

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <strings.h>
#define stricmp strcasecmp
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

#define CPCEMU_INFO_OFFSET    0x100
#define CPCEMU_TRACK_OFFSET   0x100

#define NUM_TRACK           40
#define NUM_SECTOR          9
#define SIZ_SECTOR          512
#define SIZ_TRACK           (CPCEMU_TRACK_OFFSET + NUM_SECTOR * SIZ_SECTOR)
#define SIZ_TOTAL           (NUM_TRACK * SIZ_TRACK)

char *CPCEMU_HEADER_STD   = "MV - CPCEMU Disk-File\r\nDisk-Info\r\n";
char *CPCEMU_HEADER_EX    = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";
char *CPCEMU_CREATOR      = "AMS-DSK\r\n";
char *CPCEMU_TRACK_HEADER = "Track-Info\r\n";

/* Disc Parameter Block, taken from https://www.seasip.info/Cpm/amsform.html */
struct DPB_s {
    u16 spt;   /* Number of 128-byte records per track              */
    u8 bsh;    /* Block shift. 3 => 1k, 4 => 2k, 5 => 4k....        */
    u8 blm;    /* Block mask. 7 => 1k, 0Fh => 2k, 1Fh => 4k...      */
    u8 exm;    /* Extent mask, see later                            */
    u16 dsm;   /* (no. of blocks on the disc)-1                     */
    u16 drm;   /* (no. of directory entries)-1                      */
    u8 al0;    /* Directory allocation bitmap, first byte           */
    u8 al1;    /* Directory allocation bitmap, second byte          */
    u16 cks;   /* Checksum vector size, 0 or 8000h for a fixed disc.*/
               /* No. directory entries/4, rounded up.              */
    u16 off;   /* Offset, number of reserved tracks                 */
    u8 psh;    /* Physical sector shift, 0 => 128-byte sectors      */
               /* 1 => 256-byte sectors  2 => 512-byte sectors...   */
    u8 phm;    /* Physical sector mask,  0 => 128-byte sectors      */
               /* 1 => 256-byte sectors, 3 => 512-byte sectors...   */
};

struct DPB_s DPB_CPC_system = { 0x24, 3, 7, 0, 0x0AA, 0x3F, 0x0C0, 0, 0x10, 2, 2, 3 };
struct DPB_s DPB_CPC_data   = { 0x24, 3, 7, 0, 0x0B3, 0x3F, 0x0C0, 0, 0x10, 0, 2, 3 };
struct DPB_s *DPB;

u8 sector_skew_table[NUM_SECTOR];
u8 allocation_table[(NUM_TRACK * NUM_SECTOR * SIZ_SECTOR) / 1024]; /* ?? Make it dynamic */

/* Tightly packed disc data structures */
#pragma pack(push)
#pragma pack(1)
struct cpcemu_disc_info_s {
    char header[34];                    /* CPCEMU_HEADER_STD or
                                           CPCEMU_HEADER_EX */
    char creator[14];                   /* Extended only */
    u8 num_tracks;                      /* 40, 42, 80 */
    u8 num_heads;                       /* 1, 2 */
    u16 track_size;                     /* Standard only */
    u8 track_size_table[NUM_TRACK * 2]; /* Extended only. High bytes
                                           of track sizes */
    u8 _unused1[204 - NUM_TRACK * 2];

    /* - For single sided formats the table contains track sizes of just one
       side, otherwise for two alternating sides.

       - A size of value 0 indicates an unformatted track.

       - Actual track data length = table value * 256.

       - Keep in mind that the image contains additional 256 bytes for each
         track info. */
};

struct cpcemu_sector_info_s {
    u8 track;
    u8 head;
    u8 sector_id;
    u8 sector_size;      /* BPS (sector ID information) */
    u8 FDC_status_reg1;  /* state 1 error code (0) */
    u8 FDC_status_reg2;  /* state 2 error code (0) */
    u8 _notused[2];      /* Extended only. sector data length in bytes (little
                            endian notation).  This allows different sector
                            sizes in a track.  It is computed as (0x0080 <<
                            real_BPS). */
};

struct cpcemu_track_info_s {
    char header[13];     /* CPCEMU_TRACK_HEADER */
    u8 _unused0[3];
    u8 track_num;        /* track number (0 to number of tracks-1) */
    u8 head_num;         /* head number (0 or 1) */
    u8 _unused1[2];

    /* Format track parameters */
    u8 sector_size;      /* BPS (bytes per sector) (2 for 0x200 bytes) */
    u8 num_sectors;      /* SPT (sectors per track) (9, at the most 18) */
    u8 GAP3_length;      /* GAP#3 format (gap for formatting; 0x4E) */
    u8 filler_byte;      /* Filling byte (filling byte for formatting; 0xE5) */
    struct cpcemu_sector_info_s sector_info_table[29];

    /* - The sector data must follow the track information block in the order of
         the sector IDs. No track or sector may be omitted.

       - With double sided formats, the tracks are alternating, e.g. track 0
         head 0, track 0 head 1, track 1 ...
*/

};

struct cpm_diren_s {
    u8 user_number;
    u8 file_name[8];
    u8 ext[3];
    u8 EX;
    u8 S1;
    u8 S2;
    u8 RC;
    u8 AL[16];
};
#pragma pack(pop)

#define CPM_NO_FILE             0xE5
#define CPM_SYSTEM_DISK         0x41
#define CPM_DATA_DISK           0xC1
#define CPM_IBM_DISK            0x01

int NUM_DIR_SECTORS;
int NUM_FILE_PER_SECTOR;

void read_disc_info(FILE *fp, struct cpcemu_disc_info_s *info)
{
    assert(fp);
    assert(info);

    fseek(fp, 0, SEEK_SET);
    fread(info, 1, sizeof(*info), fp);
}

void write_disc_info(FILE *fp, struct cpcemu_disc_info_s *info)
{
    assert(fp);
    assert(info);

    fseek(fp, 0, SEEK_SET);
    fwrite(info, 1, sizeof(*info), fp);
}

void read_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info)
{
    int offset_track;

    assert(fp);
    assert(track_info);

    offset_track = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);

    fseek(fp, offset_track, SEEK_SET);
    fread(track_info, 1, sizeof(struct cpcemu_track_info_s), fp);
}

void write_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info)
{
    int offset_track;

    assert(fp);
    assert(track_info);

    offset_track = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);

    fseek(fp, offset_track, SEEK_SET);
    fwrite(track_info, 1, sizeof(struct cpcemu_track_info_s), fp);
}

int check_disk_type(FILE *fp, u8 sector_id)
{
    struct cpcemu_track_info_s track_info;

    assert(fp);

    read_track_info(fp, 0, &track_info);

    return track_info.sector_info_table[0].sector_id == sector_id;
}

int check_extended(FILE *fp)
{
    struct cpcemu_disc_info_s disc_info;

    assert(fp);

    read_disc_info(fp, &disc_info);

    return strncmp("EXTENDED", disc_info.header, 8) == 0;
}

int get_logical_sector(FILE *fp, u8 sector)
{
    assert(fp);

    return sector_skew_table[sector];
}

#if 0
void read_physical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int offset_sector;

    offset_track  = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    offset_sector = CPCEMU_TRACK_OFFSET + sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fread(buffer, 1, SIZ_SECTOR, fp);
}
#endif

void read_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int logical_sector;
    int offset_sector;

    offset_track   = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    logical_sector = get_logical_sector(fp, sector);
    offset_sector  = CPCEMU_TRACK_OFFSET + logical_sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fread(buffer, 1, SIZ_SECTOR, fp);
}

void write_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR])
{
    int offset_track;
    int logical_sector;
    int offset_sector;

    offset_track   = CPCEMU_INFO_OFFSET + (track * SIZ_TRACK);
    logical_sector = get_logical_sector(fp, sector);
    offset_sector  = CPCEMU_TRACK_OFFSET + logical_sector * SIZ_SECTOR;

    fseek(fp, offset_track + offset_sector, SEEK_SET);
    fwrite(buffer, 1, SIZ_SECTOR, fp);
}

void normalize_filename(char *full_file_name, struct cpm_diren_s *dir)
{
    char file_name[9];
    char ext[4];
    int long_file;
    int k, j;

    file_name[8] = 0;
    ext[3]       = 0;

    memcpy(file_name, dir->file_name, sizeof(dir->file_name));
    memcpy(ext, dir->ext, sizeof(dir->ext));

    for (j = 0, k = 0; k < sizeof(file_name); k++) {
        int c = file_name[k];

        if (isprint(c) && !isspace(c)) {
            full_file_name[j++] = c & 0x7f;
        }
    }

    full_file_name[j++] = '.';

    for (k = 0; k < sizeof(ext); k++) {
        int c = ext[k];

        if (!isspace(c)) {
            full_file_name[j++] = c & 0x7f;
        }
    }
}

/* ?? Improve */
void denormalize_filename(char *full_file_name, struct cpm_diren_s *dest)
{
    char buffer[1024];
    char *token;
    int file_name_length;
    int ext_length;
    int i;

    assert(full_file_name);
    assert(dest);

    memset(dest->file_name, 0, sizeof(dest->file_name));
    memset(dest->ext, 0, sizeof(dest->ext));

    strcpy(buffer, full_file_name);

    token = strtok(buffer, ".");
    file_name_length = strlen(token);
    if (file_name_length > 8) {
        fprintf(stderr, "File name cannot be longer then 8 characters.\n");
        exit(1);
    }

    for (i = 0; i < 8; i++) {
        dest->file_name[i] = i < file_name_length ? toupper(token[i]) : ' ';
    }

    token = strtok(NULL, ".");

    if (token) {
        ext_length = strlen(token);

        if (ext_length > 3) {
            fprintf(stderr, "File extension cannot be longer then 3 characters.\n");
            exit(1);
        }

        for (i = 0; i < 3; i++) {
            dest->ext[i] = i < ext_length ? toupper(token[i]) : ' ';
        }
    }
}

void init_sector_skew_table(FILE *fp)
{
    struct cpcemu_track_info_s track_info;
    int is_system_disk;
    int i;

    assert(fp);

    read_track_info(fp, 0, &track_info);

    is_system_disk = check_disk_type(fp, CPM_SYSTEM_DISK);

    for (i = 0; i < NUM_SECTOR; i++) {
        int sector_id = track_info.sector_info_table[i].sector_id;
        int logical_sector = sector_id - (is_system_disk ? CPM_SYSTEM_DISK : CPM_DATA_DISK);

        sector_skew_table[logical_sector] = i;
    }
}

void hex_dump(u8 *buf, int offset, size_t len)
{
#define STRIDE 16
    int i;

    printf("%.4x: ", offset);

    for (i = 0; i < len; i++) {
        printf("%.2x ", buf[i]);

        if ((i + 1) % STRIDE == 0) {
            int j;

            for (j = STRIDE - 1; j >= 0; j--) {
                int c = buf[i - j];

                if (isprint(c)) {
                    putchar(c);
                } else {
                    putchar('.');
                }
            }
            putchar('\n');
            if (i + 1 != len) {
                printf("%.4x: ", offset + i);
            }
        }
    }
#undef STRIDE
}

int find_dir_entry(FILE *fp, char *file_name, struct cpm_diren_s *dest, u8 extent)
{
    int base_track;
    int i;

    assert(dest);

    memset(dest, 0, sizeof(struct cpm_diren_s));

    base_track = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;

    for (i = 0; i < NUM_DIR_SECTORS; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, base_track, i, buffer);

        for (j = 0; j < NUM_FILE_PER_SECTOR; j++) {
            struct cpm_diren_s dir;
            char full_file_name[13];
            int file_found;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            file_found = stricmp(full_file_name, file_name) == 0;

            if (file_found && dir.EX == extent) {
                memcpy(dest, &dir, sizeof(struct cpm_diren_s));

                return 1;
            }
        }
    }

    return 0;
}

void cpm_dir(FILE *fp)
{
    int base_track;
    int i;

    base_track = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;

    for (i = 0; i < NUM_DIR_SECTORS; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, base_track, i, buffer);

        for (j = 0; j < NUM_FILE_PER_SECTOR; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int system_file;
            int extent_index;
            int sum_RC;
            int file_size;
            int block_size;

            sum_RC       = 0;
            block_size   = 128 << DPB->bsh;
            extent_index = 1;                 /* Start searching from
                                                 the next extent */

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            system_file = dir.ext[1] & 0x80;

            if (   dir.user_number == CPM_NO_FILE
                || system_file
                || dir.AL[0]       == 0
                || dir.EX          != 0) {
                continue;
            }

            sum_RC += dir.RC;

            while (find_dir_entry(fp, full_file_name, &extent_diren, extent_index++) != 0) {
                sum_RC += extent_diren.RC;
            }

            file_size = sum_RC * 128 / block_size + 1;

            printf("%.8s.%.3s %3dK\n", dir.file_name, dir.ext, file_size);
        }
    }
}

void cpm_dump_append_to_file(struct cpm_diren_s *dir, FILE **fp, u8 *buf, size_t len)
{
    char full_file_name[13];

    assert(dir);
    assert(fp);
    assert(buf);

    normalize_filename(full_file_name, dir);

    if (!*fp) {
        *fp = fopen(full_file_name, "w");
        if (!*fp) {
            fprintf(stderr, "Failed to open file %s for writing.\n", full_file_name);
            exit(1);
        }
    }

    fwrite(buf, 1, len, *fp);
}

void cpm_dump_entry(FILE *fp, struct cpm_diren_s *dir, u8 base_track, int to_file, FILE **write_file)
{
    int block_size;
    int num_sector_per_block;
    int k;

    block_size           = 128 << DPB->bsh;
    num_sector_per_block = block_size / SIZ_SECTOR;

    for (k = 0; k < 16; k++) {
        int h, s;
        int is_last_AL;
        int file_num_record;
        int sector_offset;
        int sector;
        int track;
        int byte_offset;
        int record_counter;

        is_last_AL = 0;

        if (   dir->RC        != 0x80
            && (k + 1)        != sizeof(dir->AL)
            && dir->AL[k + 1] == 0) {

            is_last_AL = 1;
        }

        file_num_record = dir->RC * 128;
        byte_offset     = dir->AL[k] * block_size;
        sector_offset   = byte_offset / SIZ_SECTOR;
        track           = base_track + sector_offset / NUM_SECTOR;
        sector          = sector_offset % NUM_SECTOR;
        record_counter  = 0;

        if (!byte_offset) {
            break;
        }

        for (s = 0; s < num_sector_per_block; s++) {
            u8 block_buffer[SIZ_SECTOR];
            int cur_sector;
            int cur_track;
            int num_record_per_sector;
            int num_record_per_block;

            cur_sector            = (sector + s) % NUM_SECTOR;
            cur_track             = track + (sector + s) / NUM_SECTOR;
            num_record_per_sector = SIZ_SECTOR / 128;
            num_record_per_block  = block_size / 128;

            read_logical_sector(fp, cur_track, cur_sector, block_buffer);

            for (h = 0; h < num_record_per_sector; h++) {
                if (to_file) {
                    cpm_dump_append_to_file(dir, write_file, block_buffer + h * 128, 128);
                } else {
                    hex_dump(block_buffer + h * 128, byte_offset + s * SIZ_SECTOR + h * 128, 128);
                }

                if (is_last_AL && (record_counter + 1) >= (dir->RC % num_record_per_block)) { /* ?? */
                    return;
                }

                record_counter++;
            }
        }
    }
}

void cpm_dump(FILE *fp, char *file_name, int to_file)
{
    FILE *write_file;
    int base_track;
    int i;
    int file_found;

    file_found = 0;
    write_file = 0;
    base_track = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;

    for (i = 0; i < NUM_DIR_SECTORS; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, base_track, i, buffer);

        for (j = 0; j < NUM_FILE_PER_SECTOR; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int extent_index;
            int file_found;

            file_found   = 0;
            extent_index = 1;

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            file_found = stricmp(full_file_name, file_name) == 0;

            if (!file_found) {
                continue;
            }

            cpm_dump_entry(fp, &dir, base_track, to_file, &write_file);

            while (find_dir_entry(fp, full_file_name, &extent_diren, extent_index++) != 0) {
                cpm_dump_entry(fp, &dir, base_track, to_file, &write_file);
            }

            if (write_file) {
                printf("Extract file %s.\n", file_name);
                fclose(write_file);
            }
            return;
        }
    }

    if (!file_found) {
        printf("File %s not found.\n", file_name);
    }
}

int get_free_alloc_index(int base_index)
{
    int i;

    for (i = base_index; i < sizeof(allocation_table); i++) {
        if (!allocation_table[i]) {
            allocation_table[i] = 1;
            return i;
        }
    }

    return -1;
}

void init_alloc_table(FILE *fp)
{
    int alloc_index;
    int base_track;
    int i;

    base_track  = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;
    alloc_index = 0;

    for (i = 0; i < NUM_DIR_SECTORS; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, base_track, i, buffer);

        for (j = 0; j < NUM_FILE_PER_SECTOR; j++) {
            int k;
            struct cpm_diren_s dir;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            if (dir.user_number == CPM_NO_FILE) {
                continue;
            }

            for (k = 0; k < 16; k++) {
                allocation_table[dir.AL[k]] = 1;
            }
        }
    }
}

int cpm_find_empty_diren_index(FILE *fp)
{
    int base_track;
    int i;
    int counter;

    base_track = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;
    counter    = 0;

    for (i = 0; i < NUM_DIR_SECTORS; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, base_track, i, buffer);

        for (j = 0; j < NUM_FILE_PER_SECTOR; j++) {
            struct cpm_diren_s dir;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            counter += 1;

            if (   dir.user_number != CPM_NO_FILE
                && dir.AL[0]       != 0) {
                continue;
            }

            return counter;
        }
    }

    return -1;
}

void cpm_write_diren(FILE *fp, struct cpm_diren_s *dir, int diren_index)
{
    int diren_sector;
    int diren_sector_offset;
    u8 buffer[SIZ_SECTOR];
    int base_track;

    base_track          = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;
    diren_sector        = (diren_index * 32) / SIZ_SECTOR;
    diren_sector_offset = (diren_index * 32) % SIZ_SECTOR;

    read_logical_sector(fp, base_track, diren_sector, buffer);

    memcpy(buffer + diren_sector_offset, dir, sizeof(*dir));
    write_logical_sector(fp, base_track, diren_sector, buffer);
}

void cpm_insert(FILE *fp, char *file_name)
{
    FILE *to_read;
    int new_diren_index;
    int cur_extent;
    int block_size;
    int base_track;
    int base_alloc_index;

    to_read = fopen(file_name, "rb");
    if (!to_read) {
        fprintf(stderr, "Failed to open file %s for reading.\n", file_name);
        exit(1);
    }

    base_track       = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;
    cur_extent       = 0;
    block_size       = 128 << DPB->bsh;
    base_alloc_index = ((DPB->drm + 1) * 32) / block_size;

    init_alloc_table(fp);

    while (1) {
        int dir_AL_index;
        struct cpm_diren_s dir;

        memset(&dir, 0, sizeof(dir));

        new_diren_index = cpm_find_empty_diren_index(fp);
        if (new_diren_index < 0) {
            fprintf(stderr, "No empty slot left in directory entry table\n");
            exit(1);
        }

        denormalize_filename(file_name, &dir);

        dir.user_number = 0;
        dir.EX = cur_extent;
        dir.S1 = 0;
        dir.S2 = 0;
        dir.RC = 0;
        memset(((u8 *) &dir) + 16, 0, sizeof(dir.AL));

        dir_AL_index = 0;

        while (1) {
            int free_alloc_index;
            u8 host_file_buffer[SIZ_TRACK];
            int dest_total_sector;
            int dest_track;
            int dest_sector;
            int k;

            memset(host_file_buffer, CPM_NO_FILE, SIZ_TRACK);

            if (dir_AL_index >= 16) {
                cur_extent++;
                break;
            }

            free_alloc_index = get_free_alloc_index(base_alloc_index);

            if (free_alloc_index < 0) {
                fprintf(stderr, "No space left in disk\n");
                exit(1);
            }

            dir.AL[dir_AL_index] = free_alloc_index;

            dest_total_sector = free_alloc_index * block_size / SIZ_SECTOR;
            dest_track = base_track + dest_total_sector / NUM_SECTOR;
            dest_sector = dest_total_sector % NUM_SECTOR;

            for (k = 0; k < block_size / SIZ_SECTOR; k++) {
                int byte_read = fread(host_file_buffer, 1, SIZ_SECTOR, to_read);

                dir.RC += byte_read / 128;

                write_logical_sector(fp, dest_track, dest_sector + k, host_file_buffer);

                if (feof(to_read)) {
                    cpm_write_diren(fp, &dir, new_diren_index);
                    printf("Wrote file into disk.\n");
                    fclose(to_read);
                    return;
                }
            }

            dir_AL_index++;
        }

        cpm_write_diren(fp, &dir, new_diren_index);
    }
}

void init_disk_info(struct cpcemu_disc_info_s *dest,
                    char *header,
                    char *creator,
                    u8 num_tracks,
                    u8 num_heads,
                    u16 track_size)
{
    assert(dest);
    assert(header);
    assert(creator);

    memset(dest, 0, sizeof(struct cpcemu_disc_info_s));
    sprintf(dest->header, "%s", header);
    sprintf(dest->creator, "%s", creator);
    dest->num_tracks = num_tracks;
    dest->num_heads = num_heads;
    dest->track_size = track_size;

    memset(dest->track_size_table, track_size >> 8, sizeof(dest->track_size_table));
}

void init_track_info(struct cpcemu_track_info_s *dest,
                     char *header,
                     u8 track_num,
                     u8 head_num)
{
    struct cpcemu_sector_info_s sector_info_table[29];
    int i;

    assert(dest);
    assert(header);

    memset(dest, 0, sizeof(struct cpcemu_track_info_s));
    sprintf(dest->header, "%s", header);
    dest->track_num = track_num;
    dest->head_num = head_num;
    dest->sector_size = 2; /* ?? Make it dynamic */
    dest->num_sectors = NUM_SECTOR;
    dest->GAP3_length = 0x4E;
    dest->filler_byte = 0xE5;

    for (i = 0; i < NUM_SECTOR; i++) {
        struct cpcemu_sector_info_s *sector = &dest->sector_info_table[i];

        sector->track = track_num;
        sector->head = head_num;
        sector->sector_id = CPM_DATA_DISK + sector_skew_table[i];
        sector->sector_size = 2; /* ?? Make it dynamic */
        sector->FDC_status_reg1; /* ?? */
        sector->FDC_status_reg2; /* ?? */
        sector->_notused[1] = 2; /* ?? */
    }
}

void cpm_new(FILE *fp, char *file_name)
{
    struct cpcemu_disc_info_s disk_info;
    int track;
    int sector;
    u8 skew_table[] = { 0, 5, 1, 6, 2, 7, 3, 8, 4 }; /* ?? */

    memcpy(sector_skew_table, skew_table, sizeof(skew_table));

    init_disk_info(&disk_info, CPCEMU_HEADER_STD, CPCEMU_CREATOR, 40, 1, SIZ_TRACK);
    write_disc_info(fp, &disk_info);

    for (track = 0; track < NUM_TRACK; track++) {
        struct cpcemu_track_info_s track_info;

        init_track_info(&track_info, CPCEMU_TRACK_HEADER, track, 0);
        write_track_info(fp, track, &track_info);

        for (sector = 0; sector < NUM_SECTOR; sector++) {
            u8 buffer[SIZ_SECTOR];

            memset(buffer, CPM_NO_FILE, SIZ_SECTOR);
            write_logical_sector(fp, track, sector, buffer);
        }
    }
}

void print_usage_and_exit()
{
    printf("Arguments:\n");
    printf("  --file filename.dsk <command>\n");
    printf("  --new  filename.dsk <type>\n");
    printf("Options:\n");
    printf("  <command>:\n");
    printf("    dir                 Lists the contents of the disk.\n");
    printf("    dump <file_name>    Hexdump the contents of file to standard output.\n");
    printf("    extract <file_name> Extract the contents of file into host disk.\n");
    printf("    insert <file_name>  Insert the file in host system into disk.\n");
    printf("    del <file_name>     Delete the file from disk.\n");
    printf("  <type>:\n");
    printf("    standard            Standard AMSDOS DSK.\n");
    printf("    extended            Extended DSK.\n");

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

        struct {
            int valid;
        } standard;

        struct {
            int valid;
        } extended;
    } new;
};

void getopts(struct getopts_s *opts, int argc, char *argv[])
{
    int i;

    assert(opts);

    memset(opts, 0, sizeof(struct getopts_s));

    if (argc <= 2) {
        print_usage_and_exit();
    }

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
            }

            if (strcmp(argv[i], "del") == 0) {
                if (i + 1 == argc) {
                    print_usage_and_exit();
                }

                opts->file.del.valid = 1;
                opts->file.del.file_name = argv[i + 1];
            }
        }

        if (opts->new.valid) {
            if (strcmp(argv[i], "standard") == 0) {
                opts->new.standard.valid = 1;
            }

            if (strcmp(argv[i], "extended") == 0) {
                opts->new.extended.valid = 1;
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

    if (opts->new.valid && !opts->new.standard.valid && !opts->new.extended.valid) {
        opts->new.standard.valid = 1;
    }
}

void init_disk_params(FILE *fp)
{
    int is_system_disk;

    is_system_disk = check_disk_type(fp, CPM_SYSTEM_DISK);

    if (is_system_disk) {
        DPB = &DPB_CPC_system;
    } else if (check_disk_type(fp, CPM_DATA_DISK)) {
        DPB = &DPB_CPC_data;
    } else {
        printf("Unrecognized disk type\n");
        return;
    }

    NUM_DIR_SECTORS     = ((DPB->drm + 1) * 32) / SIZ_SECTOR;
    NUM_FILE_PER_SECTOR = SIZ_SECTOR / 32;
}

int main(int argc, char *argv[])
{
    struct getopts_s opts;

    getopts(&opts, argc, argv);

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
            cpm_dump(fp, opts.file.dump.file_name, 0);
        }

        if (opts.file.extract.valid) {
            cpm_dump(fp, opts.file.extract.file_name, 1);
        }

        if (opts.file.insert.valid) {
            cpm_insert(fp, opts.file.insert.file_name);
        }

        fclose(fp);
    }

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

    return 0;
}
