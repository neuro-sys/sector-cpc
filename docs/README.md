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
   MOVCPM.COM	 10K
      PIP.COM	  8K
   SUBMIT.COM	  2K
     XSUB.COM	  1K
       ED.COM	  7K
      ASM.COM	  9K
      DDT.COM	  5K
     LOAD.COM	  2K
     STAT.COM	  6K
     DUMP.COM	  1K
```

Hex-dump the contents of file:

```
./sector-cpc --file test.dsk dump test.txt
0800: 00 54 45 53 54 20 20 20 20 54 58 54 00 00 00 00 .TEST    TXT....
080f: 00 00 02 00 00 e5 e5 00 07 00 00 00 00 00 00 00 ................
081f: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
082f: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
083f: 07 00 00 9a 04 00 00 00 00 00 00 00 00 00 00 00 ................
084f: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
085f: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
086f: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
0880: 46 4f 4f 42 41 52 0a e5 e5 e5 e5 e5 e5 e5 e5 e5 FOOBAR..........
088f: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
089f: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
08af: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
08bf: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
08cf: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
08df: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
08ef: e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 e5 ................
```

Extract a file from the disk image:

```
./sector-cpc --file test.dsk extract test.txt
Extract file test.txt.
```
