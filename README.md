# SSTFLASH
DOS command line tool to program SST89SF0x0 (SST89SF010A,
SST89SF020A, SST89SF040) flash ROMs.

To rebuild the executable, open the PRJ file in Turbo C++ 3.0
and hit F9. The source is in C and can likely be compiled in
other DOS 16 bit compilers with little to no modification.

    SSTFLASH Version 0.9b1 - Programs SST39SF0x0 Flash ROMs
    Copyright (C) 2021 Titanium Studios Pty Ltd

    Usage: SSTFLASH <memory address> <ROM image file>
     e.g.: SSTFLASH C800 ABIOS.BIN
