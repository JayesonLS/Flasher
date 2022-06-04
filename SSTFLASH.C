// SSTFLASH - Programs SST39SF0x0 Flash ROMs
//
// Copyright (C) 2021 Titanium Studios Pty Ltd 
// 

// This code requires far data pointers. Near code pointers are fine.
// Tested with Borland Turbo C++ using the Compact memory model.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <conio.h>

#if defined(MSDOS) || defined(_MSDOS) || defined(__MSDOS__) || defined(__TURBOC__)
#include <dos.h>
#else
#include "fakedos.h"
#endif

#define TRUE 1
#define FALSE 0

#define MAX_ROM_SIZE_K 256
#define ROM_BLOCK_SIZE__K 2
#define FLASH_BLOCK_SIZE_K 4
#define ROM_BLOCK_SIZE_ (ROM_BLOCK_SIZE__K * 1024)
#define FLASH_BLOCK_SIZE (FLASH_BLOCK_SIZE_K * 1024)
#define MAX_ROM_BLOCK_COUNT (MAX_ROM_SIZE_K / FLASH_BLOCK_SIZE_K)

static const char *PRODUCT_STRING =
    "SSTFLASH Version 0.9b2 - Programs SST39SF0x0 Flash ROMs\n"
    "Copyright (C) 2021 Titanium Studios Pty Ltd\n"
    "\n";

static const char *USAGE_STRING =
    "\n"
    "Usage: SSTFLASH [options] <memory address> <ROM image file>\n"
    "\n"
    "Examples:\n"
    "    SSTFLASH C800 ABIOS.BIN\n"
    "    SSTFLASH -size 32 D000 BBIOS.BIN\n"
    "\n"
    "Options:\n"
    "-size <size in K>: Override amount of flash memory written.\n"
    "                   Default is size of file. May be larger or\n"
    "                   smaller than file size.\n";

typedef short bool;

typedef struct _Options
{
    unsigned short destSeg;
    const char *romImgPath;
    short sizeOverrideK;
} Options;

typedef struct _RomData
{
    unsigned char *romBlocks[MAX_ROM_BLOCK_COUNT];
    short numRomBlocks;
    unsigned long romSize;
    unsigned long origRomSize;
} RomData;

void PrintMessage(const char *msg, ...)
{
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
}

void LogWarning(const char *msg, ...)
{
    va_list args;

    printf("WARNING: ");

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    printf("\n");
}

void LogError(const char *msg, ...)
{
    va_list args;

    printf("ERROR: ");

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    printf("\n");
}

bool CheckMemoryModel()
{
    // We need a memory model with far pointers.
    // So a non-specific pointer should work the same
    // as an explicitly far pointer.
    void *ptr = MK_FP(0, 0);
    void far *farPtr = MK_FP(0, 0);

    if ((void far *)ptr != farPtr)
    {
        LogError("This app must be compiled with a memory model that uses far pointers.");
        return FALSE;
    }

    return TRUE;
}

bool ParseCmdLine(short argc, char **argv, Options* optionsOut)
{
    short i;

    memset(optionsOut, 0, sizeof(Options));

    for (i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        const char *nextArg = (i + 1 < argc) ? argv[i + 1] : NULL;

        if ((arg[0] == '-' || arg[0] == '/') &&
            !optionsOut->destSeg && 
            !optionsOut->romImgPath)
        {
            const char *opt = arg + 1;

            // Parse option.
            if (stricmp(opt, "?") == 0 || stricmp(opt, "h") == 0 || stricmp(opt, "help") == 0)
            {
                return FALSE; // Returning an error will result in the usage being displayed.
            }
            if (stricmp(opt, "size") == 0)
            {
                if (!nextArg)
                {
                    LogError("Size option missing size override value.");
                    return FALSE;
                }

                optionsOut->sizeOverrideK = atoi(nextArg);

                if (optionsOut->sizeOverrideK <= 0 || 
                    optionsOut->sizeOverrideK > MAX_ROM_SIZE_K ||
                    optionsOut->sizeOverrideK %2 != 0)
                {
                    LogError("Size override must be a multiple of 2 and %d and a multiple of 2.", MAX_ROM_SIZE_K);
                    return FALSE;
                }
                
                i++; // Skip past nextArg.
            }
            else
            {
                LogError("Invalid option '%s'", arg);
                return FALSE;
            }
        }
        else if (optionsOut->destSeg == 0)
        {
            // Parse address.
            unsigned short destSeg = (unsigned short)strtol(arg, NULL, 16);

            static const short BLOCK_SIZE =
                FLASH_BLOCK_SIZE > ROM_BLOCK_SIZE_ ?
                FLASH_BLOCK_SIZE : ROM_BLOCK_SIZE_;

            if (destSeg == 0 ||
                strlen(arg) > 4 ||
                destSeg % (BLOCK_SIZE / 16) != 0 ||
                destSeg < 0xA000)
            {
                LogError("Memory address must be between A000 and F800 and on a %dK boundary.",
                    BLOCK_SIZE / 1024);
                return FALSE;
            }

            optionsOut->destSeg = destSeg;
        }
        else if (optionsOut->romImgPath == NULL)
        {
            // Parse ROM image address.
            optionsOut->romImgPath = arg;
        }
        else
        {
            // Unexpected argument.
            LogError("Unexpected argument '%s'", arg);
            return FALSE;
        }
    }

    return optionsOut->destSeg && optionsOut->romImgPath;
}

