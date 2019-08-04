#ifndef CPM_H_
#define CPM_H_

#include <stdio.h>

#include "types.h"
#include "cpcemu.h"

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

#define CPM_NO_FILE             0xE5
#define CPM_SYSTEM_DISK         0x41
#define CPM_DATA_DISK           0xC1
#define CPM_IBM_DISK            0x01

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

int cpm_find_empty_diren_index(FILE *fp);
void cpm_write_diren(FILE *fp, struct cpm_diren_s *dir, int diren_index);
void cpm_insert(FILE *fp, const char *file_name, u16 entry_addr, u16 exec_addr, int amsdos);
int cpm_del(FILE *fp, const char *file_name);
void cpm_dir(FILE *fp);
void cpm_info(FILE *fp, const char *file_name);
void cpm_dump(FILE *fp, const char *file_name, int to_file, int text);
void cpm_new(FILE *fp);
void init_disk_params(FILE *fp);
void denormalize_filename(const char *full_file_name, struct cpm_diren_s *dest);

#endif
