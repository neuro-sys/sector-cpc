#include "cpm.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "platformdef.h"
#include "cpcemu.h"
#include "amsdos.h"

u8 allocation_table[(NUM_TRACK * NUM_SECTOR * SIZ_SECTOR) / 1024]; /* ?? Make it dynamic */

static struct DPB_s DPB_CPC_system = { 0x24, 3, 7, 0, 0x0AA, 0x3F, 0x0C0, 0, 0x10, 2, 2, 3 };
static struct DPB_s DPB_CPC_data   = { 0x24, 3, 7, 0, 0x0B3, 0x3F, 0x0C0, 0, 0x10, 0, 2, 3 };
static struct DPB_s *DPB;

static int g_num_diren = 32;
static int g_record_size = 128;

int g_base_track;
int g_block_size;
int g_num_sector_per_block;
int g_diren_table_index;
int g_num_record_per_sector;
int g_num_record_per_block;
int g_num_sector_in_diren_table;
int g_num_file_per_sector;


static
void hex_dump(u8 *buf, int offset, int len)
{
#define STRIDE 16
    int i;

    printf("%.4x: ", offset);

    for (i = 0; i < len; i++) {
        printf("%.2x ", (unsigned int) buf[i]);

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
                printf("%.4x: ", offset + i + 1);
            }
        }
    }
#undef STRIDE
}

static
void normalize_filename(char *full_file_name, struct cpm_diren_s *dir)
{
    char file_name[9];
    char ext[4];
    unsigned int k, j;

    file_name[8] = 0;
    ext[3]       = 0;

    memcpy(file_name, dir->file_name, sizeof(dir->file_name));
    memcpy(ext, dir->ext, sizeof(dir->ext));

    for (j = 0, k = 0; k < sizeof(file_name); k++) {
        int c = file_name[k];

        if (isprint(c) && !isspace(c)) {
            full_file_name[j++] = strchr("<>:\"/\\|?*",c) ? '_' : c & 0x7f;
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
void denormalize_filename(const char *full_file_name, struct cpm_diren_s *dest)
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
    if (!token) {
        fprintf(stderr, "File name has no extension.\n");
        exit(1);
    }

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

static
int find_dir_entry(FILE *fp, char *file_name, struct cpm_diren_s *dest, u8 extent)
{
    int i;

    assert(dest);

    memset(dest, 0, sizeof(struct cpm_diren_s));

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir;
            char full_file_name[13];
            int file_found;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            file_found = dir.user_number != CPM_NO_FILE
                && stricmp(full_file_name, file_name) == 0;

            if (file_found && dir.EX == extent) {
                memcpy(dest, &dir, sizeof(struct cpm_diren_s));

                return 1;
            }
        }
    }

    return 0;
}

static
void init_alloc_table(FILE *fp)
{
    int i;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            int k;
            struct cpm_diren_s dir;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            if (dir.user_number == CPM_NO_FILE) {
                continue;
            }

            for (k = 0; k < 16; k++) {
                if (dir.AL[k]) {
                    allocation_table[dir.AL[k]] = 1;
                }
            }
        }
    }
}

static
void init_disk_info(struct cpcemu_disc_info_s *dest,
                    const char *header,
                    const char *creator,
                    u8 num_tracks,
                    u8 num_heads,
                    u16 track_size)
{
    assert(dest);
    assert(header);
    assert(creator);

    memset(dest, 0, sizeof(struct cpcemu_disc_info_s));
    snprintf(dest->header, sizeof(dest->header), "%s", header);
    snprintf(dest->creator, sizeof(dest->creator), "%s", creator);
    dest->num_tracks = num_tracks;
    dest->num_heads = num_heads;
    dest->track_size = track_size;

    memset(dest->track_size_table, track_size >> 8, sizeof(dest->track_size_table));
}


static
int get_free_alloc_index(int base_index)
{
    unsigned int i;

    for (i = base_index; i < sizeof(allocation_table); i++) {
        if (!allocation_table[i]) {
            allocation_table[i] = 1;
            return i;
        }
    }

    return -1;
}