bool LoadRomDataFromFile(const Options *options, RomData *romDataOut)
{
    FILE *f;
    long sizeRemaining;

    memset(romDataOut, 0, sizeof(RomData));

    f = fopen(options->romImgPath, "rb");
    if (!f)
    {
        LogError("Unable to open file '%s'", options->romImgPath);
        return FALSE;
    }

    sizeRemaining = (long)MAX_ROM_BLOCK_COUNT * (long)FLASH_BLOCK_SIZE;
    if (options->sizeOverrideK > 0)
    {
        sizeRemaining = (long)(options->sizeOverrideK) * 1024l;
    }

    while (!feof(f) && sizeRemaining > 0)
    {
        unsigned char *buffer;
        unsigned short readSize;

        if (romDataOut->numRomBlocks >= MAX_ROM_BLOCK_COUNT)
        {
            fclose(f);

            LogError("ROM image file exceeds max size of %dK", 
                MAX_ROM_SIZE_K);
            return FALSE;
        }

        buffer = (unsigned char *)malloc(FLASH_BLOCK_SIZE);
        memset(buffer, 0, FLASH_BLOCK_SIZE);
        romDataOut->romBlocks[romDataOut->numRomBlocks] = buffer;
        readSize = sizeRemaining < (long)FLASH_BLOCK_SIZE ? (unsigned short)sizeRemaining : FLASH_BLOCK_SIZE;
        sizeRemaining -= readSize;
        romDataOut->origRomSize += (short)fread(buffer, 1, readSize, f);
        romDataOut->numRomBlocks++;
    }

    fclose(f);

    // Add 4K blocks if there is remaining size.
    if (options->sizeOverrideK > 0)
    {
        while (sizeRemaining > 0)
        {
            unsigned char *buffer = (unsigned char *)malloc(FLASH_BLOCK_SIZE);
            memset(buffer, 0, FLASH_BLOCK_SIZE);
            romDataOut->romBlocks[romDataOut->numRomBlocks++] = buffer;
            sizeRemaining -= FLASH_BLOCK_SIZE;
        }
    }

    romDataOut->romSize = (unsigned long)romDataOut->numRomBlocks * (unsigned long)FLASH_BLOCK_SIZE;

    if (!romDataOut->origRomSize)
    {
        LogError("ROM image file is empty.");
        return FALSE;
    }

    if (romDataOut->origRomSize % ROM_BLOCK_SIZE__K)
    {
        LogError("ROM image file must be a multiple of %dK.",
            ROM_BLOCK_SIZE__K);
        return FALSE;
    }

    if (romDataOut->origRomSize < romDataOut->romSize)
    {
        PrintMessage("%dK image will be rounded up to %dK (4K multiple) with zeros.\n",
                     (short)(romDataOut->origRomSize / 1024L),
                     (short)(romDataOut->romSize / 1024L));
    }

    return TRUE;
}

void FreeRomData(RomData *romData)
{
    short i;

    for (i = 0; i < MAX_ROM_BLOCK_COUNT; i++)
    {
        if (romData->romBlocks[i] != NULL)
        {
            free(romData->romBlocks[i]);
        }
    }

    memset(romData, 0, sizeof(RomData));
}

