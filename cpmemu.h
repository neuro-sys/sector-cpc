#ifndef __CPM_EMU_H
#define __CPM_EMU_H

#include <stdio.h>
#include "types.h"

#define CPCEMU_INFO_OFFSET    0x100
#define CPCEMU_TRACK_OFFSET   0x100

#define NUM_TRACK           40
#define NUM_SECTOR          9
#define SIZ_SECTOR          512
#define SIZ_TRACK           (CPCEMU_TRACK_OFFSET + NUM_SECTOR * SIZ_SECTOR)
#define SIZ_TOTAL           (NUM_TRACK * SIZ_TRACK)

extern char *CPCEMU_HEADER_STD;
extern char *CPCEMU_HEADER_EX;
extern char *CPCEMU_CREATOR;
extern char *CPCEMU_TRACK_HEADER;

extern int NUM_DIR_SECTORS;
extern int NUM_FILE_PER_SECTOR;


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

#pragma pack(pop)

u8 sector_skew_table[NUM_SECTOR];

void read_disc_info(FILE *fp, struct cpcemu_disc_info_s *info);
void write_disc_info(FILE *fp, struct cpcemu_disc_info_s *info);
void read_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info);
void write_track_info(FILE *fp, u8 track, struct cpcemu_track_info_s *track_info);
int check_disk_type(FILE *fp, u8 sector_id);
int check_extended(FILE *fp);
int get_logical_sector(FILE *fp, u8 sector);
void read_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR]);
void write_logical_sector(FILE *fp, u8 track, u8 sector, u8 buffer[SIZ_SECTOR]);

#endif
