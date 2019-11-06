#ifndef AMSDOS_H_
#define AMSDOS_H_

#include "types.h"

#include <stdio.h>

#pragma pack(push)
#pragma pack(1)
struct amsdos_header_s {
    u8 user_number;
    u8 filename[8];
    u8 extension[3];
    u8 _unused1[4];
    u16 _unused2; /* block number and last block */
    u8 filetype; /* 0: basic, 1: protected, 2: binary */
    u16 data_length; /* ?? ignored */
    u16 data_location; /* loading address */
    u8 first_block; /* #FF? */
    u16 logical_length;
    u16 entry_address; /* execution address */
    u8 _unused3[36];
    u8 _unused_file_length[3]; /* 24-bit file length */
    u16 check_sum;
    u8 _unused4[60];
};
#pragma pack(pop)

void amsdos_new(FILE *fp, struct amsdos_header_s *dest, const char *file_name, u16 entry_addr, u16 exec_addr);
int amsdos_header_exists(struct amsdos_header_s *header);
void amsdos_print_header(struct amsdos_header_s *header);


#endif
