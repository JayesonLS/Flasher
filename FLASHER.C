// Flasher
// Programs SST89SF0x0A Flash ROMs
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TRUE 1
#define FALSE 0

#define MAX_ROM_SIZE_K 256
#define ROM_BLOCK_SIZE_K 2
#define FLASH_BLOCK_SIZE_K 4
#define ROM_BLOCK_SIZE (ROM_BLOCK_SIZE_K * 1024)
#define FLASH_BLOCK_SIZE (FLASH_BLOCK_SIZE_K * 1024)
#define MAX_ROM_BLOCK_COUNT (MAX_ROM_SIZE_K / FLASH_BLOCK_SIZE_K)

static const char *PRODUCT_STRING =
	"Flasher Version 0.1 - Programs SST89SF0x0A Flash ROMs\n"
	"Copyright (C) 2021 Titanium Studios Pty Ltd\n"
	"\n";

static const char *USAGE_STRING =
	"Usage: FLASHER <memory address> <ROM image file>\n"
	" e.g.: FLASHER C800 ABIOS.BIN\n";

typedef int bool;

typedef struct _Options
{
	unsigned int destAddr;
	const char *romImgPath;
} Options;

typedef struct _RomData
{
	unsigned char *romBlocks[MAX_ROM_BLOCK_COUNT];
	unsigned int numRomBlocks;
	unsigned long origRomSize;
} RomData;

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

bool ParseCmdLine(int argc, char **argv, Options* optionsOut)
{
	int i;

	memset(optionsOut, 0, sizeof(Options));

	for (i = 1; i < argc; i++)
	{
		const char *arg = argv[i];
		const char *nextArg = (i + 1 < argc) ? argv[i + 1] : NULL;

		if (arg[0] == '-' && 
			!optionsOut->destAddr && 
			!optionsOut->romImgPath)
		{
			// Parse option.
			//if (....)
			//{
			//
			//}
			//else
			{
				LogError("Invalid option '%s'", arg);
			}
		}
		else if (optionsOut->destAddr == 0)
		{
			// Parse address.
			unsigned int destAddr = (unsigned int)strtol(arg, NULL, 16);

			static const int BLOCK_SIZE =
				FLASH_BLOCK_SIZE > ROM_BLOCK_SIZE ?
				FLASH_BLOCK_SIZE : ROM_BLOCK_SIZE;

			if (destAddr == 0 ||
				strlen(arg) > 4 ||
				destAddr % (BLOCK_SIZE / 16) != 0 ||
				destAddr < 0xA000)
			{
				LogError("Memory address must be between A000 and F800 and on a %dK boundary.",
					BLOCK_SIZE / 1024);
				return FALSE;
			}

			optionsOut->destAddr = destAddr;
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

	return optionsOut->destAddr && optionsOut->romImgPath;
}

long RomDataFlashSize(const RomData *romData)
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

	if (romDataOut->origRomSize % ROM_BLOCK_SIZE)
	{
		LogError("ROM image file must be a multiple of %dK.",
			ROM_BLOCK_SIZE_K);
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

bool FlashRom(unsigned int destAddr, const RomData* romData)
{
	// TODO: Calibrate timeout timer.
	// TODO: Compute sequence base address.
	// TODO: Warn if sequence will run outside of specified ROM range.
	// TODO: Detect device, and if OK, display message.
	// TODO: Disable interrupts.
	// TODO: Do the programming. Erase, then write.
	// TODO: Enable interrupts.
	// TODO: Prompt the user to reboot.
	// TODO: Lock the computer.

	return FALSE;
}

int main(int argc, char **argv)
{
	Options options;
	RomData romData;
	bool flashResult;

	printf(PRODUCT_STRING);

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

	flashResult = FlashRom(options.destAddr, &romData);

	FreeRomData(&romData);
	
	return flashResult ? 0 : 1;
}