static
void init_track_info(struct cpcemu_track_info_s *dest,
                     const char *header,
                     u8 track_num,
                     u8 head_num)
{
    int i;

    assert(dest);
    assert(header);

    memset(dest, 0, sizeof(struct cpcemu_track_info_s));
    snprintf(dest->header, sizeof(dest->header), "%s", header);
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
        sector->sector_id = CPM_DATA_DISK + g_sector_skew_table[i];
        sector->sector_size = 2; /* ?? Make it dynamic */
        /* sector->FDC_status_reg1; /\* ?? *\/ */
        /* sector->FDC_status_reg2; /\* ?? *\/ */
        sector->_notused[1] = 2; /* ?? */
    }
}

static
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

        g_sector_skew_table[logical_sector] = i;
    }
}



int cpm_find_empty_diren_index(FILE *fp)
{
    int i;
    int counter;

    counter    = 0;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir;

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            if (dir.user_number == CPM_NO_FILE) {
                return counter;
            }

            counter += 1;
        }
    }

    return -1;
}

void cpm_write_diren(FILE *fp, struct cpm_diren_s *dir, int diren_index)
{
    int diren_sector;
    int diren_sector_offset;
    u8 buffer[SIZ_SECTOR];

    diren_sector        = (diren_index * g_num_diren) / SIZ_SECTOR;
    diren_sector_offset = (diren_index * g_num_diren) % SIZ_SECTOR;

    read_logical_sector(fp, g_base_track, diren_sector, buffer);

    memcpy(buffer + diren_sector_offset, dir, sizeof(*dir));
    write_logical_sector(fp, g_base_track, diren_sector, buffer);
}

int cpm_del(FILE *fp, const char *file_name)
{
    int i;
    int file_deleted;

    assert(fp);
    assert(file_name);

    file_deleted = 0;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int file_found;
        int j;

        file_found = 0;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir;
            char full_file_name[13];

            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            if (stricmp(full_file_name, file_name) != 0) {
                continue;
            }

            file_found = 1;
            file_deleted = 1;

            dir.user_number = CPM_NO_FILE;
            memcpy(buffer + j * sizeof(dir), &dir, sizeof(dir));
        }

        if (file_found) {
            write_logical_sector(fp, g_base_track, i, buffer);
        }
    }

    init_sector_skew_table(fp);

    return file_deleted;
}

static
void convert_AL_to_track_sector(u8 AL, int *track, int *sector)
{
    int sector_offset;

    sector_offset = (AL * (g_record_size << DPB->bsh)) / SIZ_SECTOR;
    *track        = g_base_track + sector_offset / NUM_SECTOR;
    *sector       = sector_offset % NUM_SECTOR;
}

static
void add_offset_to_track_sector(int *track, int *sector, int offset)
{
    int new_track;
    int new_sector;

    assert(track);
    assert(sector);

    new_track = *track + (*sector + offset) / NUM_SECTOR;
    new_sector = (*sector + offset) % NUM_SECTOR;

    *track = new_track;
    *sector = new_sector;
}