// Waits for the value to be found at *addr. Loop timeout count is provided.
// Returns TRUE if expected value is read, FALSE in timeout.
bool WaitForValue(unsigned char *addr, unsigned char value, unsigned short timeoutCount)
{
    volatile unsigned char *addrVolatile;

	addrVolatile = addr;

	do
	{
		if (*addrVolatile == value)
		{
			return TRUE;
		}

	} while (--timeoutCount);

	return FALSE;
}

// Returns the number of polling loops needed for ~215us delay.
// Implementation: we see how many times we can implement a 256-count
// polling loop in one BIOS timer tick. Timer tick is ~18.6ms. Dividing
// by 256 ~= 215us.
unsigned short CalculateTimeoutLoopCount(unsigned long destSeg)
{
#ifdef __FAKEDOS__
    // Can't calculate this outside of DOS.
    // Fudge a number just so code will run.
    return 1000;
#else
	volatile unsigned char *biosTimerLsb = MK_FP(0x0040, 0x006C);
	unsigned char *destPtr = MK_FP(destSeg, 0x0000);
	unsigned char tickValue;
    unsigned short tickLoopCount;
	unsigned char expectedValue;

	// Wait for BIOS timer to tick over once.
	tickValue = *biosTimerLsb;
	while (*biosTimerLsb == tickValue)
	{
	}

	// Wait for BIOS timer to tick over once more,
	// counting how many test loops we can do.
	tickValue = *biosTimerLsb;
	expectedValue  = ~(destPtr[0]);

	// Overflow should not be possible, however we handle it
	// anyway. Reads from the SST device are over a slow bus, and even
	// reading at the min read cycle time on the fastest version of the
	// device, it would not be possible to overflow.
	for (tickLoopCount = 0; 
		 tickLoopCount < 0xFFFF && *biosTimerLsb == tickValue;
		 tickLoopCount++)
	{
		// We must read from the flashing destination to get correct
		// read timing. We are passing in a value that will never be 
		// matched, therefore the polling loop will run to timeout.
		WaitForValue(destPtr, expectedValue, 256);
	} while (*biosTimerLsb == tickValue);

	return tickLoopCount;
#endif
}

unsigned short CalculateSequenceSeg(unsigned short destSeg, long flashLen)
{
    const long sequenceWindowSize = 32L * 1024L;
    long destAddr;
    long seqAddr;
    unsigned short seqSeg;

    destAddr = (long)destSeg << 4L;
    seqAddr = destAddr & ~(sequenceWindowSize - 1L);

    if (seqAddr < destAddr && (seqAddr + sequenceWindowSize * 2L) <= destAddr + flashLen)
    {
        // Rounded down address was outside of flashing range. 
        // However, rounding up does fit within the flashing range,
        // so go ahead and round up.
        seqAddr += sequenceWindowSize;
    }

    seqSeg = (short)(seqAddr >> 4L);

    return seqSeg;
}

bool IsBiosAtSeg(unsigned short seg)
{
    unsigned char *ptr = MK_FP(seg, 0);

    return ptr[0] == 0x55 || ptr[1] == 0xFF;
}

bool HaveOverlappingBioses(unsigned short sequenceSeg, unsigned short destSeg, unsigned long flashLen)
{
    unsigned short twoKInSeg = 2 * 1024 / 16;
    unsigned short thirtyTwoKInSeg = 32 / 16 * 1024;
    unsigned short flashLenInSeg = (unsigned short)(flashLen / 16L);
    unsigned short endSeg = sequenceSeg + thirtyTwoKInSeg;
    unsigned short curr;

    for (curr = sequenceSeg; curr < endSeg; curr += twoKInSeg)
    {
        if (curr == destSeg)
        {
            // Skip the explicit range of the destination we will flash to.
            curr += flashLenInSeg;
            continue;
        }

        if (IsBiosAtSeg(curr))
        {
            return TRUE;
        }
    }

    return FALSE;
}

// Returns TRUE on Y.
bool GetYNConfirmation()
{
    short charInput;

    do
    {
        charInput = tolower(getch());
    } while (charInput != 'y' && charInput != 'n');

    PrintMessage("%c\n", charInput);

    return charInput == 'y';
}

