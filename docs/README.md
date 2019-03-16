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

## Build

You can install CMake with a any C89/90 compliant C compiler.

```
mkdir build
cd build
cmake ..
make
```

Or you can simply do:

```
cc *.c -osector-cpc
```

## Examples

Create a new DSK image:

```
./sector-cpc --new test.dsk
```

Insert a file into the image:

```
./sector-cpc --file test.dsk insert test.txt
```

For a binary file with `entry` and `execution` addresses:

```
./sector-cpc --file test.dsk insert code.bin 8000,8000
```

List files in the image:

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

Hex-dump the contents of file:

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

Extract a file from the disk image:

```
./sector-cpc --file test.dsk extract test.txt
Extract file test.txt.
```