/* TODO: Revise this */
void cpm_insert(FILE *fp, const char *file_name, u16 entry_addr, u16 exec_addr, int amsdos)
{
    FILE *to_read;
    int new_diren_index;
    int cur_extent;
    struct amsdos_header_s amsdos_header;
    int amsdos_header_written;
    long file_size;

    to_read = fopen(file_name, "rb");
    if (!to_read) {
        fprintf(stderr, "Failed to open file %s for reading.\n", file_name);
        exit(1);
    }
    cur_extent            = 0;
    amsdos_header_written = 0;

    fseek(to_read, 0, SEEK_END);
    file_size = ftell(to_read);
    fseek(to_read, 0, SEEK_SET);

    init_alloc_table(fp);

    amsdos_new(to_read, &amsdos_header, file_name, entry_addr, exec_addr);

    cpm_del(fp, file_name);

    while (1) {
        struct cpm_diren_s dir;
        unsigned dir_index;

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
        memset(&dir.AL, 0, sizeof(dir.AL));

        /* Filling Allocation Table, 16 entries, each 1024 bytes block */
        for (dir_index = 0; dir_index < sizeof(dir.AL); dir_index++) {
            int j, k;
            int free_alloc_index;
            int dest_track;
            int dest_sector;
            u8 sector_buffer[SIZ_SECTOR];

            free_alloc_index = get_free_alloc_index(g_base_track + g_diren_table_index);

            if (free_alloc_index < 0) {
                fprintf(stderr, "No space left on disk.\n");
                exit(1);
            }

            dir.AL[dir_index] = free_alloc_index;

            for (j = 0; j < g_num_sector_per_block; j++) {
                memset(sector_buffer, CPM_NO_FILE, SIZ_SECTOR);

                for (k = 0; k < g_num_record_per_sector; k++) {
                    size_t n;

                    if (amsdos && !amsdos_header_written) {
                        amsdos_header_written = 1;
                        memcpy(sector_buffer, &amsdos_header, g_record_size);
                        dir.RC += 1;
                        continue;
                    }

                    n = fread(sector_buffer + k * g_record_size, 1, g_record_size, to_read);

                    dir.RC += 1;

                    if (feof(to_read) || n == 0 || ftell(to_read) == file_size) {
                        convert_AL_to_track_sector(free_alloc_index, &dest_track, &dest_sector);
                        add_offset_to_track_sector(&dest_track, &dest_sector, j);

                        write_logical_sector(fp, dest_track, dest_sector, sector_buffer);
                        cpm_write_diren(fp, &dir, new_diren_index);

                        printf("Wrote %s into disk.\n", file_name);
                        fclose(to_read);
                        return;
                    }
                }

                convert_AL_to_track_sector(free_alloc_index, &dest_track, &dest_sector);
                add_offset_to_track_sector(&dest_track, &dest_sector, j);

                write_logical_sector(fp, dest_track, dest_sector, sector_buffer);
            }
        }

        cpm_write_diren(fp, &dir, new_diren_index);

        cur_extent += 1;
    }
}

void cpm_dir(FILE *fp)
{
    int i;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int system_file;
            int read_only;
            int extent_index;
            int sum_RC;
            int file_size;

            sum_RC       = 0;
            extent_index = 1; /* Start searching from the next extent */

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            system_file = dir.ext[1] & 0x80;
            read_only   = dir.ext[0] & 0x80;

            if (   dir.user_number == CPM_NO_FILE
                || dir.AL[0]       == 0
                || dir.EX          != 0) {
                continue;
            }

            normalize_filename(full_file_name, &dir);

            sum_RC += dir.RC;

            while (find_dir_entry(fp, full_file_name, &extent_diren, extent_index++) != 0) {
                sum_RC += extent_diren.RC;
            }

            file_size = ceil(sum_RC * g_record_size / 1024.0);

            printf("%13s\t%3dK\t%.6s\t%.9s\n", full_file_name, file_size,
                   system_file ? "system" : "",
                   read_only ? "read-only" : "");
        }
    }
}

void del_cpm_dirn_s(cpm_dirn_s *a) {
    if(a->next) del_cpm_dirn_s(a->next);
    free(a);
}

cpm_dirn_s* cpm_dir_short(FILE *fp) {
    int i;
    cpm_dirn_s* go = malloc(sizeof(cpm_dirn_s)); // Leaving the freeing to the caller!
    cpm_dirn_s *last,*cur;
    cur = go; cur->next = NULL; last = go;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int extent_index = 1; /* Start searching from the next extent */

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            if (   dir.user_number == CPM_NO_FILE
                || dir.AL[0]       == 0
                || dir.EX          != 0) {
                continue;
            }

            normalize_filename(full_file_name, &dir);

            memcpy(&cur->file_name, &full_file_name, sizeof(full_file_name));
            //printf("Indexed file %s.\n",cur->file_name);

            last = cur;
            cur->next = malloc(sizeof(cpm_dirn_s));
            cur = (cpm_dirn_s*)cur->next;
            cur->next = NULL;
        }
    }
    last->next = NULL; free(cur);
    return go;
}

