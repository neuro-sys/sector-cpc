# sector-cpc

![](face.png)

sector-cpc is a command line utility to create and edit DSK images for Amstrad CPC 464/664/6128 and Plus family of computers.

## Usage

```
Arguments:
  --file filename.dsk <command>
  --no-amsdos                         Do not add AMSDOS header.
  --text                              Treat file as text, and SUB byte as EOF marker. [0]
Options:
  <command>:
    new                               Create a new empty disk image.
    dir                               Lists contents of disk image.
    dump <file_name>                  Hexdump contents of file to standard output. [1]
    extract <file_name>               Extract contents of file into host disk.
    insert <file_name> [<entry_addr>, <exec_addr>]
                                      Insert file on host system into disk.
    del <file_name>                   Delete file from disk.
    info <file_name> [--tracks]       Print info about file in disk.

Notes:
 - [0] In CP/M 2.2 there is no way to distinguish if a file is text or binary. When
    extracting file records of every 128 bytes, an ASCII file past SUB byte is garbage
    as it signifies end of file. Use this flag when extracing text files.

 - [1] <entry_addr> and <exec_addr> are in base 16, non-numeric characters will be ignored.
    Also give space between the two.    E.g. 0x8000, or &8000 and 8000h are valid.

```

## Build

You can install CMake and any C89/90 compliant C compiler.

```
mkdir build
cd build
cmake ..
make
```

## Examples

Create a new disk image:

```
./sector-cpc --file test.dsk new
```

Insert a file into disk image:

```
./sector-cpc --file test.dsk insert test.txt
```

For a binary file with `entry` and `execution` addresses:

```
./sector-cpc --file test.dsk insert code.bin 8000,8000
```

List files in disk image:

```
./sector-cpc --file test.dsk dir
   MAXAM0.BIN	  7K	system	read-only
    MAXAM.BAS	  2K		read-only
  MAXUSER.BAK	  1K		
    MAXUSER. 	  2K		read-only
   MAXAM1.BIN	 16K	system	read-only
   MAXAM2.BIN	  7K	system	read-only
   MAXAM3.BIN	 12K	system	read-only
        DISC.	  1K		
  MAXUSER.BAS	  1K		
     LIB1.ASM	  1K		
     TEST.BAK	  1K		
     TEST.ASM	  1K		
     LIB1.BAK	  1K		
```

Hexdump contents of file:

```
./sector-cpc --file test.dsk dump test.txt
0c00: 00 54 45 53 54 20 20 20 20 54 58 54 00 00 00 00 .TEST    TXT....
0c10: 00 00 02 00 00 00 00 00 07 00 00 00 00 00 00 00 ................
0c20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0c30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0c40: 07 00 00 d0 02 00 00 00 00 00 00 00 00 00 00 00 ................
0c50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0c60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0c70: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0c80: 46 4f 4f 42 41 52 0a e5 e5 e5 e5 e5 e5 e5 e5 e5 FOOBAR..........
0c90: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0ca0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0cb0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0cc0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0cd0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0ce0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
0cf0: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
```

Extract a file from disk image:

```
./sector-cpc --file test.dsk extract test.txt
Extracted file test.txt.
```

Create a disk, insert an executable binary at load address 0x8000 and execute address 0x8000

```
./sector-cpc --new test.dsk --file test.dsk insert copper.bin 8000 8000
```

Print info about file in disk image:

```
Directory Entry: 00
-------------------
 U     FILE_NAME EX S1 S2  RC
00      TEST.BIN 00 00 00 128

Allocation blocks
-----------------
02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17

Track, Sector pairs
-------------------
0x00, 0xc9
0x00, 0xc2
0x00, 0xc4
0x00, 0xc6
0x00, 0xc8
0x01, 0xc1
0x01, 0xc3
0x01, 0xc5
0x01, 0xc7
0x01, 0xc9
0x01, 0xc2
0x01, 0xc4
0x01, 0xc6
0x01, 0xc8
0x02, 0xc1
0x02, 0xc3
0x02, 0xc5
0x02, 0xc7
0x02, 0xc9
0x02, 0xc2
0x02, 0xc4
0x02, 0xc6
0x02, 0xc8
0x03, 0xc1
0x03, 0xc3
0x03, 0xc5
0x03, 0xc7
0x03, 0xc9
0x03, 0xc2
0x03, 0xc4
0x03, 0xc6
0x03, 0xc8

Directory Entry: 01
-------------------
 U     FILE_NAME EX S1 S2  RC
00      TEST.BIN 01 00 00 001

Allocation blocks
-----------------
18 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

Track, Sector pairs
-------------------
0x04, 0xc1
```
