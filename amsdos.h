#ifndef __AMSDOS_H_
#define __AMSDOS_H_

#include "types.h"

#pragma pack(push)
#pragma pack(1)
struct amsdos_header_s {
    u8 user_number;
    u8 filename[7];
    u8 extension[3];
    u8 _unused1[4];
    u16 _unused2;
    u8 filetype; /* 0: basic, 1: protected, 2: binary */
    u16 data_length; /* ?? ignored */
    u16 data_location; /* loading address */
    u8 first_block;
    u16 logical_length;
    u16 entry_address; /* execution address */
    u8 _unused3[35];
    u8 _unused_file_length[3]; /* 24-bit file length */
    u16 check_sum;
    u8 _unused4[58];
};
#pragma pack(pop)

void amsdos_new(struct amsdos_header_s *dest, char *file_name, char *extension,
                u8 filetype, u16 data_location, u16 file_length);

#endif
