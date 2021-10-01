// Flasher - Programs SST89SF0x0A Flash ROMs
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

#define MAX_TIMEOUT_VALUE 0xFFFFFFFF

static const char *PRODUCT_STRING =
	"Flasher Version 0.1 - Programs SST89SF0x0A Flash ROMs\n"
	"Copyright (C) 2021 Titanium Studios Pty Ltd\n"
	"\n";

static const char *USAGE_STRING =
	"Usage: FLASHER [options] <memory address> <ROM image file>\n"
	" e.g.: FLASHER C800 ABIOS.BIN\n"
	"   or: FLASHER -qw D000 BBIOS.BIN\n"
	"\n"
	"Options: -qw     Be quiet about 32K window warnings.";

typedef int bool;

typedef struct _Options
{
	unsigned int destSeg;
	const char *romImgPath;
	bool quietWindowCheck; // True means we dont need to warn about writing less than 32K to the 32K window.
} Options;

typedef struct _RomData
{
	unsigned char *romBlocks[MAX_ROM_BLOCK_COUNT];
	unsigned int numRomBlocks;
	unsigned long origRomSize;
} RomData;

void LogMessageNoCr(const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);
}

void LogMessage(const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);

	printf("\n");
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
			if (stricmp(arg, "-qw") == 0)
			{
				optionsOut->quietWindowCheck = TRUE;
			}
			else
			{
				LogError("Invalid option '%s'", arg);
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

long RomDataFlashLength(const RomData *romData)
{
	return (long)romData->numRomBlocks * FLASH_BLOCK_SIZE;
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
		LogMessage("%dK ROM image will be rounded up to a multiple of %dK.",
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
	volatile unsigned char *addrVol;

	addrVol = addr;

	for (count = 0; count < timeoutCount; count++)
	{
		if (*addrVol == value)
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

unsigned int CalculateSequenceSeg(unsigned int destSeg, long flashLength, bool quietWindowCheck)
{
	const long sequenceWindowSize = 32L * 1024L;
	long destAddr;
	long seqAddr;
	unsigned int seqSeg;
	int charInput;

	destAddr = (long)destSeg << 4L;
	seqAddr = destAddr & ~(sequenceWindowSize - 1L);

	if (seqAddr < destAddr && (seqAddr + sequenceWindowSize * 2L) <= destAddr + flashLength)
	{
		// Rounded down address was outside of flashing range. 
		// However, rounding up does fit within the flashing range,
		// so go ahead and round up.
		seqAddr += sequenceWindowSize;
	}

	seqSeg = seqAddr >> 4L;

	if (!quietWindowCheck)
	{
		if (seqAddr < destAddr || seqAddr + sequenceWindowSize > destAddr + flashLength)
		{
			LogMessageNoCr("\n"
				           "The ROM does not cover entire 32K range starting at %04X.\n"
			               "If there is a second SST Flash ROM in this address range,\n"
				           "it's data may be corrupted.\n"
			               "\n"
			               "Continue Y/N? ",
				           seqSeg);
			
			do
			{
				charInput = tolower(getch());
			} while (charInput != 'y' && charInput != 'n');

			LogMessage("%c\n", charInput);

			if (charInput != 'y')
			{
				LogMessage("Exiting.");
				return 0;
			}
		}
	}
	LogMessage("Sequence segment %04X", seqSeg);

	return seqSeg;
}

bool FlashRom(unsigned int destSeg, const RomData* romData, const Options* options)
{
	unsigned long msTimeoutLoopCount;
	unsigned int sequenceSeg;

	// Find the segment address to use for the programming sequences.
	sequenceSeg = CalculateSequenceSeg(destSeg, RomDataFlashLength(romData), options->quietWindowCheck);
	if (!sequenceSeg)
	{
		return FALSE;
	}

	//Calibrate timeout timer.
	LogMessageNoCr("Calibrating timeout timer...");
	msTimeoutLoopCount =
		CalculateMsTimeoutLoopCount();
	LogMessage(" %ld loops per ms", msTimeoutLoopCount);

	// TODO: Detect device, and if OK, display message.
	// TODO: Disable interrupts.
	// TODO: Do the programming. Erase, then write.
	// TODO: Enable interrupts.
	// TODO: Prompt the user to reboot.
	// TODO: Lock the computer.

	LogMessageNoCr("\nProgramming complete! Please reboot your computer.");

	// Since the BIOS just flashed is likely no longer functioning properly, 
	// the computer needs to be rebooted.
	while (1) {} 

	return TRUE;
}

int main(int argc, char **argv)
{
	Options options;
	RomData romData;
	bool flashResult;

	printf(PRODUCT_STRING);

	if (!CheckMemoryModel())
	{
		return 1;
	}

	if (!ParseCmdLine(argc, argv, &options))
	{
		printf("\n");
		printf(USAGE_STRING);
		return 1;
	}

	if (!LoadRomDataFromFile(options.romImgPath, &romData))
	{
		FreeRomData(&romData);
		return 1;
	}

	flashResult = FlashRom(options.destSeg, &romData, &options);

	FreeRomData(&romData);
	
	return flashResult ? 0 : 1;
}
