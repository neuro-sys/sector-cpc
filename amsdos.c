#include "amsdos.h"

#include <string.h>
#include <assert.h>

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

void amsdos_new(struct amsdos_header_s *dest, char *file_name, char *extension,
                u8 filetype, u16 data_location, u16 file_length)
{
    assert(dest);
    assert(file_name);
    assert(extension);

    memset(dest, 0, sizeof(*dest));

    dest->user_number = 0;
    memcpy(dest->filename, file_name, sizeof(dest->filename));
    memcpy(dest->extension, extension, sizeof(dest->extension));
    dest->filetype = filetype;
    dest->data_location = data_location;
    dest->logical_length = file_length;
    dest->entry_address = data_location;
    dest->check_sum = header_checksum(dest);
}