static void print_tracks_sectors_info(int tracks_sectors[512][2], int tracks_sectors_c)
{
    int i;
    int tracks[40];
    int tracks_c;

    tracks_c = 0;
    memset(tracks, 0, sizeof(tracks));

    for (i = 0; i < tracks_sectors_c; i++) {
        int j;
        int track_inlist;
        int track;

        track_inlist = 0;
        track = tracks_sectors[i][0];

        for (j = 0; j < tracks_c; j++) {
            if (track == tracks[j]) {
                track_inlist = 1;
            }
        }

        if (!track_inlist) {
            tracks[tracks_c++] = track;
        }
    }

    for (i = 0; i < tracks_c; i++) {
        int j;
        int min_sector;
        int max_sector;
        int track;

        track = tracks[i];
        min_sector = INT_MAX;
        max_sector = INT_MIN;

        for (j = 0; j < tracks_sectors_c; j++) {
            int sector;

            if (tracks_sectors[j][0] != track) {
                continue;
            }

            sector = tracks_sectors[j][1];

            if (min_sector > sector) {
                min_sector = sector;
            }

            if (max_sector < sector) {
                max_sector = sector;
            }
        }

        printf("db 0x%.2x, 0x%.2x, 0x%.2x\n", track, min_sector, max_sector);
    }

    printf("db 0xff\n");
}

void cpm_info(FILE *fp, const char *file_name, int tracks_only)
{
    int i;
    int first_sector_id;
    int tracks_sectors[512][2];
    int tracks_sectors_i;
    int file_found;

    file_found = 0;
    first_sector_id = check_disk_type(fp, CPM_SYSTEM_DISK)
      ? CPM_SYSTEM_DISK
      : CPM_DATA_DISK;

    tracks_sectors_i = 0;
    memset(tracks_sectors, 0, sizeof(tracks_sectors));

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int extent_index;
            int k;

            extent_index = 1;

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            if (stricmp(full_file_name, file_name) != 0) {
                continue;
            }

            file_found = 1;

            if (!tracks_only) {
                printf("Directory Entry: %.2d\n", dir.EX);
                printf("-------------------\n");
                printf(" U     FILE_NAME EX S1 S2  RC\n");
                printf("%.2d %13s %.2d %.2d %.2d %.3d\n",
                       dir.user_number, full_file_name, dir.EX, dir.S1, dir.S2, dir.RC);
                printf("\n");
                printf("Allocation blocks\n");
                printf("-----------------\n");
                for (k = 0; k < 16; k++) {
                    printf("%.2d ", dir.AL[k]);
                }
                printf("\n");
                printf("\n");
                printf("Track, Sector pairs\n");
                printf("-------------------\n");
            }

            for (k = 0; k < 16; k++) {
                int track;
                int sector;
                int sector_id;
                int is_last;
                int is_last_and_two_sectors;

                if (!dir.AL[k]) {
                    break;
                }

                is_last = k != 15 && dir.AL[k + 1] == 0;
                is_last_and_two_sectors = is_last && (dir.RC / g_num_record_per_sector > 0);

                convert_AL_to_track_sector(dir.AL[k], &track, &sector);
                sector_id = sector + first_sector_id;

                if (!tracks_only) {
                    printf("0x%.2x, 0x%.2x\n", track, sector_id);
                }

                tracks_sectors[tracks_sectors_i][0] = track;
                tracks_sectors[tracks_sectors_i++][1] = sector_id;

                if (!is_last || is_last_and_two_sectors) {
                    add_offset_to_track_sector(&track, &sector, 1);
                    sector_id = sector + first_sector_id;
                    if (!tracks_only) {
                        printf("0x%.2x, 0x%.2x\n", track, sector_id);
                    }
                    tracks_sectors[tracks_sectors_i][0] = track;
                    tracks_sectors[tracks_sectors_i++][1] = sector_id;
                }
            }

            if (!tracks_only) {
                printf("\n");
            }
        }
    }

    if (!file_found) {
        printf("File %s not found.\n", file_name);
        exit(0);
    }

    if (!tracks_only) {
        int first_track = tracks_sectors[0][0];
        int first_sector = tracks_sectors[0][1] - first_sector_id;
        u8 buffer[SIZ_SECTOR];
        int has_amsdos_header;

        read_logical_sector(fp, first_track, first_sector, buffer);

        has_amsdos_header = amsdos_header_exists((struct amsdos_header_s *) buffer);

        if (has_amsdos_header) {
            struct amsdos_header_s amsdos_header;

            memcpy(&amsdos_header, buffer, g_record_size);

            amsdos_print_header(&amsdos_header);
        }
    }

    if (tracks_only) {
        printf("; track number, first sector, last sector for the file %s\n", file_name);
        print_tracks_sectors_info(tracks_sectors, tracks_sectors_i);
    }
}

