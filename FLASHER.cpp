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

static const char *PRODUCT_STRING =
	"Flasher Version 0.1 - Programs SST89SF0x0A Flash ROMs\n"
	"Copyright (C) 2021 Titanium Studios Pty Ltd\n"
	"\n";

static const char *USAGE_STRING =
	"Usage: FLASHER <memory address> <ROM image file>\n"
	" e.g.: FLASHER C800 ABIOS.BIN\n";

#ifdef __TCPLUSPLUS__
typedef int bool;
static const bool true = 1;
static const bool false = 0;
#endif

static const int MAX_ROM_SIZE_K = 256;
static const int ROM_BLOCK_SIZE_K = 2;
static const int FLASH_BLOCK_SIZE_K = 4;
static const int ROM_BLOCK_SIZE = ROM_BLOCK_SIZE_K * 1024;
static const int FLASH_BLOCK_SIZE = FLASH_BLOCK_SIZE_K * 1024;
static const int MAX_ROM_BLOCK_COUNT = MAX_ROM_SIZE_K / FLASH_BLOCK_SIZE_K;

void LogMessage(const char *msg, ...);
void LogWarning(const char *msg, ...);
void LogError(const char *msg, ...);

template <class T> void SafeDelete(T* &ptr)
{
	if (ptr != NULL)
	{
		delete ptr;
		ptr = NULL;
	}
}

template <class T> void SafeDeleteArray(T* &ptr)
{
	if (ptr != NULL)
	{
		delete[] ptr;
		ptr = NULL;
	}
}


class Options
{
public:
	unsigned int destAddr;
	const char *romImgPath;

	Options();
	bool ParseCmdLine(int argc, char **argv);
};

Options::Options()
{
	destAddr = 0;
	romImgPath = NULL;
}

bool Options::ParseCmdLine(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		const char *arg = argv[i];
		const char *nextArg = (i + 1 < argc) ? argv[i + 1] : NULL;

		if (arg[0] == '-')
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
		else if (destAddr == 0)
		{
			// Parse address.
			destAddr = (unsigned int)strtol(arg, NULL, 16);

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
				return false;
			}
		}
		else if (romImgPath == NULL)
		{
			// Parse ROM image address.
			romImgPath = arg;
		}
		else
		{
			// Unexpected argument.
			LogError("Unexpected argument '%s'", arg);
			return false;
		}
	}

	return destAddr && romImgPath;
}

class RomData
{
public:
	unsigned char *romBlocks[MAX_ROM_BLOCK_COUNT];
	unsigned int numRomBlocks;
	unsigned long origRomSize;

	RomData();
	~RomData();
	bool LoadFromFile(const char *path);

	long FlashSize()
	{
		return (long)numRomBlocks * FLASH_BLOCK_SIZE;
	}

private:
	void ClearData();
};

RomData::RomData()
{
	memset(romBlocks, 0, sizeof(romBlocks));
}

RomData::~RomData()
{
	ClearData();
}

bool RomData::LoadFromFile(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		LogError("Unable to open file '%s'", path);
		return false;
	}

	ClearData();

	while (!feof(f))
	{
		if (numRomBlocks >= MAX_ROM_BLOCK_COUNT)
		{
			fclose(f);

			LogError("ROM image file exceeds max size of %dK", 
				MAX_ROM_SIZE_K);
			return false;
		}

		unsigned char *buffer = new unsigned char[FLASH_BLOCK_SIZE];
		memset(buffer, 0, FLASH_BLOCK_SIZE);

		romBlocks[numRomBlocks] = buffer;

		origRomSize += (int)fread(buffer, 1, FLASH_BLOCK_SIZE, f);

		numRomBlocks++;
	}

	fclose(f);

	if (!origRomSize)
	{
		LogError("ROM image file is empty.");
		return false;
	}

	if (origRomSize % ROM_BLOCK_SIZE)
	{
		LogError("ROM image file must be a multiple of %dK.",
			ROM_BLOCK_SIZE_K);
		return false;
	}

	if (origRomSize % FLASH_BLOCK_SIZE)
	{
		LogMessage("%dK ROM image will be rounded up to a multiple of %dK.",
			origRomSize / 1024,
			FLASH_BLOCK_SIZE_K);
	}

	return true;
}

void RomData::ClearData()
{
	numRomBlocks = 0;
	origRomSize = 0;

	for (int i = 0; i < MAX_ROM_BLOCK_COUNT; i++)
	{
		SafeDeleteArray(romBlocks[i]);
	}
}

class RomFlasher
{
public:
	bool FlashRom(unsigned int destAddr, const RomData& romData);
};

bool RomFlasher::FlashRom(unsigned int destAddr, const RomData& romData)
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

	return false;
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
	printf("WARNING: ");

	va_list args;
	va_start(args, msg);

	vprintf(msg, args);

	va_end(args);

	printf("\n");
}

void LogError(const char *msg, ...)
{
	printf("ERROR: ");

	va_list args;
	va_start(args, msg);

	vprintf(msg, args);

	va_end(args);

	printf("\n");
}

int main(int argc, char **argv)
{
	printf(PRODUCT_STRING);

	Options options;
	if (!options.ParseCmdLine(argc, argv))
	{
		printf("\n");
		printf(USAGE_STRING);
		return 1;
	}


	RomData romData;
	if (!romData.LoadFromFile(options.romImgPath))
	{
		return 1;
	}

	RomFlasher flasher;
	if (!flasher.FlashRom(options.destAddr, romData))
	{
		return 1;
	}

	return 0;
}
