# SECTOR-CPC

![](face.png)

SECTOR-CPC is a command line utility to create and edit DSK images for Amstrad CPC 464/644/6128 family of computers.


## Usage

```
Arguments:
  --file filename.dsk <command>
  --new  filename.dsk
  --no-amsdos                         Do not add amsdos header.
Options:
  <command>:
    dir                               Lists the contents of the disk.
    dump <file_name>                  Hexdump the contents of file to standard output.
    extract <file_name>               Extract the contents of file into host disk.
    insert <file_name> [<entry_addr>, <exec_addr>]
                                      Insert the file in host system into disk.
    del <file_name>                   Delete the file from disk.

Notes:
  <entry_addr> and <exec_addr> are in base 16, non-numeric characters will be ignored.
    E.g. 0x8000, or &8000 and 8000h is valid.

```