/* Return -1 for exit early */
int cpm_dump_append_to_file(struct cpm_diren_s *dir, FILE **fp, u8 *buf, size_t len, int text)
{
    char full_file_name[13];

    assert(dir);
    assert(fp);
    assert(buf);

    normalize_filename(full_file_name, dir);

    if (!*fp) {
        *fp = fopen(full_file_name, "wb");
        if (!*fp) {
            fprintf(stderr, "Failed to open file %s for writing.\n", full_file_name);
            exit(1);
        }
    }

    /* Find index of EOF marker if text file */
#define SUB 0x1a
    if (text) {
        unsigned int i;
        int sub_found;

        sub_found = 0;

        for (i = 0; i < len; i++) {
            if (buf[i] == SUB) {
                sub_found = 1;
                break;
            }
        }

        if (sub_found) {
            fwrite(buf, 1, i, *fp);
            return - 1;
        }
#undef SUB
    }

    if (fwrite(buf, 1, len, *fp) != len) {
        fprintf(stderr, "Error writing to file.\n");
        exit(1);
    }

    return 0;
}

static
void dump_extent(FILE *fp, struct cpm_diren_s *dir, int to_file, FILE **write_file,
                    int text)
{
    unsigned k;
    int record_counter;

    record_counter = 0;

    for (k = 0; k < sizeof(dir->AL); k++) {
        int r, s;
        int is_last_AL;
        int sector;
        int track;

        is_last_AL = 0;

        if (   dir->RC        != 0x80
            && (k + 1)        != sizeof(dir->AL)
            && dir->AL[k + 1] == 0) {

            is_last_AL = 1;
        }

        if (!dir->AL[k]) {
            break;
        }

        convert_AL_to_track_sector(dir->AL[k], &track, &sector);

        for (s = 0; s < g_num_sector_per_block; s++) {
            u8 block_buffer[SIZ_SECTOR];
            int cur_sector;
            int cur_track;

            cur_sector            = (sector + s) % NUM_SECTOR;
            cur_track             = track + (sector + s) / NUM_SECTOR;

            read_logical_sector(fp, cur_track, cur_sector, block_buffer);

            for (r = 0; r < g_num_record_per_sector; r++) {
                if (to_file) {
                    int skip_early;

                    skip_early = 0;
                    if (r == 0) {
                        int skip_amsdos;
                        int has_amsdos_header;

                        has_amsdos_header = amsdos_header_exists((struct amsdos_header_s *) block_buffer);
                        skip_amsdos = dir->EX == 0 && cur_track == track && cur_sector == sector
                            && has_amsdos_header;
                        if (skip_amsdos) {
                            continue;
                        }
                    }
                    skip_early = cpm_dump_append_to_file(dir, write_file, block_buffer + r * g_record_size, g_record_size, text);

                    if (skip_early == -1) {
                        return;
                    }
                } else {
                    if (r == 0) {
                        printf("# track: %2d, sector: %2d\n", cur_track, cur_sector);
                    }
                    hex_dump(block_buffer + r * g_record_size, (dir->AL[k] * g_block_size) + s * SIZ_SECTOR + r * g_record_size, g_record_size);
                }

                if (is_last_AL && (record_counter + 1) >= dir->RC) {
                    return;
                }

                record_counter++;
            }
        }
    }
}