void PrintSegAddress(unsigned short seqSeg, unsigned short destSeg)
{
    PrintMessage("%04X", destSeg);

    if (seqSeg != destSeg)
    {
        PrintMessage(" (sequence address % 04X)", seqSeg);
    }
}

void EnableInterrupts()
{
#ifndef __FAKEDOS__
    asm sti;
#endif
}

void DisableInterrupts()
{
#ifndef __FAKEDOS__
    asm cli;
#endif
}

const char *DetectDeviceType(unsigned short seqSeg, unsigned short destSeg)
{
    volatile unsigned char *seqPtr = MK_FP(seqSeg, 0);
    volatile unsigned char *destPtr = MK_FP(destSeg, 0);
    unsigned char vendorId;
    unsigned char deviceId;

    DisableInterrupts();

    // Enter software ID.
    seqPtr[0x5555] = 0xAA;
    seqPtr[0x2AAA] = 0x55;
    seqPtr[0x5555] = 0x90;

	vendorId = destPtr[0]; // Extra reads to give device time to respond. 
	vendorId = destPtr[0];
	vendorId = destPtr[0];

    vendorId = destPtr[0];
    deviceId = destPtr[1];

    // Exit software ID.
    seqPtr[0x5555] = 0xF0;

    EnableInterrupts();

    if (vendorId == 0xBF)
    {
        switch (deviceId)
        {
        case 0xB4:
            return "SST39SF512";
        case 0xB5:
            return "SST39SF010";
        case 0xB6:
            return "SST39SF020";
        case 0xB7:
            return "SST39SF040";
        default:
            break;
        }
    }

    return NULL;
}

bool EraseBlock(unsigned short seqSeg, unsigned char *dest, unsigned short timeoutLoopCount)
{
    volatile unsigned char *seqPtr = MK_FP(seqSeg, 0);
	short timeoutOuterLoopCount;

    seqPtr[0x5555] = 0xAA;
    seqPtr[0x2AAA] = 0x55;
    seqPtr[0x5555] = 0x80;
    seqPtr[0x5555] = 0xAA;
    seqPtr[0x2AAA] = 0x55;
    dest[0] = 0x30;

	// 1163 loops x ~215us = 250ms = 10x datasheet max.
	for (timeoutOuterLoopCount = 1163; timeoutOuterLoopCount; --timeoutOuterLoopCount)
	{
		if (WaitForValue(dest, 0xFF, timeoutLoopCount))
		{
			return TRUE;
		}
	}

    return FALSE;
}

bool ProgramBlock(unsigned short seqSeg, unsigned char *source, unsigned char *dest, unsigned short timeoutLoopCount)
{
    volatile unsigned char *seqPtr = MK_FP(seqSeg, 0);
    short i;

    for (i = 0; i < FLASH_BLOCK_SIZE; i++)
    {
        seqPtr[0x5555] = 0xAA;
        seqPtr[0x2AAA] = 0x55;
        seqPtr[0x5555] = 0xA0;

        dest[i] = source[i];

		// Device won't return actual data until write complete.
		// Timeout ~215us, or ~10x 20us max program time from datasheet.
        if (!WaitForValue(dest + i, source[i], timeoutLoopCount))
        {
            return FALSE;
        }
    }

    return TRUE;
}

// Returns number of blocks flashed.
// 0 if none flashed.
// -1 on error.
short FlashRom(unsigned short seqSeg, unsigned short destSeg, const RomData* romData, unsigned short timeoutLoopCount)
{
    const short blockSizeInSeg = FLASH_BLOCK_SIZE >> 4;
    unsigned char *destPtr;
    short numBlocksFlashed = 0;
    const char *errorString = NULL;
    short blockIndex;

    DisableInterrupts();

    for (blockIndex = 0; blockIndex < romData->numRomBlocks; blockIndex++, destSeg += blockSizeInSeg)
    {
        destPtr = MK_FP(destSeg, 0);

        if (memcmp(destPtr, romData->romBlocks[blockIndex], FLASH_BLOCK_SIZE) == 0)
        {
            continue;
        }

        if (!EraseBlock(seqSeg, destPtr, timeoutLoopCount))
        {
            errorString = "Timeout erasing block.";
            break;
        }

        if (!ProgramBlock(seqSeg, romData->romBlocks[blockIndex], destPtr, timeoutLoopCount))
        {
            errorString = "Timeout programming block.";
            break;
        }

        numBlocksFlashed++;
    }

    EnableInterrupts();

    if (errorString)
    {
        LogError(errorString);
        return -1;
    }

    return numBlocksFlashed;
}

