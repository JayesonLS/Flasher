// SSTFLASH - Programs SST39SF0x0 Flash ROMs
//
// Copyright (C) 2021 Titanium Studios Pty Ltd 
// 
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.If not, see < https://www.gnu.org/licenses/>.

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

#define BYTE_WRITE_TIMEOUT_MS 1ul // Data sheet says only 14us.
#define SECTOR_ERASE_TIMEOUT_MS 250ul // 10x data sheet max time.
#define CHIP_ERASE_TIMEOUT_MS 1000ul // 10x data sheet max time.
#define MAX_TIMEOUT_VALUE 0xFFFFFFFF

static const char *PRODUCT_STRING =
	"SSTFLASH Version 0.9b1 - Programs SST39SF0x0 Flash ROMs\n"
	"Copyright (C) 2021 Titanium Studios Pty Ltd\n"
	"\n";

static const char *USAGE_STRING =
	"\n"
	"Usage: SSTFLASH <memory address> <ROM image file>\n"
	" e.g.: SSTFLASH C800 ABIOS.BIN\n";

typedef int bool;

typedef struct _Options
{
	unsigned int destSeg;
	const char *romImgPath;
} Options;

typedef struct _RomData
{
	unsigned char *romBlocks[MAX_ROM_BLOCK_COUNT];
	int numRomBlocks;
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

bool ParseCmdLine(int argc, char **argv, Options* optionsOut)
{
	int i;

	memset(optionsOut, 0, sizeof(Options));

	for (i = 1; i < argc; i++)
	{
		const char *arg = argv[i];
		const char *nextArg = (i + 1 < argc) ? argv[i + 1] : NULL;

		if (arg[0] == '-' && 
			!optionsOut->destSeg && 
			!optionsOut->romImgPath)
		{
			(void)nextArg; // Variable is for if we add complex options.
			// Parse option.
			// if (stricmp(arg, "-option_name") == 0)
			// {
			// 	// set somethign here.
			// }
			// else
			{
				LogError("Invalid option '%s'", arg);
				return FALSE;
			}
		}
		else if (optionsOut->destSeg == 0)
		{
			// Parse address.
			unsigned int destSeg = (unsigned int)strtol(arg, NULL, 16);

			static const int BLOCK_SIZE =
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

bool LoadRomDataFromFile(const char *path, RomData *romDataOut)
{
	FILE *f;

	memset(romDataOut, 0, sizeof(RomData));

	f = fopen(path, "rb");
	if (!f)
	{
		LogError("Unable to open file '%s'", path);
		return FALSE;
	}

	while (!feof(f))
	{
		unsigned char *buffer;

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
		romDataOut->origRomSize += (int)fread(buffer, 1, FLASH_BLOCK_SIZE, f);
		romDataOut->romSize += FLASH_BLOCK_SIZE;
		romDataOut->numRomBlocks++;
	}

	fclose(f);

	if (!romDataOut->origRomSize)
	{
		LogError("ROM image file is empty.");
		return FALSE;
	}

	if (romDataOut->origRomSize % ROM_BLOCK_SIZE_)
	{
		LogError("ROM image file must be a multiple of %dK.",
			ROM_BLOCK_SIZE__K);
		return FALSE;
	}

	if (romDataOut->origRomSize % FLASH_BLOCK_SIZE)
	{
		PrintMessage("%dK ROM image will be rounded up to a multiple of %dK.\n",
			         (int)(romDataOut->origRomSize / 1024L),
			         FLASH_BLOCK_SIZE_K);
	}

	return TRUE;
}

void FreeRomData(RomData *romData)
{
	int i;

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
// Returns actual number of loops waited.
unsigned long WaitForValue(unsigned char *addr, unsigned char value, unsigned long timeoutCount)
{
	unsigned long count;
	volatile unsigned char *addrVolatile;

	addrVolatile = addr;

	for (count = 0; count < timeoutCount; count++)
	{
		if (*addrVolatile == value)
		{
			return count;
		}
	}

	return MAX_TIMEOUT_VALUE; // Timed out.
}

// Returns the number of polling loops needed for ~1ms delay.
unsigned long CalculateMsTimeoutLoopCount()
{
#ifdef __FAKEDOS__
	// Can't calculate this outside of DOS.
	// Fudge a number just so code will run.
	return 1000;
#else
	unsigned char *biosTimerLsb;
	unsigned char startValue;
	unsigned long tickLoopCount;

	biosTimerLsb = (unsigned char *)MK_FP(0x0040, 0x006C);
	startValue = *biosTimerLsb;

	// Wait for the timer to tick over once.
	WaitForValue(biosTimerLsb, startValue + 1, MAX_TIMEOUT_VALUE);

	// Now that it just ticked over, wait for the it to 
	// tick over one more time.
	tickLoopCount = 
		WaitForValue(biosTimerLsb, startValue + 2, MAX_TIMEOUT_VALUE);

	// Tick is approximately 55ms, so scale accordingly.
	return tickLoopCount / 55;
#endif
}

unsigned int CalculateSequenceSeg(unsigned int destSeg, long flashLen)
{
	const long sequenceWindowSize = 32L * 1024L;
	long destAddr;
	long seqAddr;
	unsigned int seqSeg;

	destAddr = (long)destSeg << 4L;
	seqAddr = destAddr & ~(sequenceWindowSize - 1L);

	if (seqAddr < destAddr && (seqAddr + sequenceWindowSize * 2L) <= destAddr + flashLen)
	{
		// Rounded down address was outside of flashing range. 
		// However, rounding up does fit within the flashing range,
		// so go ahead and round up.
		seqAddr += sequenceWindowSize;
	}

	seqSeg = seqAddr >> 4L;

	return seqSeg;
}

bool IsBiosAtSeg(unsigned int seg)
{
	unsigned char *ptr = MK_FP(seg, 0);

	return ptr[0] == 0x55 || ptr[1] == 0xFF;
}

bool HaveOverlappingBioses(unsigned int sequenceSeg, unsigned int destSeg, unsigned long flashLen)
{
	unsigned int twoKInSeg = 2 * 1024 / 16;
	unsigned int thirtyTwoKInSeg = 32 / 16 * 1024;
	unsigned int flashLenInSeg = (unsigned int)(flashLen / 16L);
	unsigned int endSeg = sequenceSeg + thirtyTwoKInSeg;
	unsigned int curr;

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
	int charInput;

	do
	{
		charInput = tolower(getch());
	} while (charInput != 'y' && charInput != 'n');

	PrintMessage("%c\n", charInput);

	return charInput == 'y';
}

void PrintSegAddress(unsigned int seqSeg, unsigned int destSeg)
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

const char *DetectDeviceType(unsigned int seqSeg, unsigned int destSeg)
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

bool EraseBlock(unsigned int seqSeg, unsigned char *dest, unsigned long timeoutLoopCount)
{
	volatile unsigned char *seqPtr = MK_FP(seqSeg, 0);

	seqPtr[0x5555] = 0xAA;
	seqPtr[0x2AAA] = 0x55;
	seqPtr[0x5555] = 0x80;
	seqPtr[0x5555] = 0xAA;
	seqPtr[0x2AAA] = 0x55;
	dest[0] = 0x30;

	return WaitForValue(dest, 0xFF, timeoutLoopCount) != MAX_TIMEOUT_VALUE;
}

bool ProgramBlock(unsigned int seqSeg, unsigned char *source, unsigned char *dest, unsigned long timeoutLoopCount)
{
	volatile unsigned char *seqPtr = MK_FP(seqSeg, 0);
	int i;

	for (i = 0; i < FLASH_BLOCK_SIZE; i++)
	{
		seqPtr[0x5555] = 0xAA;
		seqPtr[0x2AAA] = 0x55;
		seqPtr[0x5555] = 0xA0;

		dest[i] = source[i];

		if (WaitForValue(dest + i, source[i], timeoutLoopCount) == MAX_TIMEOUT_VALUE)
		{
			return FALSE;
		}
	}

	return TRUE;
}

// Returns number of blocks flashed.
// 0 if none flashed.
// -1 on error.
int FlashRom(unsigned int seqSeg, unsigned int destSeg, const RomData* romData, unsigned long msTimeoutLoopCount)
{
	const int blockSizeInSeg = FLASH_BLOCK_SIZE >> 4;
	const unsigned long sectorEraseTimeout = SECTOR_ERASE_TIMEOUT_MS * msTimeoutLoopCount;
	const unsigned long byteWriteTimeout = BYTE_WRITE_TIMEOUT_MS * msTimeoutLoopCount;
	unsigned char *destPtr;
	int numBlocksFlashed = 0;
	const char *errorString = NULL;
	int blockIndex;

	DisableInterrupts();

	for (blockIndex = 0; blockIndex < romData->numRomBlocks; blockIndex++, destSeg += blockSizeInSeg)
	{
		destPtr = MK_FP(destSeg, 0);

        if (memcmp(destPtr, romData->romBlocks[blockIndex], FLASH_BLOCK_SIZE) == 0)
		{
			continue;
		}

		if (!EraseBlock(seqSeg, destPtr, sectorEraseTimeout))
		{
			errorString = "Timeout erasing block.";
			break;
		}

		if (!ProgramBlock(seqSeg, romData->romBlocks[blockIndex], destPtr, byteWriteTimeout))
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

bool VerifyRom(unsigned int destSeg, const RomData* romData)
{
	const int blockSizeInSeg = FLASH_BLOCK_SIZE >> 4;
	unsigned char *destPtr;
	int blockIndex;

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
	unsigned long msTimeoutLoopCount;
	unsigned int sequenceSeg;
	const char *deviceName;
	int numBlocksFlashed;

	//Calibrate timeout timer.
	PrintMessage("Calibrating timeout timer...");
	msTimeoutLoopCount =
		CalculateMsTimeoutLoopCount();
	PrintMessage(" %ld loops per ms\n", msTimeoutLoopCount);

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
		         (unsigned int)(romData->romSize / 1024L),
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

	numBlocksFlashed = FlashRom(sequenceSeg, options->destSeg, romData, msTimeoutLoopCount);
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

int main(int argc, char **argv)
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

	if (!LoadRomDataFromFile(options.romImgPath, &romData))
	{
		FreeRomData(&romData);
		return 1;
	}

	flashResult = ProcessRom(&options, &romData);

	FreeRomData(&romData);
	
	return flashResult ? 0 : 1;
}