void cpm_dump(FILE *fp, const char *file_name, int to_file, int text)
{
    FILE *write_file;
    int i;
    int file_found;

    file_found = 0;
    write_file = 0;

    for (i = 0; i < g_num_sector_in_diren_table; i++) {
        u8 buffer[SIZ_SECTOR];
        int j;

        read_logical_sector(fp, g_base_track, i, buffer);

        for (j = 0; j < g_num_file_per_sector; j++) {
            struct cpm_diren_s dir, extent_diren;
            char full_file_name[13];
            int extent_index;

            extent_index = 1;

            memset(&extent_diren, 0, sizeof(dir));
            memset(&dir, 0, sizeof(dir));
            memcpy(&dir, buffer + j * sizeof(dir), sizeof(dir));

            normalize_filename(full_file_name, &dir);

            if (stricmp(full_file_name, file_name) != 0) {
                continue;
            }

            dump_extent(fp, &dir, to_file, &write_file, text);

            while (find_dir_entry(fp, full_file_name, &extent_diren, extent_index++) != 0) {
                dump_extent(fp, &extent_diren, to_file, &write_file, text);
            }

            if (write_file) {
                printf("Extracted file %s.\n", file_name);
                fclose(write_file);
            }
            return;
        }
    }

    if (!file_found) {
        printf("File %s not found.\n", file_name);
    }
}

void cpm_new(FILE *fp)
{
    struct cpcemu_disc_info_s disk_info;
    int track;
    int sector;
    u8 skew_table[] = { 0, 5, 1, 6, 2, 7, 3, 8, 4 }; /* ?? */

    memcpy(g_sector_skew_table, skew_table, sizeof(skew_table));

    init_disk_info(&disk_info, CPCEMU_HEADER_STD, CPCEMU_CREATOR, NUM_TRACK, 1, SIZ_TRACK);
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


void cpm_init(FILE *fp)
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

    g_base_track                 = check_disk_type(fp, CPM_SYSTEM_DISK) ? 2 : 0;
    g_block_size                 = g_record_size << DPB->bsh;
    g_num_sector_per_block       = g_block_size / SIZ_SECTOR;
    g_diren_table_index          = ((DPB->drm + 1) * g_num_diren) / g_block_size;
    g_num_record_per_sector      = SIZ_SECTOR / g_record_size;
    g_num_record_per_block       = g_block_size / g_record_size;
    g_num_sector_in_diren_table  = ((DPB->drm + 1) * g_num_diren) / SIZ_SECTOR;
    g_num_file_per_sector        = SIZ_SECTOR / g_num_diren;

#if 0
    printf("g_base_track                 = %d\n", g_base_track);
    printf("g_block_size                 = %d\n", g_block_size);
    printf("g_num_sector_per_block       = %d\n", g_num_sector_per_block);
    printf("g_diren_table_index          = %d\n", g_diren_table_index);
    printf("g_num_record_per_sector      = %d\n", g_num_record_per_sector);
    printf("g_num_record_per_block       = %d\n", g_num_record_per_block);
    printf("g_num_sector_in_diren_table  = %d\n", g_num_sector_in_diren_table);
    printf("g_num_file_per_sector        = %d\n", g_num_file_per_sector);
#endif

    init_sector_skew_table(fp);
}