bool VerifyRom(unsigned short destSeg, const RomData* romData)
{
    const short blockSizeInSeg = FLASH_BLOCK_SIZE >> 4;
    unsigned char *destPtr;
    short blockIndex;

    for (blockIndex = 0; blockIndex < romData->numRomBlocks; blockIndex++, destSeg += blockSizeInSeg)
    {
        destPtr = MK_FP(destSeg, 0);

        if (memcmp(destPtr, romData->romBlocks[blockIndex], FLASH_BLOCK_SIZE) != 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

bool ProcessRom(const Options* options, const RomData* romData)
{
    unsigned short timeoutLoopCount;
    unsigned short sequenceSeg;
    const char *deviceName;
    short numBlocksFlashed;

    //Calibrate timeout timer.
    PrintMessage("Calibrating timeout timer...");
    timeoutLoopCount =
        CalculateTimeoutLoopCount(options->destSeg);
    PrintMessage(" %d loops per ms\n", timeoutLoopCount);

    // Find the segment address to use for the programming sequences.
    sequenceSeg = CalculateSequenceSeg(options->destSeg, romData->romSize);

    // Detect the flash ROM device.
    deviceName = DetectDeviceType(sequenceSeg, options->destSeg);
    if (!deviceName)
    {
        PrintMessage("Unable to detect SST39SF0x0 flash ROM at address ");
        PrintSegAddress(sequenceSeg, options->destSeg);
        PrintMessage(".\n");
        return FALSE;
    }

    // Display a warning if there is another BIOS we might be able to overwrite.
    if (HaveOverlappingBioses(sequenceSeg, options->destSeg, romData->romSize))
    {
        PrintMessage("\n"
                     "*** WARNING: Another ROM image was found in the 32K programming range ***\n"
                     "*** starting at %04X. If there is a second SST Flash ROM in this      ***\n"
                     "*** range, it's data may be become corrupted after programming.       ***\n",
                     sequenceSeg);
    }

    // Print details on what we are about to do.
    PrintMessage("\n"
                 "Will program %dK to %s at address ",
                 (unsigned short)(romData->romSize / 1024L),
                 deviceName);
    PrintSegAddress(sequenceSeg, options->destSeg);
    PrintMessage(".\n");

    // Check that user wants to continue.
    PrintMessage("Continue Y/N? ");
    if (!GetYNConfirmation())
    {
        PrintMessage("Exiting.\n");
        return FALSE;
    }

    PrintMessage("Programming...");

    numBlocksFlashed = FlashRom(sequenceSeg, options->destSeg, romData, timeoutLoopCount);
    if (numBlocksFlashed == 0)
    {
        PrintMessage("\nFlash ROM already up to date. No programming done.\n");
        return TRUE;
    }

    if (numBlocksFlashed < 0)
    {
        PrintMessage("\nError during programming. The flash ROM might now have corrupt data.\n"
                     "Please reboot your computer.");
    }
    else if (VerifyRom(options->destSeg, romData))
    {
        PrintMessage("\nProgramming complete! Please reboot your computer.");
    }
    else
    {
        PrintMessage("\nVerify failed! The flash ROM does not have correct data.\n"
                        "Please reboot your computer.");
    }

    // Since the BIOS has just been flashed, the previous version still
    // running is unlikely to continue to function properly. The only 
    // practical option is to have the user reboot the computer.
    while (1) {} 
}

short main(short argc, char **argv)
{
    Options options;
    RomData romData;
    bool flashResult;

    PrintMessage(PRODUCT_STRING);

    if (!CheckMemoryModel())
    {
        return 1;
    }

    if (!ParseCmdLine(argc, argv, &options))
    {
        PrintMessage(USAGE_STRING);
        return 1;
    }

    if (!LoadRomDataFromFile(&options, &romData))
    {
        FreeRomData(&romData);
        return 1;
    }

    flashResult = ProcessRom(&options, &romData);

    FreeRomData(&romData);
    
    return flashResult ? 0 : 1;
}
