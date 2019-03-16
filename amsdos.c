#include "amsdos.h"

#include <string.h>
#include <assert.h>

#include "cpm.h"
#include "platformdef.h"

static
int header_checksum(struct amsdos_header_s *header)
{
    int i;
    int sum;

    sum = 0;

    for (i = 0; i < 67; i++) {
        u8 *data = (u8 *) header;
        sum += data[i];
    }

    return sum;
}

void amsdos_new(FILE *fp, struct amsdos_header_s *dest, char *file_name, u16 entry_addr, u16 exec_addr)
{
    int data_length;
    struct cpm_diren_s filename_diren;
    int file_type;
    int data_location;

    assert(fp);
    assert(dest);
    assert(file_name);

    memset(dest, 0, sizeof(*dest));

    fseek(fp, 0, SEEK_END);
    data_length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    denormalize_filename(file_name, &filename_diren);

    file_type = strnicmp(filename_diren.ext, "bas", 3) == 0 ? 0 : 2;

    if (file_type == 0) {
        data_location = 0x170; /* BASIC start address */
    }

    if (entry_addr) {
        data_location = entry_addr;
    }

    dest->user_number = 0;
    memcpy(dest->filename, filename_diren.file_name, sizeof(dest->filename));
    memcpy(dest->extension, filename_diren.ext, sizeof(dest->extension));
    dest->filetype = file_type;
    /* dest->data_length = file_length + 128; */
    dest->data_location = data_location;
    dest->first_block = 0;
    dest->logical_length = data_length;
    dest->entry_address = exec_addr;
    memcpy(&dest->_unused_file_length, &data_length, 2);
    dest->check_sum = header_checksum(dest);
}

int amsdos_header_exists(struct amsdos_header_s *header)
{
    assert(header);

    return header_checksum(header) == header->check_sum;
}